// Sketch to get compass azimuth. It was needed to evaluate the compass output interference with other circuits.

#include <Wire.h> // I2C protocol library
#include <MechaQMC5883.h>

MechaQMC5883 compass;
int16_t x, y, z, azimuth;

void setup(){
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);
  Serial.begin(9600);
  Wire.begin();
  compass.init();
  x = 0;
  y = 0;
  z = 0;
  azimuth = 0;
}

void loop(){
  compass.read(&x,&y,&z);
  azimuth = ((atan2(x, y) - 0.3994)/0.0174532925); // calculate azimuth with correct declination of BH
  if(azimuth < 0){azimuth+=360;} // we want only positive numbers
  Serial.println(azimuth);
  delay(200);
}
