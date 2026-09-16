/*
 * ============================================================
 * Hamamatsu C12880MA Spectrometer with LED Strobe Control, 
 * SD Card Logging, Auto-Exposure, OLED, & Serial Commands
 * ============================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>

// ============================================================
// OLED CONFIGURATION
// ============================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================
// PINS CONFIGURATION
// ============================================================
#define C12880MA_CLK_PIN      16
#define C12880MA_ST_PIN       17
#define C12880MA_VIDEO_PIN    34
#define LED_ILLUM_PIN         32  // LED Control on D32
#define SD_CS_PIN             5   // Standard SPI Chip Select for SD Card
#define SPECTROGRAPH_CHANNELS 288

// ============================================================
// GLOBAL VARIABLES & STATE
// ============================================================
uint16_t spectrographData[SPECTROGRAPH_CHANNELS];
uint16_t currentMaxIntensity = 0;
uint16_t currentMinIntensity = 4095;

// Auto-Exposure Control
unsigned long currentIntegrationTimeUs = 10000; 
const unsigned long minIntegrationTimeUs = 11000;   
const unsigned long maxIntegrationTimeUs = 1000000; 
const uint16_t targetPeakMin = 2500;
const uint16_t targetPeakMax = 3500;

unsigned long lastReadMillis = 0;

// System Controls
bool ledState = true;               // Master LED Enable (Strobe active when true)
bool autoExposureEnabled = true;    // Auto-exposure active by default
String userComment = "DEFAULT";     // Comment column string for SD logs
bool requestSaveToSD = false;       // Flag triggered via serial command

// Burst Buffer State (10 frames x 288 channels)
#define BURST_SIZE 10
uint16_t burstBuffer[BURST_SIZE][SPECTROGRAPH_CHANNELS];
unsigned long burstIntegrationTimes[BURST_SIZE];
uint16_t burstMaxIntensities[BURST_SIZE];
bool isBursting = false;
bool burstStabilizing = false;
int burstCount = 0;
int stabilizationFrames = 0;

// Interval Datalogging State
bool isIntervalLogging = false;
unsigned long logIntervalMs = 5000; // Default 5 seconds
unsigned long lastIntervalLogMillis = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("[INIT] Starting C12880MA Complete Spectrometer System..."));

  // Initialize Illumination LED Pin (default to LOW/Off)
  pinMode(LED_ILLUM_PIN, OUTPUT);
  digitalWrite(LED_ILLUM_PIN, LOW);

  // Initialize I2C and OLED
  Wire.begin(21, 22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[ERROR] SSD1306 allocation failed!"));
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Initialize Spectrometer Pins
  pinMode(C12880MA_CLK_PIN, OUTPUT);
  pinMode(C12880MA_ST_PIN, OUTPUT);
  digitalWrite(C12880MA_CLK_PIN, LOW);
  digitalWrite(C12880MA_ST_PIN, LOW);

  // Initialize SD Card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("[WARNING] SD Card Mount Failed. Logging disabled."));
  } else {
    Serial.println(F("[INFO] SD Card Initialized Successfully."));
  }

  printHelpMenu();
}

void loop() {
  // 1. Process Serial Commands from Serial Monitor / Tera Term
  handleSerialCommands();

  // Handle Burst Capture Mode & Stabilization Phase if active
  if (isBursting) {
    // Phase A: Auto-exposure pre-flight stabilization (if enabled)
    if (burstStabilizing) {
      readSpectrometer(); // LED strobes automatically inside here
      if (autoExposureEnabled) {
        updateAutoExposure();
      }
      stabilizationFrames++;

      // Show stabilizing status on OLED
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print(F("STABILIZING..."));
      display.setCursor(0, 20);
      display.print(F("Max Peak: "));
      display.print(currentMaxIntensity);
      display.display();

      // Check if exposure is stable within target window or max checks reached
      bool inTargetRange = (currentMaxIntensity >= targetPeakMin && currentMaxIntensity <= targetPeakMax);
      if (inTargetRange || stabilizationFrames >= 10 || !autoExposureEnabled) {
        burstStabilizing = false; // Stabilization complete, transition to actual burst
        burstCount = 0;
      }
      return;
    }
    
    // Phase B: Actual 10-frame RAM capture loop
    readSpectrometer();

    // Store in RAM buffer
    for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
      burstBuffer[burstCount][i] = spectrographData[i];
    }
    burstIntegrationTimes[burstCount] = currentIntegrationTimeUs;
    burstMaxIntensities[burstCount] = currentMaxIntensity;
    burstCount++;

    // Update OLED live with frame counter & scan number indication
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
      Serial.println(F("[SD] 10-frame Burst complete and appended to master file."));
    }
    return; // Skip normal loop processing during burst
  }

  // 1.5. Handle Periodic Interval Datalogging (if active)
  if (isIntervalLogging && !isBursting) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastIntervalLogMillis >= logIntervalMs) {
      lastIntervalLogMillis = currentMillis;
      
      saveSpectrumToSD();
      Serial.print(F("[LOG] Auto-saved interval frame at timestamp: "));
      Serial.println(currentMillis);
    }
  }

  // 2. Pace Readings & Auto-Exposure Updates
  unsigned long currentMillis = millis();
  unsigned long adaptiveInterval = (currentIntegrationTimeUs / 1000) + 100;
  if (adaptiveInterval < 100) adaptiveInterval = 100;

  if (currentMillis - lastReadMillis >= adaptiveInterval) {
    lastReadMillis = currentMillis;
    
    readSpectrometer(); // LED strobes automatically inside here
    
    if (autoExposureEnabled) {
      updateAutoExposure();
    }

    // Handle background SD save request if triggered
    if (requestSaveToSD) {
      saveSpectrumToSD();
      requestSaveToSD = false;
    }
  }

  // 3. Update OLED Display
  display.clearDisplay();
  
  display.setCursor(0, 0);
  display.print(F("Int:")); 
  if (currentIntegrationTimeUs >= 1000000) {
    display.print(currentIntegrationTimeUs / 1000000.0, 1);
    display.print(F("s"));
  } else {
    display.print(currentIntegrationTimeUs / 1000.0, 1); 
    display.print(F("ms"));
  }
  
  display.setCursor(72, 0);
  display.print(F("Max:")); 
  display.print(currentMaxIntensity);

  // Draw Spectrum Graph
  for (int x = 0; x < 128; x++) {
    int specIndex = map(x, 0, 127, 0, SPECTROGRAPH_CHANNELS - 1);
    
    int barHeight = 0;
    if (currentMaxIntensity > currentMinIntensity) {
      barHeight = map(spectrographData[specIndex], currentMinIntensity, currentMaxIntensity, 0, 50);
    }
    barHeight = constrain(barHeight, 0, 50);
    
    display.drawFastVLine(x, 63 - barHeight, barHeight, SSD1306_WHITE);
  }
  display.display();
}

void readSpectrometer() {
  // Strobe LED ON a short moment before integration (if master ledState is true)
  if (ledState) {
    digitalWrite(LED_ILLUM_PIN, HIGH);
    delay(2); // 2ms pre-stabilization window for LED intensity
  }

  const int halfClkDelay = 2; 

  analogSetPinAttenuation(C12880MA_VIDEO_PIN, ADC_11db);
  analogReadResolution(12);

  digitalWrite(C12880MA_CLK_PIN, LOW);
  digitalWrite(C12880MA_ST_PIN, LOW);
  delayMicroseconds(5);

  digitalWrite(C12880MA_ST_PIN, HIGH);
  delayMicroseconds(2);

  digitalWrite(C12880MA_CLK_PIN, HIGH);
  delayMicroseconds(halfClkDelay);
  digitalWrite(C12880MA_CLK_PIN, LOW);
  delayMicroseconds(halfClkDelay);

  digitalWrite(C12880MA_ST_PIN, LOW);

  for (int i = 0; i < 14; i++) {
    digitalWrite(C12880MA_CLK_PIN, HIGH);
    delayMicroseconds(halfClkDelay);
    digitalWrite(C12880MA_CLK_PIN, LOW);
    delayMicroseconds(halfClkDelay);
  }

  unsigned long remainingTime = currentIntegrationTimeUs;
  while (remainingTime > 16383) {
    delayMicroseconds(16383);
    remainingTime -= 16383;
  }
  if (remainingTime > 0) {
    delayMicroseconds(remainingTime);
  }

  digitalWrite(C12880MA_ST_PIN, HIGH);
  digitalWrite(C12880MA_CLK_PIN, HIGH);
  delayMicroseconds(halfClkDelay);
  digitalWrite(C12880MA_CLK_PIN, LOW);
  delayMicroseconds(halfClkDelay);
  digitalWrite(C12880MA_ST_PIN, LOW);

  currentMaxIntensity = 0;
  currentMinIntensity = 4095;
  
  for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
    digitalWrite(C12880MA_CLK_PIN, HIGH);
    delayMicroseconds(2); 

    spectrographData[i] = analogRead(C12880MA_VIDEO_PIN);

    if (spectrographData[i] > currentMaxIntensity) {
      currentMaxIntensity = spectrographData[i];
    }
    if (spectrographData[i] < currentMinIntensity) {
      currentMinIntensity = spectrographData[i];
    }

    digitalWrite(C12880MA_CLK_PIN, LOW);
    delayMicroseconds(halfClkDelay);
  }

  for (int i = 0; i < 8; i++) {
    digitalWrite(C12880MA_CLK_PIN, HIGH);
    delayMicroseconds(halfClkDelay);
    digitalWrite(C12880MA_CLK_PIN, LOW);
    delayMicroseconds(halfClkDelay);
  }

  // Strobe LED OFF immediately after measurement cycle completes
  digitalWrite(LED_ILLUM_PIN, LOW);
}

void updateAutoExposure() {
  if (currentMaxIntensity > targetPeakMax) {
    float scaleFactor = (float)targetPeakMax / (float)currentMaxIntensity;
    currentIntegrationTimeUs = (unsigned long)((float)currentIntegrationTimeUs * scaleFactor);
  } 
  else if (currentMaxIntensity < targetPeakMin && currentMaxIntensity > 400) {
    currentIntegrationTimeUs = (unsigned long)((float)currentIntegrationTimeUs * 1.15);
  }
  else if (currentMaxIntensity <= 400) {
    currentIntegrationTimeUs += 10000;
  }

  currentIntegrationTimeUs = constrain(currentIntegrationTimeUs, minIntegrationTimeUs, maxIntegrationTimeUs);
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    String upperInput = input;
    upperInput.toUpperCase();

    if (upperInput.equalsIgnoreCase("L")) {
      ledState = !ledState; 
      digitalWrite(LED_ILLUM_PIN, LOW); // Force off immediately if toggled
      Serial.print(F("[CMD] Illumination Strobe Feature -> "));
      Serial.println(ledState ? F("ENABLED") : F("DISABLED (Forces LED OFF)"));
    } 
    else if (upperInput.equalsIgnoreCase("A")) {
      autoExposureEnabled = !autoExposureEnabled; 
      Serial.print(F("[CMD] Auto-Exposure -> "));
      Serial.println(autoExposureEnabled ? F("ENABLED") : F("DISABLED"));
    } 
    else if (upperInput.equalsIgnoreCase("S")) {
      requestSaveToSD = true;
      Serial.println(F("[CMD] Save triggered for next frame..."));
    } 
    else if (upperInput.equalsIgnoreCase("B")) {
      isBursting = true;
      burstStabilizing = true;
      stabilizationFrames = 0;
      burstCount = 0;
      Serial.println(F("[CMD] Starting burst stabilization and capture..."));
    }
    else if (upperInput.startsWith("LOG")) {
      String arg = input.substring(3);
      arg.trim();

      if (arg.equalsIgnoreCase("OFF") || arg.equalsIgnoreCase("STOP")) {
        isIntervalLogging = false;
        Serial.println(F("[CMD] Periodic SD Datalogging -> STOPPED"));
      } 
      else {
        if (arg.length() > 0) {
          unsigned long newSec = arg.toInt();
          if (newSec > 0) {
            logIntervalMs = newSec * 1000;
          }
        }
        isIntervalLogging = true;
        lastIntervalLogMillis = millis();
        Serial.print(F("[CMD] Periodic SD Datalogging -> STARTED (Every "));
        Serial.print(logIntervalMs / 1000);
        Serial.println(F(" seconds)"));
      }
    }
    else if (upperInput.startsWith("COMMENT ")) {
      userComment = input.substring(8); 
      Serial.print(F("[CMD] Comment updated: "));
      Serial.println(userComment);
    } 
    else if (upperInput.startsWith("INT ")) {
      unsigned long newInt = input.substring(4).toInt();
      if (newInt > 0) {
        currentIntegrationTimeUs = constrain(newInt, minIntegrationTimeUs, maxIntegrationTimeUs);
        autoExposureEnabled = false; 
        Serial.print(F("[CMD] Integration set manually to (us): "));
        Serial.println(currentIntegrationTimeUs);
      }
    } 
    else if (upperInput.equalsIgnoreCase("HELP")) {
      printHelpMenu();
    }
  }
}

void saveSpectrumToSD() {
  bool fileExists = SD.exists("/spectra_log.csv");

  File dataFile = SD.open("/spectra_log.csv", FILE_APPEND);
  if (dataFile) {
    if (!fileExists) {
      dataFile.print("Timestamp_ms,IntegrationTime_us,AutoExposure,LED_State,Comment,Max_ADC");
      for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
        dataFile.print(",P");
        dataFile.print(i);
      }
      dataFile.println();
    }

    dataFile.print(millis());
    dataFile.print(',');
    dataFile.print(currentIntegrationTimeUs);
    dataFile.print(',');
    dataFile.print(autoExposureEnabled ? "ON" : "OFF");
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
    Serial.println(F("[SD] Spectrum successfully appended to /spectra_log.csv"));
  } else {
    Serial.println(F("[ERROR] Failed to open spectra_log.csv on SD card"));
  }
}

void writeBurstToSD() {
  bool fileExists = SD.exists("/spectra_log.csv");

  File dataFile = SD.open("/spectra_log.csv", FILE_APPEND);
  if (dataFile) {
    if (!fileExists) {
      dataFile.print("Timestamp_ms,IntegrationTime_us,AutoExposure,LED_State,Comment,Max_ADC");
      for (int i = 0; i < SPECTROGRAPH_CHANNELS; i++) {
        dataFile.print(",P");
        dataFile.print(i);
      }
      dataFile.println();
    }

    for (int b = 0; b < BURST_SIZE; b++) {
      dataFile.print(millis());
      dataFile.print(',');
      dataFile.print(burstIntegrationTimes[b]);
      dataFile.print(',');
      dataFile.print(autoExposureEnabled ? "ON" : "OFF");
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
    Serial.println(F("[SD] 10-frame burst successfully appended to /spectra_log.csv"));
  } else {
    Serial.println(F("[ERROR] Failed to open /spectra_log.csv on SD card"));
  }
}

void printHelpMenu() {
  Serial.println(F("\n--- C12880MA Terminal Control Menu ---"));
  Serial.println(F("L             - Toggle LED Strobe feature ON/OFF"));
  Serial.println(F("A             - Toggle auto-exposure ON/OFF"));
  Serial.println(F("S             - Save current spectrum frame to SD card"));
  Serial.println(F("B             - Burst capture 10 frames to RAM and append to CSV"));
  Serial.println(F("LOG <sec>     - Start periodic logging every N seconds (e.g., LOG 900 for 15m)"));
  Serial.println(F("LOG OFF       - Stop periodic datalogging"));
  Serial.println(F("COMMENT <text>- Set current comment string for SD logs"));
  Serial.println(F("INT <us>      - Set integration time in microseconds & disable auto"));
  Serial.println(F("HELP          - Print this menu"));
  Serial.println(F("--------------------------------------\n")); 
}