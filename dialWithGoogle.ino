// acessing doGet function in a google sheet
// https://script.google.com/macros/s/google_sheet_key/exec?node=node_name&action=action_keyword




#define SHEET_SERVER  "script.google.com"
// need about 30K of ram !!!!!! WiFiClientSecure
// todo return better errcode
bool dialWithGoogle(const String& aNode, const String& aAction, String& jsonParam ) {
  // (global) HTTPClient http;  //Declare an object of class HTTPClient
  Serial.print(F("Dial With gSheet as '"));
  Serial.print(aNode);
  Serial.print(':');
  Serial.print(aAction);
  Serial.println('\'');
  D_println(helperFreeRam() + 000);
  if (helperFreeRam() < 41000) {
    Serial.println(F("https need more memory"));
    return (false);
  }
  String bigString;
  //bigString.reserve(1000);
  bigString = F("https://" SHEET_SERVER "/macros/s/");

  if (aNode.length() == 0 || aAction.length() == 0) return (false);
  //bigString = F("https://" SHEET_SERVER "/macros/s/");
  bigString += jobGetConfigStr(F("gkey"));

  bigString += F("/exec?node=");
  bigString += encodeUri(aNode);;

  bigString += F("&action=");
  bigString += encodeUri(aAction);;

  if ( jsonParam.length() > 0 ) {
    //D_println(jsonParam);
    bigString += F("&json=");
    bigString += encodeUri(jsonParam);
  }
  D_println(bigString.length());
  //D_println(bigString);
  //D_println(helperFreeRam() + 0001);
  {
    WiFiClientSecure wifiSecure;   // 7K
    //D_println(helperFreeRam() + 0002);

    HTTPClient http;  //Declare an object of class HTTPClient (Gsheet and webclock)
    http.setTimeout(15000); // 10 Seconds   (could be long with google)
    //  HTTPClient http;  //Declare an object of class HTTPClient
    // !!! TODO get a set of valid root certificate for google !!!!
    wifiSecure.setInsecure(); //the magic line, use with caution  !!! certificate not checked
    http.begin(wifiSecure, bigString); //Specify request destination
    // define requested header
    const char * headerKeys[] = {"location"} ;
    const size_t numberOfHeaders = 1;
    http.collectHeaders(headerKeys, numberOfHeaders);
    //D_println(helperFreeRam() + 01);
    int httpCode = http.GET();//Send the request  (gram 22K of ram)

    /*** HTTP client errors
        #define HTTPCLIENT_DEFAULT_TCP_TIMEOUT (5000)
      #define HTTPC_ERROR_CONNECTION_FAILED   (-1)
      #define HTTPC_ERROR_SEND_HEADER_FAILED  (-2)
      #define HTTPC_ERROR_SEND_PAYLOAD_FAILED (-3)
      #define HTTPC_ERROR_NOT_CONNECTED       (-4)
      #define HTTPC_ERROR_CONNECTION_LOST     (-5)
      #define HTTPC_ERROR_NO_STREAM           (-6)
      #define HTTPC_ERROR_NO_HTTP_SERVER      (-7)
      #define HTTPC_ERROR_TOO_LESS_RAM        (-8)
      #define HTTPC_ERROR_ENCODING            (-9)
      #define HTTPC_ERROR_STREAM_WRITE        (-10)
      #define HTTPC_ERROR_READ_TIMEOUT        (-11)
    */

    D_println(httpCode);
    D_println(helperFreeRam() + ( 0 * 02));
    uint8_t antiLoop = 0;
    while (httpCode == 302 && antiLoop++ < 3) {
      // google will give answer in relocation
      //D_println(helperFreeRam() + (0 * 31) );
      bigString = http.header(headerKeys[0]);
      http.getString();   //Get the request response payload
      http.end();   //Close connection (got err -7 if not)
      //D_println(helperFreeRam() + (0 * 32) );
      D_println(bigString.length());
      http.begin(wifiSecure, bigString); //Specify request new destination
      http.collectHeaders(headerKeys, numberOfHeaders);
      httpCode = http.GET();//Send the request
      //D_println(httpCode);
    }
    //  bigString = "";
    //    if (httpCode < 0) {
    //      Serial.print(F("cant get an answer :( http.GET()="));
    //      Serial.println(httpCode);
    //      http.end();   //Close connection
    //      return (false);
    //    }

    if (httpCode != 200) {
      Serial.print(F("got an error in http.GET() "));
      D_println(httpCode);
      http.end();   //Close connection
      return (false);
    }

    //D_println(helperFreeRam() + (0 * 04));
    bigString = http.getString();   //Get the request response payload
    //D_println(helperFreeRam() + (0 * 05));
    http.end();   //Close connection (restore 22K of ram)
  }

  D_println(helperFreeRam() + (0 * 06))
  D_println(bigString.length());             //Print the response payload
  //D_println(bigString);
  // check json string without real json lib  not realy good but use less memory and faster
  int16_t answerPos = bigString.indexOf(F(",\"answer\":{"));
  if ( !bigString.startsWith(F("{\"status\":true,")) || answerPos < 0 ) {
    return (false);
  }
  // hard cut of "answer":{ xxxxxx } //
  jsonParam = bigString.substring(answerPos + 10, bigString.length() - 1);
  D_println(jsonParam.length());
  D_println(helperFreeRam() + 001);
  return (true);
}

#define Hex2Char(X) (char)((X) + ((X) <= 9 ? '0' : ('A' - 10)))

// encode optimisé pour le json
String encodeUri(const String aUri) {
  String answer = "";
  String specialChar = F(".-~_{}[],;:\"\\");  // add  !=- ???
  // uri encode maison :)
  for (int N = 0; N < aUri.length(); N++) {
    char aChar = aUri[N];
    //TODO:  should I keep " " to "+" conversion ????  save 2 char but oldy
    if (aChar == ' ') {
      answer += '+';
    } else if ( isAlphaNumeric(aChar) ) {
      answer +=  aChar;
    } else if (specialChar.indexOf(aChar) >= 0) {
      answer +=  aChar;
    } else {
      answer +=  '%';
      answer += Hex2Char( aChar >> 4 );
      answer += Hex2Char( aChar & 0xF);
    } // if alpha
  }
  return (answer);
}
