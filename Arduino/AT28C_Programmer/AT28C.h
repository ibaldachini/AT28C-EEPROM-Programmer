/*  
  AT28C_Programmer.ino - Programmatore EEPROM AT28C
  Copyright (C) 2023 DrVector
  
  Gestione EEPROM AT28C
*/

#include <Arduino.h>

//******************************************************************************************************************//
//* Lettura di un byte all'indirizzo selezionato
//******************************************************************************************************************//
byte readByte(unsigned int address);

//******************************************************************************************************************//
//* Scrittura di un byte all'indirizzo selezionato
//******************************************************************************************************************//
byte writeByte(unsigned int address, byte value);

//******************************************************************************************************************//
//* Scrittura della EEPROM
//******************************************************************************************************************//
void writeEEPROM(int size);

//******************************************************************************************************************//
//* Lettura della EEPROM
//******************************************************************************************************************//
void readEEPROM(int size);
