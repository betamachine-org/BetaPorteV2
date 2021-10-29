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
    Events.pushDelay(1 * 3600 * 1000, evCheckGSheet); // recheck in 1 hours
    return false;
  }
  uint16_t baseIndex = (int)jsonData["baseindex"];
  if ( localBaseIndex != baseIndex ) {
    gsheetBaseIndex = baseIndex;
    Serial.println(F("demande de relecture des données GSheet"));
    Events.pushDelay(10 * 1000, evReadGSheet); // reread data from 0
  } else {
    // all ok re check in 6 hours
    Events.pushDelay(6 * 3600 * 1000, evCheckGSheet); // recheck in 6 hours
  }
  return true;
}


// lecture des badges puis ecriture sur la flash fichier badges.json
// la lecture est faire par paquet de 50
//
bool jobReadBadgesGSheet() {
  D_println(Events.freeRam() + 01);
  Events.pushDelay(15 * 60 * 1000, evCheckGSheet); // recheck in 15 min en cas d'erreur
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
  D_println(Events.freeRam() + 02);
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
    D_println(Events.freeRam() + 03);
  }
  D_println(Events.freeRam() + 04);
  for (int N = 0 ; N < len ; N++ ) {

    //    Serial.print(jsonData[N][0]);
    //    Serial.print(" ");
    //    Serial.print(jsonData[N][1]);
    //    Serial.print(" ");
    //    Serial.print(niceDisplayTime(jsonData[N][2], true));
    //    Serial.print("-");
    // pour limiter la taille des donnée JSON la date de debut et de fin est transmise 
    // en nombre de jour depuis 1/1/2000
    //Entre le 01/01/1970 et le 01/01/2000, il s'est écoulé 10957 jours soit 30 ans.

    time_t aDate = ((time_t)jsonData["badges"][N][2] + 10957) * 24 * 3600;
    jsonData["badges"][N][2] = aDate;

    aDate = ((time_t)jsonData["badges"][N][3] + 10957) * 24 * 3600;
    jsonData["badges"][N][3] = aDate;
    Serial.print(niceDisplayTime(aDate, true));
    Serial.println(" ");
    Serial.print(niceDisplayTime(jsonData["badges"][N][3], true));
    Serial.println(" ");
    String aLine = JSON.stringify(jsonData["badges"][N]);
    D_println(aLine);  //["043826CAAA5C81","Test_01A",1609459200,1640908800,0,"PERM"]
    aFile.println(aLine);
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
    Events.pushDelay(10000, evReadGSheet);
  } else {
    MyLittleFS.remove(F("/badges.json"));
    MyLittleFS.rename(F("/badges.tmp"), F("/badges.json"));
    Events.pushDelay(6 * 3600 * 1000, evCheckGSheet); // recheck in 6 hours
    localBaseIndex = baseIndex;
    Serial.print(F("New base "));
    D_println(localBaseIndex);
  }



  D_println(Events.freeRam() + 05);

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
badgeMode_t jobCheckBadge(const String aUUID) {
  //enum badgeMode_t {bmInvalide, bmBadDate, bmBadTime ,bmOk, bmBaseErreur bmMAX };
  File aFile = MyLittleFS.open(F("/badges.json"), "r");
  if (!aFile) return (bmBaseErreur);
  String aString = aFile.readStringUntil('\n');

  //D_println(aString);  //aString => '{"baseindex":19,"timestamp":1633712861,"badgenumber":5}

  JSONVar jsonHeader = JSON.parse(aString);
  if (JSON.typeof(jsonHeader) != F("object")) return (bmBaseErreur);

  if (!jsonHeader.hasOwnProperty("badgenumber") || JSON.typeof(jsonHeader["badgenumber"]) != F("number") ) {
    return (bmBaseErreur);
  }
  int badgeNumber = jsonHeader["badgenumber"];
  D_println(badgeNumber);
  int N = 0;
  while (N < badgeNumber ) {
    aString = aFile.readStringUntil('\n');
    //  D_println(aString);
    JSONVar jsonLine = JSON.parse(aString);
    //jsonLine[0] UUid
    //jsonLine[1] pseudo
    //jsonLine[2] date debut en jour
    //jsonLine[3] date fin en jours 
    if ( JSON.typeof(jsonLine) == F("array") && jsonLine[0] == aUUID ) {
      aFile.close();
      D_println(niceDisplayTime(jsonLine[2]));
      D_println(niceDisplayTime(currentTime));
      D_println(niceDisplayTime(jsonLine[3]));
      if (currentTime < (time_t)jsonLine[2] || currentTime > (time_t)jsonLine[3]) {
        return (bmBadDate);
      }
      Serial.print(F("Match "));
      D_println((const char*)jsonLine[1]);
      String pseudo = (const char*)jsonLine[1];
      currentMessage = pseudo.substring(0, 15);

      return (bmOk);
    }
    N++;
  }
  aFile.close();
  return (bmInvalide);

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
  Events.pushDelay(0.2 * 60 * 1000, evSendHisto); // send histo in 5 minutes
}


// drop an allocated object around 18K !!!!
//02:34:26.816 -> 12:34:14,CPU=100%,Loop=1,Nill=0,Ram=45880,Frag=44%,MaxMem=18144 Miss:3/0
//12:34:27.799 -> 12:34:15,CPU=100%,Loop=7,Nill=0,Ram=45880,Frag=31%,MaxMem=25936 Miss:9/0

void JobSendHisto() {
  if (!WiFiConnected) return;
  File aFile = MyLittleFS.open(F("/histo.json"), "r");
  if (!aFile) return;
  aFile.setTimeout(1);
  if (!aFile.available()) {
    aFile.close();
    return;
  }
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
    Events.pushDelay(1 * 60 * 1000, evSendHisto); // retry in one hour
    aFile.close();
    return;
  }
  uint16_t baseIndex = (int)jsonData["baseindex"];
  if ( localBaseIndex != baseIndex ) {
    gsheetBaseIndex = baseIndex;
    Serial.println(F("demande de relecture des données GSheet"));
    Events.pushDelay(60 * 1000, evReadGSheet); // reread data from 0
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
  Events.pushDelay(1 * 60 * 1000, evSendHisto); // send histo in 5 minutes
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
