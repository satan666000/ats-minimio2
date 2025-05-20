// =================================
// INCLUDES Y CONFIGURACIÓN
// =================================
#include <Wire.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>
#include <SI4735.h>
#include "Rotary.h"
#include "Button.h"

// Configuración del Waterfall
#define WATERFALL_HEIGHT 50       // Altura en píxeles
#define WATERFALL_HISTORY 20      // Líneas de historial
#define WF_UPDATE_INTERVAL 100    // ms entre actualizaciones
#define WF_COLOR_LEVELS 5         // Niveles de color

// Configuración de radio
#define DEFAULT_VOLUME 35
#define MIN_ELAPSED_RSSI_TIME 200
#define RDS_CHECK_TIME 250

// Pines
#define ENCODER_PIN_A 12
#define ENCODER_PIN_B 14
#define ENCODER_PUSH_BUTTON 13
#define PIN_LCD_BL 27
#define RESET_PIN 33

// =================================
// ESTRUCTURAS Y VARIABLES
// =================================
typedef struct {
  uint16_t minFreq;
  uint16_t maxFreq;
  uint16_t currentFreq;
  uint8_t bandMode;
} Band;

Band bands[] = {
  {8750, 10800, 9470, 0},   // FM
  {520, 1710, 810, 1},       // AM
  {3500, 4000, 3700, 2}      // SSB
};

uint8_t currentBand = 0;
uint16_t currentFrequency = bands[currentBand].currentFreq;
uint8_t currentMode = bands[currentBand].bandMode;
int16_t currentBFO = 0;
uint8_t volume = DEFAULT_VOLUME;

// Waterfall
uint16_t waterfall[WATERFALL_HISTORY][TFT_WIDTH];
uint8_t wfIndex = 0;
uint32_t lastWfUpdate = 0;
bool showWaterfall = true;
const uint16_t wfColors[WF_COLOR_LEVELS] = {
  TFT_BLUE, TFT_CYAN, TFT_GREEN, TFT_YELLOW, TFT_RED
};

// Objetos globales
TFT_eSPI tft = TFT_eSPI();
SI4735 radio;
Rotary encoder(ENCODER_PIN_A, ENCODER_PIN_B);

// =================================
// FUNCIONES DEL WATERFALL
// =================================
void initWaterfall() {
  for(uint8_t y = 0; y < WATERFALL_HISTORY; y++) {
    for(uint16_t x = 0; x < TFT_WIDTH; x++) {
      waterfall[y][x] = TFT_BLACK;
    }
  }
}

void updateWaterfall() {
  if(!showWaterfall || (millis() - lastWfUpdate) < WF_UPDATE_INTERVAL) return;
  
  lastWfUpdate = millis();

  // Datos simulados del espectro (reemplazar con radio.getSpectrum() si está disponible)
  uint8_t spectrum[TFT_WIDTH];
  for(uint16_t i = 0; i < TFT_WIDTH; i++) {
    spectrum[i] = random(0, 100);
  }

  // Actualizar buffer
  for(uint16_t x = 0; x < TFT_WIDTH; x++) {
    uint8_t level = map(spectrum[x], 0, 100, 0, WF_COLOR_LEVELS-1);
    waterfall[wfIndex][x] = wfColors[level];
  }

  wfIndex = (wfIndex + 1) % WATERFALL_HISTORY;
}

void drawWaterfall() {
  if(!showWaterfall) return;

  uint16_t yPos = tft.height() - WATERFALL_HEIGHT;
  
  for(uint8_t y = 0; y < WATERFALL_HEIGHT; y++) {
    uint8_t histIdx = (wfIndex + y) % WATERFALL_HISTORY;
    for(uint16_t x = 0; x < TFT_WIDTH; x++) {
      tft.drawPixel(x, yPos + y, waterfall[histIdx][x]);
    }
  }
}

// =================================
// FUNCIONES PRINCIPALES
// =================================
void setup() {
  Serial.begin(115200);
  
  // Inicializar pantalla
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  
  // Inicializar encoder
  pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), []{ encoder.process(); }, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), []{ encoder.process(); }, CHANGE);

  // Inicializar radio
  Wire.begin();
  radio.setup(RESET_PIN, currentMode);
  radio.setVolume(volume);
  
  // Inicializar waterfall
  initWaterfall();
  
  // Mostrar pantalla inicial
  drawMainDisplay();
}

void loop() {
  static uint32_t lastRssiCheck = 0;
  
  // Manejar encoder
  handleEncoder();
  
  // Actualizar RSSI periódicamente
  if(millis() - lastRssiCheck > MIN_ELAPSED_RSSI_TIME) {
    updateSignalInfo();
    lastRssiCheck = millis();
  }
  
  // Actualizar waterfall
  updateWaterfall();
  
  // Redibujar si es necesario
  static uint32_t lastDraw = 0;
  if(millis() - lastDraw > 100) {
    drawMainDisplay();
    lastDraw = millis();
  }
  
  delay(5);
}

// =================================
// FUNCIONES DE RADIO
// =================================
void updateFrequency(int16_t increment) {
  Band* b = &bands[currentBand];
  currentFrequency = constrain(currentFrequency + increment, b->minFreq, b->maxFreq);
  radio.setFrequency(currentFrequency);
  b->currentFreq = currentFrequency;
}

void updateSignalInfo() {
  radio.getCurrentReceivedSignalQuality();
  // Actualizar indicadores de señal...
}

// =================================
// INTERFAZ GRÁFICA
// =================================
void drawMainDisplay() {
  tft.fillScreen(TFT_BLACK);
  
  // Dibujar información principal
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Freq: ");
  tft.print(currentFrequency / 100.0, 2);
  tft.print(" MHz");
  
  // Dibujar waterfall
  drawWaterfall();
}

// =================================
// MANEJADOR DE ENCODER
// =================================
void handleEncoder() {
  static int8_t lastPos = 0;
  int8_t newPos = encoder.getPosition();
  
  if(newPos != lastPos) {
    updateFrequency((newPos - lastPos) * getStepSize());
    lastPos = newPos;
  }
  
  // Manejar botón del encoder...
}
