#include <FastLED.h> // FastLED version 3.4.0 https://github.com/FastLED/FastLED
#include <MIDIUSB.h> // MIDIUSB version 1.0.5 https://www.arduino.cc/en/Reference/MIDIUSB
#include <timer-api.h> // arduino-timer-api version 0.1.0 https://github.com/sadr0b0t/arduino-timer-api

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

#define PR_RGB_ACYC 0
#define PR_RGB_CYC 1
#define PR_DEFAULT PR_RGB_ACYC

#define MAX_PROGRAM 1 // 0..1
#define LED_INT_STEPS 8      // LED Intensity button steps
//#define DEBUG

CRGB leds[MAX_LEDS];
byte program = PR_DEFAULT;
#define ledIntensity LED_INT_STEPS // default LED brightness
byte ledBrightness = ceil(255 / LED_INT_STEPS * ledIntensity); // default LED brightness
uint8_t FPS = 30;

bool enableRender = false;
uint8_t processedMsgs = 1; // needs to be 1 for first render

//------------------------------------------- MAPPING ----------------------------------------------//

#define CC_R 0x11
#define CC_G 0x12
#define CC_B 0x13
#define CC_INTENSITY 0x14
#define CC_POSITION 0x09
#define CC_SIZE 0x02
#define CC_FADEIN 0x03
#define CC_FADEOUT 0x04
#define CC_BASE_R 0x15
#define CC_BASE_G 0x16
#define CC_BASE_B 0x17
#define CC_BASE_INTENSITY 0x18
#define CC_FPS 0x05
#define CC_LENGTH_LOW 0x06
#define CC_LENGTH_HIGH 0x07

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

#if defined(DEBUG)
int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#endif

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
  showProgram();
  delay(100);
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
      for (int i = max(0, (int)point.position - point.size/2 - point.fadein); i < min((int) NUM_LEDS, (int)point.position - point.size/2); i++){
        #if defined(DEBUG)
            Serial.print(F("FadeIn  "));
            Serial.print(i);
        #endif
        #if defined(DEBUG)
          Serial.print(F("Free RAM: "));
          Serial.print(freeRam());
        #endif
        if ( program == PR_RGB_ACYC )
          if ( ( i < 0 ) || ( i >= (int)NUM_LEDS ) )
            continue;
        leds[i % NUM_LEDS] = setPixel( leds[i % NUM_LEDS], point, (double) 1 / (j*j) );
        j--;
        #if defined(DEBUG)
            Serial.println();
        #endif
      }
      // core
      for (int i = max(0, (int)point.position - point.size/2); i < min((int) NUM_LEDS, (int)point.position + (point.size+1)/2); i++){
        #if defined(DEBUG)
            Serial.print(F("Core    "));
            Serial.print(i);
        #endif
        #if defined(DEBUG)
          Serial.print(F("Free RAM: "));
          Serial.print(freeRam());
        #endif
        if ( program == PR_RGB_ACYC )
          if ( ( i < 0 ) || ( i >= (int)NUM_LEDS ) )
            continue;
        leds[i % NUM_LEDS] = setPixel( leds[i % NUM_LEDS], point );
        #if defined(DEBUG)
            Serial.println();
        #endif
      }
      // fade out
      j = 1;
      for (int i = max(0, (int)point.position + (point.size+1)/2); i < min((int) NUM_LEDS, (int)point.position + (point.size+1)/2 + point.fadeout); i++){
        #if defined(DEBUG)
            Serial.print(F("FadeOut "));
            Serial.print(i);
        #endif
        #if defined(DEBUG)
          Serial.print(F("Free RAM: "));
          Serial.print(freeRam());
        #endif
        j++;
        if ( program == PR_RGB_ACYC )
          if ( ( i < 0 ) || ( i >= (int)NUM_LEDS ) )
            continue;
        leds[i % NUM_LEDS] = setPixel( leds[i % NUM_LEDS], point, (double) 1 / (j*j) );
        #if defined(DEBUG)
            Serial.println();
        #endif
      }
    }
  }
}

void controlChange(byte channel, byte control, byte value) {

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
    FPS = max(1, value);
    timer_stop_ISR(_TIMER1);
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

  #if defined(DEBUG)
    Serial.print(F("R="));
    Serial.print(points[channel].r, HEX);
    Serial.print(F(", G="));
    Serial.print(points[channel].g, HEX);
    Serial.print(F(", B="));
    Serial.print(points[channel].b, HEX);
    Serial.print(F(", intensity="));
    Serial.print(points[channel].intensity);
    Serial.print(F(", position="));
    Serial.print(points[channel].position);
    Serial.print(F(", size="));
    Serial.print(points[channel].size);
    Serial.print(F(", fadein="));
    Serial.print(points[channel].fadein);
    Serial.print(F(", fadeout="));
    Serial.print(points[channel].fadeout, HEX);
    Serial.print(F(", channel="));
    Serial.print(channel);
    Serial.print(F(", bR="));
    Serial.print(base_r, HEX);
    Serial.print(F(", bG="));
    Serial.print(base_g, HEX);
    Serial.print(F(", bB="));
    Serial.print(base_b, HEX);
    Serial.print(F(", bInt="));
    Serial.print(base_intensity);
    Serial.print(F(", FPS="));
    Serial.print(FPS);
    Serial.print(F(", NUM_LEDS="));
    Serial.print(NUM_LEDS);
    Serial.print(F(", procMsg="));
    Serial.print(processedMsgs);
    Serial.println();
  #endif

  processedMsgs++;

}

//-------------------------------------------- RENDER ---------------------------------------------//

void timer_handle_interrupts(__attribute__((unused)) int timer)  {
  if ((enableRender) && (processedMsgs > 0)) {
    update();
    #if defined(DEBUG)
      Serial.println(F("Render"));
    #endif
    FastLED.show();
    processedMsgs = 0;
  }
}

void initRender() {
  uint16_t trigger = min(max((uint16_t) ( ((F_CPU / 256) / FPS) - 1), 1), 65535);
  #if defined(DEBUG)
    uint16_t time = 1000 / FPS;
    Serial.print(F("F_CPU: "));
    Serial.print(F_CPU);
    Serial.print(F(", FPS: "));
    Serial.print(FPS);
    Serial.print(F(", render time (ms): "));
    Serial.print(time);
    Serial.print(F(", trigger: "));
    Serial.println(trigger);
  #endif
  timer_init_ISR(_TIMER1, TIMER_PRESCALER_1_256, trigger);
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
//  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN2), [](){}, RISING);
  #if defined(DEBUG)
    Serial.println(F("MIDI2Neopixel is ready!"));
  #endif
}

//--------------------------------------------- LOOP ----------------------------------------------//
void loop() {
  ////////////////////////////////////////// MIDI Read routine ////////////////////////////////////////
  midiEventPacket_t rx = MidiUSB.read();
  digitalWrite(LED_ACT_PIN, HIGH);
  switch (rx.header) {
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
  digitalWrite(LED_ACT_PIN, LOW);
}
//------------------------------------------ END OF LOOP -------------------------------------------//
