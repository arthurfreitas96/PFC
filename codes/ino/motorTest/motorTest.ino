// Code to test how many steps (or microsteps) Nema 17 takes from one microswitch to another.

#define ena 5
#define dir 3
#define pul 4
#define switch1 7
#define switch2 9
bool s1;
bool s2;
const int interval = 64000;   // Interval between changes in pulse state. For step modes: recommended 2000 for 1/32, 4000 for 1/16, 8000 for 1/8, 16000 for 1/4, 64000 for 1/2 and 1.
// With the recommended settings, Nema 17 moves slowly, softly and the results are more stable because there is less bouncing when the panel reaches the switches.
bool pulse; // Pulse state
int steps;

void setup()
{
  Serial.begin(9600);
  pinMode(ena, OUTPUT);
  pinMode(dir, OUTPUT);
  pinMode(pul, OUTPUT);
  pinMode(switch1, INPUT_PULLUP); // Define switches as input pullup so there is no need to use resistors.
  pinMode(switch2, INPUT_PULLUP);
  digitalWrite(ena, HIGH); // Enables Nema 17
  digitalWrite(dir, HIGH); // LOW Clockwise / HIGH Counter Clockwise
  digitalWrite(pul, LOW);
  steps = 0;
  pulse = 0;
}

void loop()
{
  s1 = digitalRead(switch1);
  s2 = digitalRead(switch2);
  if(s1 == 1 && s2 == 0) { // When the panel reaches switch s1
    digitalWrite(dir, HIGH); // Changes direction of the next step
    if(steps > 100){ // To avoid bouncing
      Serial.println(steps); // Print result
      steps = 0; // Resets counter
      delay(3000);
    }
  }
  if(s1 == 0 && s2 == 1) { // When the panel reaches switch s1
    digitalWrite(dir, LOW); // Changes direction of the next step
    if(steps > 100){ // To avoid bouncing
      Serial.println(steps); // Print result
      steps = 0; // Resets counter
      delay(3000);
    }
  }
  pulse = !pulse;
  digitalWrite(pul, pulse);
  delayMicroseconds(interval);
  if(pulse){steps = steps + 1;} // Nema 17 will only move with a rising border in 'pul'. Increments counter only in rising border.
}
