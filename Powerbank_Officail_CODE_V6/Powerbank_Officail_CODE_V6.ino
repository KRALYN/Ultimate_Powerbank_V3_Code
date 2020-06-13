#include <ezButton.h>         //ez button libary 

#include <Type4051Mux.h>      //4051 MUx libary
#include <FastLED.h>          //LED RING Libaries
#include <X9C.h>              // digital pot, for changing charging current

#include <Adafruit_GFX.h>     //display graphics
#include <TM1640.h>           //LED matrix chip libary
#include <TM16xxMatrixGFX.h>  
#include <Fonts/Picopixel.h>  //small font for numbers

//---------------------Setup phase---------------------------------

//Pin Declorations------------------------------------------------------
const int powerPin = 8; //Keep high to turn the powerbank on, low to immedialy turn off
const int fanPin = 16; //high to turn fan on, low to turn fan off
const int relayPin = 17; //relay pin to enable charging

//multiplexer: 
Type4051Mux inMux(A7, INPUT, ANALOG, 5, 6, 7); //define Mux pins

//set up buttons
ezButton powerButton(11);      // create ezButton object that attach to pin 6;
ezButton usbButton(12);       // create ezButton object that attach to pin 7;
ezButton wirelessButton(13);  // create ezButton object that attach to pin 8;

//WS2812 LED ring:
#define NUM_LEDS 24 //numbers of leds in the ring
#define DATA_PIN 14  //pin to send data to led ring
CRGB leds[NUM_LEDS]; // Define the array of leds
int LEDBrightness = 10; //LED brightness in non-flashlight mode
byte flashlightBrightness = 250; //defult led brightness in flashlight mode
byte gHue = 0; // rotating "base color" used by many of the patterns

//X9C103 digital Pot:
#define INC 2   //to inc pin x9c
#define UD 3  // to ud pin on x9c
#define CS 4   // to cs pin x9c
X9C pot;       // create a pot controller

//devices names and corrisponding pins
const int usbOnPIN = 10; //usb QC 3.0 module pin
const int modulePin = 9; //wireless charging module pin
const int wirelessPin = 15; //wireless charging module pin


//ACS712 Hall effect current sensor: 
int mVperAmp20A = 100; // use 100 for 20A; 100mV per amp
int ACSoffset = 2500; 
double Voltage = 0;
double Amps = 0;

//8x16 I2C LED dot matrix display:
TM1640 module(A4, A5);    // I2C connection for LED matrix
#define MODULE_SIZECOLUMNS 16    // number of GRD lines, will be the y-height of the display
#define MODULE_SIZEROWS 8    // number of SEG lines, will be the x-width of the display
TM16xxMatrixGFX matrix(&module, MODULE_SIZECOLUMNS, MODULE_SIZEROWS);    // TM16xx object, columns, rows

//100K NTC thermistor values:
float R1 = 10000;
float logR2, R2, T;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

//Voltage mesureing values:
float VinMv = 0.0;       //raw voltage value in Mv
float VdivR1 = 100000.0; // resistance of R1 (100K Ohm) for input voltage
float VdivR0 = 47500.0; // resistance of R (47K Ohm) for battery voltage
float VdivR2 = 10000.0; // resistance of R2 (10K Ohm) common

//displayed texts values
int textDelay = 1000; //how long between text displays
unsigned long lastTextON;
bool line1; 
String lineOneTEXT;
String lineTwoTEXT;

//Sensors Values: 
int batteryTempRAW;
int chargingTempRAW;
int caseTempRAW;
int moduleTempRAW;
int moduleConnectRAW;
int batteryVoltageRAW;
int inputVoltageRAW;
int powerbankAmpRAW;
int chargingAmpRAW;

double powerbankAmp;
double chargingAmp;
double batteryTemp;
double moduleTemp;
double caseTemp;
double chargingTemp;
double batteryVoltage;
double inputVoltage;
double powerOutputWATT;
bool moduleConnect;

//modes
#define NUM_MODES 10         //Update this number to the highest number of "cases" actually +1 because 0 is case 1
int ledMode = 0; 
bool chargingMode;
bool fanOn = false;
bool usbQcON = false;
bool wirelessOn = false;
bool moduleOn = false;
bool inChargingMODE = false; //indicate that charging mode state;
bool matrixShortON = false; 
bool flashlightMode = false;
bool inModuleMODE = false;
bool inQcMODE = false;
bool inWirelessMODE = false;
bool inputVoltageLOW = true;
bool turnOnFINISHED = false;

//other values:
int readSensorINTERVAL = 1000; //every x milisecond read sensors
int numLEDsVOLT;
int batteryPercentage;
int greenLEDsINTENSITY;
bool ringVoltageON = false; //indicate LED ring voltage display on/off
bool matrixVoltageON = false; //indicate matrix voltage diaplay on/off
bool chargeFlashLED = false; //indicating the charge flash led is on or off 
int fanSpeed; //0-255 is 0-100% fan
int displayTextCYCLE = 0; //times to switch display text mode
int fanOnTEMP = 38; //temperature to turn the fan on
float idlePowerWATT = 4; //if powerbank is drawing less than x watts, it is consided in idle.
bool chargeCurrentLOW = false; //CC mode low current indication
//int fanMaxTEMP = 48; //temperature fan speed reaches max

//protection mechnisms:
float currentLimit = 8.5; //maxium amount of current
float maxBatteryVOLTAGE = 21.2; //maxium battery voltage - around 4.23V x num of cells (5s or 4s)
float fullBatteryVOLTAGE = 20.7; //full battery voltage - slightly lower than max voltage or user defined
float emptyBatteryVOLTAGE = 14; //maxium battery voltage - around 2.75V x num of cells (5s or 4s)
float minBatteryVOLTAGE = 11; //minium battery voltage - around 2.2V x num of cells (5s or 4s)
int fullBatteryADC;
int emptyBatteryADC;
int warnTimesLED = 4; //times warning leds flash
int warnTimesMATRIX = 6; //times text would change
bool overCurrentPROTEC = false; //start with over voltage protection off. 
bool overTempPROTEC = false;
bool overVoltPROTEC = false;
bool batteryFaultPROTEC = false;

//time values:
//buttons:
int shortPressDURATION = 500; //max time hold for button press to be counted as short press
int longPressDURATION = 1500;  //max time hold for button press to be counted as long press
int powerOffDURATION = 2000; //time to hold power button to turn off power bank
int voltageUpdateTIME = 8000; //every x seconds in charging mode, update the battery voltage display
int chargingFlashTIME = 500; //every x mili seconds in charging mode, flash the next ring voltage indicator led
int matrixOnDELAY = 4500; //time it takes for matrix to turn off once displaying voltage
int chargeCurrentCHECK = 10000; //if charging current stays low for 10 seconds, stop charging
unsigned long idlePowerOFF = 90000; //how long it takes for powerbank to auto shut off when less than idle power
unsigned long lastChargeCURRENT;
unsigned long currentMillis;
unsigned long timeSinceUPDATE;
unsigned long timeSinceFLASH;
unsigned long idleTime;
unsigned long lastInputVOLTAGEhigh;
int powerTimeHOLD;
unsigned long powerTimePRESSED;
unsigned long lastDisplayTEXT;
unsigned long powerTimeRELEASED;
int usbTimeHOLD;
unsigned long usbTimePRESSED;
unsigned long usbTimeRELEASED;
int wirelessTimeHOLD;
unsigned long wirelessTimePRESSED;
unsigned long wirelessTimeRELEASED;
unsigned long matrixOnTIME;  
unsigned long lastCurrentCHECK = 0;
unsigned long lastUpdateTIME = 0;    
unsigned long lastActiveTIME;
unsigned long lastFlashTIME = 0;    
unsigned long preMillisLED = 0;
unsigned long preMillisWARN = 0;
unsigned long preMillisDISPLAY1 = 0;
unsigned long preMillisDISPLAY2 = 0;
unsigned long firstOnTIME;
const long interval = 4000; 

void setup() {

Serial.begin(9600); 

//define devices signal pin as output
pinMode(fanPin,OUTPUT);
pinMode(powerPin,OUTPUT);
pinMode(usbOnPIN,OUTPUT);
pinMode(modulePin,OUTPUT);
pinMode(wirelessPin,OUTPUT);
pinMode(relayPin,OUTPUT); 

digitalWrite(powerPin,HIGH); //keep the powerbank on
pot.begin(CS,INC,UD);  //initialize digital pot..
//pot.setPot(10,true);    //set pot to 100% of highest value

LEDS.addLeds<WS2812,DATA_PIN,RGB>(leds,NUM_LEDS); //intialize LEDS
//the real order is Green, Red, blue

fullBatteryADC = 1023 * (fullBatteryVOLTAGE * VdivR2 / (VdivR2 + VdivR0)) / 5; //calculating full battery voltage in adc value
emptyBatteryADC =  1023 * (emptyBatteryVOLTAGE * VdivR2 / (VdivR2 + VdivR0)) / 5; //calculating empty battery voltage in adc value
  
//turning on actions.
readSensors();
mapVoltages (); //map voltages onto displays and prencntages
LEDRingDisplayVOLTAGE (); //displays voltages on LED ring
matrixDisplayPRECENT (); //display battery precentage on led matrix
  firstOnTIME = millis(); //set ontime counter to zero
  ringVoltageON = true; //indicate that the ring is on

  powerButton.setDebounceTime(50); // set debounce time to 50 milliseconds
  usbButton.setDebounceTime(50); // set debounce time to 50 milliseconds
  wirelessButton.setDebounceTime(50); // set debounce time to 50 milliseconds
  
}  //end void Setup

void loop() { //Main code OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
  
  //Serial.println("New loop begin");
  currentMillis = millis(); //set current time to miliseconds since started.
  
  EVERY_N_MILLISECONDS(readSensorINTERVAL) { //fast LED lib timer, read sensors every (interval) second
     readSensors();
     interpretValues ();
   }//end every (interval) sec actions
   
  if(matrixShortON == true && currentMillis - matrixOnTIME > matrixOnDELAY){ //in matrix activated for a short time, turn it off
    displayOff ();
    matrixShortON = false;
  }//end if matrix short on 
  
  idleTime = currentMillis - lastActiveTIME;
  //Serial.print(idleTime);
  if(powerOutputWATT >= idlePowerWATT || flashlightMode == true || chargingMode == true) { 
    //if powerbank power consumption is larger than idle power watt, or in charging or flashlight, reset auto off timer.
    lastActiveTIME = millis(); //set last active time to current time.
  } // end auto off sequence


  if(idleTime >= idlePowerOFF) { //if powerbank is idle for longer than idle power off seconds, SHUT DOWN
    Serial.println("time out power off");
    lineOneTEXT = ("TIME");
  lineTwoTEXT = ("OUT");
  matrix.fillScreen(LOW); 
   matrix.write();
   matrix.setCursor(0,5);
  matrix.print(lineOneTEXT);
  matrix.write();
  delay(800);
  matrix.setCursor(0,5);
   matrix.fillScreen(LOW); 
   matrix.write();
  matrix.print(lineTwoTEXT);
  matrix.write();
  delay(800);
    powerOff ();
  } //end if idle power off

  
   powerButton.loop(); // loop for buttons (constantly detecting state)
   usbButton.loop(); 
   wirelessButton.loop(); 
    
     if (powerButton.isPressed()){ //begin power button pressed actions
      powerTimePRESSED = millis();
    } //end if usb is pressed
    if(powerButton.isReleased()){
      powerTimeRELEASED = millis();
      powerTimeHOLD = powerTimeRELEASED - powerTimePRESSED;
      
      if(powerTimeHOLD <= shortPressDURATION){
        if(flashlightMode == false && matrixShortON == false) { //if flashlight mode is false, display voltages on matrix
        mapVoltages ();
        matrixDisplayPRECENT (); //display battery precentage on led matrix
        matrixOnTIME = millis(); //reset matrix on timer
        matrixShortON = true;
        }//end if flashlight mode false short press actions

        if(flashlightMode == true) { //if flashlightmode is true, switch mode
          ledMode++;
          if (ledMode > NUM_MODES){
          ledMode=0; 
          }
        }//end if flashlight mode true short press actions
        
        }//end power button pressed actions
     if(powerTimeHOLD > shortPressDURATION && powerTimeHOLD <= longPressDURATION && turnOnFINISHED == true){
       if(flashlightMode == false){
        flashlightMode = true;
        } //end if
        else{
          ledRingOFF ();  //turn ring off
          flashlightMode = false;
       } //end flashlight mode flip flop
     }//end power long press actions
    }//end power button pressed trigger actions
  int powerButtonSTATE = powerButton.getState();
  int powerHoldTIME = currentMillis - powerTimePRESSED;
   if (powerButtonSTATE == 0 && powerHoldTIME >= powerOffDURATION){ // if power button is held for longer than x mili sec, POWER OFF
    Serial.print("POWERING OFFFFFFFFFFFFFFFFFFF");
    powerOff ();
   } //end all power button actions
   

    if (usbButton.isPressed()){ //begin usb button pressed actions
      usbTimePRESSED = millis();
    } //end if usb is pressed
    if(usbButton.isReleased()){
      usbTimeRELEASED = millis();
      usbTimeHOLD = usbTimeRELEASED - usbTimePRESSED;
      
      if(usbTimeHOLD <= shortPressDURATION){
       if(flashlightMode == true ) {
         if(flashlightBrightness <= 220) {
        flashlightBrightness = flashlightBrightness + 30;
         }
       }//end if flashlight mode is on short press usb actions
       if(flashlightMode == false) {
        if(usbQcON == false){
           usbQcON = true;
           }
          else{
           usbQcON = false;
        } //end usb qc flip flop
        }
        }//end all short press actions
     if(usbTimeHOLD > shortPressDURATION && usbTimeHOLD <= longPressDURATION){
       
        }
    }//end usb button pressed trigger actions

    if (wirelessButton.isPressed()){ //begin wireless button actions
      wirelessTimePRESSED = millis();
    } //end if usb is pressed
    if(wirelessButton.isReleased()){
      wirelessTimeRELEASED = millis();
      wirelessTimeHOLD = wirelessTimeRELEASED - wirelessTimePRESSED;
      
      if(wirelessTimeHOLD <= shortPressDURATION){
        if(flashlightMode == true ) {
         if( flashlightBrightness >= 30) {
        flashlightBrightness = flashlightBrightness - 30;
         }
       }//end if flashlight mode is on short press wireless actions

       if(flashlightMode == false) {
        if(wirelessOn == false){
           wirelessOn = true;
           }
          else{
           wirelessOn = false;
        } //end wireless module flip flop
        }
      }//end all short press wireless button actions
     if(wirelessTimeHOLD > shortPressDURATION && wirelessTimeHOLD <= longPressDURATION){
        if(moduleOn == false && moduleConnect == true){
           moduleOn = true;
           }
          else{
           moduleOn = false;
        } //end extermal module flip flop
        }
    }//end wireless button pressed trigger actions

if (wirelessOn == true && inWirelessMODE == false){ //Wireless on actions (once)
  lineOneTEXT = ("ON");
  lineTwoTEXT = ("WIR");
  lastDisplayTEXT = millis ();
  digitalWrite(wirelessPin,HIGH); 
  inWirelessMODE = true; 
} //end Pd on actions

if (wirelessOn == false && inWirelessMODE == true){ //Wireless off actions (once)
  lineOneTEXT = ("OFF");
  lineTwoTEXT = ("WIR");
  lastDisplayTEXT = millis ();
  digitalWrite(wirelessPin,LOW); 
  ledRingOFF ();
  inWirelessMODE = false; 
} //end Qc on actions   

if (inWirelessMODE == true){ //breath blue led while charging
   fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 30, 0, 255) );
 float breath = (exp(sin(millis()/2000.0*PI)) - 0.36787944)*5.0;
 FastLED.setBrightness(breath);
 FastLED.show(); 
}//end in wireless mode breathe blue led

if (usbQcON == true && inQcMODE == false){ //Qc on actions (once)
  lineOneTEXT = ("ON");
  lineTwoTEXT = ("USB");
  lastDisplayTEXT = millis ();
  digitalWrite(usbOnPIN,HIGH); 
  inQcMODE = true; 
} //end Pd on actions

if (usbQcON == false && inQcMODE == true){ //Qc off actions (once)
  lineOneTEXT = ("OFF");
  lineTwoTEXT = ("USB");
  lastDisplayTEXT = millis ();
  digitalWrite(usbOnPIN,LOW); 
  inQcMODE = false; 
} //end Qc on actions    

if (moduleOn == true && inModuleMODE == false){ //module on actions (once)
  lineOneTEXT = ("ON");
  lineTwoTEXT = ("EXTR");
  lastDisplayTEXT = millis ();
  digitalWrite(modulePin,HIGH); 
  inModuleMODE = true; 
} //end module on actions

if (moduleOn == false && inModuleMODE == true){ //module off actions (once)
  lineOneTEXT = ("OFF");
  lineTwoTEXT = ("EXTR");
  lastDisplayTEXT = millis ();
  digitalWrite(modulePin,LOW); 
  inModuleMODE = false; 
} //end module on actions

if(moduleOn == true || usbQcON == true || wirelessOn == true) {  //display battery percentage periodocally 
   if (currentMillis - lastDisplayTEXT > 2100) {
  timeSinceUPDATE = millis();
   if(timeSinceUPDATE - lastUpdateTIME >= voltageUpdateTIME) {   
      mapVoltages (); //map voltages onto displays and prencntages
      displayOff();
      matrixDisplayPRECENT (); //display battery percent
    lastUpdateTIME = timeSinceUPDATE; //resetting the update timer
   }
   }//end update charging status screens
}//end display voltages

if(turnOnFINISHED == true){
if (currentMillis - lastDisplayTEXT <= 2000){ //display texts for two seconds
     displayTexts();
  }//end display texts
if (currentMillis - lastDisplayTEXT > 2000 && currentMillis - lastDisplayTEXT < 2100){
     displayOff ();
  }//end display texts
}

if (ringVoltageON == true && currentMillis - firstOnTIME >= 3000) {  //after interval, turn off all LEDs and display 
   LEDFadeBLACK ();
   displayOff ();
   turnOnFINISHED = true; 
   ringVoltageON = false; 
  }//end LEDs fading to black.


if(currentMillis - lastInputVOLTAGEhigh > 3500) {
  inputVoltageLOW = true;
}//end latching input voltage

if(inputVoltage > 10){
  lastInputVOLTAGEhigh = currentMillis;
  inputVoltageLOW = false;
}

if(chargingMode == true){ //action to do when input voltage is in range
   inChargingMODE = true; //indicate that charging mode state;
  for(int k = 0; k < 1; k++) {
     LEDRingDisplayVOLTAGE ();
  }//end initial voltage display
   lastChargeCURRENT = millis();
   timeSinceUPDATE = millis(); 
   timeSinceFLASH = millis();
   if(timeSinceUPDATE - lastUpdateTIME >= voltageUpdateTIME) {   
      mapVoltages (); //map voltages onto displays and prencntages
      displayOff();
      matrixDisplayPRECENT (); //display battery percent
      ledRingOFF ();
      LEDS.setBrightness(LEDBrightness);      // 0-255 value
      fill_solid( &(leds[0]), numLEDsVOLT, CRGB( greenLEDsINTENSITY, 180, 0) );
      FastLED.show(); 
    lastUpdateTIME = timeSinceUPDATE; //resetting the update timer
   }//end update charging status screens

    if(timeSinceFLASH - lastFlashTIME >= chargingFlashTIME) {
      if(chargeFlashLED == false) {
     leds[numLEDsVOLT].setRGB( greenLEDsINTENSITY, 180, 0);
     FastLED.show();
     chargeFlashLED = true;
    }
    else {
     leds[numLEDsVOLT] = CRGB::Black; //set led to black
     FastLED.show();
     chargeFlashLED = false;
    }
    lastFlashTIME = timeSinceFLASH; //resetting the Flash timer
   }//end flash charging status LED
   if(batteryTemp <= 40 && chargingTemp <= 55){ //if temps are low, use full charging speed
  if(inputVoltage < 6){ //if voltage is less than 6V use charge using 10W
    pot.setPot(10);
  } //end first 10W charging mode

  if(inputVoltage >= 6 && inputVoltage < 14){ //15W charging mode
    pot.setPot(20);
  } //end 15W charging mode

  if(inputVoltage >= 14 && inputVoltage < 17){ //30W charging mode
    pot.setPot(25);
  } //end 30W charging mode

  if(inputVoltage >= 17 && inputVoltage < 21){ //40W charging mode
    pot.setPot(35);
  } //end 40W charging mode

  if(inputVoltage >= 21 && inputVoltage < 28){ //60W charging mode
    pot.setPot(54);
  } //end 60W charging mode
delay(10);
   }//end normal charging mode
else{
  pot.setPot(10);
} //end thermal throttle

if(chargingAmp > 0.2){
  lastCurrentCHECK = lastChargeCURRENT;
  Serial.println("LOW CHARG CURR");
}

if(lastChargeCURRENT - lastCurrentCHECK >= chargeCurrentCHECK){
  chargeCurrentLOW = true;
  Serial.println("low charging current trigger");
}
   
digitalWrite(relayPin, HIGH);
}//end if charging mode is on

if(chargingMode == false && inChargingMODE == true ) {
  inChargingMODE = false;
  LEDFadeBLACK ();
  displayOff ();
  pot.setPot(0);    //set pot to x% of highest value  
  delay(50);
  digitalWrite(relayPin, LOW);
} 

if(batteryVoltage <= fullBatteryVOLTAGE){
  chargeCurrentLOW = false;
  //Serial.println("charge resert");
}
  
if (overCurrentPROTEC ==  true || overTempPROTEC == true || overVoltPROTEC == true || batteryFaultPROTEC == true) { //activate warninng LED if system exceedes nominal value
  displayTexts ();
  if (currentMillis - preMillisWARN >= 1500 && warnTimesLED <= 4) {
     LEDS.setBrightness(20); 
     fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 0, 255, 0) ); //fill all leds red.
     FastLED.show();
     delay (100);
     ledRingOFF ();
     
     Serial.print("OVER CURRENT text 1");
     preMillisWARN = currentMillis;
     warnTimesLED ++;
  }//end for loop warning lights
}//end over current protection


if(flashlightMode == true){
 switch (ledMode) {
      case 0:  coolWhite(); break;
       case 1:  warmWhite();  break;
       case 2:  pureRed(); break;
       case 3:  pureOrange(); break;
       case 4:  pureYellow(); break;
       case 5:  pureGreen(); break;      
       case 6:  pureBlue();  break; 
       case 7:  purePurple (); break;
       case 8:  rainbow (); break;
       case 9:  confetti (); break;
       case 10: rainbowComet (); break;
 }//end define led modes
 EVERY_N_MILLISECONDS( 30 ) { gHue++; }
      FastLED.show();
}//end if flashlight mode on

}//end void loop

void readSensors() { //OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
  
batteryTempRAW = inMux.read(4);         //read overall powerbank power consumption
chargingTempRAW = inMux.read(6);       // read module power consumptionn
caseTempRAW = inMux.read(5);            //read USB & wireless charging power consumption 
moduleTempRAW = inMux.read(7);       //read battery temp raw
batteryVoltageRAW = inMux.read(0);      //read charging circuit temp
inputVoltageRAW = inMux.read(3);    //read battery voltage
powerbankAmpRAW = inMux.read(2);      //read charging input voltage
chargingAmpRAW = inMux.read(1);         //see if module 1 is connected

moduleConnectRAW = analogRead(A6);          //read module connect status

//calculating amps in and out:
  Voltage = (chargingAmpRAW / 1023.0) * 5000;                   // Gets mV for modules amps
  chargingAmp = ((Voltage - ACSoffset) / mVperAmp20A);

  Voltage = (powerbankAmpRAW / 1023.0) * 5000;                   // Gets mV for modules amps
  powerbankAmp = ((Voltage - ACSoffset) / mVperAmp20A);


//calculating temperatures:
  R2 = R1 * ((float)batteryTempRAW / (1023.0 - (float)batteryTempRAW)); //calculating battery temp
  logR2 = log(R2);
  batteryTemp = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  batteryTemp = batteryTemp - 273.15;

  R2 = R1 * ((float)chargingTempRAW / (1023.0 - (float)chargingTempRAW));         //calculating charging temp
  logR2 = log(R2);
  chargingTemp = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  chargingTemp = chargingTemp - 273.15;

  R2 = R1 * ((float)caseTempRAW / (1023.0 - (float)caseTempRAW));              //calculating case temperature
  logR2 = log(R2);
  caseTemp = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  caseTemp = caseTemp - 273.15;

  R2 = R1 * ((float)moduleTempRAW / (1023.0 - (float)moduleTempRAW));                //calculating case temperature
  logR2 = log(R2);
  moduleTemp = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  moduleTemp = moduleTemp - 273.15;

//calculating voltages:
  VinMv = (batteryVoltageRAW * 5.0) / 1023.0;                 //battery voltage calculation
  batteryVoltage = VinMv / (VdivR2/(VdivR0+VdivR2)); 

  VinMv = (inputVoltageRAW * 5.0) / 1023.0;                   //input voltage calculation
  inputVoltage = VinMv / (VdivR2/(VdivR1+VdivR2)); 

if ( moduleConnectRAW > 700 ) {                                 //compute module connected or not
  moduleConnect = 1;
}
else {
  moduleConnect = 0;
}

 powerOutputWATT = (batteryVoltage * powerbankAmp);   //calculating power output in watts

Serial.println(batteryTemp);
Serial.println(chargingTemp);
Serial.println(caseTemp);
Serial.println(moduleTemp);
Serial.println(moduleConnect);
Serial.println(batteryVoltage);
Serial.println(inputVoltage);
Serial.println(powerbankAmp);
Serial.println(chargingAmp);
Serial.println(powerOutputWATT);
Serial.println(" ");
//delay(1000);

}//end Void Readsensors.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXxx


void interpretValues () {
if (powerbankAmp > currentLimit) { //if powerbank exceeds current limit, trigger overcurrent protection
  overCurrentPROTEC = true;
  warnTimesLED = 0;
  outputOff ();
  lineOneTEXT = ("OVER");
  lineTwoTEXT = ("POW");
  lastDisplayTEXT = millis ();
  displayTexts ();
} //end if over current protection
else {
  overCurrentPROTEC = false;
}

if (batteryTemp > 52 || caseTemp > 65 || chargingTemp > 66 || moduleTemp > 65) { //if these temperatures (in degrees C) are exceded, trigger over temp protection
  outputOff ();
  lineOneTEXT = ("OVER");
  lineTwoTEXT = ("TEMP");
  lastDisplayTEXT = millis ();
  displayTexts ();
}//end if over temp protection
else{
  overTempPROTEC = false;
}//end over general temp protection


if (batteryVoltage < minBatteryVOLTAGE || batteryVoltage > (maxBatteryVOLTAGE + 1)) { //if battery is under min voltage, trigger under voltage protection.
  batteryFaultPROTEC = true;
  outputOff ();
  warnTimesLED = 0;
  lineOneTEXT = ("BATT");
  lineTwoTEXT = ("BAD");
  displayTexts ();
}
else {
  batteryFaultPROTEC = false;
}//end under voltage protection


if (batteryVoltage < emptyBatteryVOLTAGE && batteryVoltage > minBatteryVOLTAGE) { //low power shut off
  outputOff ();
  lineOneTEXT = ("LOW");
  lineTwoTEXT = ("BATT");
  matrix.fillScreen(LOW); 
   matrix.write();
   matrix.setCursor(0,5);
  matrix.print(lineOneTEXT);
  matrix.write();
  delay(800);
  matrix.setCursor(0,5);
   matrix.fillScreen(LOW); 
   matrix.write();
  matrix.print(lineTwoTEXT);
  matrix.write();
  delay(800);
  powerOff ();
} //end low voltage power off


if (inputVoltageLOW == false && inputVoltage < 28 && batteryFaultPROTEC == false && overTempPROTEC == false && chargeCurrentLOW == false ) { //if input voltage is over 4.5V, and if over 28V, dont charge to protect electronics
  chargingMode = true;
}
else{
  chargingMode = false;
}
//end if enter charging mode

if(moduleTemp > 60) { //if module temp is grater than x, turn it off
  moduleOn = false;
  digitalWrite(modulePin,LOW);
}//end module over heat

   int higherTemp = max(caseTemp,chargingTemp); //takes the higher temperature value of the two sensors
   //Serial.println(higherTemp);

   if (fanOn == false && higherTemp > 42){ //if temperature is lower than fanOnTEMP, turn off fan
     digitalWrite (fanPin, HIGH);
     fanOn = true;
   }

   if (fanOn == true && higherTemp < 37){ //if temperature is lower than fanOnTEMP, turn off fan
     digitalWrite (fanPin, LOW);
     fanOn = false;
   }//end fan on or off control
   
} //end interpret values XXXXXXXXXXXXXXXX

void displayTexts (){ //display two line texts for warning, status and power off/on
  if(currentMillis - lastTextON > textDelay){
    lastTextON = millis();
  if(line1 == false){
    line1 = true;
   matrix.fillScreen(LOW); 
   matrix.write();
   matrix.setCursor(0,5);
matrix.print(lineTwoTEXT);
  matrix.write();
  }
  else{
  matrix.setCursor(0,5);
   matrix.fillScreen(LOW); 
   matrix.write();
  matrix.print(lineOneTEXT);
  matrix.write();
  line1 = false;
  displayTextCYCLE++;
  }//end else
  }
}//end matrix display texts


void LEDFadeBLACK () {
  Serial.println("turning off leds");  
    for (int fadeValue = LEDBrightness; fadeValue >= 1; fadeValue --) {
      LEDS.setBrightness(fadeValue); 
      FastLED.show();
      //fadeValue --;
      delay(20);
      } //end reduceing value      
      ringVoltageON = false;
    ledRingOFF ();
}//end LED fade balck

void mapVoltages () { //map voltages onto display value OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOo
   int ADCbatteryV = batteryVoltageRAW; //batteryVoltageRAW goes here
  
  if( ADCbatteryV >= fullBatteryADC ) {
    ADCbatteryV = fullBatteryADC;
  }

   if( ADCbatteryV <= emptyBatteryADC ) {
    ADCbatteryV = emptyBatteryADC;
  }
 
  greenLEDsINTENSITY = map(ADCbatteryV, emptyBatteryADC, fullBatteryADC, 0, 255); //maps battery voltage to LED color, lower voltage, more red. Higher voltage, more green
  numLEDsVOLT = map(ADCbatteryV, emptyBatteryADC, fullBatteryADC, 0, NUM_LEDS);         //maps battery voltage to number of leds, higher voltage, more LEDs lit.
  batteryPercentage = map(ADCbatteryV, emptyBatteryADC, fullBatteryADC, 0, 100);  //maps battery voltage to a precentage number (0-100%)
}//end map Voltages


void LEDRingDisplayVOLTAGE () { //LED ring display voltage OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
//Serial.println(numLEDsVOLT);
LEDS.setBrightness(LEDBrightness);      // 0-255 value
for(int i = 0; i <= numLEDsVOLT; i++) {
    // Set the i'th led to red 
    leds[i] = CRGB(greenLEDsINTENSITY, 180, 0);
    // Show the leds
    FastLED.show(); 
    delay(30);
    } //end for loop led
} //end void LEDRingDisplayVOLTAGE

void matrixDisplayPRECENT () { //displays battery precentage onto matrix OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
  
  matrix.setIntensity(1); // Use a value between 0 and 7 for brightness
  matrix.setRotation(1);
  matrix.setMirror(true);   // set X-mirror true when using the WeMOS D1 mini Matrix LED Shield (X0=Seg1/R1, Y0=GRD1/C1)
  matrix.setFont(&Picopixel);
  if(batteryPercentage < 10) { //center screen if less than 10%
    matrix.setCursor(5,5);
  }//end if
   if(batteryPercentage >= 10 && batteryPercentage < 100) { //center screen if between 10 and 100 percent
    matrix.setCursor(3,5);
  }//end if 
   if(batteryPercentage == 100) { //center screen if 100 percent
    matrix.setCursor(1,5);
  }//end if 
  matrix.print(batteryPercentage);
  matrix.println("%");
  matrix.write();
  matrixVoltageON = true;
} //end matrixDisplayPRECENT;

//BEGIN LED modes in flashlightmode ----------------------------
void coolWhite () {
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 255, 255, 255) ); //fill all leds cool white
}//end cool white set code

void warmWhite () { //warm white led mode
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 200, 255, 30) ); //fill all leds warm white
}//end warm white set code

void pureRed () { 
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 0, 255, 0) ); //fill all leds red
}//end pure red set code

void pureOrange () {
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 30, 255, 0) ); //fill all leds orange color
}//end pure orange set code

void pureYellow () {
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 110, 255, 0) ); //fill all leds yellow color
}//end pure yellow  set code

void pureGreen () {
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 255, 0, 0) ); //fill all leds green
}//end pure green set code

void pureBlue () {
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 0, 0, 255) ); //fill all leds warm blue
}//end pureBlue set code

void purePurple () {
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fill_solid( &(leds[0]), 24 /*number of leds*/, CRGB( 0, 255, 170) ); //fill all leds purple
}//end purple set code

void rainbow() {
  // FastLED's built-in rainbow generator
   FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fill_rainbow( leds, NUM_LEDS, gHue, 3);
}//end rainbow mode

void confetti() {  // random colored speckles that blink in and fade smoothly
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}//end confetti

void rainbowComet() {  // a colored dot sweeping back and forth, with fading trails
  FastLED.setBrightness(flashlightBrightness); //set led to flashlight brightness.
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
} //end rainbowComet

//end flashlight Modes

void displayOff () {
  Serial.print("display off");
  matrix.fillScreen(LOW); 
   matrix.write();
} //end turning led display off

void ledRingOFF () {
   Serial.print("Led ring off");
   FastLED.clear();  // clear all pixel data
  FastLED.show(); //this refeshes the led display data
}

void outputOff () {
  Serial.print("Output off");
  flashlightMode = false;
  ledRingOFF (); 
  usbQcON = false;
  moduleOn = false;
  wirelessOn = false;
  digitalWrite(usbOnPIN,LOW);
  digitalWrite(modulePin,LOW);
  digitalWrite(wirelessPin,LOW);
} //end turing modules off actions

void powerOff () {
  Serial.print("Powering OFF");
  flashlightMode = false; 
  outputOff();
  displayOff();
  lineOneTEXT = ("Powr");
  lineTwoTEXT = ("OFF");
   matrix.fillScreen(LOW); 
   matrix.write();
   matrix.setCursor(0,5);
  matrix.print(lineOneTEXT);
  matrix.write();
  delay(800);
  matrix.setCursor(0,5);
   matrix.fillScreen(LOW); 
   matrix.write();
  matrix.print(lineTwoTEXT);
  matrix.write();
  delay(800);
  digitalWrite(powerPin,LOW);
} //end poweroff actions
