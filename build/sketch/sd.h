#line 1 "c:\\Data\\oXs_logger_arduino\\sd.h"
#pragma once
#include "Arduino.h"
#include "RTCx.h"

uint setupSdCard();

uint createcsvFile();
uint logCsvHeader();

void readCsvFile();

void updateCreateFile( struct RTCx::tm *tm1); // tm1 contains date & time in RTCx format


void logOnSD(uint32_t writeIdx , uint16_t len);
