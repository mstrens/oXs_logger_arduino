#line 1 "c:\\Data\\oXs_logger_arduino\\serial_in.h"
#pragma once


void setupSerialIn();
void handleSerialIn();

void setupTest();
bool sendTest_callback(struct repeating_timer *t);
//void sendTest();

void fillCsvBuffer();
void writeRec();
void updateBytesInBuf();
void setupQueues();

void writeOnSdcard();

void reportStats(uint interval);

void printTestCount();
struct TLM {
    uint8_t type;
    uint32_t data ;
};

struct writeQueue_t {
    uint32_t idx;
    uint16_t len;
};