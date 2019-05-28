#include "FastLED.h"
#include <EEPROM.h>
int currentModeAddress = 0;

FASTLED_USING_NAMESPACE

#define DATA_PIN 2

#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS 81
CRGB leds[NUM_LEDS];
bool active_leds[NUM_LEDS];

#define BRIGHTNESS 16
#define FRAMES_PER_SECOND 120

bool gReverseDirection = false;

//IO
#define BUTTON_PIN 3
#define KNOB_PIN A0

volatile uint8_t gCurrentPatternNumber = 6; // Index number of which pattern is current

#define BallCount 3
byte colors[3][3] = {{0xff, 0, 0},
                     {0xff, 0xff, 0xff},
                     {0, 0, 0xff}};

float Gravity = -9.81;
int StartHeight = 1;

float Height[BallCount];
float ImpactVelocityStart = sqrt(-2 * Gravity * StartHeight);
float ImpactVelocity[BallCount];
float TimeSinceLastBounce[BallCount];
int Position[BallCount];
long ClockTimeSinceLastBounce[BallCount];
float Dampening[BallCount];

int knobValue(int rangeStart, int rangeEnd)
{
  int _lastValue = analogRead(KNOB_PIN);
  return (map(_lastValue, 0, 1023, rangeStart, rangeEnd));
}

void nextPattern();
void setup()
{

  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), nextPattern, LOW);

  Serial.println("Start");

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  //  Serial.println("EEPROM.get");
  EEPROM.get(currentModeAddress, gCurrentPatternNumber);

  for (int i = 0; i < BallCount; i++)
  {
    ClockTimeSinceLastBounce[i] = millis();
    Height[i] = StartHeight;
    Position[i] = 0;
    ImpactVelocity[i] = ImpactVelocityStart;
    TimeSinceLastBounce[i] = 0;
    Dampening[i] = 0.90 - float(i) / pow(BallCount, 2);
  }
}

void rainbowCycle();
void confetti();
void fire();
void sinelon();
void juggle();
void bpm();
void BallLoop();
// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
//                                      0                   1         2       3         4       5      6
SimplePatternList gPatterns = {rainbowCycle, confetti, fire, sinelon, juggle, bpm, BallLoop};

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void loop()
{
  random16_add_entropy(random());

  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // do some periodic updates
  EVERY_N_MILLISECONDS(20) { gHue++; } // slowly cycle the "base color" through the rainbow

  // EVERY_N_SECONDS(1)
  // {
  //   //    Serial.print("Knob value:");
  //   //    Serial.println(knobValue(0, 255));
  //   Serial.print("gCurrentPatternNumber:");
  //   Serial.println(gCurrentPatternNumber);
  // }

  //  EVERY_N_SECONDS(60){
  //    nextPattern();
  //  }

  FastLED.setBrightness(knobValue(0, 255));
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern()
{
  //  Serial.println("nextPattern");
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);

  EEPROM.write(currentModeAddress, gCurrentPatternNumber);
  FastLED.clear();
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(gHue + random8(64), 200, 255);
}


void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, NUM_LEDS, 20);
  int pos = beatsin16(13, 0, NUM_LEDS);
  leds[pos] += CHSV(gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (int i = 0; i < NUM_LEDS; i++)
  { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle()
{
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, NUM_LEDS, 20);
  byte dothue = 0;
  for (int i = 0; i < 8; i++)
  {
    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
#define COOLING 80

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 100

long fireTimer;

void Fire2012()
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for (int i = 0; i < NUM_LEDS; i++)
  {
    heat[i] = qsub8(heat[i], random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for (int k = NUM_LEDS - 1; k >= 2; k--)
  {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if (random8() < SPARKING)
  {
    int y = random8(7);
    heat[y] = qadd8(heat[y], random8(160, 255));
  }

  // Step 4.  Map from heat cells to LED colors
  for (int j = 0; j < NUM_LEDS; j++)
  {
    CRGB color = HeatColor(heat[j]);
    int pixelnumber;
    if (gReverseDirection)
    {
      pixelnumber = (NUM_LEDS - 1) - j;
    }
    else
    {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }
}

void fire()
{
  if (millis() > fireTimer + 1000 / FRAMES_PER_SECOND)
  {
    fireTimer = millis();
    Fire2012(); // run simulation frame
  }
}

byte *Wheel(byte WheelPos)
{
  static byte c[3];

  if (WheelPos < 85)
  {
    c[0] = WheelPos * 3;
    c[1] = 255 - WheelPos * 3;
    c[2] = 0;
  }
  else if (WheelPos < 170)
  {
    WheelPos -= 85;
    c[0] = 255 - WheelPos * 3;
    c[1] = 0;
    c[2] = WheelPos * 3;
  }
  else
  {
    WheelPos -= 170;
    c[0] = 0;
    c[1] = WheelPos * 3;
    c[2] = 255 - WheelPos * 3;
  }

  return c;
}

void setPixel(int Pixel, byte red, byte green, byte blue)
{
  leds[Pixel].r = red;
  leds[Pixel].g = green;
  leds[Pixel].b = blue;
}

void rainbowCycle()
{

  byte *c;
  uint16_t i;

  for (i = 0; i < NUM_LEDS; i++)
  {
    c = Wheel(((i * 256 / NUM_LEDS) - gHue) & 255);
    setPixel(i, *c, *(c + 1), *(c + 2));
  }
}

void setAll(byte red, byte green, byte blue)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    setPixel(i, red, green, blue);
  }
}
void BallLoop()
{

  setAll(0, 0, 0);
  for (int i = 0; i < BallCount; i++)
  {
    TimeSinceLastBounce[i] = millis() - ClockTimeSinceLastBounce[i];
    Height[i] = 0.5 * Gravity * pow(TimeSinceLastBounce[i] / 1000, 2.0) + ImpactVelocity[i] * TimeSinceLastBounce[i] / 1000;

    if (Height[i] < 0)
    {
      Height[i] = 0;
      ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
      ClockTimeSinceLastBounce[i] = millis();

      if (ImpactVelocity[i] < 0.01)
      {
        ImpactVelocity[i] = ImpactVelocityStart;
      }
    }
    Position[i] = round(Height[i] * (NUM_LEDS - 1) / StartHeight);
  }

  for (int i = 0; i < BallCount; i++)
  {
    setPixel(Position[i], colors[i][0], colors[i][1], colors[i][2]);
  }
}
