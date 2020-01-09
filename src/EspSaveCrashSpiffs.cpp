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

#include "EspSaveCrashSpiffs.h"

char* pcCrashFilePath;

/**
 * @brief      Saves to log to the SPIFFS.
 *
 * @param      content  The content
 */
void saveToSpiffsLog(char *content)
{
  // if there is a filename given to store the logs to
  if(pcCrashFilePath)
  {
    saveToSpiffsFile(content, pcCrashFilePath);
  }
}

/**
 * @brief      Saves to log to the SPIFFS.
 *
 * @param      content   The content
 * @param[in]  fileName  The file name
 */
void saveToSpiffsFile(char *content, const char *fileName)
{
  // struct FSInfo {
  //   size_t totalBytes;
  //   size_t usedBytes;
  //   size_t blockSize;
  //   size_t pageSize;
  //   size_t maxOpenFiles;
  //   size_t maxPathLength;
  // };
  FSInfo fs_info;

  // fill FSInfo struct with informations about the SPIFFS
  SPIFFS.info(fs_info);

  // if the remaining space is less than the length of the content to save
  if ((fs_info.totalBytes - (fs_info.usedBytes * 1.05)) < strlen(content))
  {
    // exit this function
    return;
  }

  // open the file in appending mode
  File fileCrashFile = SPIFFS.open(fileName, "a");

  // if the file does not yet exist
  if(!fileCrashFile)
  {
    // open the file in write mode
    fileCrashFile = SPIFFS.open(fileName, "w");
  }

  // if the file is a valid file
  if(fileCrashFile)
  {
    // size_t File::write(const uint8_t *buf, size_t size);
    fileCrashFile.write(content, strlen(content));

    fileCrashFile.close();
  }
}

/**
 * This function is called automatically if ESP8266 suffers an exception
 * It should be kept quick / consise to be able to execute before hardware wdt may kick in
 *
 * Without writing to SPIFFS this function take 2-3ms
 * Writing to flash only takes 10-11ms.
 * This complete function should be finised in 15-20ms
 */
extern "C" void custom_crash_callback(struct rst_info * rst_info, uint32_t stack, uint32_t stack_end)
{
  uint32_t crashTime = millis();

  // create loop iterators
  int16_t i;
  uint8_t j;

  char* _thisCrashFilePath = (char*)calloc(100, sizeof(char));
  if (pcCrashFilePath)
  {
    // use the new created/set name
    _thisCrashFilePath = pcCrashFilePath;
    Serial.printf("No NULL pointer, using '%s'", _thisCrashFilePath);
  }
  else
  {
    // use the default crash file naming
    sprintf(_thisCrashFilePath, "%s%s", CRASHFILEPATH, CRASHFILENAME);
    Serial.printf("NULL pointer, created default filepath: '%s'", _thisCrashFilePath);
  }

  // flag to break the loop in case the buffer is full
  // uint8_t breakFlag = 0;

  // open the file in appending mode
  File fileCrashFile = SPIFFS.open(_thisCrashFilePath, "a");

  // if the file does not yet exist
  if(!fileCrashFile)
  {
    // open the file in write mode
    fileCrashFile = SPIFFS.open(_thisCrashFilePath, "w");
  }
  free(_thisCrashFilePath);

  // if the file is (now) a valid file
  if(fileCrashFile)
  {
    // // one complete log has 170 chars + 45 * n
    // // n >= 2 e [2, 4, 6, ...]
    // // 4220 + safety will last for 90 stack traces including header (170)
    // // uint16_t maximumFileContent = 4250;

    // maximum tmpBuffer size needed is 83, so 100 should be enough
    char *tmpBuffer = (char*)calloc(100, sizeof(char));
    // char *fileContent = (char*)calloc(maximumFileContent, sizeof(char));

    // max. 65 chars of Crash time, reason, exception
    sprintf(tmpBuffer, "Crashed at %d ms\nRestart reason: %d\nException cause: %d\n", crashTime, rst_info->reason, rst_info->exccause);
    // strcat(fileContent, tmpBuffer);
    fileCrashFile.write(tmpBuffer, strlen(tmpBuffer));

    // 83 chars of epc1, epc2, epc3, excvaddr, depc info + 13 chars of >stack>
    sprintf(tmpBuffer, "epc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x\n>>>stack>>>\n", rst_info->epc1, rst_info->epc2, rst_info->epc3, rst_info->excvaddr, rst_info->depc);
    // strcat(fileContent, tmpBuffer);
    fileCrashFile.write(tmpBuffer, strlen(tmpBuffer));

    int16_t stackLength = stack_end - stack;
    uint32_t stackTrace;

    // strcat(fileContent, ">>>stack>>>\n");
    // fileCrashFile.write(">>>stack>>>\n", strlen(">>>stack>>>\n"));

    // collect stack trace
    // one loop contains 45 chars of stack address and its content
    // e.g. "3fffffb0: feefeffe feefeffe 3ffe8508 40100459"
    for (i = 0; i < stackLength; i += 0x10)
    {
      // // if the breakFlag is set
      // // OR this loop content (45 char) and <<<stack<<<\n\n (15 char)
      // // won't fit anymore into the fileContent buffer
      // if ((breakFlag == 1) || ((strlen(fileContent) + 45 + 15) >= maximumFileContent))
      // {
      //   break;
      // }

      sprintf(tmpBuffer, "%08x: ", stack + i);
      // strcat(fileContent, tmpBuffer);
      fileCrashFile.write(tmpBuffer, strlen(tmpBuffer));

      for (j = 0; j < 4; j++)
      {
        uint32_t* byteptr = (uint32_t*) (stack + i+j*4);
        stackTrace = *byteptr;

        sprintf(tmpBuffer, "%08x ", stackTrace);
        fileCrashFile.write(tmpBuffer, strlen(tmpBuffer));

        // // check if this tmpBuffer iteration and the <<<stack<<< (15 char aprox)
        // // will fit finally into the fileContent buffer
        // if ((15 + strlen(fileContent) + strlen(tmpBuffer)) < maximumFileContent)
        // {
        //   strcat(fileContent, tmpBuffer);
        // }
        // else
        // {
        //   // set the breakFlag
        //   breakFlag = 1;

        //   // break from this loop
        //   break;
        // }
      }
      // strcat(fileContent, "\n");
      fileCrashFile.write("\n", strlen("\n"));
    }
    // as we checked +15 char, this will always fit
    // strcat(fileContent, "<<<stack<<<\n\n");
    fileCrashFile.write("<<<stack<<<\n\n", strlen("<<<stack<<<\n\n"));

    // free the tmpBuffer before saving to SPIFFS gives another 100 byte of RAM
    free(tmpBuffer);

    // save to pcCrashFilePath
    // saveToSpiffsLog(fileContent);
    //
    // save to custom file
    // saveToSpiffsFile(fileContent, aCustomFileName);

    // free the fileContent, this will happen anyway due to reset of the device
    // free(fileContent);

    fileCrashFile.close();
  }
}

/**
 * @brief      Constructs a new instance.
 *
 * check whether the crash file is not empty.
 * if the file has content
 *  - check weather enough space is available
 *  - find the next filename (based on the crash file name and extension)
 *  - rename the file to the next free filename
 * if the file is empty, continue without any action
 *
 * @param      alternativeFilePath  The alternative crash log file path
 */
EspSaveCrashSpiffs::EspSaveCrashSpiffs(char *alternativeFilePath)
{
  // just for debug
  Serial.begin(115200);

  // update the statifn only if a new filename is given
  if (alternativeFilePath)
  {
    pcCrashFilePath = alternativeFilePath;
    // Serial.printf("Alternative filepath given: '%s'\n", pcCrashFilePath);
  }
  else
  {
    // set the default crash file
    pcCrashFilePath = (char*)calloc(100, sizeof(char));
    sprintf(pcCrashFilePath, "%s%s", CRASHFILEPATH, CRASHFILENAME);
    // Serial.printf("Created filepath based on .h file definitions: '%s'\n", pcCrashFilePath);
  }

  SPIFFS.begin();

  // open the file in reading mode
  File fileCrashFile = SPIFFS.open(pcCrashFilePath, "r");

  // if the file is a valid file (it exists)
  if(fileCrashFile)
  {
    // allocate some space for the filename and filepath
    char *nextFileName = (char*)calloc(255, sizeof(char));
    char *nextFilePath = (char*)calloc(255, sizeof(char));

    // find the new/next filename
    _find_file_name(1, nextFileName, CRASHFILEPATH, CRASHFILEPATTERN, CRASHFILEEXTENSION);
    // Serial.printf("Found next filename: '%s'\n", nextFileName);

    // rename only if the next filename is valid
    if (strlen(nextFileName))
    {
      // create the filepath (must always start with '/')
      sprintf(nextFilePath, "%s%s", CRASHFILEPATH, nextFileName);

      // rename the old file to the new generated filename
      Serial.printf("Renaming file '%s' to '%s'\n", getLogFileName(), nextFilePath);
      // SPIFFS.rename(pathFrom, pathTo)
      SPIFFS.rename(pcCrashFilePath, nextFilePath);

      // write filename to file
      File lastFileNameFile = SPIFFS.open(LASTCRASHFILEPATH, "w+");
      // size_t File::write(const uint8_t *buf, size_t size);
      lastFileNameFile.write(nextFilePath, strlen(nextFilePath));
      lastFileNameFile.close();
    }

    // free the allocated space
    free(nextFileName);
    free(nextFilePath);
  }
}

/**
 * @brief      Gets the string after the found pattern.
 *
 * If nothing is found, it will return Null.
 * Given 'asdf-12.xyz' as theString and '-' as thePattern, this function
 * will return '-12'
 *
 * @param[in]  theString   The string
 * @param[in]  thePattern  The pattern
 *
 * @return     The position in the string.
 */
const char* EspSaveCrashSpiffs::_get_from_string(const char *theString, const char thePattern)
{
  char *pos = strrchr(theString, thePattern);

  return pos;
}

/**
 * @brief      Gets the file extension.
 *
 * Given 'asdf-12.xyz' as filename, this function will return 'xyz'
 *
 * @param[in]  fileName  The filename
 *
 * @return     The file extension.
 */
const char* EspSaveCrashSpiffs::_get_file_extension(const char *fileName)
{
  // add +1 to not use or return the '.' itself
  return _get_from_string(fileName, '.')+1;
}

/**
 * @brief      Extract the filename from the file string
 *
 * Given 'asdf-12.xyz' as filename, this function will return 'asdf-12'
 *
 * @param[in]  name      The name
 * @param      fileName  The filename
 */
void EspSaveCrashSpiffs::_extract_file_name(const char *name, char *fileName)
{
  memcpy(fileName, name, _get_from_string(name, '.')-name);
}

/**
 * @brief      Check if some string starts with some other
 *
 * @param[in]  a     The string to check
 * @param[in]  b     The start string
 *
 * @retval     True   Success
 * @retval     False  Failed
 */
uint8_t EspSaveCrashSpiffs::_starts_with(const char *a, const char *b)
{
  if(strncmp(a, b, strlen(b)) == 0) return 1;
  return 0;
}

/**
 * @brief      Check if some string ends with some other
 *
 * @param[in]  a     The string to check
 * @param[in]  b     The end string
 *
 * @retval     True   Success
 * @retval     False  Failed
 */
uint8_t EspSaveCrashSpiffs::_ends_with(const char *a, const char *b)
{
  if(strncmp(a+strlen(a)-strlen(b), b, strlen(b)) == 0) return 1;
  return 0;
}

/**
 * @brief      Find the next or most recent file name.
 *
 * Crawl the directory for files matching the pattern and the extension.
 *
 * @param[in]  nextOrLatest    Copy next (1) most recent (0) to nextFileName
 * @param      nextFileName    The next file name
 * @param[in]  filePattern     The file pattern
 * @param[in]  fileExtension   The file extension
 *
 * Example usage to find the most recent file:
 * @code
 *    // allocate some space for the file name
 *    char* fileName = (char*)calloc(100, sizeof(char));
 *
 *    // find the most recent file starting with 'crashLog'
 *    // and ending with 'log' in the root directory '/'
 *    // fileName will be e.g. 'crashLog-123.log'
 *    // if the most recent one was called 'crashLog-123.log'
 *    _find_file_name(0, fileName, "/", "crashLog", "log");
 * @endcode
 *
 * Example usage to find the next file:
 * @code
 *    // allocate some space for the file name
 *    char* fileName = (char*)calloc(100, sizeof(char));
 *
 *    // find the next filename based on starting with 'crashLog'
 *    // and ending with 'log' in the root directory '/'
 *    // fileName will be e.g. 'crashLog-124.log'
 *    // if the most recent one was called 'crashLog-123.log'
 *    _find_file_name(1, fileName, "/", "crashLog", "log");
 * @endcode
 */
void EspSaveCrashSpiffs::_find_file_name(uint8_t nextOrLatest, char* nextFileName, const char* directoryName, const char* filePattern, const char* fileExtension)
{
  uint32_t ulNextCrashLogFileIndex = 0;

  // allocate some space for the filepath
  char *thisRawFilePath = (char*)calloc(255, sizeof(char));

  // if no directory name is given
  if (!directoryName)
  {
    // take the default root directory
    directoryName = (char*)"/";
    // Serial.println("No directory given, using default root");
  }
  Dir thisDirectory = SPIFFS.openDir(directoryName);
  // or Dir thisDirectory = LittleFS.openDir("/data");

  // Serial.printf("Crawling directory: '%s'\n", directoryName);

  // iterate through all files in this directory
  while (thisDirectory.next())
  {
    // collect filename of this loop iteration
    sprintf(thisRawFilePath, "%s", thisDirectory.fileName().c_str());

    // get only the filename without any directory
    // '/path/to/logs/crashLog-1.log' becomes 'crashLog-1.log'
    const char* thisFile = _get_from_string(thisRawFilePath, '/');

    // if the file has some path before it's actual filename
    if (thisFile)
    {
      // take everything after the last occurance of a backslash
      thisFile = thisFile + 1;
    }

    // Serial.printf("The filename only: '%s'\n", thisFile);

    // allocate some space for the filename
    char *thisFileName = (char*)calloc(255, sizeof(char));


    // gets 'asdf-12' out of 'asdf-12.xyz'
    _extract_file_name(thisFile, thisFileName);
    // Serial.printf("\t File name: '%s'\n", thisFileName);


    // gets 'xyz' out of 'asdf-12.xyz'
    char *thisFileExtension = (char*) _get_file_extension(thisFile);
    // Serial.printf("\t Extension: '%s'\n", thisFileExtension);


    // gets the string after the given delimiter. E.g. '-12'
    const char *delimiterPosition = _get_from_string(thisFileName, '-');

    // if a valid content has been found
    if (delimiterPosition)
    {
      // Serial.printf("\t File name '%s' contains delimiter '-'\n", thisFileName);


      // gets '12' out of 'asdf-12'
      char *thisFileIndex = (char*)delimiterPosition+1;
      // Serial.printf("\t Splitted file index (str): '%s'\n", thisFileIndex);


      // convert the thisFileIndex to an integer. E.g. make 12 out of '12'
      uint32_t ulThisFileIndex = atoi(thisFileIndex);

      // if atoi was successful (larger zero)
      if (ulThisFileIndex > 0)
      {
        // get 12 (based on the example)
        // Serial.printf("\t Final evaluated file index (int): %d\n", ulThisFileIndex);


        // same as get_file_name but with delimiter '-' instead of '.'
        // gets 'asdf' out of 'asdf-12'
        // allocate some space for the filename without it's index
        char *thisFileNameWithoutIndex = (char *)calloc(strlen(thisFileName)+1, sizeof(char));
        memcpy(thisFileNameWithoutIndex, thisFileName, delimiterPosition-thisFileName);
        // Serial.printf("\t File name without index and delimiter: '%s'\n", thisFileNameWithoutIndex);


        // if the file pattern and the extension are matching (result is 0)
        if ((strcmp(thisFileNameWithoutIndex, filePattern) == 0) && (strcmp(thisFileExtension, fileExtension) == 0))
        {
          // Serial.printf("\t Matches pattern '%s' and extension '%s'\n", filePattern, fileExtension);

          // if the current file index is larger than the previous found
          if ((ulThisFileIndex + 1) > ulNextCrashLogFileIndex)
          {
            ulNextCrashLogFileIndex = ulThisFileIndex + 1;

            // check for null pointer
            if (nextFileName)
            {
              if (nextOrLatest == 0)
              {
                // create new file name with non incremented aka. highest index
                // or just simply keep this filename
                // as it is the most recent filename
                sprintf(nextFileName, "%s", thisFile);

                // Serial.printf("\t Most recent '%s'\n", nextFileName);
              }
              else
              {
                // create new file name with incremented index
                sprintf(nextFileName, "%s-%d.%s", thisFileNameWithoutIndex, ulNextCrashLogFileIndex, thisFileExtension);
                // Serial.printf("\t Next will be '%s'\n", nextFileName);
              }
            }
          }
        }
        // else
        // {
        //   Serial.printf("\t pattern '%s' NOT MATCHING '%s' AND extension '%s' NOT MATCHING '%s'\n", filePattern, thisFileNameWithoutIndex, fileExtension, thisFileExtension);
        // }

        // free the allocated space
        free(thisFileNameWithoutIndex);
      }
    }
    // free the allocated space
    free(thisFileName);
  }

  // free the allocated space
  free(thisRawFilePath);
}

/**
 * @brief      Removes a file.
 *
 * If the given number is zero, the current log file will be removed.
 * Otherwise the file at that index.
 *
 * @param[in]  fileNumber  The file number
 *
 * @retval     True   Success
 * @retval     False  Failed
 */
bool EspSaveCrashSpiffs::removeFile(uint32_t ulFileNumber)
{
  // if there is a filenumber given, remove this
  if (ulFileNumber > 0)
  {
    uint32_t ulThisFileNumber = 0;
    Dir thisDirectory = SPIFFS.openDir("/");

    while (thisDirectory.next())
    {
      ulThisFileNumber++;

      // if the given and the current file index are matching
      if (ulThisFileNumber == ulFileNumber)
      {
        // Serial.printf("Reached file #%d named '%s'\n", ulFileNumber, thisDirectory.fileName().c_str());

        // allocate some space for latestFileName
        char *latestFileName = (char*)calloc(255, sizeof(char));
        char *latestFilePath = (char*)calloc(255, sizeof(char));

        // re-create the filepath (must always start with '/')
        sprintf(latestFilePath, "%s%s", CRASHFILEPATH, CRASHFILEPATTERN);

        // if this file is a crash log file
        if (_starts_with(thisDirectory.fileName().c_str(), latestFilePath) && _ends_with(thisDirectory.fileName().c_str(), CRASHFILEEXTENSION))
        {
          // Serial.printf("File starts with '%s' and ends with '%s'\n", latestFilePath, CRASHFILEEXTENSION);

          // find the most recent filename, do not rely on LASTCRASHFILEPATH
          _find_file_name(0, latestFileName, CRASHFILEPATH, CRASHFILEPATTERN, CRASHFILEEXTENSION);

          // re-create the filepath (must always start with '/')
          sprintf(latestFilePath, "%s%s", CRASHFILEPATH, latestFileName);

          // if this filename matches the latest crash log file (result is 0)
          if (strcmp(thisDirectory.fileName().c_str(), latestFilePath) == 0)
          {
            // Serial.printf("The file '%s' is the most recent crash log file\n", latestFilePath);

            // remove this current file, it exists for sure as we iterate
            SPIFFS.remove(thisDirectory.fileName().c_str());

            // reset the content
            memset(latestFileName, 0, sizeof(char));

            // find the NOW most recent filename
            _find_file_name(0, latestFileName, CRASHFILEPATH, CRASHFILEPATTERN, CRASHFILEEXTENSION);

            // re-create the filepath (must always start with '/')
            sprintf(latestFilePath, "%s%s", CRASHFILEPATH, latestFileName);

            // Serial.printf("After deleting '%s' the most recent crashlog file is now '%s'\n", thisDirectory.fileName().c_str(), latestFilePath);

            // overwrite with the now most recent log file name
            File lastFileNameFile = SPIFFS.open(LASTCRASHFILEPATH, "w+");
            // size_t File::write(const uint8_t *buf, size_t size);
            lastFileNameFile.write(latestFilePath, strlen(latestFilePath));
            lastFileNameFile.close();
          }
          else
          {
            // remove this current file, it exists for sure as we iterate
            SPIFFS.remove(thisDirectory.fileName().c_str());
          }
        }
        else
        {
          // Serial.printf("Some other file, no '%s'*.'%s'\n", CRASHFILEPATTERN, CRASHFILEEXTENSION);

          // remove this current file, it exists for sure as we iterate
          SPIFFS.remove(thisDirectory.fileName().c_str());
        }

        // free the allocated space
        free(latestFileName);
        free(latestFilePath);

        return true;
      }
    }
  }
  else
  {
    // if the given filenumber is zero, remove the current logfile
    if (SPIFFS.exists(pcCrashFilePath))
    {
      // remove this file
      SPIFFS.remove(pcCrashFilePath);

      // allocate some space for latestFileName
      char *latestFileName = (char*)calloc(255, sizeof(char));
      char *latestFilePath = (char*)calloc(255, sizeof(char));

      // find the now most recent filename
      _find_file_name(0, latestFileName, CRASHFILEPATH, CRASHFILEPATTERN, CRASHFILEEXTENSION);

      // re-create the filepath (must always start with '/')
      sprintf(latestFilePath, "%s%s", CRASHFILEPATH, latestFileName);

      // overwrite with the now most recent log file name
      File lastFileNameFile = SPIFFS.open(LASTCRASHFILEPATH, "w+");
      // size_t File::write(const uint8_t *buf, size_t size);
      lastFileNameFile.write(latestFilePath, strlen(latestFilePath));
      lastFileNameFile.close();

      // free the allocated space
      free(latestFileName);
      free(latestFilePath);

      return true;
    }
  }

  return false;
}

/**
 * @brief      Check file operations.
 *
 * This function checks if:
 *  - the file system can be accessed
 *  - the file exists
 *  - the file can be written or read
 * @param[in]  theFileName  The file name
 * @param[in]  openMode     The open mode "r", "w", "a"...
 *
 * @retval     True   Success
 * @retval     False  Failed
 */
bool EspSaveCrashSpiffs::checkFile(const char* theFileName, const char* openMode)
{
  // if starting the SPIFFS failed
  if (!SPIFFS.begin())
  {
    return false;
  }

  // if the file does not exist
  if (!SPIFFS.exists(theFileName))
  {
    return false;
  }

  // if reading or writing operations should also be checked
  if (openMode)
  {
    File theFile = SPIFFS.open(theFileName, openMode);

    // if opening the file failed
    if (!theFile)
    {
      return false;
    }

    theFile.close();
  }

  return true;
}

/**
 * @brief      Reads a file to buffer.
 *
 * @param[in]  fileName    The file name
 * @param      size        The size
 * @param      userBuffer  The user buffer to store the content of the file
 *
 * @return     Sucess or error accessing the file
 */
bool EspSaveCrashSpiffs::readFileToBuffer(const char* fileName, char* userBuffer)
{
  // check if SPIFFS is working and file exists.
  // NULL parameter as checking reading or writing is not neccessary here
  if (!checkFile(fileName, "r"))
  {
    // exit on file error
    return false;
  }

  File theFile = SPIFFS.open(fileName, "r");

  theFile.readBytes(userBuffer, theFile.size());

  theFile.close();

  return true;
}

/**
 * @brief      Print the file content to outputDev
 *
 * @param[in]  fileName   The file name
 * @param      outputDev  The output dev
 *
 * @return     Sucess or error accessing the file
 */
bool EspSaveCrashSpiffs::print(const char* fileName, Print& outputDev)
{
  // check if SPIFFS is working and file exists.
  // NULL parameter as checking reading or writing is not neccessary here
  if (!checkFile(fileName, "r"))
  {
    // exit on file error
    return false;
  }

  File theFile = SPIFFS.open(fileName, "r");

  while (theFile.available())
  {
    outputDev.write(theFile.read());
  }

  theFile.close();

  return true;
}

/**
 * @brief      Count files matching the pattern
 *
 * @param      dirName  The directory name
 * @param      pattern  The pattern to search for
 *
 * @return     Number of files matching the pattern
 */
uint32_t EspSaveCrashSpiffs::count(char* dirName, char* pattern)
{
  // if no directory name is given
  if (!dirName)
  {
    // take the default root directory
    dirName = (char*)"/";
  }

  // if no pattern is given
  if (!pattern)
  {
    return 0;
  }

  // search for files in the log directory
  Dir thisDirectory = SPIFFS.openDir(dirName);

  uint32_t ulCrashCounter = 0;
  while (thisDirectory.next())
  {
    // if (thisDirectory.fileName().startsWith(CRASHFILEPATTERN))
    // if (thisDirectory.fileName().endsWith(CRASHFILEEXTENSION))
    if (thisDirectory.fileName().endsWith(pattern))
    {
      ulCrashCounter++;
    }
  }

  return ulCrashCounter;
}

/**
 * @brief      Gets the total number of files in this directory.
 *
 * @param      dirName  The directory name
 *
 * @return     The number of files.
 */
uint32_t EspSaveCrashSpiffs::getNumberOfFiles(char* dirName)
{
  // if no directory name is given
  if (!dirName)
  {
    // take the default root directory
    dirName = (char*)"/";
  }

  // search for files in the log directory
  Dir thisDirectory = SPIFFS.openDir(dirName);

  uint32_t ulFileCounter = 0;

  while (thisDirectory.next())
  {
    ulFileCounter++;
  }

  return ulFileCounter;
}

/**
 * @brief      Gets the longest file name.
 *
 * @param      dirName  The directory name
 *
 * @return     The longest file name as integer.
 */
uint32_t EspSaveCrashSpiffs::getLongestFileName(char* dirName)
{
  // if no directory name is given
  if (!dirName)
  {
    // take the default root directory
    dirName = (char*)"/";
  }

  // search for files in the log directory
  Dir thisDirectory = SPIFFS.openDir(dirName);

  uint8_t ubLongestFilename = 0;

  while (thisDirectory.next())
  {
    uint8_t _thisFilenameLength = strlen(thisDirectory.fileName().c_str());

    if (_thisFilenameLength > ubLongestFilename)
    {
      ubLongestFilename = _thisFilenameLength;
    }
  }

  return ubLongestFilename;
}

/**
 * @brief      Create a list of files.
 *
 * @param      dirName          The directory name
 * @param      ppcGivenArray    Array to store the filenames to
 * @param[in]  ubNumberOfFiles  The number of files
 */
void EspSaveCrashSpiffs::getFileList(char* dirName, char** ppcGivenArray, uint8_t ubNumberOfFiles)
{
  // if no directory name is given
  if (!dirName)
  {
    // take the default root directory
    dirName = (char*)"/";
  }

  // search for files in the log directory
  Dir thisDirectory = SPIFFS.openDir(dirName);

  uint8_t i = 0;
  while (thisDirectory.next())
  {
    sprintf(ppcGivenArray[i], "%s", thisDirectory.fileName().c_str());

    i++;

    // break if more files are available than space in array
    if (i > ubNumberOfFiles)
    {
      break;
    }
  }
}

/**
 * @brief      Get remaining space of filesystem
 *
 * @return     Free space in byte
 */
uint32_t EspSaveCrashSpiffs::getFreeSpace()
{
  // struct FSInfo {
  //   size_t totalBytes;
  //   size_t usedBytes;
  //   size_t blockSize;
  //   size_t pageSize;
  //   size_t maxOpenFiles;
  //   size_t maxPathLength;
  // };
  FSInfo fs_info;

  // fill FSInfo struct with informations about the SPIFFS
  SPIFFS.info(fs_info);

  return (fs_info.totalBytes - (fs_info.usedBytes * 1.05));
}

/**
 * @brief      Check if this amout of space is free on the SPIFFS
 *
 * @param[in]  ulFileSize  The size of the thing to store
 *
 * @retval     True   Requested size will fit to the SPIFFS
 * @retval     False  Requested size won't fit
 */
bool EspSaveCrashSpiffs::checkFreeSpace(const uint32_t ulFileSize)
{
  return (getFreeSpace() > ulFileSize) ? true : false;
}

/**
 * @brief      Gets the current log file name.
 *
 * @return     The log file name.
 */
const char* EspSaveCrashSpiffs::getLogFileName()
{
  if (pcCrashFilePath)
  {
    return pcCrashFilePath;
  }
  else
  {
    return (char*)"";
  }
}

/**
 * @brief      Gets the last (n-1) log file name.
 *
 * @param      fileContent  Pointer to store the file content
 *
 * @return     The log file name.
 */
void EspSaveCrashSpiffs::getLastLogFileName(char* fileContent)
{
  File theFile = SPIFFS.open(LASTCRASHFILEPATH, "r");

  theFile.readBytes(fileContent, theFile.size());

  theFile.close();
}

/**
 * @brief      Sets the log file name.
 *
 * @param      fileName  The file name
 */
void EspSaveCrashSpiffs::setLogFileName(char *fileName)
{
  pcCrashFilePath = fileName;
}
