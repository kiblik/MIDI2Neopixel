// Developed in Arduino IDE 1.8.13 (Windows)

#include <FastLED.h> // FastLED version 3.3.3 https://github.com/FastLED/FastLED
#include "MIDIUSB.h" // MIDIUSB version 1.0.4 https://www.arduino.cc/en/Reference/MIDIUSB
#include <arduino-timer.h>

#define BUTTON_PIN1 3
#define BUTTON_PIN2 2
#define LED_ACT_PIN 4
#define LED_PIN     5
#define MAX_LEDS    512
#define DEFAULT_LEDS 300
uint16_t NUM_LEDS = DEFAULT_LEDS;
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define AMPERAGE    10000     // Power supply amperage in mA

#define PR_AUTOLEARN 0
#define PR_RGB_ACYC 1
#define PR_RGB_CYC 2
#define PR_DEFAULT PR_RGB_ACYC

#define MAX_PROGRAM 2 // 0..2
#define LED_INT_STEPS 8      // LED Intensity button steps
//#define DEBUG

CRGB leds[MAX_LEDS];
byte program = PR_DEFAULT;
#define ledIntensity LED_INT_STEPS // default LED brightness
byte ledBrightness = ceil(255 / LED_INT_STEPS * ledIntensity); // default LED brightness
byte FPS = 12;

Timer<1> timer;
bool enableRender = false;
uint16_t processedMsgs = 1; // needs to be 1 for first render

//------------------------------------------- MAPPING ----------------------------------------------//

#define CC_DEFAULT_R 0x11
#define CC_DEFAULT_G 0x12
#define CC_DEFAULT_B 0x13
#define CC_DEFAULT_INTENSITY 0x14
#define CC_DEFAULT_POSITION 0x09
#define CC_DEFAULT_SIZE 0x02
#define CC_DEFAULT_FADEIN 0x03
#define CC_DEFAULT_FADEOUT 0x04
#define CC_DEFAULT_BASE_R 0x15
#define CC_DEFAULT_BASE_G 0x16
#define CC_DEFAULT_BASE_B 0x17
#define CC_DEFAULT_BASE_INTENSITY 0x18
#define CC_DEFAULT_FPS 0x05
#define CC_DEFAULT_LENGTH_LOW 0x06
#define CC_DEFAULT_LENGTH_HIGH 0x07

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
uint8_t CC_FPS = CC_DEFAULT_FPS;
uint8_t CC_LENGTH_LOW = CC_DEFAULT_LENGTH_LOW;
uint8_t CC_LENGTH_HIGH = CC_DEFAULT_LENGTH_HIGH;

#define MAX_CONFIG 14 // 0..14
#define OFFSET_CONFIG 0

byte config_id = 0;

typedef struct { 
  uint8_t r = 255;
  uint8_t g = 255;
  uint8_t b = 255;
  uint8_t intensity = 255;
  uint16_t position = 0;
  uint8_t size = 0;
  uint8_t fadein = 0;
  uint8_t fadeout = 0;
} point_t;

#define NUM_POINTS 1

point_t points[NUM_POINTS];

uint8_t base_r = 0;
uint8_t base_g = 0;
uint8_t base_b = 0;
uint8_t base_intensity = 0;


//------------------------------------------- CONFIG ----------------------------------------------//
void showConfig(byte value){
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  leds[(config_id % (MAX_CONFIG+1)) + OFFSET_CONFIG].setRGB(value, value, value);
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
    case 12:
      CC_FPS = control;
      break;
    case 13:
      CC_LENGTH_LOW = control;
      break;
    case 14:
      CC_LENGTH_HIGH = control;
      break;
    default:
      #if defined(DEBUG)
        Serial.print(F("Control change: control="));
        Serial.print(control, HEX);
        Serial.print(F(", value="));
        Serial.println(value, HEX);
      #endif
      return;
  }  
  showConfig(value);
}
void setConfig(byte newConfig){
  config_id = newConfig;
  #if defined(DEBUG)
    Serial.print(F("Setting configuration for "));
    Serial.println(String(config_id)); 
  #endif
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
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  leds[program % (MAX_PROGRAM+1)] = CRGB::Pink;
}
void setProgram(byte newProgram){
  program = newProgram;
  #if defined(DEBUG)
    Serial.print(F("Program is "));
    Serial.println(String(program));
  #endif
  if (program == PR_AUTOLEARN) {
    showProgram();
    delay(100);
    setConfig(0);
  } else {
    showProgram();
    delay(100);
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
  }
  #if defined(DEBUG)
    else
      Serial.println(F("Unsupported program"));
  #endif
  return newLed;
}

void update(){

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i].r = round( base_r * base_intensity / 255 );
    leds[i].g = round( base_g * base_intensity / 255 );
    leds[i].b = round( base_b * base_intensity / 255 );
  }

  for (uint8_t point_id = 0; point_id < NUM_POINTS; point_id++){
    point_t point = points[point_id];
    if (point.size > 0) {
      //fade in
      int j = point.fadein + 1;
      for (int i = max(0, point.position - point.size/2 - point.fadein); i < min(NUM_LEDS, (int)(point.position - point.size/2)); i++){
        if ( program == PR_RGB_ACYC )
          if ( ( i < 0 ) || ( i >= (int)NUM_LEDS ) )
            continue;
        leds[i % NUM_LEDS] = setPixel( leds[i % NUM_LEDS], point, (double) 1 / (j*j) );
        j--;
      }
      // core
      for (int i = max(0, point.position - point.size/2); i < min(NUM_LEDS, (int)(point.position + (point.size+1)/2)); i++){
        if ( program == PR_RGB_ACYC )
          if ( ( i < 0 ) || ( i >= (int)NUM_LEDS ) )
            continue;
        leds[i % NUM_LEDS] = setPixel( leds[i % NUM_LEDS], point );
      }
      // fade out
      j = 1;
      for (int i = max(0, point.position + (point.size+1)/2); i < min(NUM_LEDS, (int)(point.position + (point.size+1)/2 + point.fadeout)); i++){
        j++;
        if ( program == PR_RGB_ACYC )
          if ( ( i < 0 ) || ( i >= (int)NUM_LEDS ) )
            continue;
        leds[i % NUM_LEDS] = setPixel( leds[i % NUM_LEDS], point, (double) 1 / (j*j) );
      }
    }
  }

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
        points[channel].position = map( value, 0, 127, 0, NUM_LEDS-1);
      else if ( program == PR_RGB_CYC )
        points[channel].position = value % NUM_LEDS;
    }
    else if ( control == CC_SIZE )
      points[channel].size = min(NUM_LEDS, value);
    else if ( control == CC_FADEIN )
      points[channel].fadein = min(NUM_LEDS/3, value);
    else if ( control == CC_FADEOUT )
      points[channel].fadeout = min(NUM_LEDS/3, value);
    else if ( control == CC_BASE_R ) 
      base_r = value * 2;
    else if ( control == CC_BASE_G )
      base_g = value * 2;
    else if ( control == CC_BASE_B )
      base_b = value * 2;
    else if ( control == CC_BASE_INTENSITY )
      base_intensity = value * 2;
    else if ( control == CC_FPS ){
      FPS = min(2500, max(1000/50, value));
      timer.cancel();
      initRender();
    }
    else if ( (control == CC_LENGTH_LOW) || (control == CC_LENGTH_HIGH) ){
      uint16_t new_NUM_LEDS;
      if ( control == CC_LENGTH_LOW )
        new_NUM_LEDS = (NUM_LEDS & 0xFF80) | (value & 0x007F);      
      else
        new_NUM_LEDS = (NUM_LEDS & 0x007F) | ( (value << 7) & 0xFF80);
      if (( 0 < new_NUM_LEDS ) && ( new_NUM_LEDS <= MAX_LEDS )){
        enableRender = false;
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Black;
        }
        FastLED.show();
        NUM_LEDS = new_NUM_LEDS;
        #if defined(DEBUG)
          Serial.print(F("New NUM_LEDS: "));
          Serial.println(NUM_LEDS); 
        #endif  
        initLeds();
      }
    }
    else {
      #if defined(DEBUG)
        Serial.print(F("Control change: control="));
        Serial.print(control, HEX);
        Serial.print(F(", value="));
        Serial.print(value, HEX);
        Serial.print(F(", channel="));
        Serial.println(channel);
      #endif
      return;
    }

    processedMsgs++;

    #if defined(DEBUG)
      Serial.print("R=");
      Serial.print(points[channel].r, HEX);
      Serial.print(", G=");
      Serial.print(points[channel].g, HEX);
      Serial.print(", B=");
      Serial.print(points[channel].b, HEX);
      Serial.print(F(", intensity="));
      Serial.print(points[channel].intensity, HEX);
      Serial.print(F(", position="));
      Serial.print(points[channel].position, HEX);
      Serial.print(F(", size="));
      Serial.print(points[channel].size, HEX);
      Serial.print(F(", fadein="));
      Serial.print(points[channel].fadein, HEX);
      Serial.print(F(", fadeout="));
      Serial.print(points[channel].fadeout, HEX);
      Serial.print(F(", channel="));
      Serial.println(channel);
    #endif
  }
}

//-------------------------------------------- RENDER ---------------------------------------------//

bool render(__attribute__((unused)) void *argument) {
  if ((program != PR_AUTOLEARN) && (enableRender) && (processedMsgs > 0)) {
    update();
    #if defined(DEBUG)
      Serial.println(F("Render"));
    #endif
    FastLED.show();
    processedMsgs = 0;
  }
  
  return true;
}

void initRender() {
  uint16_t time = 1000 / FPS;
    #if defined(DEBUG)
      Serial.print(F("Render time: "));
      Serial.println(time);
    #endif
  timer.every( time, render);
}

void initLeds() {
  pinMode(LED_ACT_PIN, OUTPUT);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(ledBrightness);
  fill_solid( leds, NUM_LEDS, CRGB(0,0,0));

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Pink;
  }
  FastLED.show();
  delay( 10 );
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();

  points[0].position = points[0].size = 1;

  enableRender = true;
}

//-------------------------------------------- SETUP ----------------------------------------------//

void setup() {
  #if defined(DEBUG)
    Serial.begin(115200);
  #endif
  delay( 3000 ); // power-up safety delay
  FastLED.setMaxPowerInVoltsAndMilliamps(5, AMPERAGE);
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  initRender();
  initLeds();
  setProgram(PR_DEFAULT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN1), [](){
    nextProgram();
    delay(1000);
  }, RISING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN2), [](){
    if (program == PR_AUTOLEARN) { 
      nextConfig();
    }
    delay(1000);
  }, RISING);
  #if defined(DEBUG)
    Serial.println(F("MIDI2Neopixel is ready!"));
  #endif
}

//--------------------------------------------- LOOP ----------------------------------------------//
void loop() {
  timer.tick();

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

    #if defined(DEBUG)
    default:
        Serial.print(F("Unhandled MIDI message: "));
        Serial.print(rx.header, HEX);
        Serial.print("-");
        Serial.print(rx.byte1, HEX);
        Serial.print("-");
        Serial.print(rx.byte2, HEX);
        Serial.print("-");
        Serial.println(rx.byte3, HEX);
    #endif
  }

  if (rx.header) {digitalWrite(LED_ACT_PIN, LOW);}
}
//------------------------------------------ END OF LOOP -------------------------------------------//
