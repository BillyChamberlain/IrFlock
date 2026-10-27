// scream_face.ino: a face on the OLED that screams louder the closer you get.
//
// Nobody in range: the face sleeps. Something closer than ON_CM wakes it up
// screaming. The closer it gets, the louder and higher the scream, the wider
// the mouth, the bigger the eyes and the harder the face shakes. When nothing
// has been in range for OFF_MS it calms down and goes back to sleep.
//
// Two kinds of scream, picked with USE_CLIP below:
//   0 (the default): made on the fly, no memory used. A harsh sawtooth with
//     wobble, rasp and noise that climbs in pitch as you get closer.
//   1: a recorded scream looped from sound.h, about a second of it. Make
//     sound.h from any MP3: python3 mp3_to_sound.py scream.mp3
// Either way it's pushed into distortion as you get closer, for volume.
//
// It prints "cm 42.5 vol 180" lines at 115200, the format bench.ino uses, so
// bench.html reads it: cm becomes present, vol becomes loud (sense-serial.js).
//
// Wiring, Arduino Nano:
//   HC-SR04  VCC -> 5V    GND -> GND    TRIG -> D9    ECHO -> D10
//   OLED     VCC -> 5V    GND -> GND    SDA  -> A4    SCL  -> A5
//   Speaker  see scream-wiring.png (D3 through a transistor for real volume)
// Needs the U8g2 library.

#define USE_CLIP 0

#include <U8g2lib.h>
#include <Wire.h>
#if USE_CLIP
#include "sound.h"
#endif

// Page mode: draws the screen in eight strips with a 128-byte buffer, because
// the Nano has 2 KB of RAM and a full frame would take half of it.
U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

const int TRIG = 9;
const int ECHO = 10;
const int SPEAKER = 3;             // Timer 2's PWM pin (OC2B)

const float ON_CM = 150.0;         // closer than this wakes it up screaming
const float LOUD_CM = 15.0;        // full scream at this distance or closer
const uint8_t QUIET = 50;          // the volume at the edge of range, 0-255
const unsigned long OFF_MS = 1000; // nothing in range this long: back to sleep

const float LOW_HZ = 750;          // scream pitch at the edge of range
const float HIGH_HZ = 1700;        // scream pitch up close

#if USE_CLIP
const uint16_t RATE = SOUND_RATE;  // samples a second, from sound.h
#else
const uint16_t RATE = 15625;       // samples a second: 16 MHz / 1024
#endif

// --- the scream, one sample at a time in an interrupt -----------------------

volatile uint16_t baseStep = 0;    // the pitch, set by loop()
volatile uint8_t vol = 0;          // 0-255, set by loop()
volatile uint8_t drive = 1;        // distortion, 1 (raw) to 4 (nearly square)

uint16_t pos = 0;                  // USE_CLIP: the next sample of sound.h
uint16_t phase = 0, step = 0, rasp = 0, lfsr = 0xACE1;
uint8_t lfo = 0, tick = 0;

ISR(TIMER1_COMPA_vect) {
#if USE_CLIP
  int16_t s = (int16_t)pgm_read_byte(&SOUND[pos]) - 128;
  if (++pos >= SOUND_LEN) pos = 0;
#else
  // Every 32 samples (2 ms): wobble the pitch, like a voice that can't hold it.
  if (++tick >= 32) {
    tick = 0;
    lfo += 3;                                    // about 6 wobbles a second
    int8_t tri = (lfo < 128 ? lfo : 255 - lfo) - 64;
    uint16_t b = baseStep;
    int16_t wobble = ((int32_t)(b >> 4) * tri) >> 6;           // +-6%
    int16_t jitter = ((int32_t)(b >> 6) * (int8_t)lfsr) >> 7;  // +-1.5%, random
    step = b + wobble + jitter;
  }
  lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400u);  // white noise

  phase += step;
  rasp += 300;                                   // a 70 Hz flutter: the rasp
  int16_t s = (int16_t)(phase >> 8) - 128;       // sawtooth, -128..127
  s += (int8_t)(lfsr >> 8) >> 2;                 // breath
  if (rasp & 0x8000) s = (s * 3) >> 2;
#endif
  s *= drive;                                    // push it into distortion
  if (s > 127) s = 127;
  if (s < -128) s = -128;
  OCR2B = ((uint16_t)(s + 128) * vol) >> 8;
}

bool screaming = false;

void startScream() {
  pos = 0;                         // a recorded clip starts from the top
  TCNT1 = 0;
  TIMSK1 |= _BV(OCIE1A);           // start the sample interrupt
  TCCR2A |= _BV(COM2B1);           // connect the PWM to D3
  screaming = true;
}

void stopScream() {
  TIMSK1 &= ~_BV(OCIE1A);
  TCCR2A &= ~_BV(COM2B1);          // D3 back to a plain pin, held low, so no
  digitalWrite(SPEAKER, LOW);      // current flows through the speaker at rest
  screaming = false;
  vol = 0;
}

// --- the sensor -------------------------------------------------------------

float readCm() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  // pulseInLong times with micros(), so the scream's interrupt doesn't throw
  // the reading off the way it would with pulseIn.
  unsigned long us = pulseInLong(ECHO, HIGH, 30000);
  return us ? us / 58.0 : 400.0;
}

// --- the face ---------------------------------------------------------------
//
// The top 16 rows (yellow on two-colour screens) hold the status: zzz or
// AAAH, and the distance. The face lives in the blue below.

const int EYE_L = 40, EYE_R = 88, EYE_Y = 29;
const int MOUTH_X = 64, MOUTH_Y = 54;

float mouth = 0;                   // 0 asleep / calm .. 1 full scream
unsigned long nextBlink = 3000;
uint8_t frame = 0;

void drawSleeping() {
  // closed eyes, a little line of a mouth, and z's that drift up
  oled.drawHLine(EYE_L - 9, EYE_Y, 18);
  oled.drawHLine(EYE_R - 9, EYE_Y, 18);
  oled.drawPixel(EYE_L - 10, EYE_Y - 1);
  oled.drawPixel(EYE_R + 9, EYE_Y - 1);
  oled.drawHLine(MOUTH_X - 4, MOUTH_Y, 8);
  oled.setFont(u8g2_font_6x12_tr);
  oled.drawStr(2, 11, "zzz");
  uint8_t z = (frame / 4) % 12;
  oled.drawStr(100 + z / 2, 44 - z * 2, "z");
}

void drawEye(int x, int y, int r, int pupil, int dx, int dy, bool blink) {
  if (blink) {
    oled.drawHLine(x - r, y, 2 * r + 1);
    return;
  }
  oled.drawCircle(x, y, r);
  oled.drawCircle(x, y, r - 1);
  oled.drawDisc(x + dx, y + dy, pupil);
}

// Random bits (shake, darting pupils, blinks) are picked once per frame in
// pickFrame(): page mode calls the draw code once per strip, and fresh random
// numbers each time would tear the face apart.
int sx, sy, dx, dy, tx;
bool blink;

void pickFrame(unsigned long now) {
  int shake = mouth > 0.4 ? (int)(mouth * 4) : 0;
  sx = shake ? random(-shake, shake + 1) : 0;
  sy = shake ? random(-shake / 2, shake / 2 + 1) : 0;
  tx = shake ? random(-1, 2) : 0;
  dx = mouth > 0.6 ? random(-2, 3) : 0;
  dy = mouth > 0.6 ? random(-1, 2) : 0;
  blink = mouth < 0.5 && now > nextBlink;
  if (now > nextBlink + 120) nextBlink = now + random(2000, 5000);
}

void drawAwake(float cm) {
  // Shake harder as it screams harder (sx, sy from pickFrame).
  // Eyes: wider and wilder up close, pupils shrink to pinpricks.
  int r = 8 + (int)(mouth * 3);
  int pupil = 4 - (int)(mouth * 3);
  drawEye(EYE_L + sx, EYE_Y + sy, r, pupil, dx, dy, blink);
  drawEye(EYE_R + sx, EYE_Y + sy, r, pupil, dx, dy, blink);

  // Eyebrows: inner ends shoot up as it panics.
  int lift = (int)(mouth * 4);
  int by = EYE_Y - r - 2 + sy;
  oled.drawLine(EYE_L - 9 + sx, by, EYE_L + 7 + sx, by - lift);
  oled.drawLine(EYE_R - 7 + sx, by - lift, EYE_R + 9 + sx, by);

  // Mouth: a little o that stretches into a full scream with the volume.
  int rx = 3 + (int)(mouth * 19);
  int ry = 2 + (int)(mouth * 6);
  int mx = MOUTH_X + sx, my = MOUTH_Y + sy;
  oled.drawEllipse(mx, my, rx, ry);
  if (rx > 4 && ry > 3) {
    oled.drawEllipse(mx, my, rx - 1, ry - 1);
    oled.drawFilledEllipse(mx, my + ry - 2, rx / 2, 2, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);  // tongue
  }

  // Status: more A's the louder it gets, and the distance on the right.
  char buf[16];
  uint8_t n = 1 + (uint8_t)(mouth * 9);
  buf[0] = 'A';
  for (uint8_t i = 1; i <= n; i++) buf[i] = 'A';
  buf[n + 1] = 'H';
  buf[n + 2] = mouth > 0.7 ? '!' : 0;
  buf[n + 3] = 0;
  oled.setFont(u8g2_font_6x12_tr);
  oled.drawStr(2 + tx, 11, buf);
  if (cm < 399.0) {
    dtostrf(cm, 0, 0, buf);
    strcat(buf, "cm");
    oled.drawStr(127 - oled.getStrWidth(buf), 11, buf);
  }
}

// --- the loop ---------------------------------------------------------------

unsigned long lastSeen = 0;
float level = 0;                   // smoothed volume, so it swells instead of jumping
float cmSmooth = 400;

void setup() {
  Serial.begin(115200);            // sense-serial.js opens the port at this speed
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(SPEAKER, OUTPUT);
  digitalWrite(SPEAKER, LOW);
  randomSeed(analogRead(A0));

  // Timer 2: fast PWM, no prescaler, 16 MHz / 256 = 62.5 kHz on D3.
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  OCR2B = 0;
  // Timer 1: interrupts RATE times a second (enabled in startScream).
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS10);
  OCR1A = F_CPU / RATE - 1;

  oled.begin();
  oled.setBusClock(400000);
}

void loop() {
  float cm = readCm();
  unsigned long now = millis();

  if (cm < ON_CM) {
    lastSeen = now;
    if (!screaming) {
      startScream();
      level = 0;
    }
    cmSmooth += (cm - cmSmooth) * 0.5;
    // 0 at the edge of range, 1 at LOUD_CM or closer
    float nearness = constrain((ON_CM - cmSmooth) / (ON_CM - LOUD_CM), 0.0, 1.0);
    float target = QUIET + nearness * (255 - QUIET);
    level += (target - level) * 0.4;
    vol = (uint8_t)level;
    drive = 1 + (uint8_t)(nearness * 3.5);
    uint16_t s = (LOW_HZ + nearness * (HIGH_HZ - LOW_HZ)) * 65536.0 / RATE;
    noInterrupts();
    baseStep = s;
    interrupts();
  } else if (screaming && now - lastSeen > OFF_MS) {
    stopScream();
    cmSmooth = 400;
  }

  // The mouth follows the volume, so it opens as the scream gets louder.
  float target = screaming ? (vol - QUIET) / (255.0 - QUIET) * 0.85 + 0.15 : 0;
  mouth += (target - mouth) * 0.5;

  Serial.print("cm ");
  Serial.print(cm, 1);
  Serial.print(" vol ");
  Serial.print(vol);
  Serial.println();

  frame++;
  pickFrame(now);
  oled.firstPage();
  do {
    if (screaming) drawAwake(cm);
    else drawSleeping();
  } while (oled.nextPage());
}
