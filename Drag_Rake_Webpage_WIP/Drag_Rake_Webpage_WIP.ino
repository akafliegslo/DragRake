// DRAGRAKE
// Written by Bennett Diamond and Christian BLoemhof, webpage code from Rui Santos
// Collects pressure transducer data via i2c, processes data to make airspeed readings, creates own wifi network
// with Wifi.softAP(), hosts webpage with live airspeed data and changes sensor read type


// Libraries ---------------------------------------------------------------------------------------------------
#include <Wire.h>
#include <SPI.h>
#include <WiFi101.h>
char ssid[] = "DragRake";
char pass[] = "";                // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                // your network key Index number (needed only for WEP)
int status = WL_IDLE_STATUS;
// Variables ---------------------------------------------------------------------------------------------------

// i2c stuff
int Amphenol_Address = 0x29; // sensor address
#define P_SENSE_BYTES 7 // number of bytes coming from pressure sensor
int dataBuffer[P_SENSE_BYTES];
char msg[128]; // size of message
int switch_pin[2] = {9, 10}; // these pins are the GPIO pins that switches between which sensor is being read
bool sensor1 = 1; // used for switchPin, TRUE if using sensor 1,

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
int cal_reg[11] = {47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57};




//Danny's Mess of trying to get things to work
String output26State = "off";
int output26 = 9;
String output27State = "off";
int output27 = 10;
String text[6];
String color[6];
int selected;


#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
// Required for Serial on Zero based boards
#define Serial SERIAL_PORT_USBVIRTUAL
#endif










// WiFi Stuff

// Network credentials (password optional)

// const char* password = "1234";

// Set web server port number to 80 (does this need to be changed?)
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// the IP address for the softAP:
//IPAddress ip(69, 420, 69, 420); // nice // This would be funny except that values can only go up to 255 lmao

int cal_val[8];
// Setup -------------------------------------------------------------------------------------------------------
void setup() {

  WiFi.setPins(8, 7, 4, 2);









  // Starting i2c
  Wire.begin(); // starts i2c
  pinMode(switch_pin[1], OUTPUT);           // set switch pins to outputs
  pinMode(switch_pin[2], OUTPUT);

  //  for (int i = 0; i < 2; i++)
  //  { // pulling calibration values from both pressure sensors
  //    // Sensor Calibration Coefficients -----------------------------------------------------------------------
  //    for (int b = 0; b < 9; b++) {
  //      Wire.requestFrom(cal_reg[i], 1);
  //
  //      while (Wire.available()) {
  //        cal_val[b] = Wire.read();
  //      }
  //    }
  //    //    pinSwitch(sensor1); // switch pins to get calibration data for other sensor
  //  }

  // Starting network - Rui Santos

  Serial.begin(9600); // change baud rate?
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  // Create Wi-Fi network with SSID and password
  Serial.print("Creating access point named: ");
  Serial.println(ssid);

  // by default the local IP address of will be 192.168.1.1
  // you can override it with the following:
  // WiFi.config(IPAddress(10, 0, 0, 1));
  status = WiFi.beginAP(ssid);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true);
  }
  delay(10000);


  server.begin();

  printWiFiStatus();
}

// Loop --------------------------------------------------------------------------------------------------------
void loop()
{


  //  if (Wire.available() <= P_SENSE_BYTES) { // reads data if wire is available  - check if correct number
  //    for (int i = 0; i < P_SENSE_BYTES; i++) {
  //      dataBuffer[i] = Wire.read(); // Reads the data from the register
  //    }
  //  }

  // Airspeed Calcs with acquired sensor data
  // outputs will be float top_diff_airspeed and float bottom_diff_airspeed

  // Publishing data to website - Rui Santos

  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {
    // If a new client connects,
    //    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";  // make a String to hold incoming data from the client
    int i = 0;

    while (client.connected()) {
      if (client.available()) {


        char c = client.read();
//        Serial.print(c);



        if ((75 < i) && (i < 78))
        {

          char q[2];
          q[i - 76] = c;
          if (q[0] == '1')
          {
            if (i == 77)
            {
              if (q[1] == '0')
              {
                selected = 3;
                Serial.println("4 AVG");
              }
              if (q[1] == '1')
              {
                selected = 4;
                Serial.println("8 AVG");
              }
              if (q[1] == '2')
              {
                selected = 5;
                Serial.println("16 AVG");
              }
              else
              {

              }
            }
            else
            {
              ;
            }
          }
          else if (q[0] == '5')
          {
            selected = 0;
            Serial.println("Off");
          }
          else if (q[0] == '8')
          {
            selected = 1;
            Serial.println("Single");
          }
          else if (q[0] == '9')
          {
            selected = 2;
            Serial.println("2 AVG");
          }
          else
          {
          }
        }

        else
        {
        }
        i++;





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

            for (int i = 0; i < 6; i++)
            {
              text[i] = "black";
              color[i] = "ddd}";
            }
            text[selected] = "white";
            color[selected] = "2196F3}";
//            Serial.print("Selected = ");
//            Serial.println(selected);
            client.println("<!DOCTYPE html ><html><head><title>Drag Rake Controls</title><meta name = \"viewport\" content = \"width = device - width, initial - scale = 1\">");
            client.println("<style>html { font - family: Helvetica; display: inline - block; margin: 0px auto; text - align: center;}");
            client.println(".btn {border: none;color: black;width: 128px; height: 64px;text - align: center;");
            client.println("font - size: 16px;margin: 4px 2px;transition: 0.3s;border - radius: 20px;}");
            client.println(".off {color:" + text[0] + ";background-color: #" + color[0]);
            client.println(".single {color:" + text[1] + ";background-color: #" + color[1]);
            client.println(".avg2 {color:" + text[2] + ";background-color: #" + color[2]);
            client.println(".avg4 {color:" + text[3] + ";background-color: #" + color[3]);
            client.println(".avg8 {color:" + text[4] + ";background-color: #" + color[4]);
            client.println(".avg16{color:" + text[5] + ";background-color: #" + color[5]);

            client.println("</style></head><body><h2>Sensor Reader Mode </h2>");
            client.println("<form method = \"post\">");
            client.println("<input class = \"btn off\" type = \"submit\" name = \"1\" id=\"Off\" value = \"Off\" onclick = \"submit()\">");
            client.println("<input class = \"btn single\" type = \"submit\" name = \"1\" id=\"Single\" value = \"Single\" onclick = \"submit()\">");
            client.println("<input class = \"btn avg2\" type = \"submit\" name = \"123\" id=\"2 AVG\" value = \"2 AVG\" onclick = \"submit()\">");
            client.println("<br>");
            client.println("<input class = \"btn avg4\" type = \"submit\" name = \"1234\" id=\"4 AVG\" value = \"4 AVG\" onclick = \"submit()\">");
            client.println("<input class = \"btn avg8\" type = \"submit\" name = \"13245\" id=\"8 AVG\" value = \"8 AVG\" onclick = \"submit()\">");
            client.println("<input class = \"btn avg16\" type = \"submit\" name = \"12345\" id=\"16 AVG\" value = \"16 AVG\" onclick = \"submit()\">");
            /*
               The text[] and color[] arrays simply hold a string telling the buttons what color
               to have the background and text. This is so that I can easily set them all to the
               same color, then only set one of them to be highlighted.

               Now would be a good time to explain what all this stuff is (specifically the form).
               The arduino receives a response from the computer each time a button is pressed,
               in the form of name=value. So, for "Off", this would be "1=Off" which is 5 characters long.
               I couldn't get the Arduino to tell me what the actual message was (I was planning to
               parse the message to see if it was "Off" "Single" etc), but it will tell me the length
               of the message. So, I made all the buttons send back a different length message so that
               I can tell which one is being pressed.

               What can I say, I was desperate.
            */
            client.println("</form></body></html>");




            //OLD HTML (Hopefully new html looks nice)


            //            // Display the HTML web page
            //            client.println(" < !DOCTYPE html > <html>");
            //            client.println("<head> < meta name = \"viewport\" content=\"width=device-width, initial-scale=1\">");
            //                        client.println("<link rel=\"icon\" href=\"data:,\">");
            //
            //            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            //            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            //            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            //            client.println(".button2 {background-color: #555555;}</style></head>");
            //
            //            // Web Page Heading
            //            client.println("<body><h1>Drag Rake</h1>");
            //
            //            // Display current state, and ON/OFF buttons for GPIO 26, this is where some of the button stuff
            //            // should be defined
            //            client.println("<p>Sensor measurement mode - State " + output26State + "</p>");
            //            // If the output26State is off, it displays the ON button
            //            if (output26State == "off") {
            //              client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
            //            } else {
            //              client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
            //            }
            //
            //            // Display current state, and ON/OFF buttons for GPIO 27
            //            client.println("<p>GPIO 27 - State " + output27State + "</p>");
            //            // If the output27State is off, it displays the ON button
            //            if (output27State == "off") {
            //              client.println("<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>");
            //            } else {
            //              client.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
            //            }
            //            client.println("</body></html>");



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
    //    Serial.println("Client disconnected.");
    //    Serial.println("");
  }
}



void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
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

















// Embedded Functions ------------------------------------------------------------------------------------------

//void pinSwitch(sensor1) {
//  // Switches which pressure sensor is being read/commanded by "jamming" the other sensor by writing
//  // high to SDA on the inactive sensor
//  if (sensor1 == TRUE) { // reading from sensor 1
//    digitalWrite(switch_pin[1], HIGH); // disables sensor 1, enables sensor 2
//    digitalWrite(switch_pin[2], LOW);
//    sensor1 = FALSE; // switches sensor1 value, sensor 2 is now being read from
//
//    else {
//      digitalWrite(switch_pin[2], HIGH);
//      digitalWrite(switch_pin[1], LOW);
//      sensor1 = TRUE; // sensor 1 now being read from
//    }
////    return sensor1
//  }
//}
