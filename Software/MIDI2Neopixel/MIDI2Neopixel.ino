// Developed in Arduino IDE 1.8.13 (Windows)

#include <FastLED.h> // FastLED version 3.3.3 https://github.com/FastLED/FastLED
#include "MIDIUSB.h" // MIDIUSB version 1.0.4 https://www.arduino.cc/en/Reference/MIDIUSB

#define BUTTON_PIN1 3
#define BUTTON_PIN2 2
#define LED_ACT_PIN 4
#define LED_PIN     5
#define NUM_LEDS    12
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define AMPERAGE    1500     // Power supply amperage in mA

#define PR_AUTOLEARN 0
#define PR_RGB_ACYC 1
#define PR_RGB_CYC 2
#define PR_DEFAULT PR_RGB_ACYC

#define MAX_PROGRAM 2 // 0..2
#define LED_INT_STEPS 8      // LED Intensity button steps
#define DEBUG         false

CRGB leds[NUM_LEDS];
byte color = 0xFFFFFF;
int program = PR_DEFAULT;
int ledIntensity = LED_INT_STEPS;  // default LED brightness
int ledBrightness = ceil(255 / LED_INT_STEPS * ledIntensity); // default LED brightness
int buttonState1;
int buttonState2;
int lastButtonState1 = LOW;
int lastButtonState2 = LOW;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long debounceDelay = 50;

//------------------------------------------- MAPPING ----------------------------------------------//

#define CC_DEFAULT_R 0x1C
#define CC_DEFAULT_G 0x1D
#define CC_DEFAULT_B 0x1E
#define CC_DEFAULT_INTENSITY 0x1F
#define CC_DEFAULT_POSITION 0x3C
#define CC_DEFAULT_SIZE 0x18
#define CC_DEFAULT_FADEIN 0x10
#define CC_DEFAULT_FADEOUT 0x66
#define CC_DEFAULT_BASE_R 0x34
#define CC_DEFAULT_BASE_G 0x35
#define CC_DEFAULT_BASE_B 0x36
#define CC_DEFAULT_BASE_INTENSITY 0x37

uint8_t CC_R = CC_DEFAULT_R;
uint8_t CC_G = CC_DEFAULT_G;
uint8_t CC_B = CC_DEFAULT_B;
uint8_t CC_INTENSITY = CC_DEFAULT_INTENSITY;
uint8_t CC_POSITION = CC_DEFAULT_POSITION;
uint8_t CC_SIZE = CC_DEFAULT_SIZE;
uint8_t CC_FADEIN = CC_DEFAULT_FADEIN;
uint8_t CC_FADEOUT = CC_DEFAULT_FADEOUT;
uint8_t CC_BASE_R = CC_DEFAULT_BASE_R;
uint8_t CC_BASE_G = CC_DEFAULT_BASE_G;
uint8_t CC_BASE_B = CC_DEFAULT_BASE_B;
uint8_t CC_BASE_INTENSITY = CC_DEFAULT_BASE_INTENSITY;

#define MAX_CONFIG 11 // 0..11

uint8_t config_id = 0;

typedef struct { 
  uint8_t r = 255;
  uint8_t g = 255;
  uint8_t b = 255;
  uint8_t intensity = 255;
  int8_t position = 0;
  uint8_t size = 0;
  uint8_t fadein = 0;
  uint8_t fadeout = 0;
} point_t;

#define NUM_POINTS 16

point_t points[NUM_POINTS];

uint8_t base_r = 0;
uint8_t base_g = 0;
uint8_t base_b = 0;
uint8_t base_intensity = 0;


//------------------------------------------- CONFIG ----------------------------------------------//
void showConfig(byte value){
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  leds[config_id % (MAX_CONFIG+1)].setRGB(value, value, value);
  FastLED.show();
}
void updateConfig( byte control, byte value){
  switch (config_id) {
    case 0:
      CC_R = control;
      break;
    case 1:
      CC_G = control;
      break;
    case 2:
      CC_B = control;
      break;
    case 3:
      CC_INTENSITY = control;
      break;
    case 4:
      CC_POSITION = control;
      break;
    case 5:
      CC_SIZE = control;
      break;
    case 6:
      CC_FADEIN = control;
      break;
    case 7:
      CC_FADEOUT = control;
      break;
    case 8:
      CC_BASE_R = control;
      break;
    case 9:
      CC_BASE_G = control;
      break;
    case 10:
      CC_BASE_B = control;
      break;
    case 11:
      CC_BASE_INTENSITY = control;
      break;
    default:
      Serial.print("Control change: control=");
      Serial.print(control, HEX);
      Serial.print(", value=");
      Serial.println(value, HEX);
      return;
  }  
  showConfig(value);
}
void setConfig(byte newConfig){
  config_id = newConfig;
  if (DEBUG == true){ Serial.println("Setting configuration for " + String(config_id)); }
  showConfig(128);
}
void nextConfig(){  
  if ( config_id >= MAX_CONFIG ){
    setConfig(0);
  }
  else {
    setConfig(config_id + 1);
  }
}

//------------------------------------------- PROGRAM ----------------------------------------------//
void showProgram(){
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  leds[program % (MAX_PROGRAM+1)] = CRGB::Pink;
  FastLED.show();
}
void setProgram(byte newProgram){
  program = newProgram;
  if (DEBUG == true){ Serial.println("Program is " + String(program)); }
  if (program == PR_AUTOLEARN) {
    showProgram();
    delay(100);
    setConfig(0);
  } else {
    showProgram();
    delay(100);
    update();
  }
}
void nextProgram(){  
  if ( program >= MAX_PROGRAM ){
    setProgram(0);
  }
  else {
    setProgram(program + 1);
  }
}
//------------------------------------------- Control ----------------------------------------------//

CRGB setPixel(CRGB newLed, point_t point, double fade = 1){
  if ( ( program == PR_RGB_ACYC ) || ( program == PR_RGB_CYC ) ) { // RGB
    newLed.setRGB(
      max( newLed.r, round( point.r * fade * point.intensity / 255 )),
      max( newLed.g, round( point.g * fade * point.intensity / 255 )),
      max( newLed.b, round( point.b * fade * point.intensity / 255 ))
    );
  } else
    Serial.println('Unsupported program');
  return newLed;
}

void update(){
  CRGB newLeds[NUM_LEDS];

  for (int i = 0; i < NUM_LEDS; i++) {
    newLeds[i].r = round( base_r * base_intensity / 255 );
    newLeds[i].g = round( base_g * base_intensity / 255 );
    newLeds[i].b = round( base_b * base_intensity / 255 );
  }

  for (int point_id = 0; point_id < NUM_POINTS; point_id++){
    point_t point = points[point_id];
    if (point.size > 0) {
      //fade in
      int j = point.fadein + 1;
      for (int i = point.position - point.size/2 - point.fadein; i < point.position - point.size/2; i++){
        if ( program == PR_RGB_ACYC )
          if ( ( i < 0 ) || ( i >= NUM_LEDS ) )
            continue;
        newLeds[i % NUM_LEDS] = setPixel( newLeds[i % NUM_LEDS], point, (double) 1 / (j*j) );
        j--;
      }
      // core
      for (int i = point.position - point.size/2; i < point.position + (point.size+1)/2; i++){
        if ( program == PR_RGB_ACYC )
          if ( ( i < 0 ) || ( i >= NUM_LEDS ) )
            continue;
        newLeds[i % NUM_LEDS] = setPixel( newLeds[i % NUM_LEDS], point );
      }
      // fade out
      j = 1;
      for (int i = point.position + (point.size+1)/2; i < point.position + (point.size+1)/2 + point.fadeout; i++){
        j++;
        if ( program == PR_RGB_ACYC )
          if ( ( i < 0 ) || ( i >= NUM_LEDS ) )
            continue;
        newLeds[i % NUM_LEDS] = setPixel( newLeds[i % NUM_LEDS], point, (double) 1 / (j*j) );
      }
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = newLeds[i];
  }

  FastLED.show();
}

void controlChange(byte channel, byte control, byte value) {

  if (program == PR_AUTOLEARN) {
    updateConfig(control, value*2 );
    return;
  } else {
    if (control == CC_R) 
      points[channel].r = value * 2;
    else if ( control == CC_G )
      points[channel].g = value * 2;
    else if ( control == CC_B )
      points[channel].b = value * 2;
    else if ( control == CC_INTENSITY )
      points[channel].intensity = value * 2;
    else if ( control == CC_POSITION ) {
      if ( program == PR_RGB_ACYC )
        points[channel].position = max( 0, min( NUM_LEDS-1, value ) );
      else if ( program == PR_RGB_CYC )
        points[channel].position = value % NUM_LEDS;
    }
    else if ( control == CC_SIZE )
      points[channel].size = value;
    else if ( control == CC_FADEIN )
      points[channel].fadein = value;
    else if ( control == CC_FADEOUT )
      points[channel].fadeout = value;
    else if ( control == CC_BASE_R ) 
      base_r = value * 2;
    else if ( control == CC_BASE_G )
      base_g = value * 2;
    else if ( control == CC_BASE_B )
      base_b = value * 2;
    else if ( control == CC_BASE_INTENSITY )
      base_intensity = value * 2;
    else {
      Serial.print("Control change: control=");
      Serial.print(control, HEX);
      Serial.print(", value=");
      Serial.print(value, HEX);
      Serial.print(", channel=");
      Serial.println(channel);
      return;
    }

    if (DEBUG == true){ 
      Serial.print("R=");
      Serial.print(points[channel].r, HEX);
      Serial.print(", G=");
      Serial.print(points[channel].g, HEX);
      Serial.print(", B=");
      Serial.print(points[channel].b, HEX);
      Serial.print(", intensity=");
      Serial.print(points[channel].intensity, HEX);
      Serial.print(", position=");
      Serial.print(points[channel].position, HEX);
      Serial.print(", size=");
      Serial.print(points[channel].size, HEX);
      Serial.print(", fadein=");
      Serial.print(points[channel].fadein, HEX);
      Serial.print(", fadeout=");
      Serial.print(points[channel].fadeout, HEX);
      Serial.print(", channel=");
      Serial.println(channel);
    }

    update();
  }
}

//-------------------------------------------- SETUP ----------------------------------------------//

void setup() {
  if (DEBUG == true){ Serial.begin(115200); }
  delay( 3000 ); // power-up safety delay
  FastLED.setMaxPowerInVoltsAndMilliamps(5, AMPERAGE);
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(LED_ACT_PIN, OUTPUT);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(ledBrightness);
  fill_solid( leds, NUM_LEDS, CRGB(0,0,0));
  setProgram(PR_DEFAULT);
  if (DEBUG == true){ Serial.println("MIDI2Neopixel is ready!"); }
}

//--------------------------------------------- LOOP ----------------------------------------------//
void loop() {
  ////////////////////////////////////////////// Button 1 /////////////////////////////////////////////

  int reading1 = digitalRead(BUTTON_PIN1);

  if (reading1 != lastButtonState1) {
    lastDebounceTime1 = millis();
  }

  if ((millis() - lastDebounceTime1) > debounceDelay) {
    if (reading1 != buttonState1) {
      buttonState1 = reading1;

      // only toggle the LED if the new button state is HIGH
      if (buttonState1 == LOW) { nextProgram(); }
    }
  }

  ////////////////////////////////////////////// Button 2 /////////////////////////////////////////////

  int reading2 = digitalRead(BUTTON_PIN2);

  if (reading2 != lastButtonState2) {
    lastDebounceTime2 = millis();
  }

  if ((millis() - lastDebounceTime2) > debounceDelay) {
    if (reading2 != buttonState2) {
      buttonState2 = reading2;

      // only toggle the LED if the new button state is HIGH
      if (buttonState2 == LOW) { 
        if (program == PR_AUTOLEARN) { 
          nextConfig();
        }
      }
    }
  }

  ////////////////////////////////////////// MIDI Read routine ////////////////////////////////////////
  midiEventPacket_t rx = MidiUSB.read();
  if (rx.header) {digitalWrite(LED_ACT_PIN, HIGH);}
  switch (rx.header) {
    case 0:
      break; //No pending events
    
    case 0xB:
      controlChange(
        rx.byte1 & 0xF,  //channel
        rx.byte2,        //control
        rx.byte3         //value
      );
      break;

    case 0xC:
      setProgram(rx.byte2);
      break;

    case 0x8: // Note off
    case 0x9: // Note on
    case 0xA: // Polyphonic key Pressure
    case 0xD: // Channel Pressure
      break;  // we are ignoring all key and pressure messages

    default:
      if (DEBUG == true){ 
        Serial.print("Unhandled MIDI message: ");
        Serial.print(rx.header, HEX);
        Serial.print("-");
        Serial.print(rx.byte1, HEX);
        Serial.print("-");
        Serial.print(rx.byte2, HEX);
        Serial.print("-");
        Serial.println(rx.byte3, HEX);
      }
  }

  if (rx.header) {digitalWrite(LED_ACT_PIN, LOW);}
  lastButtonState1 = reading1;
  lastButtonState2 = reading2;
}
//------------------------------------------ END OF LOOP -------------------------------------------//
