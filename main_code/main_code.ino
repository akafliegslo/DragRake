// DRAGRAKE
// Written by Danny Maas and Bennett Diamond
// Collects pressure transducer data to an SD card via SPI
// Creates WiFi network, hosts a webpage that displays this data

// Libraries --------------------------------------------------------------------------------------
#include <SPI.h>
#include <WiFi101.h>
#include <SD.h>

// Variables ---------------------------------------------------------------------------------------

// Command Registers (SPI commands for the pressure transducer)
#define Start_Single 0xAA    // Hexadecimal address for single read
#define Start_Average2 0xAC  // Hexadecimal address for average of 2 reads
#define Start_Average4 0xAD  // Hexadecimal address for average of 4 reads
#define Start_Average8 0xAE  // Hexadecimal address for average of 8 reads
#define Start_Average16 0xAF // Hexadecimal address for average of 16 reads

// Initialing SPI chip select lines
#define s1Toggle 11   // Pin used to toggle sensor 1
#define s2Toggle 12   // Pin used to toggle sensor 2
#define SD_card_CS 10 // Pin used for SD card slave select
bool sen1 = HIGH;     // Initialize everything high
bool sen2 = HIGH;

// Array of all commands to simplify changing modes
byte start_comm[5] =
    {Start_Single,
     Start_Average2,
     Start_Average4,
     Start_Average8,
     Start_Average16};
int st_mode = 1; // Command mode selector

// Array of calibration registers
int cal_reg[11] = {47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57};

// Variables and Arrays needed for website to work
String text[6];
String color[6];
int selected = 1;

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
float temp1;
float temp2;

// SD Card Variables
String current_file = "data.txt"; // start with default name

// Required for Serial on Zero based boards
#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
#define Serial SERIAL_PORT_USBVIRTUAL
#endif

// WiFi Network Variables
char ssid[] = "DragRake"; // Network Name
char pass[] = "";         // Network password
int keyIndex = 0;
int status = WL_IDLE_STATUS;

// Needed for HTTP requests to work
#define REQ_BUF_SZ 50
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer

// Set web server port number to 80 (Change if needed)
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// SPI configurations
SPISettings sensors(1000000, MSBFIRST, SPI_MODE0); //Both sensors are logic state low

// Setup -------------------------------------------------------------------------------------------------------
void setup()
{
  // Set one sensor to active, the other to off
  pinMode(s1Toggle, OUTPUT);
  pinMode(s2Toggle, OUTPUT);
  pinMode(SD_card_CS, OUTPUT);

  WiFi.setPins(8, 7, 4, 2); // needed for ATWINC1500
  Serial.begin(9600);
  SPI.begin();    // moved ahead of setting up wifi, makes a difference?
  requestCalib(); // requests calibration coefficients from sensor EEPROM

  // Start WiFi network
  Serial.print("Creating access point named: ");
  Serial.println(ssid);
  WiFi.config(IPAddress(4, 20, 6, 9));   // Override the default IP address of 192.168.1.1
  status = WiFi.beginAP(ssid);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // Don't continue if creating the WiFi network failed
    while (true);
  }

  server.begin(); // starts WiFi server
  printWiFiStatus();
  WiFi.lowPowerMode(); // tries to keep power draw during operation to a minimum

  // Initializing Sd card interface
  SD.begin(SD_card_CS);
  if (!SD.begin(SD_card_CS))
  {
    Serial.println("SD Card Failure");
  }

  // Creating unique file name to avoid overwriting existing files
  int i; // declare i for while loop
  while (SD.exists(current_file)) // prevents overwriting data by generating a new file if one with the same name already exists
  {
    current_file = String("data" + String(i) + ".txt");
    i++;
  }

  // Adding headers to data file
  File dataFile = SD.open(current_file, FILE_WRITE);
  String dataHeader = "Time (from starting)  Top Pressure  Bottom Pressure  Top Temp  Bottom Temp  Battery Voltage";

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataHeader);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataHeader);
  }
  Serial.print("a"); // debug 
  
}

// Loop --------------------------------------------------------------------------------------------------------
void loop()
{
  recordData(); // records all the sensor data

  Serial.print("rawTempCoeff");
  for (int i = 0; i < 7; i++)
  {
    Serial.print(rawTempCoeff[i]);
  }

  Serial.print("Coeff");
  for (int i = 0; i < 7; i++)
  {
    Serial.print(coeff[i]);
  }

  /*
  WiFiClient client = server.available();   // Listen for incoming clients
  if (client)
  {
    Serial.print("I am in client if statement");
    boolean currentLineIsBlank = true;
    String currentLine = "";                // Make a String to hold incoming data from the client
    int i = 0;
    while (client.connected())
    {
       recordData(); // records all the sensor data
      Serial.print("I am in client connected while statement");
      if (client.available())
      {
        Serial.print("I am in client available if statement");
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
  */

  WiFiClient client = server.available(); // listen for incoming clients

  if (client)
  {                               // if you get a client,
    Serial.println("new client"); // print a message out the serial port
    String currentLine = "";      // make a String to hold incoming data from the client
    while (client.connected())
    { // loop while the client's connected
      if (client.available())
      {                         // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        if (c == '\n')
        { // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            //HTML Setup Stuff
            client.println("<!DOCTYPE html> <html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">");
            client.println("<title>Drag Rake Controls</title>");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}");
            client.println("body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px; text-align: right;}");
            
            // Button Characteristics
            client.println(".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;");
            client.print("text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}");
            client.println(".button-on {background-color: #3498db;}");
            client.println(".button-on:active {background-color: #2980b9;}");
            client.println(".button-off {background-color: #34495e;}");
            client.println(".button-off:active {background-color: #2c3e50;}");
            client.println("p {font-size: 14px;color: #888;margin-bottom: 10px;}");

            // Data Readout Characteristics
            client.println(".read {display: block; height: 64 px; width: 128px; background-color: #FFFFFF; ");
            client.print("color: white; padding: 13px 30px; font-size: 32px; margin: 0px auto 35px; cursor: pointer; border-radius: 4px;}");

            //Create each data button (non-clickable): MIGHT NOT BE NEEDED
            client.println(".upp {border-color: #000000;color: black;}");
            client.println(".low {border-color: #000000;color: black;}");
            client.println(".avg {border-color: #000000;color: black;}");
            client.println(".batt {border-color: #000000;color: black;}");

            // Data Input
            client.println("<inputs><upper>");
            client.println(sensor1);
            client.println("</upper>");
            client.println("<lower>");
            client.println(sensor2);
            client.println("</lower>");
            client.println("<average>");
            client.println((sensor1 + sensor2) / (float)2);
            client.println("</average>");
            client.println("<battery>");
            client.println(measuredvbat);
            client.println("</battery>");
            client.println("</inputs>");

            client.println("</style>");
            client.println("</head>");
            client.println("<body>");

            // Title
            client.println("<h1>Drag Rake</h1>");
            client.println("<h3>Akaflieg SLO ATWINC1500 Proto-build</h3>");
            client.println("<!-- EASTER EGG -->");

            // Data displays
            client.print("<h2><input class=\"read upp\" id=\"Upper\" value=\"0\"/>Upper Surface &Delta;Pressure (Pa)</h2>");
            client.print("<h2><input class=\"read low\" id=\"Lower\" value=\"0\"/>Lower Surface &Delta;Pressure (Pa)</h2>");
            client.print("<h2><input class=\"read avg\"id=\"Average\" value=\"0\"/>Average &Delta;Pressure (Pa)</h2>");
            client.println("<h2><input class=\"read batt\"id=\"Battery\" value=\"0\"/>Battery Remaining (%)</h2>");

            // Buttons
            client.println("<h3>Sensor Read Modes</h3>");
            switch (selected)
            {
            case 0:
              client.print("<a class=\"button button-off\" href=\"/\">OFF</a>");
              client.print("<a class=\"button button-on\" href=\"/read1\">ON</a>");
              client.println("<a class=\"button button-on\" href=\"/read2\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read4\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read8\">ON</a>");
              client.println("<a class=\"button button-on\" href=\"/read16\">ON</a>");
              break;

            case 1:
              client.print("<a class=\"button button-on\" href=\"/\">ON</a>");
              client.print("<a class=\"button button-off\" href=\"/read1\">OFF</a>");
              client.println("<a class=\"button button-on\" href=\"/read2\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read4\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read8\">ON</a>");
              client.println("<a class=\"button button-on\" href=\"/read16\">ON</a>");
              break;

            case 2:
              client.print("<a class=\"button button-on\" href=\"/\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read1\">ON</a>");
              client.println("<a class=\"button button-off\" href=\"/read2\">OFF</a>");
              client.print("<a class=\"button button-on\" href=\"/read4\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read8\">ON</a>");
              client.println("<a class=\"button button-on\" href=\"/read16\">ON</a>");
              break;

            case 3:
              client.print("<a class=\"button button-on\" href=\"/\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read1\">ON</a>");
              client.println("<a class=\"button button-on\" href=\"/read2\">ON</a>");
              client.print("<a class=\"button button-off\" href=\"/read4\">OFF</a>");
              client.print("<a class=\"button button-on\" href=\"/read8\">ON</a>");
              client.println("<a class=\"button button-on\" href=\"/read16\">ON</a>");
              break;

            case 4:
              client.print("<a class=\"button button-on\" href=\"/\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read1\">ON</a>");
              client.println("<a class=\"button button-on\" href=\"/read2\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read4\">ON</a>");
              client.print("<a class=\"button button-off\" href=\"/read8\">OFF</a>");
              client.println("<a class=\"button button-on\" href=\"/read16\">ON</a>");
              break;

            case 5:
              client.print("<a class=\"button button-on\" href=\"/\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read1\">ON</a>");
              client.println("<a class=\"button button-on\" href=\"/read2\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read4\">ON</a>");
              client.print("<a class=\"button button-on\" href=\"/read8\">ON</a>");
              client.println("<a class=\"button button-off\" href=\"/read16\">OFF</a>");
              break;
            }

            client.println("</body>");
            client.println("</html>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          }
          else
          { // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }

        // Checks read mode:
        if (currentLine.endsWith("GET /")) {
          selected = 0;
        } else if (currentLine.endsWith("GET /read1")) {
          selected = 1;
        } else if (currentLine.endsWith("GET /read2")) {
          selected = 2;
        } else if (currentLine.endsWith("GET /read4")) {
          selected = 3;
        } else if (currentLine.endsWith("GET /read8")) {
          selected = 4;
        } else if (currentLine.endsWith("GET /read16")) {
          selected = 5;
        }    
        Serial.print("Selected mode:");
        Serial.println(selected);    
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

// Functions ------------------------------------------------------------------------------------------

// Takes all data independent of wifi in case client isn't connected
void recordData()
{
  Serial.println("I take data now");
  takeMeasurements();

  // Display Data
  Serial.println("Sensor 1:" + String(sensor1));
  Serial.println("Sensor 2:" + String(sensor2));
  Serial.println("Temp 1:" + String(temp1));
  Serial.println("Temp 2:" + String(temp2));

  measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 6.6;  // Multiply by 2 and 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  Serial.println(measuredvbat);
  delay(1000);
  //SD_write(current_file); // Write values to SD Card
}

// Reads data from both sensors and stores it in global variable
void takeMeasurements()
{
  requestRead(1);
  waitForDone(1);
  sensor1 = requestData(1, &temp1);
  delay(1);
  requestRead(2);
  waitForDone(2);
  sensor2 = requestData(2, &temp2);
}

// Get calibration data from sensors (On startup)
void requestCalib()
{
  // Coefficients are the same from both sensors, so we only need them from one
  int j = 0;
  for (byte i = 0x2F; i < 0x39; i++)
  {
    SPI.beginTransaction(sensors);
    toggle(1);
    SPI.transfer(i);
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    uint32_t buff = (0xF0) | (0x00 << 8) | 0x00;
    SPI.transfer(buff);

    // Arduino transmits measurement start command, will receive back status byte and two bytes of "undefined data"
    toggle(1);
    SPI.endTransaction();

    // Parse bytes into one "word"
    word x = word(byte(buff >> 8), byte(buff));

    // Add piece of data to the words
    words[j++] = x;
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

void toggle(int sensor)
{
  if (sensor == 2)
  {
    digitalWrite(s2Toggle, !sen1);
    sen1 = !sen1;
  }
  else if (sensor == 1)
  {
    digitalWrite(s1Toggle, !sen2);
    sen2 = !sen2;
  }
}

/*
// send the XML file with switch statuses and analog value
void XML_response(WiFiClient cl)
{
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
*/

// Prints wifi status to serial port
void printWiFiStatus()
{

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
int requestRead(int tog)
{
  // If button is not set to "Off"
  if (selected > 0)
  {
    Serial.println("Trying to request sensor data");
    uint32_t buff = ((start_comm[selected - 1] << 16) | (0x00 << 8) | 0x00);
    SPI.beginTransaction(sensors);
    toggle(tog);
    SPI.transfer(&buff, 3);
    //Arduino transmits measurement start command, will receive back status byte and two bytes of "undefined data"
    toggle(tog);
    SPI.endTransaction();
  }
  else
  {
  }
}

// Allows us to wait for the sensor to be done reading before asking for data back
int waitForDone(int tog)
{
  int i = 0;
  bool lineReady = 0;
  while (lineReady == 0)
  {

    // If the sensor is still taking data, the bit will be 1.
    // If it is ready, the bit will be zero, and we can read data.
    byte Status = 0xF0;
    SPI.beginTransaction(sensors);
    toggle(tog);

    //Arduino transmits 0xF0, receives back status byte
    SPI.transfer(&Status, 3);
    toggle(tog);
    SPI.endTransaction();
    bool lineReady = 0;
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
float requestData(int tog, float *temperature)
{
  int32_t iPraw;
  float pressure;
  int32_t iTemp;
  float AP3, BP2, CP, LCorr, PCorr, Padj, TCadj, TC50, Pnorm;
  int32_t Tdiff, Tref, iPCorrected;
  uint32_t uiPCorrected;
  Serial.println("Trying to take sensor data");
  // Get data from SPI lines
  SPI.beginTransaction(sensors);
  toggle(tog);
  byte stat = SPI.transfer(0xF0);
  byte p3 = SPI.transfer(0x00);
  byte p2 = SPI.transfer(0x00);
  byte p1 = SPI.transfer(0x00);
  byte t3 = SPI.transfer(0x00);
  byte t2 = SPI.transfer(0x00);
  byte t1 = SPI.transfer(0x00);
  unsigned char data[7] = {stat, p3, p2, p1, t3, t2, t1};

  // Arduino transmits read command, gets back lots of data (hopefully)
  toggle(tog);
  SPI.endTransaction();

  //Big 'ol calculation (which may not actually be right :/
  iPraw = ((data[1] << 16) + (data[2] << 8) + (data[3]) - 0x800000);
  iTemp = (data[4] << 16) + (data[5] << 8) + (data[6]);
  Pnorm = (float)iPraw;
  Pnorm /= (float)0x7FFFFF;
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
  {
    TC50 = (coeff[4] - 1);
  }
  else
  {
    TC50 = (coeff[5] - 1);
  }
  if (Pnorm > 0.5)
  {
    Padj = Pnorm - 0.5;
  }
  else
  {
    Padj = 0.5 - Pnorm;
  }

  // Calculating compensated pressure
  TCadj = (1.0 - (coeff[6] * 1.25 * Padj)) * Tdiff * TC50;
  PCorr = Pnorm + LCorr + TCadj;                    // corrected P: float, ±1.0
  iPCorrected = (int32_t)(PCorr * (float)0x7FFFFF); // corrected P: signed int32
  uiPCorrected = (uint32_t)(iPCorrected + 0x800000);
  pressure = ((float)12.5 * ((float)uiPCorrected / (float)pow(2, 23)));
  pressure = pressure - (float)9.69;

  // Calculating temperature (degrees Celcius)
  *temperature = ((iTemp * 125) / pow(2, 24)) - 40; // Celcius

  return pressure;
}

void SD_write(String current_file)
{
  // Creating Data String, see comments at top of code for formatting
  String dataString = String(millis() + "  " + String(sensor1) + "  " + String(sensor2) +
                             "  " + String(temp1) + "  " + String(temp2) + "  " + String(measuredvbat));

  // opening the file
  File dataFile = SD.open(current_file, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile)
  {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else
  {
    Serial.println("error opening file");
  }
}
