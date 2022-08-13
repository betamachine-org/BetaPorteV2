/*************************************************
 *************************************************
    Sketch BetaPorteV2.ino   Gestion d'un four de poterie

    Copyright 20201 Pierre HENRY net234@frdev.com
    https://github.com/betamachine-org/BetaPorteV2   net234@frdev.com

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


  History
    V2.0 (30/09/2021)
    - Full rebuild from BetaPorte V1.3 (15/03/2020)

  // TODO : gerer le changement d'horaire
  // TODO : gerer le changement nodeName (nom de l'unitée)
  !! todo push timezone changed
    V2.0.1 (29/10/2021)
    Correction pour betaEvents V2.2
    V2.0.3 (29/03/2022)
    Correction pour betaEvents V2.3
    Correction bug perte de config sur changement d'horaire
    Meilleur de detection d'une absence de config
    V2.1.0 (13/08/2022)
    Gestion des betaporte escalve via UDP
    Gestion de la confirmation de la fermeture de la porte via le port


*************************************************/

// Definition des constantes pour les IO
#include "ESP8266.h"
static_assert(sizeof(time_t) == 8, "This version works with time_t 64bit  move to ESP8266 kernel 3.0 or more");

#define APP_NAME "BetaPorte V2.0.3"


//
/* Evenements du Manager (voir EventsManager.h)
  evNill = 0,      // No event  about 1 every milisecond but do not use them for delay Use pushDelay(delay,event)
  ev100Hz,         // tick 100HZ    non cumulative (see betaEvent.h)
  ev10Hz,          // tick 10HZ     non cumulative (see betaEvent.h)
  ev1Hz,           // un tick 1HZ   cumulative (see betaEvent.h)
  ev24H,           // 24H when timestamp pass over 24H
  evInit,
  evInChar,
  evInString,
*/


#define NOT_A_DATE_YEAR   2000

// Liste des evenements specifique a ce projet


enum tUserEventCode {
  // evenement utilisateurs
  evBP0 = 100,      // low = low power allowed
  evLed0,
  evLowPower,
  evCloseDoor,          // timer desactiver le relai
  evBlinkClock,         // clignotement pendule
  evCheckBadge,         //timer lecture etat badge
  evBadgeIn,            // Arrivee du badge
  evBadgeOut,           // Sortie du badge
  evBadgeTrame,         // Trame UDP avec un badge
  evCheckDistantBase,   // simple check de la base normalement toute les 6 heures (5 minutes apres un acces)
  evReadDistantBadges,   // lecture de la base distant (mmise a jour liste des badges)
  evReadDistantPlages,   // lecture de la base distant (mmise a jour liste des plages horares)
  evSendHisto,
  evClearMessage,
  // evenement action
  doReset,
};



// instance betaEvent

//  une instance "Events" avec un poussoir "BP0" une LED "Led0" un clavier "Keyboard"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur pinBP0
//  Led0 genere un evenement evLed0 a chaque clignotement de la led precablée sur la platine
//  Keyboard genere un evenement evChar a char caractere recu et un evenement evString a chaque ligne recue
//  Debug permet sur reception d'un "T" sur l'entrée Serial d'afficher les infos de charge du CPU

//#define DEFAULT_PIN
// les sortie pour la led et le poussoir sont definis dans esp8266.h avec BP0_PIN  et LED0_PIN
#include <BetaEvents.h>


//  Info I2C

#define LCD_I2CADR  0x4E / 2  //adresse LCD

#include <Wire.h>         // Gestion I2C

// Instance LCD
#include <LiquidCrystal_PCF8574.h>
LiquidCrystal_PCF8574 lcd(LCD_I2CADR); // set the LCD address

// Objet d'interface pour le lecteur de badge
#include "BadgeNfc_PN532_I2C.h"
// Taille maxide la chaine RFID en lecture (not used in this version)
#define MAXRFIDSIZE 50
BadgeNfc_PN532_I2C lecteurBadge;   // instance du lecteur de badge

// littleFS
#include <LittleFS.h>  //Include File System Headers 
#define MyLittleFS  LittleFS

//WiFI
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>

bool sleepOk = true;
int  multi = 0; // nombre de clic rapide

// gestion de l'ecran

bool  lcdOk = false;
bool  lcdRedraw = false;
char lcdTransmitSign = '!';

// rtc memory to keep date
//struct __attribute__((packed))
struct  {
  // all these values are keep in RTC RAM
  uint8_t   crc8;                 // CRC for savedRTCmemory
  time_t    actualTimestamp;      // time stamp restored on next boot Should be update in the loop() with setActualTimestamp
} savedRTCmemory;




// Variable d'application locale
String   nodeName = "NODE_NAME";    // nom de  la device (a configurer avec NODE=)"
//String   GKey = "";               // clef google Sheet (a configurer avec GKEY=)"
// clef lue localement dans dialWithGoogle pour economiser de la ram globale
bool     badgePresent = false;
bool     WiFiConnected = false;
bool     lowPowerAllowed = false;
bool     lowPowerActive = false;

time_t   currentTime;
int8_t   timeZone = -2;          //les heures sont toutes en localtimes
uint16_t badgesBaseVersion = 0;  //version de la base badges en flash
uint16_t plagesBaseVersion = 0;  //version de la base plages en flash
uint16_t distantBaseVersion = 0; //version derniere base distante connue
//uint16_t distantBaseIndex = 0;   //position de la lecture base en cours (uniquement pour les badges)

bool     configOk = true; // global used by getConfig...

String   messageL1;
String   messageL2;
String   messageUUID;
bool     displayClock;

bool     configErr = false;
enum badgeMode_t {bmOk, bmBadDate, bmBadTime, bmInvalide, bmBaseErreur, bmMAX };
//badgeMode_t badgeMode = bmInvalide;

#include <WiFiUdp.h>
// port d'ecoute UDP
const unsigned int localUdpPort = 23423;      // local port to listen on
//Objet UDP pour la liaison avec la console
WiFiUDP MyUDP;


void setup() {
  enableWiFiAtBootTime();   // obligatoire pour lekernel ESP > 3.0
  Serial.begin(115200);

  //porte fermée = gache active
  pinMode(GACHE_PIN, OUTPUT);
  digitalWrite(GACHE_PIN, GACHE_ACTIVE);   //Porte fermée

  Serial.println(F("\r\n\n" APP_NAME));

  // Start instance
  Events.begin();

  D_println(WiFi.getMode());
  // normaly not needed
  if (WiFi.getMode() != WIFI_STA) {
    Serial.println(F("!!! Force WiFi to STA mode !!!"));
    WiFi.mode(WIFI_STA);
    //WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  Serial.println(F("Bonjour ...."));

  //Init I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  //  Check LCD
  if (!checkI2C(LCD_I2CADR)) {
    Serial.println(F("No LCD detected"));
  }

  // Init LCD
  messageL1.reserve(16);
  messageL2.reserve(16);
  messageUUID.reserve(16);

  lcd.begin(16, 2); // initialize the lcd
  lcd.setBacklight(100);
  lcd.println(F(APP_NAME));

  //   Reset PN532
  //  pinMode(POWER_PN532, OUTPUT);
  //  digitalWrite(POWER_PN532, LOW);   //Reset PN532
  //  delay(500);
  //  digitalWrite(POWER_PN532, HIGH);   //Reset PN532



  // check PN532
  // Initialisation  Lecteur Badge  (sizebuffer retry  timeout )
  if  (lecteurBadge.begin(MAXRFIDSIZE) != true ) {
    Serial.println(F("NFC not ok."));
    lcd.setCursor(0, 1);
    lcd.print(F(LCD_CLREOL "Erreur NFC"));
    fatalError(2);
  }
  // Serial.println(F("NFC Module Ok."));

  // System de fichier
  if (!MyLittleFS.begin()) {
    Serial.println(F("erreur MyLittleFS"));
    fatalError(3);
  }

  // recuperation de l'heure dans la static ram de l'ESP
  if (!getRTCMemory()) {
    savedRTCmemory.actualTimestamp = 0;
  }
  // little trick to leave timeStatus to timeNotSet
  // TODO: see with https://github.com/PaulStoffregen/Time to find a way to say timeNeedsSync
  adjustTime(savedRTCmemory.actualTimestamp);
  currentTime = savedRTCmemory.actualTimestamp;

  // recuperation de la config dans config.json
  nodeName = jobGetConfigStr(F("nodename"));
  if (nodeName == "") {
    Serial.println(F("!!! Configurer le nom de la device avec 'NODE=nodename' !!!"));
    configErr = true;
  }
  D_println(nodeName);

  // recuperation de la timezone dans la config
  timeZone = jobGetConfigInt(F("timezone"));
  if (!configOk) {
    timeZone = -2; // par defaut France hivers
    jobSetConfigInt(F("timezone"), timeZone);
    Serial.println(F("!!! timezone !!!"));
  }
  D_println(timeZone);



  //GKey = jobGetConfigStr(F("gkey"));
  if (jobGetConfigStr(F("gkey")) == "") {
    Serial.println(F("!!! Configurer la clef google sheet avec 'GKEY=key' !!!"));
    configErr = true;
  }
  //D_println(GKey);

  if (configErr) {
    setMessage(F("Config Error"));
    beep( 880, 500);
    delay(500);
  } else {
    setMessage(F("Init Ok"), nodeName);
    // a beep
    beep( 880, 500);
    delay(500);
    beep( 988, 500);
    delay(500);
    beep( 1047, 500);
  }
  D_println(Events.freeRam());
}

String niceDisplayTime(const time_t time, bool full = false);

void loop() {
  Events.get(sleepOk);
  Events.handle();
  switch (Events.code)
  {
    case evNill:
      handleUdpPacket();        // handle UDP connection other betaporte
      break;


    case evInit:
      Serial.println("Init");
      jobGetBadgesVersion();
      jobGetPlagesVersion();
      writeHisto(F("boot"), "");
      Events.delayedPush(5 * 1000, evCheckBadge); // lecture badges dans 5 secondes
      break;


    case ev24H:
      Serial.println("---- 24H ---");
      break;


    case ev10Hz: {

        // check and updaate LCD
        if (lcdOk) {
          if (!checkI2C(LCD_I2CADR)) {
            lcdOk = false;
            Serial.println(F("LCD lost"));
          } else {
            if ( lcdOk && lcdRedraw ) {
              lcd.home();
              if (messageL1.length()) {
                lcd.print(messageL1);
              } else {
                if (year() < 2000) {
                  lcd.print F("? Date ?");
                } else {
                  lcd.print(Digit2_str(day()));
                  lcd.print('/');
                  lcd.print(Digit2_str(month()));
                  lcd.print('/');
                  lcd.print(Digit2_str(year() % 100));
                }
              }
              lcd.print(LCD_CLREOL);
              displayClock = (messageL1.length() < 11);
              if (displayClock) {
                lcd.setCursor(11, 0);
                lcd.print(Digit2_str(hour()));
                lcd.print(lcdTransmitSign);
                lcd.print(Digit2_str(minute()));
                Events.delayedPush(300, evBlinkClock, false);
              }
              lcd.setCursor(0, 1);
              String aStr = messageL2;
              if (aStr.length() == 0) aStr = nodeName.substring(0, 16);
              lcd.print(aStr);
              if (aStr.length() < 16) lcd.print(LCD_CLREOL);
              lcdRedraw = false;
            }
          }
        }

      }
      break;

    case evBlinkClock: {
        if (displayClock) {
          bool show = Events.ext;
          lcd.setCursor(13, 0);
          lcd.print( show ? lcdTransmitSign : ' ');
          show = !show;
          Events.delayedPush(show ? 250 : 750, evBlinkClock, show);
        }
      }
      break;

    case evClearMessage: {
        messageL1 = "";
        messageL2 = "";
        lcdRedraw = true;
      }
      break;
    case ev1Hz: {

        // check for connection to local WiFi  1 fois par seconde c'est suffisant
        static uint8_t oldWiFiStatus = 99;
        uint8_t  WiFiStatus = WiFi.status();
        if (oldWiFiStatus != WiFiStatus) {
          oldWiFiStatus = WiFiStatus;
          D_println(WiFiStatus);
          //    WL_IDLE_STATUS      = 0,
          //    WL_NO_SSID_AVAIL    = 1,
          //    WL_SCAN_COMPLETED   = 2,
          //    WL_CONNECTED        = 3,
          //    WL_CONNECT_FAILED   = 4,
          //    WL_CONNECTION_LOST  = 5,
          //    WL_DISCONNECTED     = 6
          WiFiConnected = (WiFiStatus == WL_CONNECTED);
          static bool wasConnected = WiFiConnected;
          if (wasConnected != WiFiConnected) {
            wasConnected = WiFiConnected;
            Led0.setFrequence(WiFiConnected ? 1 : 2);
            lcdTransmitSign = WiFiConnected ? ':' : '!';
            if (WiFiConnected) {
              setSyncProvider(getWebTime);
              setSyncInterval(6 * 3600);
              Events.delayedPush(5 * 60 * 1000, evCheckDistantBase);  // controle de la base dans 5 minutes
              // lisen UDP 23423
              Serial.println("Listen broadcast");
              MyUDP.begin(localUdpPort);
            }
            D_println(WiFiConnected);
            writeHisto( WiFiConnected ? F("wifi Connected") : F("wifi lost"), WiFi.SSID() );
          }
        }

        // Save current time in RTC memory (not erased by a reset)
        currentTime = now();
        savedRTCmemory.actualTimestamp = currentTime;  // save time in RTC memory
        saveRTCmemory();
        static uint8_t lastMinute = minute();
        if (lastMinute != minute()) {
          lastMinute = minute();
          lcdRedraw = true;
        }

        // If we are not connected we warn the user every 30 seconds that we need to update credential
        if ( !WiFiConnected && second() % 30 ==  15) {
          // every 30 sec
          Serial.print(F("module non connecté au Wifi local "));
          D_println(WiFi.SSID());
          Serial.println(F("taper WIFI= pour configurer le Wifi"));
        }

        // controle retours LCD
        if (!lcdOk && checkI2C(LCD_I2CADR)) {
          lcdOk = true;
          Serial.println(F("LCD ok"));
          lcd.clear();
          lcdRedraw = true;
        }
      }

      break;

    // Detection changement d'etat badge
    case evCheckBadge: {
        int rateCheckBadge = 300; //lowPower  ? 2000 : 250;
        Events.delayedPush(rateCheckBadge, evCheckBadge); // je reessaye plus tard
        // un badge est il present ?
        bool etatBadge = lecteurBadge.badgePresent();
        if (etatBadge == badgePresent) {
          // toujours dans le meme etat
          //          Serial.println(F("PWD"));
          if ( !lecteurBadge.powerDownMode() ) {

            lcd.clear();
            lcd.print(F("Reboot NFC"));
            if (Serial) Serial.print(F("Reboot NFC"));
            fatalError(2);
          }
          break; // non alors plus rien a faire
        }
        badgePresent = etatBadge;
        D_println(badgePresent);
        jobActionDetected();
        Events.push((badgePresent ? evBadgeIn : evBadgeOut)); // Signalement a l'application
      }
      break;

    // Arrivee d'un badge
    case evBadgeIn: {
        beep( 1047, 200);
        Events.delayedPush(2 * 1000, evCheckBadge); // on laisse du temps a l'application pour lire et transmettre au moins une fois
        Events.delayedPush(5 * 60 * 1000, evCheckDistantBase); // on controle la base dans 5 minutes

        //leBadge = sBadge();
        // Affichage sur l'ecran
        //Events.push(evShowLcd);
        lcd.clear();
        //delay(500);
        String UUID = lecteurBadge.getUUIDTag();
        D_println(UUID);

        badgeMode_t badgeMode = jobCheckBadge(UUID);
        jobBroadcastCard(UUID);
        if (badgeMode == bmOk) {
          Serial.println(F("Badge Ok "));
          setMessage(F("Bonjour ..."));
          jobOpenDoor();

          writeHisto(F("badge ok"), UUID);

        } else {
          delay(200);
          beep( 444, 400);
          //enum badgeMode_t {bmOk, bmBadDate, bmBadTime, bmInvalide, bmBaseErreur, bmMAX };

          String aString = F("Badge err : ");
          aString += badgeMode;
          Serial.println(aString);
          setMessage(aString);
          writeHisto(aString, UUID);
        }

      }
      break;

    // Arrivee d'un badge via trame distante (UUID dans messageUUID)
    case evBadgeTrame: {
        beep( 1047, 200);
        Events.delayedPush(3 * 60 * 1000, evCheckDistantBase); // on controle la base dans 3 minutes

        lcd.clear();
        D_println(messageUUID);

        badgeMode_t badgeMode = jobCheckBadge(messageUUID);
        if (badgeMode == bmOk) {
          Serial.println(F("Badge Distant Ok "));
          setMessage(F("Bonjour ..."));
          jobOpenDoor();

          writeHisto(F("badge distant ok"), messageUUID);

        } else {
          delay(200);
          beep( 444, 400);
          //enum badgeMode_t {bmOk, bmBadDate, bmBadTime, bmInvalide, bmBaseErreur, bmMAX };

          String aString = F("Badge distant err : ");
          aString += badgeMode;
          Serial.println(aString);
          setMessage(aString);
          writeHisto(aString, messageUUID);
        }

      }
      break;

      

    // controle de la version de la base distante
    case evCheckDistantBase:  {
        Serial.println(F("evCheckDistantBase"));
        // si les base locales (badges et plages) sont a jous on lance une demande de version a la base distante (toute les 6 heures)
        if (distantBaseVersion == badgesBaseVersion && distantBaseVersion == plagesBaseVersion) {
          uint16_t aVersion = jobGetDistantBaseVersion();
          if (aVersion == 0) {
            Serial.println(F("Erreur acces base distante"));
            Events.delayedPush(1 * 3600 * 1000, evCheckDistantBase); // recheck in 1 hours
            break;
          }
          distantBaseVersion = aVersion;
        }
        // controle si les 2 bases sont a jours
        if ( distantBaseVersion != badgesBaseVersion ) {
          Serial.println(F("demande de relecture de la base badges"));
          Events.delayedPush(10 * 1000, evReadDistantBadges, true); // reread data from 0
        } else         if ( distantBaseVersion != plagesBaseVersion ) {
          Serial.println(F("demande de relecture de la base plages"));
          Events.delayedPush(10 * 1000, evReadDistantPlages );
        } else   {
          // all ok re check in 6 hours
          Events.delayedPush(6 * 3600 * 1000, evCheckDistantBase); // recheck in 6 hours
        }
      }
      break;

    // relecture de la base distante des badge  cette base est lue par paquet de 50 elelments max
    case evReadDistantBadges: {
        Serial.println(F("evReadDistantBadges"));
        // si event.ext est true on fait une lecture depuis le debut
        uint8_t result = jobReadDistantBadges(Events.ext);
        // result : 0 = done  1 = continu 2 = abandon
        if (result == 0) {
          // validation de la version
          badgesBaseVersion = distantBaseVersion;
          D_println(badgesBaseVersion);
          writeHisto(F("Lecture badge Ok"), String(badgesBaseVersion));
        } else if (result == 1) {
          // il reste des elements a lire
          Events.delayedPush(10 * 1000, evReadDistantBadges, false);
          break;
        } else {
          writeHisto(F("Erreur lecture badge"), String(result));
        }
      }
      break;


    // relecture de la base distante des plages  cette base est lue en une seule fois
    case evReadDistantPlages: {
        Serial.println(F("evReadDistantPlages"));
        if (jobReadDistantPlages()) {
          // validation de la version
          plagesBaseVersion = distantBaseVersion;
          D_println(plagesBaseVersion);
          writeHisto(F("Lecture plages Ok"), String(plagesBaseVersion));
        } else {
          writeHisto(F("Erreur lecture plages"), "");
        }
      }
      break;



    case evSendHisto: {
        Serial.println(F("evSendHisto"));
        JobSendHisto();
      }
      break;

    case evCloseDoor: {
        jobCloseDoor();
      }
      break;

    case doReset:
      Events.reset();
      break;

    case evLowPower:
      if (Events.ext) {
        //Serial.println(F("Low Power On"));
        lowPowerActive = true;
        lcd.setBacklight(0);
        //WiFi.disconnect();
        //WiFi.mode(WIFI_OFF);
        //WiFi.forceSleepBegin();  // this do  a WiFiMode OFF  !!! 21ma
      } else {
        //Serial.println(F("Low Power Off"));
        lowPowerActive = false;
        lcd.setBacklight(100);
        //WiFi.mode(WIFI_STA);
        //WiFi.disconnect();
        //WiFi.reconnect();
        //jobSleepLater();
      }
      break;

    // BP0 = detecteur de presence
    case evBP0:
      switch (Events.ext) {
        case evxBPDown:
          lowPowerAllowed = true;
          break;
        case evxBPUp:
          lowPowerAllowed = false;
          break;
        default:
          return;
      }
      jobActionDetected();
      // D_println(lowPowerAllowed);
      break;

    case evInChar: {
        if (Debug.trackTime < 2) {
          char aChar = Keyboard.inputChar;
          //          if (isPrintable(aChar)) {
          //            D_println(aChar);
          //          } else {
          //            D_println(int(aChar));
          //          }
        }
        switch (Keyboard.inputChar)
        {
          case '0': delay(10); break;
          case '1': delay(100); break;
          case '2': delay(200); break;
          case '3': delay(300); break;
          case '4': delay(400); break;
          case '5': delay(500); break;


        }
      }
      break;



    case evInString:
      if (Debug.trackTime < 2) {
        D_println(Keyboard.inputString);
      }

      if (Keyboard.inputString.startsWith(F("NODE="))) {
        Serial.println(F("SETUP NODENAME : 'NODE=nodename'  ( this will reset)"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.replace(" ", "_");
        aStr.trim();

        if (aStr != "") {
          nodeName = aStr;
          D_println(nodeName);
          jobSetConfigStr(F("nodename"), nodeName);
          delay(1000);
          Events.reset();
        }
      }


      if (Keyboard.inputString.startsWith(F("GKEY="))) {
        Serial.println(F("SETUP GKEY : 'GKEY=google sheet key"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.trim();

        if (aStr != "") {
          D_println(aStr);
          jobSetConfigStr(F("gkey"), aStr);
        }
      }



      if (Keyboard.inputString.startsWith(F("WIFI="))) {
        Serial.println(F("SETUP WIFI : 'WIFI=WifiName,password"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        String ssid = grabFromStringUntil(aStr, ',');
        ssid.trim();
        D_println(ssid);
        if (ssid != "") {
          String pass = aStr;
          pass.trim();
          D_println(pass);
          bool result = WiFi.begin(ssid, pass);
          Serial.print(F("WiFi begin "));
          D_println(result);
        }

      }

      if (Keyboard.inputString.equals(F("RESET"))) {
        Serial.println(F("RESET"));
        Events.push(doReset);
      }
      if (Keyboard.inputString.equals(F("FREE"))) {
        D_println(Events.freeRam());
      }
      if (Keyboard.inputString.equals("S")) {
        sleepOk = !sleepOk;
        D_println(sleepOk);
      }
      if (Keyboard.inputString.equals(F("RAZCONF"))) {
        Serial.println(F("RAZCONF this will reset"));
        eraseConfig();
        delay(1000);
        Events.reset();
      }

      if (Keyboard.inputString.equals(F("CHECK"))) {
        Events.push(evCheckDistantBase);
      }

      if (Keyboard.inputString.equals(F("RBADGE"))) {
        bool jReadDistantBadges = jobReadDistantBadges(true);
        D_println(jReadDistantBadges);
      }

      if (Keyboard.inputString.equals(F("RPLAGE"))) {
        bool jReadDistantPlages = jobReadDistantPlages();
        D_println(jReadDistantPlages);
      }
      break;

  }
}

void jobOpenDoor() {
  digitalWrite(GACHE_PIN, !GACHE_ACTIVE);   //ouvre la porte
  Led0.setFrequence(5);
  Events.delayedPush(GACHE_TEMPO, evCloseDoor);
}

void jobCloseDoor() {
  digitalWrite(GACHE_PIN, GACHE_ACTIVE);   //Porte fermée
  Led0.setFrequence(WiFiConnected ? 1 : 2);
}


// helpers

// Check I2C
bool checkI2C(const uint8_t i2cAddr)
{
  Wire.beginTransmission(i2cAddr);
  return (Wire.endTransmission() == 0);
}


// fatal error
// flash led0 same number as error 10 time then reset
//
void fatalError(const uint8_t error) {
  Serial.print(F("Fatal error "));
  Serial.println(error);

  lcd.clear();
  lcd.setBacklight(100);
  lcd.print(F("Fatal error "));
  lcd.println(error);

  // display error on LED_BUILTIN
  for (uint8_t N = 1; N <= 5; N++) {
    for (uint8_t N1 = 1; N1 <= error; N1++) {
      delay(150);
      Led0.setOn(true);
      beep(988, 100);
      delay(150);
      Led0.setOn(false);
    }
    delay(500);
  }
  delay(2000);
  Events.reset();
}


void beep(const uint16_t frequence, const uint16_t duree) {
  tone(BEEP_PIN, frequence, duree);
}

void jobActionDetected() {
  if (lowPowerActive) {
    Serial.println(F("Wake from low power"));
    Events.push(evLowPower, false);
  }
  if (lowPowerAllowed) {
    Serial.println(F("low power in 5 minutes"));
    Events.delayedPush(1 * 60 * 1000, evLowPower, true);
  }
}

String niceDisplayTime(const time_t time, bool full) {

  String txt;
  // we supose that time < NOT_A_DATE_YEAR is not a date
  if ( year(time) < NOT_A_DATE_YEAR ) {
    txt = "          ";
    txt += time / (24 * 3600);
    txt += ' ';
    txt = txt.substring(txt.length() - 10);
  } else {

    txt = Digit2_str(day(time));
    txt += '/';
    txt += Digit2_str(month(time));
    txt += '/';
    txt += year(time);
  }

  static String date;
  if (!full && txt == date) {
    txt = "";
  } else {
    date = txt;
    txt += " ";
  }
  txt += Digit2_str(hour(time));
  txt += ':';
  txt += Digit2_str(minute(time));
  txt += ':';
  txt += Digit2_str(second(time));
  return txt;
}

// helper to save and restore RTC_DATA
// this is ugly but we need this to get correct sizeof()
#define  RTC_DATA(x) (uint32_t*)&x,sizeof(x)

bool saveRTCmemory() {
  setCrc8(&savedRTCmemory.crc8 + 1, sizeof(savedRTCmemory) - 1, savedRTCmemory.crc8);
  //system_rtc_mem_read(64, &savedRTCmemory, sizeof(savedRTCmemory));
  return ESP.rtcUserMemoryWrite(0, RTC_DATA(savedRTCmemory) );
}


bool getRTCMemory() {
  ESP.rtcUserMemoryRead(0, RTC_DATA(savedRTCmemory));
  //Serial.print("CRC1="); Serial.println(getCrc8( (uint8_t*)&savedRTCmemory,sizeof(savedRTCmemory) ));
  return ( setCrc8( &savedRTCmemory.crc8 + 1, sizeof(savedRTCmemory) - 1, savedRTCmemory.crc8 ) );
}

/////////////////////////////////////////////////////////////////////////
//  crc 8 tool
// https://www.nongnu.org/avr-libc/user-manual/group__util__crc.html


//__attribute__((always_inline))
inline uint8_t _crc8_ccitt_update  (uint8_t crc, const uint8_t inData)   {
  uint8_t   i;
  crc ^= inData;

  for ( i = 0; i < 8; i++ ) {
    if (( crc & 0x80 ) != 0 ) {
      crc <<= 1;
      crc ^= 0x07;
    } else {
      crc <<= 1;
    }
  }
  return crc;
}

bool  setCrc8(const void* data, const uint16_t size, uint8_t &refCrc ) {
  uint8_t* dataPtr = (uint8_t*)data;
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < size; i++) crc = _crc8_ccitt_update(crc, *(dataPtr++));
  //Serial.print("CRC "); Serial.print(refCrc); Serial.print(" / "); Serial.println(crc);
  bool result = (crc == refCrc);
  refCrc = crc;
  return result;
}
