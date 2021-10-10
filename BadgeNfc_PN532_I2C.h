/*************************************************
 *************************************************
 **  BadgeNfc_PN532_I2C   Librairie de gestion d'un lecteur de badge NFC avec un lecteur PN532 sous I2C
 **
 **  Isolement des librairies  <PN532_I2C.h> <PN532.h> <NfcAdapter.h>
 **
 **  V1.0 P.HENRY  29/02/2020
 **  Isolement du code
 **  V1.1 P.HENRY  10/03/2020
 **  Ajout des parametres retry timeout au begin
 **  nettoyage des serial print en biblio NFC
 **  Ajout de powerDownMode()
 *************************************************/
#pragma once


#include "Arduino.h"
//#define  VERBOSE 


//const int  NFC_TIMEOUT = 40;  // 30 < 10' de sec environ   300 environ 8 sec  mini 15 sinon cela ne marceh pas !!!!!
// retry 50  timeout 60 pour les anciens badges
const   byte NDEFTextMAX  = 150;




class BadgeNfc_PN532_I2C
{
  public:
    BadgeNfc_PN532_I2C() {};
    bool begin(const byte sizebuffer = NDEFTextMAX,const byte retry = 1,const int timeout=20);
    bool badgePresent();
    bool readBadge();
    bool writeBadge(String &aText);
    bool formatBadge();
    bool powerDownMode();
    String getUUIDTag();
    byte UUIDTag[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; // Le premier char est la taille (4 ou 7)
    char   NDEFType;    // - ou T ou U ou +  (Vide ou Text ou URI ou message trop long);
    String NDEFText;    // Texte du premier record NFC si celui ci est un TXT ou une URI
    private:
    byte _sizeBuffer = NDEFTextMAX;  // taille maxi de la chaine texte a recuperer
    int  _timeout;    // timeout pour la lecture
  private:

};
