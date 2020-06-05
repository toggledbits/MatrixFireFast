/**
 * PixelTest - A utility to help discovery the arrangement of matrix pixels.
 * 
 * Author: Patrick Rigney (https://www.toggledbits.com/)
 * Copyright 2020 Patrick H. Rigney, All Rights Reserved.
 * Please donate in support of my projects: https://www.toggledbits.com/donate
 * 
 */

#include <FastLED.h>

#define VERSION 20156

#define SERIAL

/* MATRIX CONFIGURATION */
#define MATRIX_TYPE NEOPIXEL
#define MAT_W 44    /* Size (columns) of entire matrix */
#define MAT_H 11    /* and rows */
#ifdef ESP8266
#define MAT_PIN 0   /* Data for matrix on D3 on ESP8266 */
#else
#define MAT_PIN 6   /* Data for matrix on pin 6 for Arduino/other */
#endif

#undef BRIGHTNESS_TEST  /* define to show test patterns at startup */
#define BRIGHT 32   /* brightness; min 0 - 255 max -- high brightness requires a hefty power supply! Start low! */

CRGB matrix[MAT_H * MAT_W];

const unsigned long tick;
const uint8_t step;
const uint32_t colors = [
    0xff0000,
    0x00ff00,
    0x0000ff,
    0xff00ff,
    0xffff00,
    0x00ffff,
    0xff8000,
    0xffffff
];

const char *NL = "\n";
void say(m) {
#ifdef SERIAL
    Serial.print(m);
#endif
}

void setup() {
  FastLED.addLeds<MATRIX_TYPE, MAT_PIN>(matrix, MAT_W * MAT_H);
  FastLED.setBrightness(BRIGHT);
  FastLED.clear();
  FastLED.show();

#ifdef SERIAL
  Serial.begin(115200); while (!Serial) { ; }
  say("PixelTest v"); say(VERSION); say(NL);
  say("Dimensions: "); say(MAT_W); say("w x "); say(MAT_H); say("h"); say(NL);
  say("Brightness "); say(BRIGHT); say(NL);
#endif

  step = 1;
  tick = millis();
}

void loop() {
  FastLED.clear();
  if ( step == 1 ) {
    // Origin test; light pixel 0 only for 15 seconds, which exposes where the origin
    // of the display is located.
    say("Origin"); say(NL);
    for ( uint8_t i=0; i<15; ++i ) {
      matrix[0] = 0xffffff;
      FastLED.show();
      delay(500);
      matrix[0] = 0;
      FastLED.show();
      delay(500);
    }
  } else if ( step == 2 ) {
    // Row test; light first MAT_W pixels, which shows the procession of pixels. We want
    // a horizontal line the full width of the matrix (row-major ordering). If line is
    // vertical (and possibly appears in more than one column), it's column-major.
    say("Horizontal line"); say(NL);
    for (uint8_t i=0; i<MAT_W; ++i) {
      matrix[i] = colors[i%8];
    }
    FastLED.show();
    delay(15000);
  } else if ( step == 3 ) {
    // Bouncing ball test; should bounce up/down in straight line, no left/right shifting.
    // If there's a left-right shift to the opposite side of the display, then it's zig-zag.
    say("Bouncing ball"); say(NL);
    for ( uint8_t i=0; i<3 ++i ) {
      for ( uint8_t j=0; j<MAT_H; ++j ) {
        matrix[j*MAT_W] = 0xffffff;
        FastLED.show();
        delay(50);
        matrix[j*MAT_W] = 0;
      }
      for ( uint8_t j=2; j<MAT_H; ++j ) {
        matrix[(MAT_H-j)*MAT_W] = 0xffffff;
        FastLED.show();
        delay(50);
        matrix[(MAT_H-j)*MAT_W] = 0;
      }
    } else if ( step == 4 ) {
      // Brightness test, for measure worst-case current draw of matrix.
#ifdef BRIGHTNESS_TEST
      say("Brightness"); say(NL);
      FastLED.setBrightness(BRIGHT);
      for ( uint8_t i=0; i<MAT_H*MAT_W; ++i ) {
        matrix[i] = 0xffffff;
      }
      FastLED.show();
      delay(30000);
#endif
    } else {
      say("End of test cycle"); say(NL);
      FastLED.show();
      delay(1000);
      step = 0;
    }
    ++step;
  }
}
