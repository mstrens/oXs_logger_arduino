#line 1 "c:\\Data\\oXs_logger_arduino\\param.h"
#pragma once
#include <ctype.h>
#include "pico/stdlib.h"
#include "logger_config.h"
//#include "crsf_frames.h"

#define CONFIG_VERSION 6
struct CONFIG{
    uint8_t version = CONFIG_VERSION;
    uint8_t pinSpiCs = SPI_CS;
    uint8_t pinSpiMiso = SPI_RX;
    uint8_t pinSpiMosi = SPI_TX;
    uint8_t pinSPiSck = SPI_SCK; 
    uint8_t pinSerialRx = SERIAL_IN_RX_GPIO;
    uint8_t pinLed = 16;
    uint8_t protocol = 'O' ; // O = oXs + csv
    uint32_t serialBaudrate = SERIAL_IN_BAUDRATE;
    uint8_t ledInverted;
    bool fieldToAdd[64];
};

void handleUSBCmd(void);
void processCmd(void);

char * skipWhiteSpace(char * str);
void removeTrailingWhiteSpace( char * str);
void upperStr( char *p);
void saveConfig();                 // save the config
void cpyChannelsAndSaveConfig();   // copy the channels values and save them into the config.
void addPinToCount(uint8_t pinId);
void checkConfig();
void setupConfig();
void printConfig();
void printConfigOffsets();
void printFieldValues();
void removeSpaces(char *str);
int _msprintf(char *format, ...);
void updateFromToConfig(uint8_t fromValue, uint8_t toValue, bool flag);
void printCsvFile();
                    

