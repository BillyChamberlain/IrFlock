// name_it.ino: AAAAAH, the face that names you and panics about it.
//
// scream_face.ino, but now it decides what you ARE before it reacts.
// Three names, each one a rule that reads only the distance in cm, held:
//
//   MONSTER   closer than 15 cm, held 1 s    -> full distorted scream, face shakes
//   STRANGER  closer than 150 cm, held 0.5 s -> silent, wide darting eyes, "who's there?"
//   ALONE     150 cm or more, held 5 s       -> falls asleep, whimpers every 20 s
//
// The name is written in the top (yellow) strip of the OLED, so a visitor
// reads exactly what it decided they are.
//
// Same serial output as bench.ino ("cm 42.5" lines at 115200), plus
// "vol 0-255" and "name 0/1/2" (0 alone, 1 stranger, 2 monster), so
// sense-serial.js works unchanged.
//
// Wiring, Arduino Nano (same as scream_face):
//   HC-SR04  VCC -> 5V    GND -> GND    TRIG -> D9    ECHO -> D10
//   OLED     VCC -> 5V    GND -> GND    SDA  -> A4    SCL  -> A5
//   Speaker  see ../scream_face/scream-wiring.png (D3 through a transistor)
// Needs the U8g2 library.

#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

const int TRIG = 9;
const int ECHO = 10;
const int SPEAKER = 3;             // Timer 2's PWM pin (OC2B)

// ---- The names: a number, a side, a hold ------------------------------------
// Change these after three fresh Serial Monitor readings of your own room.

const float MONSTER_CM  = 15.0;    // under this...
const unsigned long MONSTER_HOLD  = 1000;  // ...for 1 s
const float STRANGER_CM = 150.0;   // under this...
const unsigned long STRANGER_HOLD = 500;   // ...for 0.5 s
const unsigned long ALONE_HOLD    = 5000;  // 150 cm or more for 5 s

const unsigned long WHIMPER_EVERY = 20000; // ALONE: one sad little chirp this often
const unsigned long WHIMPER_MS    = 450;

const uint8_t ALONE = 0, STRANGER = 1, MONSTER = 2;
const char *const LABEL[] = { "alone.", "who's there?", "MONSTER!!!" };

uint8_t name = ALONE;                 // what it has decided you are
uint8_t candidate = ALONE;            // what the rule says right now
unsigned long candidateSince = 0;

// ---- The scream, made one sample at a time in an interrupt -------------------

const uint16_t RATE = 15625;       // samples a second: 16 MHz / 1024

volatile uint16_t baseStep = 0;
volatile uint8_t vol = 0;
volatile uint8_t drive = 1;

uint16_t phase = 0, step = 0, rasp = 0, lfsr = 0xACE1;
uint8_t lfo = 0, tick = 0;

ISR(TIMER1_COMPA_vect) {
  if (++tick >= 32) {
    tick = 0;
    lfo += 3;
    int8_t tri = (lfo < 128 ? lfo : 255 - lfo) - 64;
    uint16_t b = baseStep;
    int16_t wobble = ((int32_t)(b >> 4) * tri) >> 6;
    int16_t jitter = ((int32_t)(b >> 6) * (int8_t)lfsr) >> 7;
    step = b + wobble + jitter;
  }
  lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400u);

  phase += step;
  rasp += 300;
  int16_t s = (int16_t)(phase >> 8) - 128;
  s += (int8_t)(lfsr >> 8) >> 2;
  if (rasp & 0x8000) s = (s * 3) >> 2;
  s *= drive;
  if (s > 127) s = 127;
  if (s < -128) s = -128;
  OCR2B = ((uint16_t)(s + 128) * vol) >> 8;
}

bool sounding = false;

void soundOn() {
  if (sounding) return;
  TCNT1 = 0;
  TIMSK1 |= _BV(OCIE1A);
  TCCR2A |= _BV(COM2B1);
  sounding = true;
}

void soundOff() {
  if (!sounding) return;
  TIMSK1 &= ~_BV(OCIE1A);
  TCCR2A &= ~_BV(COM2B1);
  digitalWrite(SPEAKER, LOW);
  sounding = false;
  vol = 0;
}

void setPitch(float hz) {
  uint16_t s = hz * 65536.0 / RATE;
  noInterrupts();
  baseStep = s;
  interrupts();
}

// ---- The sensor ----------------------------------------------------------------

float readCm() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  unsigned long us = pulseInLong(ECHO, HIGH, 30000);
  return us ? us / 58.0 : 400.0;   // nothing came back reads as far away
}

// ---- The face --------------------------------------------------------------------

const int EYE_L = 40, EYE_R = 88, EYE_Y = 29;
const int MOUTH_X = 64, MOUTH_Y = 54;

float mouth = 0;                   // 0 shut .. 1 full scream
unsigned long nextBlink = 3000;
uint8_t frame = 0;
bool whimpering = false;

int sx, sy, dx, dy, tx;
bool blink;

void pickFrame(unsigned long now) {
  int shake = name == MONSTER ? 4 : 0;
  sx = shake ? random(-shake, shake + 1) : 0;
  sy = shake ? random(-shake / 2, shake / 2 + 1) : 0;
  tx = shake ? random(-1, 2) : 0;
  // STRANGER: pupils dart around looking for you. MONSTER: they jitter.
  bool dart = name == STRANGER && (frame % 6 == 0);
  if (dart) { dx = random(-3, 4); dy = random(-2, 3); }
  if (name == MONSTER) { dx = random(-2, 3); dy = random(-1, 2); }
  blink = name == STRANGER && now > nextBlink;
  if (now > nextBlink + 120) nextBlink = now + random(1500, 4000);
}

void drawLabel(const char *label) {
  oled.setFont(u8g2_font_6x12_tr);
  oled.drawStr(2 + tx, 11, label);
}

void drawSleeping() {
  oled.drawHLine(EYE_L - 9, EYE_Y, 18);
  oled.drawHLine(EYE_R - 9, EYE_Y, 18);
  if (whimpering) {
    // a single tear, and a wobbly little frown
    oled.drawPixel(EYE_L - 4, EYE_Y + 4 + (frame % 4));
    oled.drawLine(MOUTH_X - 5, MOUTH_Y + 1, MOUTH_X, MOUTH_Y - 1);
    oled.drawLine(MOUTH_X, MOUTH_Y - 1, MOUTH_X + 5, MOUTH_Y + 1);
  } else {
    oled.drawHLine(MOUTH_X - 4, MOUTH_Y, 8);
    uint8_t z = (frame / 4) % 12;
    oled.setFont(u8g2_font_6x12_tr);
    oled.drawStr(100 + z / 2, 44 - z * 2, "z");
  }
  drawLabel(whimpering ? "alone. hello?" : LABEL[ALONE]);
}

void drawEye(int x, int y, int r, int pupil, bool shut) {
  if (shut) { oled.drawHLine(x - r, y, 2 * r + 1); return; }
  oled.drawCircle(x, y, r);
  oled.drawCircle(x, y, r - 1);
  oled.drawDisc(x + dx, y + dy, pupil);
}

void drawAwake(float cm) {
  int r = 8 + (int)(mouth * 3) + (name == STRANGER ? 2 : 0);
  int pupil = name == MONSTER ? 1 : 3;
  drawEye(EYE_L + sx, EYE_Y + sy, r, pupil, blink);
  drawEye(EYE_R + sx, EYE_Y + sy, r, pupil, blink);

  int lift = name == MONSTER ? 4 : 2;
  int by = EYE_Y - r - 2 + sy;
  oled.drawLine(EYE_L - 9 + sx, by, EYE_L + 7 + sx, by - lift);
  oled.drawLine(EYE_R - 7 + sx, by - lift, EYE_R + 9 + sx, by);

  int rx = 3 + (int)(mouth * 19);
  int ry = 2 + (int)(mouth * 6);
  int mx = MOUTH_X + sx, my = MOUTH_Y + sy;
  oled.drawEllipse(mx, my, rx, ry);
  if (rx > 4 && ry > 3) {
    oled.drawEllipse(mx, my, rx - 1, ry - 1);
    oled.drawFilledEllipse(mx, my + ry - 2, rx / 2, 2, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  }

  drawLabel(LABEL[name]);
  if (cm < 399.0) {
    char buf[8];
    dtostrf(cm, 0, 0, buf);
    strcat(buf, "cm");
    oled.setFont(u8g2_font_6x12_tr);
    oled.drawStr(127 - oled.getStrWidth(buf), 11, buf);
  }
}

// ---- The rule: numbers in, a name out --------------------------------------------

float cmSmooth = 400;

uint8_t ruleSays(float cm) {
  if (cm < MONSTER_CM) return MONSTER;
  if (cm < STRANGER_CM) return STRANGER;
  return ALONE;
}

unsigned long holdFor(uint8_t n) {
  return n == MONSTER ? MONSTER_HOLD : n == STRANGER ? STRANGER_HOLD : ALONE_HOLD;
}

void decide(float cm, unsigned long now) {
  uint8_t says = ruleSays(cm);
  if (says == name) { candidate = name; return; }
  if (says != candidate) { candidate = says; candidateSince = now; }
  if (now - candidateSince >= holdFor(says)) name = says;
}

// ---- The loop ----------------------------------------------------------------------

unsigned long whimperAt = WHIMPER_EVERY;
uint8_t lastName = ALONE;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(SPEAKER, OUTPUT);
  digitalWrite(SPEAKER, LOW);
  randomSeed(analogRead(A0));

  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  OCR2B = 0;
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS10);
  OCR1A = F_CPU / RATE - 1;

  oled.begin();
  oled.setBusClock(400000);
}

void loop() {
  float cm = readCm();
  unsigned long now = millis();

  // one bad echo shouldn't rename anybody
  cmSmooth += (cm - cmSmooth) * 0.5;
  decide(cmSmooth, now);

  if (name != lastName) {
    if (name == ALONE) whimperAt = now + WHIMPER_EVERY;
    lastName = name;
  }

  // ---- It does: one action per name, no two the same ----
  float mouthTarget = 0;
  if (name == MONSTER) {
    // full scream, max distortion, pitch climbing the longer you stay
    float t = constrain((now - candidateSince) / 4000.0, 0.0, 1.0);
    setPitch(1500 + t * 500);
    drive = 4;
    vol = 255;
    soundOn();
    mouthTarget = 1.0;
    whimpering = false;
  } else if (name == STRANGER) {
    // dead silent. just staring. that's worse.
    soundOff();
    mouthTarget = 0.12;
    whimpering = false;
  } else {
    // ALONE: asleep, and every 20 s one tiny falling whimper
    mouthTarget = 0;
    if (!whimpering && now >= whimperAt) {
      whimpering = true;
      drive = 1;
      vol = 45;
      soundOn();
    }
    if (whimpering) {
      unsigned long t = now - whimperAt;
      if (t < WHIMPER_MS) {
        setPitch(1100 - (t * 600.0 / WHIMPER_MS));   // 1100 Hz sliding to 500 Hz
      } else {
        soundOff();
        whimpering = false;
        whimperAt = now + WHIMPER_EVERY;
      }
    }
  }
  mouth += (mouthTarget - mouth) * 0.5;

  Serial.print("cm ");
  Serial.print(cm, 1);
  Serial.print(" vol ");
  Serial.print(vol);
  Serial.print(" name ");
  Serial.print((int)name);
  Serial.println();

  frame++;
  pickFrame(now);
  oled.firstPage();
  do {
    if (name == ALONE) drawSleeping();
    else drawAwake(cm);
  } while (oled.nextPage());
}
