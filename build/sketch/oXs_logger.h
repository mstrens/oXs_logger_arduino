#line 1 "c:\\Data\\oXs_logger_arduino\\oXs_logger.h"
#pragma once


enum LEDState{
    STATE_STARTING = 0,
    STATE_NO_SD,
    STATE_WRITE_ERROR,
    STATE_NO_DATA,
    STATE_OVERFLOW,
    STATE_OK
};
