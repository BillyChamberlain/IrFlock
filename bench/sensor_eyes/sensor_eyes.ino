// sensor_eyes.ino: sonar + IR sensor, both on ONE screen.
//
// One OLED, split in two:
//   left side  = the sonar: distance in big numbers, plus a word at the top
//   right side = the IR sensor: an eye that opens when something is in front of it
//   bottom bar = the sonar again: fuller = closer
//
//   +-----------------------------+
//   | here              IR: YOU!  |   <- sonar word        IR word
//   |  42 cm      |    ( O )      |   <- distance          the eye
//   |             |     87        |                        raw IR number
//   | [#########.................]|   <- closeness bar
//   +-----------------------------+
//
// Serial prints one line per reading at 115200, like "cm 42.5 ir 1012",
// so bench.html / sense-serial.js still reads the cm the same way.
//
// Wiring, Arduino Nano:
//   HC-SR04 sonar   VCC -> 5V   GND -> GND   TRIG -> D9    ECHO -> D10
//   OLED            VCC -> 5V   GND -> GND   SDA  -> A4    SCL  -> A5
//   IR sensor       VCC -> 5V   GND -> GND   OUT  -> A3
//
// Needs the U8g2 library (Arduino IDE -> Library Manager -> "U8g2").
// Board: Arduino Nano. If upload fails: Processor -> ATmega328P (Old Bootloader).

#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// ---- Pins ---------------------------------------------------------------------

const int TRIG = 9;
const int ECHO = 10;
const int IR_PIN = A3;

// ---- Settings you can change ----------------------------------------------------

const float CLOSE_CM = 15.0;       // sonar: under this reads CLOSE!
const float HERE_CM  = 150.0;      // sonar: under this reads here, otherwise nobody
const float MAX_CM   = 200.0;      // the bar is full at 0 cm, empty at this distance

// Most 3-pin IR modules pull OUT LOW when they see something.
// If the screen says IR: YOU! with nothing there, change this to false.
const bool IR_ACTIVE_LOW = true;
const int IR_THRESHOLD = 512;      // strength above this = it sees something

// ---- Sonar ----------------------------------------------------------------------

float cm = 400;                    // latest reading (400 = nothing came back)
float cmSmooth = 400;              // smoothed, so one bad echo doesn't jump around

float readCm() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  unsigned long us = pulseIn(ECHO, HIGH, 30000);   // 0 if no echo within 30 ms
  return us ? us / 58.0 : 400.0;
}

// ---- IR -------------------------------------------------------------------------

int irRaw = 0;
bool irSees = false;

void readIr() {
  irRaw = analogRead(IR_PIN);
  int strength = IR_ACTIVE_LOW ? 1023 - irRaw : irRaw;   // 0 nothing .. 1023 right there
  irSees = strength > IR_THRESHOLD;
}

// ---- The screen -----------------------------------------------------------------

void drawScreen() {
  oled.setFont(u8g2_font_6x12_tr);

  // top line: the sonar's word on the left, the IR's word on the right
  const char *word = cmSmooth < CLOSE_CM ? "CLOSE!" :
                     cmSmooth < HERE_CM  ? "here"   : "nobody";
  oled.drawStr(0, 11, word);
  const char *irWord = irSees ? "IR: YOU!" : "IR: --";
  oled.drawStr(127 - oled.getStrWidth(irWord), 11, irWord);

  // the divider between the two halves
  oled.drawVLine(72, 16, 34);

  // left half: the distance, big
  char buf[8];
  if (cm >= 399.0) strcpy(buf, "---");
  else dtostrf(cm, 0, 0, buf);
  oled.setFont(u8g2_font_logisoso24_tn);
  int w = oled.drawStr(2, 44, buf);              // drawStr gives back the width
  oled.setFont(u8g2_font_6x12_tr);
  if (2 + w + 3 + 12 < 72) oled.drawStr(2 + w + 3, 44, "cm");

  // right half: the IR eye
  const int ex = 100, ey = 30;
  if (irSees) {
    oled.drawCircle(ex, ey, 12);
    oled.drawCircle(ex, ey, 11);
    oled.drawDisc(ex, ey, 6);
    oled.setDrawColor(0);
    oled.drawDisc(ex + 2, ey - 2, 2);            // the shine
    oled.setDrawColor(1);
  } else {
    oled.drawHLine(ex - 12, ey, 25);             // asleep
    oled.drawLine(ex - 12, ey, ex, ey + 4);
    oled.drawLine(ex, ey + 4, ex + 12, ey);
  }
  itoa(irRaw, buf, 10);
  oled.drawStr(ex - oled.getStrWidth(buf) / 2, 52, buf);

  // bottom: the sonar bar, fuller = closer
  float near = 1.0 - constrain(cmSmooth / MAX_CM, 0.0, 1.0);
  oled.drawFrame(0, 55, 128, 9);
  oled.drawBox(2, 57, (int)(near * 124), 5);
}

// ---- Setup and loop ---------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(IR_PIN, INPUT);
  oled.begin();
  oled.setBusClock(400000);
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

  oled.firstPage();
  do { drawScreen(); } while (oled.nextPage());
}
