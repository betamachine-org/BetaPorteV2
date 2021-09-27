/*************************************************
 *************************************************
 **  BadgeNfc_PN532_I2C   Librairie de gestion d'un lecteur de badge NFC avec un lecteur PN532 sous I2C
 **
 **  Isolement des librairies  <PN532_I2C.h> <PN532.h> <NfcAdapter.h>
 **
 **  V1.0 P.HENRY  29/03/2020
 **  Isolement du code
 *************************************************/
#include "BadgeNfc_PN532_I2C.h"
#define D_println(x) Serial.print(F(#x " => '")); Serial.print(x); Serial.println("'");

//#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>


//Object Static Necessaire
PN532_I2C   InterfacePn532i2c(Wire);  // PN532 interface
PN532       LePN532(InterfacePn532i2c);     // pour lire plus finement la presence badge
NfcAdapter  LeTraducteurNFC = NfcAdapter(InterfacePn532i2c);  // pour lire les tag NFC plus facilement

bool  BadgeNfc_PN532_I2C::begin(const byte sizebuffer, const byte retry, const int timeout ) {
  LePN532.begin();

  uint32_t versiondata = LePN532.getFirmwareVersion();
  if (versiondata == 0) {
#ifdef  VERBOSE
    Serial.println(F("no NFC chip"));
#endif
    return (false);
  }
#ifdef  VERBOSE
  Serial.print(F("Found NFC chip PN5"));  Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print(F("Version Data "));  Serial.println(versiondata, HEX);
  Serial.print(F("Firmware ver. ")); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);
#endif
  _timeout = timeout;
  //const int  NFC_TIMEOUT = 40;
  // 30 donne  100ms  environ
  // 300 environ 8 sec
  // 15 au minimum mini 15 sinon cela ne marceh pas !!!!!


  _sizeBuffer = sizebuffer;
  // size of buffer is the max size accepted
  // be carefull NDEF need around 300byte + 2xSizebuffer to survive so it is much for nano
  NDEFText.reserve(_sizeBuffer);  // avoid heap fragmentation but lock memory
  // the default behaviour of the PN532.
  LePN532.setPassiveActivationRetries(retry);
  // configure board to read RFID tags
  LePN532.SAMConfig();  // return false
#ifdef  VERBOSE
  LeTraducteurNFC.begin(true);
#else
  LeTraducteurNFC.begin(false);
#endif
  return (true);
}

// turn Radio Module to low power
// turn Radio Module to low power  return vrai si le PN est present
bool BadgeNfc_PN532_I2C::powerDownMode() {

  //Serial.print(LePN532.readGPIO());
  bool result = ( LePN532.getFirmwareVersion() != 0 );
  //  if (!result) {
  //    delay (100);
  //    result = ( LePN532.getFirmwareVersion() !=0 );
  //  }
  LePN532.powerDownMode();  // return always false
  return (result);
}

// true if badge is present or if the badge present is still the same.
// if UID change it's return false then on next call it will answer true;
bool BadgeNfc_PN532_I2C::badgePresent() {
  static bool wasOk = false;
  // uint8_t currentKeyLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  uint8_t key[] = { 0, 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID

  // String sKeyCode;

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  //bool  result = LePN532.readPassiveTargetID(PN532_MIFARE_ISO14443A, &key[0], &currentKeyLength,100);
  // sur certain badge la deuxieme  lecture ne passe pas  !!!!
  
  bool  isOk = LePN532.readPassiveTargetID(PN532_MIFARE_ISO14443A, &key[1], &key[0], _timeout);
  //Serial.print("NFC "); Serial.print(_timeout); Serial.print(" "); 
  //Serial.print((byte)key[0], HEX); Serial.print(" "); Serial.print((byte)key[1], HEX); Serial.print(" "); Serial.println((byte)key[2], HEX);
  if (key[0] != 7 && key[0] != 4) isOk = false;
  // verification de l'UUID
  if (wasOk && isOk) {
    for (int N = 0; (N <= (byte)key[0]) && isOk; N++) {
     // Serial.print((byte)key[N], HEX);
      isOk &= ((byte)key[N] == (byte)UUIDTag[N]);  // force un false si l'uuid a changé
    }
    // Serial.println();
  }
  if (!wasOk && isOk) {
    for (int N = 0; N < 8 ; N++) {
      UUIDTag[N] = key[N];  // recopie de l'UUId et de sa taille
    }
  }

  wasOk = isOk;
  return (isOk);
}

#define Hex2Char(X) (char)((X) + ((X) <= 9 ? '0' : ('A' - 10)))

String BadgeNfc_PN532_I2C::getUUIDTag() {
  String result = "";
  for (int N = 1; N <= UUIDTag[0]; N++) {
    result += Hex2Char( UUIDTag[N] >> 4);
    result += Hex2Char( UUIDTag[N] & 0xF);
  
   }
   return (result);
}

// Lecture du message NFC si un badge a ete detecté.
// Attention a la memoire : 300 + 2xBuffersize
bool  BadgeNfc_PN532_I2C::readBadge() {
  NDEFText = "";
  NDEFType = '-';
  if ( !LeTraducteurNFC.tagPresent(_timeout)) {
    return (false);
  };  // Eject pas de badge

  NfcTag tag = LeTraducteurNFC.read();
  if ( !tag.hasNdefMessage()) {
    return (false);
  };  // Eject le badge n'as pas de record NFC

  // Lecture du premier record NFC
  NdefMessage message = tag.getNdefMessage();
  NdefRecord record = message[0];

  // Only TEXT or URI Records
  record.getType((byte*)&NDEFType);
  if ( (NDEFType != 'T') && (NDEFType != 'U') )  {
    //  Serial.print(F("Type="));
    //      Serial.println(NDEFType);
    NDEFType = '?';    // type inconnu
    return (false);                                      // Eject le message n'est pas du bon type
  }

  // Grab Message
  // Check if message is not too long
  int  payloadLength = record.getPayloadLength();
  if ( payloadLength > _sizeBuffer ) {
    NDEFType = '+';  // Message Trop Grand
    return (false);
  };  // Eject le message est trop long

  // Get the message
  // Serial.println(payloadLength);
  byte payload[payloadLength];
  record.getPayload(payload);
  // le premier octet a un usage special : le codage
  // voir https://blog.zenika.com/2012/04/24/nfc-iv-les-types-de-messages-du-nfc-forum-wkt/
  byte    offsetPayload = 0;
  // NDEFText = "";
  if (NDEFType == 'T') {
    // pour un text cela encode le type d'encodage ascii utilisé (je m'en debarasse )
    if ( payload[0] > 10)  {
      return (true); // pas d'encodage ascii ?
    };  // Eject Codage du payload incorrect
    offsetPayload = payload[0] + 1;
  }
  if (NDEFType == 'U') {
    // pour une URI c'est une entete a ajouter (je supose pas d'entete mais je retourne le n° d'entete si il y en a un)
    //        Serial.println(payload[0]);
    switch (payload[0]) {
      case 0:  //NDEFText = "";  // pas d'entete
        break;
      case 1:  //NDEFText F("http://www.");
      case 2:  // NDEFText = F("https://www.");
      case 3: // NDEFText = F("http://");
      case 4: // NDEFText = F("https://");
        NDEFText = (char)(payload[0] + 0x30);
        break;
      default:  return (false);                                      // Eject le message n'est pas du bon type
    }
    offsetPayload = 1;
  }
  //  if (payloadLength - offsetPayload + NDEFText.length() > _sizeBuffer) {
  //    return (false);
  //  };  // Eject le message est trop long

  for (int c = offsetPayload; c < payloadLength; c++) {
    NDEFText += (char)payload[c];
  }
  return (true);
}

// Ecriture d'un badge
bool BadgeNfc_PN532_I2C::writeBadge(String &aString) {
  if ( !LeTraducteurNFC.tagPresent(_timeout)) {
    return (false);
  };  // Eject pas de badge
  NdefMessage message = NdefMessage();
  if (aString.startsWith(F("http"))) { // || aString.startsWith("https://")) {
    message.addUriRecord(aString);
  } else {
    message.addTextRecord(aString);
  }

  return (LeTraducteurNFC.write(message));
}

// Formatage d'un badge
bool BadgeNfc_PN532_I2C::formatBadge() {
  if ( !LeTraducteurNFC.tagPresent(_timeout)) {
    return (false);
  };  // Eject pas de badge
  bool  result = false;
  LeTraducteurNFC.clean();       //reset tag
  result = LeTraducteurNFC.format();
  return (result);
}
