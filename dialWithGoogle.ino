// acessing doGet function in a google sheet
// https://script.google.com/macros/s/AKfycbycR7N4a3pIuYFCfjR3Ys_wp7yAUb2-M6okvlhYkhzTHD6cOGaKUMcyG9MAiwltS400RQ/exec?node=test&action=getBaseInfo
// AKfycbycR7N4a3pIuYFCfjR3Ys_wp7yAUb2-M6okvlhYkhzTHD6cOGaKUMcyG9MAiwltS400RQ

#define SHEET_SERVER  "script.google.com"
#define SHEET_UID     "AKfycbycR7N4a3pIuYFCfjR3Ys_wp7yAUb2-M6okvlhYkhzTHD6cOGaKUMcyG9MAiwltS400RQ"
#define SHEET_URI     "https://" SHEET_SERVER "/macros/s/" SHEET_UID "/exec"



// croquis utilise 422860 octets (40%) de l'espace de stockage de programmes. Le maximum est de 1044464 octets.
//Les variables globales utilisent 29280 octets (35%) de mémoire dynamique, ce qui laisse 52640 octets pour les variables locales. Le maximum est de 81920 octets.



bool dialWithGoogle(const String aNode, const String aAction, JSONVar &jsonData) {

  Serial.print(F("Dial With Google "));
  Serial.print(aNode);
  Serial.print(':');
  Serial.println(aAction);
  String aUri = F(SHEET_URI);
  aUri += F("?node=");
  aUri += aNode;
  aUri += F("&action=");
  aUri += aAction;

  //D_println(aUri);

  WiFiClientSecure client;
  HTTPClient http;  //Declare an object of class HTTPClient
  // !!! TODO get a set of valid root certificate for google !!!!
  client.setInsecure(); //the magic line, use with caution  !!! certificate not checked
  //client.connect(HTTP_SERVER, 443);   not needed

  http.begin(client, aUri); //Specify request destination

  // define requested header
  const char * headerKeys[] = {"location"} ;
  const size_t numberOfHeaders = 1;
  http.collectHeaders(headerKeys, numberOfHeaders);

  int httpCode = http.GET();//Send the request
  int antiLoop = 0;
  while (httpCode == 302 && antiLoop++ < 3) {
    String newLocation = http.header(headerKeys[0]);
    // google will give answer in relocation
    D_println(newLocation);
    http.begin(client, newLocation); //Specify request new destination
    http.collectHeaders(headerKeys, numberOfHeaders);
    httpCode = http.GET();//Send the request
  }
  if (httpCode < 0) {
    Serial.print(F("cant get an answer :( http.GET()="));
    Serial.println(httpCode);
    http.end();   //Close connection
    return (false);
  }

  if (httpCode != 200) {
    Serial.print(F("got an error in answer :( http.GET()="));
    Serial.println(httpCode);
    http.end();   //Close connection
    return (false);
  }
  {
    String payload = http.getString();   //Get the request response payload
    http.end();   //Close connection
    D_println(payload);             //Print the response payload
    jsonData = JSON.parse(payload);
  } // free payload (can be hudge in memory)
  
  if (JSON.typeof(jsonData) != F("object")) return (false);

  // super check json data for "status" is a bool true  to avoid foolish data then supose all json data are ok.
  if (!jsonData.hasOwnProperty("status") || JSON.typeof(jsonData["status"]) != F("boolean") || !jsonData["status"]) return (false);

  D_println( niceDisplayTime(jsonData["timestamp"]) );
  JSONVar answer = jsonData["answer"];  // cant grab object from the same object
  jsonData = answer;                    // so memory use is temporary duplicated here
  return (true);



}
