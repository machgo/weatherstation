/* 
 Weather Station using the Electric Imp
 By: Marco Schmid
 SparkFun Electronics
 Date: August 14th, 2014
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 
 Most of this is based on Nathan Seidle's Weather Station code.
 
*/


#include <avr/wdt.h> //Watchdog
#include <Wire.h> //I2C needed for sensors
#include "MPL3115A2.h" //Pressure
#include "HTU21D.h" //Humidity

MPL3115A2 myPressure; //instance of the pressure sensor
HTU21D myHumidity; //instance of the humidity sensor

// digital I/O pins
const byte WSPEED = 3;
const byte RAIN = 2;
const byte STAT1 = 7;
const byte STAT2 = 8;

// analog I/O pins
const byte WDIR = A0;
const byte LIGHT = A1;
const byte BATT = A2;
const byte REFERENCE_3V3 = A3;

long lastSecond;
byte seconds;
byte minutes;

long lastWindCheck = 0;
volatile long lastWindIRQ = 0;
volatile byte windClicks = 0;

byte windSpeedValues[60];
int windDirectionValues[60];

volatile float rainMinute;
volatile unsigned long rainlast;

//Interrupts
void rainIRQ()
{
  if (millis() - rainlast > 10) // ignore switch-bounce glitches less than 10mS after initial edge
  {
    rainlast = millis();
    rainMinute += 0.2794; //Increase this minute's amount of rain
  }
}

void wspeedIRQ()
{
  if (millis() - lastWindIRQ > 10) // Ignore switch-bounce glitches less than 10ms (142MPH max reading) after the reed switch closes
  {
    lastWindIRQ = millis(); //Grab the current time
    windClicks++; //There is 1.492MPH for each click per second.
  }
}

//Setup Methode
void setup()
{
  wdt_reset();
  wdt_disable();
  
  Serial.begin(9600);
  
  pinMode(WSPEED, INPUT_PULLUP); // input from wind meters windspeed sensor
  pinMode(RAIN, INPUT_PULLUP); // input from wind meters rain gauge sensor

  pinMode(WDIR, INPUT);
  pinMode(LIGHT, INPUT);
  pinMode(BATT, INPUT);
  pinMode(REFERENCE_3V3, INPUT);
  
  pinMode(STAT1, OUTPUT);
  pinMode(STAT2, OUTPUT);
  
  //Configure the pressure sensor
  myPressure.begin(); 
  myPressure.setModeBarometer();
  myPressure.setOversampleRate(128);
  myPressure.enableEventFlags();  
  myPressure.setModeActive();
  
  //Configure the humidity sensor
  myHumidity.begin();
  
  //attach interrupt pins
  attachInterrupt(0, rainIRQ, FALLING);
  attachInterrupt(1, wspeedIRQ, FALLING);
  
  interrupts();
  
  Serial.println("Balous Weather Station online!");
  reportWeather();
}

void loop()
{
  wdt_reset();

  //Keep track of which minute it is
  if(millis() - lastSecond >= 1000)
  {
    lastSecond += 1000;

    //Take a speed and direction reading every second for 1 minute average
    if(++seconds > 59)
    {
      seconds = 0;
      if(++minutes > 59) minutes = 0;
    }
    
    windSpeedValues[seconds] = (int)get_wind_speed();
    windDirectionValues[seconds] = get_wind_direction();
  
    //Blink LED
    digitalWrite(STAT1, HIGH);
    delay(25);
    digitalWrite(STAT1, LOW);
  }

  //Wait for the imp to ping us with the ! character
  if(Serial.available())
  {
    byte incoming = Serial.read();
    if(incoming == '!')
    {
      //Blink LED
      digitalWrite(STAT2, HIGH);
      delay(25);
      digitalWrite(STAT2, LOW);
      
      reportWeather(); //Send all to the imp
      //Serial.print("Pinged!");
    }
    else if(incoming == '@') //Special character from Imp indicating midnight local time
    {
      midnightReset(); //Reset a bunch of variables like rain and total rain
      //Serial.print("Midnight reset");
    }
    else if(incoming == '#') //Special character from Imp indicating a hardware reset
    {
      //Serial.print("Watchdog reset");
      delay(5000); //This will cause the system to reset because we don't pet the dog
    }
  }

  delay(100); //Update every 100ms. No need to go any faster.
}

//Reports the weather string to the Imp
void reportWeather()
{
  //Calc WindSpeed Average
  float windspdavg = 0;
  for(int i = 0 ; i < 60 ; i++)
  {
    windspdavg += windSpeedValues[i];
  }
  windspdavg /= 60.0;
  
  //Calc WindDir Average
  float winddiravg = 0;
  for(int i = 0 ; i < 60 ; i++)
  {
    winddiravg += windDirectionValues[i];
  }
  winddiravg /= 60.0;

  float windgustspd = 0;
  float windgustdir = 0;
  for (int i = 0; i < 60; i++)
  {
    if (windgustspd < windSpeedValues[i])
    {
      windgustspd = windSpeedValues[i];
      windgustdir = windDirectionValues[i];     
    }
  }
  
  float humidity = myHumidity.readHumidity();
  float tempc = myPressure.readTemp();
  float pressure = myPressure.readPressure();

  float rainmm = rainlast;
  rainlast = 0;
  
  float light_lvl = get_light_level();
  float batt_lvl = get_battery_level();
  
  Serial.print("{\"windspdavg\": ");
  Serial.print(windspdavg);
  Serial.print(", \"winddiravg\": ");
  Serial.print(winddiravg, 1);
  Serial.print(", \"windgustspd\": ");
  Serial.print(windgustspd, 1);
  Serial.print(", \"windgustdir\": ");
  Serial.print(windgustdir);
  Serial.print(", \"humidity\": ");
  Serial.print(humidity, 1);
  Serial.print(", \"tempc\": ");
  Serial.print(tempc, 1);
  Serial.print(", \"pressure\": "); 
  Serial.print(pressure, 2);
  Serial.print(", \"rainmm\": ");
  Serial.print(rainmm, 2);
  Serial.print(", \"batt_lvl\": ");
  Serial.print(batt_lvl, 2);
  Serial.print(", \"light_lvl\": ");
  Serial.print(light_lvl, 2);
  Serial.println("}");

  //Test string
  //Serial.println("$,winddir=270,windspeedmph=0.0,windgustmph=0.0,windgustdir=0,windspdmph_avg2m=0.0,winddir_avg2m=12,windgustmph_10m=0.0,windgustdir_10m=0,humidity=998.0,tempf=-1766.2,rainin=0.00,dailyrainin=0.00,-999.00,batt_lvl=16.11,light_lvl=3.32,#,");
}

float get_wind_speed()
{
  float deltaTime = millis() - lastWindCheck; //750ms

  deltaTime /= 1000.0; //Covert to seconds

  float windSpeed = (float)windClicks / deltaTime; //3 / 0.750s = 4

  windClicks = 0; //Reset and start watching for new wind
  lastWindCheck = millis();

  windSpeed *= 2.4; // 4* 2.4 = 9.6KMH

  return(windSpeed);
}

int get_wind_direction() 
{
  unsigned int adc;

  adc = averageAnalogRead(WDIR); // get the current reading from the sensor

  if (adc < 380) return (113);
  if (adc < 393) return (68);
  if (adc < 414) return (90);
  if (adc < 456) return (158);
  if (adc < 508) return (135);
  if (adc < 551) return (203);
  if (adc < 615) return (180);
  if (adc < 680) return (23);
  if (adc < 746) return (45);
  if (adc < 801) return (248);
  if (adc < 833) return (225);
  if (adc < 878) return (338);
  if (adc < 913) return (0);
  if (adc < 940) return (293);
  if (adc < 967) return (315);
  if (adc < 990) return (270);
  return (-1); // error, disconnected?
}

float get_light_level()
{
  float operatingVoltage = averageAnalogRead(REFERENCE_3V3);

  float lightSensor = averageAnalogRead(LIGHT);
  
  operatingVoltage = 3.3 / operatingVoltage; //The reference voltage is 3.3V
  
  lightSensor *= operatingVoltage;
  
  return(lightSensor);
}

float get_battery_level()
{
  float operatingVoltage = averageAnalogRead(REFERENCE_3V3);
  float rawVoltage = averageAnalogRead(BATT);
  
  operatingVoltage = 3.30 / operatingVoltage; //The reference voltage is 3.3V  
  rawVoltage *= operatingVoltage; //Convert the 0 to 1023 int to actual voltage on BATT pin
  rawVoltage *= 4.90; //(3.9k+1k)/1k - multiply BATT voltage by the voltage divider to get actual system voltage
  
  return(rawVoltage);
}

void midnightReset()
{
  //TODO: Clean up this function
  return;
}

//Takes an average of readings on a given pin
//Returns the average
int averageAnalogRead(int pinToRead)
{
  byte numberOfReadings = 8;
  unsigned int runningValue = 0; 

  for(int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
  runningValue /= numberOfReadings;

  return(runningValue);  
}





