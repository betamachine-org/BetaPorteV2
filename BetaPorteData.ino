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

  if (localBaseIndex != gsheetBaseIndex) {
    Serial.println(F("demande de relecture des données GSheet"));
    MyEvents.pushDelayEvent(10 * 1000, evReadGSheet); // reread data
  }
  return true;
}

// mark la base comme lu sur la version actuelle
bool jobMarkIndexReadGSheet()    {
  Serial.println(F("jobMarkIndexReadGSheet"));
  if (!WiFiConnected) return false;

  JSONVar jsonData;
  if (!dialWithGoogle(nodeName, "mark", jsonData)) {
    Serial.println(F("Erreur acces GSheet"));
    MyEvents.pushDelayEvent(1 * 3600 * 1000, evCheckGSheet); // recheck in 1 hours
    return false;
  }
  return true;
}


// lecture des badges puis ecriture sur la flash fichier badges.json
bool jobReadBadgesGSheet() {
  Serial.println(F("jobReadBadgesGSheeet"));
  D_println(MyEvents.freeRam() + 01);
  JSONVar jsonData;
  if (!dialWithGoogle(nodeName, "getBadges", jsonData)) return (false);
  D_println(MyEvents.freeRam() + 02);
  uint16_t badgeNumber = jsonData.length();
  D_println(badgeNumber);
  if (badgeNumber >= 1000) {
    badgeNumber = 1000;
  }

  File aFile = MyLittleFS.open(F("/badges.json"), "w");
  if (!aFile) return false;
  {
    JSONVar jsonHeader;
    jsonHeader["baseindex"] = gsheetBaseIndex;
    jsonHeader["timestamp"] = currentTime;
    jsonHeader["badgenumber"] = badgeNumber;
    aFile.println(JSON.stringify(jsonHeader));
    D_println(MyEvents.freeRam() + 03);

  }
  D_println(MyEvents.freeRam() + 04);
  for (int N = 0 ; N < badgeNumber ; N++ ) {
    String aLine = JSON.stringify(jsonData[N]);
    D_println(aLine);  //["043826CAAA5C81","Test_01A",1609459200,1640908800,0,"PERM"]
    aFile.println(aLine);

    //    Serial.print(jsonData[N][0]);
    //    Serial.print(" ");
    //    Serial.print(jsonData[N][1]);
    //    Serial.print(" ");
    //    Serial.print(niceDisplayTime(jsonData[N][2], true));
    //    Serial.print("-");
    //    Serial.print(niceDisplayTime(jsonData[N][3], true));
    //    Serial.println(" ");
  }
  aFile.close();
  aFile = MyLittleFS.open(F("/badges.json"), "r");
  if (!aFile) {
    D_println(F("TW: no saved config .conf !!!! "));
    return (false);
  }
  String aString = aFile.readStringUntil('\n');
  D_println(aString);  //aString => '{"baseindex":19,"timestamp":1633712861,"badgenumber":5}

  aFile.close();
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
  //D_println(badgeNumber);
  int N = 0;
  while (N < badgeNumber ) {
    aString = aFile.readStringUntil('\n');
    JSONVar jsonLine = JSON.parse(aString);
    if (JSON.typeof(jsonLine) != F("array")) break;
    if ( jsonLine[0] == aUUID) {
      Serial.print("Match ");
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
