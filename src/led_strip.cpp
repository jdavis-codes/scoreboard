#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <gama.h>
#include "score_event.h"

// Define segment geometry BEFORE the header so it uses these values
#define SEGMENT_PIXELS 6
#define SEGMENTS_PER_DIGIT 7
#define PIXELS_PER_DIGIT (SEGMENTS_PER_DIGIT * SEGMENT_PIXELS)

// 4x 7-segment sections for home and away, plus 2 extra segments for the minus signs
constexpr size_t TOTAL_SEGMENTS = 4 * 7 + 2;
constexpr size_t REQUIRED_LEDS = SEGMENT_PIXELS * TOTAL_SEGMENTS; // Total number of LEDs needed for the display

#include <seven_segment_strip.h>

#define LED_PIN 14
#define LED_COUNT REQUIRED_LEDS
#define BRIGHTNESS 240 // Set BRIGHTNESS to about 1/5 (max = 255)

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

void colorWipe(uint32_t color, int wait);
void whiteOverRainbow(int whiteSpeed, int whiteLength);
void pulseWhite(uint8_t wait);
void rainbowFade2White(int wait, int rainbowLoops, int whiteLoops);

struct Score;
void displayScores(Score &home, Score &away);

// LED order: home ones, home tens, home minus, away ones, away tens, away minus
constexpr int HOME_ONES = 0;
constexpr int HOME_TENS = HOME_ONES + PIXELS_PER_DIGIT;
constexpr int HOME_MINUS = HOME_TENS + PIXELS_PER_DIGIT; // segment 14
constexpr int AWAY_ONES = HOME_MINUS + SEGMENT_PIXELS;   // segment 15
constexpr int AWAY_TENS = AWAY_ONES + PIXELS_PER_DIGIT;
constexpr int AWAY_MINUS = AWAY_TENS + PIXELS_PER_DIGIT; // segment 29
constexpr bool USE_RAINBOW_CYLON = true;

// Perimeter of a "0": segments A,B,C,D,E,F clockwise, middle segment G excluded
constexpr int RING_LEN = 6 * SEGMENT_PIXELS;

// create an array of RGBW values for the entire strip based on the segment pixel data
struct pixel
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
  bool on = false;
};

pixel homeColor = {180, 100, 0, 20}; // Initialize to Orange
pixel awayColor = {0, 0, 255, 0};    // Initialize to Blue
pixel offColor = {0, 0, 0, 0};       // Initialize to Off

struct Score
{
  Segment digit0;
  Segment digit1;
  char text[3] = "00";     // String representation of the score
  int value = 0;           // Integer representation of the score
  bool isNegative = false; // Flag to indicate if the score is negative
};

Score homeScore;
Score awayScore;

// forward declarations
void printScore(Score &home, Score &away);
void displayScores(Score &home, Score &away);
// side: -1 = both, 0 = home only, 1 = away only
void bootCylon(uint16_t cycles = 3, uint8_t frameDelay = 18, int tail = 5, bool rainbow = false, int8_t side = -1);

void setupStrip()
{
  strip.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();  // Turn OFF all pixels ASAP
  strip.setBrightness(BRIGHTNESS);
}

void stripTask(void *pvParameter)
{
  initScoreQueue();

  while (1)
  {
    ScoreMessage msg;

    if (xQueueReceive(scoreQueue, &msg, portMAX_DELAY) == pdTRUE)
    {
      if (msg.target == SCORE_TARGET_HOME || msg.target == SCORE_TARGET_BOTH)
      {
        switch (msg.action)
        {
        case SCORE_ACTION_INCREMENT:
          homeScore.value += (msg.value != 0 ? msg.value : 1);
          break;
        case SCORE_ACTION_DECREMENT:
          homeScore.value -= (msg.value != 0 ? msg.value : 1);
          break;
        case SCORE_ACTION_SET:
          homeScore.value = msg.value;
          break;
        case SCORE_ACTION_RESET:
          homeScore.value = 0;
          break;
        }
      }

      if (msg.target == SCORE_TARGET_AWAY || msg.target == SCORE_TARGET_BOTH)
      {
        switch (msg.action)
        {
        case SCORE_ACTION_INCREMENT:
          awayScore.value += (msg.value != 0 ? msg.value : 1);
          break;
        case SCORE_ACTION_DECREMENT:
          awayScore.value -= (msg.value != 0 ? msg.value : 1);
          break;
        case SCORE_ACTION_SET:
          awayScore.value = msg.value;
          break;
        case SCORE_ACTION_RESET:
          awayScore.value = 0;
          break;
        }
      }

      bool homeChanged = (msg.target == SCORE_TARGET_HOME || msg.target == SCORE_TARGET_BOTH);
      bool awayChanged = (msg.target == SCORE_TARGET_AWAY || msg.target == SCORE_TARGET_BOTH);

      homeScore.value %= 100;
      awayScore.value %= 100;

      // Send local changes to queue for database transmission
      if (!msg.isWeb && supabaseQueue != NULL) {
          ScoreUpdate update = { static_cast<int16_t>(homeScore.value), static_cast<int16_t>(awayScore.value) };
          xQueueSend(supabaseQueue, &update, 0);
      }

      if (homeChanged && homeScore.value > 0 && homeScore.value % 99 == 0)
      {
        bootCylon(10, 26, 20, USE_RAINBOW_CYLON, 0);
      }
      if (awayChanged && awayScore.value > 0 && awayScore.value % 99 == 0)
      {
        bootCylon(10, 26, 20, USE_RAINBOW_CYLON, 1);
      }

      if (homeChanged && homeScore.value > 0 && homeScore.value % 10 == 0)
      {
        bootCylon(5, 8, 20, false, 0);
      }
      if (awayChanged && awayScore.value > 0 && awayScore.value % 10 == 0)
      {
        bootCylon(5, 8, 20, false, 1);
      }

      printScore(homeScore, awayScore);

      displayScores(homeScore, awayScore);
    }
  }
}

void printScore(Score &home, Score &away)
{
  if (!Serial)
    return; // skip when no USB host is reading (CDC write would block)

  const int rasterRows = (SEGMENT_PIXELS * 2) + 3;

  const char homeSign = home.isNegative ? '-' : '+';
  const char awaySign = away.isNegative ? '-' : '+';

  Serial.printf("Home Score: %c %c%c\n", homeSign, home.text[0], home.text[1]);
  // print segment values for debugging
  printSegment(home.digit0);
  printSegment(home.digit1);

  Serial.printf("Away Score: %c %c%c\n", awaySign, away.text[0], away.text[1]);
  printSegment(away.digit0);
  printSegment(away.digit1);

  for (int row = 0; row < rasterRows; ++row)
  {
    printMinusRow(row, home.isNegative);
    Serial.print("  ");
    printRasterRowForSegment(home.digit0, row);
    Serial.print("  ");
    printRasterRowForSegment(home.digit1, row);
    Serial.print("  ");
    printMinusRow(row, away.isNegative);
    Serial.print("  ");
    printRasterRowForSegment(away.digit0, row);
    Serial.print("  ");
    printRasterRowForSegment(away.digit1, row);
    Serial.println();
  }
}

// Clockwise perimeter offsets (within one digit) of the "0" ring.
static void buildRing(int ring[RING_LEN])
{
  const int SP = SEGMENT_PIXELS;
  int n = 0;
  for (int i = 2 * SP - 1; i >= SP; --i)
    ring[n++] = i; // A: top, left->right
  for (int i = SP - 1; i >= 0; --i)
    ring[n++] = i; // B: top-right, top->bottom
  for (int i = 4 * SP; i < 5 * SP; ++i)
    ring[n++] = i; // C: bottom-right, top->bottom
  for (int i = 5 * SP; i < 6 * SP; ++i)
    ring[n++] = i; // D: bottom, right->left
  for (int i = 6 * SP; i < 7 * SP; ++i)
    ring[n++] = i; // E: bottom-left, bottom->top
  for (int i = 3 * SP - 1; i >= 2 * SP; --i)
    ring[n++] = i; // F: top-left, bottom->top
}

// Cylon comet sweeping around the "0" ring of every digit at boot (or one side, on score milestones).
void bootCylon(uint16_t cycles, uint8_t frameDelay, int tail, bool rainbow, int8_t side)
{
  int ring[RING_LEN];
  buildRing(ring);

  int digitBases[4];
  int numDigits = 0;
  if (side != 1)
  {
    digitBases[numDigits++] = HOME_ONES;
    digitBases[numDigits++] = HOME_TENS;
  }
  if (side != 0)
  {
    digitBases[numDigits++] = AWAY_ONES;
    digitBases[numDigits++] = AWAY_TENS;
  }

  for (uint16_t c = 0; c < cycles; ++c)
  {
    for (int head = 0; head < RING_LEN; ++head)
    {
      // only blank the animated ring(s), leave the other score's digits lit
      for (int d = 0; d < numDigits; ++d)
        for (int i = 0; i < RING_LEN; ++i)
          strip.setPixelColor(digitBases[d] + i, strip.Color(0, 0, 0, 0));

      for (int d = 0; d < numDigits; ++d)
      {
        for (int t = 0; t <= tail; ++t)
        {
          int idx = head - t;
          if (idx < 0)
            idx += RING_LEN;
          uint8_t level = 255 - (uint8_t)((t * 255) / (tail + 1)); // fading tail
          if (rainbow)
          {
            uint16_t hue = static_cast<uint16_t>(((idx * 65536UL) / RING_LEN) + (d * 4096UL));
            strip.setPixelColor(digitBases[d] + ring[idx], strip.gamma32(strip.ColorHSV(hue, 255, level)));
          }
          else
          {
            strip.setPixelColor(digitBases[d] + ring[idx], strip.Color(gamma8[level], 0, 0, 0));
          }
        }
      }
      strip.show();
      delay(frameDelay);
    }
  }

  for (int d = 0; d < numDigits; ++d)
    for (int i = 0; i < RING_LEN; ++i)
      strip.setPixelColor(digitBases[d] + i, strip.Color(0, 0, 0, 0));
  strip.show();
}

TaskHandle_t bootCylonTaskHandle = nullptr;
volatile bool bootCylonStopRequested = false;

// Repeats the boot Cylon animation until stopBootCylonTask() requests an orderly exit.
void bootCylonTask(void *pvParameter)
{
  while (!bootCylonStopRequested)
  {
    bootCylon(1, 26, 20, USE_RAINBOW_CYLON);
  }

  bootCylonTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

void startBootCylonTask()
{
  bootCylonStopRequested = false;
  xTaskCreate(&bootCylonTask, "bootCylon", 4096, NULL, 5, &bootCylonTaskHandle);
}

// Wait for the boot task to finish its current LED operation before clearing the strip.
void stopBootCylonTask()
{
  if (bootCylonTaskHandle)
  {
    bootCylonStopRequested = true;
    while (bootCylonTaskHandle)
    {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    strip.clear();
    strip.show();
  }
}

// Apply per-channel gamma correction from gama.h
uint32_t gammaColor(const pixel &c)
{
  return strip.Color(gamma8[c.r], gamma8[c.g], gamma8[c.b], gamma8[c.w]);
}

uint32_t homeColorP = gammaColor(homeColor);
uint32_t awayColorP = gammaColor(awayColor);
uint32_t offColorP = gammaColor(offColor);

static void renderDigit(const Segment &seg, int ledBase, uint32_t onColor)
{
  for (int p = 0; p < PIXELS_PER_DIGIT; ++p)
    strip.setPixelColor(ledBase + p, seg.pixels[p] ? onColor : offColorP);
}

static void renderMinus(int ledBase, bool on, uint32_t color)
{
  for (int i = 0; i < SEGMENT_PIXELS; ++i)
    strip.setPixelColor(ledBase + i, on ? color : offColorP);
}

void displayScores(Score &home, Score &away)
{
  // update the negative flags based on the score values
  home.isNegative = (home.value < 0);
  away.isNegative = (away.value < 0);

  // Render absolute values on 2 digits; minus signs are rendered separately.
  int homeDisplay = home.value;
  if (homeDisplay < 0)
    homeDisplay = -homeDisplay;
  homeDisplay %= 100;

  int awayDisplay = away.value;
  if (awayDisplay < 0)
    awayDisplay = -awayDisplay;
  awayDisplay %= 100;

  snprintf(home.text, sizeof(home.text), "%02d", homeDisplay);
  snprintf(away.text, sizeof(away.text), "%02d", awayDisplay);

  // Update the segments for home score
  UpdateSegmentFromChar(home.digit0, home.text[0]);
  UpdateSegmentFromChar(home.digit1, home.text[1]);
  // Update the segments for away score
  UpdateSegmentFromChar(away.digit0, away.text[0]);
  UpdateSegmentFromChar(away.digit1, away.text[1]);

  renderDigit(home.digit1, HOME_ONES, homeColorP); // text[1] = ones
  renderDigit(home.digit0, HOME_TENS, homeDisplay < 10 ? offColorP : homeColorP); // blank tens for single digits
  renderDigit(away.digit1, AWAY_ONES, awayColorP);
  renderDigit(away.digit0, AWAY_TENS, awayDisplay < 10 ? offColorP : awayColorP);
  renderMinus(HOME_MINUS, home.isNegative, homeColorP);
  renderMinus(AWAY_MINUS, away.isNegative, awayColorP);
  strip.show();

  // Combine the pixel data from home and away, plus an extra minus sign segment for each score.
}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait)
{
  for (int i = 0; i < strip.numPixels(); i++)
  {                                // For each pixel in strip...
    strip.setPixelColor(i, color); //  Set pixel's color (in RAM)
    strip.show();                  //  Update strip to match
    delay(wait);                   //  Pause for a moment
  }
}

void whiteOverRainbow(int whiteSpeed, int whiteLength)
{

  if (whiteLength >= strip.numPixels())
    whiteLength = strip.numPixels() - 1;

  int head = whiteLength - 1;
  int tail = 0;
  int loops = 3;
  int loopNum = 0;
  uint32_t lastTime = millis();
  uint32_t firstPixelHue = 0;

  for (;;)
  { // Repeat forever (or until a 'break' or 'return')
    for (int i = 0; i < strip.numPixels(); i++)
    {                                     // For each pixel in strip...
      if (((i >= tail) && (i <= head)) || //  If between head & tail...
          ((tail > head) && ((i >= tail) || (i <= head))))
      {
        strip.setPixelColor(i, strip.Color(0, 0, 0, 255)); // Set white
      }
      else
      { // else set rainbow
        int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
      }
    }

    strip.show(); // Update strip with new contents
    // There's no delay here, it just runs full-tilt until the timer and
    // counter combination below runs out.

    firstPixelHue += 40; // Advance just a little along the color wheel

    if ((millis() - lastTime) > whiteSpeed)
    { // Time to update head/tail?
      if (++head >= strip.numPixels())
      { // Advance head, wrap around
        head = 0;
        if (++loopNum >= loops)
          return;
      }
      if (++tail >= strip.numPixels())
      { // Advance tail, wrap around
        tail = 0;
      }
      lastTime = millis(); // Save time of last movement
    }
  }
}

void pulseWhite(uint8_t wait)
{
  for (int j = 0; j < 256; j++)
  { // Ramp up from 0 to 255
    // Fill entire strip with white at gamma-corrected brightness level 'j':
    strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
    strip.show();
    delay(wait);
  }

  for (int j = 255; j >= 0; j--)
  { // Ramp down from 255 to 0
    strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
    strip.show();
    delay(wait);
  }
}

void rainbowFade2White(int wait, int rainbowLoops, int whiteLoops)
{
  int fadeVal = 0, fadeMax = 100;

  // Hue of first pixel runs 'rainbowLoops' complete loops through the color
  // wheel. Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to rainbowLoops*65536, using steps of 256 so we
  // advance around the wheel at a decent clip.
  for (uint32_t firstPixelHue = 0; firstPixelHue < rainbowLoops * 65536;
       firstPixelHue += 256)
  {

    for (int i = 0; i < strip.numPixels(); i++)
    { // For each pixel in strip...

      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      uint32_t pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());

      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the three-argument variant, though the
      // second value (saturation) is a constant 255.
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue, 255,
                                                          255 * fadeVal / fadeMax)));
    }

    strip.show();
    delay(wait);

    if (firstPixelHue < 65536)
    { // First loop,
      if (fadeVal < fadeMax)
        fadeVal++; // fade in
    }
    else if (firstPixelHue >= ((rainbowLoops - 1) * 65536))
    { // Last loop,
      if (fadeVal > 0)
        fadeVal--; // fade out
    }
    else
    {
      fadeVal = fadeMax; // Interim loop, make sure fade is at max
    }
  }

  for (int k = 0; k < whiteLoops; k++)
  {
    for (int j = 0; j < 256; j++)
    { // Ramp up 0 to 255
      // Fill entire strip with white at gamma-corrected brightness level 'j':
      strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
      strip.show();
    }
    delay(1000); // Pause 1 second
    for (int j = 255; j >= 0; j--)
    { // Ramp down 255 to 0
      strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
      strip.show();
    }
  }

  delay(500); // Pause 1/2 second
}