#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

// tipologie memorie conosciute
typedef enum {
  AT28C64,
  AT28C256,
  E2764,
  E27128,
  E27256,
  NONE
} e_rom_type;


// restituisce la dimensione della memoria in bytes
int getTotalBytes(e_rom_type romtype);

// invia il comando di richiesta della versione del firmware
int requestFirmware(int fd);

// invia il comando di richiesta lettura della memoria
int requestRead(int fd, e_rom_type romtype);

// invia il comando di richiesta scrittura della memoria
int requestWrite(int fd, e_rom_type romtype, bool paged);

// legge e visualizza o salva nel buffer la risposta dal programmatore
int readAnswer(int fd, char* buffer, long msec);

// legge la risposta dal programmatore con il contenuto della memoria e lo salva sul file indicato, attende la risposta per max msec millisecondi 
int readEprom(int fd, e_rom_type romtype, char* filename, long msec);

// legge la risposta dal programmatore con il contenuto della memoria e lo verifica con il contenuto del file indicato, attende la risposta per max msec millisecondi 
int verifyEprom(int fd, e_rom_type romtype, char* filename, long msec);

// invia al prorammatore i dati da scrivere leggendoli dal file indicato, per ogni byte attende al massimo msecforbyte millisecondi
int writeEprom(int fd, e_rom_type romtype, bool paged, char* filename, long msecforbyte);

// setup Software Data Protection
int setupSDP(int fd, bool enable, long msec);

// invia al programmatore la locazione di memoria e il byte da scrivere
int requestWriteByte(int fd, int address, unsigned char val, long msecforbyte);

// legge al programmatore la locazione di memoria da leggere
int requestReadByte(int fd, e_rom_type romtype, int address, long msecforbyte);

// legge la risposta dal programmatore con il contenuto della memoria ed esegueil blank check, attende la risposta per max msec millisecondi 
int blankCheck(int fd, e_rom_type romtype, long msec);

// applicazione principale
int main (int argc, char **argv) {
  // selezione memoria di default
  e_rom_type romtype = NONE;

  // indicatore operazione
  char operation = 0;

  // indicatore operazione paginata
  bool paged = false;

  // indicatore leura scrittura singolo byte
  bool singlebyte = false;

  // nome del device seriale a cui è collegato il programmatore
  char *device = NULL;

  // nome del file da leggere o scrivere
  char *filename = NULL;

  // indirizzo da leggere o scrivere
  int address = -1;

  // valore da scrivere
  int val = -1;

  // effettua il parsing dei parametri passati da linea di comando
  int c;
  while ((c = getopt (argc, argv, "d:f:t:o:a:b:")) != -1) {
    switch (c) {
      // nome della seriale alla quale è connesso il programmatore
      case 'd':
        device = optarg;
        break;
      // nome del file da leggere o scrivere
      case 'f':
        filename = optarg;
        break;
      // selezione tipologia di memoria
      case 't':
        if (strcmp("AT28C64", optarg) == 0) romtype = AT28C64;
        if (strcmp("AT28C256", optarg) == 0) romtype = AT28C256;
        if (strcmp("2764", optarg) == 0) romtype = E2764;
        if (strcmp("27128", optarg) == 0) romtype = E27128;
        if (strcmp("27256", optarg) == 0) romtype = E27256;
        if (romtype == NONE) {
          printf("unknown romtype\n");
        }
        break;
      case 'o':
        // opzione per la scrittura della memoria
        if (optarg[0] == 'w') {
          operation = 'w';
          // opzione per la scrittura della memoria paginata
          if (optarg[1] == 'p') {
            paged = true;
          }
          // opzione per la scrittura di un byte
          if (optarg[1] == 'b') {
            singlebyte = true;
          }
        // opzione per la verifica della memoria
        } else if (optarg[0] == 'v') {
          operation = 'v';
        // opzione per la lettura della memoria
        } else if (optarg[0] == 'r') {
          operation = 'r';
          // opzione per la lettura di un byte
          if (optarg[1] == 'b') {
            singlebyte = true;
          }
        // opzione per l'abilitazione del software data protection
        } else if (optarg[0] == 'e') {
          operation = 'e';
        // opzione per la disabilitazione del software data protection
        } else if (optarg[0] == 'd') {
          operation = 'd';
        // opzione per il blank check
        } else if (optarg[0] == 'b') {
          operation = 'b';
        }
        else {
          printf("unknown operation\n");
        }
        break;
      // indirizzo da leggere o scrivere
      case 'a':
        if (optarg[0] == 'x') {
          sscanf(optarg + 1, "%x", &address);
        } else {
          address = atoi(optarg);
        }
        break;
      // valore da scrivere
      case 'b':
        if (optarg[0] == 'x') {
          int ival;
          sscanf(optarg + 1, "%x", &ival);
          val = ival;
        } else {
          val = atoi(optarg);
        }
        if (val > 255) {
          printf("wrong value\n");
          return -1;
        }
        break;
    }
  }

  int totalbytes = getTotalBytes(romtype);

  if (address >= totalbytes) {
    printf("wrong address\n");
    address = -1; // print help if needed
  }

  // se non sono stati impostati gli argomenti obbligatori visualizza l'help ed esce
  if (device == NULL ||
      operation == 0 ||
      romtype == NONE ||
      (filename == NULL && (operation == 'w' || operation == 'v') && singlebyte == false) ||
      (address == -1 && (operation == 'w' || operation == 'r') && singlebyte == true) ||
      (val == -1 && operation == 'w' && singlebyte == true)) {
    printf("AT28CProgrammer V.1.1\n");
    printf("use: AT28CProgrammer -d <device> -t <romtype> -o <operation> [-a <address>] [-b <byte>] [-f <filename>]\n");
    printf("\t-d: serial port\n");
    printf("\t-t AT28C64: eeprom type AT28C64\n");
    printf("\t-t AT28C256: eeprom type AT28C256\n");
    printf("\t-t 2764: eprom type 2764\n");
    printf("\t-t 27128: eprom type 27128\n");
    printf("\t-t 27256: eprom type 27256\n");
    printf("\t-o r: set to read eprom (save to file or dump to screen if no file selected)\n");
    printf("\t-o rb: set to read byte (needed -a parameter)\n");
    printf("\t-o w: set to write eprom\n");
    printf("\t-o wp: set to paged write eprom (only supported by AT28C256)\n");
    printf("\t-o wb: set to write byte (needed -a and -b parameters)\n");
    printf("\t-o v: set to verify eprom\n");
    printf("\t-o e: set to enable software data protection\n");
    printf("\t-o d: set to disable software data protection\n");
    printf("\t-o b: blank check\n");
    printf("\t-a: address to read or write for single byte mode (decimal or preceded with x for hex)\n");
    printf("\t-b: byte to write for single byte mode (decimal or preceded with x for hex)\n");
    printf("\t-f: file name to read or write\n");
    printf("read  example:        AT28CProgrammer -d /dev/ttyUSB0 -t AT28C256 -o r -f /tmp/dump.bin\n");
    printf("write example:        AT28CProgrammer -d /dev/ttyUSB0 -t AT28C64 -o w -f /tmp/towrite.bin\n");
    printf("read byte example:    AT28CProgrammer -d /dev/ttyUSB0 -t AT28C64 -o rb -a 4096\n");
    printf("read byte example:    AT28CProgrammer -d /dev/ttyUSB0 -t AT28C64 -o rb -a x1000\n");
    printf("blank check example:  AT28CProgrammer -d /dev/ttyUSB0 -t 2764 -o b\n");
    return -1;
  }

  // visualizza la memoria selezionata
  if (romtype == AT28C64) {
    printf("selected AT28C64\n");
  } else if (romtype == AT28C256) {
    printf("selected AT28C256\n");
  } else if (romtype == E2764) {
    printf("selected 2764\n");
  } else if (romtype == E27128) {
    printf("selected 27128\n");
  } else if (romtype == E27256) {
    printf("selected 27256\n");
  }

  // apre la comunicazione con il programmatore tramite la porta seriale
  int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    printf("open_port: Unable to open device\n");
    return -1;
  }

  // impostazione dei parametri della porta seriale
  struct termios tio;
  if (tcgetattr(fd, &tio) != 0) {
    close(fd);
    printf("error tcgetattr\n");
    return -1;
  }

  // minimum number of character for non canonical read
  tio.c_cc[VMIN] = 1;
  // timeout in deciseconds for non canonical read
  tio.c_cc[VTIME] = 0;
  tio.c_iflag = 0;
  tio.c_lflag = 0;
  tio.c_cflag = 0;
  tio.c_oflag = 0;

  // ignore break condition on input
  tio.c_iflag |= IGNBRK;
  // 8 bit data
  tio.c_cflag |= CS8;
  // enable receiver, lower modem contron on close device, ignore input modem control lines
  tio.c_cflag |= CREAD | HUPCL | CLOCAL;

  // 115200 baud
  cfsetispeed(&tio, B115200);
  cfsetospeed(&tio, B115200);

  // apply settings to serial port
  if (tcsetattr(fd, TCSANOW, &tio) != 0) {
    close(fd);
    printf("error tcsetattr\n");
    return -1;
  }

  // imposta la seriale in modalità non blocking durante la lettura e pulisce eventuali comunicazioni residue
  fcntl(fd, F_SETFL, FNDELAY);
  usleep(100000);
  tcflush(fd, TCIOFLUSH);

  // invia il comando di richiesta firmware
  if (requestFirmware(fd) == -1) {
    close(fd);
    printf("error request firmware version\n");
    return -1;
  }

  // attende la risposta per un massimo di 100 ms (se il dispositivo non è stato resettato nell'apertura della comunicazione risponderà qui)
  char fwbuf[64];
  if (readAnswer(fd, fwbuf, 100) == -1) {
    // non ha ricevuto la risposta alla versione firmware
    // attende l'eventuale intestazione inviata dal programmatore per massimo 1.5 secondi
    if (readAnswer(fd, NULL, 1500) == -1) {
      close(fd);
      printf("error reading header\n");
      return -1;
    }

    // invia il comando di richiesta firmware
    if (requestFirmware(fd) == -1) {
      close(fd);
      printf("error request firmware version\n");
      return -1;
    }

    // attende la risposta contentente la versione firmware per un massimo di 100 ms
    if (readAnswer(fd, fwbuf, 100) == -1) {
      close(fd);
      printf("error reading firmware version\n");
      return -1;
    }
  }
  // fwbuf: +VERSION=0.004
  if (memcmp(fwbuf, "+VERSION=", 9) != 0) {
    close(fd);
    printf("error reading firmware version\n");
    return -1;
  }
  printf("%s\n", fwbuf);
  fwbuf[10] = 0;
  fwbuf[14] = 0;
  unsigned int maj = atoi(fwbuf + 9);
  unsigned int min = atoi(fwbuf + 11);
  unsigned int fwver = maj * 1000 + min;
  if (fwver < 4) {
    close(fd);
    printf("please update firmware on AT28C EEPROM PROGRAMMER\n");
    return -1;
  }

  // verifica se richiesta verifica della memoria
  if (operation == 'v') {
    // invia il comando di richiesta lettura della memoria selezionata
    if (requestRead(fd, romtype) == -1) {
      close(fd);
      printf("error request read eprom\n");
      return -1;
    }
    // legge la risposta con il contenuto della memoria e lo verifica con quanto presente su file
    if (verifyEprom(fd, romtype, filename, 100) == -1) {
      close(fd);
      printf("error verifying eprom\n");
      return -1;
    }
  }
  // verifica se richiesta scrittura della memoria
  else if (operation == 'w') {
    if (singlebyte) {
      // invia il comando di richiesta scrittura della byte
      if (requestWriteByte(fd, address, val, 100) == -1) {
        close(fd);
        printf("error request write byte\n");
        return -1;
      }
      // attende la risposta contentente il byte presente nella EPROM dopo la scrittura
      char buffer[64];
      if (readAnswer(fd, buffer, 100) == -1) {
        close(fd);
        printf("error reading firmware version\n");
        return -1;
      }
      if (memcmp(buffer, "+WRITEBYTE=", 11) == 0) {
        int wval;
        sscanf(buffer + 11, "%d", &wval);
        unsigned char c = (unsigned char)wval;
        if (c != val) {
          printf("write error, read byte %u [x%02X] at address %u [x%04X]\n", c, c, (unsigned int)address, (unsigned int)address);
        } else {
          printf("written byte %u [x%02X] at address %u [x%04X]\n", c, c, (unsigned int)address, (unsigned int)address);
        }
      }
    } else {
      // invia il comando di richiesta scrittura della memoria selezionata
      if (requestWrite(fd, romtype, paged) == -1) {
        close(fd);
        printf("error request write eprom\n");
        return -1;
      }
      // invia il contenuto del file da scrivere, per ogny byte scritto attende al massimo 10 ms per la scrittura
      if (writeEprom(fd, romtype, paged, filename, 100) == -1) {
        close(fd);
        printf("error write eprom\n");
        return -1;
      }
    }
  }
  // verifica se richiesta lettura della memoria
  else if (operation == 'r') {
    if (singlebyte) {
      // invia il comando di richiesta scrittura della byte
      if (requestReadByte(fd, romtype, address, 100) == -1) {
        close(fd);
        printf("error request read byte\n");
        return -1;
      }
      // attende la risposta contentente il byte letto dalla EPROM
      char buffer[64];
      if (readAnswer(fd, buffer, 100) == -1) {
        close(fd);
        printf("error reading firmware version\n");
        return -1;
      }
      if (memcmp(buffer, "+READBYTE=", 10) == 0) {
        int val;
        sscanf(buffer + 10, "%d", &val);
        unsigned char c = (unsigned char)val;
        printf("read byte %u [x%02X] at address %u [x%04X]\n", c, c, (unsigned int)address, (unsigned int)address);
      }
    } else {
      // invia il comando di richiesta lettura della memoria selezionata
      if (requestRead(fd, romtype) == -1) {
        close(fd);
        printf("error request read eprom\n");
        return -1;
      }
      // legge la risposta con il contenuto della memoria e lo salva su file
      if (readEprom(fd, romtype, filename, 100) == -1) {
        close(fd);
        printf("error reading eprom\n");
        return -1;
      }
    }
  }
  // verifica se richiesta abilitazione del software data protection
  else if (operation == 'e') {
    // invia il comando di richiesta abilitazione del software data protection
    if (setupSDP(fd, true, 100) == -1) {
      close(fd);
      printf("error enabling software data protection\n");
      return -1;
    }
  }
  // verifica se richiesta disabilitazione del software data protection
  else if (operation == 'd') {
    // invia il comando di richiesta disabilitazione del software data protection
    if (setupSDP(fd, false, 100) == -1) {
      close(fd);
      printf("error disabling software data protection\n");
      return -1;
    }
  }
  // verifica se richiesto blank check
  else if (operation == 'b') {
    // invia il comando di richiesta lettura della memoria selezionata
    if (requestRead(fd, romtype) == -1) {
      close(fd);
      printf("error request read eprom\n");
      return -1;
    }
    if (blankCheck(fd, romtype, 100) == -1) {
      close(fd);
      printf("blank check error\n");
      return -1;
    }
  }

  close(fd);

  fflush(stdout);

  return 0;
}

// restituisce la dimensione della memoria in bytes
int getTotalBytes(e_rom_type romtype) {
  int totalbytes = 0;
  if (romtype == AT28C64 || romtype == E2764) {
    totalbytes = 8192;
  }
  if (romtype == E27128) {
    totalbytes = 16384;
  }
  if (romtype == AT28C256 || romtype == E27256) {
    totalbytes = 32768;
  }
  return totalbytes;
}

// invia il comando di richiesta della versione del firmware
int requestFirmware(int fd) {
  const char* cmdGetVersion = "VERSION=?\r";
  return write(fd, cmdGetVersion, strlen(cmdGetVersion));
}

// invia il comando di richiesta lettura della memoria
int requestRead(int fd, e_rom_type romtype) {
  tcflush(fd, TCIOFLUSH);

  int totalbytes = getTotalBytes(romtype);

  const char* cmdRead = "READEEPROM=%d,%d\r";
  char buf[64];
  sprintf(buf, cmdRead, romtype, totalbytes);
  if (totalbytes != 0) {
    return write(fd, buf, strlen(buf));
  }
  return -1;
}

// invia il comando di richiesta scrittura della memoria
int requestWrite(int fd, e_rom_type romtype, bool paged) {
  tcflush(fd, TCIOFLUSH);

  int totalbytes = getTotalBytes(romtype);

  const char* cmdWrite = "WRITEEEPROM=%d";
  char buf[64];
  sprintf(buf, cmdWrite, totalbytes);
  if (paged) {
    strcat(buf, ",64");
  }
  strcat(buf, "\r");
  if (totalbytes != 0) {
    return write(fd, buf, strlen(buf));
  }
  return -1;
}

// legge e visualizza o salva nel buffer la risposta dal programmatore
int readAnswer(int fd, char* buffer, long msec) {
  int readed = 0;
  while (true) {
    fd_set rfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec = (msec * 1000) / 1000000;
    tv.tv_usec = (msec * 1000) % 1000000;

    retval = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1) {
      printf("error select\n");
      return -1;
    } else if (retval > 0) {
      // ricevuto risposta legge 1 carattere e lo stampa
      char c;
      read(fd, &c, 1);
      // filtra i caratteri di line feed e carriage return
      if (c != '\n' && c != '\r') {
        if (buffer == NULL) {
          printf("%c", c);
        } else {
          buffer[readed] = c;
          buffer[readed + 1] = 0;
        }
      }
      readed++;
      // ha ricevuto almeno un carattere,
      // imposta il timeout per i prossimi caratteri a 100 ms
      msec = 100;
    } else {
      // timeout attesa risposta
      break;
    }
  }

  // se ha ricevuto dei caratteri solo alla fine visualizza un fine riga
  if (readed) {
    if (buffer == NULL) {
      printf("\n");
    }
    return 0;
  }

  // se non ha ricevuto risposta ritorna una indicazione di errore
  return -1;
}

// legge la risposta dal programmatore con il contenuto della memoria e lo salva sul file indicato, attende la risposta per max msec millisecondi 
int readEprom(int fd, e_rom_type romtype, char* filename, long msec) {
  int writefd = -1;
  int readed = 0;
  int lastperc = -1;
  int totalbytes = getTotalBytes(romtype);
  if (filename != NULL) {
    unlink(filename);
    writefd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (writefd == -1) {
      printf("error opening output file\n");
      return -1;
    }
  }
  char buf[128];
  char bufr[16 + 1];
  while (true) {
    fd_set rfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec = (msec * 1000) / 1000000;
    tv.tv_usec = (msec * 1000) % 1000000;

    retval = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1) {
      printf("error select\n");
      return -1;
    } else if (retval > 0) {
      unsigned char c;
      read(fd, &c, 1);
      if (filename != NULL) {
        write(writefd, &c, 1);
      } else {
        if (readed % 16 == 0) {
          sprintf(buf, "x%04X: ", readed);
        }
        sprintf(buf + strlen(buf), " x%02X", c);
        bufr[readed % 16] = c >= 0x20 && c < 0x7F ? c : '.';
      }
      readed++;
      if (filename == NULL && readed % 16 == 0) {
        bufr[16] = 0;
        sprintf(buf + strlen(buf), "  -  | %s |\n", bufr);
        printf(buf);
      }
      int perc = readed * 100 / totalbytes;
      if (perc != lastperc) {
        if (filename != NULL) {
          printf("<- read percent: %d%%\r", perc);
        }
        fflush(stdout);
        lastperc = perc;
      }
    } else {
      // timeout attesa risposta
      break;
    }
  }

  if (filename != NULL && readed) {
    printf("\n");
  }

  // visualizza il numero di bytes ricevuti
  printf("read: %d\n", readed);
  if (filename != NULL) {
    close(writefd);
  }

  // verifica se ha ricevuto il numero di bytes attesi
  if (readed != totalbytes) {
    return -1;
  }

  return 0;
}

// legge la risposta dal programmatore con il contenuto della memoria e lo verifica con il contenuto del file indicato, attende la risposta per max msec millisecondi 
int verifyEprom(int fd, e_rom_type romtype, char* filename, long msec) {
  int readed = 0;
  int errors = 0;
  int lastperc = -1;
  int totalbytes = getTotalBytes(romtype);
  int readfd = open(filename, O_RDONLY);
  if (readfd == -1) {
    printf("error opening input file\n");
    return -1;
  }

  while (true) {
    fd_set rfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec = (msec * 1000) / 1000000;
    tv.tv_usec = (msec * 1000) % 1000000;

    retval = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1) {
      printf("error select\n");
      return -1;
    } else if (retval > 0) {
      char c;
      read(fd, &c, 1);
      char rc;
      read(readfd, &rc, 1);
      if (c != rc) {
          printf("\n-> address: 0x%04X, eprom byte: 0x%02X, file byte: 0x%02X\n", (unsigned int)readed, (unsigned char)c, (unsigned char)rc);
          errors++;
      }
      if (errors >= 3) {
          printf("\n-> print maximum three errors\r");
          break;
      }
      readed++;
      int perc = readed * 100 / totalbytes;
      if (perc != lastperc) {
        printf("<- verify percent: %d%%\r", perc);
        fflush(stdout);
        lastperc = perc;
      }
    } else {
      // timeout attesa risposta
      break;
    }
  }

  if (readed) {
    printf("\n");
  }

  // visualizza il numero di bytes ricevuti
  printf("verified: %d\n", readed);
  close(readfd);

  // verifica se ha ricevuto il numero di bytes attesi
  if (readed != totalbytes) {
    return -1;
  }

  if (errors) {
    printf("%d errors found\n", errors);
    return -1;
  }

  return 0;
}

// invia al prorammatore i dati da scrivere leggendoli dal file indicato, per ogni byte attende al massimo msecforbyte millisecondi
int writeEprom(int fd, e_rom_type romtype, bool paged, char* filename, long msecforbyte) {
  size_t written = 0;
  int lastperc = -1;
  int totalbytes = getTotalBytes(romtype);
  int readfd = open(filename, O_RDONLY);
  if (readfd == -1) {
    printf("error opening input file\n");
    return -1;
  }
  char c;
  size_t blocksize = paged ? 64 : 1;
  unsigned char buf[blocksize];
  while (read(readfd, buf, blocksize) == blocksize) {
    write(fd, buf, blocksize);

    size_t recvd = 0;
    unsigned char rbuf[blocksize];
    while (recvd < blocksize) {
      fd_set rfds;
      struct timeval tv;
      int retval;

      FD_ZERO(&rfds);
      FD_SET(fd, &rfds);

      tv.tv_sec = (msecforbyte * 1000) / 1000000;
      tv.tv_usec = (msecforbyte * 1000) % 1000000;

      retval = select(fd + 1, &rfds, NULL, NULL, &tv);
      if (retval == -1) {
        printf("error select\n");
        return -1;
      } else if (retval > 0) {
        char rc;
        recvd += read(fd, rbuf + recvd, blocksize - recvd);
      } else {
        // timeout attesa risposta scrittura byte
        printf("write timeout\n");
        break;
      }
    }

    bool err = false;
    for (size_t idx = 0; idx < blocksize; idx++) {
      if (rbuf[idx] != buf[idx]) {
        printf("\n-> written byte: %u [x%02X], read byte: %u [x%02X]\n", buf[idx], buf[idx], rbuf[idx], rbuf[idx]);
        err = true;
        break;
      }
    }

    if (err) {
      break;
    }

    written += blocksize;
    int perc = written * 100 / totalbytes;
    if (perc != lastperc) {
      printf("-> write percent: %d%%\r", perc);
      fflush(stdout);
      lastperc = perc;
    }
  }

  if (written) {
    printf("\n");
  }

  // visualizza il numero di bytes scritti
  printf("written: %d\n", written);
  close(readfd);

  // verifica se ha scritto il numero di bytes attesi
  if (written != totalbytes) {
    return -1;
  }

  return 0;
}

// setup Software Data Protection
int setupSDP(int fd, bool enable, long msec) {
  tcflush(fd, TCIOFLUSH);

  const char* cmdEnableSDP = "ENABLESDP=1\r";
  const char* cmdDisableSDP = "ENABLESDP=0\r";
  if (enable) {
    printf("enable software data protection\n");
    return write(fd, cmdEnableSDP, strlen(cmdEnableSDP));
  } else {
    printf("disable software data protection\n");
    return write(fd, cmdDisableSDP, strlen(cmdDisableSDP));
  }
  return -1;
}

// invia al programmatore la locazione di memoria e il byte da scrivere
int requestWriteByte(int fd, int address, unsigned char val, long msecforbyte) {
  tcflush(fd, TCIOFLUSH);

  const char* cmdWriteByte = "WRITEBYTE=%d,%d\r";
  char buff[32];
  sprintf(buff, cmdWriteByte, address, val);
  printf("write byte %u [x%02X] at address %u [x%04X]\n", (unsigned char)val, (unsigned char)val, (unsigned int)address, (unsigned int)address);
  return write(fd, buff, strlen(buff));
}

// legge al programmatore la locazione di memoria da leggere
int requestReadByte(int fd, e_rom_type romtype, int address, long msecforbyte) {
  tcflush(fd, TCIOFLUSH);

  const char* cmdReadByte = "READBYTE=%d,%d\r";
  char buff[64];
  sprintf(buff, cmdReadByte, romtype, address);
  printf("read byte from address %u [x%04X]\n", (unsigned int)address, (unsigned int)address);
  return write(fd, buff, strlen(buff));
}

// legge la risposta dal programmatore con il contenuto della memoria ed esegueil blank check, attende la risposta per max msec millisecondi 
int blankCheck(int fd, e_rom_type romtype, long msec) {
  int readed = 0;
  int errors = 0;
  int lastperc = -1;
  int totalbytes = getTotalBytes(romtype);

  while (true) {
    fd_set rfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec = (msec * 1000) / 1000000;
    tv.tv_usec = (msec * 1000) % 1000000;

    retval = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1) {
      printf("error select\n");
      return -1;
    } else if (retval > 0) {
      unsigned char c;
      read(fd, &c, 1);
      if (c != 0xFF) {
          printf("\n-> address: 0x%04X, eprom byte: 0x%02X, blank check failed\n", (unsigned int)readed, (unsigned char)c);
          errors++;
      }
      if (errors >= 3) {
          printf("\n-> print maximum three errors\r");
          break;
      }
      readed++;
      int perc = readed * 100 / totalbytes;
      if (perc != lastperc) {
        printf("<- verify percent: %d%%\r", perc);
        fflush(stdout);
        lastperc = perc;
      }
    } else {
      // timeout attesa risposta
      break;
    }
  }

  if (readed) {
    printf("\n");
  }

  // visualizza il numero di bytes ricevuti
  printf("checked: %d\n", readed);

  // verifica se ha ricevuto il numero di bytes attesi
  if (readed != totalbytes) {
    return -1;
  }

  if (errors) {
    printf("%d errors found\n", errors);
    return -1;
  }

  return 0;
}
