// DRAGRAKE
// Written by Danny Maas, Bennett Diamond and Christian BLoemhof
// Collects pressure transducer data via i2c, processes data to make airspeed readings,
// Creates WiFi network, hosts a webpage that displays this data


// Libraries --------------------------------------------------------------------------------------
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include "FS.h"

// Variables ---------------------------------------------------------------------------------------

// Command Registers (SPI commands for the pressure transducer)
#define Start_Single 0xAA     // Hexadecimal address for single read
#define Start_Average2 0xAC   // Hexadecimal address for average of 2 reads
#define Start_Average4 0xAD   // Hexadecimal address for average of 4 reads
#define Start_Average8 0xAE   // Hexadecimal address for average of 8 reads
#define Start_Average16 0xAF  // Hexadecimal address for average of 16 reads

// Initialing SPI chip select lines
#define s1Toggle 11           // Pin used to toggle sensor 1
#define s2Toggle 12           // Pin used to toggle sensor 2
#define SD_card_CS 10         // Pin used for SD card slave select
bool sen1 = HIGH;             // Initialize everything high 
bool sen2 = HIGH;

// Array of all commands to simplify changing modes (SPI stuff)
byte start_comm[5] =
{ Start_Single,
  Start_Average2,
  Start_Average4,
  Start_Average8,
  Start_Average16
};
int st_mode = 1;              // Command mode selector

// Array of calibration registers
int cal_reg[11] = {47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57};

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
String current_file = "/data.txt"; // start with default name

// WiFi Variables
char ssid[] = "DragRake";         // Network Name
char pass[] = "";                 // Network password

int status = WL_IDLE_STATUS;
IPAddress local_ip(4,20,6,9);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

// Set web server port number to 80 (Change if needed)
WebServer server(80);

// SPI configurations
SPISettings sensors(14000000, MSBFIRST, SPI_MODE0); //Both sensors are logic state low

// Setup -------------------------------------------------------------------------------------------------------
void setup()
{
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

  // Setting Webpage modes
  server.on("/", handle_read(0));
  server.on("/read1", handle_read(1));
  server.on("/read2", handle_read(2));
  server.on("/read4", handle_read(4));
  server.on("/read8", handle_read(8));
  server.on("/read16", handle_read(16));
  server.onNotFound(handle_NotFound);

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
  File dataFile = SD.open(current_file);
  
  while (dataFile // prevents overwriting data by generating a new file if one with the same name already exists
  {
    current_file = String("/data" + String(i) + ".txt");
    i++;
    dataFile = SD.open(current_file);
  }

  String dataHeader = "Time (from starting)  Top Pressure  Bottom Pressure  Top Temp  Bottom Temp  Battery Voltage \r\n";
  writeFile(SD, current_file, dataHeader); // write headers to file

  dataFile.close(); // finish file

}

// Loop --------------------------------------------------------------------------------------------------------
void loop()
{
  // Taking and recording data
  Serial.println("I take data now");
  takeMeasurements();
  measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 6.6;  // Multiply by 2 and 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  SD_write(current_file); // Write values to SD Card 
  
  // Webpage stuff
  server.handleClient();

// Functions -------------------------------------------------------------------------------------------

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
  //Coefficients are the same from both sensors, so we only need them from one
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
// Starts read of pressure and temperature data
int requestRead(int tog)
{
  // If button is not set to "Off"
  if (selected > 0)
  {
    uint8_t buff1 = start_comm[selected - 1] << 16; // WHY IS THIS ONE 16 BITS??
    uint8_t buff2 = 0x00 << 8; 
    uint8_t buff3 = 0x00;
    SPI.beginTransaction(sensors);
    toggle(tog);
    SPI.transfer(&buff1); // ESP32 SPI doesn't support multiple byte transfer
    SPI.transfer(&buff2); // WHY ARE THESE POINTERS?
    SPI.transfer(&buff3);
    //Arduino transmits measurement start command, will receive back status byte and two bytes of "undefined data"
    toggle(tog);
    SPI.endTransaction();

    uint8_t buff = (buff1 | buff2 | buff3); // concatenating bytes
  }
  else
  {
  }
}

// Allows us to wait for the sensor to be done reading before asking for data back
int  waitForDone(int tog)
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
      else {
    }
  }
}

// Requests temperature and pressure data back from sensors
float requestData(int tog, float* temperature)
{
  int32_t iPraw;
  float pressure;
  int32_t iTemp;
  float AP3, BP2, CP, LCorr, PCorr, Padj, TCadj, TC50, Pnorm;
  int32_t Tdiff, Tref, iPCorrected;
  uint32_t uiPCorrected;

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
  if (iTemp > Tref) {
    TC50 = (coeff[4] - 1);
  }
  else {
    TC50 = (coeff[5] - 1);
  }
  if (Pnorm > 0.5) {
    Padj = Pnorm - 0.5;
  }
  else {
    Padj = 0.5 - Pnorm;
  }

  // Calculating compensated pressure
  TCadj = (1.0 - (coeff[6] * 1.25 * Padj)) * Tdiff * TC50;
  PCorr = Pnorm + LCorr + TCadj; // corrected P: float, ±1.0
  iPCorrected = (int32_t)(PCorr * (float)0x7FFFFF); // corrected P: signed int32
  uiPCorrected = (uint32_t) (iPCorrected + 0x800000);
  pressure = ((float)12.5 * ((float)uiPCorrected / (float)pow(2, 23)));
  pressure = pressure - (float) 9.69;

  // Calculating temperature (degrees Celcius)
  *temperature = ((iTemp * 125)/pow(2,24)) - 40; // Celcius

  return pressure;
}

void handle_read(int mode) 
{
  if (mode == 0)
  {
    Serial.println("Not readings sensors");
  } 
  else if (mode == 1)
  {
    Serial.println("Single sensor read");     
  } 
  else 
  {
    Serial.println("Average of " + String(mode) + " readings")
  }
  server.send(200, "text/html", SendHTML(mode)); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
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
  
  // Data Input
  ptr +="<inputs><upper>";
  ptr += String(sensor1);
  ptr +="</upper>";
  ptr +="<lower>";
  ptr += String(sensor2);
  ptr +="</lower>";
  ptr +="<average>";
  ptr += String((sensor1 + sensor2) / (float)2);
  ptr +="</average>";
  ptr +="<battery>";
  ptr +=String(measuredvbat);
  ptr +="</battery>";
  ptr +="</inputs>";

  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";

  // Title
  ptr +="<h1>Drag Rake</h1>\n";
  ptr +="<h3>Akaflieg SLO ESP32 Proto-build</h3>\n";
  ptr +="<!-- EASTER EGG -->";

  // Data displays
  ptr +=("<h2><input class=\"read upp\" id=\"Upper\" value=\"0\"/>Upper Surface &Delta;Pressure (Pa)</h2>";
  ptr +="<h2><input class=\"read low\" id=\"Lower\" value="
  ptr +=String(sensor2); // atempting to see if this works
  ptr +="/>Lower Surface &Delta;Pressure (Pa)</h2>";
  ptr +="<h2><input class=\"read avg\"id=\"Average\" value=\"0\"/>Average &Delta;Pressure (Pa)</h2>";
  ptr +="<h2><input class=\"read batt\"id=\"Battery\" value=\"0\"/>Battery Remaining (%)</h2>";

  // Buttons
  ptr +="<h3>Sensor Read Modes</h3>\n";
  switch (mode)
  {
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

  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr; 
}

void SD_write(String current_file)
{
  // Creating Data String, see top for formatting
  String dataString = String(millis()) + "  " + String(sensor1) + "  " + String(sensor2) + 
  "  " + String(temp1) + "  " + String(temp2) + "  " + String(measuredvbat) + "\r\n";

  // open and apend data to the file
  appendFile(SD, current_file, dataString.c_str());
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