// DRAGRAKE
// Written by Bennett Diamond and Christian BLoemhof, webpage code from Rui Santos
// Collects pressure transducer data via i2c, processes data to make airspeed readings, creates own wifi network 
// with Wifi.softAP(), hosts webpage with live airspeed data and changes sensor read type


// Libraries ---------------------------------------------------------------------------------------------------
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h> // doesn't seem to be needed
#include <WiFiClient.h> // doesn't seem to be needed
#include <WiFiServer.h> // doesn't seem to be needed

// Variables ---------------------------------------------------------------------------------------------------

// i2c stuff 
int Amphenol_Address = 0x29; // sensor address
#define P_SENSE_BYTES 7 // number of bytes coming from pressure sensor
int dataBuffer[P_SENSE_BYTES];
char msg[128]; // size of message 
const switch_pin[2] = [7, 8]; // these pins are the GPIO pins that switches between which sensor is being read
bool sensor1 = TRUE; // used for switchPin, TRUE if using sensor 1, 

// Command Registers (i2c commands for the pressure transducer)
#define Start_Single 0xAA // Hexadecimal address for single read
#define Start_Average2 0xAC // Hexadecimal address for average of 2 reads
#define Start_Average4 0xAD // Hexadecimal address for average of 4 reads
#define Start_Average8 0xAE // Hexadecimal address for average of 8 reads
#define Start_Average16 0xAF // Hexadecimal address for average of 16 reads

// array of all commands to simplify changing modes (i2c stuff)
int start_comm[5] = {Start_Single, Start_Average2, Start_Average4, Start_Average8, Start_Average16}; 
int st_mode = 1; // command mode selector

// array of calibration registers 
int cal_reg[10] = {47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57};


// WiFi Stuff

// Network credentials (password optional)
const char* ssid = "DragRake";
// const char* password = "1234";

// Set web server port number to 80 (does this need to be changed?)
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// the IP address for the softAP:
IPAddress ip(69, 420, 69, 420); // nice    


// Setup -------------------------------------------------------------------------------------------------------
void setup() {

// Starting i2c 
//   Wire.begin(); // starts i2c
//   pinMode(switch_pin[1], OUTPUT);           // set switch pins to outputs
//   pinMode(switch_pin[2], OUTPUT);

//   for (int i = 0; i < 2; i++){ // pulling calibration values from both pressure sensors
//      // Sensor Calibration Coefficients -----------------------------------------------------------------------
//      for (int i = 0; i < 9; i++) {
//        Wire.requestFrom(cal_reg[i],1);
        
//        while(Wire.available()) {
//          int cal_val[i] = Wire.read(); 
//        }
//      }
//          pinSwitch(sensor1); // switch pins to get calibration data for other sensor
//   }
  
// Starting network - Rui Santos 

  Serial.begin(115200); // change baud rate?

  // Create Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Removed the password parameter, so the AP is open
  WiFi.softAP(ssid);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();

  
}

// Loop --------------------------------------------------------------------------------------------------------
void loop() {

// Commanding and reading the sensor
//     Wire.beginTransmission(Amphenol_Address); // Begin transmission to the Sensor 
//     //Ask the particular registers for data
//     Wire.write(1); // number of bytes?
//     Wire.endTransmission(); // Ends the transmission and transmits the data from the register
//     Wire.requestFrom(start_comm[st_mode], P_SENSE_BYTES); // Read 7 bytes from sensor
    
//     if (Wire.available()<=P_SENSE_BYTES) {  // reads data if wire is available  - check if correct number
//         for (int i = 0; i < P_SENSE_BYTES; i++) {
//             dataBuffer[i] = Wire.read(); // Reads the data from the register
//     }
//   }

// Airspeed Calcs with acquired sensor data
// outputs will be float top_diff_airspeed and float bottom_diff_airspeed

// Publishing data to website - Rui Santos

WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // The following example code turns the GPIOs on and off, we want this to display and 
            // change the sensor read type: single read, 2 reads, 4 reads, 8 reads, or 16 reads
            // we also want the code to display the airspeed parameters (top and bottom diff_airspeed)
            if (header.indexOf("GET /26/on") >= 0) {
              Serial.println("GPIO 26 on");
              output26State = "on";
              digitalWrite(output26, HIGH);
            } else if (header.indexOf("GET /26/off") >= 0) {
              Serial.println("GPIO 26 off");
              output26State = "off";
              digitalWrite(output26, LOW);
            } else if (header.indexOf("GET /27/on") >= 0) {
              Serial.println("GPIO 27 on");
              output27State = "on";
              digitalWrite(output27, HIGH);
            } else if (header.indexOf("GET /27/off") >= 0) {
              Serial.println("GPIO 27 off");
              output27State = "off";
              digitalWrite(output27, LOW);
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");

            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>Drag Rake</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 26, this is where some of the button stuff
            // should be defined  
            client.println("<p>Sensor measurement mode - State " + output26State + "</p>");
            // If the output26State is off, it displays the ON button       
            if (output26State=="off") {
              client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 
               
            // Display current state, and ON/OFF buttons for GPIO 27  
            client.println("<p>GPIO 27 - State " + output27State + "</p>");
            // If the output27State is off, it displays the ON button       
            if (output27State=="off") {
              client.println("<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");    
  }
}

// Embedded Functions ------------------------------------------------------------------------------------------

bool pinSwitch(sensor1) {
  // Switches which pressure sensor is being read/commanded by "jamming" the other sensor by writing
  // high to SDA on the inactive sensor
  if (sensor1 == TRUE) { // reading from sensor 1
    digitalWrite(switch_pin[1],HIGH); // disables sensor 1, enables sensor 2
    digitalWrite(switch_pin[2],LOW);
    sensor1 = FALSE; // switches sensor1 value, sensor 2 is now being read from
    
    else {
      digitalWrite(switch_pin[2],HIGH);
      digitalWrite(switch_pin[1],LOW);
      sensor1 = TRUE; // sensor 1 now being read from
    }
    return sensor1
  }
}
