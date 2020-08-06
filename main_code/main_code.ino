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
int selected = 1;

// Variables used to measure battery voltage
#define VBATPIN A7
float measuredvbat;

// Variables used in calculations
float coeff[7];
float sensor1;
float sensor2;
float temp1;
float temp2;

// Required for Serial on Zero based boards
#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
#define Serial SERIAL_PORT_USBVIRTUAL
#endif

// WiFi Network Variables
int status = WL_IDLE_STATUS;


String HTTP_req;
bool currentLineIsBlank = true;

// Set web server port number to 80 (Change if needed)
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Setup -------------------------------------------------------------------------------------------------------
void setup() {
  // Set one sensor to active, the other to off
  pinMode(s1Toggle, OUTPUT);
  pinMode(s2Toggle, OUTPUT);
  pinMode(SD_card_CS, OUTPUT);

  WiFi.setPins(8, 7, 4, 2); // needed for ATWINC1500
  Serial.begin(115200);

  SPI.begin();    // Starts SPI bus

  // Start WiFi network
  Serial.print("Creating access point gals: ");
  //WiFi.config(IPAddress(192, 168, 1, 1));
  status = WiFi.beginAP("Drag Rake");
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // Don't continue if creating the WiFi network failed
    while (true) {
      } //Arduino is stupid
  }

  server.begin(); // starts WiFi server
  printWiFiStatus();

  // Initializing Sd card interface
  SD.begin(SD_card_CS);
  if (!SD.begin(SD_card_CS))
  {
    Serial.println("SD Card Failure"); /* starts printing here */
  }

  // Creating unique file name to avoid overwriting existing files
  String current_file = "data.txt"; // start with default name
  for(size_t i = 0; SD.exists(current_file); i++)
    current_file = String("data" + String(i) + ".txt");

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

  requestCalib(); // requests calibration coefficients from sensor EEPROM


}

// Loop --------------------------------------------------------------------------------------------------------
void loop() {
  recordData();

  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {
    Serial.print("I am in client if statement");
    bool currentLineIsBlank = true;
    String HTTP_req = "";                // Make a String to hold incoming data from the client
    int i = 0;
    while (client.connected()) {

      Serial.print("I am in client connected while statement");
      if (client.available()) {
        Serial.print("I am in client available if statement");
        char c = client.read();             // Store the client data

        HTTP_req += c;

        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: keep-alive");
          client.println();

          if (HTTP_req.indexOf("ajax_inputs") > -1) {
            // send rest of HTTP header
            // client.println("Content-Type: text/xml");
            // client.println("Connection: keep-alive");
            // client.println();
            // send XML file containing input states
            XML_response(client);
          } else {


            client.println("<!DOCTYPE html><html><head>");
            client.println("<title>Drag Rake Controls</title>");

            //Script for getting and processing the Arduino data
            client.println("<script>");
            client.println("function GetSwitchState() {");
            client.println("nocache = \"&nocache=\";");
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
            client.println(".button-on {background-color: #3498db;}");
            client.println(".button-on:active {background-color: #2980b9;}");
            client.println(".button-off {background-color: #34495e;}");
            client.println(".button-off:active {background-color: #2c3e50;}");

            //Create each clickable button:
            // client.println(".off {color:" + text[0] + ";background-color: #" + color[0]);
            // client.println(".single {color:" + text[1] + ";background-color: #" + color[1]);
            // client.println(".avg2 {color:" + text[2] + ";background-color: #" + color[2]);
            // client.println(".avg4 {color:" + text[3] + ";background-color: #" + color[3]);
            // client.println(".avg8 {color:" + text[4] + ";background-color: #" + color[4]);
            // client.println(".avg16{color:" + text[5] + ";background-color: #" + color[5]);
            client.println("</style></head>");

            //Body of code:
            client.println("<body onload=\"GetSwitchState()\">");

            //Data Division:
            client.println("<div>");
            client.println("<h1>Akaflieg SLO Drag Rake</h1>");
            client.println("<h2><input class=\"read upp\" id=\"Upper\" value=\"0\"/>Upper Surface &Delta;P (Pa)</h2>");
            client.println("<h2><input class=\"read low\" id=\"Lower\" value=\"0\"/>Lower Surface &Delta;P (Pa)</h2>");
            client.println("<h2><input class=\"read avg\"id=\"Average\" value=\"0\"/>Average &Delta;P (Pa)</h2>");
            client.println("<h2><input class=\"read batt\"id=\"Battery\" value=\"0\"/>Battery Remaining (%)</h2>");
            client.println("</div>");

            //Form Division:
            //client.println("<div>");
            client.println("<h1>Sensor Reader Mode</h1>");
            // Buttons

            switch (selected) {
              case 0:
                client.print("<a class=\"button button-off\" href=\"/\">OFF</a>");
                client.print("<a class=\"button button-on\" href=\"/read1\">Single</a>");
                client.println("<a class=\"button button-on\" href=\"/read2\">2 AVG</a>");
                client.println("<br>");
                client.print("<a class=\"button button-on\" href=\"/read4\">4 AVG</a>");
                client.print("<a class=\"button button-on\" href=\"/read8\">8 AVG</a>");
                client.println("<a class=\"button button-on\" href=\"/read16\">16 AVG</a>");
                break;

              case 1:
                client.print("<a class=\"button button-on\" href=\"/\">OFF</a>");
                client.print("<a class=\"button button-off\" href=\"/read1\">Single</a>");
                client.println("<a class=\"button button-on\" href=\"/read2\">2 AVG</a>");
                client.println("<br>");
                client.print("<a class=\"button button-on\" href=\"/read4\">4 AVG</a>");
                client.print("<a class=\"button button-on\" href=\"/read8\">8 AVG</a>");
                client.println("<a class=\"button button-on\" href=\"/read16\">16 AVG</a>");
                break;

              case 2:
                client.print("<a class=\"button button-on\" href=\"/\">OFF</a>");
                client.print("<a class=\"button button-on\" href=\"/read1\">Single</a>");
                client.println("<a class=\"button button-off\" href=\"/read2\">2 AVG</a>");
                client.println("<br>");
                client.print("<a class=\"button button-on\" href=\"/read4\">4 AVG</a>");
                client.print("<a class=\"button button-on\" href=\"/read8\">8 AVG</a>");
                client.println("<a class=\"button button-on\" href=\"/read16\">16 AVG</a>");
                break;

              case 3:
                client.print("<a class=\"button button-on\" href=\"/\">OFF</a>");
                client.print("<a class=\"button button-on\" href=\"/read1\">Single</a>");
                client.println("<a class=\"button button-on\" href=\"/read2\">2 AVG</a>");
                client.println("<br>");
                client.print("<a class=\"button button-off\" href=\"/read4\">4 AVG</a>");
                client.print("<a class=\"button button-on\" href=\"/read8\">8 AVG</a>");
                client.println("<a class=\"button button-on\" href=\"/read16\">16 AVG</a>");
                break;

              case 4:
                client.print("<a class=\"button button-on\" href=\"/\">OFF</a>");
                client.print("<a class=\"button button-on\" href=\"/read1\">Single</a>");
                client.println("<a class=\"button button-on\" href=\"/read2\">2 AVG</a>");
                client.println("<br>");
                client.print("<a class=\"button button-on\" href=\"/read4\">4 AVG</a>");
                client.print("<a class=\"button button-off\" href=\"/read8\">8 AVG</a>");
                client.println("<a class=\"button button-on\" href=\"/read16\">16 AVG</a>");
                break;

              case 5:
                client.print("<a class=\"button button-on\" href=\"/\">OFF</a>");
                client.print("<a class=\"button button-on\" href=\"/read1\">Single</a>");
                client.println("<a class=\"button button-on\" href=\"/read2\">2 AVG</a>");
                client.println("<br>");
                client.print("<a class=\"button button-on\" href=\"/read4\">4 AVG</a>");
                client.print("<a class=\"button button-on\" href=\"/read8\">8 AVG</a>");
                client.println("<a class=\"button button-off\" href=\"/read16\">16 AVG</a>");
                break;
            }

            //client.println("</div>");
            client.println("</body></html>");
            client.println(); // The HTTP response ends with another blank line

          }

          // finished with request, empty string
          HTTP_req = "";
          break;
        }

        // every line of text received from the client ends with \r\n
        if (c == '\n') {

          // last character on line of received text
          // starting new line with next character read
          currentLineIsBlank = true;
          HTTP_req = "";
        } else if (c != '\r') {
          // a text character was received from client
          currentLineIsBlank = false;
        }

        // Checks read mode:
        if (HTTP_req.startsWith("GET /"))
          selected = 0;

        if (HTTP_req.startsWith("GET /read1")) {
          selected = 1;
        } else if (HTTP_req.startsWith("GET /read2")) {
          selected = 2;
        } else if (HTTP_req.startsWith("GET /read4")) {
          selected = 3;
        } else if (HTTP_req.startsWith("GET /read8")) {
          selected = 4;
        } else if (HTTP_req.startsWith("GET /read16")) {
          selected = 5;
        }
        Serial.print("Selected mode:");
        Serial.println(selected);

      } // end if (client.available())
    } // end while (client.connected())
    delay(1);      // give the web browser time to receive the data
    client.stop(); // close the connection
  } // end if (client)

}

// Functions ------------------------------------------------------------------------------------------

// Takes all data independent of wifi in case client isn't connected
void recordData() {
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

}

// Reads data from both sensors and stores it in global variable
void takeMeasurements() {
  requestRead(1);
  waitForDone(1);
  sensor1 = requestData(1, &temp1);
  delay(1);
  requestRead(2);
  waitForDone(2);
  sensor2 = requestData(2, &temp2);
}

// Get calibration data from sensors (On startup)
void requestCalib() {
  // Coefficients are the same from both sensors, so we only need them from one
  Serial.println("Tryin' to calibrate");
  int j = 0;
  toggle(1);
  uint16_t words[10];
  for (byte i = 0x2F; i < 0x39; i++) {
    SPISettings sensors(1000000, MSBFIRST, SPI_MODE0); //Both sensors are logic state low
    SPI.beginTransaction(sensors);

    SPI.transfer(i);
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    Serial.println("Transfered request");
    byte outBuff[2];
    SPI.transfer(0xF0);
    outBuff[0] = SPI.transfer(0x00);
    outBuff[1] = SPI.transfer(0x00);
    Serial.println("Received high and low byte");

    // Arduino transmits measurement start command, will receive back status byte and two bytes of "undefined data"
    SPI.endTransaction();

    // Parse bytes into one "word"
    uint16_t x = (outBuff[0] << 8) | (outBuff[1]);

    // Add piece of data to the words
    words[j++] = x;
  }

  toggle(1);
  Serial.println("Exited loop");
  int32_t rawCoeff[4];
  for (int b = 0; b < 4; b++) {
    rawCoeff[b] = (words[(2 * b)] << 16) | (words[(2 * b) + 1]);
    coeff[b] = (float)rawCoeff[b] / (float)0x7FFFFFFF;
  }
Serial.println("Stacked coefficients");
  // Parse coefficient data to get coefficients
  int8_t rawTempCoeff[3] = {((byte)words[8] >> 8), (byte)words[8], ((byte)words[9] >> 8)};
  coeff[4] = (float)rawTempCoeff[0] / 0x7F;
  coeff[5] = (float)rawTempCoeff[1] / 0x7F;
  coeff[6] = (float)rawTempCoeff[2] / 0x7F;

}

void toggle(int sensor) {
  if (sensor == 2) {
    digitalWrite(s2Toggle, !sen1);
    sen1 = !sen1;
  } else if (sensor == 1) {
    digitalWrite(s1Toggle, !sen2);
    sen2 = !sen2;
  }
}

// send the XML file with sensor data
void XML_response(WiFiClient cl) {
  recordData();
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
int requestRead(int tog) {
  // If button is not set to "Off"
  if (selected > 0) {
    Serial.println("Trying to request sensor data");
    uint32_t buff[1] = {((start_comm[selected - 1] << 16) | 0x0000)};
    SPISettings sensors(1000000, MSBFIRST, SPI_MODE0); //Both sensors are logic state low
    SPI.beginTransaction(sensors);
    toggle(tog);
    SPI.transfer(&buff, 3);
    //Arduino transmits measurement start command, will receive back status byte and two bytes of "undefined data"
    toggle(tog);
    SPI.endTransaction();
  }
}

// Allows us to wait for the sensor to be done reading before asking for data back
int waitForDone(int tog) {
    SPISettings sensors(1000000, MSBFIRST, SPI_MODE0); //Both sensors are logic state low

  while (true) {
    Serial.println("In the while loops");
    // If the sensor is still taking data, the bit will be 1.
    // If it is ready, the bit will be zero, and we can read data.
    byte Status = 0xF0;
    SPI.beginTransaction(sensors);
    toggle(tog);

    //Arduino transmits 0xF0, receives back status byte
    byte sensorStatus = SPI.transfer(Status);
    toggle(tog);
    SPI.endTransaction();

    Serial.println(sensorStatus & 0x20);
    if ((sensorStatus & 0x20) == 0) {
      break;
    }
  }
}

// Requests temperature and pressure data back from sensors
float requestData(int tog, float *temperature) {
    SPISettings sensors(1000000, MSBFIRST, SPI_MODE0); //Both sensors are logic state low

  Serial.println("Trying to take sensor data");
  // Get data from SPI lines

  SPI.beginTransaction(sensors);
  toggle(tog);
  byte stat = SPI.transfer(0xF0);
  byte p3 = SPI.transfer(0x00); // (float)
  byte p2 = SPI.transfer(0x00);
  byte p1 = SPI.transfer(0x00);
  byte t3 = SPI.transfer(0x00);
  byte t2 = SPI.transfer(0x00);
  byte t1 = SPI.transfer(0x00);

  // Arduino transmits read command, gets back lots of data (hopefully)
  toggle(tog);
  SPI.endTransaction();

  //Big 'ol calculation (which may not actually be right :/
  int32_t iPraw = ((p3 << 16) | (p2 << 8) | p1) - 0x800000;
  int32_t iTemp = (t3 << 16) | (t2 << 8) | t1;
  float Pnorm = (float)iPraw / 0x7FFFFF;

  float AP3 = coeff[0] * Pnorm * Pnorm * Pnorm;
  float BP2 = coeff[1] * Pnorm * Pnorm;
  float CP = coeff[2] * Pnorm;
  float LCorr = AP3 + BP2 + CP + coeff[3];

  // Compute Temperature - Dependent Adjustment:
  int32_t Tref = (int32_t)(pow(2, 24) * 65 / 125); // Reference Temperature, in sensor counts
  int32_t Tdiff = iTemp - Tref;

  //TC50: Select High/Low, based on sensor temperature reading:
  float TC50;

  if (iTemp > Tref)
    TC50 = coeff[4];
  else
    TC50 = coeff[5];

  float Padj;
  if (Pnorm > 0.5)
    Padj = Pnorm - 0.5;
  else
    Padj = 0.5 - Pnorm;

  // Calculating compensated pressure
  float TCadj = (1.0 - (coeff[6] * 1.25 * Padj)) * Tdiff * TC50;
  float PCorr = Pnorm + LCorr + TCadj;                    // corrected P: float, ±1.0
  int32_t iPCorrected = (int32_t)(PCorr * (float)0x7FFFFF); // corrected P: signed int32
  uint32_t uiPCorrected = (uint32_t)(iPCorrected + 0x800000);

  float pressure = (float)1.25 * ((float)(uiPCorrected - (0.5*pow(2,24)))/ (float)pow(2, 24)) * 2490.889; // convert to Pa

  // Calculating temperature (degrees Celcius)
  *temperature = ((iTemp * 125) / pow(2, 24)) - 40; // Celcius

  return pressure;
}

void SD_write(String current_file) {
  // Creating Data String, see comments at top of code for formatting
  String dataString = String(millis() + "  " + String(sensor1) + "  " + String(sensor2) +
                      "  " + String(temp1) + "  " + String(temp2) + "  " + String(measuredvbat));

  // opening the file
  File dataFile = SD.open(current_file, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  } else {
    Serial.println("error opening file");
  }
}
