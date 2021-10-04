// acessing doGet function in a google sheet
// https://script.google.com/macros/s/AKfycbycR7N4a3pIuYFCfjR3Ys_wp7yAUb2-M6okvlhYkhzTHD6cOGaKUMcyG9MAiwltS400RQ/exec?node=test&action=getBaseInfo
// AKfycbycR7N4a3pIuYFCfjR3Ys_wp7yAUb2-M6okvlhYkhzTHD6cOGaKUMcyG9MAiwltS400RQ

#define SHEET_SERVER  "script.google.com"
#define SHEET_UID     "AKfycbycR7N4a3pIuYFCfjR3Ys_wp7yAUb2-M6okvlhYkhzTHD6cOGaKUMcyG9MAiwltS400RQ"
#define SHEET_URI     "https://" SHEET_SERVER "/macros/s/" SHEET_UID "/exec"






bool dialWithGoogle(const String aNode, const String aAction, JSONVar &jsonData) {

  Serial.print("Dial With Google ");
  Serial.print(aNode);
  Serial.print(':');
  Serial.print(aAction);
  String aUri = SHEET_URI;
  aUri += "?node=";
  aUri += aNode;
  aUri += "&action=";
  aUri += aAction;

  Serial.println(F("connect to " SHEET_SERVER " to do action"));
  D_println(aUri);

  WiFiClientSecure client;
  HTTPClient http;  //Declare an object of class HTTPClient
  // !!! TODO get a set of valid root certificate for google !!!!
  client.setInsecure(); //the magic line, use with caution  !!! certificate not checked
  //client.connect(HTTP_SERVER, 443);   not needed

  http.begin(client, aUri); //Specify request destination

  // define requested header
  const char * headerKeys[] = {"date", "location"} ;
  const size_t numberOfHeaders = 2;
  http.collectHeaders(headerKeys, numberOfHeaders);

  int httpCode = http.GET();//Send the request
  int antiLoop = 0;
  while (httpCode == 302 && antiLoop++ < 3) {
    String newLocation = http.header(headerKeys[1]);
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

  String payload = http.getString();   //Get the request response payload
  http.end();   //Close connection
  Serial.println(payload);             //Print the response payload
  JSONVar myObject = JSON.parse(payload);
  Serial.print("JSON.typeof(myObject) = ");
  Serial.println(JSON.typeof(myObject)); // prints: object
  if (myObject.hasOwnProperty("status")) {

    Serial.print("JSON.typeof(myObject[\"status\"]) = ");
    Serial.println( JSON.typeof(myObject["status"]) );
    Serial.print("myObject[\"status\"] = ");
    Serial.println( myObject["status"]);
    
  }
  D_println( JSON.typeof(myObject["answer"]) );
  D_println( myObject["answer"]);
  D_println( niceDisplayTime(myObject["timestamp"]) );
  jsonData = myObject["answer"];
  bool result = payload.startsWith(F("{\"status\":true,"));
  return (true);



}
