/*
  WiFi Web Server LED Blink

  A simple web server that lets you blink an LED via the web.
  This sketch will print the IP address of your WiFi module (once connected)
  to the Serial Monitor. From there, you can open that address in a web browser
  to turn on and off the LED_BUILTIN.

  If the IP address of your board is yourAddress:
  http://yourAddress/H turns the LED on
  http://yourAddress/L turns it off

  This example is written for a network using WPA encryption. For
  WEP or WPA, change the WiFi.begin() call accordingly.

  Circuit:
  * Board with NINA module (Arduino MKR WiFi 1010, MKR VIDOR 4000 and Uno WiFi Rev.2)
  * LED attached to pin 9

  created 25 Nov 2012
  by Tom Igoe

  Find the full UNO R4 WiFi Network documentation here:
  https://docs.arduino.cc/tutorials/uno-r4-wifi/wifi-examples#simple-webserver
 */

#include "WiFiS3.h"

#include "arduino_secrets.h" 
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)

int led =  LED_BUILTIN;
int status = WL_IDLE_STATUS;
WiFiServer server(80);

void setup() {
  Serial.begin(9600);      // initialize serial communication
  pinMode(led, OUTPUT);      // set the LED pin mode

  Serial.println(SECRET_SSID);
  Serial.println(SECRET_PASS);

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);                   // print the network name (SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  server.begin();                           // start the web server on port 80
  printWifiStatus();                        // you're connected now, so print out the status
}


void loop() {
  WiFiClient client = server.available();   // listen for incoming clients
  if (!client) { return; }

  // First: Get the request type (is it POST, GET, or OPTIONS?)

  String request = "";
  // Reads in all request-related data (E.X. POST /H)
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      request += c;
      if (c == '\n') break;
    }
  }

  Serial.print("Request: ");
  Serial.println(request);

  if (request.startsWith("OPTIONS")){
    handleOptions(client);
  }
  if (request.startsWith("POST")) {

    // If POST request, need to read in the body:
    // @Ryan: see if you can search up how to read POST body for Arduino server
    //        then, you just have to do the client.println(...) stuff to send all the necessary data

    Serial.println("RECEIVED POST REQUEST HAHAHA...");

      int contentLength = 0;

    while (client.available()) {
      String headerLine = client.readStringUntil('\n');
      headerLine.trim();

      if (headerLine.startsWith("Content-Length:")) {
        contentLength = headerLine.substring(strlen("Content-Length:")).toInt();
      }

      if (headerLine.length() == 0) break; // end of headers
    }

    Serial.print("Content-Length: ");
    Serial.println(contentLength);

    String body = "";
    for (int i = 0; i < contentLength; i++) {
      while (!client.available());  // wait for data
      body += (char)client.read();
    }

    Serial.println("POST body received:");
    Serial.println(body);


    //handle body post body should look something like {"led": "on"}
    if (body.indexOf("on") >= 0) {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("LED ON");
    }
    else if (body.indexOf("off") >= 0) {
      digitalWrite(LED_BUILTIN, LOW);
      Serial.println("LED OFF");
    }
    else {
      Serial.println("Invalid LED command");
    }


    // Then: need to send CORS

    client.println("HTTP/1.1 200 OK");
    sendCORS(client);
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"status\":\"ok\"}");

  }
  if (request.startsWith("GET")){
    if (request.endsWith("/H")){
      digitalWrite(LED_BUILTIN, HIGH);               // GET /H turns the LED on

    }
    else{
      digitalWrite(LED_BUILTIN, LOW);               // GET /H turns the LED on
    }

    // Send data back to client
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    sendCORS(client);
    client.println();

    // the content of the HTTP response follows the header:
    client.print("<p style=\"font-size:7vw;\">Click <a href=\"/H\">here</a> turn the LED on<br></p>");
    client.print("<p style=\"font-size:7vw;\">Click <a href=\"/L\">here</a> turn the LED off<br></p>");
    
    // The HTTP response ends with another blank line:
    client.println();
  }

  client.stop();
  Serial.println("Client has disconnected");


  // if (client) {                             // if you get a client,
  //   Serial.println("new client");           // print a message out the serial port
  //   String currentLine = "";                // make a String to hold incoming data from the client
  //   while (client.connected()) {            // loop while the client's connected
  //     if (client.available()) {             // if there's bytes to read from the client,
  //       char c = client.read();             // read a byte, then
  //       Serial.write(c);                    // print it out to the serial monitor
  //       if (c == '\n') {                    // if the byte is a newline character

  //         // if the current line is blank, you got two newline characters in a row.
  //         // that's the end of the client HTTP request, so send a response:
  //         if (currentLine.length() == 0) {
  //           // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  //           // and a content-type so the client knows what's coming, then a blank line:
  //           client.println("HTTP/1.1 200 OK");
  //           client.println("Content-type:text/html");
  //           client.println("Access-Control-Allow-Origin: *"); // Allow requests from any origin
  //           client.println();

  //           // the content of the HTTP response follows the header:
  //           client.print("<p style=\"font-size:7vw;\">Click <a href=\"/H\">here</a> turn the LED on<br></p>");
  //           client.print("<p style=\"font-size:7vw;\">Click <a href=\"/L\">here</a> turn the LED off<br></p>");
            
            
  //           // The HTTP response ends with another blank line:
  //           client.println();
  //           // break out of the while loop:
  //           break;
  //         } else {    // if you got a newline, then clear currentLine:
  //           currentLine = "";
  //         }
  //       } else if (c != '\r') {  // if you got anything else but a carriage return character,
  //         currentLine += c;      // add it to the end of the currentLine
  //       }

  //       // Check to see if the client request was "GET /H" or "GET /L":
  //       if (currentLine.endsWith("GET /H")) {
  //         digitalWrite(LED_BUILTIN, HIGH);               // GET /H turns the LED on
  //       }
  //       if (currentLine.endsWith("GET /L")) {
  //         digitalWrite(LED_BUILTIN, LOW);                // GET /L turns the LED off
  //       }
  //       if (currentLine.endsWith("GET /api/ping")){
  //         Serial.println("Made GET request to ping server");
  //         // Discard the body for now
  //       }
  //     }
      
  //   }
  //   // close the connection:
  //   client.stop();
  //   Serial.println("client disconnected");
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}

void handleOptions(WiFiClient client){
  client.println("HTTP/1.1 204 No Content");
  sendCORS(client);
  client.println("Connection: close");
  client.println();
  client.stop();
}

void handlePost(){

}

void sendCORS(WiFiClient &client) {
  client.println("Access-Control-Allow-Origin: *");
  client.println("Access-Control-Allow-Methods: GET, POST, OPTIONS");
  client.println("Access-Control-Allow-Headers: Content-Type");
  client.println("Access-Control-Max-Age: 3600");
}
