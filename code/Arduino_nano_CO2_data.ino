/* BOARD: Arduino Uno / Nano
   PROJECT: CO2-Sync
   DEVELOPER: [Your Name]
   ROLE: Sensor Reader & Display
*/

#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// MH-Z19 Connections: Arduino D10 -> Sensor TX, Arduino D11 -> Sensor RX
SoftwareSerial co2Serial(10, 11);
// I2C LCD Address
LiquidCrystal_I2C lcd(0x27, 16, 2);

// MH-Z19 Hex Command
byte cmd_read[] = {0xFF, 0x01, 0x86, 0, 0, 0, 0, 0, 0x79};
uint8_t response[9];

int co2ppm = 0;
int tempC = 0;

uint8_t getChecksum(uint8_t *packet) {
  uint8_t sum = 0;
  for (int i = 1; i < 8; i++) sum += packet[i];
  return (uint8_t)(0xFF - sum + 1);
}

bool readSensor() {
  while (co2Serial.available()) co2Serial.read();
  co2Serial.write(cmd_read, 9);
  
  unsigned long start = millis();
  int idx = 0;
  while (millis() - start < 1000) {
    if (co2Serial.available()) {
      response[idx++] = co2Serial.read();
      if (idx == 9) break;
    }
  }

  if (idx < 9) return false;
  if (response[0] != 0xFF || response[1] != 0x86) return false;
  if (getChecksum(response) != response[8]) return false;
  co2ppm = (response[2] << 8) + response[3];
  tempC = response[4] - 40;
  return true;
}

void setup() {
  Serial.begin(9600); 
  co2Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // --- SMART STARTUP SEQUENCE ---
  unsigned long startTime = millis();
  const unsigned long warmupDuration = 120000;
  // 120 seconds

  while (millis() - startTime < warmupDuration) {
    unsigned long elapsed = millis() - startTime;
    unsigned long remaining = (warmupDuration - elapsed) / 1000;

    if (elapsed < 2000) {
       // 0-1 Sec: Developer Name
       if (elapsed < 1000) {
         lcd.setCursor(0, 0);
         // Make sure your name fits in 16 chars!
         lcd.print("[Your Name]     ");
         lcd.setCursor(0, 1); lcd.print("Developer       ");
       } 
       // 1-2 Sec: Project Name
       else {
         lcd.setCursor(0, 0);
         lcd.print("Project         ");
         lcd.setCursor(0, 1);
         lcd.print("CO2-Sync        ");
       }
    } else {
       // 2-120 Sec: Warmup Countdown
       lcd.setCursor(0, 0);
       lcd.print("Sensor Warmup...");
       lcd.setCursor(0, 1); 
       lcd.print("Time left: "); 
       lcd.print(remaining); 
       lcd.print("s   ");
    }
    
    // Optional debug (NodeMCU ignores this)
    Serial.print("Warmup: "); Serial.println(remaining);
    delay(100); 
  }
  
  lcd.clear();
}

void loop() {
  if (readSensor()) {
    // 1. Update LCD
    lcd.setCursor(0, 0);
    lcd.print("CO2: "); lcd.print(co2ppm); lcd.print(" ppm    ");
    lcd.setCursor(0, 1);
    lcd.print("Temp: "); lcd.print(tempC);
    lcd.print(" C      ");

    // 2. Send to NodeMCU via Serial (After warmup is done)
    Serial.print(co2ppm);
    Serial.print(",");
    Serial.println(tempC); 
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error    ");
    Serial.println("ERR,ERR");
  }
  
  delay(2000); 
}