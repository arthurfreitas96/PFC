// Code that allows Attiny85 to transfer data do Arduino via I2C.

#define I2C_SLAVE_ADDRESS 0x4 // Address of the slave
 
#include <TinyWireS.h>

#define ldr1 A3 // LDR + 1k
#define ldr2 A0 // LDR + 100
#define ldr3 A2 // LDR + 500
 
uint16_t medidaLdr1;
uint16_t medidaLdr2;
uint16_t medidaLdr3;
uint16_t output;
float v1;
float v2;
float v3;
uint8_t myArray[2];
 
void setup()
{
    TinyWireS.begin(I2C_SLAVE_ADDRESS); // join i2c network
    //TinyWireS.onReceive(receiveEvent); // not using this
    TinyWireS.onRequest(requestEvent);
    medidaLdr1 = 0;
    medidaLdr2 = 0;
    medidaLdr3 = 0;
    output = 0;
}
 
void loop()
{
    // This needs to be here
    TinyWireS_stop_check();
}
 
// Gets called when the ATtiny receives an i2c request
void requestEvent()
{
    medidaLdr1 = analogRead(ldr1);
    medidaLdr2 = analogRead(ldr2);
    medidaLdr3 = analogRead(ldr3);
    v1 = (medidaLdr1 * 5)/1023.0;
    v2 = (medidaLdr2 * 5)/1023.0;
    v3 = (medidaLdr3 * 5)/1023.0;
    if(2.75 < v1 && v1 < 4.76){
      output = round(47.5*pow(v1,2) - 4.4*pow(10,2)*v1 + 1048); 
    }
    else if(0.38 < v2 && v2 < 0.61){
      output = round(5.5*pow(10,3)*pow(v2,2) - 6.3*pow(10,3)*v2 + 2*pow(10,3));
    }
    else if(0.32 < v2 && v2 < 0.39){
      output = round(1.93*pow(10,4)*pow(v2,2) - 1.7*pow(10,4)*v2 + 4*pow(10,3));
    }
    else if(0.29 < v2 && v2 < 0.33){
      output = round(4.5*pow(10,4)*pow(v2,2) - 3.3*pow(10,4)*v2 + 6.5*pow(10,3));
    }
    else if(0.37 < v3 && v3 < 0.42){
      output = round(4.8*pow(10,4)*pow(v3,2) - 4.2*pow(10,4)*v3 + 9999.7);
    }
    else output = 0;
    myArray[0] = (output >> 8) & 0xFF;
    myArray[1] = output & 0xFF;
    TinyWireS.write(myArray[0]);
    TinyWireS.write(myArray[1]);
}
