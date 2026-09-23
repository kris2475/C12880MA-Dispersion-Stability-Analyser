#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>

// ============================================================
// WIFI & NTP CONFIGURATION
// ============================================================
#define PRIMARY_SSID     "SKYYRMR7"
#define PRIMARY_PASS     "K2xWvDFZkuCh"

#define FALLBACK_SSID    "CyDen-Guests"
#define FALLBACK_PASS    "29336Visit2#02"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;       // Adjust for your timezone offset in seconds (e.g., 3600 for UTC+1)
const int   daylightOffset_sec = 0;   // Adjust for daylight savings if applicable

// ============================================================
// OLED CONFIGURATION
// ============================================================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================
// PINS CONFIGURATION
// ============================================================
#define C12880MA_CLK_PIN      16
#define C12880MA_ST_PIN       17
#define C12880MA_VIDEO_PIN    34
#define LED_ILLUM_PIN         32
#define SD_CS_PIN             5   // Standard SPI CS for SD Card
#define SPECTROGRAPH_CHANNELS 288

// ============================================================
// SPECTROMETER DATA & STATE
// ============================================================
uint16_t spectrographData[SPECTROGRAPH_CHANNELS];
uint16_t currentMaxIntensity = 0;
uint16_t currentMinIntensity = 4095;

// System Controls
bool ledState = false;              // LED OFF by default
String userComment = "DEFAULT";     // Optional comment string for CSV logs
bool requestSaveToSD = false;       // Flag triggered via 'S' command

// Auto-Integration State
bool autoIntegrationEnabled = false;
const uint16_t TARGET_MAX_INTENSITY = 2500; 

// Timed Logging State (LOG <seconds>)
unsigned long logIntervalMs = 0;
unsigned long lastLogTime = 0;
bool timedLoggingActive = false;

// Burst Buffer State (10 frames x 288 channels)
#define BURST_SIZE 10
uint16_t burstBuffer[BURST_SIZE][SPECTROGRAPH_CHANNELS];
unsigned long burstIntegrationTimes[BURST_SIZE];
uint16_t burstMaxIntensities[BURST_SIZE];
bool isBursting = false;
int burstCount = 0;

// ============================================================
// C12880MA TIMING
// ============================================================
const unsigned long CLK_PERIOD_US = 2;
const unsigned long CLK_HALF_US   = 1;

unsigned long currentIntegrationTimeUs = 20000; // Default 20 ms
const unsigned long minIntegrationTimeUs = 11000;
const unsigned long maxIntegrationTimeUs = 1000000;

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================
void connectWiFi();
void readSpectrometer();
void clockPulse();
void handleSerialCommands();
void printHelpMenu();
void updateDisplay();
void saveSpectrumToSD();
void writeBurstToSD();
void updateAutoIntegration();
String getFormattedTimestamp();

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("========================================"));
  Serial.println(F(" C12880MA ESP32 + WIFI + NTP + LOGGING"));
  Serial.println(F("========================================"));

  // LED OFF initially
  pinMode(LED_ILLUM_PIN, OUTPUT);
  digitalWrite(LED_ILLUM_PIN, LOW);

  // Initialize OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[ERROR] OLED allocation failed!"));
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Initialize Spectrometer GPIO
  pinMode(C12880MA_CLK_PIN, OUTPUT);
  pinMode(C12880MA_ST_PIN, OUTPUT);
  digitalWrite(C12880MA_CLK_PIN, LOW);
  digitalWrite(C12880MA_ST_PIN, LOW);

  // Initialize ADC
  analogSetPinAttenuation(C12880MA_VIDEO_PIN, ADC_11db);
  analogReadResolution(12);

  // Initialize SD Card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("[WARNING] SD Card Mount Failed. Logging disabled."));
  } else {
    Serial.println(F("[INFO] SD Card Initialized Successfully."));
  }

  // Connect to WiFi and sync time
  connectWiFi();

  printHelpMenu();

  // Sacrificial scan
  Serial.println(F("[INIT] Performing first sacrificial scan..."));
  readSpectrometer();
  Serial.println(F("[INIT] Measurement ready"));
  Serial.println();
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  // 1. Process Serial Commands
  handleSerialCommands();

  // 2. Handle Burst Capture Mode if Active
  if (isBursting) {
    readSpectrometer();

    for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
      burstBuffer[burstCount][i] = spectrographData[i];
    }
    burstIntegrationTimes[burstCount] = currentIntegrationTimeUs;
    burstMaxIntensities[burstCount] = currentMaxIntensity;
    burstCount++;

    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("BURST CAPTURE"));
    display.setCursor(0, 20);
    display.print(F("Scan #: "));
    display.print(burstCount);
    display.print(F("/10"));
    display.display();

    if (burstCount >= BURST_SIZE) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print(F("WRITING BURST..."));
      display.display();

      writeBurstToSD();
      isBursting = false;
      Serial.println(F("[SD] 10-frame Burst complete and appended to /spectra_log.csv"));
    }
    return;
  }

  // 3. Normal Acquisition Loop
  readSpectrometer();

  // Update Auto-Integration if enabled
  if (autoIntegrationEnabled) {
    updateAutoIntegration();
  }

  // Handle single frame save request
  if (requestSaveToSD) {
    saveSpectrumToSD();
    requestSaveToSD = false;
  }

  // Handle Timed Periodic Logging (LOG <seconds>)
  if (timedLoggingActive && (millis() - lastLogTime >= logIntervalMs)) {
    lastLogTime = millis();
    saveSpectrumToSD();
    Serial.println(F("[LOG] Periodic spectrum frame logged to SD."));
  }

  // 4. Update OLED
  updateDisplay();
}

// ============================================================
// WIFI CONNECTION & NTP SYNC
// ============================================================
void connectWiFi() {
  Serial.print(F("[WIFI] Connecting to primary: "));
  Serial.println(PRIMARY_SSID);
  
  WiFi.begin(PRIMARY_SSID, PRIMARY_PASS);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  // Try fallback if primary failed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("\n[WIFI] Primary failed. Trying fallback: "));
    Serial.println(FALLBACK_SSID);
    WiFi.begin(FALLBACK_SSID, FALLBACK_PASS);
    
    startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\n[WIFI] Connected successfully!"));
    Serial.print(F("[WIFI] IP Address: "));
    Serial.println(WiFi.localIP());

    // Initialize and get NTP time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println(F("[NTP] Synchronizing time..."));
    
    // Wait explicitly for real-world time to be populated (up to 5 seconds)
    struct tm timeinfo;
    unsigned long ntpStart = millis();
    bool timeSet = false;
    while (millis() - ntpStart < 5000) {
      if (getLocalTime(&timeinfo)) {
        timeSet = true;
        break;
      }
      delay(200);
    }

    if (timeSet) {
      Serial.println(F("[NTP] Time synchronized successfully with seconds tracking."));
    } else {
      Serial.println(F("[NTP] Failed to obtain time within timeout window."));
    }
  } else {
    Serial.println(F("\n[WIFI] Failed to connect to any network. Timestamps will default to uptime."));
  }
}

String getFormattedTimestamp() {
  struct tm timeinfo;
  struct timeval tv;
  
  // Get both standard time elements and microsecond offsets for high accuracy
  gettimeofday(&tv, NULL);
  if (!getLocalTime(&timeinfo)) {
    return String(millis()); // Fallback to millis if NTP unavailable
  }
  
  char timeBuffer[64];
  // Formats as YYYY-MM-DD HH:MM:SS.mmm (including milliseconds)
  int millisVal = (tv.tv_usec / 1000);
  snprintf(timeBuffer, sizeof(timeBuffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, millisVal);
           
  return String(timeBuffer);
}

// ============================================================
// AUTO-INTEGRATION CONTROLLER
// ============================================================
void updateAutoIntegration() {
  if (currentMaxIntensity < 50) {
    currentIntegrationTimeUs = min(maxIntegrationTimeUs, currentIntegrationTimeUs * 2);
    return;
  }

  long error = (long)TARGET_MAX_INTENSITY - (long)currentMaxIntensity;
  if (abs(error) > 150) {
    float adjustmentRatio = (float)TARGET_MAX_INTENSITY / (float)currentMaxIntensity;
    long newTime = currentIntegrationTimeUs + (long)((float)currentIntegrationTimeUs * (adjustmentRatio - 1.0) * 0.5);
    currentIntegrationTimeUs = constrain(newTime, minIntegrationTimeUs, maxIntegrationTimeUs);
  }
}

// ============================================================
// CLOCK PULSE & READOUT
// ============================================================
void clockPulse() {
  digitalWrite(C12880MA_CLK_PIN, HIGH);
  delayMicroseconds(CLK_HALF_US);
  digitalWrite(C12880MA_CLK_PIN, LOW);
  delayMicroseconds(CLK_HALF_US);
}

void readSpectrometer() {
  digitalWrite(LED_ILLUM_PIN, ledState ? HIGH : LOW);
  if (ledState) {
    delay(2);
  }

  const unsigned long integrationClockComponent = 48UL * CLK_PERIOD_US;
  unsigned long stHighUs = 0;

  if (currentIntegrationTimeUs > integrationClockComponent) {
    stHighUs = currentIntegrationTimeUs - integrationClockComponent;
  } else {
    stHighUs = 1;
  }

  digitalWrite(C12880MA_CLK_PIN, LOW);
  digitalWrite(C12880MA_ST_PIN, LOW);
  delayMicroseconds(5);

  digitalWrite(C12880MA_ST_PIN, HIGH);
  delayMicroseconds(stHighUs);
  digitalWrite(C12880MA_ST_PIN, LOW);

  clockPulse();

  for (int i = 0; i < 86; i++) {
    clockPulse();
  }

  currentMaxIntensity = 0;
  currentMinIntensity = 4095;

  for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
    spectrographData[i] = analogRead(C12880MA_VIDEO_PIN);

    if (spectrographData[i] > currentMaxIntensity) {
      currentMaxIntensity = spectrographData[i];
    }
    if (spectrographData[i] < currentMinIntensity) {
      currentMinIntensity = spectrographData[i];
    }

    clockPulse();
  }

  digitalWrite(C12880MA_ST_PIN, HIGH);
  for (int i = 0; i < 7; i++) {
    clockPulse();
  }

  digitalWrite(C12880MA_CLK_PIN, LOW);
  digitalWrite(C12880MA_ST_PIN, LOW);
  digitalWrite(LED_ILLUM_PIN, LOW);
}

// ============================================================
// OLED UPDATE
// ============================================================
void updateDisplay() {
  display.clearDisplay();

  display.setCursor(0, 0);
  if (autoIntegrationEnabled) {
    display.print(F("Auto:"));
  } else {
    display.print(F("Int:"));
  }
  
  if (currentIntegrationTimeUs >= 1000000UL) {
    display.print(currentIntegrationTimeUs / 1000000.0, 1);
    display.print(F("s"));
  } else {
    display.print(currentIntegrationTimeUs / 1000.0, 1);
    display.print(F("ms"));
  }

  display.setCursor(68, 0);
  display.print(F("Max:"));
  display.print(currentMaxIntensity);

  // Show logging indicator if active
  if (timedLoggingActive) {
    display.setCursor(0, 56);
    display.print(F("[LOGGING ACTIVE]"));
  }

  for (int x = 0; x < 128; x++) {
    int specIndex = map(x, 0, 127, 0, SPECTROGRAPH_CHANNELS - 1);
    int barHeight = 0;

    if (currentMaxIntensity > currentMinIntensity) {
      barHeight = map(spectrographData[specIndex], currentMinIntensity, currentMaxIntensity, 0, 45);
    }
    barHeight = constrain(barHeight, 0, 45);

    display.drawFastVLine(x, 50 - barHeight, barHeight, SSD1306_WHITE);
  }
  display.display();
}

// ============================================================
// SD CARD LOGGING FUNCTIONS
// ============================================================
void saveSpectrumToSD() {
  bool fileExists = SD.exists("/spectra_log.csv");

  File dataFile = SD.open("/spectra_log.csv", FILE_APPEND);
  if (dataFile) {
    if (!fileExists) {
      dataFile.print("Timestamp,IntegrationTime_us,LED_State,Comment,Max_ADC");
      for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
        dataFile.print(",P");
        dataFile.print(i);
      }
      dataFile.println();
    }

    dataFile.print('"');
    dataFile.print(getFormattedTimestamp());
    dataFile.print('"');
    dataFile.print(',');
    dataFile.print(currentIntegrationTimeUs);
    dataFile.print(',');
    dataFile.print(ledState ? "ON" : "OFF");
    dataFile.print(',');
    dataFile.print('"');
    dataFile.print(userComment);
    dataFile.print('"');
    dataFile.print(',');
    dataFile.print(currentMaxIntensity);

    for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
      dataFile.print(',');
      dataFile.print(spectrographData[i]);
    }
    dataFile.println();
    dataFile.close();
  } else {
    Serial.println(F("[ERROR] Failed to open spectra_log.csv on SD card"));
  }
}

void writeBurstToSD() {
  bool fileExists = SD.exists("/spectra_log.csv");

  File dataFile = SD.open("/spectra_log.csv", FILE_APPEND);
  if (dataFile) {
    if (!fileExists) {
      dataFile.print("Timestamp,IntegrationTime_us,LED_State,Comment,Max_ADC");
      for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
        dataFile.print(",P");
        dataFile.print(i);
      }
      dataFile.println();
    }

    for (int b = 0; b < BURST_SIZE; b++) {
      dataFile.print('"');
      dataFile.print(getFormattedTimestamp());
      dataFile.print('"');
      dataFile.print(',');
      dataFile.print(burstIntegrationTimes[b]);
      dataFile.print(',');
      dataFile.print(ledState ? "ON" : "OFF");
      dataFile.print(',');
      dataFile.print('"');
      dataFile.print(userComment);
      dataFile.print('"');
      dataFile.print(',');
      dataFile.print(burstMaxIntensities[b]);

      for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
        dataFile.print(',');
        dataFile.print(burstBuffer[b][i]);
      }
      dataFile.println();
    }
    dataFile.close();
  } else {
    Serial.println(F("[ERROR] Failed to open /spectra_log.csv on SD card"));
  }
}

// ============================================================
// SERIAL COMMAND HANDLER
// ============================================================
void handleSerialCommands() {
  if (Serial.available() <= 0) {
    return;
  }

  String input = Serial.readStringUntil('\n');
  input.trim();
  String upperInput = input;
  upperInput.toUpperCase();

  if (upperInput.startsWith("INT ")) {
    autoIntegrationEnabled = false; 
    String valueString = input.substring(4);
    valueString.trim();
    unsigned long newInt = valueString.toInt();

    if (newInt > 0) {
      currentIntegrationTimeUs = constrain(newInt, minIntegrationTimeUs, maxIntegrationTimeUs);
      Serial.print(F("[CMD] Integration Time set to: "));
      Serial.print(currentIntegrationTimeUs);
      Serial.println(F(" us"));
    } else {
      Serial.println(F("[ERROR] Invalid integration time"));
    }
  }
  else if (upperInput.startsWith("LOG ")) {
    String valueString = input.substring(4);
    valueString.trim();
    unsigned long seconds = valueString.toInt();

    if (seconds > 0) {
      logIntervalMs = seconds * 1000UL;
      timedLoggingActive = true;
      lastLogTime = millis();
      Serial.print(F("[CMD] Timed logging enabled every "));
      Serial.print(seconds);
      Serial.println(F(" seconds."));
    } else {
      timedLoggingActive = false;
      Serial.println(F("[CMD] Timed logging disabled."));
    }
  }
  else if (upperInput.equalsIgnoreCase("A")) {
    autoIntegrationEnabled = !autoIntegrationEnabled;
    Serial.print(F("[CMD] Auto-Integration -> "));
    Serial.println(autoIntegrationEnabled ? F("ENABLED (Target Max: 2500)") : F("DISABLED"));
  }
  else if (upperInput.equalsIgnoreCase("L")) {
    ledState = !ledState;
    Serial.print(F("[CMD] LED Strobe -> "));
    Serial.println(ledState ? F("ENABLED") : F("DISABLED"));
  }
  else if (upperInput.equalsIgnoreCase("S")) {
    requestSaveToSD = true;
    Serial.println(F("[CMD] Single save requested for next frame..."));
  }
  else if (upperInput.equalsIgnoreCase("B")) {
    isBursting = true;
    burstCount = 0;
    Serial.println(F("[CMD] Starting 10-frame burst capture..."));
  }
  else if (upperInput.startsWith("COMMENT ")) {
    userComment = input.substring(8);
    Serial.print(F("[CMD] Comment updated: "));
    Serial.println(userComment);
  }
  else if (upperInput.equals("HELP")) {
    printHelpMenu();
  }
  else {
    Serial.print(F("[ERROR] Unknown command: "));
    Serial.println(input);
  }
}

// ============================================================
// HELP MENU
// ============================================================
void printHelpMenu() {
  Serial.println(F("----------------------------------------"));
  Serial.println(F(" C12880MA COMMAND MENU"));
  Serial.println(F("----------------------------------------"));
  Serial.println(F("INT <us>        - Set integration time & disable auto-int"));
  Serial.println(F("LOG <sec>       - Log spectrum every N seconds (LOG 0 to stop)"));
  Serial.println(F("A               - Toggle Auto-Integration ON/OFF (Target: 2500)"));
  Serial.println(F("L               - Toggle LED illumination strobe ON/OFF"));
  Serial.println(F("S               - Save single spectrum frame to SD card"));
  Serial.println(F("B               - Burst capture 10 frames to RAM and save to CSV"));
  Serial.println(F("COMMENT <text>  - Set log comment string (e.g. COMMENT SampleA)"));
  Serial.println(F("HELP            - Show this menu"));
  Serial.println(F("----------------------------------------"));
  Serial.println();
}