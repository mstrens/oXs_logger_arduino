
#include "logger_config.h"
#include "sd.h"
#include "SdFat.h"
#include "param.h"
#include "oXs_logger.h"



extern CONFIG config;
extern char const * listOfCsvFieldName[] ; 
extern uint8_t ledState;


//  Change the value of SD_CS_PIN if you are using SPI and
// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SPI_CS;
#else   // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SPI_CS;
#endif  // SDCARD_SS_PIN


// This example was designed for exFAT but will support FAT16/FAT32.
// Note: Uno will not support SD_FAT_TYPE = 3.
// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 1

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
//#define SPI_CLOCK SD_SCK_MHZ(20)

// parameters for sdfat configuration
//#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16))

// Preallocate 1GiB file.
const uint32_t PREALLOCATE_SIZE_MiB = 1UL; //1024UL;
const uint64_t PREALLOCATE_SIZE = (uint64_t)PREALLOCATE_SIZE_MiB << 20;
// Max length of file name including zero byte.
#define FILE_NAME_DIM 40

#if SD_FAT_TYPE == 0
typedef SdFat sd_t;
typedef File file_t;
#elif SD_FAT_TYPE == 1
typedef SdFat32 sd_t;
typedef File32 file_t;
#elif SD_FAT_TYPE == 2
typedef SdExFat sd_t;
typedef ExFile file_t;
#elif SD_FAT_TYPE == 3
typedef SdFs sd_t;
typedef FsFile file_t;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE


sd_t sd;
file_t csvFile;
file_t csvFileToRead; 

// You may modify the filename.  Digits before the dot are file versions.
char csvName[] = "oXsLog000.csv";
uint8_t csvFileError = 255;

void readCsvFile(){
    Serial.println("trying to read the csv");
    csvFile.sync();     // write the last data on SD
    csvFileToRead.close();
    if (!csvFileToRead.open(csvName, O_READ)) {       // try to open
        Serial.println("open csvName failed");
        csvFileError = 2;
        return;
    }
    Serial.println("reading the file");
    // read from the file until there's nothing else in it:
    int data;
    while ((data = csvFileToRead.read()) >= 0) {
        Serial.write(data);
    }
    // close the file:
    csvFileToRead.close();
}

uint createcsvFile() {
  csvFile.close();
  while (sd.exists(csvName)) {           // if file exist
    char* p = strchr(csvName, '.');      // search . in file name
    if (!p) {
      Serial.println("no dot in filename");
      csvFileError = 1;
      return csvFileError;
    }
    while (true) {
      p--;
      if (p < csvName || *p < '0' || *p > '9') {      // check that pos before is 0...9 (start with '.' position)
        Serial.println("Can't create file name");
      }
      if (p[0] != '9') {                 // when char is not 9, increase the value
        p[0]++;
        break;
      }
      p[0] = '0';                       // if 9, replace by 0
    }
  }
  if (!csvFile.open(csvName, O_RDWR | O_CREAT)) {       // try to open
    Serial.println("open csvName failed");
    csvFileError = 2;
    return csvFileError;
  }
  Serial.print("log file will be in "); Serial.println(csvName);   
  //if (!csvFile.preAllocate(PREALLOCATE_SIZE)) {
  //  Serial.println("preAllocate failed");
  //}
  
  //csvFileError = 0;
  //Serial.println("calling logCsvHeader");   
  
  return logCsvHeader();   // write a csv header in the file    
}

extern char csvBuf[CSV_MAX_BUFFER_LEN];

uint32_t maxWriteOnSdUs = 0;
uint32_t maxSyncOnSdUs = 0;

uint logCsvHeader(){            // write csv header in the file
    //uint8_t headerBuffer[80];
    //Serial.println("entering logcsvHeader");
    //if (csvFileError) {
    //    Serial.print("can not log csv header; csvFileError="); Serial.println(csvFileError);
    //    return; // skip when file was not created
    //}
    
    //Serial.println("writing Time");
    if (csvFile.write("Time (msec)") == 0){
        csvFileError = 3;
        Serial.println("Error writing csvHeader 1");
        return csvFileError;
    }; 
    for (uint8_t i=0; i<63 ; i++){
    //Serial.println("writing comma");
        if ( config.fieldToAdd[i]){                            // insert the data only if foreseen in the config
            if (csvFile.write(",") == 0){                      // write the comma
                csvFileError = 3;
                Serial.println("Error writing , in csvHeader");
                return csvFileError;  
            }
            if (csvFile.write(listOfCsvFieldName[i]) == 0){    // write the name of field
                csvFileError = 3;
                Serial.println("Error writing text in csvHeader");
                return csvFileError;
            }
        } //else {
        //    Serial.print("not in csv for i="); Serial.println(i);
        //}
    }
    if (csvFile.write("\r\n") ==0){
        csvFileError = 3;
        Serial.println("Error writing CRLF in csvHeader");
        return csvFileError;
    }
            
    if (csvFile.sync() == false){
        csvFileError = 3;
        Serial.println("Error sync in csvHeader");
        return csvFileError;
    }
    //Serial.println("calling readCsvFile");
    //readCsvFile();
    return 0 ; // 0 means that no error occured       
}



void logOnSD(uint32_t writeIdx , uint16_t len){    // return the time in usec
    static uint32_t lastSyncMillis;
    // Wait until SD is not busy.
    
    uint32_t usec = micros();
    
    while (sd.card()->isBusy()) {
    }

    // Time to log next record.
    uint32_t writtenLen;
    writtenLen = csvFile.write(&csvBuf[writeIdx],  len);
    if (writtenLen != len) {
        Serial.print("write csvFile failed written len="); Serial.print(writtenLen);
        Serial.print("   exepected len="); Serial.println(len);
        ledState = STATE_WRITE_ERROR;
    } else {
        ledState = STATE_OK;
    }
    usec = micros() - usec;
    if (usec > maxWriteOnSdUs) {
        maxWriteOnSdUs = usec;      // register max time to write
    }
    
    // at regular basis (e.g every 10 sec) perform a synchronization on SD cad (to avoid loosing all data)
    if (( millis() - lastSyncMillis ) > 10000 ) {
    //binFile.truncate();
        usec = micros();
        csvFile.sync();
        usec = micros() - usec;
        if (usec > maxSyncOnSdUs) {
            maxSyncOnSdUs = usec;
        }
        lastSyncMillis = millis();
        //Serial.print(F("syncMicros:")); Serial.print(usec);
        //Serial.print(F(" maxWriteMicros:")); Serial.println(maxWriteMicros);
    }    
}

uint setupSdCard(){
    delay(1000);
    SPI.setRX(SPI_RX);
    SPI.setCS(SPI_CS);
    SPI.setSCK(SPI_SCK);
    SPI.setTX(SPI_TX);

    // Initialize SD.
    if (!sd.begin(SD_CONFIG)) {
        Serial.println("error in sd.begin");
        //sd.initErrorHalt(&Serial);
        return 1;
    }
    
    //Serial.println("trying to create csv");

    return createcsvFile();
    //Serial.println("End of setup SD card");
}

