// Code that allows Arduino to hear the data transfered by Attiny85 via I2C.

#include <Wire.h>
uint16_t a;
uint16_t b;

void setup()
{
 Wire.begin(); // join i2c bus (address optional for master)
 Serial.begin(9600); // start serial for output;
 a = 0;
 b = 0;
}
 
void loop()
{
 Wire.requestFrom(4, 2); // request 1 byte from slave device address 4
 
//while(Wire.available()){ // slave may send less than requested
if(2<=Wire.available()){
 a = Wire.read(); // MSB
 b = Wire.read(); // LSB
 Serial.println((a << 8) + b); // print the character
 }
 
delay(1000);
}
