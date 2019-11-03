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
byte start_comm[5] = {Start_Single, Start_Average2, Start_Average4, Start_Average8, Start_Average16};
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
#define VBATPIN A7
float measuredvbat;
#define EOC 10
byte data[7];
signed long raw;
float scaled;
#define twentyFour 16777216
#define sixteen 65536
#define eight 256
long partA;
long partB;
long partC;






#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
// Required for Serial on Zero based boards
#define Serial SERIAL_PORT_USBVIRTUAL
#endif




#define REQ_BUF_SZ   50
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer




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
  pinMode(EOC, INPUT);
  Wire.begin();
  WiFi.setPins(8, 7, 4, 2);
  Serial.begin(115200); // change baud rate?
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
  server.begin();
  printWiFiStatus();
}

// Loop --------------------------------------------------------------------------------------------------------
void loop()
{


  WiFiClient client = server.available();   // Listen for incoming clients
  if (client)
  {
    boolean currentLineIsBlank = true;
    String currentLine = "";  // make a String to hold incoming data from the client
    int i = 0;

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        if (req_index < (REQ_BUF_SZ - 1)) {
          HTTP_req[req_index] = c;          // save HTTP request character
          req_index++;
        }


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
          else if (i == 76)
          {
            if (q[0] == '5')
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
          }
          else
          {
          }
        }
        else
        {
        }
        i++;


        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          if (StrContains(HTTP_req, "ajax_inputs")) {
            // send rest of HTTP header
            client.println("Content-Type: text/xml");
            client.println("Connection: keep-alive");
            client.println();
            // send XML file containing input states
            XML_response(client);
          }
          else {
            client.println("Content-Type: text/html");
            client.println("Connection: keep-alive");
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
            //Set all the clickable buttons to be gray
            for (int i = 0; i < 6; i++)
            {
              text[i] = "black";
              color[i] = "ddd}";
            }
            //Set ONLY the selected button to be blue
            text[selected] = "white";
            color[selected] = "2196F3}";

            client.println("<!DOCTYPE html><html><head>");
            client.println("<title>Drag Rake Controls</title>");

            //Script for getting and processing the Arduino data
            client.println("<script>");
            client.println("function GetSwitchState() {");
            client.println("nocache = \"&nocache=\"+ Math.random() * 1000000;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseXML != null) {");

            client.println("document.getElementById(\"Upper\").value =this.responseXML.getElementsByTagName('upper')[0].childNodes[0].nodeValue;");
            client.println("document.getElementById(\"Lower\").value =this.responseXML.getElementsByTagName('lower')[0].childNodes[0].nodeValue;");
            client.println("document.getElementById(\"Average\").value =this.responseXML.getElementsByTagName('average')[0].childNodes[0].nodeValue;");
            client.println("document.getElementById(\"Battery\").value =this.responseXML.getElementsByTagName('battery')[0].childNodes[0].nodeValue;");

            client.println("}}}}");
            client.println("request.open(\"GET\", \"ajax_inputs\" + nocache, true);");
            client.println("request.send(null);");
            client.println("setTimeout('GetSwitchState()', 100);"); //EDIT THIS FOR POLLING SPEED
            client.println("}");
            client.println("</script>");
            //End Script

            //Attempt at Scaling
            client.println("<meta name = \"viewport\" content = \"width = device - width, initial - scale = 1\">");
            client.println("<style>");
            //Set header properties (to get proper spacing):
            client.println("h1{line-height:64px;font-size: 32px;margin:0px;padding:0px;}");
            client.println("h2{line-height:0px;font-size: 32px;margin:0px;padding:0px;}");
            //Button properties for the data readouts:
            client.println(".read {border: 2px solid black;margin: 12px 8px;background-color: white;color: black;");
            client.println("height: 64px;width: 128px;font-size: 32px;cursor: pointer;text-align: center;}");
            //Create each data button (non-clickable):
            client.println(".upp {border-color: #000000;color: black;}");
            client.println(".low {border-color: #000000;color: black;}");
            client.println(".avg {border-color: #000000;color: black;}");
            client.println(".batt {border-color: #000000;color: black;}");
            //Button properties for the clickable buttons :
            client.println(".btn {border: none;color: black;width: 128px; height: 64px;text - align: center;");
            client.println("font - size: 32px;margin: 4px 2px;border - radius: 20px;}");
            //Create each clickable button:
            client.println(".off {color:" + text[0] + ";background-color: #" + color[0]);
            client.println(".single {color:" + text[1] + ";background-color: #" + color[1]);
            client.println(".avg2 {color:" + text[2] + ";background-color: #" + color[2]);
            client.println(".avg4 {color:" + text[3] + ";background-color: #" + color[3]);
            client.println(".avg8 {color:" + text[4] + ";background-color: #" + color[4]);
            client.println(".avg16{color:" + text[5] + ";background-color: #" + color[5]);
            client.println("</style></head>");
            //Body of code:
            client.println("<body onload=\"GetSwitchState()\">");
            //Data Division:
            client.println("<div>");
            client.println("<h1>Akaflieg SLO Drag Rake</h1>");
            client.println("<h2><input class=\"read upp\" id=\"Upper\" value=\"0\"/>Upper Surface &Delta;V (kts)</h2>");
            client.println("<h2><input class=\"read low\" id=\"Lower\" value=\"0\"/>Lower Surface &Delta;V (kts)</h2>");
            client.println("<h2><input class=\"read avg\"id=\"Average\" value=\"0\"/>Average &Delta;V (kts)</h2>");
            client.println("<h2><input class=\"read batt\"id=\"Battery\" value=\"0\"/>Battery Remaining (%)</h2>");
            client.println("</div>");

            //Form Division:
            client.println("<div>");
            client.println("<h1>Sensor Reader Mode</h1>");
            client.println("<form method = \"post\">");
            client.println("<input class = \"btn off\" type = \"submit\" name = \"1\" id=\"Off\" value = \"Off\" onclick = \"submit()\">");
            client.println("<input class = \"btn single\" type = \"submit\" name = \"1\" id=\"Single\" value = \"Single\" onclick = \"submit()\">");
            client.println("<input class = \"btn avg2\" type = \"submit\" name = \"123\" id=\"2 AVG\" value = \"2 AVG\" onclick = \"submit()\">");
            client.println("<br>");
            client.println("<input class = \"btn avg4\" type = \"submit\" name = \"1234\" id=\"4 AVG\" value = \"4 AVG\" onclick = \"submit()\">");
            client.println("<input class = \"btn avg8\" type = \"submit\" name = \"13245\" id=\"8 AVG\" value = \"8 AVG\" onclick = \"submit()\">");
            client.println("<input class = \"btn avg16\" type = \"submit\" name = \"12345\" id=\"16 AVG\" value = \"16 AVG\" onclick = \"submit()\">");
            client.println("</form>");
            client.println("</div>");
            client.println("</body></html>");
            client.println(); // The HTTP response ends with another blank line
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
          }
          // display received HTTP request on serial port

          // finished with request, empty string
          req_index = 0;
          StrClear(HTTP_req, REQ_BUF_SZ);
          break;
        }
        // every line of text received from the client ends with \r\n
        if (c == '\n') {
          // last character on line of received text
          // starting new line with next character read
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // a text character was received from client
          currentLineIsBlank = false;
        }
      } // end if (client.available())
    } // end while (client.connected())
    delay(1);      // give the web browser time to receive the data
    client.stop(); // close the connection
  } // end if (client)
}



// send the XML file with switch statuses and analog value
void XML_response(WiFiClient cl)
{
  int analog_val;

  cl.print("<?xml version = \"1.0\" ?>");
  cl.print("<inputs>");
  cl.print("<upper>");
  requestRead();
  waitForEOC();
  requestData();
  cl.print(scaled);
  cl.print("</upper>");
  cl.print("<lower>");
  cl.print(analogRead(3));
  cl.print("</lower>");
  cl.print("<average>");
  cl.print(analogRead(2));
  cl.print("</average>");
  cl.print("<battery>");
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  cl.print(measuredvbat);
  cl.print("</battery>");
  cl.print("</inputs>");
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
  for (int i = 0; i < length; i++) {
    str[i] = 0;
  }
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
  char found = 0;
  char index = 0;
  char len;

  len = strlen(str);

  if (strlen(sfind) > len) {
    return 0;
  }
  while (index < len) {
    if (str[index] == sfind[found]) {
      found++;
      if (strlen(sfind) == found) {
        return 1;
      }
    }
    else {
      found = 0;
    }
    index++;
  }

  return 0;
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


void requestRead()
{
  if (selected > 0)
  {
    Wire.beginTransmission(0x29);
    Wire.write(start_comm[selected - 1]);
    Wire.endTransmission();
  }
  else
  {
  }
}


void  waitForEOC()
{
  if (selected > 0)
  {
    int wait = 2;
    while (wait == 2)
    {
      if (digitalRead(EOC) == 0)
      {
        wait = 1;
        while (wait == 1)
        {
          if (digitalRead(EOC) == 1)
          {
            wait = 0;

          }
          else
          {
          }
        }
      }
      else
      {
      }
    }
  }
  else
  {

  }
}

void requestData()
{
  if (selected > 0)
  {
    Wire.requestFrom(0x29, 7);
    int i = 0;
    while (Wire.available())
    {
      data[i++] = Wire.read();
    }
    int A = (data[1]);
    int B = (data[2]);
    int C = (data[3]);
    partA = (A * sixteen);
    partB = (B * eight);
    raw = (partA + partB + C);
    scaled = 8567500 - raw;
  }
  else
  {
    scaled = 0;
  }
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
