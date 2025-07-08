#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

// ==== OLED ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==== Rotary Encoder ====
#define ENCODER_CLK 18
#define ENCODER_DT  19
#define ENCODER_SW  23
int lastClkState;
bool buttonPressed = false;

// ==== SD Card ====
#define SD_CS   5
#define SD_MOSI 13
#define SD_MISO 12
#define SD_SCK  14
SPIClass sdSPI(VSPI);

// ==== I2S DAC ====
#define I2S_BCLK  4
#define I2S_LRC   33
#define I2S_DIN   2

AudioGeneratorWAV *wav;
AudioFileSourceSD *file;
AudioOutputI2S *out;

// ==== File List ====
#define MAX_FILES 20
String fileList[MAX_FILES];
int fileCount = 0;
int currentIndex = 0;

// ==== Setup ====
void setup() {
  Serial.begin(115200);

  // Rotary
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastClkState = digitalRead(ENCODER_CLK);

  // OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // SD
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    displayError("SD failed!");
    return;
  }

  // I2S DAC
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
  out->SetGain(0.9);

  listWavFiles();
  drawMenu();
}

void loop() {
  handleEncoder();
  handleButton();
  if (wav && wav->isRunning()) {
    wav->loop();
  }
}

// ==== Display Helpers ====
void drawMenu() {
  display.clearDisplay();
  for (int i = 0; i < fileCount; i++) {
    if (i == currentIndex) display.print("> ");
    else display.print("  ");
    display.println(fileList[i]);
  }
  display.display();
}

void displayError(String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

// ==== Encoder ====
void handleEncoder() {
  int clkState = digitalRead(ENCODER_CLK);
  int dtState = digitalRead(ENCODER_DT);

  if (clkState != lastClkState && clkState == LOW) {
    if (dtState == HIGH) {
      currentIndex++;
    } else {
      currentIndex--;
    }

    if (currentIndex < 0) currentIndex = fileCount - 1;
    if (currentIndex >= fileCount) currentIndex = 0;

    drawMenu();
  }

  lastClkState = clkState;
}

void handleButton() {
  if (digitalRead(ENCODER_SW) == LOW && !buttonPressed) {
    buttonPressed = true;
    playSelectedFile();
  } else if (digitalRead(ENCODER_SW) == HIGH) {
    buttonPressed = false;
  }
}

// ==== Play Sound ====
void playSelectedFile() {
  String filename = "/" + fileList[currentIndex];
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Playing:");
  display.println(filename);
  display.display();

  if (wav && wav->isRunning()) {
    wav->stop();
  }

  file = new AudioFileSourceSD(filename.c_str());
  wav = new AudioGeneratorWAV();
  if (!wav->begin(file, out)) {
    displayError("Playback error");
  }
}

// ==== File Scanner ====
void listWavFiles() {
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry || fileCount >= MAX_FILES) break;

    String name = entry.name();
    name.toLowerCase();
    if (name.endsWith(".wav")) {
      fileList[fileCount++] = String(entry.name());
    }
    entry.close();
  }

  if (fileCount == 0) {
    fileList[0] = "No .wav files!";
    fileCount = 1;
  }
}
