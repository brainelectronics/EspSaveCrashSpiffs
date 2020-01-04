/*
  This in an Arduino library to save exception details
  and stack trace to flash in case of ESP8266 crash.
  Please check repository below for details

  Repository: https://github.com/brainelectronics/EspSaveCrashSpiffs
  File: EspSaveCrashSpiffs.h
  Revision: 0.1.0
  Date: 04-Jan-2020
  Author: brainelectronics

  based on code of
  Date: 18-Aug-2016
  Author: krzychb
  Repository: https://github.com/krzychb/EspSaveCrash

  inspired by code of:
  Date: 28-Dec-2018
  Author: popok75
  Repository: https://github.com/esp8266/Arduino/issues/1152

  Copyright (c) 2020 brainelectronics. All rights reserved.

  This application is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This application is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _ESPSAVECRASHSPIFFS_H_
#define _ESPSAVECRASHSPIFFS_H_

#include "Arduino.h"
#include "FS.h"
#include "user_interface.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// the crash log file MUST end with '-1.log' to iterate correctly
#define CRASHFILEPATH       "/"
#define CRASHFILENAME       "crashLog-1.log"
#define CRASHFILEPATTERN    "crashLog"
#define CRASHFILEEXTENSION  "log"

#ifndef LASTCRASHFILEPATH
#define LASTCRASHFILEPATH "/lastName.txt"
#endif

// define the usage of SPIFFS crash log before anything else
// #define SPIFFS_CRASH_LOG  1
// #define EEPROM_CRASH_LOG


/**
 * Structure of the single crash data set
 *
 *  1. Crash time
 *  2. Restart reason
 *  3. Exception cause
 *  4. epc1
 *  5. epc2
 *  6. epc3
 *  7. excvaddr
 *  8. depc
 *  9. adress of stack start
 * 10. adress of stack end
 * 11. stack trace bytes
 *     ...
 */

class EspSaveCrashSpiffs
{
  public:
    EspSaveCrashSpiffs(char *pcAlternativeFilePath=0);

    bool removeFile(uint32_t ulFileNumber);
    bool readFileToBuffer(const char* fileName, char* userBuffer);
    bool print(const char* fileName, Print& outDevice = Serial);
    uint32_t count(char *dirName, char *pattern);
    bool checkFreeSpace(const uint32_t ulFileSize);
    uint32_t getFreeSpace();
    const char *getLogFileName();
    void getLastLogFileName(char* fileContent);
    bool checkFile(const char* theFileName, const char* openMode);
    void setLogFileName(char* fileName);
  private:
    const char* _get_from_string(const char *theString, const char thePattern);
    const char* _get_file_extension(const char *fileName);
    void _extract_file_name(const char *name, char *fileName);
    uint8_t _starts_with(const char *a, const char *b);
    uint8_t _ends_with(const char *a, const char *b);
    void _find_file_name(uint8_t nextOrLatest, char* nextFileName, const char* directoryName, const char* filePattern, const char* fileExtension);
};

void saveToSpiffsLog(char *content);
void saveToSpiffsFile(char *content, const char *fileName);

#endif
