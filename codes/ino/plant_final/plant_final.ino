// This code is still under development and it is subject to constant changes.
// The pins defined here are not necessarily the same as shown in the paper.

#include <SD.h> // SD card communication library
#include <SPI.h> // SPI protocol librabry
#include <avr/sleep.h> //this AVR library contains the methods that controls the sleep modes
#include <Wire.h> // I2C protocol library
#include <DS3232RTC.h>  //RTC Library https://github.com/JChristensen/DS3232RTC
#include <MechaQMC5883.h> // Compass module library
#include <OneWire.h> // OneWire protocol library

// To the control pin of the relay module
#define relayPin 8

// Define One Wire Bus
#define owBus 6

// Set Chip Select
#define cs 10

// Pin we are going to use to wake up the Arduino
#define interruptPin 2

// Creates compass object
MechaQMC5883 compass;

// WD2404 - Nema 17 control pins
#define ena17 5 //enable pin
#define dir17 3 //direction pin
#define pul17 4 //pulse pin

// WD2404 - Nema 23 control pins
#define ena23 A0 //enable pin
#define dir23 A3 //direction pin
#define pul23 A1 //pulse pin

// Nema 17 limit switches
#define limit1 7
#define limit2 9

// For panel voltage reading
#define vpan A2

// Calibration of the number of steps. Constant provided by tests.
const uint16_t Nsteps = 542;

// Interrupt counter for event queue
volatile uint8_t intrrCnt; // Interruption counters must always be declared as volatile

// Wake up flag
bool wakingUp;

// Nemas pulse state variables
boolean pulse17, pulse23; // Stores pulse state

// Delay between Nemas pulses
const uint16_t microsec17 = 64000;
const uint16_t microsec23 = 16000;

// Stores number of moves in Nema 17's course
uint16_t nMoves17;

// DS18B20 addresses
OneWire myds(owBus);
const uint8_t dsaddr1[8] = { 0x28, 0xD4, 0x2D, 0x79, 0xA2, 0x01, 0x03, 0xB6 }; // defining temp sensor adresses manually
const uint8_t dsaddr2[8] = { 0x28, 0xA2, 0xA5, 0x79, 0xA2, 0x00, 0x03, 0xAD };
const uint8_t dsaddr3[8] = { 0x28, 0xA6, 0xED, 0x79, 0xA2, 0x01, 0x03, 0x62 };
const uint8_t dsaddr4[8] = { 0x28, 0x0B, 0x84, 0x79, 0xA2, 0x00, 0x03, 0x52 };
const uint8_t dsaddr5[8] = { 0x28, 0x77, 0xB6, 0x91, 0x0B, 0x00, 0x00, 0x7F };
const uint8_t dsaddr6[8] = { 0x28, 0xFF, 0x78, 0x01, 0x04, 0x17, 0x04, 0xE9 };
const uint8_t dsaddr7[8] = { 0x28, 0xA5, 0x3C, 0x79, 0xA2, 0x01, 0x03, 0xA8 };
 
// Stores the relay state
bool relayState;

// Temperature variables
//float t1, t2, t3 ,t4, t5, t6, t7;

// Declaring for both alarms. Alarm 1 is automatic calculated by the Fourier fit model and alarm 2 must be set. Set to 1 minute, because it's the higest resolution in that particular alarm.
uint8_t time_interval1;
const uint8_t time_interval2 = 1; // minutes

// Sunrise and sunset variables
uint16_t Sunrise, Sunset;
float CorrSunrise, CorrSunset;

// SD file global variable
File myFile;

// RTC global variable
time_t t;

//=================START=OF=LOW=POWER=FUNCTIONS=======================

void intrrRoutine(){
  if(wakingUp == true){
    sleep_disable();//Disable sleep mode
    wakingUp = false;
  }
  intrrCnt = intrrCnt + 1;
}

void Going_To_Sleep(){
  wakingUp = true;
  sleep_enable();//Enabling sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);//Setting the sleep mode, in our case full sleep
  sleep_cpu();//activating sleep mode
}

//=================END=OF=LOW=POWER=FUNCTIONS=======================


//=================START=OF=ONEWIRE=BUS=FUNCTIONS=======================

// this function sets the resolution for ALL ds18b20s on an instantiated OneWire
void dssetresolution(OneWire myds, uint8_t resolution)  
{
  myds.reset();            // onewire intialization sequence, to be followed by other commands
  myds.write(0xcc);        // onewire "SKIP ROM" command, selects ALL ds18b20s on bus
  myds.write(0x4e);        // onewire "WRITE SCRATCHPAD" command (requires write to 3 registers: 2 hi-lo regs, 1 config reg)
  myds.write(100);         // 1) write dummy value (100) to temp hi register
  myds.write(0);           // 2) write dummy value (0)to temp lo register
  myds.write(resolution);  // 3) write the selected resolution to configuration registers of all ds18b20s on the bus
}

void dsconvertcommand(OneWire myds) {
  myds.reset();
  myds.write(0xCC, 1);        // send Skip ROOM command, next command will be for all devices, see datasheet
  myds.write(0x44, 1);        // send Convert T command, start conversion
}



float dsreadtemp(OneWire myds, byte addr[8]){
  int i;
  byte data[12];
  byte type_s;
  float celsius;

  myds.reset();
  myds.select(addr);
  myds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++){           // we need 9 bytes
    data[i] = myds.read();
  }
  
  // convert the data to actual temperature

  unsigned int raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // count remain gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    } else {
      byte cfg = (data[4] & 0x60);
      if (cfg == 0x00) raw = raw << 3;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw << 2; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw << 1; // 11 bit res, 375 ms
      // default is 12 bit resolution, 750 ms conversion time
    }
  }
  celsius = (float)raw / 16.0;
  return celsius;
}

//=================END=OF=ONEWIRE=BUS=FUNCTIONS=======================

//=================START=OF=FOURIER=FIT=FUNCTIONS=======================

// Model: fourier fit with 10 parameters

// sunrise_max_error = 0.5151760758451474
int sunrise(int x) {
  return 353.0865554817542 -
    37.73952916675878 * sin(0.017195232694974145 * x + 1.9276619963184738) -
    9.760753743433716 * sin(0.03439046538994829 * x + 3.4005782964879403) +
    0.9502585744624494 * sin(0.05158569808492244 * x + 5.505838036403829) +
    0.21738316023174167 * sin(0.06878093077989658 * x + 6.747924371907783);
}
// sunset_max_error = 0.5530093242593921
int sunset(int x) {
  return 1077.3927516239257 +
    34.13662474160315 * sin(0.017206737183049266 * x + 1.534202414640225) -
    10.10266809670871 * sin(0.03441347436609853 * x + 3.55788201252587) -
    0.8202787799563591 * sin(0.051620211549147796 * x + 4.839335459412188) +
    0.2426777917749307 * sin(0.06882694873219707 * x + 7.189867893341051);
}


//=================END=OF=FOURIER=FIT=FUNCTIONS=======================

//=================START=OF=TIME=MANIPULATION=FUNCTIONS==============

int dayOfTheYear(int day, int mon, int year){  

  int days_in_feb = 28;
  int doy;    // day of year
 
        
  doy = day;

  // check for leap year
  if( (year % 4 == 0 && year % 100 != 0 ) || (year % 400 == 0) ){
    days_in_feb = 29;
  }

  switch(mon){
    case 2:
        doy += 31;
        break;
    case 3:
        doy += 31+days_in_feb;
        break;
    case 4:
        doy += 31+days_in_feb+31;
        break;
    case 5:
        doy += 31+days_in_feb+31+30;
        break;
    case 6:
        doy += 31+days_in_feb+31+30+31;
        break;
    case 7:
        doy += 31+days_in_feb+31+30+31+30;
        break;            
    case 8:
        doy += 31+days_in_feb+31+30+31+30+31;
        break;
    case 9:
        doy += 31+days_in_feb+31+30+31+30+31+31;
        break;
    case 10:
        doy += 31+days_in_feb+31+30+31+30+31+31+30;            
        break;            
    case 11:
        doy += 31+days_in_feb+31+30+31+30+31+31+30+31;            
        break;                        
    case 12:
        doy += 31+days_in_feb+31+30+31+30+31+31+30+31+30;            
        break;                                    
  }
  return doy;
}

void secondsToHMS(int seconds, int *h, int *m, int *s)
{
    int t = seconds;

    *s = t % 60;

    t = (t - *s)/60;
    *m = t % 60;

    t = (t - *m)/60;
    *h = t;
}

float HMSToMin(int Hour, int Minute, int Seconds){
  return (Hour*60 + Minute + Seconds*1.0/60);
}

//=================END=OF=TIME=MANIPULATION=FUNCTIONS==============

//=================START=OF=MOTOR=POSITION=CONTROL=FUNCTIONS=======

int calcPerStep(float x){
  float stepPerSeg = x*60/Nsteps;
  return round(stepPerSeg);
}

void stepNema17(){
  pulse17 = !pulse17;
  digitalWrite(pul17, pulse17);
  delayMicroseconds(microsec17);
  pulse17 = !pulse17;
  digitalWrite(pul17, pulse17);
  delayMicroseconds(microsec17);
}

void origNema17(){
  bool l2 = digitalRead(limit2);
  while(l2 != 1){
    stepNema17();
    l2 = digitalRead(limit2);
  }
}

void stepNema23(){
  pulse23 = !pulse23;
  digitalWrite(pul23, pulse23);
  delayMicroseconds(microsec23);
  pulse23 = !pulse23;
  digitalWrite(pul23, pulse23);
  delayMicroseconds(microsec23);
}

void correctAzimuth(){
  int comp = getCompData();
  if(comp<181){
    digitalWrite(dir23, HIGH); // Low CW, High CCW
    while(comp>0){
      stepNema23();
      comp = getCompData();
      Serial.println(comp);
    }
  }
  else{
    digitalWrite(dir23, LOW); // Low CW, High CCW
    while(comp>0){
      stepNema23();
      comp = getCompData();
      Serial.println(comp);
    }
  }
}

//=================END=OF=PANEL=POSITION=CONTROL=FUNCTIONS=======

//=================START=OF=DATA=READING=FUNCTIONS===============

void tempRead(float *temps){
  dsconvertcommand(myds); // Commands all temp sensors convertion. Respect delay for proper convertion.
  delay(200); // 94ms for 9bit res. / 188ms for 10bit res. / 375ms for 11bit res. / 750ms for 12bit res.
  temps[0] = dsreadtemp(myds, dsaddr1);
  temps[1] = dsreadtemp(myds, dsaddr2);
  temps[2] = dsreadtemp(myds, dsaddr3);
  temps[3] = dsreadtemp(myds, dsaddr4);
  temps[4] = dsreadtemp(myds, dsaddr5);
  temps[5] = dsreadtemp(myds, dsaddr6);
  temps[6] = dsreadtemp(myds, dsaddr7);
}

void evalCooler(float *temps){
  float t1 = temps[0];
  float t2 = temps[1];
  float t3 = temps[2];
  float t4 = temps[3];
  float t5 = temps[4];
  float t6 = temps[5];
  //Describing hysteresis. Set here proper temperatures for each component measured.
  if(!relayState){
    if((t1 > 80) || (t2 > 80) || (t3 > 45) || (t4 > 45) || (t5 > 42) || (t6 > 42)){
      //Relay state definition
      relayState = 1;
      //Relay command
      digitalWrite(relayPin, relayState);
    }
  }
  if(relayState){
    if((t1 < 75) && (t2 < 75) && (t3 < 40) && (t4 < 40) && (t5 < 39) && (t6 < 39)){
      //Relay state definition
      relayState = 0;
      //Relay command
      digitalWrite(relayPin, relayState);
    }
  }
}

int getCompData(){
  int x, y, z, azimuth;
  // Getting compass data
  compass.read(&x,&y,&z);
  azimuth = ((atan2(x, y) - 0.3994)/0.0174532925); // calculate azimuth with correct declination of BH
  //azimuth-=200; // East wanted
  if(azimuth < 0){azimuth+=360;} // we want only positive numbers
  else if(azimuth > 359){azimuth-=360;}
  //if(azimuth < 0){azimuth+=360;}
  azimuth-=200; // East wanted
  if(azimuth < 0){azimuth+=360;}
  return azimuth; // magnetic declination correction and east is wanted
}


int getPirData(){
  Wire.requestFrom(4, 2); // request 2 bytes from slave device address 4
  if(2<=Wire.available()){
    uint16_t PirA = Wire.read(); // MSB
    uint8_t PirB = Wire.read(); // LSB
    return (PirA << 8) + PirB;
  } 
}

float getVPanData(){
  return (analogRead(vpan)/1023) * 5;
}

//=================END=OF=DATA=READING=FUNCTIONS===============

//=================START=OF=SD=PRINTING=FUNCTIONS==============

void printTimeStamp(){
  //Printing timestamp
  myFile.print("[");
  // Send date
  myFile.print(day(t));
  myFile.print("/");
  myFile.print(month(t));
  myFile.print("/");
  myFile.print(year(t));
  myFile.print(" -- ");
  // Send time
  myFile.print(hour(t));
  myFile.print(":");
  myFile.print(minute(t));
  myFile.print(":");
  myFile.print(second(t));
  myFile.print("]");
  myFile.print("  ");  
}

void printDataSD(float *temps, int pirData, int vpanData){
  printTimeStamp();
  //Printing process variables
  myFile.print(nMoves17);
  myFile.print("  ");
  myFile.print(temps[0]);
  myFile.print("  ");
  myFile.print(temps[1]);
  myFile.print("  ");
  myFile.print(temps[2]);
  myFile.print("  ");
  myFile.print(temps[3]);
  myFile.print("  ");
  myFile.print(temps[4]);
  myFile.print("  ");
  myFile.print(temps[5]);
  myFile.print("  ");
  myFile.print(temps[6]);
  myFile.print("  ");
  myFile.print(relayState);
  myFile.print("  ");
  myFile.print(pirData);
  myFile.print("  ");
  myFile.print(vpanData);
  //myFile.print("  ");
  //myFile.println(azimuth);
  myFile.println();
  myFile.flush();  // Saving it.
}

void printTest(){
  printTimeStamp();
  // Print Test
  myFile.println("Hic sunt machinamenta.");
  myFile.println();
  myFile.flush();  // Saving it.
}

//=================END=OF=SD=PRINTING=FUNCTIONS==============


void setup(void){

  Wire.begin(); // Begin I2C communication

  //=========START=OF=RTC=INITIALIZATION=====
  intrrCnt = 0;
  wakingUp = false;
  pinMode(interruptPin,INPUT_PULLUP);//Set pin d5 to input using the buildin pullup resistor
  attachInterrupt(0, intrrRoutine, FALLING);//attaching a interrupt to pin d2
  // initialize the alarms to known values, clear the alarm flags, clear the alarm interrupt flags
  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
  RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarm(ALARM_2);
  RTC.alarmInterrupt(ALARM_1, true);
  RTC.alarmInterrupt(ALARM_2, true);
  RTC.squareWave(SQWAVE_NONE);
  //=========END=OF=RTC=INITIALIZATION=====
  
  //=========START=OF=SD=CARD=INITIALIZATION=====
  pinMode(cs, OUTPUT);
  
  pinMode(SS, OUTPUT);
   
  // see if the card is present and can be initialized:
  if(!SD.begin(cs)){
    while (1);
  }
  myFile = SD.open("PFCII.txt", FILE_WRITE);
  if(myFile){
    myFile.println("============================================");
    myFile.print("Initialized:");
    myFile.print("  ");
    printTimeStamp();
    myFile.println();
    myFile.println("[timestamp] nMoves17, S1, S2, S3, S4, S5, S6, relayState, azimuth");  // Send Your First Line to that file
    myFile.println();
    myFile.flush();  // Save it.
  }
  else{
   while(1); 
  }
  //=========END=OF=SD=CARD=INITIALIZATION=====

  //=========START=OF=RELAY=INITIALIZATION=====
  pinMode(relayPin, OUTPUT); 
  digitalWrite(relayPin, 0);
  relayState = false;
  //=========END=OF=SD=CARD=INITIALIZATION=====

  //=========START=OF=TEMPERATURE=SENSOR=IN  ITIALIZATION=====
  dssetresolution(myds, 10); // According to DSS18B20's datasheet, a resolution of 10 gives us 0.25⁰C precision
  //=========END=OF=TEMPERATURE=SENSOR=INITIALIZATION=====

  //=========START=OF=COMPASS=INITIALIZATION=====
  compass.init();
  //=========END=OF=COMPASS=INITIALIZATION=====

  //=========START=OF=NEMAS=INITIALIZATION=====
  nMoves17 = 0;
  pulse17 = HIGH;
  pulse23 = HIGH;
  pinMode(limit1, INPUT_PULLUP); // Defines as pullup so there is no need to use resistors with the limit switches
  pinMode(limit2, INPUT_PULLUP);
  pinMode(ena17, OUTPUT);
  pinMode(dir17, OUTPUT);
  pinMode(pul17, OUTPUT);
  digitalWrite(ena17, HIGH); // Enables motor
  digitalWrite(dir17, HIGH); // Low CW, High CCW
  digitalWrite(pul17, LOW);
  pinMode(ena23, OUTPUT);
  pinMode(dir23, OUTPUT);
  pinMode(pul23, OUTPUT);
  digitalWrite(ena23, HIGH); // Enables motor
  digitalWrite(dir23, HIGH); // Low CW, High CCW
  digitalWrite(pul23, LOW);
  origNema17();
  delay(1000);
  digitalWrite(ena17, LOW); // Disables motor
  correctAzimuth();
  delay(1000);
  digitalWrite(ena23, HIGH);
  //=========END=OF=NEMAS=INITIALIZATION=====

  //=========START=OF=PANEL=VOLTAGE=READING=INITIALIZATION=====
  pinMode(vpan, INPUT);
  //=========END=OF=PANEL=VOLTAGE=READING=INITIALIZATION=====
  
  t = RTC.get();
  
  int SEC = second(t);
  int MIN = minute(t);
  int HR = hour(t);
  int DAY = day(t);
  int MON = month(t);
  int YEAR = year(t);
  
  float minPassed = HMSToMin(HR, MIN, SEC);
  uint16_t DOY = dayOfTheYear(DAY, MON, YEAR);
  Sunrise = sunrise(DOY);
  Sunset = sunset(DOY);
  uint16_t diff = Sunset-Sunrise;
  CorrSunrise = Sunrise + (17.5/180)*diff;
  CorrSunset = Sunset - (17.5/180)*diff;
  float CorrDiff = CorrSunset - CorrSunrise;
  time_interval1 = calcPerStep(CorrDiff);
  int HRaux, MINaux, SECaux;
  
  if(minPassed < Sunrise){ // Day hasnt started, must not read data yet, set alarm #2 for later
    secondsToHMS((Sunrise*60), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM2_MATCH_HOURS, 0, MINaux, HRaux, 0);
    //Serial.println("Alarme 2 setado para hoje mais tarde.");
    //Serial.println(Sunset);
    //Serial.println(Sunrise);
  }
  else if(minPassed > Sunset){ // Day has ended, must not read data anymore, set alarm #2 for tomorrow
    DOY = dayOfTheYear(DAY+1, MON, YEAR);
    Sunrise = sunrise(DOY);
    Sunset = sunset(DOY);
    secondsToHMS((Sunrise*60), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM2_MATCH_HOURS, 0, MINaux, HRaux, 0);
    //Serial.println("Alarme 2 setado para amanha");
    //Serial.println(Sunset);
    //Serial.println(Sunrise);
  }
  else{ // Day is running, must read data and set alarm #2 for next cicle
    MINaux = MIN+1;
    if(MINaux > 60){MINaux = MINaux - 60;}
    RTC.setAlarm(ALM2_MATCH_MINUTES, 0, MINaux, 0, 0);
    //Serial.println("Iniciou durante o dia. Medindo.");
    //Serial.println(Sunset);
    //Serial.println(Sunrise);
  }
  
  if(minPassed < CorrSunrise){ // Sun isn't in the panel angular bounds yet, set alarm #1 for later
    secondsToHMS(round(CorrSunrise * 60 + 0.5 * time_interval1), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM1_MATCH_HOURS, SECaux, MINaux, HRaux, 0);
    //Serial.println("Alarme 1 setado para hoje mais tarde.");
    //Serial.println(CorrSunset);
    //Serial.println(CorrSunrise);
  }
  else if(minPassed > CorrSunset){ // Sun already left the panel angular bounds, set alarm #1 for tomorrow
    uint16_t DOYaux = dayOfTheYear(DAY+1, MON, YEAR);
    uint16_t SunriseAux = sunrise(DOYaux);
    uint16_t SunsetAux = sunset(DOYaux);
    uint16_t diffAux = SunsetAux-SunriseAux;
    CorrSunrise = SunriseAux + (17.5/180)*diffAux;
    CorrSunset = SunsetAux - (17.5/180)*diffAux;
    float CorrDiffAux = CorrSunset - CorrSunrise;
    time_interval1 = calcPerStep(CorrDiffAux);
    secondsToHMS(round(CorrSunrise * 60 + 0.5 * time_interval1), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM1_MATCH_HOURS, SECaux, MINaux, HRaux, 0);
    //Serial.println("Alarme 1 setado para amanha");
    //Serial.println(CorrSunset);
    //Serial.println(CorrSunrise);
  }
  else{ // Sun is in the panel angular bounds, must recover position and set alarm #1 for next cicle
    digitalWrite(ena17, HIGH);
    digitalWrite(dir17, LOW);
    uint16_t stepsLate = floor((minPassed*1.0-CorrSunrise)*60/time_interval1);
    for(int i=0; i<stepsLate; i++){
      bool l1 = digitalRead(limit1);
      if(l1 != 1){
        stepNema17();
        nMoves17++;
      }
    }
    secondsToHMS(round(CorrSunrise*60 + (stepsLate+1)*time_interval1 + 0.5*time_interval1), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM1_MATCH_HOURS, SECaux, MINaux, HRaux, 0);
    //Serial.println("Iniciou durante o dia. Posição recuperada.");
    //Serial.println(CorrSunset);
    //Serial.println(CorrSunrise);
  }
}

void loop(void){
  Going_To_Sleep(); // Sleeps Arduino
  // Wakes up on interruption
  while(intrrCnt > 0){ // While there's still entries in the event queue, do:
    t = RTC.get();
    int SEC = second(t);
    int MIN = minute(t);
    int HR = hour(t);
    int DAY = day(t);
    int MON = month(t);
    int YEAR = year(t);
    if(RTC.alarm(ALARM_2)){ // if alarm #2 was triggered, clear its flag and execute its functions: tempRead, evalCooler, getCompassData, printDataSD.
      if(HMSToMin(HR, MIN, SEC) > Sunset){ // If sun has set, do
        digitalWrite(relayPin, LOW);
        digitalWrite(ena17, HIGH);
        digitalWrite(dir17, LOW);
        origNema17();
        digitalWrite(ena17, LOW);
        uint16_t DOY = dayOfTheYear(DAY+1, MON, YEAR);
        Sunrise = sunrise(DOY);
        Sunset = sunset(DOY);
        int HRaux, MINaux, SECaux;
        secondsToHMS((Sunrise*60), &HRaux, &MINaux, &SECaux);
        RTC.setAlarm(ALM2_MATCH_HOURS, 0, MINaux, HRaux, 0);
        //Serial.println("Alarme 2 setado para outro dia.");
      }
      else{ // Regular alarm #2 data reading batch
        uint8_t MINaux = MIN + time_interval2;
        if(MINaux > 59){MINaux = MINaux - 60;}
        RTC.setAlarm(ALM2_MATCH_MINUTES , 0, MINaux, 0, 0); // Set New Alarm
        float *temps;
        tempRead(temps);
        evalCooler(temps);
        int pirData = getPirData();
        float vpanData = getVPanData();
        printDataSD(temps, pirData, vpanData);
      }
    }
    if(RTC.alarm(ALARM_1)){ // if alarm #1 was triggered, clear its flag and execute its functions: printTest.
      digitalWrite(ena17, HIGH);
      bool l1 = digitalRead(limit1);
      if(l1 == 1){ // If sun is out of panel angular bounds, do:
        digitalWrite(ena17, LOW);
        uint16_t DOYaux = dayOfTheYear(DAY+1, MON, YEAR);
        uint16_t SunriseAux = sunrise(DOYaux);
        uint16_t SunsetAux = sunset(DOYaux);
        uint16_t diffAux = SunsetAux-SunriseAux;
        CorrSunrise = SunriseAux + (17.5/180)*diffAux;
        CorrSunset = SunsetAux - (17.5/180)*diffAux;
        float CorrDiffAux = CorrSunset - CorrSunrise;
        time_interval1 = calcPerStep(CorrDiffAux);
        int HRaux, MINaux, SECaux;
        secondsToHMS(round(CorrSunrise * 60 + 0.5 * time_interval1), &HRaux, &MINaux, &SECaux);
        RTC.setAlarm(ALM1_MATCH_HOURS, SECaux, MINaux, HRaux, SECaux);
        //Serial.println("Alarme 1 setado para outro dia.");
      }
      else{ // Regular alarm #1 panel position control batch
        if(time_interval1 > 60){
          uint8_t MINaux = MIN + 1;
          uint8_t SECaux = SEC + (time_interval1-60);
          if(SECaux > 59){
            SECaux = SECaux - 60;
            MINaux++;
          }
          RTC.setAlarm(ALM1_MATCH_MINUTES , SECaux, MINaux, 0, 0); // Define here wake up time
        }
        else{
          uint8_t SECaux = second(t) + time_interval1;
          if(SECaux > 59){SECaux = SECaux - 60;}
          RTC.setAlarm(ALM1_MATCH_SECONDS, SECaux, 0, 0, 0); // Set New Alarm
        }
        digitalWrite(ena17, HIGH);
        digitalWrite(dir17, LOW);
        stepNema17();
        nMoves17++;
      }
    }
    intrrCnt = intrrCnt - 1;
  }
}
