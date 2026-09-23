/* 
***************************************************************************  
**  Program  : File System, part of DSMRloggerAPI
**
**  Copyright (c) 2026 Smartstuff / based on DSMR Api Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

//===========================================================================================
int32_t freeSpace() 
{
  int32_t space;
  space = (int32_t)(LittleFS.totalBytes() - LittleFS.usedBytes());
  return space;
  
} // freeSpace()

//===========================================================================================
String indexRefPath()
{
  return String(settingIndexPage) + ".ver";
}

bool indexFileUsable(const char* path)
{
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  const size_t size = file.size();
  file.close();
  return size > 200;
}

bool FirmwareIndexPinned()
{
  File file = LittleFS.open(indexRefPath(), "r");
  if (!file) return false;
  String ref = file.readString();
  file.close();
  ref.trim();
  return ref == CDN_FORK_REF;
}

void writeFirmwareIndexRef()
{
  File file = LittleFS.open(indexRefPath(), "w");
  if (!file) return;
  file.print(CDN_FORK_REF);
  file.close();
}

String RewriteFirmwareCdnRefs(const String& html)
{
  if (!FirmwareIndexPinned()) return html;

  const String marker = String(CDN_FORK_REPO) + "@";
  const String pinned = marker + CDN_FORK_REF;
  String out;
  out.reserve(html.length() + 32);
  int pos = 0;
  while (pos < (int)html.length()) {
    const int at = html.indexOf(marker, pos);
    if (at < 0) {
      out += html.substring(pos);
      break;
    }
    const int slash = html.indexOf('/', at + marker.length());
    if (slash < 0) {
      out += html.substring(pos);
      break;
    }
    out += html.substring(pos, at);
    out += pinned;
    pos = slash;
  }
  return out;
}

void SendCachedIndexPage()
{
  File file = LittleFS.open(settingIndexPage, "r");
  if (!file) {
    httpServer.send(404, "text/plain", F("FileNotFound\r\n"));
    return;
  }

  httpServer.sendHeader("Cache-Control", "no-store, max-age=0");
  httpServer.sendHeader("Pragma", "no-cache");
  if (!FirmwareIndexPinned() || file.size() > 32768) {
    httpServer.streamFile(file, "text/html");
    file.close();
    return;
  }

  const String html = RewriteFirmwareCdnRefs(file.readString());
  file.close();
  httpServer.send(200, "text/html", html);
}

//===========================================================================================
bool EnsureIndexFilePresent()
{
  const bool exists = indexFileUsable(settingIndexPage);
  if (exists && FirmwareIndexPinned()) return true;

  String backup;
  if (exists) {
    DebugTln(F("Index UI does not match firmware, refreshing"));
    backup = String(settingIndexPage) + ".bak";
    LittleFS.remove(backup);
    if (!LittleFS.rename(settingIndexPage, backup.c_str())) return true;
  } else {
    DebugTln(F("Oeps! Index file not pressent, try to download it!\r"));
  }

  if (GetFile(settingIndexPage, PATH_DATA_FILES) && indexFileUsable(settingIndexPage)) {
    if (backup.length()) LittleFS.remove(backup);
    writeFirmwareIndexRef();
    return true;
  }

  LittleFS.remove(settingIndexPage);
  if (backup.length() && LittleFS.exists(backup)) {
    LittleFS.rename(backup.c_str(), settingIndexPage);
    DebugTln(F("Keeping installed index until the versioned UI can be downloaded"));
    return true;
  }

  DebugTln(F("Index file not found at version URL, try fallback URL!\r"));
  if (GetFile(settingIndexPage, URL_INDEX_FALLBACK) && indexFileUsable(settingIndexPage)) return true;

  DebugTln(F("Index file still not pressent!\r"));
  return false;
}

//===========================================================================================
void listFS() 
{
   typedef struct _fileMeta {
    char    Name[30];     
    int32_t Size;
  } fileMeta;

  _fileMeta dirMap[30];
  int fileNr = 0;
  
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while ( file && ( fileNr < 30 ) )  
  {
    dirMap[fileNr].Name[0] = '\0';
    strlcpy(dirMap[fileNr].Name, file.name(), sizeof(dirMap[fileNr].Name));
    dirMap[fileNr].Size = file.size();
    fileNr++;
    file = root.openNextFile();
  }

  // -- bubble sort dirMap op .Name--
  for (int8_t y = 0; y < fileNr; y++) {
    yield();
    for (int8_t x = y + 1; x < fileNr; x++)  {
      //DebugTf("y[%d], x[%d] => seq[x][%s] ", y, x, dirMap[x].Name);
      if (compare(String(dirMap[x].Name), String(dirMap[y].Name)))  
      {
        fileMeta temp = dirMap[y];
        dirMap[y]     = dirMap[x];
        dirMap[x]     = temp;
      } /* end if */
      //Debugln();
    } /* end for */
  } /* end for */

  DebugTln(F("\r\n"));
  for(int f=0; f<fileNr; f++)
  {
    Debugf("%-25s %6ld bytes \r\n", dirMap[f].Name, (long)dirMap[f].Size);
    yield();
  }

  Debugln(F("\r"));
  if (freeSpace() < 10.240) Debugf("Available File System space [%6ld]kB (LOW ON SPACE!!!)\r\n", freeSpace() / 1024);
  else  {
    Debugf("Available File System space [%6ld]kB\r\n", freeSpace() / 1024);
    Debugf("           File System Size [%6lu]kB\r\n", (unsigned long)(LittleFS.totalBytes() / 1024));
  }

} // listFS()


//===========================================================================================
void eraseFile() 
{
  char eName[30] = "";

  //--- erase buffer
  while (TelnetStream.available() > 0) 
  {
    yield();
    TelnetStream.read();
  }

  Debug("Enter filename to erase: ");
  TelnetStream.setTimeout(10000);
  TelnetStream.readBytesUntil('\n', eName, sizeof(eName)); 
  TelnetStream.setTimeout(1000);

  //--- remove control chars like \r and \n ----
  //--- and shift all char's one to the right --
  for(int i=strlen(eName); i>0; i--) 
  {
    eName[i] = eName[i-1];
    if (eName[i] < ' ') eName[i] = '\0';
  }
  //--- add leading slash on position 0
  eName[0] = '/';

  if (LittleFS.exists(eName))
  {
    Debugf("\r\nErasing [%s] from FS\r\n\n", eName);
    LittleFS.remove(eName);
  }
  else
  {
    Debugf("\r\nfile [%s] not found..\r\n\n", eName);
  }
  //--- empty buffer ---
  while (TelnetStream.available() > 0) 
  {
    yield();
    TelnetStream.read();
  }

} // eraseFile()


//===========================================================================================
bool DSMRfileExist(const char* fileName, bool doDisplay) 
{
  char fName[30] = "";
  if (fileName[0] != '/') strConcat(fName, 5, "/");
  
  strConcat(fName, 29, fileName);
  
  DebugTf("check if [%s] exists .. ", fName);
 

  if (!LittleFS.exists(fName) )
  {
    if (doDisplay)
    {
      Debugln(F("NO! Error!!")); 
      return false;
    }
    else
    {
      Debugln(F("NO! "));
      return false;
    }
  } 
  else 
  {
    Debugln(F("Yes! OK!"));
    
  }
  return true;
  
} //  DSMRfileExist()


/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
