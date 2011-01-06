/*
  AeroQuad v2.1.3 Beta - December 2010
  www.AeroQuad.com
  Copyright (c) 2010 Ted Carancho.  All rights reserved.
  An Open Source Arduino based multicopter.
 
  This program is free software: you can redistribute it and/or modify 
  it under the terms of the GNU General Public License as published by 
  the Free Software Foundation, either version 3 of the License, or 
  (at your option) any later version. 

  This program is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  GNU General Public License for more details. 

  You should have received a copy of the GNU General Public License 
  along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

// Written by Honk: http://aeroquad.com/showthread.php?1369-The-big-enhancement-addition-to-2.0-code&p=13282#post13282

// *************************************************************************
// ************************** Battery Monitor ******************************
// *************************************************************************
class BatteryMonitor {
public: 
  #define BATTERYPIN 0      // Ain 0 (universal to every Arduino), pin 55 on Mega (1280)
  #define OK 0
  #define WARNING 1
  #define ALARM 2
  byte batteryStatus;
  float lowVoltageWarning;  // Pack voltage at which to trigger alarm (first alarm)
  float lowVoltageAlarm;    // Pack voltage at which to trigger alarm (critical alarm)
  float batteryVoltage;
  float batteryScaleFactor;
  
  BatteryMonitor(void) { 
    lowVoltageWarning = 10.0; //10.8;
    lowVoltageAlarm = 9.5; //10.2;
    batteryStatus = OK;
  }

  virtual void initialize(void); 
  virtual const float readBatteryVoltage(byte); // defined as virtual in case future hardware has custom way to read battery voltage
  virtual void lowBatteryEvent(byte); 
  
  void measure(void) {     
    batteryVoltage = smooth(readBatteryVoltage(BATTERYPIN), batteryVoltage, 0.1);
    if (batteryVoltage < lowVoltageWarning) batteryStatus = WARNING;
    if (batteryVoltage < lowVoltageAlarm) batteryStatus = ALARM;
    lowBatteryEvent(batteryStatus);
  }
  
  const float getData(void) {
    return batteryVoltage;
  }
};

// ***********************************************************************************
// ************************ BatteryMonitor APM & CHR6DM  *****************************
// ***********************************************************************************
/* Circuit:
  
  Vin--D1--R1--|--R2--GND
               |
               |
              Vout
*/
//If lipo is 12.6V and diode drop is 0.6V (res 12.0V), the voltage from divider network will be = 2.977V
//calculation: AREF/1024.0 is Vout of divider network
//Vin = lipo voltage minus the diode drop
//Vout = (Vin*R2) * (R1+R2)
//Vin = (Vout * (R1+R2))/R2
//Vin = ((((AREF/1024.0)*adDECIMAL) * (R1+R2)) / R2) + Diode drop //(aref/1024)*adDecimal is Vout
//Vout connected to Ain0 on any Arduino
/* Circuit:
  PIN57--FL_LED--150ohm--GND
  PIN58--FR_LED--150ohm--GND
  PIN59--RR_LED--150ohm--GND
  PIN60--RL_LED--150ohm--GND
*/

class BatteryMonitor_APM : public BatteryMonitor { 
private:
  #define FL_LED 57 // Ain 2 on Mega
  #define FR_LED 58 // Ain 3 on Mega
  #define RR_LED 59 // Ain 4 on Mega
  #define RL_LED 60 // Ain 5 on Mega
  #define LEDDELAY 200
  float R1; //the SMD 10k resistor measured with DMM
  float R2; //3k3 user mounted resistor measured with DMM
  float Aref; //AREF 3V3 used (solder jumper) and measured with DMM
  float diode; //Schottky diode on APM board
  
  void ledCW(void){ 
    digitalWrite(RL_LED, HIGH);
    delay(LEDDELAY);
    digitalWrite(RL_LED, LOW);
    digitalWrite(RR_LED, HIGH);
    delay(LEDDELAY);
    digitalWrite(RR_LED, LOW);
    digitalWrite(FR_LED, HIGH);
    delay(LEDDELAY);
    digitalWrite(FR_LED, LOW);
    digitalWrite(FL_LED, HIGH);
    delay(LEDDELAY);
    digitalWrite(FL_LED, LOW); 
  };
  
  void ledsON(void){
    digitalWrite(RL_LED, HIGH);
    digitalWrite(RR_LED, HIGH);
    digitalWrite(FR_LED, HIGH);
    digitalWrite(FL_LED, HIGH); 
  };
  
  void ledsOFF(void){
    digitalWrite(RL_LED, LOW);
    digitalWrite(RR_LED, LOW);
    digitalWrite(FR_LED, LOW);
    digitalWrite(FL_LED, LOW); 
  };

public: 
  BatteryMonitor_APM() : BatteryMonitor(){}
  void initialize(void) {
    R1 = 10050; //the SMD 10k resistor measured with DMM
    R2 = 3260; //3k3 user mounted resistor measured with DMM
    Aref = 3.27F; //AREF 3V3 used (solder jumper) and measured with DMM
    diode = 0.306F; //Schottky diode on APM board, drop measured with DMM
    batteryScaleFactor = ((Aref / 1024.0) * ((R1 + R2) / R2)) + diode;
    
    pinMode(FL_LED ,OUTPUT);
    pinMode(FR_LED ,OUTPUT);
    pinMode(RR_LED ,OUTPUT);
    pinMode(RL_LED ,OUTPUT);
    analogReference(EXTERNAL); //use Oilpan 3V3 AREF or if wanted, define DEFAULT here to use VCC as reference and define that voltage in BatteryReadArmLed.h
  }
  
  void lowBatteryEvent(byte level) {  // <- this logic by Jose Julio
    static byte batteryCounter=0;
    byte freq;
  
    if (level == OK) {
      ledsON();
      autoDescent = 0; //reset autoAscent if battery is good
    }
    else {
      batteryCounter++;
      if (level == WARNING) freq = 40;  //4 seconds wait
      else freq = 5; //0.5 second wait
      
      if (batteryCounter < 2) ledsOFF();  //indicate with led's everytime autoDescent kicks in
      #ifndef AltitudeHold
        #ifdef AutoDescent
          if (throttle > 1400) autoDescent -= 2; //will remove 2µs throttle every time led's blink in two speeds (10.8 and 10.2V) as long as there is throttle to lower
        #endif
      #endif
      #if defined(AltitudeHold) && defined(AutoDescent)
        if (throttle > 1400) holdAltitude -= 0.2; //-0.2m in 2 fixed rates, one where battery < 10.8V and one where battery < 10.2V, only done if in altitude hold mode
      #endif
      else if (batteryCounter < freq) ledsON();
      else batteryCounter = 0;
    }
  }
  
  const float readBatteryVoltage(byte channel) {
    return (analogRead(channel) * batteryScaleFactor) + diode;
  }
};

// *******************************************************************************
// ************************ AeroQuad Battery Monitor *****************************
// *******************************************************************************
class BatteryMonitor_AeroQuad : public BatteryMonitor {
private:
  long previousTime;
  float R1;
  float R2;
  float Aref;
  float diode; // raw voltage goes through diode on Arduino
  byte state;
  
public: 
  BatteryMonitor_AeroQuad() : BatteryMonitor(){}

  void initialize(void) {
    analogReference(DEFAULT);
    pinMode(49, OUTPUT); // connect a 12V buzzer to pin 49
    digitalWrite(49, LOW);
    R1 = 15000.0;
    R2 = 7500.0;
    Aref = 5.0;
    diode = 0.9; // measured with DMM
    batteryScaleFactor = ((Aref / 1024.0) * ((R1 + R2) / R2));
    previousTime = millis();
    state = LOW;
  }

  void lowBatteryEvent(byte level) {
    long currentTime = millis()- previousTime;
    if (level == WARNING) {
      if ((autoDescent == 0) && (currentTime > 1000)) {
        autoDescent = -75;
        digitalWrite(LED2PIN, LOW);
      }
      if (currentTime > 1100) {
        autoDescent = 0;
        previousTime = millis();
        digitalWrite(LED2PIN, HIGH);
      }
    }
    if (level == ALARM) {
      digitalWrite(49, HIGH); // enable buzzer
      if ((currentTime > 500) && (throttle > 1400)) {
        autoDescent -= 2; // auto descend quad
        previousTime = millis();
        if (state == LOW) state = HIGH;
        else state = LOW;
        digitalWrite(LEDPIN, state);
        digitalWrite(LED2PIN, state);
      }
    }
  }
  
  const float readBatteryVoltage(byte channel) {
    return (analogRead(channel) * batteryScaleFactor) + diode; 
  } 
};
