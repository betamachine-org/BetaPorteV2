//divers functions pour betaconsole


// Imprime une ligne sur LCD reformatée a 20 char avec neutralisation des char accentués
void lcdprint20(String const astr) {
  String aStr = astr;
  aStr.replace("é", "e");
  aStr.replace("è", "e");
  aStr.replace("à", "a");
  lcd.print('\x03'); //clreol
  lcd.print(aStr);
}
