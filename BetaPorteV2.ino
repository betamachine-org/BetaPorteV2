/*************************************************
 *************************************************
    Sketch BetaFour.ino   Gestion d'un four de poterie

    Copyright 20201 Pierre HENRY net23@frdev.com All - right reserved.

  This file is part of betaEvents.

    betaEvents is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    betaEvents is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.


  History
    V1.0 (21/11/2020)
    - Full rebuild from PH_Event V1.3.1 (15/03/2020)
       V1.1 (30/11/2020)
    - Ajout du percentCPU pour une meilleur visualisation de l'usage CPU


  e croquis utilise 267736 octets (25%) de l'espace de stockage de programmes. Le maximum est de 1044464 octets.
  Les variables globales utilisent 27348 octets (33%) de mémoire dynamique, ce qui laisse 54572 octets pour les
  e croquis utilise 267904 octets (25%) de l'espace de stockage de programmes. Le maximum est de 1044464 octets.
  Les variables globales utilisent 27300 octets (33%) de mémoire dynamique, ce qui laisse 54620 octets pour les variables locales. Le maximum est de 81920 oct
 *************************************************/

#include "ESP8266.h"

#define APP_NAME "BetaPorte V2.0"
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

// Liste des evenements specifique a ce projet
enum tUserEventCode {
  // evenement utilisateurs
  evBP0 = 100,
  evLed0,
  evLcdUpdate,  //update LCD
  // evenement action
  doReset,
};




// instance betaEvent

//  une instance "MyEvents" avec un poussoir "MyBP0" une LED "MyLed0" un clavier "MyKeyboard"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur D2
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

// just to shut off wifi on this basic version
#include <ESP8266WiFi.h>



bool sleepOk = true;
int  multi = 0; // nombre de clic rapide

// gestion de l'ecran

bool  lcdOk = false;
enum displayMode_t { dmInfo,  dmMAX };
displayMode_t  displayMode = dmInfo;

bool  displayRedraw = true;  // true si il faut reafficher entierement le lcd

void setup() {

  Serial.begin(115200);
  Serial.println(F("\r\n\n" APP_NAME));

  WiFi.forceSleepBegin();  // this do  a WiFiMode OFF  !!! 21ma

  // Start instance
  MyEvents.begin();

  Serial.println("Bonjour ....");

  //Init I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  //  Check LCD
  if (!checkI2C(LCD_I2CADR)) {
    fatalError(3);
  }

  // Init LCD
  lcd.begin(20, 4); // initialize the lcd
  lcd.setBacklight(100);
  lcd.println(F(APP_NAME));


  // a beep
  tone(BeepPin, 880, 500);
  delay(500);
  tone(BeepPin, 988, 500);
  delay(500);
  tone(BeepPin, 1047, 500);

}

byte BP0Multi = 0;   // multi click on BP0   should remove it


void loop() {
  MyEvents.getEvent(sleepOk);
  MyEvents.handleEvent();
  switch (MyEvents.currentEvent.code)
  {
    case evInit:
      Serial.println("Init");
      break;


    case ev24H:
      Serial.println("---- 24H ---");
      break;




    case ev10Hz:

      break;

    case ev1Hz:
      break;




    case doReset:
      helperReset();
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
        }


        break;

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

  lcd.begin(20, 4); // reinitialize the lcd (needed at boot)
  lcd.setBacklight(100);
  lcd.print(F("Fatal error "));
  lcd.println(error);

  // display error on LED_BUILTIN
  for (uint8_t N = 1; N <= 10; N++) {
    for (uint8_t N1 = 1; N1 <= error; N1++) {
      delay(150);
      MyLed0.setOn(true);
      delay(150);
      MyLed0.setOn(false);
      tone(BeepPin, 988, 100);
    }
    delay(500);
  }
  delay(2000);
  helperReset();
}
