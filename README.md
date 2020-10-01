# MatrixFireFast

Just on a whim, I decided to make my own fire simulation using a 44x11 WS2812 matrix I had purchased on Amazon. There's plenty of code available to do this, but I just wanted to figure out for myself how to code a nice-looking animation. So I ignored all of those other implementations, and just set about failing repeatedly... until I didn't.

The result is MatrixFireFast. It works on any LED matrix supported by [FastLED](http://fastled.io/). You can see it [here on YouTube](https://youtu.be/KcE_DxXfV1g).

I started development with an Arduino Uno and the Adafruit NeoMatrix library, and that worked fine as long as I kept the display size down. The Uno's RAM topped out at about 11x12, meaning I was using less than 1/4th of the entire matrix width available. So when things started to look like they were going to work well, I dug out a Mega 2560, which had enough RAM to manage the entire 44x11 display. But it was still not refreshing quite fast enough for my taste, so I moved off the Adafruit libraries to FastLED. That worked famously, so after some tweaking and a couple of hours playing with it on both Arduino and a NodeMCU/ESP8266, I was satisfied (for the moment).

The simulation does what I think pretty much everyone does at the most basic level: establishes "heat" near the bottom of the display and percolates it up, reducing the heat as you go. To simulate the licking flames of fire, I added random "flares" in the fire that rise from the bottom of the display, and also radiate outward, which I think is really the thing that most improves the effect.

The implementation is straightforward. After getting the effect right, I went through and tried to optimize a bit, particularly for memory use, to try and squeeze in larger displays on the lesser Arduinos. 

> Note: When I say "large" in this context, I do not mean physical size; it always and only means the total number of pixels.

As it stands today, I have tested it on:

* an Arduino Mega 2560 using a matrix of 44x11;
* an Arduino Uno using a 32x8 matrix;
* a NodeMCU/ESP8266 using the 44x11 matrix.

If you get it running in a different configuration, please let me know! Also, see "Matrix Size and Processor Selection" below for more information.

> Tip: Don't have a matrix handy? I think the simulation also looks really good on a long one-pixel-wide strip. When configuring a linear strip, set the matrix _width_ to 1, and the height to the number of pixels in the string.

Also note that FastLED is a really fast library for sending pixels out to LED strings (kudos!), but it's not a matrix library. It views a matrix as it is *electrically* &mdash; a linear arrangement of LEDs. In order to map the rectangular physical arrangement of LEDs into their linear electrical implementation, the function `pos()` is used extensively. It maps a "canonical" column and row address to a linear pixel address. The included implementation of `pos()` uses several preprocessor macros in the attempt to support the most common arrangements. You will only need to customize `pos()` if the configuration switches provided don't adequately cover your matrix's pixel arrangement. See "Customizing the `pos()` Function" below for more details about this. 

## Matrix Size and Processor Selection

So far, the ESP8266 clearly wins for RAM and processor speed from among the devices I've tested with. High frame rates (too high to be realistic, I think) are easy, and I haven't yet probed the limits of display size. The form factor is also much better. If you were going to choose a processor for a semi-permanent installation, I would definitely go with the ESP8266. Just be aware that level shifting may be required, as the ESP is a 3.3V device, and the voltage of the matrix may not be compatible, and could in fact, damage the microprocessor if you hook it up incorrectly.

As of this writing, the RAM requirement is basically a baseline of about 180 bytes (without other libraries, including Serial), plus four bytes per pixel (i.e. at least `width * height * 4 + 180` bytes). Using the full size of my 484 pixel 44x11 matrix, the RAM requirement is just over 2K (hence the need for the Arduino Mega 2560). A 32x8 display squeezes into an Uno. Increasing or decreasing either the number of possible flares or the number of displayed colors slightly modifies the baseline 180-byte requirement but is not a significant contributor. The matrix is the greedy consumer. For fun, I've compiled ESP8266 versions with matrix sizes up to 128x96 and there's plenty of RAM, but at that size, other factors like communicating with the matrix really start to come into play (read on).

> Note: I've made attempts to reduce this requirement to three bytes per pixel (just what FastLED requires), but thus far my attempts have increased code complexity and reduced performance, and the exchange isn't worth it, IMO. RAM is cheap.

Multi-panel displays are possible, but memory requirements increase accordingly: if you make a display of 4 matrix panels, your memory requirement is four times that of a single panel. See *Multiple Panel Displays* below.

Another thing to be aware of is that the refresh rate will be limited by the size of the display and the type of LEDs. For NeoPixels, for example, there are strict timing requirements for data transmission and it takes about 30&micro;s per pixel. So 100 pixels takes the library 3ms to transmit. During this time, FastLED must disable interrupts, so anything else going on is going to be delayed accordingly. My 44x11 test matrix, with 484 pixels, takes 14.52ms to transmit to the display, and therefore the theoretical limit of the frame rate is around 68 frames per second. Fortunately, this is higher than what I feel looks good (for that matrix, something in the low 20s is most pleasing to my eye).

I mentioned above that I've compiled the sketch with matrix sizes of 128x96, or over 12,000 LEDs. In practice, this would take almost 370ms (over 1/3 of a second) for FastLED to transmit to the array (in NeoPixel), and therefore the refresh rate would be too slow to look right. In practical terms, the refresh rate, as limited by transmit bandwidth, limits matrix size to something around 2048 NeoPixels. Other types of LEDs may do better, or worse.

## Basic Configuration

The most basic thing you need to do is configure the size of the display you are using, and what data pin is used to transmit the pixel stream to the display. If you do nothing else to the code when you first try it, you have to get these right. These are set near the stop of the code:

* `MAT_TYPE` - The type of LEDs in the matrix (default: `NEOPIXEL`; see the [FastLED docs](https://github.com/FastLED/FastLED/wiki/Overview) for other supported values);
* `MAT_W` - Width of the matrix in pixels;
* `MAT_H` - Height of the matrix in pixels;
* `MAT_PIN` - The pin to use for serial pixel stream.

Note: If you are using an ESP8266, you may need to use a level shifter. ESP8266 is a 3.3V device, and presenting voltages much higher may damage the microprocessor. The 3.3V data output signal may also be insufficient to be recognized as data by your LED matrix. As it happened, the 5V 44x11 matrix I used worked fine without a level shifter &mdash; it's data line is high impedence (presents no voltage, just accepts whatever the processor gives it), and senses edges fine at the 3.3V level.

You also need to know a little bit about your matrix. In a pixel matrix, although you are looking at a rectangular arrangement, electrically the pixels are linear. That is, if you have a 16x16 matrix, you have 256 pixels numbered from 0 to 255. Depending on the manufacturer, pixel 0 can be in any corner (and hopefully nowhere else but one of the four corners, because that would just be weird). Pixel 1 then, depending on where pixel 0 is, could be to the left or right, above or below. You need to work out the order of the pixels in your display. If you don't know, and the documentation for the matrix doesn't tell you, or your dog ate it, or whatever, you can run the PixelTest program included in the distribution to determine it. See "Using PixelTest" below.

If your display has pixel 0 in the top row, either at the left or right, make sure `MAT_TOP` is *define*d. If pixel 0 is in the bottom row, then `MAT_TOP` must be *undef*ined.

If pixel 0 is on the left edge of your display, then `MAT_LEFT` must be *define*d; otherwise, it should be *undef*ined.

If your panel is column-major, that is, if pixel 1 is above or below pixel 0 rather than being to its left or right, define `MAT_COL_MAJOR`; otherwise, it must be *undef*ined.

If your pixels zig-zag, that is, if each row goes the opposite direction of the previous row, then `MAT_ZIGZAG` should be *define*d; otherwise, it must be *undef*ined.

### Your First Run

When you start up the code, if you've left the `DISPLAY_TEST` macro defined as it is by default, it will run a display test by first displaying white vertical bars sweeping from left to right, following by horizontal bars sweeping bottom to top. If these bars are broken or look odd, your display configuration is different from what you have set in the foregoing instructions. Recheck `MAT_TOP`, `MAT_LEFT`, and `MAT_ZIGZAG`. If you are unsure, read "Using PixelTest" below.

If the display test displayed properly, you should now see the fire simulation. The default settings for fire "behavior" are set up for wider displays (32 pixel width or more). If your display is smaller, please read on, as you will likely need to tone down some of the flare settings so the display is less "busy" in the smaller matrix. Tuning is part of the fun.

## Multiple Panel Displays

Turn on multi-panel support by defining `MULTIPANEL`. Set the `PANELS_W` constant to the number of matrix panels in width, and `PANELS_H` to the number of matrix panels in height. All panels in the display must be identical in configuration/geometry.

**CAUTION!** Multi-panel displays require *a lot* of memory. Make sure you choose a processor configuration that has adequate memory to the task. The total number of panels is limited by available memory and the matrix element size. Also note that the serial link bandwidth is limited, so as always, the more total pixels, the lower the maximum refresh rate.

The following are the possible panel connection order (as an example) a 4x2 arrangement (so `PANELS_W` is 4, and `PANELS_H` is 2):

```
    +---+---+---+---+        +---+---+---+---+        +---+---+---+---+        +---+---+---+---+
    | 4 | 5 | 6 | 7 |        | 7 | 6 | 5 | 4 |        | 0 | 1 | 2 | 3 |        | 0 | 1 | 2 | 3 |
    +---+---+---+---+        +---+---+---+---+        +---+---+---+---+        +---+---+---+---+
    | 0 | 1 | 2 | 3 |        | 0 | 1 | 2 | 3 |        | 4 | 5 | 6 | 7 |        | 7 | 6 | 5 | 4 |
    +---+---+---+---+        +---+---+---+---+        +---+---+---+---+        +---+---+---+---+
    #undef PANEL_TOP         #undef PANEL_TOP         #define PANEL_TOP        #define PANEL_TOP
    #undef PANEL_ZIGZAG      #define PANEL_ZIGZAG     #undef PANEL_ZIGZAG      #define PANEL_ZIGZAG
```

Note that panel 0, the one connected directly to the processor, must be on the left edge in all cases.

## Advanced Configuration

The most common thing you'll probably want to tweak is the apparent "intensity" of the inferno. The licking of flames is simulated by randomly injecting "flares" into the fire near the bottom. The following constants effect where and when these flares occur, and how big they can be:

* `flarerows` - How many of the bottom rows are allowed to flare; flares occur in a randomly selected row up to `flamerows` from the bottom; if 0, only the bottom row can flare.
* `maxflare` - The maximum number of simultaneous flares. A flare lives until it cools out and dies, so it lives for several frames. Multiple flares can exist at various stages of cooling, but no more than `maxflare`. New flares are not created unless there is room. The default of 8 is pleasant for a 44-pixel wide matrix, but will likely look too busy and intense if you are using a narrower matrix, like a 16x16. As a rule of thumb, start with a value equal to about 1/4 of the display width, and then tweak it up or down for the look you want.
* `flarechance` - Even if there's room for a flare doesn't mean there will be one. This number sets the probability of a flare (0-100%). Lower numbers make a more tame-looking fire (I think it also looks more fakey).
* `flaredecay` - This sets the rate at which the flare's radiation decays. Smaller values yield more radiation; values under 10 will likely produce too much radiation and overwhelm the display (the smaller the display, the worse that will be); values over 20 will likely produce too little radiation to be interesting. The default is 14.

Other knobs you can turn:

* `BRIGHT` - Brightness of the display, from 0-255. This value is passed directly to `FastLED.setBrightness()` at initialization.
* `FPS` - (frames per second) the frequency of display updates. Lower values is a slower flame effect, and higher values make a roaring fire. The upper limit of this is a function of your processor speed and anything else you may have the processor doing.

If you're *really* into customization, you can change the color of the flames. This is done by replacing the values in the `colors` array. Just make sure element 0 is black. You can have as few or as many colors as you wish. Heat "decays" from the highest-numbered color to 0. As a rule of thumb, though, the number of colors should be +/-2 from the height of the display. If you have too few colors, the flames will die out closer to the bottom of the display. If you have too many, the flames will die out somewhere above the display, and the display will just look like a solid sheet of color.

## Watch Your Power!

A large LED matrix can draw substantial power. If you are powering your matrix from your microprocessor, and you've powered *it* from USB, then it's very easy for a large, bright display to swamp that power supply. This will cause a drop in voltage, and the microprocessor will crash/reset. To avoid this being a problem right at the start, the default brightness given to FastLED is pretty low. If you have a good power supply or if the display is small and its power budget fits your supply, you can increase this brightness (via the `BRIGHT` constant).

A better approach is to either power your matrix separately from a larger supply, or if possible (the micro and matrix have compatible operating voltages), power both your micro and matrix from the same larger supply. An "adequate" supply is one that will hold up the voltage within 5% of spec under full load (I'll go into that below). Regardless of your arrangement, just remember that **you must always connect the ground of the matrix and its power supply together with the ground of your microprocessor**, or you may have unpredictable behavior or maybe worse, a more realistic fire effect than this code provides on its own.

What is full load? You could take the approach of measuring the current draw of the matrix with all LEDs on white at full brightness. This would not be wrong &mdash; it is the worst-case scenario. But it's also not the way this effect operates. The display is never all white. If you use the default color map, which is predominantly red hues, the actual full power consumption of the matrix will likely be closer to 1/3 of the worst-case current. When running, the display is mostly red hues at the bottom, with a lot of black (off) pixels near the top, and maybe a little green or blue mixed in on a handful of the pixels as part of the effect. So really, you're mostly driving the red LEDs in the pixels, and the small mix-in of green and blue is trivial and offset by the black pixels that draw no power at all. So if you measure your display at 15A with full brightness white, you will likely get away with 5A for displaying the effect with the default color map.

Whatever number you come up with, it's always a good idea to derate slightly. If you figure out you need 4A to drive your matrix at 5V, choosing a 20W power supply isn't recommended. Derating 80% (common), a better choice would be at least a 25W power supply (20 / 0.80 = 25).

> Just to repeat, the hands-down best way to judge the size of power supply you need is to run the actual effect on the intended matrix while powered from a bench power supply with current limiting and power monitoring. Observe the peak current (the current varies as the effect runs; some bench supplies have a min/max feature that will track this value for you). Then derate accordingly.

## Customizing the `pos()` Function

If the given switches for display configuration don't cover your matrix product, you can code your own replacement for `pos()`. It needs to convert a canonical (col,row) to a pixel address. If the pixel at that address is illuminated, it should be "row" rows up from the bottom of the display and "col" columns from the left of the display. That is, the canonical form for MatrixFireFast is (0,0) (origin) at the bottom of the display, row major, with columns progressing from left to right, and rows from bottom to top as displayed. 

The 44x11 matrix I used for development, for example, has its original at upper-left with rows advancing downward (so opposite the desired canonical addressing), and just for added fun, columns in the "top" row progress from left to right, but the next row is *right-to-left*, and the row following back to left-to-right, and so on (a "zig-zag" arrangement of LEDs). All of this is handled by setting preprocessor macros at the top of the code, which are used in `pos()` to compute the correct pixel address. 

But I have not provided preprocessor values for every possible arrangement of LED matrix. For example, if the pixel at address 0 is the top left pixel, but pixel 1 is the first pixel in the next *row* rather than the next *column*, I have not provided a flag for that. You'll have to work it out (e.g. if it's a square matrix, you could/should just rotate the display...). 

If you need to do surgery on `pos()` to make it work with the matrix you are using and the way it is going to be installed/displayed, just make sure that it returns the index of the pixel in the bottom left corner pixel of the entire display when its input coordinate is (0,0), and the index of the next pixel to the _right_ for (1,0), and for (0,1) it must return the index of the pixel that is _one row up_ from (0,0) (in the same column).

Your `pos()` implementation should also take into account that the displayed size of the animation may be portion of the matrix, and that the position of this "subwindow" in the display has its own origin. See "Sharing the Display" below for how that works. It's only necessary to do this if you are sharing the display.

Here are the global constants you may need to perform the necessary calculation:

* `MAT_W` and `MAT_H` - the configured pixel width and height, respectively, of the entire matrix;
* `cols` and `rows` - the pixel width and height, respectively, of the subwindow in which the animation is displayed (defaults to `MAT_W` and `MAT_H` unless you change them);
* `xorg` and `yorg` - the offset column and row, respectively, of the origin of the subwindow in which the animation is displayed (default to 0,0 unless change them).

## Sharing the Display

It is possible for the fire simulation to share a large matrix with other displayed data. In order to do this, you need to set the following constants in the code:

* `cols` - Normally this is set to `MAT_W`, the full width of the matrix. If you are using the fire as a sub-display, set this constant to the desired width (must be \<= `MAT_W`).
* `rows` - Normally this is set to `MAT_H`, the full height of the matrix. Set this to the number of rows for your sub-display (must be \<= `MAT_H`).
* `xorg` and `yorg` - Default 0, 0 respectively; the origin of the sub-display, offset from canonical (0,0) (bottom left corner).

Anything else you need to display with the fire simulation you can now set yourself directly into the `matrix` array. This array is `MAT_W * MAT_H` pixels long &mdash; the entire matrix. Whatever you do to the array, calling `FastLED.show()` will display it. In your program's `loop()` function, just call `make_fire()` either before or after all your other work to set your pixels. Make sure you call `make_fire()` often enough to uphold the configured refresh rate in the `FPS` constant.

## Using PixelTest

PixelTest is a simple sketch that displays a series of patterns meant to expose the arrangement of pixels in the matrix. By watching how the pixels display, you can determine the arrangement of the matrix and get MatrixFireFast configured correctly.

Before you run PixelTest, you will need to set (in the PixelTest sketch) the matrix data pin and its width and height in the same manner as described for MatrixFireFast in "Basic Configuration" above. Also confirm that the matrix LED type (`MAT_TYPE` is correct). The LED types supported by FastLED can be found in their [documentation](https://github.com/FastLED/FastLED/wiki/Overview).

If you are using a separate (non-USB) power supply, you may also want to turn on `BRIGHTNESS_TEST` and set `BRIGHT` (0-255) as well. If enabled, the brightness test will allow you to measure the worst-case current draw of your matrix at full brightness with all pixels on (and white). Since this test can crash a USB-powered matrix configuration, it is normally disabled in the distribution. A good way to do this is to simply use a good-quality bench power supply, as these usually display the (ampere) measurement of the load. This test also allows you to try out various values for `BRIGHT` to see what you might want to use in MatrixFireFast. **CAUTION: SOME MATRIX PANELS, PARTICULARLY THE ONES WITH FLEXIBLE PCBS, CANNOT ACTUALLY CARRY THE FULL CURRENT OF ALL LEDS ON AT FULL BRIGHTNESS AND MAY BE DAMAGED.** Please refer to the manufacturer's instructions and specs for details. Use the lowest possible brightness that gives you the desired results.

> NOTE: Make sure the matrix is in the orientation in which you want to install it before starting the test. The advice in this section assumes you are looking at the matrix in the same orientation as that in which it will be installed.

Download the configured PixelTest sketch to the micro. PixelTest will begin by blinking the first pixel, the "origin" pixel 0, for 15 seconds. If pixel 0 is anywhere in the *top* row of the matrix, you need to *define* `MAT_TOP` in MatrixFireFast (e.g. `#define MAT_TOP`). Otherwise, undefine it (e.g. `#undef MAT_TOP`). If pixel 0 is on the *left* edge of the matrix, you should *define* `MAT_LEFT`; otherwise, undefine it.

PixelTest will next turn on (in various colors) the remaining pixels in the row. If the matrix displays a _horizontal line of pixels at the top or bottom edge_, proceed to the next step. Otherwise, you are most likely to see one or more vertical columns of pixels lit (and possibly a partial column). In this case, the display is arranged in column-major order (you need to define `MAT_COL_MAJOR` in MatrixFireFast).

PixelTest will then try to bounce a single pixel up and down in a single column. The "ball" should move straight up and down in the same column, without changing columns as it ascends and descends. If the pixel jumps from left to right while moving up and down, you should define `MAT_ZIGZAG` in MatrixFireFast; otherwise (it keeps a single straight line), undefine it.

If you enabled the brightness test, PixelTest will now turn on all LEDs at your configured brightness (`BRIGHT`) and hold for 30 seconds. This will allow you to measure the maximum draw on your power supply with that brightness. If you have USB-powered your matrix through the microprocessor, and you have a large matrix, there's a good chance it will crash at this point. Don't use USB as a power supply; you need something more.

PixelTest will then do a few seconds of "random sparkles" and start another test cycle.

> NOTE: If you get no display at all, check all of the following: you are wired to the correct data pin; your matrix is powered correctly; check your configuration.

I posted an example run of [what PixelTest looks like on my 44x11 matrix up on YouTube](https://youtu.be/py8j66cxQGI). The run shows that my display's origin pixel (blinking in the first test) is in the top row, so `MAT_TOP` needs to be define. It's also on the left edge, so `MAT_LEFT` needs to be defined. In the "bouncing ball" test, you can see the "ball" moving from one edge of the display to the other, rather than just up and down in a straight line, and this indicates that `MAT_ZIGZAG` needs to be set.

## Donations

Donations in support of my projects are always greatly appreciated, regardless of size. You can [make a one-time donation here](https://www.toggledbits.com/donate).

## License

MatrixFastFire is licensed under the Gnu Public License version 3. Please see the LICENSE file.

Copyright 2020 Patrick H. Rigney, All Rights Reserved.
