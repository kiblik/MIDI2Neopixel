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

#define MAX_LED_PROGRAM 2
#define LED_INT_STEPS 8      // LED Intensity button steps
#define DEBUG         true

CRGB leds[NUM_LEDS];
byte color = 0xFFFFFF;
int program = 0;
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

#define CC_R 0xC // C
#define CC_G 0xD // S
#define CC_B 0xE // V
#define CC_INTENSITY 0xF
#define CC_POSITION 0x15
#define CC_SIZE 0x16
#define CC_FADEIN 0x17
#define CC_FADEOUT 0x18

typedef struct { 
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t h;
  uint8_t s;
  uint8_t v;
  uint8_t intensity;
  uint8_t position;
  uint8_t size;
  uint8_t fadein;
  uint8_t fadeout;
} point_t;

#define NUM_POINTS 16

point_t points[NUM_POINTS];

//------------------------------------------- PROGRAM ----------------------------------------------//

void setProgram(byte newProgram){
  program = newProgram;
  if (DEBUG == true){ Serial.println("Program is " + String(program)); }
}
void nextProgram(){  
  if (program>=MAX_LED_PROGRAM){
    setProgram(0);
  }
  else {
    setProgram(program + 1);
  }
  update();
}
//------------------------------------------- Control ----------------------------------------------//

CRGB setPixel(CRGB newLed, point_t point, double fade = 1){
  if (program == 0) { // RGB
    newLed.setRGB(
      round( point.r * fade * point.intensity / 255 ),
      round( point.g * fade * point.intensity / 255 ),
      round( point.b * fade * point.intensity / 255 )
    );
  } else if (program == 1) { // HSV
    newLed.setHSV(
      round( point.h ),
      round( point.s ),
      round( point.v * fade * point.intensity / 255 )
    );
  } else
    Serial.println('Unsupported program');
  return newLed;
}

void update(){
  CRGB newLeds[NUM_LEDS];

  for (int i = 0; i < NUM_LEDS; i++) {
    newLeds[i] = 0;
  }
  for (int point_id = 0; point_id < NUM_POINTS; point_id++){
    point_t point = points[point_id];
    if (point.size > 0) {
      //fade in
      int j = 0;
      for (int i = max(0, point.position - point.size/2 - point.fadein ); i < min(NUM_LEDS, point.position - point.size/2 ); i++){
        j++;
        newLeds[i] = setPixel( newLeds[i], point, (double) j / (point.fadein+1) );
      }
      // core
      for (int i = max(0, point.position - point.size/2 ); i < min(NUM_LEDS, point.position + (point.size+1)/2 ); i++){
        newLeds[i] = setPixel( newLeds[i], point );
      }
      // fade out
      j = point.fadeout;
      for (int i = max(0, point.position + (point.size+1)/2 ); i < min(NUM_LEDS, point.position + (point.size+1)/2 + point.fadeout); i++){
        newLeds[i] = setPixel( newLeds[i], point, (double) j / (point.fadeout+1) );
        j--;
      }
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = newLeds[i];
  }

  FastLED.show();
}

void controlChange(byte channel, byte control, byte value) {

  switch (control) {
    case CC_R:
      points[channel].r = points[channel].h = value * 2;
      break;
    case CC_G:
      points[channel].g = points[channel].s = value * 2;
      break;
    case CC_B:
      points[channel].b = points[channel].v = value * 2;
      break;
    case CC_INTENSITY:
      points[channel].intensity = value * 2;
      break;
    case CC_POSITION:
      points[channel].position = value;
      break;
    case CC_SIZE:
      points[channel].size = value;
      break;
    case CC_FADEIN:
      points[channel].fadein = value;
      break;
    case CC_FADEOUT:
      points[channel].fadeout = value;
      break;
    default:
      Serial.print("Control change: control=");
      Serial.print(control, HEX);
      Serial.print(", value=");
      Serial.print(value, HEX);
      Serial.print(", channel=");
      Serial.println(channel);
      return;
  }
  // Serial.print("R/H=");
  // Serial.print(points[channel].r, HEX);
  // Serial.print(", G/S=");
  // Serial.print(points[channel].g, HEX);
  // Serial.print(", B/V=");
  // Serial.print(points[channel].b, HEX);
  // Serial.print(", intensity=");
  // Serial.print(points[channel].intensity, HEX);
  // Serial.print(", position=");
  // Serial.print(points[channel].position, HEX);
  // Serial.print(", size=");
  // Serial.print(points[channel].size, HEX);
  // Serial.print(", fadein=");
  // Serial.print(points[channel].fadein, HEX);
  // Serial.print(", fadeout=");
  // Serial.print(points[channel].fadeout, HEX);
  // Serial.print(", channel=");
  // Serial.println(channel);

  update();

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
  FastLED.show();
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
        ledIntensity++;
        if (ledIntensity>LED_INT_STEPS){
          ledIntensity = 1;
        }
        // Map ledIntensity to ledBrightness
        ledBrightness = map(ledIntensity, 1, LED_INT_STEPS, 3, 255);
        FastLED.setBrightness(ledBrightness);
        if (DEBUG == true){     Serial.println("Button 2 pressed! Intensity is " + String(ledIntensity) + " and brightness to "+ String(ledBrightness)); }
        FastLED.show();
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
