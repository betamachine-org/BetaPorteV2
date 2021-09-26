// Gestion de l'encodage et des dates
// KEY= JJ/MM/AAAA-JJJ:CC
// JJJ = duree en jours  (000 - 999)
// CC  = type d'abonnement (00..99)
//        int result = sscanf(TheEvent.inputString.c_str(),"%2u",  &duree );  //"%2u/%2u/%4u-%4u:%3c"


//3 type d'abonnements 
//1 a 9   => acces si le lab est ouvert (swith "lab est ouvert") durant la periode de validité + clef non invalidée par la domotique
//10 a 19 => acces 24/24 durant la periode de validité + clef validée par la domotique + creation de badge type 1 a 9 avec la console
//20 a 29 => acces 24/24 durant la periode de validité + clef validée par la domotique + creation de badge tout type avec la console
//La console log dans un CSV la creation des badges et quel abonnement a crée le badge
//
//Les 2 derniers digits de la clef changent a chaque creation/modification de badge

// Variable du badge decodé
struct sBadge {
  uint8_t   wDay = 0;
  uint8_t   day  = 0;
  uint8_t   month = 0;
  uint16_t  year  = 0;
  uint16_t  duree = 0;
  uint8_t   abonnement = 0;
  String ID = "";
  uint8_t  checksum = 0;


} leBadge;


//#define Hex2Char(X) (X + (X <= 9 ? '0' : ('A' - 10)))
char Hex2Char( byte aByte) {
  return aByte + (aByte <= 9 ? '0' : 'A' -  10);
}
//#define Char2Hex(X) (X  - (X <= '9' ? '0' : ('A' - 10)))
byte Char2Hex(unsigned char aChar) {
  return aChar  - (aChar <= '9' ? '0' : 'A' - 10);
}


//====== Sram dispo =========
int freeRam () {

#ifndef __AVR__
  int result = ESP.getFreeHeap();
#else
  extern int __heap_start, *__brkval;
  int result =  (int) &result - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
#endif

  return (result);
}
void Serial_print_freeRam() {
  Serial.print(F("Ram="));
  Serial.print(freeRam());
  Serial.println();
}


// encode un badge en chaine de caractere
void badgeEncode(sBadge &aBadge, String  &aStr) {
  byte aBuffer[6];
  //
  aBuffer[0] = millis();                                        //   salt
  //  aBuffer[0] = 0;
  aBuffer[1] = (aBadge.day & 0x1F) << 3   | (aBadge.month & 0x0E) >> 1;                //05bit day (1..31) 0x1F
  aBuffer[2] = (aBadge.month & 0x01) << 7 | ( (aBadge.year - 2000) & 0x03F8 ) >> 3;    //04bit month (1..12) 0x0F
  aBuffer[3] = ((aBadge.year - 2000) & 0x0007) << 5 | (aBadge.duree & 0x03E0) >> 5;    //10bit year (2000-2999) 0x03FF
  aBuffer[4] = (aBadge.duree & 0x001F) << 3 | (aBadge.abonnement & 0x0070) >> 4;       //10bit duree (0-999)    0x03FF
  aBuffer[5] = (aBadge.abonnement & 0x000F) << 4 ;        //07bit abonnement (0-99) 0x7F
  //36bit au total 6 Octets avec le salt
  // Checksum 4 bit
  byte chk = 0;
  for (byte N = 0; N < 6; N++) {
    chk += (aBuffer[N] & 0x0F) + ((aBuffer[N] & 0xF0) >> 4);
  }
  aBuffer[5] |= (chk & 0x0F) ^ 0x0F;

  // Offuscation
  for (byte N = 1; N < 6; N++) {
    aBuffer[N] ^=  aBuffer[N - 1];
  }
  chk = 0;
  for (byte N = 0; N < leBadge.ID.length(); N++) {
    chk += leBadge.ID[N];
  }
  aBuffer[5] += chk;
  aBadge.checksum = aBuffer[5];
  //   Encodage en Hexa (ajout a la chaine existante)
  // aStr = "";
  for (byte N = 0; N < 6; N++) {
    aStr += Hex2Char( aBuffer[N] >> 4 );
    aStr += Hex2Char( aBuffer[N] & 0xF);
  }
}


// Decode une chaine en badge  return false si le code n'est pas valide
bool badgeDecode( sBadge &aBadge, const String  &aStr) {
  // Decodage Hexa
  byte aBuffer[6];

  for (byte N = 0; N < 6; N++) {
    //aBuffer[N]=(Char2Hex(aStr.charAt(N+N))<<4) + Char2Hex(aStr.charAt(N+N+1));
    aBuffer[N] = (Char2Hex(aStr[N + N]) << 4) + Char2Hex(aStr[N + N + 1]);

  }
  aBadge.checksum = aBuffer[5];
  // Deoffuscation
  byte chk = 0;
  for (byte N = 0; N < aBadge.ID.length(); N++) {
    chk += leBadge.ID[N];
  }
  aBuffer[5] -= chk;

  for (byte N = 5; N > 0; N--) {
    aBuffer[N] ^=  aBuffer[N - 1] ;
  }

  //checksum
  chk = 1;
  for (byte N = 0; N < 6; N++) {
    chk += (aBuffer[N] & 0x0F) + ((aBuffer[N] & 0xF0) >> 4);
  }
  chk &= 0x0F;
  // controle du checksum
  if (chk == 0) {
    aBadge.day = (aBuffer[1] & 0xF8) >> 3;
    aBadge.month = (aBuffer[1] & 0x07) << 1 | (aBuffer[2] & 0x80) >> 7;
    aBadge.year = 2000 + ((aBuffer[2] & 0x7F) << 3 | (aBuffer[3] & 0xE0) >> 5);
    aBadge.duree = (aBuffer[3] & 0x1F) << 5 | (aBuffer[4] & 0xF8 ) >> 3;
    aBadge.abonnement = (aBuffer[4] & 0x07) << 4 | (aBuffer[5] & 0xF0) >> 4;
  } else {
    aBadge = sBadge();
  }
  return (chk == 0);
}


//
// il n'y aura pas d'exception d'annne bysectile avant 2100 :)
#define LEAP_YEAR(Y)     ( ((Y)%4==0) && ((Y)%100!=0)) || ((Y)%400==0)

//Que va-t-il se passer le 19 janvier 2038 a 03:14:07, :) OK  up to 6 Fev 20106
//   from https://github.com/PaulStoffregen/Time/blob/master/Time.cpp

const byte monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

uint16_t getDayStamp(const sBadge &aBadge) {
  // Transforme la date du badge en nombre de jours depuis 1970
  // L'utilisation d'un daystamp Ungned est ok up to 2148
  // L'utilisation d'un daystamp Signed est ok up to 2058
  // retourne 0 si l'année est < 1970 ou > 2105

  uint16_t days = 0;  // nombre de seconde depuis 1970
  if (aBadge.year < 1970 || aBadge.year > 2105) {
    return (days);
  }
  short N;
  if (aBadge.year >= 2019) {  // petite economie de 39 boucles :)
    days = 17897;
    N = 2019;
  } else {
    N = 1970;
  }
  // days from 1970 till 1 jan 00:00:00 of the given year

  while (N <  aBadge.year ) {
    days += (LEAP_YEAR(N)) ? 366 : 365;
    N++;
  }
  //const byte monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  // Gestion des mois
  for (N = 1; N < aBadge.month; N++) {
    days +=  monthDays[N - 1];
  }
  if ( (aBadge.month >= 2) && LEAP_YEAR(aBadge.year) ) {
    days ++; // +1 day pour fevrier
  }
  days += aBadge.day - 1;
  return (days);
}


const char wDayShortNames[] = "Dim\0Lun\0Mar\0Mer\0Jeu\0Ven\0Sam\0Dim\0";

char* wDayStr(byte wday) {
  return ((char*)&wDayShortNames[(wday & 0x7) * 4]);
}

void  breakDayStamp(sBadge &badge, const uint16_t adaystamp) {
  uint16_t  dayRemaining = adaystamp;
//  Serial.println(dayRemaining);
  badge.wDay = ((dayRemaining + 4) % 7); // Dim = 0 Lun = 1 .. sam = 6   (7 is sun too)
  uint16_t year = 1970;    // epoch for the system
  if (dayRemaining >= 17897U) {
    dayRemaining -= 17897U;  // 18262UL = 2020  17897UL = 2019 
    year = 2019;
  }

  int dYear = 365; // 1970 ou 2019 n'est pas est leapyear
  while (dayRemaining >= dYear) {
  //  Serial.println(dayRemaining);
    dayRemaining -= dYear;
    year++;
    dYear = (LEAP_YEAR(year)) ? 366 : 365;
             //    Serial.print(dayRemaining); Serial.print("  ");  Serial.print(year); Serial.println("  ");1577836800
  }
  dYear -= 365;
  uint16_t month = 0;
  //Serial.print(dayRemaining); Serial.print("  ");  Serial.print(monthDays[month] +   ((month == 1) ? dYear : 0 )) ; Serial.print("  ");  Serial.print(dYear); Serial.println("  ");
  while (dayRemaining >= (monthDays[month] +   ((month == 1) ? dYear : 0 )) ) {

    dayRemaining -= (monthDays[month] +   ((month == 1) ? dYear : 0 ));

    month++;
    //  Serial.print(dayRemaining); Serial.print("  ");  Serial.print(monthDays[month] +   ((month == 1) ? dYear : 0 )) ; Serial.print("  ");  Serial.print(dYear); Serial.println("  ");
  }
  month++;
  badge.year = year;
  badge.month = month;
  badge.day = dayRemaining + 1;
}
