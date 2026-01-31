/* PROJECT: Dual-MCU Env Monitor (Final)
   DEVICE:  Arduino Uno (Sensor Node)
   AUTHOR:  [Your Name]
   
   FEATURES:
   - Reads DS18B20 (Inside) & DHT22 (Outside + Humidity)
   - Displays Temperature on LCD
   - Sends formatted string via UART: #INS:25.5,OUT:28.1,HUM:65.0
   - Independent error handling (one sensor failure doesn't stop the other)
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <SoftwareSerial.h>

// Pin Definitions
#define DS18B20_PIN 2
#define DHT_PIN     9
#define DHT_TYPE    DHT22
#define LED_PIN     13

// LCD Config
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 
2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// SoftwareSerial (RX=11 unused, TX=10 -> ESP32)
SoftwareSerial espSerial(11, 10); 

OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastRead = 0;

// LCD Helper to print fixed-width numbers to avoid screen flicker
void lcdPrintFixedFloat(int col, int row, float val) {
  char buf[8];
  if (isnan(val)) {
    lcd.setCursor(col, row); lcd.print("  Err "); lcd.print("  ");
  } else {
    dtostrf(val, 6, 2, buf);
    lcd.setCursor(col, row); lcd.print(buf); lcd.print((char)223); lcd.print("C");
  }
}

// DHT Retry Helper (improves stability)
float readDHTWithRetry(bool isTemp) {
  float val = isTemp ? dht.readTemperature() : dht.readHumidity();
  if (isnan(val)) {
    delay(250); // wait briefly and try again
    val = isTemp ?
    dht.readTemperature() : dht.readHumidity();
  }
  return val;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  espSerial.begin(9600);
  
  ds18b20.begin();
  dht.begin();
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Inside: ");
  lcd.setCursor(0, 1); lcd.print("Outside:");

  // Enhance DS18B20 Resolution
  uint8_t count = ds18b20.getDeviceCount();
  if (count > 0) {
    DeviceAddress addr;
    if (ds18b20.getAddress(addr, 0)) ds18b20.setResolution(addr, 12);
  }
  
  Serial.println("UNO Sensor Node Started.");
}

void loop() {
  // Wait 2000ms for stable readings
  if (millis() - lastRead < 2000) return;
  lastRead = millis();

  // 1. READ SENSORS
  ds18b20.requestTemperatures();
  float tIn = ds18b20.getTempCByIndex(0);
  float tOut = readDHTWithRetry(true);
  float hOut = readDHTWithRetry(false);

  // 2. UPDATE LCD (Temps Only)
  if (tIn == DEVICE_DISCONNECTED_C) lcdPrintFixedFloat(8, 0, NAN);
  else lcdPrintFixedFloat(8, 0, tIn);

  if (isnan(tOut)) lcdPrintFixedFloat(8, 1, NAN);
  else lcdPrintFixedFloat(8, 1, tOut);
  // 3. SERIAL MONITOR DEBUG (Show all values)
  Serial.print("Uno Monitor -> In:");
  if(tIn == DEVICE_DISCONNECTED_C) Serial.print("ERR"); else Serial.print(tIn);
  Serial.print(" C | Out:");
  if(isnan(tOut)) Serial.print("ERR"); else Serial.print(tOut);
  Serial.print(" C | Hum:");
  if(isnan(hOut)) Serial.print("ERR"); else Serial.print(hOut);
  Serial.println(" %");
  // 4. SEND DATA FRAME TO ESP32
  // Protocol: #INS:25.50,OUT:28.00,HUM:60.5
  espSerial.print("#INS:");
  if (tIn == DEVICE_DISCONNECTED_C) espSerial.print("ERR");
  else espSerial.print(tIn, 2);

  espSerial.print(",OUT:");
  if (isnan(tOut)) espSerial.print("ERR"); else espSerial.print(tOut, 2);

  espSerial.print(",HUM:");
  if (isnan(hOut)) espSerial.print("ERR"); else espSerial.print(hOut, 1);

  espSerial.print("\n");
  // End of frame

  // Blink Status LED
  digitalWrite(LED_PIN, HIGH); delay(30); digitalWrite(LED_PIN, LOW);
}