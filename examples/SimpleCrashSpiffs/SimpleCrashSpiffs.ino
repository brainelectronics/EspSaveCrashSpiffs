/*
  Example application to show how to save crash information
  to ESP8266's flash using EspSaveCrashSpiffs library
  Please check repository below for details

  Repository: https://github.com/brainelectronics/EspSaveCrashSpiffs
  File: SimpleCrashSpiffs.ino
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

extern "C" {
#include "user_interface.h"
}

// include custom lib for this example
#include "EspSaveCrashSpiffs.h"

// include Arduino Filesystem lib
#include <FS.h>

// use the default file name defined in EspSaveCrashSpiffs.h
EspSaveCrashSpiffs SaveCrashSpiffs(0);

// NOT TESTED
// set a custom file name for the error log
// EspSaveCrashSpiffs SaveCrashSpiffs("/newLogFileName.log");
// or use the dedicated function
// SaveCrashSpiffs.setLogFileName("/newLogFileName.log");

char *_serialReadContent;

void setup(void)
{
  // begin serial communication with 115200 baud
  Serial.begin(115200);

  Serial.println();
  Serial.println("SimpleCrashSpiffs.ino");
  Serial.println();

  // Serial.printf("*** IMPORTANT ***\nEnsure you have at least 255 byte of allocatable RAM space for creation of crashlogs\n*** IMPORTANT ***\n");
  // Serial.println();
  // Serial.printf("Default crashlog filename: '%s'\n", SaveCrashSpiffs.getLogFileName());

  // start SPIFFS
  SPIFFS.begin();

  // allocate memory for serial reading content
  _serialReadContent = (char*)calloc(100, sizeof(char));

  // print help menu
  printHelp();
  Serial.println();

  // print informations about the '/' directory
  printDirectoryInfo();
  Serial.println();
}

void loop(void)
{
  // if some content has been received via serial
  if (Serial.available() > 0)
  {
    // read input to string and copy the string as c_str without
    // the last element (the '\n')
    String _readString = Serial.readString();

    // clear the memory before copy of new stuff
    // memset(_serialReadContent, 0, sizeof(char));
    // memcpy(_serialReadContent, _readString.c_str(), strlen(_readString.c_str())-1);
    sprintf(_serialReadContent, "%s", _readString.c_str());

    // Serial.printf("Received: '%s' of length %d\n", _serialReadContent, strlen(_serialReadContent));

    // if something other than '' has been received
    if (strlen(_serialReadContent) > 0)
    {
      // convert the content to an integer
      uint32_t fileIndexInt = atoi(_serialReadContent);

      // if it was convertable to integer and is not zero
      if (fileIndexInt > 0)
      {
        Serial.printf("Printing content of file %d\n", fileIndexInt);

        Serial.printf("--- BEGIN of file #%d ---\n", fileIndexInt);
        printFileOfIndex(fileIndexInt);
        Serial.printf("--- END of file #%d ---\n", fileIndexInt);
      }
      else
      {
        // received something which is not a number/integer
        // switch based on the first char of _serialReadContent
        switch (*_serialReadContent)
        {
          // switch by ascii
          case 48:  // '0'
            // force error by dividing by zero
            Serial.println("Attempting to divide by zero ...");

            int result, zero;
            zero = 0;
            result = 1 / zero;
            Serial.print("Result = ");
            Serial.println(result);
            break;
          case 101: // 'e'
            // force error by reading a null pointer value
            Serial.println("Attempting to read through a pointer to no object ...");

            int* nullPointer;
            nullPointer = NULL;
            // null pointer dereference - read
            // attempt to read a value through a null pointer
            Serial.print(*nullPointer);
            break;
          case 98:  // 'b'
            // put content of last crash log file to buffer and print buffer
            printLogToBuffer();
            break;
          case 99:  // 'c'
            // count number of files with some pattern
            countNumberOfFiles();
            break;
          case 100: // 'd'
            // delete some file
            deleteSomeFile();
            break;
          case 102: // 'f'
            // print the directory informations
            printDirectoryInfo();
            break;
          case 104: // 'h'
            // print the help menu
            printHelp();
            break;
          case 105: // 'i'
            // print filesystem space info
            printFilesystemInfo();
            break;
          case 108: // 'l'
            // print the last crash log filename
            printLastCrashLogName();
            break;
          case 112: // 'p'
            // print the last crash log directly to serial
            printLogToSerial();
            break;
          default:
            break;
        }
      }
    }
    // free allocated space
    // free(_serialReadContent);

    // print empty line for better overview
    Serial.println();
    // Serial.println();
  }
}

/**
 * @brief      Prints a help menu.
 */
void printHelp()
{
  Serial.printf("Saving crash informations to '%s'\n", SaveCrashSpiffs.getLogFileName());

  Serial.println("Press a key + <enter>");
  Serial.println("0 : attempt to divide by zero");
  Serial.println("e : attempt to read through a pointer to no object");

  Serial.println("b : store latest crash information to buffer and print buffer");
  Serial.println("c : print number of crash files");
  Serial.printf("d : remove default crash info file '%s'\n", SaveCrashSpiffs.getLogFileName());
  Serial.printf("d123 : remove file number '123' of the directory '/'\n");
  Serial.println("f : print all filenames and their size");
  Serial.println("h : print this help menu");
  Serial.println("i : print info of filesystem");
  Serial.println("l : print filename of latest crash log");
  Serial.println("p : print latest crash information");

  Serial.println("Enter a number + <enter> to print the content of that file. Use 'f' to get a list of all files in the root directory");
  // Serial.println();
}

/**
 * @brief      Prints a directory information.
 */
void printDirectoryInfo()
{
  uint32_t ulThisFileNumber = 0;

  Dir dir = SPIFFS.openDir("/");

  Serial.println("Directory Info\nFile size\tFile name");
  while (dir.next())
  {
    ulThisFileNumber++;

    Serial.printf("%d: %d byte\t%s\n", ulThisFileNumber, dir.fileSize(), dir.fileName().c_str());
  }

  Serial.printf("Found %d files in this directory\n", ulThisFileNumber);
}

/**
 * @brief      Prints the file number x of the directory.
 *
 * @param[in]  givenFileNumber  The file number x
 */
void printFileOfIndex(uint8_t givenFileNumber)
{
  uint32_t ulThisFileNumber = 0;

  Dir dir = SPIFFS.openDir("/");

  while (dir.next())
  {
    ulThisFileNumber++;

    if (ulThisFileNumber == givenFileNumber)
    {
      SaveCrashSpiffs.print(dir.fileName().c_str());
      break;
    }
  }
}

/**
 * @brief      Print the latest log file to serial.
 */
void printLogToSerial()
{
  // allocate some space for the filename
  char* _lastCrashFileName;
  _lastCrashFileName = (char*)calloc(255, sizeof(char));

  // get the last filename
  SaveCrashSpiffs.getLastLogFileName(_lastCrashFileName);

  Serial.printf("Name of last log file: '%s'\n", _lastCrashFileName);

  Serial.println("--- BEGIN of crash file ---");
  SaveCrashSpiffs.print(_lastCrashFileName);
  Serial.println("--- END of crash file ---");

  // free the allocated space
  free(_lastCrashFileName);
}

/**
 * @brief      Prints the last crash log name to serial.
 */
void printLastCrashLogName()
{
  // allocate some space for the filename
  char* _lastCrashFileName;
  _lastCrashFileName = (char*)calloc(255, sizeof(char));

  // get the last filename
  SaveCrashSpiffs.getLastLogFileName(_lastCrashFileName);

  Serial.printf("Name of last log file: '%s'\n", _lastCrashFileName);

  // free the allocated space
  free(_lastCrashFileName);
}

/**
 * @brief      Prints information about the filesystem.
 */
void printFilesystemInfo()
{
  Serial.printf("Remaining space on SPIFFS: %d byte\n", SaveCrashSpiffs.getFreeSpace());
  Serial.printf("Will 1024 byte fit to SPIFFS? %s\n", SaveCrashSpiffs.checkFreeSpace(1024) ? "True" : "False");
  Serial.printf("Will 4MB (4194394 byte) fit to SPIFFS? %s\n", SaveCrashSpiffs.checkFreeSpace(4194394) ? "True" : "False");
}

/**
 * @brief      Counts the number of files and prints to serial.
 */
void countNumberOfFiles()
{
  // find numbers of '.log' files in directory '/'
  uint32_t ulNumberOfFiles = SaveCrashSpiffs.count((char*)"/", (char*)".log");

  Serial.printf("Found %d files in directory '/' ending with '.log'\n", ulNumberOfFiles);
}

/**
 * @brief      Put the latest log file content to buffer and print it.
 *
 * This will only print the log file if there is enought free RAM to allocate.
 */
void printLogToBuffer()
{
  // allocate some space for the filename
  char* _lastCrashFileName;
  _lastCrashFileName = (char*)calloc(255, sizeof(char));

  // get the last filename
  SaveCrashSpiffs.getLastLogFileName(_lastCrashFileName);

  Serial.printf("Name of last log file: '%s'\n", _lastCrashFileName);

  // open the file in reading mode
  File theLogFile = SPIFFS.open(_lastCrashFileName, "r");

  // get the size of the file
  size_t _crashFileSize = theLogFile.size();
  theLogFile.close();

  // get free heap/RAM of the system
  uint32_t _ulFreeHeap = system_get_free_heap_size();

  // check if file size is smaller than available RAM
  if (_ulFreeHeap > _crashFileSize)
  {
    // create buffer for file content with the size of the file+1
    char *_crashFileContent;
    _crashFileContent = (char*)calloc(_crashFileSize+1, sizeof(char));

    // read the file content to the buffer
    SaveCrashSpiffs.readFileToBuffer(_lastCrashFileName, _crashFileContent);

    Serial.println("--- BEGIN of crash file ---");
    Serial.print(_crashFileContent);
    Serial.println("--- END of crash file ---");

    // free the allocated space
    free(_crashFileContent);
  }
  else
  {
    // print error message in case of not enought RAM for this file
    Serial.printf("Error reading file '%s' to buffer. %d byte of RAM is not enough to read %d byte of file content", _lastCrashFileName, _ulFreeHeap, _crashFileSize);
  }

  // free the allocated space
  free(_lastCrashFileName);
}

/**
 * @brief      Delete some file on the filesystem.
 *
 * If no filenumber is specified the default crash log file will be removed
 */
void deleteSomeFile()
{
  // default result is false
  bool _bRemoveResult = false;

  // if the next character(s) is/are number
  // e.g. if 'd' is followed by a number, e.g. 'd12'
  if (atoi((_serialReadContent+1)))
  {
    // convert '12' to 12
    uint32_t ubFileNumberToRemove = atoi((_serialReadContent+1));

    Serial.printf("Remove file #%d\n", ubFileNumberToRemove);

    // remove the file e.g. #12
    _bRemoveResult = SaveCrashSpiffs.removeFile(ubFileNumberToRemove);
  }
  else
  {
    // remove the default log file
    _bRemoveResult = SaveCrashSpiffs.removeFile(0);
  }

  // check result of action
  if (_bRemoveResult)
  {
    Serial.printf("Removed file successfully\n");
  }
  else
  {
    Serial.printf("Failed to remove file\n");
  }
}

