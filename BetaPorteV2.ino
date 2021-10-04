/*************************************************
 *************************************************
    Sketch BetaPorteV2.ino   Gestion d'un four de poterie

    Copyright 20201 Pierre HENRY net23@frdev.com All - right reserved.

  This file is part of betaEvents.

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
    V2.0 (30/10/2021)
    - Full rebuild from BetaPorte V1.3 (15/03/2020)

  // TODO : gerer le changement d'horaire
  // TODO : gerer le changement NODE_NAME (nom de l'unitée)


*************************************************/

#include "ESP8266.h"

#define APP_NAME "BetaPorte V2.0"
#define NODE_NAME     "Node0"   // name of the device

//
/* Evenements du Manager (voir EventsManager.h)
  evNill = 0,      // No event  about 1 every milisecond but do not use them for delay Use pushDelayEvent(delay,event)
  ev100Hz,         // tick 100HZ    non cumulative (see betaEvent.h)
  ev10Hz,          // tick 10HZ     non cumulative (see betaEvent.h)
  ev1Hz,           // un tick 1HZ   cumulative (see betaEvent.h)
  ev24H,           // 24H when timestamp pass over 24H
  evInit,
  evInChar,
  evInString,
*/

#define Hex2Char(X) (char)((X) + ((X) <= 9 ? '0' : ('A' - 10)))
#define NOT_A_DATE_YEAR   2000

// Liste des evenements specifique a ce projet


enum tUserEventCode {
  // evenement utilisateurs
  evBP0 = 100,      // low = low power allowed
  evLed0,
  evLowPower,
  evBlinkClock,     // clignotement pendule
  //evLcdRedraw,  //update LCD
  evCheckBadge,  //timer lecture etat badge
  evBadgeIn,            // Arrivee du badge
  evBadgeOut,           // Sortie du badge
  // evenement action
  doReset,
};



// instance betaEvent

//  une instance "MyEvents" avec un poussoir "MyBP0" une LED "MyLed0" un clavier "MyKeyboard"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur pinBP0
//  MyLed0 genere un evenement evLed0 a chaque clignotement de la led precablée sur la platine
//  MyKeyboard genere un evenement evChar a char caractere recu et un evenement evString a chaque ligne recue
//  MyDebug permet sur reception d'un "T" sur l'entrée Serial d'afficher les infos de charge du CPU

#include <BetaEvents.h>


//  Info I2C

#define LCD_I2CADR  0x4E / 2  //adresse LCD

#include <Wire.h>         // Gestion I2C

// Instance LCD
#include <LiquidCrystal_PCF8574.h>
LiquidCrystal_PCF8574 lcd(LCD_I2CADR); // set the LCD address

// Objet d'interface pour le lecteur de badge
#include "BadgeNfc_PN532_I2C.h"
// Taille maxide la chaine RFID en lecture
#define MAXRFIDSIZE 100
BadgeNfc_PN532_I2C lecteurBadge;   // instance du lecteur de badge

//
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>

bool sleepOk = true;
int  multi = 0; // nombre de clic rapide

// gestion de l'ecran

bool  lcdOk = false;
bool  lcdRedraw = false;
char lcdTransmitSign = '!';
//enum displayMode_t { dmInfo,  dmMAX };
//displayMode_t  displayMode = dmInfo;

// Variable d'application locale
bool     badgePresent = false;
bool     WiFiConnected = false;
bool     lowPowerAllowed = false;
bool     lowPowerActive = false;
time_t   currentTime;


void setup() {

  Serial.begin(115200);
  Serial.println(F("\r\n\n" APP_NAME));

  //WiFi.forceSleepBegin();  // this do  a WiFiMode OFF  !!! 21ma

  // Start instance
  MyEvents.begin();

  D_println(WiFi.getMode());
  // This exemple supose a WiFi connection so if we are not WIFI_STA mode we force it
  if (WiFi.getMode() != WIFI_STA) {
    Serial.println(F("!!! Force WiFi to STA mode !!!  should be done only ONCE even if we power off "));
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
  lcd.print(F("\r" LCD_CLREOL ));
  Serial.println(F("NFC Module Ok."));


  // a beep
  beep( 880, 500);
  delay(500);
  beep( 988, 500);
  delay(500);
  beep( 1047, 500);

}


void loop() {
  MyEvents.getEvent(sleepOk);
  MyEvents.handleEvent();
  switch (MyEvents.currentEvent.code)
  {
    case evInit:
      Serial.println("Init");
      MyEvents.pushDelayEvent(1000, evCheckBadge); // arme la lecture du badge

      break;


    case ev24H:
      Serial.println("---- 24H ---");
      break;


    case ev10Hz: {
        // check for connection to local WiFi
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
          if (WiFiConnected) {
            setSyncProvider(getWebTime);
            setSyncInterval(60 * 59);
          }
          D_println(WiFiConnected);
        }

        // check and updaate LCD
        if (lcdOk) {
          if (!checkI2C(LCD_I2CADR)) {
            lcdOk = false;
            Serial.println(F("LCD lost"));
          } else {
            static uint8_t lastMinute = minute();
            if (lastMinute != minute()) {
              lastMinute = minute();
              lcdRedraw = true;
            }
            if ( lcdOk && lcdRedraw ) {
              lcd.home();

              if (year() < 2000) {
                lcd.print F("? Date ?");
              } else {
                lcd.print(Digit2_str(day()));
                lcd.print('/');
                lcd.print(Digit2_str(month()));
                lcd.print('/');
                lcd.print(Digit2_str(year() % 100));
              }

              lcd.print(F("   "));
              //lcdTransmitSign = ' ';
              lcd.print(Digit2_str(hour()));
              lcd.print(':');
              lcd.print(Digit2_str(minute()));
              MyEvents.pushDelayEvent(300, evBlinkClock, false);


              lcdRedraw = false;
            }
          }
        }

      }
      break;

    case evBlinkClock: {
        bool show = MyEvents.currentEvent.ext;
        lcd.setCursor(13, 0);
        lcd.print( show ? ':' : ' ');
        show = !show;
        MyEvents.pushDelayEvent(show ? 250 : 750, evBlinkClock, show);
      }
      break;

    case ev1Hz:
      // If we are not connected we warn the user every 30 seconds that we need to update credential
      currentTime = now();
      if ( !WiFiConnected && second() % 30 ==  15) {
        // every 30 sec
        static uint16_t lastWarn = millis();
        if ( millis() - lastWarn > 30 * 1000 ) {
          lastWarn = millis();
          Serial.print(F("device not connected to local "));
          D_println(WiFi.SSID());
          Serial.println(F("type 'W' to adjust WiFi credential"));
        }
      }

      if (!lcdOk && checkI2C(LCD_I2CADR)) {
        lcdOk = true;

        Serial.print(F("LCD ok"));
        lcd.clear();
        lcdRedraw = true;
      }


      break;

    // Detection changement d'etat badge
    case evCheckBadge: {
        int rateCheckBadge = 300; //lowPower  ? 2000 : 250;
        MyEvents.pushDelayEvent(rateCheckBadge, evCheckBadge); // je reessaye plus tard
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
        MyEvents.pushEvent((badgePresent ? evBadgeIn : evBadgeOut)); // Signalement a l'application
      }
      break;

    // Arrivee d'un badge
    case evBadgeIn: {
        beep( 1047, 200);
        MyEvents.pushDelayEvent(2 * 1000, evCheckBadge); // on laisse du temps a l'application pour lire et transmettre au moins une fois
        //leBadge = sBadge();
        // Affichage sur l'ecran
        //MyEvents.pushEvent(evShowLcd);
        lcd.clear();
        delay(500);
        String UUID = lecteurBadge.getUUIDTag();
        D_println(UUID);
        lcd.setCursor(0, 0);
        lcd.println(F("Bonjour ..."));
        lcd.println(UUID);
      }
      break;


    case doReset:
      helperReset();
      break;

    case evLowPower:
      if (MyEvents.currentEvent.ext) {
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
      switch (MyEvents.currentEvent.ext) {
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
        if (MyDebug.trackTime < 2) {
          char aChar = MyKeyboard.inputChar;
          if (isPrintable(aChar)) {
            D_println(aChar);
          } else {
            D_println(int(aChar));
          }
        }
        switch (toupper(MyKeyboard.inputChar))
        {
          case '0': delay(10); break;
          case '1': delay(100); break;
          case '2': delay(200); break;
          case '3': delay(300); break;
          case '4': delay(400); break;
          case '5': delay(500); break;
          case 'W': {
              Serial.println(F("SETUP WIFI : 'W WifiName,password"));
              if ( Serial.read() == ' ') {
                String ssid = Serial.readStringUntil(',');
                Serial.println(ssid);
                ssid.trim();
                if (ssid != "") {
                  String pass = Serial.readStringUntil('\n');
                  pass.replace("\r", "");
                  pass.trim();
                  Serial.println(pass);
                  bool result = WiFi.begin(ssid, pass);
                  Serial.print(F("WiFi begin "));
                  D_println(result);
                }
              }
            }
            break;
          case 'G': {
              JSONVar jsonData;
              dialWithGoogle(NODE_NAME, "getBaseInfo", jsonData);
              D_println( JSON.typeof(jsonData) );
              D_println( jsonData);

            }
        }
      }
      break;



    case evInString:
      if (MyDebug.trackTime < 2) {
        D_println(MyKeyboard.inputString);
      }
      if (MyKeyboard.inputString.equals("RESET")) {
        Serial.println(F("RESET"));
        MyEvents.pushEvent(doReset);
      }
      if (MyKeyboard.inputString.equals("S")) {
        sleepOk = !sleepOk;
        D_println(sleepOk);
      }




      break;


  }
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
      MyLed0.setOn(true);
      beep(988, 100);
      delay(150);
      MyLed0.setOn(false);
    }
    delay(500);
  }
  delay(2000);
  helperReset();
}


void beep(const uint16_t frequence, const uint16_t duree) {
  tone(BeepPin, frequence, duree);
}

void jobActionDetected() {
  if (lowPowerActive) {
    Serial.println(F("Wake from low power"));
    MyEvents.pushEvent(evLowPower, false);
  }
  if (lowPowerAllowed) {
    Serial.println(F("low power in 5 minutes"));
    MyEvents.pushDelayEvent(1 * 60 * 1000, evLowPower, true);
  }
}

String niceDisplayTime(time_t time) {

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
  if (txt == date) {
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
