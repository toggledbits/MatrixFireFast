/**
 * MatrixFireFast - A fire simulation for NeoPixel (and other?) matrix
 * displays on Arduino (or ESP8266) using FastLED.
 * 
 * Author: Patrick Rigney (https://www.toggledbits.com/)
 * Copyright 2020 Patrick H. Rigney, All Rights Reserved.
 * Free to use for non-commercial works.
 * Please donate in support of my projects: https://www.toggledbits.com/donate
 * 
 * Hardware used: Arduino Mega 2560 + 44x11 NeoPixel matrix. See
 *                further comments below.
 *
 * The approach is straight-forward. A map is kept of "heat" locations that
 * overlays the display. Each refresh cycle, each pixel is "cooled" by one
 * step and moved up a row toward the top of the display. A random "burn"
 * is done in the bottom row of the display, with new heat values set within
 * a small range of all but the hottest values. Occasionally, a "flare" will
 * be set of in the bottom rows, which causes a large radiating heat spike.
 * This simulates the large licking flames observed in fire.
 *
 * I developed this using a C3 44x11 LED matrix panel. Currently, it eats a
 * good bit of RAM, so the larger-RAM 'duinos are required (this was written 
 * and tested on a Mega 2560). Other than RAM, it doesn't need much, so 
 * theoretically it should run well on a Zero, for example. Memory requirement 
 * as of this writing is about 180 bytes plus four bytes per pixel. It will
 * run on an Uno (that's what I started with) if you have a small display;
 * I've tested it up to 32x8 and it's a tight fit, but it works.
 * 
 * It also works very nicely on an ESP8266/NodeMCU. The fast processor allows
 * for really high refresh rates, and the larger RAM makes using a larger
 * matrix possible. The form factor is also more pleasant. This is what I 
 * recommend if you are going to do a semi-permanently-installed project.
 * 
 * I did the original development on an Arduino Uno with Adafruit_NeoMatrix.
 * The Uno's RAM topped out at about 11x12 on the display, and the refresh
 * rate wasn't high enough. This version uses FastLED to improve the refresh
 * rate. The Adafruit library was taking care of matrix construction issues
 * (i.e. how the sequential order of pixels maps to the cartesian system of
 * the display, tiling of display modules, etc.) but FastLED does not do this. 
 * So, the pos() function maps a (col,row) coordinate to an ordinal pixel in 
 * the LED string. The matrix display I used has its origin in the upper-left 
 * corner, and rows "zig-zag" (the first row is left-to-right, but the next 
 * right-to-left, then back to left-to-right, and so on). I've preemptively 
 * added a few macros to start supporting the other possible display 
 * constructions, but it's only scratching the surface so far. But FastLED 
 * gives a great refresh rate even for the large 484 pixel size of the 44x11 
 * display, and you may see in the YouTube video, the effect is pretty good. 
 * At least, I'm happy with it.
 * 
 * If you are powering the Arduino from USB and you run a large display at
 * high brightness, you will likely swamp the supply and crash the system due
 * to voltage drop. MAKE SURE YOU USE AN ADEQUATELY-SIZED POWER SUPPLY! I
 * recommend you run at low brightness first, and only increase brightness
 * after you've connected your larger power supply. Remember to tie the
 * grounds of your power supply output, display, and the Arduino together or 
 * you're going to have a bad time. If you're using a 3.3V system, you may 
 * need to use a level shifter/isolator.
 * 
 * The maximum refresh rate is a function of the processor speed and the 
 * number of pixels in the display. Because serial communication is used to
 * update the matrix, you can't refresh any faster than it takes to send the
 * entire frame (plus overhead). A WS2812 (NeoPixel) takes about 30us per LED,
 * so a 100 pixel string takes 3000us or 3ms to transmit, and interrupts are
 * disabled during that time, so beware! Your serial port data and other
 * operations can be affected by this.
 *
 * There are several knobs you can turn for configuration. See below.
 */

#include <FastLED.h>

#define VERSION 20156

#define DISPLAY_TEST  /* define to show test patterns at startup */

/* MATRIX CONFIGURATION */
#define MAT_W 44    /* Size (columns) of entire matrix */
#define MAT_H 11    /* and rows */
#ifdef ESP8266
#define MAT_PIN 0   /* Data for matrix on D3 on ESP8266 */
#else
#define MAT_PIN 6   /* Data for matrix on pin 6 for Arduino/other */
#endif
#define MAT_TOP     /* define if matrix 0,0 is in top row of display; undef if bottom */
#define MAT_LEFT    /* define if matrix 0,0 is on left edge of display; undef if right */
#define MAT_ZIGZAG  /* define if matrix rows zig-zag ---> <--- ---> <---; undef if scanning ---> ---> ---> */

#define BRIGHT 32   /* brightness; min 0 - 255 max -- high brightness requires a hefty power supply! Start low! */
#define FPS 15      /* Refresh rate */

/* DEBUG CONFIGURATION */
#undef  SERIAL      /* Serial slows things down; don't leave it on. */

/* Display size; can be smaller than matrix size, and if so, you can move the origin.
 * This allows you to have a small fire display on a large matrix sharing the display
 * with other stuff. */
const uint8_t rows = MAT_H;
const uint8_t cols = MAT_W;
const uint8_t xorg = 0;
const uint8_t yorg = 0;

/* Flare constants */
const uint8_t flarerows = 2;    /* number of rows (from bottom) allowed to flare */
const uint8_t maxflare = 8;     /* max number of simultaneous flares */
const uint8_t flarechance = 50; /* chance (%) of a new flare (if there's room) */
const uint8_t flaredecay = 14;  /* decay rate of flare radiation; 14 is good */

/* This is the map of colors from coolest (black) to hottest. Want blue flames? Go for it! */
const uint32_t colors[] = {
  0x000000,
  0x100000,
  0x300000,
  0x600000,
  0x800000,
  0xA00000,
  0xC02000,
  0xC04000,
  0xC06000,
  0xC08000,
  0x807080
};
const uint8_t NCOLORS = (sizeof(colors)/sizeof(colors[0]));

uint8_t pix[rows][cols];
CRGB matrix[MAT_H * MAT_W];

uint8_t nflare = 0;
uint16_t flare[maxflare];

/** pos - convert col/row to pixel position index. This takes into account
 *  the serpentine display, and mirroring the display so that 0,0 is the
 *  bottom left corner and (MAT_W-1,MAT_H-1) is upper right. You may need
 *  to jockey this around if your display is different.
 */
#ifndef MAT_LEFT
#define __MAT_RIGHT
#endif
#ifndef MAT_TOP
#define __MAT_BOTTOM
#endif
int pos( uint8_t col, uint8_t row ) {
    col += xorg;
    row += yorg;
#if defined(MAT_LEFT) && defined(MAT_ZIGZAG)
  if ( ( row & 1 ) == 1 ) {
    /* Reverse serpentine row */
    col = MAT_W - col - 1;
  }
#elif defined(__MAT_RIGHT) && defined(MAT_ZIGZAG)
  if ( ( row & 1 ) == 0 ) {
    col = MAT_W - col - 1;
  }
#elif defined(__MAT_RIGHT)
  col = MAT_W - col - 1;
#endif
#if defined(MAT_TOP)
  // mirror vertical 
  row = MAT_H - row - 1;
#endif
  return (int) col + (int) row * (int) MAT_W;
}

uint32_t isqrt(uint32_t n) {
  if ( n < 2 ) return n;
  uint32_t smallCandidate = isqrt(n >> 2) << 1;
  uint32_t largeCandidate = smallCandidate + 1;
  return (largeCandidate*largeCandidate > n) ? smallCandidate : largeCandidate;
}

// Set pixels to intensity around flare
void glow( int x, int y, int z ) {
  int b = z * 10 / flaredecay + 1;
  for ( int i=(y-b); i<(y+b); ++i ) {
    for ( int j=(x-b); j<(x+b); ++j ) {
      if ( i >=0 && j >= 0 && i < rows && j < cols ) {
        int d = ( flaredecay * isqrt((x-j)*(x-j) + (y-i)*(y-i)) + 5 ) / 10;
        uint8_t n = 0;
        if ( z > d ) n = z - d;
        if ( n > pix[i][j] ) { // can only get brighter
          pix[i][j] = n;
        }
      }
    }
  }
}

void newflare() {
  if ( nflare < maxflare && random(1,101) <= flarechance ) {
    uint8_t x = random(0, cols);
    uint16_t y = random(0, flarerows);
    uint16_t z = NCOLORS - 1;
    flare[nflare++] = (z<<12) | (y<<6) | (x&0x3f);
    glow( x, y, z );
  }
}

/** make_fire() animates the fire display. It should be called from the
 *  loop periodically (at least as often as is required to maintain the
 *  configured refresh rate). Better to call it too often than not enough.
 *  It will not refresh faster than the configured rate. But if you don't
 *  call it frequently enough, the refresh rate may be lower than
 *  configured.
 */
unsigned long t = 0; /* keep time */
void make_fire() {
  uint8_t i, j;
  if ( t > millis() ) return;
  t = millis() + (1000 / FPS);
  
  // First, move all existing heat points up the display and fade
  for ( i=rows-1; i>0; --i ) {
    for ( j=0; j<cols; ++j ) {
      uint8_t n = 0;
      if ( pix[i-1][j] > 0 ) 
        n = pix[i-1][j] - 1;
      pix[i][j] = n;
    }
  }

  // Heat the bottom row
  for ( j=0; j<cols; ++j ) {
    i = pix[0][j];
    if ( i > 0 ) {
      pix[0][j] = random(NCOLORS-6, NCOLORS-2);
    }
  }

  // flare
  for ( i=0; i<nflare; ++i ) {
    uint16_t x = flare[i] & 0x3f;
    uint16_t y = (flare[i] >> 6) & 0x3f;
    uint16_t z = (flare[i] >> 12) & 0x0f;
    glow( x, y, z );
    if ( z > 1 ) {
      flare[i] = (flare[i] & 0x0fff) | ((z-1)<<12);
    } else {
      // This flare is out
      for ( int j=i+1; j<nflare; ++j ) {
        flare[j-1] = flare[j];
      }
      --nflare;
    }
  }
  newflare();
  
  // Set and draw
  for ( i=0; i<rows; ++i ) {
    for ( j=0; j<cols; ++j ) {
      matrix[pos(j,i)] = colors[pix[i][j]];
    }
  }
  FastLED.show();
}

void setup() {
  FastLED.addLeds<NEOPIXEL, MAT_PIN>(matrix, MAT_W * MAT_H);
  FastLED.setBrightness(BRIGHT);
  FastLED.clear();
  FastLED.show();
  
  for ( uint8_t i=0; i<rows; ++i ) {
    for ( uint8_t j=0; j<cols; ++j ) {
      if ( i == 0 ) pix[i][j] = NCOLORS - 1;
      else pix[i][j] = 0;
    }
  }

#ifdef SERIAL
    Serial.begin(115200); while (!Serial) { ; }
    Serial.print("MatrixFireFast v"); Serial.println(VERSION);
    Serial.print("Brightness "); Serial.print(BRIGHT);
    Serial.print(", FPS "); Serial.println(FPS);
#endif

#ifdef DISPLAY_TEST
  for ( uint8_t i=0; i<cols; ++i ) {
    FastLED.clear();
    for ( uint8_t j=0; j<rows; ++j ) {
      matrix[pos(i,j)] = colors[NCOLORS-1];
    }
    FastLED.show();
    delay(1000/FPS);
  }
  for ( uint8_t i=0; i<rows; ++i ) {
    FastLED.clear();
    for ( uint8_t j=0; j<cols; ++j ) {
      matrix[pos(j,i)] = colors[NCOLORS-1];
    }
    FastLED.show();
    delay(1000/FPS);
  }
  /** Show the color map briefly at the extents of the display. This "demo"
   *  is meant to help establish correct origin, extents, colors, and
   *  brightness. You can cut or comment this out if you don't need it;
   *  it's not important to functionality otherwise.
   */
  uint8_t y = 0;
  FastLED.clear();
  for ( uint8_t i=NCOLORS-1; i>=0; --i ) {
    if ( y < rows ) {
      matrix[pos(0,y)] = colors[i];
      matrix[pos(cols-1,y++)] = colors[i];
    }
    else break;
  }
  FastLED.show();
  delay(2000);
#endif
}

void loop() {
  make_fire();
  delay(10);
}
