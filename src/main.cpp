#include <Arduino.h>
#include "driver/rtc_io.h"
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <wifi_credentials.h>

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

#define DHTPIN 23     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302)

// Note: Pins 34 and 35 are input-only and don't support RTC pullups
// Using RTC-GPIO compatible pins instead
#define BUT_H 26
#define BUT_M 27
#define BUT_V 32
#define BUT_A 33
#define BUT_P 25

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds

RTC_DATA_ATTR int hugo = 0;
RTC_DATA_ATTR int vic = 0;
RTC_DATA_ATTR int arthur = 0;
RTC_DATA_ATTR int mama = 0;
RTC_DATA_ATTR int papa = 0;
RTC_DATA_ATTR int fill_count = 0;

int time_until_wakeup = 0;
bool timer_set = false;
int tempe;
int humidity;
int hour, minute, second;
int wakeup_reason;
bool oled_initialized = false;

int lastbuttoNState_H = HIGH;
int lastbuttoNState_M = HIGH;
int lastbuttoNState_V = HIGH;
int lastbuttoNState_A = HIGH;
int lastbuttoNState_P = HIGH;

Adafruit_SSD1306 oled = Adafruit_SSD1306(128, 64);
DHT dht(DHTPIN, DHTTYPE);

void setLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return;
  }

  hour = timeinfo.tm_hour;
  minute = timeinfo.tm_min;
  second = timeinfo.tm_sec;

  time_until_wakeup = (24 - hour - 1) * 3600 + (60 - minute - 1) * 60 + (60 - second);

  Serial.println();
}

void requestLocalTime(){
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void setPinModes()
{
  pinMode(BUT_H, INPUT_PULLUP);
  pinMode(BUT_M, INPUT_PULLUP);
  pinMode(BUT_V, INPUT_PULLUP);
  pinMode(BUT_A, INPUT_PULLUP);
  pinMode(BUT_P, INPUT_PULLUP);
}

void resetCounters()
{
  hugo = 0;
  vic = 0;
  arthur = 0;
  mama = 0;
  papa = 0;
  fill_count = 0;
}

void updateCounters()
{
  if (digitalRead(BUT_A) == LOW && lastbuttoNState_A == HIGH){
    if (arthur < 2){
      arthur++;
      lastbuttoNState_A = LOW;
    }
    else
      arthur = 0;
  }
  else if (digitalRead(BUT_A) == HIGH && lastbuttoNState_A == LOW){
    lastbuttoNState_A = HIGH;
  }

  if (digitalRead(BUT_H) == LOW && lastbuttoNState_H == HIGH){
    if (hugo < 2){
      hugo++;
      lastbuttoNState_H = LOW;
    }
    else
      hugo = 0;
  }
  else if (digitalRead(BUT_H) == HIGH && lastbuttoNState_H == LOW){
    lastbuttoNState_H = HIGH;
  }

  if (digitalRead(BUT_V) == LOW && lastbuttoNState_V == HIGH){
    if (vic < 2){
      vic++;
      lastbuttoNState_V = LOW;
    }
    else
      vic = 0;
  }
  else if (digitalRead(BUT_V) == HIGH && lastbuttoNState_V == LOW){
    lastbuttoNState_V = HIGH;
  }
    
  if (digitalRead(BUT_M) == LOW && lastbuttoNState_M == HIGH){
    if (mama < 2){
      mama++;
      lastbuttoNState_M = LOW;
    }
    else
      mama = 0;
  }
  else if (digitalRead(BUT_M) == HIGH && lastbuttoNState_M == LOW){
    lastbuttoNState_M = HIGH;
  }
    
  if (digitalRead(BUT_P) == LOW && lastbuttoNState_P == HIGH){
    if (papa < 2){
      papa++;
      lastbuttoNState_P = LOW;
    }
    else
      papa = 0;
  }
  else if (digitalRead(BUT_P) == HIGH && lastbuttoNState_P == LOW){
    lastbuttoNState_P = HIGH;
  }
  
  fill_count = hugo + vic + arthur + mama + papa;
}

void updateDHT22()
{
  humidity = (int)dht.readHumidity();
  tempe = (int)dht.readTemperature();
}

void oledInit(){
  oled_initialized = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.display();
}

void oledWelcome(){
  if(!oled_initialized) return; // Check if OLED is initialized
  
  oled.clearDisplay();
  oled.setTextSize(2);

  oled.setCursor(0, 0);
  oled.print("T.: " + String(tempe) + " C");

  oled.setCursor(0, 22);
  oled.print("H.: " + String(humidity) + " %");

  oled.setCursor(0, 43);
  oled.print("Count: " + String(fill_count));
  oled.display();
}

void oledPrint(){
  if(!oled_initialized) return; // Check if OLED is initialized
  
  oled.clearDisplay();
  oled.setTextSize(2);

  oled.setCursor(0, 16);
  oled.print("Ar: " + String(arthur));

  oled.setCursor(0, 32);
  oled.print("Hu: " + String(hugo));

  oled.setCursor(0, 48);
  oled.print("Vic: " + String(vic));

  oled.setCursor(0, 0);
  oled.print("Pa: " + String(papa));

  oled.setCursor(64, 0);
  oled.print("Ma: " + String(mama));

  oled.display();
}

void oledWifiConnect()
{
  if(!oled_initialized) return;
  
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(0, 16);
  oled.print("WiFi:");
  
  oled.setCursor(0, 40);
  if(WiFi.status() == WL_CONNECTED) {
    oled.print("Connected!");
  } else {
    oled.print("Loading...");
  }
  
  oled.display();
}

void oledTime(){
  if(!oled_initialized) return;
  
  oled.clearDisplay();
  oled.setTextSize(2);

  oled.setCursor(0, 0);
  oled.print((hour < 10 ? "0" : "") + String(hour) + ":" + (minute < 10 ? "0" : "") + String(minute) + ":" + (second < 10 ? "0" : "") + String(second));

  oled.setCursor(0, 32);

  if (!timer_set){
    oled.print("No Timer");
  }
  else{
    oled.print("Wakeup in :");

    oled.setCursor(0, 48);
    int hours = time_until_wakeup / 3600;
    int minutes = (time_until_wakeup % 3600) / 60;
    int seconds = time_until_wakeup % 60;
    oled.print((hours < 10 ? "0" : "") + String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds));
  }
  oled.display();
}

void sleepSetup(){
  rtc_gpio_pullup_en((gpio_num_t)BUT_H);
  rtc_gpio_pulldown_dis((gpio_num_t)BUT_H);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUT_H , 0);  // Wake when pin is LOW

  if (time_until_wakeup > 0){
    esp_sleep_enable_timer_wakeup(time_until_wakeup * uS_TO_S_FACTOR); // Wake after time_until_wakeup seconds
    timer_set = true;
  }
}

void sleepNow(){
  esp_deep_sleep_start();
}

void get_wakeup_reason(){
  esp_sleep_wakeup_cause_t esp_wakeup_reason;

  esp_wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(esp_wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : wakeup_reason=1; break;
    case ESP_SLEEP_WAKEUP_EXT1 : wakeup_reason = 2; break;
    case ESP_SLEEP_WAKEUP_TIMER : wakeup_reason = 3; break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : wakeup_reason = 4; break;
    case ESP_SLEEP_WAKEUP_ULP : wakeup_reason = 5; break;
    default : wakeup_reason = 0; break;
  }
}

void routine(int delay_ms){
  int time_count = delay_ms / 100;
  while(time_count > 0){
    updateCounters();
    updateDHT22();
    oledPrint();
    delay(100);
    time_count--;
  }

  if (fill_count > 2){
    routine(3000);
  }
}

void midgnightReset(){
  resetCounters();
  time_until_wakeup = 24 * 3600;
}

void setup() {
  setPinModes();
  oledInit();
  dht.begin();

  updateDHT22();
  oledWelcome();

  Serial.begin(115200);
  delay(3000); // Allow serial to initialize

  Serial.println(ssid);
  Serial.println(password);

  // Print the wakeup reason for ESP32
  get_wakeup_reason();

  if (wakeup_reason == 0 || wakeup_reason == 1) { // Fresh boot or wakeup from button
      routine(5000);
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);

  int wifi_attempts = 10;
  while (WiFi.status() != WL_CONNECTED && wifi_attempts > 0) {
    delay(500);
    oledWifiConnect();
    wifi_attempts--;
  }

  if(WiFi.status() == WL_CONNECTED) {
    oledWifiConnect();
    requestLocalTime();
    setLocalTime();

    WiFi.disconnect(true); // Disconnect to save power
    WiFi.mode(WIFI_OFF);
  }

  if (wakeup_reason == 3) { // Wakeup from timer at midnight
    midgnightReset();
  }
  
  sleepSetup();
  oledTime();

  delay(4000); // Display time for 4 seconds

  Serial.flush(); // Make sure all serial output is sent before sleep

  oled.clearDisplay();
  oled.display();
  sleepNow();
}

void loop() {
  // This will never be called when using deep sleep
}