// DRAGRAKE
// Written by Danny Maas, Bennett Diamond and Christian BLoemhof
// Collects pressure transducer data via i2c, processes data to make airspeed readings, 
// Creates WiFi network, hosts a webpage that displays this data


// Libraries ---------------------------------------------------------------------------------------------------
#include <Wire.h>
#include <SPI.h>
#include <WiFi101.h>
char ssid[] = "DragRake";         // Network Name
char pass[] = "";                 // Network password
int keyIndex = 0;                
int status = WL_IDLE_STATUS;
// Variables ---------------------------------------------------------------------------------------

// Command Registers (i2c commands for the pressure transducer)

#define Start_Single 0xAA     // Hexadecimal address for single read
#define Start_Average2 0xAC   // Hexadecimal address for average of 2 reads
#define Start_Average4 0xAD   // Hexadecimal address for average of 4 reads
#define Start_Average8 0xAE   // Hexadecimal address for average of 8 reads
#define Start_Average16 0xAF  // Hexadecimal address for average of 16 reads
#define s1Toggle 11           // Pin used to toggle sensor 1
#define s2Toggle 12           // Pin used to toggle sensor 2

// Array of all commands to simplify changing modes (i2c stuff)
byte start_comm[5] = 
{Start_Single, 
Start_Average2, 
Start_Average4, 
Start_Average8, 
Start_Average16};
int st_mode = 1;              // Command mode selector

// Array of calibration registers
int cal_reg[11] = {47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57};

// Variables and Arrays needed for website to work
String text[6];
String color[6];
int selected;

// Variables used to measure battery voltage
#define VBATPIN A7
float measuredvbat;

// Variables used in calculations
float coeff[7];
int32_t rawCoeff[4];
int8_t rawTempCoeff[3];
byte lowWord;
byte highWord;
byte words[20];
float sensor1;
float sensor2;

// Required for Serial on Zero based boards
#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
#define Serial SERIAL_PORT_USBVIRTUAL
#endif

// Needed for HTTP requests to work
#define REQ_BUF_SZ   50
char HTTP_req[REQ_BUF_SZ] = {0};    // buffered HTTP request stored as null terminated string
char req_index = 0;                 // index into HTTP_req buffer

// Set web server port number to 80 (Change if needed)
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Setup -------------------------------------------------------------------------------------------------------
void setup()
{
  // Set one sensor to active, the other to off
  pinMode(s1Toggle, INPUT);
  pinMode(s2Toggle, OUTPUT);
  digitalWrite(s2Toggle, HIGH);

  // Start I2C bus
  Wire.begin();

  // Start WiFi network
  WiFi.setPins(8, 7, 4, 2);
  Serial.begin(9600);
  Serial.print("Creating access point named: ");
  Serial.println(ssid);
  
  // Override the default IP address of 192.168.1.1
  WiFi.config(IPAddress(4, 20, 6, 9));
  status = WiFi.beginAP(ssid);
  
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // Don't continue if creating the WiFi network failed
    while (true);
  }
  server.begin();
  printWiFiStatus();
  requestCalib();
}

// Loop --------------------------------------------------------------------------------------------------------
void loop()
{
  WiFiClient client = server.available();   // Listen for incoming clients
  if (client)
  {
    boolean currentLineIsBlank = true;
    String currentLine = "";                // Make a String to hold incoming data from the client
    int i = 0;
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();             // Store the client data 

        if (req_index < (REQ_BUF_SZ - 1)) {
          HTTP_req[req_index] = c;          // Save HTTP request character
          req_index++;
        }
        if ((72 < i) && (i < 75))           // The data we are looking for (from the buttons) is at characters 73 - 74
                                            // We get back the length of the string from each button. This string length
                                            // Is different for each button, so by reading this length we can figure out which
                                            // Button was pressed. This allows button processing to be greatly sped up
        {
          // Serial.println(c); //Debugging purposes
          
          // This loop processes the data from the client request, and changes the variable "selected," which displays which
          // button has been pressed, as well as changes the averaging type
          char q[2];
          q[i - 73] = c;
          if (q[0] == '1')
          {
            if (i == 74)
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
            }
          }
          else if (i == 73)
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
        // End of button processing code (probably could be done much more efficiently
        
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

            //            selected = 1;

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

// Reads data from both sensors and stores it in global variable
void takeMeasurements()
{
  toggle(1);
  delay(1);
  requestRead();
  waitForDone();
  sensor1 = requestData();
  toggle(2);
  delay(1);
  requestRead();
  waitForDone();
  sensor2 = requestData();
}

// Get calibration data from sensors (On startup)
void requestCalib()
{
  // Sets sensor 1 to active
  toggle(1);
  
  //Coefficients are the same from both sensors, so we only need them from one
  int j = 0;
  for (byte i = 0x2F; i < 0x39; i++)
  {
    Wire.beginTransmission(0x29);
    Wire.write(i);
    Wire.endTransmission();
    Wire.requestFrom(0x29, 3);
    while (Wire.available())
    {
      words[j++] = Wire.read();
    }
  }
  for (int b = 0; b < 4; b++)
  {
    rawCoeff[b] = (words[(4 * b)] << 24) | (words[(4 * b) + 1] << 16) | (words[(4 * b) + 2] << 8) | (words[(4 * b) + 3]);
    coeff[b] = (float)rawCoeff[b] / (float)0x7FFFFFFF;
  }

  // Parse coefficient data to get coefficients
  rawTempCoeff[0] = words[16];
  rawTempCoeff[1] = words[17];
  rawTempCoeff[2] = words[18];
  coeff[4] = (float)rawTempCoeff[0] / (float)0x7F;
  coeff[5] = (float)rawTempCoeff[1] / (float)0x7F;
  coeff[6] = (float)rawTempCoeff[2] / (float)0x7F;
}

// Toggles which sensor is active
void toggle(int sensor)
{
  if (sensor == 1)
  {
    pinMode(s1Toggle, INPUT);
    pinMode(s2Toggle, OUTPUT);
    digitalWrite(s2Toggle, HIGH);
  }
  else if (sensor == 2)
  {
    pinMode(s1Toggle, OUTPUT);
    pinMode(s2Toggle, INPUT);
    digitalWrite(s1Toggle, HIGH);
  }
  else
  {
  }
}


// send the XML file with switch statuses and analog value
void XML_response(WiFiClient cl)
{
  int analog_val;
  takeMeasurements();
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  cl.print("<?xml version = \"1.0\" ?>");
  cl.print("<inputs>");
  cl.print("<upper>");
  cl.print(sensor1);
  cl.print("</upper>");
  cl.print("<lower>");
  cl.print(sensor2);
  cl.print("</lower>");
  cl.print("<average>");
  cl.print((sensor1 + sensor2) / (float)2);
  cl.print("</average>");
  cl.print("<battery>");
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

// Prints wifi status to serial port
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

// Starts read of pressure and temperature data
void requestRead()
{
  // If button is not set to "Off"
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

// Allows us to wait for the sensor to be done reading before asking for data back
void  waitForDone()
{
  int i = 0;
  bool lineReady = 0;
  while (lineReady == 0)
  {
    
    // If the sensor is still taking data, the bit will be 1. 
    // If it is ready, the bit will be zero, and we can read data.
    Wire.requestFrom(0x29, 1);
    byte Status = 0;
    bool lineReady = 0;
    while (Wire.available())
    {
      Status = Wire.read();
    }
    if (bitRead(Status, 5) == 0)
    {
      lineReady = 1;
      break;
    }
    else
    {
    }
  }
}

// Requests temperature and pressure data back from sensors
float requestData()
{
  int sign;
  float deltaV;
  int32_t iPraw;
  float pressure;
  int32_t iTemp;
  unsigned char data[7] = {0};
  float AP3, BP2, CP, LCorr, PCorr, Padj, TCadj, TC50, Pnorm;
  int32_t Tdiff, Tref, iPCorrected;
  uint32_t uiPCorrected;

  // Request data from I2C device
  Wire.requestFrom(0x29, 7);
  int i = 0;
  while (Wire.available())
  {
    // Add each bit to an array for ease of access
    data[i++] = Wire.read();
  }

  //Big 'ol calculation (which may not actually be right :/
  iPraw = ((data[1] << 16) + (data[2] << 8) + (data[3]) - 0x800000);
  Serial.print(iPraw);
  iTemp = (data[4] << 16) + (data[5] << 8) + (data[6]);
  Pnorm = (float)iPraw;
  Pnorm /= (float) 0x7FFFFF;
  AP3 = coeff[0] * Pnorm * Pnorm * Pnorm;
  BP2 = coeff[1] * Pnorm * Pnorm;
  CP = coeff[2] * Pnorm;
  LCorr = AP3 + BP2 + CP + coeff[3];
  
  // Compute Temperature - Dependent Adjustment:
  Tref = (int32_t)(pow(2, 24) * 65 / 125); // Reference Temperature, in sensor counts
  Tdiff = iTemp - Tref;
  Tdiff = Tdiff / 0x7FFFFF;
  
  //TC50: Select High/Low, based on sensor temperature reading:
  if (iTemp > Tref)
    TC50 = (coeff[4] - 1);
  else
    TC50 = (coeff[5] - 1);
  if (Pnorm > 0.5)
    Padj = Pnorm - 0.5;
  else
    Padj = 0.5 - Pnorm;
  TCadj = (1.0 - (coeff[6] * 1.25 * Padj)) * Tdiff * TC50;
  PCorr = Pnorm + LCorr + TCadj; // corrected P: float, ±1.0
  iPCorrected = (int32_t)(PCorr * (float)0x7FFFFF); // corrected P: signed int32
  uiPCorrected = (uint32_t) (iPCorrected + 0x800000);
  pressure = ((float)12.5 * ((float)uiPCorrected / (float)pow(2, 23)));
  pressure = pressure - (float) 9.69;
  if (pressure < 0)
  {
    sign = -1;
    //Can't take sqrt of negative
  } else {
    sign = 1; 
  }
  deltaV = sqrt((2 * (abs(pressure) * 249.089)) / (1.225)); //Pascals to meters per second
  deltaV *= 1.943; //Metres per second to knots
  Serial.print(" ");
  Serial.println(deltaV);
  return sign * deltaV;
}
