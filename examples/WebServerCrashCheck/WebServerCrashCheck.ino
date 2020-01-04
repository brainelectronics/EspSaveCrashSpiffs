/*
  Example application to show how to save crash information
  to ESP8266's flash using EspSaveCrash library
  Please check repository below for details

  Repository: https://github.com/brainelectronics/EspSaveCrashSpiffs
  File: WebServerCrashCheck.ino
  Revision: 0.1.0
  Date: 04-Jan-2020
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
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// WiFi ssid and password defined in wifiConfig.h
// file is tracked with gitignore
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
  // you have to wait until WiFi.status() == WL_CONNECTED
  // before starting MDNS and the webserver
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  // create AccessPoint with custom network name and password
  WiFi.softAP(accessPointSsid, accessPointPassword);
  Serial.printf("AccessPoint created. Connect to '%s' network and open '%s' in a web browser\n", accessPointSsid, WiFi.softAPIP().toString().c_str());

  server.on("/", handleRoot);
  server.on("/log", handleLog);
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

      if (MDNS.begin("esp8266"))
      {
        MDNS.addService("http", "tcp", 80);

        Serial.println("MDNS responder started");
      }

      Serial.printf("Connected to '%s'. Open '%s' or 'esp8266.local' in a web browser\n", ssid, WiFi.localIP().toString().c_str());
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
    // print error message in case of not enought RAM for this file
    Serial.printf("Error reading file '%s' to buffer. %d byte of RAM is not enough to read %d byte of file content", _lastCrashFileName, _ulFreeHeap, _crashFileSize);
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

void handleRoot()
{
  server.send(200, "text/html", "<html><body>Hello from ESP<br><a href='/log'>Latest crash Log</a></body></html>");
}

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

    server.send(200, "text/plain", _crashFileContent);

    // free the allocated space
    free(_crashFileContent);
  }
  else
  {
    char* _debugOutputBuffer = (char *) calloc(255, sizeof(char));

    // print error message in case of not enought RAM for this file
    sprintf(_debugOutputBuffer, "507: Insufficient Storage. Error reading file '%s' to buffer. %d byte of RAM is not enough to read %d byte of file content", _lastCrashFileName, _ulFreeHeap, _crashFileSize);
    server.send(507, "text/plain", _debugOutputBuffer);

    // free the allocated space
    free(_debugOutputBuffer);
  }

  // free the allocated space
  free(_lastCrashFileName);
}

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
