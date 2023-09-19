#line 1 "c:\\Data\\oXs_logger_arduino\\sd.h"
#pragma once
#include "Arduino.h"

uint setupSdCard();

uint createcsvFile();
uint logCsvHeader();

void readCsvFile();

void logOnSD(uint32_t writeIdx , uint16_t len);
