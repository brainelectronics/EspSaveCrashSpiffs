/*
  Example application to show how to save crash information
  to ESP8266's flash using EspSaveCrash library
  Please check repository below for details

  Repository: https://github.com/brainelectronics/EspSaveCrashSpiffs
  File: WebServerCrashCheck.ino
  Revision: 0.1.1
  Date: 09-Jan-2020
  Author: brainelectronics

  based on code of
  Repository: https://github.com/krzychb/EspSaveCrash
  File: RemoteCrashCheck.ino
  Revision: 1.0.2
  Date: 17-Aug-2016
  Author: krzychb at gazeta.pl

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

// include Arduino libs
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// WiFi ssid and password defined in wifiConfig.h
// file might be tracked with gitignore
#include "wifiConfig.h"

// use the default file name defined in EspSaveCrashSpiffs.h
EspSaveCrashSpiffs SaveCrashSpiffs(0);
ESP8266WebServer server(80);

ESP8266WiFiMulti WiFiMulti;

// the buffer to put the Crash log to
char *_debugOutputBuffer;

// flag for setting up the server with WiFiMulti.addAP
uint8_t performOnce = 0;

void setup(void)
{
  // 2048 should be able to carry most of the debug stuff
  _debugOutputBuffer = (char *) calloc(2048, sizeof(char));

  Serial.begin(115200);
  Serial.println("\nWebServerCrashCheck.ino");

  Serial.printf("Connecting to %s ", ssid);

  // WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  // credentials will be added to onboard WiFi settings
  WiFiMulti.addAP(ssid, password);
  // // you have to wait until WiFi.status() == WL_CONNECTED
  // // before starting MDNS and the webserver
  // WiFi.begin(ssid, password);
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println(" connected");

  // create AccessPoint with custom network name and password
  WiFi.softAP(accessPointSsid, accessPointPassword);
  Serial.printf("AccessPoint created. Connect to '%s' network and open '%s' in a web browser\n", accessPointSsid, WiFi.softAPIP().toString().c_str());

  server.on("/", handleRoot);
  server.on("/log", handleLog);
  server.on("/list", handleListFiles);
  server.on("/file", handleFilePath);
  server.onNotFound(handleNotFound);

  // start the http server
  server.begin();

  Serial.println("\nPress a key + <enter>");
  Serial.println("0 : attempt to divide by zero");
  Serial.println("e : attempt to read through a pointer to no object");
  Serial.println("p : print crash information");
  Serial.println("b : store crash information to buffer and print buffer");
  Serial.println("i : print IP address");
}

void loop(void)
{
  server.handleClient();
  MDNS.update();

  // if a WiFi connection is successfully established
  if (WiFi.status() == WL_CONNECTED)
  {
    // if the following action has not been perfomed yet
    if (performOnce == 0)
    {
      // change the flag
      performOnce = 1;

      Serial.printf("Connected to '%s' as '%s' \n", ssid, WiFi.localIP().toString().c_str());

      // try to start MDNS service
      if (MDNS.begin("esp8266"))
      {
        MDNS.addService("http", "tcp", 80);

        Serial.println("MDNS service started. This device is also available as 'esp8266.local' in a web browser");
      }
      else
      {
        Serial.println("MDNS service failed to start.");
      }
    }
  }

  // read the keyboard
  if (Serial.available() > 0)
  {
    char inChar = Serial.read();

    switch (inChar)
    {
      case '0':
        Serial.println("Attempting to divide by zero ...");
        int result, zero;
        zero = 0;
        result = 1 / zero;
        Serial.print("Result = ");
        Serial.println(result);
        break;
      case 'e':
        Serial.println("Attempting to read through a pointer to no object ...");
        int* nullPointer;
        nullPointer = NULL;
        // null pointer dereference - read
        // attempt to read a value through a null pointer
        Serial.print(*nullPointer);
        break;
      case 'p':
        printLogToSerial();
        break;
      case 'b':
        printLogToBuffer();
        break;
      case 'i':
        // print the IP address of the http server
        if (performOnce == 1)
        {
          Serial.printf("Connected to '%s' with IP address: %s\n", ssid, WiFi.localIP().toString().c_str());
        }
        else
        {
          Serial.printf("Connection to network '%s' not yet established\n", ssid);
        }
        Serial.printf("Connect to AccessPoint '%s' at IP address: %s\n", accessPointSsid, WiFi.softAPIP().toString().c_str());
        break;
      default:
        break;
    }
  }
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
    // in case not enough RAM is available to calloc the whole file size
    char* _errorContent = (char*)calloc(255, sizeof(char));

    // print error message in case of not enought RAM for this file
    sprintf(_errorContent, "Error reading file '%s' to buffer. %d byte of RAM is not enough to read %d byte of file content", _lastCrashFileName, _ulFreeHeap, _crashFileSize);

    // print error message in case of not enought RAM
    Serial.println(_errorContent);

    // free the allocated space
    free(_errorContent);
  }

  // free the allocated space
  free(_lastCrashFileName);
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
 * @brief      Handle access on root page
 */
void handleRoot()
{
  server.send(200, "text/html", "<html><body>Hello from ESP<br><a href='/log' target='_blank'>Latest crash Log</a><br><a href='/list' target='_blank'>List all files in root directory</a><br><a href='/file?path=/crashLog-2.log' target='_blank'>Crash Log file #2</a><br></body></html>");
}

/**
 * @brief      Handle access on log page
 */
void handleLog()
{
  // allocate some space for the filename
  char* _lastCrashFileName;
  _lastCrashFileName = (char*)calloc(255, sizeof(char));

  // get the last filename
  SaveCrashSpiffs.getLastLogFileName(_lastCrashFileName);

  // open the file in reading mode
  File theLogFile = SPIFFS.open(_lastCrashFileName, "r");

  // get the size of the file
  size_t _crashFileSize = theLogFile.size();
  theLogFile.close();

  // get free heap/RAM of the system
  uint32_t _ulFreeHeap = system_get_free_heap_size();

  // check if file size is smaller than available RAM
  if (_ulFreeHeap > (_crashFileSize+1))
  {
    // create buffer for file content with the size of the file+1
    char *_crashFileContent;
    _crashFileContent = (char*)calloc(_crashFileSize+1, sizeof(char));

    // read the file content to the buffer
    SaveCrashSpiffs.readFileToBuffer(_lastCrashFileName, _crashFileContent);

    // send as text/plain to avoid problems with '<<<' signs
    server.send(200, "text/plain", _crashFileContent);

    // free the allocated space
    free(_crashFileContent);
  }
  else
  {
    char* _errorContent = (char *) calloc(255, sizeof(char));

    // print error message in case of not enought RAM for this file
    sprintf(_errorContent, "507: Insufficient Storage. Error reading file '%s' to buffer. %d byte of RAM is not enough to read %d byte of file content", _lastCrashFileName, _ulFreeHeap, _crashFileSize);
    server.send(507, "text/plain", _errorContent);

    // free the allocated space
    free(_errorContent);
  }

  // free the allocated space
  free(_lastCrashFileName);
}

/**
 * @brief      Handle given filepath with URL
 */
void handleFilePath()
{
  // if parameter is empty
  if (server.arg("path") == "")
  {
    server.send(404, "text/plain", "Path argument not found");
  }
  else
  {
    // allocate some space for the filename
    char* _fileName = (char*)calloc(255, sizeof(char));

    // put the value of the path to _fileName
    // e.g. http://192.168.4.1/file?path=/crashLog-5.log
    // will sprinf '/crashLog-5.log' to _fileName
    sprintf(_fileName, "%s", server.arg("path").c_str());

    // open the file in reading mode
    File theFile = SPIFFS.open(_fileName, "r");

    // if this file exists
    if (theFile)
    {
      // get the size of the file and close it
      size_t _fileSize = theFile.size();
      theFile.close();

      // get free heap/RAM of the system
      uint32_t _ulFreeHeap = system_get_free_heap_size();

      // check if file size is smaller than available RAM
      if (_ulFreeHeap > (_fileSize+1))
      {
        // create buffer for file content with the size of the file+1
        char* _fileContent = (char*)calloc(_fileSize+1, sizeof(char));

        // read the file content to the buffer
        SaveCrashSpiffs.readFileToBuffer(_fileName, _fileContent);

        uint32_t _pageContentSize = 255 + _fileSize;
        char* pageContent = (char*)calloc(_pageContentSize, sizeof(char));

        sprintf(pageContent, "Content of file '%s' of size %d byte\n\n%s", _fileName, _fileSize, _fileContent);

        // send as text/plain to avoid problems with '<<<' signs
        server.send(200, "text/plain", pageContent);

        // free the allocated space for page content and file name
        free(_fileContent);
        free(pageContent);
      }
      else
      {
        // in case not enough RAM is available to calloc the whole file size
        char* _errorContent = (char*)calloc(255, sizeof(char));

        // print error message in case of not enought RAM for this file
        sprintf(_errorContent, "Error reading file '%s' to buffer. %d byte of RAM is not enough to read %d byte of file content", _fileName, _ulFreeHeap, _fileSize);

        server.send(507, "text/plain", _errorContent);

        // free the allocated space
        free(_errorContent);
      }
    }
    else
    {
      // if 'file' does not exist or given argument is not a filepath or so on
      char* _errorContent = (char*)calloc(255, sizeof(char));

      // print error message in case of not enought RAM for this file
      sprintf(_errorContent, "404<br>Error reading file '%s'.<br>This file does not exist", _fileName);

      server.send(404, "text/html", _errorContent);

      // free the allocated space
      free(_errorContent);
    }

    // free the allocated space
    free(_fileName);
  }
}

/**
 * @brief      Handle access to filelist webpage
 *
 * Show a list of files with their size on the page
 */
void handleListFiles()
{
  uint8_t i;

  // allocate some space for the filename
  char* theDirectory = (char*)calloc(255, sizeof(char));

  // if parameter is empty
  if (server.arg("path") == "")
  {
    strcpy(theDirectory, "/");
  }
  else
  {
    // put the value of the path to theDirectory
    // e.g. http://192.168.4.1/list?path=/
    // will sprinf '/crashLog-5.log' to theDirectory
    sprintf(theDirectory, "%s", server.arg("path").c_str());

    // try to open that directory in reading mode
    File file = SPIFFS.open(theDirectory, "r");

    // if the specified directory is valid
    if(file.isDirectory())
    {
      // pass
      file.close();
    }
    else
    {
      // use the root directory
      strcpy(theDirectory, "/");
    }
  }

  // count total number of files in specified directory
  uint8_t ubNumberOfFiles = SaveCrashSpiffs.getNumberOfFiles(theDirectory);

  // find number of chars of longest filename to allocate as less as needed
  uint8_t ubLongestFileName = SaveCrashSpiffs.getLongestFileName(theDirectory);

  // create array for file names
  char* pcFileList[ubNumberOfFiles];

  // get free heap/RAM of the system
  uint32_t _ulFreeHeap = system_get_free_heap_size();

  // calculate required heap
  uint32_t _ulRequiredHeap = ((ubLongestFileName * sizeof(char)) + 255) * ubNumberOfFiles + 100;

  // check if file size is smaller than available RAM
  if (_ulFreeHeap > (_ulRequiredHeap+1))
  {
    // allocate space for the file list
    for (i = 0; i < ubNumberOfFiles; i++)
    {
      pcFileList[i] = (char*)calloc(ubLongestFileName, sizeof(char));

      /*
      // this will not fail as we checked free heap a priori
      // try to allocate space
      if ((pcFileList[i] = (char*)calloc(ubLongestFileName, sizeof(char))) == NULL)
      {
        // if it failes
        Serial.printf("Unable to allocate any more memory. Only space for %d/%d \n", i, ubNumberOfFiles);

        // work with most possible number of files
        ubNumberOfFiles = i-1;

        break;
      }
      */
    }

    // pointer to file list/array
    char** ppcFileList = pcFileList;

    // get the file list of theDirectory to buffer ppcFileList
    SaveCrashSpiffs.getFileList(theDirectory, ppcFileList, ubNumberOfFiles);

    // calculate maximum required space per line of list
    uint16_t lineSpace = ubLongestFileName + 255;
    char* fileLine = (char*)calloc(lineSpace, sizeof(char));

    // calculate maximum needed space of whole page
    uint32_t siteSpace = 100 + (lineSpace*ubNumberOfFiles);
    char* filePage = (char*)calloc(siteSpace, sizeof(char));

    // copy the header to the filePage variable
    strcpy(filePage, "<html><body>List of crash logs<br>");

    // Serial.printf("List of files in directory '%s'\n", theDirectory);
    for (i = 0; i < ubNumberOfFiles; i++)
    {
      // open this file in reading mode
      File theFile = SPIFFS.open(ppcFileList[i], "r");

      // sprintf required infos to the fileLine variable
      sprintf(fileLine, "%d byte <a href='/file?path=%s' target='_blank'>%s</a><br>", theFile.size(), ppcFileList[i], ppcFileList[i]);

      // close file finally
      theFile.close();

      // append this line to the page buffer
      strcat(filePage, fileLine);
    }

    // append the footer to the filePage variable
    strcat(filePage, "</body></html>");

    // serve the page
    server.send(200, "text/html", filePage);

    // file list should be free'd at the end
    for (i = 0; i < ubNumberOfFiles; i++)
    {
      free(pcFileList[i]);
    }

    // free the allocated space
    free(fileLine);
    free(filePage);
  }
  else
  {
    // in case not enough RAM is available to calloc the whole file size
    char* _errorContent = (char*)calloc(255, sizeof(char));

    // print error message in case of not enought RAM for this file
    sprintf(_errorContent, "Error reading file list to buffer. %d byte of RAM is not enough to read %d byte of file list content", _ulFreeHeap, _ulRequiredHeap);

    server.send(507, "text/plain", _errorContent);

    // free the allocated space
    free(_errorContent);
  }
}

/**
 * @brief      Provide 404 Not found page
 */
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}
