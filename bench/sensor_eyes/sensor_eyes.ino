// sensor_eyes.ino: both sensors, both screens, one file.
//
// Screen 1 shows what the sonar sees: the distance in big numbers, a bar,
// and a word for how close you are.
// Screen 2 shows what the IR sensor sees: an eye that opens when something
// is in front of it, the raw number, and a scrolling graph.
//
// Serial prints one line per reading at 115200, like "cm 42.5 ir 1012",
// so bench.html / sense-serial.js still reads the cm the same way.
//
// Wiring, Arduino Nano:
//   HC-SR04 sonar   VCC -> 5V   GND -> GND   TRIG -> D9    ECHO -> D10
//   OLED #1 (sonar) VCC -> 5V   GND -> GND   SDA  -> A4    SCL  -> A5
//   IR sensor       VCC -> 5V   GND -> GND   OUT  -> A3
//   OLED #2 (IR)    VCC -> 5V   GND -> GND   SDA  -> A1    SCL  -> A2
//
// OLED #2 uses its own software I2C pins (A1/A2), so both screens can keep
// the factory address 0x3C. No soldering.
//
// Needs the U8g2 library (Arduino IDE -> Library Manager -> "U8g2").
// Board: Arduino Nano. If upload fails: Processor -> ATmega328P (Old Bootloader).
//Fixed the I2C clock speed to 400 kHz, because the Nano's default 100 kHz is too slow for the screens to keep up with the fast loop().

#include <U8g2lib.h>
#include <Wire.h>

// Page mode (the _1_ in the name) keeps each screen's buffer at 128 bytes,
// so two screens fit in the Nano's 2 KB of RAM.
U8G2_SSD1306_128X64_NONAME_1_HW_I2C sonarScreen(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_1_SW_I2C irScreen(U8G2_R0, /* SCL */ A2, /* SDA */ A1, U8X8_PIN_NONE);

// ---- Pins ---------------------------------------------------------------------

const int TRIG = 9;
const int ECHO = 10;
const int IR_PIN = A3;

// ---- Settings you can change ----------------------------------------------------

const float CLOSE_CM = 15.0;       // sonar: under this reads CLOSE
const float HERE_CM  = 150.0;      // sonar: under this reads HERE, otherwise NOBODY
const float MAX_CM   = 200.0;      // the bar is full at 0 cm, empty at this distance

// Most 3-pin IR modules pull OUT LOW when they see something.
// If screen 2 says SEES YOU with nothing there, change this to false.
const bool IR_ACTIVE_LOW = true;
const int IR_THRESHOLD = 512;      // strength above this = it sees something

// ---- Sonar ----------------------------------------------------------------------

float cm = 400;                    // latest reading (400 = nothing came back)
float cmSmooth = 400;              // smoothed, so one bad echo doesn't jump the screen

float readCm() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  unsigned long us = pulseIn(ECHO, HIGH, 30000);   // 0 if no echo within 30 ms
  return us ? us / 58.0 : 400.0;
}

void drawSonar() {
  sonarScreen.setFont(u8g2_font_6x12_tr);

  // top strip: the word
  const char *word = cmSmooth < CLOSE_CM ? "SONAR: CLOSE!" :
                     cmSmooth < HERE_CM  ? "SONAR: here"   : "SONAR: nobody";
  sonarScreen.drawStr(0, 11, word);

  // the distance, big
  char buf[8];
  if (cm >= 399.0) strcpy(buf, "---");
  else dtostrf(cm, 0, 0, buf);
  sonarScreen.setFont(u8g2_font_logisoso24_tn);
  int w = sonarScreen.drawStr(4, 46, buf);      // drawStr returns the width
  sonarScreen.setFont(u8g2_font_6x12_tr);
  sonarScreen.drawStr(4 + w + 4, 46, "cm");

  // the bar: fuller = closer
  float near = 1.0 - constrain(cmSmooth / MAX_CM, 0.0, 1.0);
  sonarScreen.drawFrame(0, 54, 128, 10);
  sonarScreen.drawBox(2, 56, (int)(near * 124), 6);
}

// ---- IR -------------------------------------------------------------------------

int irRaw = 0;
int irStrength = 0;                // 0 nothing .. 1023 right in front of it
bool irSees = false;
uint8_t irHistory[78];             // one column of the graph per reading
uint8_t irHead = 0;

void readIr() {
  irRaw = analogRead(IR_PIN);
  irStrength = IR_ACTIVE_LOW ? 1023 - irRaw : irRaw;
  irSees = irStrength > IR_THRESHOLD;
  irHistory[irHead] = irStrength >> 5;            // 0..31 pixels tall
  irHead = (irHead + 1) % sizeof(irHistory);
}

void drawIr() {
  irScreen.setFont(u8g2_font_6x12_tr);
  irScreen.drawStr(0, 11, irSees ? "IR: SEES YOU" : "IR: nothing");
  char buf[6];
  itoa(irRaw, buf, 10);
  irScreen.drawStr(127 - irScreen.getStrWidth(buf), 11, buf);

  // the eye: open with a big pupil when it sees you, a sleepy slit when not
  const int ex = 24, ey = 40;
  if (irSees) {
    irScreen.drawCircle(ex, ey, 15);
    irScreen.drawCircle(ex, ey, 14);
    irScreen.drawDisc(ex, ey, 7);
    irScreen.setDrawColor(0);
    irScreen.drawDisc(ex + 3, ey - 3, 2);         // the shine
    irScreen.setDrawColor(1);
  } else {
    irScreen.drawHLine(ex - 14, ey, 29);
    irScreen.drawLine(ex - 14, ey, ex, ey + 4);
    irScreen.drawLine(ex, ey + 4, ex + 14, ey);
  }

  // the graph: newest reading on the right, dotted line = threshold
  const int gx = 48, gy = 63, gh = 32;
  irScreen.drawFrame(gx - 1, gy - gh - 1, sizeof(irHistory) + 2, gh + 2);
  int ty = gy - (IR_THRESHOLD >> 5);
  for (int x = 0; x < (int)sizeof(irHistory); x += 4) irScreen.drawPixel(gx + x, ty);
  for (uint8_t i = 0; i < sizeof(irHistory); i++) {
    uint8_t v = irHistory[(irHead + i) % sizeof(irHistory)];
    if (v) irScreen.drawVLine(gx + i, gy - v, v);
  }
}

// ---- Setup and loop ---------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(IR_PIN, INPUT);

  sonarScreen.begin();
  sonarScreen.setBusClock(400000);
  irScreen.begin();
}

void loop() {
  cm = readCm();
  cmSmooth += (cm - cmSmooth) * 0.5;
  readIr();

  Serial.print("cm ");
  Serial.print(cm, 1);
  Serial.print(" ir ");
  Serial.print(irRaw);
  Serial.println();

  sonarScreen.firstPage();
  do { drawSonar(); } while (sonarScreen.nextPage());

  irScreen.firstPage();
  do { drawIr(); } while (irScreen.nextPage());
}
