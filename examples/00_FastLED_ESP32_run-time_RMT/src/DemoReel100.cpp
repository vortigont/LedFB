/*
This example is based on FastLED's DemoReel100 - https://github.com/FastLED/FastLED/tree/master/examples/DemoReel100

  It will demonstrate how to create and run FastLED's clockless leds (ws2812) engine on ESP32-RMT
  with run-time defined configuration for gpio and color order


  Story: Due to historical reasons FastLED has templated classes structure for instantiating LED control engines
  due to optimization for old era 8 bit MCUs.

  Modern 32 bit MCUs usually do not require that compile-time optimization, but FastLED does not have plans to change the API to overcome this,
  (at least for time of writing)
  Ref issues: FastLED/FastLED#282, FastLED/FastLED#826, FastLED/FastLED#1594

  This lib provides simple wrapper classes derived from FastLED that allows to work-around this limitation
  and use run-time definable gpio and color order selection for ws2812 strips on ESP32 family chips.

  Other types of MCUs/stripes could also (possibly) be implemented in the same manner, I just had no need for this yet.

  You can build this example with PlatformIO's `pio run` command

*/


/*
  To use this you do not even need to install full LedFB lib, just pick only one header file from there 'w2812-rmt.hpp' (https://github.com/vortigont/LedFB/blob/main/ledfb/w2812-rmt.hpp)
  and drop it into your project's dir
  Then include it instead of "FastLED.h" (or after "FastLED.h")
*/ 
#include "w2812-rmt.hpp"


FASTLED_USING_NAMESPACE
#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120


/*

  Here what was defined in the original sketch of demoReel,
  those defines will hardcode strip's data pin, color order, number of leds and static buffer for the strip.
  We will skip it for now

#define DATA_PIN    3
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    64
CRGB leds[NUM_LEDS];
*/


// Instead we will define run-time variables where we could load our desired configuration

// this will hold our used gpio
int gpio_num;
// this will hold our used strip color order
EOrder color_order;
// this will hold the number of leds in our strip
int num_of_leds;
// this is the pointer to our to be created LED buffer, currently pointing to nowhere
CRGB* CRGB_buffer = nullptr;
// and the most important part - a pointer to a to be created derived class for ESP32RMT engine
ESP32RMT_WS2812B *wsstrip  = nullptr;

// Arduino's setup
void setup() {

  // here we will pretend that we loaded our configuration from EEPROM/NVS or json config file
  // but just for simplicity in this example we will use some static values

  // let's load gpio number for attached strip as value of 0
  gpio_num = 0;

  // color order we will use - GRB (most common one)
  color_order = GRB;

  // and num of leds will be 128
  num_of_leds = 128;

  // let's allocate dynamic mem for buffer first
  CRGB_buffer = new CRGB[num_of_leds];

  // create stripe driver object instance for WS2812B using our run-time variables
  wsstrip = new ESP32RMT_WS2812B(gpio_num, color_order);

  // attach created driver and led buffer to FastLED engine
  FastLED.addLeds(wsstrip, CRGB_buffer, num_of_leds);

  // now we've completed run-time configuration for ESP32 RMT,
  // the rest of code is not changed and uses original demoReel example,
  // except I've replaced all occurences of 'NUM_LEDS' to our 'num_of_leds'
  // and 'leds' to 'CRGB_buffer'


  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
}


// forward declarations
void rainbow();
void rainbowWithGlitter();
void confetti();
void sinelon();
void juggle();
void bpm();
void nextPattern();
void addGlitter( fract8 chanceOfGlitter);

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
  
void loop()
{
  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( CRGB_buffer, num_of_leds, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    CRGB_buffer[ random16(num_of_leds) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( CRGB_buffer, num_of_leds, 10);
  int pos = random16(num_of_leds);
  CRGB_buffer[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( CRGB_buffer, num_of_leds, 20);
  int pos = beatsin16( 13, 0, num_of_leds-1 );
  CRGB_buffer[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < num_of_leds; i++) { //9948
    CRGB_buffer[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( CRGB_buffer, num_of_leds, 20);
  uint8_t dothue = 0;
  for( int i = 0; i < 8; i++) {
    CRGB_buffer[beatsin16( i+7, 0, num_of_leds-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

