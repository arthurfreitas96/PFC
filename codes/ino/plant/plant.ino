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
#define ena 5 //enable pin
#define dir 3 //direction pin
#define pul 4 //pulse pin
// Nema 17 limit switches
#define limit11 7
#define limit12 9

const uint16_t Nsteps = 544;

// Interrupt counter for event queue
uint8_t intrrCnt;

// Wake up flag
bool wakingUp;

// Nema 17's variables
bool l11; // To store Nema 17's limit switches value
bool l12;
boolean pulse17; // Stores pulse state

// Delay between Nema 17's pulses
const uint16_t microsec17 = 16000;

// Stores number of moves in Nema 17's course
uint16_t nMoves17;

OneWire myds(owBus);
uint8_t resolution;
uint8_t dsaddr1[8] = { 0x28, 0xD4, 0x2D, 0x79, 0xA2, 0x01, 0x03, 0xB6 }; // defining temp sensor adresses manually
uint8_t dsaddr2[8] = { 0x28, 0xA2, 0xA5, 0x79, 0xA2, 0x00, 0x03, 0xAD };
uint8_t dsaddr3[8] = { 0x28, 0xA6, 0xED, 0x79, 0xA2, 0x01, 0x03, 0x62 };
uint8_t dsaddr4[8] = { 0x28, 0x0B, 0x84, 0x79, 0xA2, 0x00, 0x03, 0x52 };
uint8_t dsaddr5[8] = { 0x28, 0x77, 0xB6, 0x91, 0x0B, 0x00, 0x00, 0x7F };
uint8_t dsaddr6[8] = { 0x28, 0xFF, 0x78, 0x01, 0x04, 0x17, 0x04, 0xE9 };
//Sensor 1 : 0x28, 0xD4, 0x2D, 0x79, 0xA2, 0x01, 0x03, 0xB6
//Sensor 2 : 0x28, 0xA2, 0xA5, 0x79, 0xA2, 0x00, 0x03, 0xAD
//Sensor 3 : 0x28, 0xA6, 0xED, 0x79, 0xA2, 0x01, 0x03, 0x62
//Sensor 4 : 0x28, 0x0B, 0x84, 0x79, 0xA2, 0x00, 0x03, 0x52
//Sensor 5 : 0x28, 0x77, 0xB6, 0x91, 0x0B, 0x00, 0x00, 0x7F
//Sensor 6 : 0x28, 0xFF, 0x78, 0x01, 0x04, 0x17, 0x04, 0xE9
 
// Stores the relay state
bool relayState;

// Stores temperature reading
bool TempRead;

// For bug fixing. Bug #1: On the first run it always turns on coolers, because all sensors return 25. IDK.

bool firstRun;

// a File Object
File myFile;

// Temperature variables
float t1, t2, t3 ,t4, t5, t6;

// Compass vaiables
int16_t x, y, z, azimuth;

// Define here wake up frequency
uint8_t time_interval1; // seconds

// Define here wake up frequency
const uint8_t time_interval2 = 1; // minutes

uint8_t HR, MIN, SEC, MON;

int HRaux, MINaux, SECaux;

uint16_t DAY, YEAR, DOY;

uint16_t Sunrise, Sunset, diff, stepsLate;

float CorrSunrise, CorrSunset, CorrDiff, minPassed;

//create a temporary time variable so we can set the time and read the time from the RTC
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

//=================START=OF=FOURIER=FIT=FUNCTIONS=======================

// sunrise_max_error = 1.9399212031896127
int sunrise(int x) {
  return 352.7057070699647 -
    37.874423615159515 * sin(0.016993372856748738 * x + 1.9702732100174332) -
    9.635480273542653 * sin(0.033986745713497475 * x + 3.5210526762558465);
}
// sunset_max_error = 1.6480860263854993
int sunset(int x) {
  return 1077.7684206276278 +
    34.412579844382826 * sin(0.017048572799752065 * x + 1.5651489841528505) -
    9.938477290115724 * sin(0.03409714559950413 * x + 3.6094381149503607);
}

//=================END=OF=FOURIER=FIT=FUNCTIONS=======================

//=================START=======================

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

int calcPerStep(float x){
  float stepPerSeg = x*60/Nsteps;
  return round(stepPerSeg);
}

int HMSToSeg(int Hour, int Minute, int Seconds){
  return Hour*3600 + Minute*60 + Seconds;
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

//=================END=======================

//=================START=OF=NEW=FUNCTIONS=======================

void tempRead(){
  dsconvertcommand(myds); // Commands all temp sensors convertion. Respect delay for proper convertion.
  delay(200); // 94ms for 9bit res. / 188ms for 10bit res. / 375ms for 11bit res. / 750ms for 12bit res.
  t1 = dsreadtemp(myds, dsaddr1);
  t2 = dsreadtemp(myds, dsaddr2);
  t3 = dsreadtemp(myds, dsaddr3);
  t4 = dsreadtemp(myds, dsaddr4);
  t5 = dsreadtemp(myds, dsaddr5);
  t6 = dsreadtemp(myds, dsaddr6);
}

void evalCooler(){
  //if(firstRun) firstRun = !firstRun; // Bug #1 fixing
  //else{
  //Describing hysteresis. Set here proper temperatures for each component measured.
  if(!relayState){
    if((((t1 > 23.5)) || ((t2 > 23.5)) || ((t3 > 23.5)) || ((t4 > 23.5)) || ((t5 > 23.5)) || ((t6 > 23.5)))){
      //Relay state inversion
      relayState = 1;
      //Relay command
      digitalWrite(relayPin, relayState);
    }
  }
  if(relayState){
    if((((t1 < 22.5)) && ((t2 < 22.5)) && ((t3 < 22.5)) && ((t4 < 22.5)) && ((t5 < 22.5)) && ((t6 < 22.5)))){
      //Relay state inversion
      relayState = 0;
      //Relay command
      digitalWrite(relayPin, relayState);
    }
  }
}

void getCompassData(){
  // Getting compass data
  compass.read(&x,&y,&z);
  azimuth = ((atan2(x, y) - 0.3994)/0.0174532925); // calculate azimuth with correct declination of BH
  if(azimuth < 0) azimuth+=360; // we want only positive numbers
}

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

void printDataSD(){
  printTimeStamp();
  //Printing process variables
  myFile.print(nMoves17);
  myFile.print("  ");
  myFile.print(t1);
  myFile.print("  ");
  myFile.print(t2);
  myFile.print("  ");
  myFile.print(t3);
  myFile.print("  ");
  myFile.print(t4);
  myFile.print("  ");
  myFile.print(t5);
  myFile.print("  ");
  myFile.print(t6);
  myFile.print("  ");
  myFile.print(relayState);
  myFile.print("  ");
  myFile.println(azimuth);
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

//=================END=OF=NEW=FUNCTIONS=======================

//=================START=OF=NEMA17=FUNCTIONS=======================

void stepNema17(){
  pulse17 = !pulse17;
  digitalWrite(pul, pulse17);
  delayMicroseconds(microsec17);
  pulse17 = !pulse17;
  digitalWrite(pul, pulse17);
  delayMicroseconds(microsec17);
  nMoves17++;
}

//=================START=OF=NEMA17=FUNCTIONS=======================

void setup(void){

  Wire.begin(); // Begin I2C communication

  //=========START=OF=RTC=INITIALIZATION=====
  pinMode(interruptPin,INPUT_PULLUP);//Set pin d5 to input using the buildin pullup resistor
  intrrCnt = 0;
  attachInterrupt(0, intrrRoutine, FALLING);//attaching a interrupt to pin d2
  // initialize the alarms to known values, clear the alarm flags, clear the alarm interrupt flags
  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
  RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarm(ALARM_2);
  RTC.alarmInterrupt(ALARM_1, true);
  RTC.alarmInterrupt(ALARM_2, true);
  RTC.squareWave(SQWAVE_NONE);
  //t=RTC.get();
  //if(time_interval1 > 60){
  //  MINaux = minute(t) + 1;
  //  SECaux = second(t) + (time_interval1-60);
  //  if(SECaux> 59){
  //    SECaux = SECaux - 60;
  //    MINaux++;
  //  }
  //  RTC.setAlarm(ALM1_MATCH_MINUTES , SECaux, MINaux, 0, 0); // Define here wake up time
  //}
  //else{
  //  SECaux = second(t) + time_interval1;
  //  if(SECaux > 59){
  //    SECaux = SECaux - 60;
  //  }
  //  RTC.setAlarm(ALM1_MATCH_SECONDS , SECaux, 0, 0, 0); // Define here wake up time
  //}
  //MINaux = minute(t) + time_interval2;
  //if(MINaux > 59){
  //  MINaux = MINaux - 60;
  //}
  //RTC.setAlarm(ALM2_MATCH_MINUTES, 0, MINaux, 0, 0); // Define here wake up time
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

  //=========START=OF=TEMPERATURE=SENSOR=INITIALIZATION=====
  TempRead = false;
  resolution = 10; // min 9 max 12, if you change this, make sure to change the delay in loop too
  dssetresolution(myds, resolution);
  //firstRun = true;
  //=========END=OF=TEMPERATURE=SENSOR=INITIALIZATION=====

  //=========START=OF=COMPASS=INITIALIZATION=====
  compass.init();
  x = 0;
  y = 0;
  z = 0;
  azimuth = 0;
  //=========END=OF=COMPASS=INITIALIZATION=====

  //=========START=OF=NEMA17=INITIALIZATION=====
  l11 = false;
  l12 = false;
  nMoves17 = 0;
  pulse17 = HIGH;
  pinMode(limit11, INPUT_PULLUP); // Defines as pullup so there is no need to use resistors with the limit switches
  pinMode(limit12, INPUT_PULLUP);
  pinMode(ena, OUTPUT);
  pinMode(dir, OUTPUT);
  pinMode(pul, OUTPUT);
  digitalWrite(ena, LOW); // Enables motor
  digitalWrite(dir, LOW); // Low CCW, High CW
  digitalWrite(pul, LOW);
  l11 = digitalRead(limit11);
  while(limit11 != 1){
    stepNema17();
    l11 = digitalRead(limit11);
  }
  //=========START=OF=NEMA17=INITIALIZATION=====

  //=========START=OF=OTHER=VARIABLES=====
  wakingUp = false; // Waking up flag
  MINaux = 0; // To manipulate minutes in the turning of an hour
  //=========END=OF=OTHER=VARIABLES=====

  t=RTC.get();
  SEC = second(t);
  MIN = minute(t);
  HR = hour(t);
  DAY = day(t);
  MON = month(t);
  YEAR = year(t);
  minPassed = HMSToSeg(HR, MIN, SEC)*1.0/60;
  DOY = dayOfTheYear(DAY, MON, YEAR);
  Sunrise = sunrise(DOY);
  Sunset = sunset(DOY);
  diff = Sunset-Sunrise;
  CorrSunrise = Sunrise + (17.5/180)*diff;
  CorrSunset = Sunset - (17.5/180)*diff;
  CorrDiff = CorrSunset - CorrSunrise;
  time_interval1 = calcPerStep(CorrDiff);
  if(minPassed < Sunrise){
    secondsToHMS((Sunrise*60), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM2_MATCH_HOURS, 0, MINaux, HRaux, 0);
    myFile.println("Alarme 2 setado para hoje mais tarde.");
  }
  else if(minPassed > Sunset){
    DOY = dayOfTheYear(DAY+1, MON, YEAR);
    Sunrise = sunrise(DOY);
    Sunset = sunset(DOY);
    diff = Sunset-Sunrise;
    CorrSunrise = Sunrise + (17.5/180)*diff;
    CorrSunset = Sunset - (17.5/180)*diff;
    CorrDiff = CorrSunset - CorrSunrise;
    time_interval1 = calcPerStep(CorrDiff);
    secondsToHMS((Sunrise*60), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM2_MATCH_HOURS, 0, MINaux, HRaux, 0);
    myFile.println("Alarme 2 setado para amanha");
  }
  else{
    MINaux = MIN+1;
    if(MINaux > 60){MINaux = MINaux - 60;}
    RTC.setAlarm(ALM2_MATCH_MINUTES, 0, MINaux, 0, 0);
    myFile.println("Iniciou durante o dia. Medindo.");
  }
  
  if(minPassed < CorrSunrise){
    secondsToHMS(round(CorrSunrise * 60 + 0.5 * time_interval1), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM1_MATCH_HOURS, SECaux, MINaux, HRaux, 0);
    myFile.println("Alarme 1 setado para hoje mais tarde.");
  }
  else if(minPassed > CorrSunset){
    uint16_t DOYaux = dayOfTheYear(DAY+1, MON, YEAR);
    uint16_t SunriseAux = sunrise(DOYaux);
    uint16_t SunsetAux = sunset(DOYaux);
    uint16_t diffAux = SunsetAux-SunriseAux;
    float CorrSunriseAux = SunriseAux + (17.5/180)*diffAux;
    float CorrSunsetAux = SunsetAux - (17.5/180)*diffAux;
    float CorrDiffAux = CorrSunsetAux - CorrSunriseAux;
    uint8_t time_interval1Aux = calcPerStep(CorrDiffAux);
    secondsToHMS(round(CorrSunriseAux * 60 + 0.5 * time_interval1Aux), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM1_MATCH_HOURS, SECaux, MINaux, HRaux, 0);
    myFile.println("Alarme 1 setado para amanha"); 
  }
  else{
    digitalWrite(ena, HIGH);
    stepsLate = floor((minPassed*1.0-CorrSunrise)*60/time_interval1);
    for(int i=0; i<stepsLate; i++){
      stepNema17();
    }
    secondsToHMS(round(CorrSunrise*60 + (stepsLate+1)*time_interval1 + 0.5*time_interval1), &HRaux, &MINaux, &SECaux);
    RTC.setAlarm(ALM1_MATCH_HOURS, SECaux, MINaux, HRaux, 0);
    myFile.println("Iniciou durante o dia. Recuperando posição.");
  }
}

void loop(void){
  Going_To_Sleep();
  while(intrrCnt > 0){
    t = RTC.get();
    SEC = second(t);
    MIN = minute(t);
    HR = hour(t);
    DAY = day(t);
    MON = month(t);
    YEAR = year(t);
    if(RTC.alarm(ALARM_2)){ // if alarm #1 was triggered, clear its flag and execute its functions: tempRead, evalCooler, getCompassData, printDataSD.
      if(HMSToSeg(HR, MIN, SEC) > (Sunset*60)){
        digitalWrite(ena, HIGH);
        digitalWrite(dir, LOW);
        l11 = digitalRead(limit11);
        while(l11 != 1){
          stepNema17();
          l11 = digitalRead(limit11);
        }
        digitalWrite(ena, LOW);
        DOY = dayOfTheYear(DAY+1, MON, YEAR);
        Sunrise = sunrise(DOY);
        Sunset = sunset(DOY);
        diff = Sunset-Sunrise;
        CorrSunrise = Sunrise + (17.5/180)*diff;
        CorrSunset = Sunset - (17.5/180)*diff;
        CorrDiff = CorrSunset - CorrSunrise;
        time_interval1 = calcPerStep(CorrDiff);
        secondsToHMS((Sunrise*60), &HRaux, &MINaux, &SECaux);
        RTC.setAlarm(ALM2_MATCH_HOURS, 0, MINaux, HRaux, 0);
      }
      else{
        MINaux = MIN + time_interval2;
        if(MINaux > 59){MINaux = MINaux - 60;}
        RTC.setAlarm(ALM2_MATCH_MINUTES , 0, MINaux, 0, 0); // Set New Alarm
        tempRead();
        evalCooler();
        getCompassData();
        printDataSD();
      }
    }
    if(RTC.alarm(ALARM_1)){ // if alarm #2 was triggered, clear its flag and execute its functions: printTest.
      digitalWrite(ena, HIGH);
      l12 = digitalRead(limit12);
      if(l12 == 1){
        digitalWrite(ena, LOW);
        uint16_t DOYaux = dayOfTheYear(DAY+1, MON, YEAR);
        uint16_t SunriseAux = sunrise(DOYaux);
        uint16_t SunsetAux = sunset(DOYaux);
        uint16_t diffAux = SunsetAux-SunriseAux;
        float CorrSunriseAux = SunriseAux + (17.5/180)*diffAux;
        float CorrSunsetAux = SunsetAux - (17.5/180)*diffAux;
        float CorrDiffAux = CorrSunsetAux - CorrSunriseAux;
        uint8_t time_interval1Aux = calcPerStep(CorrDiffAux);
        secondsToHMS(round(CorrSunriseAux * 60 + 0.5 * time_interval1Aux), &HRaux, &MINaux, &SECaux);
        RTC.setAlarm(ALM1_MATCH_HOURS, SECaux, MINaux, HRaux, 0);
      }
      if(time_interval1 > 60){
        MINaux = MIN + 1;
        SECaux = SEC + (time_interval1-60);
        if(SECaux > 59){
          SECaux = SECaux - 60;
          MINaux++;
        }
        RTC.setAlarm(ALM1_MATCH_MINUTES , SECaux, MINaux, 0, 0); // Define here wake up time
      }
      else{
        SECaux = second(t) + time_interval1;
        if(SECaux > 59){SECaux = SECaux - 60;}
        RTC.setAlarm(ALM1_MATCH_SECONDS, SECaux, 0, 0, 0); // Set New Alarm
      }
      printTest();
      //l11 = digitalRead(limit11);
      //l12 = digitalRead(limit12);
      //if (l11 == 1 && l12 == 0){digitalWrite(dir, HIGH);}
      //if (l11 == 0 && l12 == 1) {digitalWrite(dir, LOW);}
      stepNema17();
    }
    intrrCnt = intrrCnt - 1;
  }
}
