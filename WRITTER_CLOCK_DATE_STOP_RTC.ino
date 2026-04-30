#include <Wire.h>
#include "RTClib.h"

// --- I2C ADDRESSES ---
#define MAX_ADDR     0x20  // MAX7312/13 I/O Expander
#define PANEL_EEPROM 0x50
#define RTC_EEPROM   0x57
#define RTC_ADDR     0x68

RTC_DS3231 rtc;

char screenBuf[33] = "                                "; // What we want to show
char shadowBuf[33] = "                                "; // What is currently on LCD


// --- STATE MACHINE ---
enum AppState { MENU, DASHBOARD, CLOCK, DATE, STOPWATCH, WRITER, CHARS };
AppState currentApp = DASHBOARD;
int menuSelection = 0;
int dashView = 0;
byte testCharCode = 0;

// --- UI / PERIPHERAL VARIABLES ---
bool blState = true;
byte lastButtonState = 0xFC;

// --- APP DATA ---
int setH=1, setM=1, setD=1, setMo=1, setY = 2026;
int setStage = 0; // 0=Running, 1=Hour, 2=Minute

// --- CUSTOM CHARACTER ---
byte degreeSymbol[8] = {
  0b00110, 0b01001, 0b01001, 0b00110, 0b00000, 0b00000, 0b00000, 0b00000
};

// Stopwatch Variables
unsigned long swElapsed = 0;
unsigned long swStartMillis = 0;
bool swRunning = false;

// --- WRITER APP DATA ---
char textBuffer[17] = "                "; // 16 spaces + null
int cursorPosX = 0;                        // Cursor position (0-15)
int charIndex = 0;                         // Current letter index in the alphabet
const char alphabet[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?.";

// --- LCD DRIVER ---
void sendNibble(byte nibble, bool isData) {
  byte rs = isData ? 0x01 : 0x00;
  byte base = (nibble << 4) | rs;
  auto writeP1 = [&](byte val) {
    Wire.beginTransmission(MAX_ADDR);
    Wire.write(0x02);
    Wire.write(val);
    Wire.endTransmission();
  };
  writeP1(base);
  writeP1(base | 0x04);
  delayMicroseconds(1);
  writeP1(base);
  delayMicroseconds(100);
}

void sendCommand(byte cmd) { sendNibble(cmd >> 4, false); sendNibble(cmd & 0x0F, false); }
void sendData(byte data)   { sendNibble(data >> 4, true);  sendNibble(data & 0x0F, true); }
void lcdPrint(const char* s) { while (*s) sendData(*s++); }
void setCursor(byte col, byte row) { sendCommand(0x80 | ((row == 0 ? 0x00 : 0x40) + col)); }

void refreshLCD() {
  for (int i = 0; i < 32; i++) {
    if (screenBuf[i] != shadowBuf[i]) {
      setCursor(i % 16, i / 16);
      sendData(screenBuf[i]);
      shadowBuf[i] = screenBuf[i];
    }
  }
}

void createCustomChar(byte location, byte charmap[]) {
  location &= 0x7;
  sendCommand(0x40 | (location << 3));
  for (int i = 0; i < 8; i++) sendData(charmap[i]);
}

void updateBufferLine(int startIdx, const char* text) {
  bool endReached = false;
  for (int i = 0; i < 16; i++) {
    if (!endReached && text[i] == '\0') endReached = true;
    
    if (endReached) {
      screenBuf[startIdx + i] = ' ';
    } else {
      screenBuf[startIdx + i] = text[i];
    }
  }
}

void fillLine(int startIdx, const char* text) {
  for (int i = 0; i < 16; i++) {
    // If we hit the end of the text, fill the rest with spaces
    if (text[i] == '\0') {
      for (int j = i; j < 16; j++) screenBuf[startIdx + j] = ' ';
      break;
    }
    screenBuf[startIdx + i] = text[i];
  }
}

void setup() {
  Wire.begin();
  rtc.begin();
  Wire.beginTransmission(MAX_ADDR);
  Wire.write(0x06); Wire.write(0x00); Wire.write(0xFC);
  Wire.endTransmission();
  delay(50);
  sendNibble(0x03, false); delay(5);
  sendNibble(0x03, false); delay(1);
  sendNibble(0x03, false);
  sendNibble(0x02, false);
  sendCommand(0x28); sendCommand(0x0C); sendCommand(0x01);
  delay(5);
  createCustomChar(0, degreeSymbol);
}

void loop() {
  DateTime now = rtc.now();
  Wire.beginTransmission(MAX_ADDR); Wire.write(0x00); Wire.endTransmission();
  Wire.requestFrom(MAX_ADDR, 2);

  if (Wire.available() >= 2) {
    Wire.read();
    byte p2_val = Wire.read();

    if (p2_val != lastButtonState) {
      bool pressed = false;

      // GLOBAL: Curved Arrow (P2.6) - Back to Menu
      if (!(p2_val & (1 << 6)) && (lastButtonState & (1 << 6))) {
        currentApp = MENU; setStage = 0; sendCommand(0x01); pressed = true;
      }

      // APP INPUT LOGIC
      if (currentApp == MENU && !pressed) {
        if (!(p2_val & (1 << 2)) && (lastButtonState & (1 << 2))) { blState = !blState; pressed = true; }
        if (!(p2_val & (1 << 5)) && (lastButtonState & (1 << 5))) { menuSelection = (menuSelection + 1) % 6; pressed = true; }
        if (!(p2_val & (1 << 3)) && (lastButtonState & (1 << 3))) { menuSelection = (menuSelection + 5) % 6; pressed = true; }
        if (!(p2_val & (1 << 4)) && (lastButtonState & (1 << 4))) {
          AppState apps[] = {DASHBOARD, CLOCK, DATE, STOPWATCH, WRITER, CHARS};
          currentApp = apps[menuSelection];
          setH  = now.hour(); 
          setM  = now.minute(); 
          setD  = now.day();    // Capture Day
          setMo = now.month();  // Capture Month
          setY  = now.year();   // Capture Year
          sendCommand(0x01); 
          pressed = true;
        }
      }
      else if (currentApp == CLOCK && !pressed) {
        // CHECK (P2.4): Toggle Set Mode
        if (!(p2_val & (1 << 4)) && (lastButtonState & (1 << 4))) {
          setStage++;
          if (setStage > 2) {
            rtc.adjust(DateTime(now.year(), now.month(), now.day(), setH, setM, 0));
            setStage = 0;
          }
          sendCommand(0x01); pressed = true;
        }
        if (setStage > 0) {
          if (!(p2_val & (1 << 5)) && (lastButtonState & (1 << 5))) { // Right: Up
            if (setStage == 1) setH = (setH + 1) % 24;
            else setM = (setM + 1) % 60;
            pressed = true;
          }
          if (!(p2_val & (1 << 3)) && (lastButtonState & (1 << 3))) { // Left: Down
            if (setStage == 1) setH = (setH + 23) % 24;
            else setM = (setM + 59) % 60;
            pressed = true;
          }
        }
      }
      // set date
      else if (currentApp == DATE && !pressed) {
        // CHECK (P2.4): Cycle through Day -> Month -> Year -> Save
        if (!(p2_val & (1 << 4)) && (lastButtonState & (1 << 4))) {
          setStage++;
          if (setStage > 3) {
            rtc.adjust(DateTime(setY, setMo, setD, now.hour(), now.minute(), now.second()));
            setStage = 0;
          }
          sendCommand(0x01); pressed = true;
        }
        if (setStage > 0) {
          // RIGHT Arrow (P2.5): Increase
          if (!(p2_val & (1 << 5)) && (lastButtonState & (1 << 5))) {
            if (setStage == 1) setD = (setD % 31) + 1;
            else if (setStage == 2) setMo = (setMo % 12) + 1;
            else if (setStage == 3) setY++;
            pressed = true;
          }
          // LEFT Arrow (P2.3): Decrease
          if (!(p2_val & (1 << 3)) && (lastButtonState & (1 << 3))) {
            if (setStage == 1) setD = (setD <= 1) ? 31 : setD - 1;
            else if (setStage == 2) setMo = (setMo <= 1) ? 12 : setMo - 1;
            else if (setStage == 3) setY--;
            pressed = true;
          }
        }
      }
      // STOPWATCH input logic
      else if (currentApp == STOPWATCH && !pressed) {
        // CHECK (P2.4): Start or Stop
        if (!(p2_val & (1 << 4)) && (lastButtonState & (1 << 4))) {
          if (!swRunning) {
            swStartMillis = millis() - swElapsed;
            swRunning = true;
          } else {
            swElapsed = millis() - swStartMillis;
            swRunning = false;
          }
          pressed = true;
        }

        // RED X (P2.2): Reset (Only works if stopped)
        if (!(p2_val & (1 << 2)) && (lastButtonState & (1 << 2))) {
          if (!swRunning) {
            swElapsed = 0;
            sendCommand(0x01); // Refresh screen to show 00:00.0
          }
          pressed = true;
        }
      }
      // char explorer
      else if (currentApp == CHARS && !pressed) {
        // RIGHT Arrow (P2.5): Increment character code
        if (!(p2_val & (1 << 5)) && (lastButtonState & (1 << 5))) {
          testCharCode++;
          pressed = true;
        }
        // LEFT Arrow (P2.3): Decrement character code
        if (!(p2_val & (1 << 3)) && (lastButtonState & (1 << 3))) {
          testCharCode--;
          pressed = true;
        }
      }

      else if (currentApp == WRITER && !pressed) {
        // RIGHT Arrow (P2.5): Cycle alphabet forward
        if (!(p2_val & (1 << 5)) && (lastButtonState & (1 << 5))) {
          charIndex = (charIndex + 1) % strlen(alphabet);
          textBuffer[cursorPosX] = alphabet[charIndex];
          pressed = true;
        }
        // LEFT Arrow (P2.3): Cycle alphabet backward
        if (!(p2_val & (1 << 3)) && (lastButtonState & (1 << 3))) {
          charIndex = (charIndex + strlen(alphabet) - 1) % strlen(alphabet);
          textBuffer[cursorPosX] = alphabet[charIndex];
          pressed = true;
        }
        // CHECK (P2.4): Move cursor right (next letter)
        if (!(p2_val & (1 << 4)) && (lastButtonState & (1 << 4))) {
          cursorPosX = (cursorPosX + 1) % 16;
          charIndex = 0; // Reset alphabet for next slot
          pressed = true;
        }
        // RED X (P2.2): Backspace / Clear current
        if (!(p2_val & (1 << 2)) && (lastButtonState & (1 << 2))) {
          textBuffer[cursorPosX] = ' ';
          if (cursorPosX > 0) cursorPosX--;
          pressed = true;
        }
      }
      // DASHBOARD
      else if (currentApp == DASHBOARD && !pressed) {
        if (!(p2_val & (1 << 5)) && (lastButtonState & (1 << 5))) { dashView = 1; sendCommand(0x01); pressed = true; }
        if (!(p2_val & (1 << 3)) && (lastButtonState & (1 << 3))) { dashView = 0; sendCommand(0x01); pressed = true; }
      }

      // OUTPUT HANDLING
      Wire.beginTransmission(MAX_ADDR); Wire.write(0x03);
      byte out = p2_val;
      if (blState) out &= ~0x01; else out |= 0x01;
      if (pressed) out &= ~0x02; else out |= 0x02;
      Wire.write(out); Wire.endTransmission();

      if (pressed) {
        delay(2);
        Wire.beginTransmission(MAX_ADDR); Wire.write(0x03);
        byte rest = p2_val | 0x02;
        if (blState) rest &= ~0x01; else rest |= 0x01;
        Wire.write(rest); Wire.endTransmission();
      }
      lastButtonState = p2_val;
    }
  }

  // --- DRAWING LOGIC (Direct Write - No Buffer) ---
  char buf[20];
  bool blinkOn = (millis() % 500 < 250);
  bool slowBlink = (millis() % 1000 < 500);

  if (currentApp == MENU) {
    setCursor(0, 0); lcdPrint("LEXMARK PANEL   ");
    const char* mNames[] = {"> DASHBOARD     ", "> CLOCK         ", "> DATE          ", "> STOPWATCH     ", "> WRITER        ", "> CHARACTERS    "};
    setCursor(0, 1); lcdPrint(mNames[menuSelection]);
  }
  
  else if (currentApp == DASHBOARD) {
    sprintf(buf, "    %02d:%02d:%02d    ", now.hour(), now.minute(), now.second());
    setCursor(0, 0); lcdPrint(buf);
    setCursor(0, 1);
    if (dashView == 0) {
      sprintf(buf, "   %02d.%02d.%04d   ", now.day(), now.month(), now.year());
      lcdPrint(buf);
    } else {
      sprintf(buf, "%02d.%02d.%04d %2d", now.day(), now.month(), now.year(), (int)rtc.getTemperature());
      lcdPrint(buf);
      sendData(0); lcdPrint("C "); 
    }
  }

  else if (currentApp == CLOCK) {
    if (setStage == 0) {
      sprintf(buf, "    %02d:%02d:%02d    ", now.hour(), now.minute(), now.second());
      setCursor(0, 0); lcdPrint(buf);
      setCursor(0, 1); lcdPrint("                ");
    } else {
      setCursor(0, 0); lcdPrint("SET CLOCK:      ");
      char hS[3], mS[3];
      if (setStage == 1 && blinkOn) strcpy(hS, "  "); else sprintf(hS, "%02d", setH);
      if (setStage == 2 && blinkOn) strcpy(mS, "  "); else sprintf(mS, "%02d", setM);
      sprintf(buf, "     %s:%s      ", hS, mS);
      setCursor(0, 1); lcdPrint(buf);
    }
  }

  else if (currentApp == DATE) {
    if (setStage == 0) {
      sprintf(buf, "   %02d.%02d.%04d   ", now.day(), now.month(), now.year());
      setCursor(0, 0); lcdPrint(buf);
      setCursor(0, 1); lcdPrint("                ");
    } else {
      setCursor(0, 0); lcdPrint("SET DATE:       ");
      char dS[3], mS[3], yS[5];
      if (setStage == 1 && blinkOn) strcpy(dS, "  "); else sprintf(dS, "%02d", setD);
      if (setStage == 2 && blinkOn) strcpy(mS, "  "); else sprintf(mS, "%02d", setMo);
      if (setStage == 3 && blinkOn) strcpy(yS, "    "); else sprintf(yS, "%04d", setY);
      sprintf(buf, "  %s.%s.%s   ", dS, mS, yS);
      setCursor(0, 1); lcdPrint(buf);
    }
  }

  else if (currentApp == STOPWATCH) {
    unsigned long dt = swRunning ? (millis() - swStartMillis) : swElapsed;
    unsigned int ms = (dt % 1000) / 100, s = (dt / 1000) % 60, m = (dt / 60000) % 60, h = (dt / 3600000);
    setCursor(0, 0); lcdPrint("STOPWATCH       ");
    sprintf(buf, "  %02d:%02d:%02d.%d   ", h, m, s, ms);
    setCursor(0, 1); lcdPrint(buf);
  }

  else if (currentApp == WRITER) {
    setCursor(0, 0); lcdPrint("WRITER:         ");
    setCursor(0, 1);
    for(int i=0; i<16; i++) {
      if (i == cursorPosX && blinkOn) sendData('_');
      else sendData(textBuffer[i]);
    }
  }

  else if (currentApp == CHARS) {
    // Top Row: Shows the actual character and its label
    setCursor(0, 0); lcdPrint("CHAR: "); 
    sendData(testCharCode); // This prints the glyph at the current code
    lcdPrint("          "); // Clear the rest of the line
    
    // Bottom Row: Shows the character code in HEX for reference
    sprintf(buf, "HEX CODE: 0x%02X ", testCharCode);
    setCursor(0, 1); lcdPrint(buf);
    lcdPrint("      "); // Padding to clear old digits
  }


  // --- GLOBAL STATUS 's' ---
  if (swRunning) {
    setCursor(15, 0);
    if (slowBlink) sendData('s'); else sendData(' ');
  }

  delay(30); // Slightly longer delay for stability

}
