#line 1 "c:\\Data\\oXs_logger_arduino\\README.md"
# Logger for openXsensor (oXs) on RP2040 board
This project allows you to log data generated by oXs on a SD card.

It uses his own RP2040 board and a SD card socket.
\
\
In the future:
* optionally, a RTC (real time clock) module could be attached in order to know when the files have been created.
* it should be possible to use this firmware to record data provided by another device than oXs (so with no reformatting)


\
In current version, the logger:
* creates a new empty file on the Sd card at each start up. The file name is oXsLogXXX.csv where XXX is a growing seuence number.
* get messages from oXs in a compressed binary format.
* each message contains a synchro byte, a timestamp (number of microsec since RP2040 start up) and one or several new data's (type+ value)
* each message is checked and uncompressed in the logger.
* after each message, the new data's are merged with previously received data's in order to build a "current" situation that can contain up to 63 types of data
* each "current" situation is then formatted in a CSV format and store on the SD card with the timestamp 
* at regular interval (e.g. 10 sec), the logger synchronise the file on the Sd card in order to avoid loose of all data's on power down 
* so, in case of power off, only the last seconds will be loosen

Note: when oXs provide GPS date and time, the log file get a creation timestamp equal to the first date and time provided by oXs.
There can be a delay because GPS has to get some satellites.
## -------  Hardware -----------------

This project requires a board with a RP2040 processor like the rapsberry pi pico.

A better alternative is the RP2040-Zero or the RP2040-TINY (both have the same processor but smaller board)

This board has to be connected at least to a SD card socket (and to another module running oXs_on_RP2040).

If you use a prebuild module for the SD card socket, select it taking care that the RP2040 does not support more than 3.3V on his GPIO/SPI pins. 

You can select a board that has both a Sd card socket and a RTC like this: https://fr.aliexpress.com/item/32854699455.html?spm=a2g0o.productlist.main.45.1e7942acekWpV4&algo_pvid=378272fa-5bc2-4836-bee7-385ad1935ac0&algo_exp_id=378272fa-5bc2-4836-bee7-385ad1935ac0-22&pdp_npi=4%40dis%21EUR%211.78%211.69%21%21%211.86%21%21%402101f49516942528977128938ebd33%2165260014220%21sea%21BE%21194729803%21S&curPageLogUid=3vzNU8gIslKE

If you want to avoid wiring the rp2040 board with the SD card socket board, you can select a board that combines the RP2040 with the Sd card socket (but it is larger and more more expensive): https://www.tindie.com/products/invector/challenger-rp2040-sdrtc/ 



## --------- Wiring --------------------

The RP2040 logger has to be connected to the Sd card socket. This requires 6 wires:
* VCC (normally 3.3V but could be 5V depending on the sd card module that you use)
* Gnd
* CS (chip select from Sd card)
* MISO (from Sd card) 
* SLCK (from Sd card)
* MOSI (from Sd card)
The Sd card can use the first or the second SPI available on RP2040.
Each SPI requires different gpio's.
Selection of SPI and gpio's id done in logger_config.h file but can also be modified with usb commands.  



RP2040 logger must also be connected to oXs with 3 wires:
* VCC <=> VCC (max 6 v like oXs vcc) 
* Gnd <=> Gnd
* gpio 5 (=UART1) from RP2040 logger <=> log Gpio (as defined by your self on oXs)  
Note: 5 can be replaced by 9, 21 or 25 in logger_config.h file or with usb commands.


## --------- Software -------------------
This software has been developped using the arduino IDE under VSCODE.

If you just want to use it, there is (in most cases) no need to install/use any tool.
* download from github the zip file containing all files and unzip them where you want.
* in your folder, in subfolder "build", there is a file named oXs_logger_arduino.ino.uf2; this is a compiled version of this software that can be directly uploaded and configured afterwards
* insert the USB cable in the RP2040 board
* press on the "boot" button on the RP2040 board while you insert the USB cable in your PC.
* this will enter the RP2040 in a special bootloader mode and your pc should show a new drive named RPI-RP2
* copy and paste (or drag and drop) the .uf2 file to this new drive
* the file should be automatically picked up by the RP2040 bootloader and flashed
* the RPI_RP2 drive should disapear from the PC and the PC shoud now have a new serial port (COMx on windows)

\
\
You can now use a serial terminal (like putty , the one from arduino IDE, the serial monitor from VSCode,...) and set it up for 115200 baud 8N1
* while the RP2040 is connected to the pc with the USB cable, connect this serial terminal to the serial port from the RP2040
* when the RP2040 starts (or pressing the reset button), press Enter to display the current configuration or ?+ENTER to display the commands to change the configuration.
* if you want to change some parameters (e.g. the list of fields to include in the CSV), fill in the command (code=value) and press the enter.
* the RP2040 should then display the new (saved) config.
Note: do not forget to setup your serial monitor in such a way that it sent CR+LF (carriage return / line feed) each time you press enter.

\
\
Developers can change the firmware with Arduino IDE (configured for RP2040 boards AND sdfat library). You have also to specify the type of board being used (e.g. RP2040 Zero)
* Once the tools are installed, copy all files provided on github on you PC (keeping the same structure).  
* Open VScode or Arduino IDE and open the folder where you put the oXs logger files.
* In VScode, press CTRL+SHIFT+P and in the input line that appears, enter (select) Arduino : Upload  
* This will create some files needed for the compilation and automatically upload the program.  

Note :  the file logger_config.h contains some #define that can easily be edited to change some default/advanced parameters.

## --- Fields being logged ---

oXs can capture:
* more than 30 different telemetry fields (number depends on the sensors being installed/configured)
* 16 Rc channels values from one or two receivers.

Those data can be transmitted to oXs_logger (if a logger pin is defined in oXs).

Each of those data's is identified by an index. You can define which data's are written on the SD card. To do so, enter commands in the serial terminal when the RP2040 is connected to the USB.

To select some data's, you have to enter ADD=xx/yy where xx is the lower index of a range and yy the upper index.\
To unselect some data's, the command is DEL=xx/yy.\
By default, all data's are selected.

\
Here the index of all oXs data's
* 0: Latitude
* 1: Longitude     note: in csv, Latitude and Longitude are merged in one field nameg GPS; so this index is always discarded 
* 2: Gps groundspeed
* 3: Gps Heading
* 4: Gps Altitude
* 5: Gps Num Sat
* 6: Gps Date
* 7: Gps Time
* 8: Gps PDOP
* 9: Gps home bearing
* 10: Gps home dist
* 11: Volt 1
* 12: Current
* 13: Volt 3
* 14: Volt 4"
* 15: Capacity
* 16: Temp 1 "
* 17: Temp 2"
* 18: VSpeed
* 19: Baro Altitude
* 20: Pitch
* 21: Roll
* 22: Yaw
* 23: RPM
* 24: ADS_1_1  &nbsp; &nbsp;  up to   &nbsp; &nbsp;   27: ADS_1_4
* 28: ADS_2_1    &nbsp; &nbsp;  up to   &nbsp; &nbsp;   31: ADS_2_4
* 32: Airspeed
* 33: Comp VSpeed
* 34: Sbus Hold count
* 35: Sbus Failsafe Count
* 36: GPS cumul dist
* 37: Unused 37    &nbsp; &nbsp;    up to   &nbsp; &nbsp;   40: Unused 40
* 41: Rc1          &nbsp; &nbsp;    &nbsp; &nbsp;    &nbsp; &nbsp;    &nbsp; &nbsp;    up to   &nbsp; &nbsp;   56: RC16
* 57: Unused 57      &nbsp; &nbsp;  up to   &nbsp; &nbsp;   62: Unused 62


oXs generates some data's at very short intervals. This can represent a huge amount of csv records. You can ask the logger to keep a minimum interval between 2 log enties in the csv file. The min interval is specified with the command INTV.

It is also possible to select when oXs are logged or not. The MODE command let you select between 3 options:
* C (continous) : oXs data's are recorded without taking care of the value of the data's
* T (Triggered) : recording starts only after some oXs data's match some MIN and MAX values; once started, recording does not stop
* F (Filtered)  : recording occurs only when some oXs data's match some MIN and MAX values

For option T and F, you have to specify the field index and the value for respectively MIN and MAX.  


## ------------------ Led -------------------
When a RP2040-Zero or RP2040-TINY is used, the firmware will handle a RGB led (internally connected to gpio16).
Led will blink at different rates/colors
* Red, fast blinking : SD card can't be detected, log file can't be created or CSV header can't be written 
* Red, slow blinking : data can't be written
* Blue, slow blinking : the logger do net get data
* Green, slow blinking: data are recorded

Note: some users got a RP2040-zero or RP2040-TINY where red and green colors are inverted.
If you got such a device and want to get the "normal" colors, you can enter a command LED=I to invert the 2 colors.

Please note that other boards do not have a RGB led on gpio16 and so this does not applies.
