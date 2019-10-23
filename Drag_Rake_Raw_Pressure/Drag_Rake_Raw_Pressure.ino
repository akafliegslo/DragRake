#include <Wire.h>
int EOC = 10;
byte data[7];
signed long raw;
float scaled;
long twentyFour = 16777216;
long sixteen = 65536;
int eight = 256;
long partA;
long partB;
long partC;

//control shift L for serial plotter


void setup() {
  pinMode(EOC, INPUT);
  Wire.begin();
}

void loop()
{
  //  Serial.println("hi");
  requestRead();
  waitForEOC();
  requestData();
  delay(50);
}


void requestRead()
{
  Wire.beginTransmission(0x29);
  Wire.write(0xAA);
  Wire.endTransmission();
  //  Serial.println("sorry");
}


void  waitForEOC()
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
          //          Serial.println("Bad");
        }
      }
    }
    else
    {
      //      Serial.println("Good");
    }
  }
}

void requestData()
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
  scaled = (raw / twentyFour);
  Serial.println(raw);
}
