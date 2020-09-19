// DRAGRAKE
// Written by Danny Maas, Bennett Diamond and Aria Diamond
// Collects pressure transducer data to an SD card via SPI
// Creates WiFi network, hosts a webpage that displays said data


// Libraries --------------------------------------------------------------------------------------
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include "FS.h"

// Variables ---------------------------------------------------------------------------------------

// Command Registers (SPI commands for the pressure transducer)
#define Start_Single 0xAA    // Hexadecimal address for single read
#define Start_Average2 0xAC  // Hexadecimal address for average of 2 reads
#define Start_Average4 0xAD  // Hexadecimal address for average of 4 reads
#define Start_Average8 0xAE  // Hexadecimal address for average of 8 reads
#define Start_Average16 0xAF // Hexadecimal address for average of 16 reads

// Array of all commands to simplify changing modes
byte start_comm[5] =
  {Start_Single,
  Start_Average2,
  Start_Average4,
  Start_Average8,
  Start_Average16};
int mode = 1;              // Command mode selector

// Initialing SPI chip select lines
#define s1Toggle 11   // Pin used to toggle sensor 1
#define s2Toggle 12   // Pin used to toggle sensor 2
#define SD_card_CS 10 // Pin used for SD card slave select
bool sen1 = HIGH;     // Initialize everything high 
bool sen2 = HIGH;

// Variables used to measure battery voltage
#define VBATPIN A7
float measuredvbat;

// Variables used in calculations
float coeff[7];
float sensor1;
float sensor2;
float temp1;
float temp2;

// SD Card Variables
String currentFileString = "/data.txt"; // start with default name
const char* currentFile = "/data.txt";

// WiFi Variables
char ssid[] = "DragRake";         // Network Name
char pass[] = "";                 // Network password
String XML_values;                // XML data structure

int status = WL_IDLE_STATUS;
IPAddress local_ip(4,20,6,9);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

// Set web server port number to 80 (Change if needed)
WebServer server(80);

// SPI configurations
SPISettings sensors(1000000, MSBFIRST, SPI_MODE0); //Both sensors are logic state low

// Setup -------------------------------------------------------------------------------------------------------
void setup() {
  // Set one sensor to active, the other to off
  pinMode(s1Toggle, OUTPUT);
  pinMode(s2Toggle, OUTPUT);

  Serial.begin(115200);
  Serial.print("aaaaaaaaa"); // debug

  SPI.begin(); // moved ahead of setting up wifi, makes a difference?
  requestCalib();

  // Starting WiFi Network
  WiFi.softAP(ssid);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  // Initializing webpages
  server.on("/", handleNoRead);
  server.on("/read1", handleRead1);
  server.on("/read2", handleRead2);
  server.on("/read4", handleRead4);
  server.on("/read8", handleRead8);
  server.on("/read16", handleRead16);
  server.on("/update", handleUpdate);
  server.onNotFound(handleNotFound);

  server.begin();

  // Initializing Sd card interface
  SD.begin(SD_card_CS);
  if (!SD.begin(SD_card_CS))
  {
    Serial.println("SD Card Failure");
  //  while(1);
  }

  // Creating unique file name to avoid overwriting existing files
  int i; // declare i for while loop
  File dataFile = SD.open(currentFile);
  
  while (dataFile) // prevents overwriting data by generating a new file if one with the same name already exists
  {
    currentFileString = String("/data" + String(i) + ".txt");
    i++;
    const char* currentFile = currentFileString.c_str();
    dataFile = SD.open(currentFile);
  }

  const char* dataHeader = "Time (from starting)  Top Pressure  Bottom Pressure  Top Temp  Bottom Temp  Battery Voltage \r\n";
  writeFile(SD, currentFile, dataHeader); // write headers to file

  dataFile.close(); // finish file

}

// Loop --------------------------------------------------------------------------------------------------------
void loop() {
  // Taking and recording data
  Serial.println("I take data now");
  takeMeasurements();
  measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 6.6;  // Multiply by 2 and 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  SD_write(currentFile); // Write values to SD Card 
  
  // Webpage stuff
  server.handleClient();
}

// Functions -------------------------------------------------------------------------------------------

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
  Serial.println("Tryin' to calibrate");
  int j = 0;
  uint16_t words[10];
  for (byte i = 0x2F; i < 0x39; i++) {
    SPI.beginTransaction(sensors);
    toggle(1);
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
    toggle(1);
    SPI.endTransaction();

    // Parse bytes into one "word"
    uint16_t x = (outBuff[0] << 8) | (outBuff[1]);

    // Add piece of data to the words
    words[j++] = x;
  }

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
// Starts read of pressure and temperature data
int requestRead(int tog) {
  // If button is not set to "Off"
  if (mode > 0)
  {
    uint8_t buff1 = start_comm[mode - 1]; // removed bit shift
    uint8_t buff2 = 0x00; 
    uint8_t buff3 = 0x00;
    SPI.beginTransaction(sensors);
    toggle(tog);
    buff1 = SPI.transfer(buff1); 
    buff2 = SPI.transfer(buff2); 
    buff3 = SPI.transfer(buff3);
    //Arduino transmits measurement start command, will receive back status byte and two bytes of "undefined data"
    toggle(tog);
    SPI.endTransaction();

    uint8_t buff = (buff1 << 16 | buff2 << 8 | buff3); // concatenating bytes
  }
}

// Allows us to wait for the sensor to be done reading before asking for data back
int  waitForDone(int tog) {

  while (true) {
    Serial.println("In the while loops");
    // If the sensor is still taking data, the bit will be 1.
    // If it is ready, the bit will be zero, and we can read data.
    const byte Status = 0xF0;
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
  Serial.println("Out of while loop");
}

// Requests temperature and pressure data back from sensors
float requestData(int tog, float *temperature) {
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

  if (iTemp > Tref) {
    TC50 = coeff[4];
  } else {
    TC50 = coeff[5];
  }

  float Padj;
  if (Pnorm > 0.5) {
    Padj = Pnorm - 0.5;
  } else {
    Padj = 0.5 - Pnorm;
  }

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

void handleNoRead() {
  Serial.println("Not reading sensors");
  server.send(200, "text/html", SendHTML(mode)); 
}

void handleRead1() {
  Serial.println("Single sensor read");   
  server.send(200, "text/html", SendHTML(mode));
}

void handleRead2() {
  Serial.println("Average of 2 readings");
  server.send(200, "text/html", SendHTML(mode)); 
}

void handleRead4() {
  Serial.println("Average of 2 readings");
  server.send(200, "text/html", SendHTML(mode)); 
}

void handleRead8() {
  Serial.println("Average of 2 readings");
  server.send(200, "text/html", SendHTML(mode)); 
}

void handleRead16() {
  Serial.println("Average of 16 readings");
  server.send(200, "text/html", SendHTML(mode)); 
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void handleUpdate() {
  // Creating XML values to send data to webpage
  XML_values  ="<inputs><upper>";
  XML_values += String(sensor1);
  XML_values +="</upper>";
  XML_values +="<lower>";
  XML_values += String(sensor2);
  XML_values +="</lower>";
  XML_values +="<average>";
  XML_values += String((sensor1 + sensor2) / (float)2);
  XML_values +="</average>";
  XML_values +="<battery>";
  XML_values +=String(measuredvbat);
  XML_values +="</battery>";
  XML_values +="</inputs>";

  server.send(200, "text/plain", XML_values);
}

String SendHTML(int mode)
{
  //HTML Setup Stuff
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>Drag Rake Controls</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  
  // Button Characteristics
  ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;";
  ptr +="text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #3498db;}\n";
  ptr +=".button-on:active {background-color: #2980b9;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";

  // Data Readout Characteristics
  ptr +=".read {display: block; height: 64 px; width: 128px; background-color: #FFFFFF; ";
  ptr +="color: white; padding: 13px 30px; font-size: 32px; margin: 0px auto 35px; cursor: pointer; border-radius: 4px;}\n";
  
  //Create each data button (non-clickable): MIGHT NOT BE NEEDED
  ptr +=".upp {border-color: #000000;color: black;}";
  ptr +=".low {border-color: #000000;color: black;}";
  ptr +=".avg {border-color: #000000;color: black;}";
  ptr +=".batt {border-color: #000000;color: black;}";  

  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";

  // Title
  ptr +="<div>";
  ptr +="<h1>Drag Rake</h1>\n";
  ptr +="<h3>Akaflieg SLO ESP32 Proto-build</h3>\n";
  ptr +="<!-- EASTER EGG -->";

  // Data displays
  ptr +="<h2><input class=\"read upp\" id=\"Upper\" value=\"0\"/>Upper Surface &Delta;Pressure (Pa)</h2>";
  ptr +="<h2><input class=\"read low\" id=\"Lower\" value=\"0\"/>Lower Surface &Delta;Pressure (Pa)</h2>";
  ptr +="<h2><input class=\"read avg\"id=\"Average\" value=\"0\"/>Average &Delta;Pressure (Pa)</h2>";
  ptr +="<h2><input class=\"read batt\"id=\"Battery\" value=\"0\"/>Battery Remaining (%)</h2>";
  ptr +="</div>";

  // Buttons
  ptr +="<div>";
  ptr +="<h3>Sensor Read Modes</h3>\n";
  switch (mode) {
    case 0:
    ptr +="<a class=\"button button-off\" href=\"/\">OFF</a>";
    ptr +="<a class=\"button button-on\" href=\"/read1\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read2\">ON</a>\n";
    ptr +="<a class=\"button button-on\" href=\"/read4\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read8\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read16\">ON</a>\n";
    break;

    case 1:
    ptr +="<a class=\"button button-on\" href=\"/\">ON</a>";
    ptr +="<a class=\"button button-off\" href=\"/read1\">OFF</a>";
    ptr +="<a class=\"button button-on\" href=\"/read2\">ON</a>\n";
    ptr +="<a class=\"button button-on\" href=\"/read4\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read8\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read16\">ON</a>\n";
    break;

    case 2:
    ptr +="<a class=\"button button-on\" href=\"/\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read1\">ON</a>";
    ptr +="<a class=\"button button-off\" href=\"/read2\">OFF</a>\n";
    ptr +="<a class=\"button button-on\" href=\"/read4\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read8\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read16\">ON</a>\n";
    break;

    case 4:
    ptr +="<a class=\"button button-on\" href=\"/\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read1\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read2\">ON</a>\n";
    ptr +="<a class=\"button button-off\" href=\"/read4\">OFF</a>";
    ptr +="<a class=\"button button-on\" href=\"/read8\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read16\">ON</a>\n";
    break;

    case 8:
    ptr +="<a class=\"button button-on\" href=\"/\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read1\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read2\">ON</a>\n";
    ptr +="<a class=\"button button-on\" href=\"/read4\">ON</a>";
    ptr +="<a class=\"button button-off\" href=\"/read8\">OFF</a>";
    ptr +="<a class=\"button button-on\" href=\"/read16\">ON</a>\n";
    break;

    case 16:
    ptr +="<a class=\"button button-on\" href=\"/\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read1\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read2\">ON</a>\n";
    ptr +="<a class=\"button button-on\" href=\"/read4\">ON</a>";
    ptr +="<a class=\"button button-on\" href=\"/read8\">ON</a>";
    ptr +="<a class=\"button button-off\" href=\"/read16\">OFF</a>\n";
    break;
  }
  ptr +="</div>";

  // AJAX Script (move to top of document if possible just for organization)
  ptr = "<script>";
  ptr = "setInterval(function() {"; // Call a function repetatively with 2 Second interval
  ptr = "getData();}, 200);"; // 200 millis update rate
  ptr = "function getData() {";
  ptr = "var xhttp = new XMLHttpRequest();";
  ptr = "xhttp.onreadystatechange = function() {";
  ptr = "  if (this.readyState == 4 && this.status == 200) {";
  ptr = "   document.getElementById(\"Upper\").value =this.responseXML.getElementsByTagName('upper')[0].childNodes[0].nodeValue;";
  ptr = "   document.getElementById(\"Lower\").value =this.responseXML.getElementsByTagName('lower')[0].childNodes[0].nodeValue;";
  ptr = "   document.getElementById(\"Average\").value =this.responseXML.getElementsByTagName('average')[0].childNodes[0].nodeValue;";
  ptr = "   document.getElementById(\"Battery\").value =this.responseXML.getElementsByTagName('battery')[0].childNodes[0].nodeValue;";
  ptr = "  }";
  ptr = "};";
  ptr = "xhttp.open(""GET"", ""update"", true);";
  ptr = "xhttp.send();";
  ptr = "}";
  ptr = "</script>";

  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr; 
}

void SD_write(const char* currentFile)
{
  // Creating Data String, see top for formatting
  String dataString = String(millis()) + "  " + String(sensor1) + "  " + String(sensor2) + 
  "  " + String(temp1) + "  " + String(temp2) + "  " + String(measuredvbat) + "\r\n";
  const char* data = dataString.c_str();

  // open and apend data to the file
  appendFile(SD, currentFile, data);
}

// Weird ESP32 SD card functions (DON'T TOUCH THESE) -----------------------------------------------

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}
