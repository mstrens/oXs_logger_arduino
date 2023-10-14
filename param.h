#pragma once
#include <ctype.h>
#include "pico/stdlib.h"
#include "logger_config.h"
//#include "crsf_frames.h"

#define CONFIG_VERSION 7
struct CONFIG{
    uint8_t version = CONFIG_VERSION;
    uint8_t spiForSd = SPI_FOR_SD ; 
    uint8_t pinSpiCs = SPI_CS;
    uint8_t pinSpiMiso = SPI_MISO;
    uint8_t pinSpiMosi = SPI_MOSI;
    uint8_t pinSpiSclk = SPI_SCLK; 
    uint8_t pinSerialRx = SERIAL_IN_RX_GPIO;
    uint8_t pinLed = 16;
    uint8_t protocol = 'O' ; // O = oXs + csv
    uint32_t serialBaudrate = SERIAL_IN_BAUDRATE;
    uint8_t ledInverted;
    bool fieldToAdd[64] = {FIELDS_TO_ADD}; //flag to say if the field must be part of CSV or not (init is done within setup of config)
    uint8_t minField = MIN_FIELD;
    int32_t minValue = MIN_VALUE;
    uint8_t maxField = MAX_FIELD;
    int32_t maxValue = MAX_VALUE;
    uint8_t mode = MODE ;
    uint16_t minInterval = MIN_INTERVAL_MS ; 
    
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
                    

