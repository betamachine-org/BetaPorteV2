/*************************************************
 *************************************************
    Sketch BetaPorteData.ino   // gestion des liste de badge, plage horaire pour betaporte

    les données badges  sont stockée en flash sous forme de fichier : 1 ligne de JSON stringifyed par badges.json
    les donnees config sont stockée en flash sous forme de fichier : 1 ligne unique de JSON stringifyed  config.json

    Copyright 20201 Pierre HENRY net23@frdev.com .

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

// acces minimal a Gsheet
// si Google n'est pas ok on reessaye dans 1 heure
// si la baseIndex n'est pas a jour on lance un evReadGsheet

bool jobCheckGSheet()    {
  Serial.println(F("jobCheckGSheet"));
  if (!WiFiConnected) return false;

  JSONVar jsonData;
  if (!dialWithGoogle(nodeName, "check", jsonData)) {
    Serial.println(F("Erreur acces GSheet"));
    MyEvents.pushDelayEvent(1 * 3600 * 1000, evCheckGSheet); // recheck in 1 hours
    return false;
  }
  uint16_t baseIndex = (int)jsonData["baseindex"];
  if ( localBaseIndex != baseIndex ) {
    gsheetBaseIndex = baseIndex;
    Serial.println(F("demande de relecture des données GSheet"));
    MyEvents.pushDelayEvent(10 * 1000, evReadGSheet); // reread data from 0
  } else {
    // all ok re check in 6 hours
    MyEvents.pushDelayEvent(6 * 3600 * 1000, evCheckGSheet); // recheck in 6 hours
  }
  return true;
}


// lecture des badges puis ecriture sur la flash fichier badges.json
// la lecture est faire par paquet de 50
//
bool jobReadBadgesGSheet() {
  D_println(MyEvents.freeRam() + 01);
  MyEvents.pushDelayEvent(15 * 60 * 1000, evCheckGSheet); // recheck in 15 min en cas d'erreur
  Serial.print(F("jobReadBadgesGSheeet at "));
  Serial.println(gsheetIndex);
  if (gsheetIndex == 0) {
    MyLittleFS.remove(F("/badges.tmp"));  // raz le fichier temp
  }

  JSONVar jsonData;
  jsonData["first"] = gsheetIndex;
  gsheetIndex = 0;  // start from 0 if error
  jsonData["max"] = 50;

  if (!WiFiConnected) return false;
  if (!dialWithGoogle(nodeName, "getBadges", jsonData)) return (false);
  uint16_t first = (int)jsonData["first"];
  uint16_t len = (int)jsonData["len"];
  uint16_t total = (int)jsonData["total"];
  uint16_t baseIndex = (int)jsonData["baseindex"];
  bool eof = jsonData["eof"];
  D_println(baseIndex);
  D_println(total);
  D_println(first);
  D_println(len);
  D_println(eof);
  D_println(MyEvents.freeRam() + 02);
  if (baseIndex != gsheetBaseIndex) {
    Serial.println(F("Abort lecture : new baseIndex"));
    gsheetBaseIndex = baseIndex;
    return (false);
  }
  File aFile = MyLittleFS.open(F("/badges.tmp"), "a");
  if (!aFile) return false;
  if (first == 1) {
    JSONVar jsonHeader;
    jsonHeader["baseindex"] = baseIndex;
    jsonHeader["timestamp"] = currentTime;
    jsonHeader["badgenumber"] = total;
    aFile.println(JSON.stringify(jsonHeader));
    D_println(currentTime);
    D_println(MyEvents.freeRam() + 03);
  }
  D_println(MyEvents.freeRam() + 04);
  for (int N = 0 ; N < len ; N++ ) {
    String aLine = JSON.stringify(jsonData["badges"][N]);
    D_println(aLine);  //["043826CAAA5C81","Test_01A",1609459200,1640908800,0,"PERM"]
    aFile.println(aLine);

    //    Serial.print(jsonData[N][0]);
    //    Serial.print(" ");
    //    Serial.print(jsonData[N][1]);
    //    Serial.print(" ");
    //    Serial.print(niceDisplayTime(jsonData[N][2], true));
    //    Serial.print("-");
    Serial.print(niceDisplayTime(jsonData["badges"][N][3], true));
    Serial.println(" ");
  }
  aFile.close();
  aFile = MyLittleFS.open(F("/badges.tmp"), "r");
  if (!aFile) {
    D_println(F("TW: no saved config .conf !!!! "));
    return (false);
  }
  String aString = aFile.readStringUntil('\n');
  D_println(aString);  //aString => '{"baseindex":19,"timestamp":1633712861,"badgenumber":5}

  aFile.close();
  if (!eof) {
    // il reste des badges a lire on relance une lecture dans 5 secondes
    gsheetIndex = first + len;
    MyEvents.pushDelayEvent(10000, evReadGSheet);
  } else {
    MyLittleFS.remove(F("/badges.json"));
    MyLittleFS.rename(F("/badges.tmp"), F("/badges.json"));
    MyEvents.pushDelayEvent(0.6 * 3600 * 1000, evCheckGSheet); // recheck in 6 hours
    localBaseIndex = baseIndex;
    Serial.print(F("New base "));
    D_println(localBaseIndex);
  }



  D_println(MyEvents.freeRam() + 05);

  return (true);
}

// lecture de la version de la base sur la flash fichier badges.json
bool jobGetBaseIndex() {
  File aFile = MyLittleFS.open(F("/badges.json"), "r");
  if (!aFile) return (false);

  String aString = aFile.readStringUntil('\n');
  aFile.close();
  D_println(aString);  //aString => '{"baseindex":19,"timestamp":1633712861,"badgenumber":5}

  JSONVar jsonHeader = JSON.parse(aString);
  if (JSON.typeof(jsonHeader) != F("object")) return (false);

  // super check json data for "status" is a bool true  to avoid foolish data then supose all json data are ok.
  if (!jsonHeader.hasOwnProperty("baseindex") || JSON.typeof(jsonHeader["baseindex"]) != F("number") ) {
    return (false);
  }
  localBaseIndex = (int)jsonHeader["baseindex"];
  gsheetBaseIndex = localBaseIndex;
  D_println(localBaseIndex);



  return (true);

}



// verification de la validité d'un badge dans la base sur la flash fichier badges.json
bool jobCheckBadge(const String aUUID) {
  File aFile = MyLittleFS.open(F("/badges.json"), "r");
  if (!aFile) return (false);
  String aString = aFile.readStringUntil('\n');

  //D_println(aString);  //aString => '{"baseindex":19,"timestamp":1633712861,"badgenumber":5}

  JSONVar jsonHeader = JSON.parse(aString);
  if (JSON.typeof(jsonHeader) != F("object")) return (false);

  if (!jsonHeader.hasOwnProperty("badgenumber") || JSON.typeof(jsonHeader["badgenumber"]) != F("number") ) {
    return (false);
  }
  int badgeNumber = jsonHeader["badgenumber"];
  D_println(badgeNumber);
  int N = 0;
  while (N < badgeNumber ) {
    aString = aFile.readStringUntil('\n');
    //  D_println(aString);
    JSONVar jsonLine = JSON.parse(aString);
    if (JSON.typeof(jsonLine) != F("array")) break;
    if ( jsonLine[0] == aUUID) {
      Serial.print(F("Match "));
      D_println((const char*)jsonLine[1]);
      jsonUserInfo = jsonLine;
      aFile.close();
      return (true);
    }
    N++;
  }
  aFile.close();
  return (false);

}

void writeHisto(const String aAction, const String aInfo) {
  //MyLittleFS.remove(F("/histo.txt"));  // raz le fichier temp
  JSONVar jsonData;
  jsonData["timestamp"] = currentTime;
  jsonData["action"] = aAction;
  jsonData["info"] = aInfo;
  String jsonHisto = JSON.stringify(jsonData);
  D_println(jsonHisto);
  //myObject["x"] = undefined;  remove an object from json
  File aFile = MyLittleFS.open(F("/histo.json"), "a+");
  if (!aFile) return;
  aFile.println(jsonHisto);
  aFile.close();
  MyEvents.pushDelayEvent(0.2 * 60 * 1000, evSendHisto); // send histo in 5 minutes
}


//
void JobSendHisto() {
  if (!WiFiConnected) return;
  File aFile = MyLittleFS.open(F("/histo.json"), "r");
  if (!aFile || !aFile.available() ) return;
  aFile.setTimeout(1);
  JSONVar jsonData;
  for (uint8_t N = 0; N < 5 ; N++) {
    if (!aFile.available()) break;
    String aLine = aFile.readStringUntil('\n');
    //D_println(aLine);  //'{"action":"badge ok","info":"0E3F0FFA"}
    JSONVar jsonLine = JSON.parse(aLine);
    jsonData["liste"][N] = jsonLine;
  }

  // sendto google
  //D_println(JSON.stringify(jsonData));
  if (!dialWithGoogle(nodeName, "histo", jsonData))  {
    MyEvents.pushDelayEvent(1 * 60 * 1000, evSendHisto); // retry in one hour
    aFile.close();
    return;
  }
  uint16_t baseIndex = (int)jsonData["baseindex"];
  if ( localBaseIndex != baseIndex ) {
    gsheetBaseIndex = baseIndex;
    Serial.println(F("demande de relecture des données GSheet"));
    MyEvents.pushDelayEvent(60 * 1000, evReadGSheet); // reread data from 0
  }
  
  if (!aFile.available()) {
    aFile.close();
    MyLittleFS.remove(F("/histo.json"));
    Serial.println("clear histo");
    return;
  }
  // cut file histo.tmp avec une copie en .tmp
  // todo: utiliser seek pour eviter la double copie ?
  File bFile = MyLittleFS.open(F("/histo.tmp"), "w");
  if (!aFile ) {
    aFile.close();
    return;
  }
  while ( aFile.available() ) {
    String aLine = aFile.readStringUntil('\n');
    bFile.println(aLine);
  }
  aFile.close();
  bFile.close();
  MyLittleFS.remove(F("/histo.json"));
  MyLittleFS.rename(F("/histo.tmp"), F("/histo.json"));
  MyEvents.pushDelayEvent(1 * 60 * 1000, evSendHisto); // send histo in 5 minutes
  Serial.println("more histo to send");
}


//get a value of a config key
String jobGetConfigStr(const String aKey) {
  String result = "";
  File aFile = MyLittleFS.open(F("/config.json"), "r");
  if (!aFile) return (result);

  JSONVar jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
  aFile.close();
  if (JSON.typeof(jsonConfig[aKey]) == F("string") ) result = jsonConfig[aKey];

  return (result);
}

// set a value of a config key
//todo : check if config is realy write ?
bool jobSetConfigStr(const String aKey, const String aValue) {
  // read current config
  JSONVar jsonConfig;  // empry config
  File aFile = MyLittleFS.open(F("/config.json"), "r");
  if (aFile) jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
  aFile.close();
  jsonConfig[aKey] = aValue;
  aFile = MyLittleFS.open(F("/config.json"), "w");
  if (!aFile) return (false);
  D_println(JSON.stringify(jsonConfig));
  aFile.println(JSON.stringify(jsonConfig));
  aFile.close();
  return (true);
}
