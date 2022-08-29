/*************************************************
 *************************************************
    Sketch BetaPorteData.ino   // gestion des liste de badge, plage horaire pour betaporte

    les données badges  sont stockée en flash sous forme de fichier : 1 ligne de JSON stringifyed par badges.json
    les donnees config sont stockée en flash sous forme de fichier : 1 ligne unique de JSON stringifyed  config.json

    Copyright 20201 Pierre HENRY net23@frdev.com .
    https://github.com/betamachine-org/BetaPorteV2   net23@frdev.com

  This file is part of BetaPorteV2.

    BetaPorteV2 is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BetaPorteV2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.



*************************************************/
#define HISTO_FNAME  F("/histo.json")
#define CONFIG_FNAME F("/config.json")
#define BADGE_FNAME F("/badges.json")
#define PLAGE_FNAME F("/plages.json")

//Version de la base 0 en cas d'erreur
uint16_t jobGetDistantBaseVersion() {
  Serial.println(F("jobGetDistantBaseVersion"));
  if (!WiFiConnected) return 0;   // en cas de reconnection evCheckDistantBase est relancée en auto

  // lecture de la version de la base distante
  String jsonStr;
  if (!dialWithGoogle(nodeName, "check", jsonStr)) return (0);
  JSONVar jsonData = JSON.parse(jsonStr);
  return (int)jsonData["baseversion"];
}


// lecture des badges puis ecriture sur la flash fichier badges.json
// la lecture est faite par paquet de 50 (taille ram dispo)
//
uint8_t jobReadDistantBadges(const bool fromStart) {
  uint8_t result = 2; // result : 0 = done  1 = continu 2 = abandon
  if (!WiFiConnected) return result;  // we need to have gsheetIndex at 0 if error

  D_println(Events.freeRam() + (0 * 01));
  static uint16_t currentBadgeIndex = 0;
  static uint16_t currentBadgeVersion = 0;
  if (fromStart) {
    currentBadgeIndex = 0;
    currentBadgeVersion = distantBaseVersion;
    MyLittleFS.remove(F("/badges.tmp"));  // raz le fichier temp
  }
  Serial.print(F("jobReadDistantBadges at "));
  Serial.println(currentBadgeIndex);

  String JsonStr = F("{\"first\":");
  JsonStr += currentBadgeIndex;
  JsonStr += F(",\"max\":50}");

  if (!dialWithGoogle(nodeName, F("getBadges"), JsonStr)) return (result);
  JSONVar jsonData = JSON.parse(JsonStr);
  uint16_t first = (int)jsonData["first"];
  uint16_t len = (int)jsonData["length"];
  uint16_t total = (int)jsonData["total"];
  uint16_t baseVersion = (int)jsonData["baseversion"];
  bool eof = jsonData["eof"];
  D_println(baseVersion);
  D_println(total);
  D_println(first);
  D_println(len);
  D_println(eof);
  //D_println(JSON.typeof(jsonData["badges"]));
  //D_println(Events.freeRam() + 02);
  if (currentBadgeVersion != baseVersion) {
    Serial.println(F("Abort lecture : new baseIndex"));
    distantBaseVersion = baseVersion;
    return (result);
  }
  File aFile = MyLittleFS.open(F("/badges.tmp"), "a");
  if (!aFile) return (result);
  if (first == 1) {
    JSONVar jsonHeader;
    jsonHeader["baseversion"] = baseVersion;
    jsonHeader["timestamp"] = (double)currentTime;
    jsonHeader["length"] = total;
    aFile.println(JSON.stringify(jsonHeader));
    //D_println(Events.freeRam() + 03);
  }
  //D_println(Events.freeRam() + 04);
  for (int N = 0 ; N < len ; N++ ) {

    D_println(jsonData["badges"][N]);
    //Entre le 01/01/1970 et le 01/01/2000, il s'est écoulé 10957 jours soit 30 ans.
    //transformation des date de validité de jours depuis 1/1/2000 en secondes depuis 1/1/1970
    time_t aDate = ((double)jsonData["badges"][N][2] + 10957) * 24 * 3600;
    jsonData["badges"][N][2] = (double)aDate;
    D_println((double)jsonData["badges"][N][2]);

    aDate = ((double)jsonData["badges"][N][3] + 10957) * 24 * 3600;
    jsonData["badges"][N][3] = (double)aDate;
    Serial.print(niceDisplayTime(aDate, true));
    Serial.println(" ");
    String aLine = JSON.stringify(jsonData["badges"][N]);
    D_println(aLine.length());  //["043826CAAA5C81","Test_01A",1609459200,1640908800,0,"PERM"]
    aFile.println(aLine);
  }
  aFile.close();
  aFile = MyLittleFS.open(F("/badges.tmp"), "r");
  if (!aFile) {
    D_println(F("TW: no saved config .conf !!!! "));
    return (result);
  }
  //  String aString = aFile.readStringUntil('\n');
  //  D_println(aString);  //aString => '{"baseindex":19,"timestamp":1633712861,"length":5}

  aFile.close();
  if (!eof) {
    // il reste des badges a lire on relance une lecture dans 5 secondes
    currentBadgeIndex = first + len;
    result = 1;
  } else {
    MyLittleFS.remove(BADGE_FNAME);
    MyLittleFS.rename(F("/badges.tmp"), BADGE_FNAME);
    Events.delayedPush(6 * 3600 * 1000, evCheckDistantBase); // recheck in 6 hours
    badgesBaseVersion = currentBadgeVersion;
    Serial.print(F("New base "));
    D_println(badgesBaseVersion);
    result = 0;
  }
  D_println(Events.freeRam() + 05);
  return (result);
}

// lecture de la version de la base sur la flash fichier badges.json
bool jobGetBadgesVersion() {
  File aFile = MyLittleFS.open(BADGE_FNAME, "r");
  if (!aFile) return (false);

  String aString = aFile.readStringUntil('\n');
  aFile.close();
  D_println(aString);  //aString => '{"baseindex":19,"timestamp":1633712861,"length":5}

  JSONVar jsonHeader = JSON.parse(aString);
  if (JSON.typeof(jsonHeader) != F("object")) return (false);

  // super check json data for "status" is a bool true  to avoid foolish data then supose all json data are ok.
  if (!jsonHeader.hasOwnProperty("baseversion") || JSON.typeof(jsonHeader["baseversion"]) != F("number") ) {
    return (false);
  }
  badgesBaseVersion = (int)jsonHeader["baseversion"];
  distantBaseVersion = badgesBaseVersion;
  D_println(badgesBaseVersion);
  return (true);

}

// lecture de la version de la base sur la flash fichier plages.json
bool jobGetPlagesVersion() {
  File aFile = MyLittleFS.open(PLAGE_FNAME, "r");
  if (!aFile) return (false);

  String aString = aFile.readStringUntil('\n');
  aFile.close();
  D_println(aString);  //aString => '{"baseindex":19,"timestamp":1633712861,"length":5}

  JSONVar jsonHeader = JSON.parse(aString);
  if (JSON.typeof(jsonHeader) != F("object")) return (false);

  // super check json data for "status" is a bool true  to avoid foolish data then supose all json data are ok.
  if (!jsonHeader.hasOwnProperty("baseversion") || JSON.typeof(jsonHeader["baseversion"]) != F("number") ) {
    return (false);
  }
  plagesBaseVersion = (int)jsonHeader["baseversion"];
  distantBaseVersion = plagesBaseVersion;
  D_println(plagesBaseVersion);
  return (true);
}



// lecture des plages horaire puis ecriture sur la flash fichier badges.json
// plagehoraire est une named array de plage en json
// en cas d'erreur retry 15 minutes
//
bool jobReadDistantPlages() {
  D_println(Events.freeRam() + 01);
  if (!WiFiConnected) return false;
  //Events.delayedPush(15 * 60 * 1000, evReadPlage); // reread in 15 min en cas d'erreur
  Serial.print(F("jobReadDistantPlages"));
  //MyLittleFS.remove(F("/plage.tmp"));  // raz le fichier temp
  String JsonStr = F("{\"max\":10}");
  if (!dialWithGoogle(nodeName, F("getPlagesHoraire"), JsonStr)) return (false);
  JSONVar jsonData = JSON.parse(JsonStr);
  uint16_t length = (int)jsonData["length"];
  uint16_t baseVersion = (int)jsonData["baseversion"];
  D_println(baseVersion);
  D_println(length);
  if (distantBaseVersion != baseVersion) {
    Serial.println(F("Abort lecture : new baseIndex"));
    distantBaseVersion = baseVersion;
    return (false);
  }
  plagesBaseVersion = baseVersion;
  File aFile = MyLittleFS.open(PLAGE_FNAME, "w");
  if (!aFile) return false;
  aFile.println(JsonStr);
  aFile.close();
  return (true);
}







// verification de la validité d'un badge dans la base sur la flash fichier badges.json
badgeMode_t jobCheckBadge(const String aUUID) {
  //enum badgeMode_t {bmOk, bmBadDate, bmBadTime, bmInvalide, bmBaseErreur, bmMAX };
  File aFile = MyLittleFS.open(BADGE_FNAME, "r");
  if (!aFile) return (bmBaseErreur);
  aFile.setTimeout(5);
  String aString = aFile.readStringUntil('\n');

  //D_println(aString);  //aString => '{"baseindex":19,"timestamp":1633712861,"length":5}

  JSONVar jsonHeader = JSON.parse(aString);
  if (JSON.typeof(jsonHeader) != F("object")) return (bmBaseErreur);

  if (!jsonHeader.hasOwnProperty("length") || JSON.typeof(jsonHeader["length"]) != F("number") ) {
    return (bmBaseErreur);
  }
  int len = jsonHeader["length"];
  D_println(len);
  int N = 0;
  while (N < len ) {
    aString = aFile.readStringUntil('\n');
    //D_println(aString);
    JSONVar jsonLine = JSON.parse(aString);
    //jsonLine[0] UUid
    //jsonLine[1] pseudo
    //jsonLine[2] date debut en jour
    //jsonLine[3] date fin en jours
    //jsonLine[4] plage
    //jsonLine[5] autorisation
    if ( JSON.typeof(jsonLine) == F("array") && jsonLine[0] == aUUID ) {
      aFile.close();
      String autorisation = (const char *)jsonLine[5];
      D_println(autorisation);
      if (autorisation.indexOf(F("PERM")) < 0) {
        D_println(niceDisplayTime((double)jsonLine[2]));
        D_println(niceDisplayTime(currentTime));
        D_println(niceDisplayTime((double)jsonLine[3]));
        if (currentTime < (double)jsonLine[2] || currentTime > (double)jsonLine[3]) {
          return (bmBadDate);
        }
      }
      Serial.print(F("Match "));
      D_println((const char*)jsonLine[1]);
      String pseudo = (const char*)jsonLine[1];
      messageL1 = pseudo.substring(0, 16);
      return (bmOk);
    }
    N++;
  }
  aFile.close();
  return (bmInvalide);
}

void writeHisto(const String aAction, const String aInfo) {
  //MyLittleFS.remove(F("/histo.txt"));  // raz le fichier temp
  {
    JSONVar jsonData;
    jsonData["timestamp"] = (double)currentTime;
    jsonData["action"] = aAction;
    jsonData["info"] = aInfo;
    String jsonHisto = JSON.stringify(jsonData);
    D_println(jsonHisto);
    //myObject["x"] = undefined;  remove an object from json
    File aFile = MyLittleFS.open(HISTO_FNAME, "a+");
    if (!aFile) return;
    aFile.println(jsonHisto);
    aFile.close();
  }
  Events.delayedPush(1 * 60 * 1000, evSendHisto); // send histo in 5 minutes (0.2 = 12 secondes pour test)
}


// drop an allocated object around 18K !!!!
//02:34:26.816 -> 12:34:14,CPU=100%,Loop=1,Nill=0,Ram=45880,Frag=44%,MaxMem=18144 Miss:3/0
//12:34:27.799 -> 12:34:15,CPU=100%,Loop=7,Nill=0,Ram=45880,Frag=31%,MaxMem=25936 Miss:9/0

void JobSendHisto() {
  if (!WiFiConnected) return;
  File aFile = MyLittleFS.open(HISTO_FNAME, "r");
  if (!aFile) return;
  aFile.setTimeout(5);
  if (!aFile.available()) {
    aFile.close();
    return;
  }
  //  JSONVar jsonData;
  //  for (uint8_t N = 0; N < 5 ; N++) {
  //    if (!aFile.available()) break;
  //    String aLine = aFile.readStringUntil('\n');
  //    //D_println(aLine);  //'{"action":"badge ok","info":"0E3F0FFA"}
  //    JSONVar jsonLine = JSON.parse(aLine);
  //    jsonData["liste"][N] = jsonLine;
  //  }
  //
  uint16_t abaseVersion;
  time_t aTimeZone;
  { // jsonData String allocation
    String jsonStr = F("{\"liste\":[");
    for (uint8_t N = 0; N < 10 ; N++) {
      if (!aFile.available()) break;
      if (N) jsonStr += ',';
      String aStr = aFile.readStringUntil('\n');
      aStr.replace("\r", "");
      jsonStr += aStr;
    }
    jsonStr += F("]}");

    // sendto google
    D_println(jsonStr);
    if (!dialWithGoogle(nodeName, F("histo"), jsonStr))  {
      Events.delayedPush(1 * 60 * 1000, evSendHisto); // retry in one hour
      aFile.close();
      return;
    }
    D_println(jsonStr);
    JSONVar jsonData;
    jsonData = JSON.parse(jsonStr);
    abaseVersion = (int)jsonData["baseversion"];
    aTimeZone = (double)jsonData["timezone"];
  }  // jsonData String de allocation

  D_println(abaseVersion);
  if ( abaseVersion != distantBaseVersion ) distantBaseVersion = abaseVersion;
  if (distantBaseVersion != badgesBaseVersion || distantBaseVersion != plagesBaseVersion) {
    Serial.println(F("demande de relecture de la base distante"));
    Events.delayedPush(60 * 1000, evCheckDistantBase);
  }

  // mise a jour de la time zone
  D_println(aTimeZone);
  if (aTimeZone != timeZone) {
    //writeHisto( F("Old TimeZone"), String(timeZone) );
    timeZone = aTimeZone;
    jobSetConfigInt("timezone", timeZone);
    // force recalculation of time
    setSyncProvider(getWebTime);
    currentTime = now();
    //writeHisto( F("New TimeZone"), String(timeZone) );
  }

  if (!aFile.available()) {
    aFile.close();
    MyLittleFS.remove(HISTO_FNAME);
    Serial.println("clear histo");
    return;
  }
  // cut file histo.tmp avec une copie en .tmp
  // todo: utiliser seek pour eviter la double copie ?
  File bFile = MyLittleFS.open(F("/histo.tmp"), "w");
  if (!bFile ) {
    aFile.close();
    return;
  }
  while ( aFile.available() ) {
    String aLine = aFile.readStringUntil('\n');
    bFile.println(aLine);
  }
  aFile.close();
  bFile.close();
  MyLittleFS.remove(HISTO_FNAME);
  MyLittleFS.rename(F("/histo.tmp"), HISTO_FNAME);
  Events.delayedPush(1 * 60 * 1000, evSendHisto); // send histo in 5 minutes
  Serial.println("more histo to send");
}


//get a value of a config key
String jobGetConfigStr(const String aKey) {
  String result = "";
  File aFile = MyLittleFS.open(CONFIG_FNAME, "r");
  if (!aFile) return (result);

  JSONVar jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
  aFile.close();
  if (JSON.typeof(jsonConfig[aKey]) == F("string") ) result = (const char*)jsonConfig[aKey];

  return (result);
}


// set a value of a config key
//todo : check if config is realy write ?
bool jobSetConfigStr(const String aKey, const String aValue) {
  // read current config
  JSONVar jsonConfig;  // empry config
  File aFile = MyLittleFS.open(CONFIG_FNAME, "r");
  if (aFile) {
    jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
    aFile.close();
  }
  jsonConfig[aKey] = aValue;
  aFile = MyLittleFS.open(CONFIG_FNAME, "w");
  if (!aFile) return (false);
  D_println(JSON.stringify(jsonConfig));
  aFile.println(JSON.stringify(jsonConfig));
  aFile.close();
  return (true);
}

int jobGetConfigInt(const String aKey) {
  int result = 0;
  File aFile = MyLittleFS.open(CONFIG_FNAME, "r");
  if (!aFile) return (result);
  aFile.setTimeout(5);
  JSONVar jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
  aFile.close();

  D_println(JSON.typeof(jsonConfig[aKey]));
  configOk = JSON.typeof(jsonConfig[aKey]) == F("number");
  if (configOk ) result = jsonConfig[aKey];
  return (result);
}

bool jobSetConfigInt(const String aKey, const int aValue) {
  // read current config
  JSONVar jsonConfig;  // empry config
  File aFile = MyLittleFS.open(CONFIG_FNAME, "r");
  if (aFile) {
    aFile.setTimeout(5);
    jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
    aFile.close();
  }
  jsonConfig[aKey] = aValue;
  aFile = MyLittleFS.open(CONFIG_FNAME, "w");
  if (!aFile) return (false);
  D_println(JSON.stringify(jsonConfig));
  aFile.println(JSON.stringify(jsonConfig));
  aFile.close();
  return (true);
}

void eraseHisto() {
  Serial.println(F("Erase  histo") );
  MyLittleFS.remove(HISTO_FNAME);
}

void eraseConfig() {
  Serial.println(F("Erase config") );
  MyLittleFS.remove(CONFIG_FNAME);
}


String grabFromStringUntil(String & aString, const char aKey) {
  String result;
  int pos = aString.indexOf(aKey);
  if ( pos == -1 ) {
    result = aString;
    aString = "";
    return (result);  // not match
  }
  result = aString.substring(0, pos);
  //aString = aString.substring(pos + aKey.length());
  aString = aString.substring(pos + 1);
  return (result);
}

void setMessage(const String & line2) {
  messageL2 = line2.substring(0, 16);
  lcdRedraw = true;
  Events.delayedPush(15 * 1000, evClearMessage);
}

void setMessage(const String & line1, const String & line2) {
  messageL1 = line1.substring(0, 16);
  setMessage(line2);
}

const  IPAddress broadcastIP(255, 255, 255, 255);

bool jobBroadcastCard(const String & cardid) {
  Serial.println("Send broadcast ");
  String message = F("cardreader\t");
  message += nodeName;
  message += F("\tcardid\t");
  message += cardid;
  message += F("\tuser\t");
  message += messageL1;
  message += '\n';

  if ( !MyUDP.beginPacket(broadcastIP, localUdpPort) ) {
    Serial.println(F("open broadcast error"));
    return false;
  }
  MyUDP.write(message.c_str(), message.length());
  MyUDP.endPacket();
  delay(100);
 if ( !MyUDP.beginPacket(broadcastIP, localUdpPort) ) {
    Serial.println(F("open broadcast error"));
    return false;
  }
  MyUDP.write(message.c_str(), message.length());
  MyUDP.endPacket();
  delay(100);
 if ( !MyUDP.beginPacket(broadcastIP, localUdpPort) ) {
    Serial.println(F("open broadcast error"));
    return false;
  }
  MyUDP.write(message.c_str(), message.length());
  MyUDP.endPacket();
  delay(100);
 if ( !MyUDP.beginPacket(broadcastIP, localUdpPort) ) {
    Serial.println(F("open broadcast error"));
    return false;
  }
  MyUDP.write(message.c_str(), message.length());

  Serial.print(message);
  return MyUDP.endPacket();
}


void handleUdpPacket() {
  int packetSize = MyUDP.parsePacket();
  if (packetSize) {
    Serial.print("Received packet UDP");
    Serial.printf("Received packet of size %d from %s:%d\n    (to %s:%d, free heap = %d B)\n",
                  packetSize,
                  MyUDP.remoteIP().toString().c_str(), MyUDP.remotePort(),
                  MyUDP.destinationIP().toString().c_str(), MyUDP.localPort(),
                  ESP.getFreeHeap());
    const int UDP_MAX_SIZE = 100;  // we handle short messages
    char udpPacketBuffer[UDP_MAX_SIZE + 1]; //buffer to hold incoming packet,
    int size = MyUDP.read(udpPacketBuffer, UDP_MAX_SIZE);

    // read the packet into packetBufffer
    if (packetSize > UDP_MAX_SIZE) {
      Serial.print(F("UDP too big "));
      return;
    }

    //TODO: clean this   cleanup line feed
    if (size > 0 && udpPacketBuffer[size - 1] == '\n') size--;

    udpPacketBuffer[size] = 0;

    String aStr = udpPacketBuffer;
    D_println(aStr);

    // Broadcast
    if  ( MyUDP.destinationIP() == broadcastIP ) {
      // it is a reception broadcast
      String bStr = F("cardreader\t");
      bStr += nodeName;
      bStr += F("B\tcardid\t");
      if (  aStr.startsWith(bStr) ) {
        aStr = aStr.substring(bStr.length());
        uint16_t pos = aStr.indexOf('\t');
        //D_println(pos);
        if (pos >= 4 && pos <= 16) {
          messageUUID = aStr.substring(0, pos);
          D_println(messageUUID);
          Events.delayedPush(1000,evBadgeTrame);
        }
        return;
      }
    }


  }
}
