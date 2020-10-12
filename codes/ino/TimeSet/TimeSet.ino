// Arduino DS3232RTC Library
// https://github.com/JChristensen/DS3232RTC
// Copyright (C) 2018 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
//
// DS3231/DS3232 Alarm Example Sketch #4
//
// Set Alarm 1 to occur every second.
// Detect the alarm by polling the RTC alarm flag.
//
// Hardware:
// Arduino Uno, DS3231 RTC.
// Connect RTC SDA to Arduino pin A4.
// Connect RTC SCL to Arduino pin A5.
//
// Jack Christensen 16Sep2017

#include <DS3232RTC.h>      // https://github.com/JChristensen/DS3232RTC

void setup()
{
    Serial.begin(9600);

    // initialize the alarms to known values, clear the alarm flags, clear the alarm interrupt flags
    RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
    RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
    RTC.alarm(ALARM_1);
    RTC.alarm(ALARM_2);
    RTC.alarmInterrupt(ALARM_1, false);
    RTC.alarmInterrupt(ALARM_2, false);
    RTC.squareWave(SQWAVE_NONE);

    //tmElements_t tm;
    //tm.Hour = 16;           // set the RTC to an arbitrary time
    //tm.Minute = 11;
    //tm.Second = 50;
    //tm.Day = 23;
    //tm.Month = 5;
    //tm.Year = 2020 - 1970;  // tmElements_t.Year is the offset from 1970
    //RTC.write(tm);          // set the RTC from the tm structure

    // set Alarm 1 to occur once per second
    RTC.setAlarm(ALM1_EVERY_SECOND, 0, 0, 0, 0);
    // clear the alarm flag
    RTC.alarm(ALARM_1);

    printDateTime(RTC.get());
}

void loop()
{
    printDateTime(RTC.get());
}

void printDateTime(time_t t)
{
    //Printing timestamp
    Serial.print("[");
    // Send date
    Serial.print(day(t));
    Serial.print(".");
    Serial.print(month(t));
    Serial.print(".");
    Serial.print(year(t));
    Serial.print(" -- ");
    // Send time
    Serial.print(hour(t));
    Serial.print(".");
    Serial.print(minute(t));
    Serial.print(".");
    Serial.print(second(t));
    Serial.println("]");
}
