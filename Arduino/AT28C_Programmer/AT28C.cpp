/*  
  AT28C_Programmer.ino - Programmatore EEPROM AT28C
  Copyright (C) 2023 DrVector
  
  Gestione EEPROM AT28C
*/

#include <Arduino.h>
#include "Const.h"
#include "SRHelper.h"
#include "AT28C.h"

//******************************************************************************************************************//
//* Imposta il bus dati in INPUT o OUTPUT
//******************************************************************************************************************//
void setDataBusMode(int mode)
{
  if (mode == INPUT || mode == OUTPUT) {
    for (int i = 0; i < 8; i++) {
      pinMode(dataPins[i], mode);
    }
  }
}

//******************************************************************************************************************//
//* Impostazione indirizzo inbase al tipo di ROM considerando che per le EPROM 27xxx A14 è sul WE delle EEPROM AT28C
//******************************************************************************************************************//
void romTypeAddressWrite(e_rom_type romtype, unsigned long value) {
  // viene indicata la tipologia di eprom invece della dimensione da leggere
  switch (romtype) {
    case AT28C64:
      break;
    case AT28C256:
      break;
    case E2764:
      // A14 su VPP del 2764, mantiene livello logico alto
      // A13 su NC del 2764
      value = value + 0x6000;
      break;
    case E27128:
      // A14 su VPP del 27128, mantiene livello logico alto
      value = value + 0x4000;
      break;
    case E27256:
      if (value < 0x4000) {
        // A14 su VPP del 27128, mantiene livello logico alto
        value = value + 0x4000;
        // WE di AT28xx su A14 di 27xx
        digitalWrite(EEPROM_WE_PIN, LOW);
      } else {
        // WE di AT28xx su A14 di 27xx
        digitalWrite(EEPROM_WE_PIN, HIGH);
      }
      break;
    case E27512:
      if (value < 0x4000 || (value >= 0x8000 && value < 0xC000)) {
        // WE di AT28xx su A14 di 27xx
        digitalWrite(EEPROM_WE_PIN, LOW);
      } else {
        // WE di AT28xx su A14 di 27xx
        digitalWrite(EEPROM_WE_PIN, HIGH);
      }
      if (value >= 0x4000 && value < 0x8000) {
        // A14 su A15 del 27512
        value += 0x4000;
      } else if (value >= 0x8000 && value < 0xC000) {
        // A14 su A15 del 27512
        value -= 0x4000;
      }
      break;
  }
  addressWrite(value);
}

//******************************************************************************************************************//
//* Lettura di un byte all'indirizzo selezionato
//******************************************************************************************************************//
byte readByte(e_rom_type romtype, unsigned long address) {

  // Imposta il bus dati in input
  setDataBusMode(INPUT);

  digitalWrite(EEPROM_CE_PIN, HIGH);
  digitalWrite(EEPROM_OE_PIN, HIGH);
  digitalWrite(EEPROM_WE_PIN, HIGH);
  delayMicroseconds(1);

  // Imposta indirizzo
  romTypeAddressWrite(romtype, address);
  //Serial.println("address=" + (String)addr);

  digitalWrite(EEPROM_CE_PIN, LOW);
  delayMicroseconds(1);

  digitalWrite(EEPROM_OE_PIN, LOW);
  delayMicroseconds(1);
  //delayMicroseconds(100);

  // Lettura pins D2/D9 (Bus Dati)
  byte bval = 0;
  for (int y = 0; y < 8; y++) {
    bitWrite(bval, y, digitalRead(dataPins[y]));
  }

  digitalWrite(EEPROM_OE_PIN, HIGH);
  delayMicroseconds(1);

  digitalWrite(EEPROM_CE_PIN, HIGH);
  delayMicroseconds(1);

  return bval;
}

//******************************************************************************************************************//
//* Scrittura di un byte all'indirizzo selezionato
//******************************************************************************************************************//
byte writeByte(e_rom_type romtype, unsigned long address, byte value)
{
  digitalWrite(EEPROM_CE_PIN, HIGH);
  digitalWrite(EEPROM_OE_PIN, HIGH);
  digitalWrite(EEPROM_WE_PIN, HIGH);

  // Imposta il bus di dati in output
  setDataBusMode(OUTPUT);

  // Imposta indirizzo
  romTypeAddressWrite(romtype, address);

  // Scrittura pins D2/D9 (Bus Dati)
  for (int i = 0; i < 8; i++)
  {
    digitalWrite(dataPins[i], bitRead(value, i));
  }
  delayMicroseconds(1);

  digitalWrite(EEPROM_CE_PIN, LOW);
  delayMicroseconds(1);

  if (romtype != E27256) {
    // on 27256 pin 27 WE is not P but A14
    digitalWrite(EEPROM_WE_PIN, LOW);
    delayMicroseconds(1);
  }

  if (romtype == E2764 || romtype == E27128 || romtype == E27256) {
    // EPROM 1 ms program pulse
    delay(1);
  }

  if (romtype != E27256) {
    // on 27256 pin 27 WE is not P but A14
    digitalWrite(EEPROM_WE_PIN, HIGH);
    delayMicroseconds(1);
  }

  digitalWrite(EEPROM_CE_PIN, HIGH);
  delayMicroseconds(1);

  return value;
}

//******************************************************************************************************************//
//* Scrittura di una pagina di 64 byte all'indirizzo selezionato
//******************************************************************************************************************//
byte writePage(e_rom_type romtype, unsigned long address, byte* page, unsigned int size)
{
  digitalWrite(EEPROM_CE_PIN, HIGH);
  digitalWrite(EEPROM_OE_PIN, HIGH);
  digitalWrite(EEPROM_WE_PIN, HIGH);

  // Imposta il bus di dati in output
  setDataBusMode(OUTPUT);

  byte value; // valore da scrivere, ultimo valore scritto
  for (unsigned int idx = 0; idx < size; idx++) {
    value = writeByte(romtype, address + idx, page[idx]);
  }

  return value;
}

//******************************************************************************************************************//
//* Attende il termine della scrittura di un byte e ne verifica la corretta valorizzazione
//******************************************************************************************************************//
byte waitAndCheckWrite(e_rom_type romtype, byte value)
{
  // Imposta il bus dati in input
  setDataBusMode(INPUT);

  if (romtype == AT28C64 || romtype == AT28C256) {
    digitalWrite(EEPROM_CE_PIN, LOW);
    delayMicroseconds(1);
  }

  digitalWrite(EEPROM_OE_PIN, LOW);
  delayMicroseconds(1);

  // Attende il termine della scrittura del byte, verificando
  // che il bit 7 corrisponda a quando scritto
  // durante la scrittura il bit 7 è il complemento di quanto inviato
  while (bitRead(value, 7) != digitalRead(dataPins[7])) {
    digitalWrite(EEPROM_OE_PIN, HIGH);
    delayMicroseconds(1);
    delay(1);
    digitalWrite(EEPROM_OE_PIN, LOW);
    delayMicroseconds(1);
  }

  // Lettura pins D2/D9 (Bus Dati)
  byte bval = 0;
  for (int y = 0; y < 8; y++) {
    bitWrite(bval, y, digitalRead(dataPins[y]));
  }

  digitalWrite(EEPROM_OE_PIN, HIGH);
  delayMicroseconds(1);

  digitalWrite(EEPROM_CE_PIN, HIGH);
  delayMicroseconds(1);

  return bval;
}

//******************************************************************************************************************//
//* Lettura della EEPROM
//******************************************************************************************************************//
void readEEPROM(e_rom_type romtype, unsigned long size) {
  unsigned int addr = 0;

  for (int i = 0; i < size; i++) {
    byte bval = readByte(romtype, addr + i);
    Serial.write(&bval, 1);
  }
}

//******************************************************************************************************************//
//* Scrittura della EEPROM
//******************************************************************************************************************//
void writeEEPROM(e_rom_type romtype, unsigned long size)
{
  unsigned int address = 0;

  while (address < size)
  {
    // single byte write    
    byte val = 0;
    if (Serial.available() > 0) {
      val = Serial.read();
      writeByte(romtype, address, val);
      byte wval = waitAndCheckWrite(romtype, val);
      Serial.write(&wval, 1);
      address++;
    }
  }
}

//******************************************************************************************************************//
//* Scrittura della EEPROM in modo paginato
//******************************************************************************************************************//
void writePagedEEPROM(e_rom_type romtype, unsigned long size, unsigned int pagesize)
{
  unsigned int address = 0;  

  while (address < size)
  {
    // page write
    unsigned int idx = 0;
    #define PAGE_SIZE 64
    byte page[PAGE_SIZE];
    while (idx < PAGE_SIZE) {
      if (Serial.available() > 0) {
        page[idx++] = Serial.read();
      }
    }
    byte val = writePage(romtype, address, page, PAGE_SIZE);
    byte wval = waitAndCheckWrite(romtype, val);
    // feedback al programmatore dei bytes letti da EPROM
    idx = 0;
    while (idx < PAGE_SIZE) {
      byte val = readByte(romtype, address + idx++);
      Serial.write(&val, 1);
    }
    address += PAGE_SIZE;
  }
}

//******************************************************************************************************************//
//* Disabilita Software Data Protection
//******************************************************************************************************************//
void disableSDP()
{
  writeByte(AT28C256, 0x5555, 0xaa);
  writeByte(AT28C256, 0x2aaa, 0x55);
  writeByte(AT28C256, 0x5555, 0x80);
  writeByte(AT28C256, 0x5555, 0xaa);
  writeByte(AT28C256, 0x2aaa, 0x55);
  writeByte(AT28C256, 0x5555, 0x20);
}

//******************************************************************************************************************//
//* Abilita Software Data Protection
//******************************************************************************************************************//
void enableSDP()
{
  writeByte(AT28C256, 0x5555, 0xaa);
  writeByte(AT28C256, 0x2aaa, 0x55);
  writeByte(AT28C256, 0x5555, 0xa0);
}
