// V1.0 - 06/2025 - Meteovision by Davide Gatti - 2025 - www.survivalhacking.it
// V1.1 - 09/2025 - L'inserimento della città manualemnte via WEB non funzionava
// V1.2 - 09/2025 - Aggiunta la possibilità di vedere la previsione del giorno corrente / del giorno dopo e del giorno dopo ancora, mediante menù di configurazione WEB
// V1.3 - 10/2025 - Non erano gestite diverse tipologie di meteo tipo Neve da 600 a 699 e nebbia fumo e altre condizioni particolari da 700 a 799
// V1.4 - 10/2025 - Aggiunta scritta in mezzo alle due icone per evidenziare il giorno relativo alle previsioni
// V1.5 - 10/2025 - Aggiunta Modalità Notturna con orario impostabile da interfaccia web, Indicazione intensità e direzione vento, fix meteo led che non si aggiornano se in modalità diversa auto
// V1.6 - 10/2025 - Bugfix salvataggio API key rivisto salvataggio di tutte le variabili più robusto gestione città con spazi e nomi lunghi a display
// V1.7 - 11/2025 - aggiunta la possibilita di previsioni 5 giorni e interfaccia web by Marco Prunca
// V1.8 - 11/2025 - FIX: Aggiornamento meteo ogni 10 minuti funzionante per tutte le modalità
// V1.81- 12/2025 - Aggiunto swithc per disabilitare l'effetto direzione e velocità del vento sull'anello.
// V1.9 - 12/2025 - Aggiunta animazione anello NeoPixel al cambio modalità e toggle di abilitazione nell'interfaccia web
// V1.91- 12/2025 - Sistemato bug che facema smettere lèeffetto respiro dell'anello
// V1.92- 12/2025 - Aggiunta la possibilità di configurare la connessione WIFI in modo manuale. Tutte le istruzioni per la configurazioni sono nel file wifi_config.h by Gigi Soft
//                            

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>       // Libreria grafica per display OLED
#include "Adafruit_SSD1306.h"   // Libreria per SSD1306 modificata per il supporto mirroring
#include <Adafruit_NeoPixel.h>  // Controllo anello LED WS2812

// Gestione connessione WiFi automatica
#include <WiFiManager.h>  // Permette setup Wi-Fi via interfaccia web

// Richieste HTTP per ottenere i dati meteo
#include <HTTPClient.h>
#include <ArduinoJson.h>  // Parsing JSON della risposta OpenWeather

#include "graphics.h"  // Contiene animazioni bitmap per display

// -- LIBRERIE PER GESTIONE TEMPO --
// Tempo standard C
#include <ctime>       // Include strutture come time_t, struct tm
#include <time.h>      // Funzioni di conversione ora/data
#include <sys/time.h>  // Per strutture temporali compatibili con ESP32

// Librerie NTP (Network Time Protocol)
#include <NTPClient.h>  // Permette sincronizzazione ora via Internet
#include <WiFiUdp.h>    // Necessario per comunicazione UDP usata da NTPClient

// Librerie custom per server web
#include <WebServer.h>    // Server HTTP in esecuzione su ESP32
#include <Preferences.h>  // Per salvare configurazioni persistenti

#include "secret.h"       // File con chiavi/API sensibili
#include "wifi_config.h"  // Configurazione WiFi manuale

// -- DEFINIZIONI HARDWARE DISPLAY OLED --
#define OLED_WIDTH 128
#define OLED_HEIGHT 32  // MODIFICATO: da 64 a 32 pixel
#define OLED_ADDR 0x3C  // Indirizzo I2C predefinito per display SSD1306
#define OLED_RESET -1   // Se il pin RESET non è collegato

// Istanzia il display OLED
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// -- DEFINIZIONI LED METEO INDIVIDUALI --
#define PIN_LED_SOLE 2       // LED per stato soleggiato
#define PIN_LED_NUVOLE 3     // LED per nuvole
#define PIN_LED_PIOGGIA 4    // LED per pioggia
#define PIN_LED_TEMPORALE 5  // LED per temporale
#define PIN_LED_LUNA 6       // LED per condizione notturna

// -- DEFINIZIONI PER ANELLO NEOPIXEL --
#define PIN_NEOPIXEL 7
#define NUM_NEOPIXELS 24  // Numero LED sull'anello
Adafruit_NeoPixel pixels(NUM_NEOPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// -- DEFINIZIONE PULSANTE FISICO --
#define PIN_PULSANTE 10  // Pulsante per cambiare modalità/luminosità

// -- DEFINIZIONI PER TEMPISMO DI SCHERMATA E METEO --
#define SCREEN_TEXT_DURATION 8000              // MODIFICATO: Ridotto per display più piccolo
#define SCREEN_ANIMATION_DURATION 5000         // MODIFICATO: Ridotto per display più piccolo
#define WEATHER_FETCH_INTERVAL 10 * 60 * 1000  // Ogni 10 minuti (in ms)

// Parametri effetto "respiro" LED (NeoPixel)
#define NEOPIXEL_BREATH_MIN 25       // Luminosità minima effetto respiro
#define NEOPIXEL_BREATH_MAX 255      // Luminosità massima
#define BREATH_UPDATE_SPEED 5        // Velocità animazione
#define NEOPIXEL_BOOT_ANIM_DELAY 50  // Delay tra accensioni LED iniziali

// Variabili per la gestione del pulsante
unsigned long buttonPressStartTime = 0;
bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
bool keypressed = false;

// Variabili per pressione lunga del pulsante
bool isLongPressMode = false;
const unsigned long LONG_PRESS_THRESHOLD = 1000;
const int BRIGHTNESS_CHANGE_INTERVAL = 100;
unsigned long lastBrightnessChangeTime = 0;

unsigned long lastScreenChange = 0;
bool showText = true;

// -- VARIABILI DI CONFIGURAZIONE PERSISTENTE --
Preferences preferences;

// Valori persistenti da conservare
char savedCity[40] = "Monza";
char savedCountryCode[5] = "IT";
int savedNeopixelBrightness = 200;
int savedMode = 0;
bool DisplayOneTime = false;
int Currentmode = 0;
int forecastDay = -1; // -1: Attuale (sempre all'accensione)
int forecastPeriod = 0; // 0: Mattino, 1: Pomeriggio, 2: Sera
#define MAX_MODE 11
bool nightModeEnabled = false;
int nightStartHour = 22;
int nightStartMinute = 00;
int nightEndHour = 7;
int nightEndMinute = 00;
bool windAnimationEnabled = true;
bool neopixelAnimationEnabled = true; // Nuova variabile per animazione cambio modalità

// Variabili per la visualizzazione del nome della modalità
bool showModeName = false;
unsigned long modeNameStartTime = 0;
const unsigned long MODE_NAME_DISPLAY_DURATION = 2000; // MODIFICATO: da 1 a 2 secondi

// Definizione delle modalità disponibili
#define MODO_AUTO 0
#define MODO_ROSSO 1
#define MODO_VERDE 2
#define MODO_BLU 3
#define MODO_VIOLA 4
#define MODO_CIANO 5
#define MODO_GIALLO 6
#define MODO_BIANCO 7
#define MODO_RING_OFF 8
#define MODO_LED_OFF 9
#define MODO_ALL_OFF 10
#define MODO_NOTTE 11

// -- VARIABILI GLOBALI NON SALVATE IN MEMORIA --
String ProductName = "MeteoVision V1.92";
String weatherDescription = "Ricerca...";
float windSpeed = 0;
int windDirection = 0;
int weatherConditionCode = 0;
float temperatureCelsius = 0.0;
int humidity = 0;
time_t sunriseTime = 0;
time_t sunsetTime = 0;
int animationFrame = 0;
const unsigned long WIND_SCAN_DURATION = 1000;
bool windScanning = false;
bool windAnimationActive = false;
int windScanFrame = 0;
unsigned long windAnimationStartTime = 0;
unsigned long animationStartTime = 0;
int windLedIndex = 0;

// Flag per forzare l'aggiornamento immediato del meteo
bool forceWeatherUpdate = false;

// Timer per l'aggiornamento periodico del meteo
unsigned long lastWeatherFetch = 0;

// -- GESTIONE RETE E SERVER WEB --
WiFiManager wm;
WebServer server(80);

// -- CLIENT NTP PER L'ORARIO DI SISTEMA --
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// -- VARIABILI PER ANIMAZIONE DELL'ANELLO NEOPIXEL --
bool neopixelAnimationActive = true;
uint32_t neopixelBaseColor = pixels.Color(0, 0, 0);
bool neopixelBootAnimationInProgress = false;
bool neopixelModeChangeAnimation = false; // Nuova variabile per animazione cambio modalità
int neopixelAnimStep = 0;
unsigned long neopixelAnimLastUpdate = 0;
const int NEOPIXEL_ANIM_STEPS = NUM_NEOPIXELS + 5; // Numero di passi dell'animazione (LED + 5 extra per completamento)
const unsigned long NEOPIXEL_ANIM_DELAY = 40; // Delay tra i passi (più veloce per effetto fluido)

// Parametro personalizzato globale per WiFiManager
WiFiManagerParameter custom_api_key("api_key", "OpenWeatherMap API Key", "", 60);

// --- Connessione WiFi Manuale ---
bool connectWiFiManual() {
  Serial.println("\n==================================");
  Serial.println("TENTATIVO CONNESSIONE WIFI MANUALE");
  Serial.println("==================================");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Timeout: ");
  Serial.print(WIFI_CONNECT_TIMEOUT / 1000);
  Serial.println(" secondi");
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("WiFi Manuale...");
  display.setCursor(0, 10);
  display.println("SSID:");
  display.setCursor(0, 18);
  display.println(WIFI_SSID);
  display.display();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startAttempt = millis();
  int dotCount = 0;
  
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
    
    dotCount = (dotCount + 1) % 4;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Connessione");
    display.setCursor(0, 10);
    for(int i = 0; i < dotCount; i++) {
      display.print(".");
    }
    display.setCursor(0, 20);
    display.print("Tempo: ");
    display.print((millis() - startAttempt) / 1000);
    display.print("s");
    display.display();
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ CONNESSIONE RIUSCITA!");
    Serial.print("Indirizzo IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("WiFi CONNESSO!");
    display.setCursor(0, 10);
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.setCursor(0, 20);
    display.print("RSSI: ");
    display.print(WiFi.RSSI());
    display.println("dBm");
    display.display();
    delay(3000);
    
    return true;
  } else {
    Serial.println("✗ CONNESSIONE FALLITA!");
    Serial.print("Stato WiFi: ");
    
    switch(WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        Serial.println("SSID non trovato");
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 8);
        display.println("ERRORE WiFi:");
        display.setCursor(0, 16);
        display.println("SSID non trovato");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("Password errata");
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 8);
        display.println("ERRORE WiFi:");
        display.setCursor(0, 16);
        display.println("Password errata");
        break;
      default:
        Serial.println("Errore sconosciuto");
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 8);
        display.println("ERRORE WiFi:");
        display.setCursor(0, 16);
        display.println("Timeout");
        break;
    }
    display.display();
    delay(3000);
    
    return false;
  }
}


// --- Mappatura traduzioni OpenWeatherMap (da codice numerico a descrizione ITA) ---
String getWeatherDescriptionItalian(int conditionCode) {
  if (conditionCode >= 200 && conditionCode < 300) return "Temporale";
  if (conditionCode >= 300 && conditionCode < 400) return "Pioggerella";
  if (conditionCode >= 500 && conditionCode < 600) {
    if (conditionCode == 500) return "Pioggia leggera";
    if (conditionCode == 501) return "Pioggia moderata";
    if (conditionCode == 502) return "Pioggia intensa";
    if (conditionCode == 503) return "Pioggia molto intensa";
    if (conditionCode == 504) return "Pioggia estrema";
    if (conditionCode == 511) return "Pioggia gelata";
    if (conditionCode == 520) return "Pioggerella intensa";
    if (conditionCode == 521) return "Acquazzone";
    if (conditionCode == 522) return "Forti acquazzoni";
    if (conditionCode == 531) return "Acquazzone irregolare";
    return "Pioggia";
  }
  if (conditionCode >= 600 && conditionCode < 700) return "Neve";
  if (conditionCode >= 700 && conditionCode < 800) {
    if (conditionCode == 701) return "Nebbia";
    if (conditionCode == 711) return "Fumo";
    if (conditionCode == 721) return "Foschia";
    if (conditionCode == 731) return "Vortici di sabbia/polvere";
    if (conditionCode == 741) return "Nebbia";
    if (conditionCode == 751) return "Sabbia";
    if (conditionCode == 761) return "Polvere";
    if (conditionCode == 762) return "Cenere vulcanica";
    if (conditionCode == 771) return "Colpo di vento";
    if (conditionCode == 781) return "Tornado";
    return "Atmosferico";
  }
  if (conditionCode == 800) return "Cielo sereno";
  if (conditionCode == 801) return "Poche nuvole";
  if (conditionCode == 802) return "Nuvole sparse";
  if (conditionCode == 803) return "Nuvole irregolari";
  if (conditionCode == 804) return "Cielo coperto";

  return "Sconosciuto";
}

// --- PROTOTIPI DI FUNZIONI ---
bool connectWiFiManual();
void saveConfig();
void loadConfig();
void saveConfigCallback();
void configModeCallback(WiFiManager *myWiFiManager);
void fetchWeather();
void fetchCurrentWeather();
void fetchForecastWeather();
void displayWeatherInfo();
void displayWeatherAnimation();
void updateWeatherLEDs();
void runNeopixelAnimation();
void setNeopixelColor(uint32_t color);
void animateNeopixelCircular();
void handleButtonPress();
void onGetCommand(unsigned char device_id, const char *device_name, bool state, unsigned char value);
time_t getNtpTime();
void AllLEDoff();
void SetRingColor(int mode, bool animate = false); // Modificato per includere parametro animazione
int getBeaufortScale(float windSpeed);
void getWindColorByBeaufort(int beaufort, uint8_t &r, uint8_t &g, uint8_t &b);
int getTrailLengthByBeaufort(int beaufort);
void runWindAnimation();
void runWindDirectionScan(int windLedIndex);
void checkNightMode();
String urlEncode(const String& str);
int getTextWidth(const String &text, int textSize);
void drawBrightnessBar(uint8_t brightness);
void bootAnimation();
void runNeopixelModeChangeAnimation(); // Nuova funzione per animazione cambio modalità

// Gestori HTTP per il server web integrato
void handleRoot();
void handleSave();

// Stato per l'animazione display
unsigned long lastFrameChange = 0;
int currentFrame = 0;
bool dualAnimation = false;
static int animationDirection = 1;
const int ANIM_FRAME_DELAY = 200;

// Wrapper semplificato per ottenere l'epoch time dal client NTP
time_t getNtpTime() {
  return timeClient.getEpochTime();
}

// --- Funzione per animazione di accensione ---
void bootAnimation() {
  Serial.println("Avvio animazione di accensione...");
  
  // Array con tutti i pin LED individuali
  int ledPins[] = {PIN_LED_SOLE, PIN_LED_NUVOLE, PIN_LED_PIOGGIA, PIN_LED_TEMPORALE, PIN_LED_LUNA};
  int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);
  
  // Sequenza di accensione dei LED individuali
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(300);
    digitalWrite(ledPins[i], LOW);
  }
  
  // Accensione completa di tutti i LED individuali
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  delay(500);
  
  // Spegnimento completo dei LED individuali
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  delay(300);
  
  // Animazione anello NeoPixel - accensione sequenziale
  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    pixels.show();
    delay(50);
  }
  delay(500);
  
  // Animazione anello NeoPixel - spegnimento sequenziale
  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    pixels.show();
    delay(50);
  }
  delay(300);
  
  // Accensione completa anello NeoPixel
  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  }
  pixels.show();
  delay(500);
  
  // Accensione completa di tutti i LED individuali
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  delay(1000);
  
  // Spegnimento completo di tutto
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  pixels.clear();
  pixels.show();
    delay(100);  // Breve attesa prima di mostrare il logo
  display.clearDisplay();
  display.drawBitmap((128 - logo_width) / 2, (32 - logo_height) / 2, logo_data, logo_width, logo_height, 1);
  display.display();  // Visualizza il logo "Survival Hacking" all'avvio
  Serial.println("Animazione di accensione completata");
}

// --- Funzione per animazione cambio modalità NeoPixel (ONDA FLUIDA) ---
void runNeopixelModeChangeAnimation() {
  if (neopixelModeChangeAnimation) {
    unsigned long currentMillis = millis();
    
    if (currentMillis - neopixelAnimLastUpdate >= NEOPIXEL_ANIM_DELAY) {
      neopixelAnimLastUpdate = currentMillis;
      
      // Calcola il colore di destinazione in base alla modalità
      uint32_t targetColor = neopixelBaseColor;
      
      // Esegui i diversi passi dell'animazione a onda fluida
      switch (neopixelAnimStep) {
        case 0:
          // Primo passo: spegni tutti i LED
          for (int i = 0; i < NUM_NEOPIXELS; i++) {
            pixels.setPixelColor(i, pixels.Color(0, 0, 0));
          }
          pixels.show();
          break;
          
        case 1:
          // Secondo passo: accendi il primo LED
          pixels.setPixelColor(0, targetColor);
          pixels.show();
          break;
          
        default:
          // Passi successivi: accendi progressivamente i LED successivi
          if (neopixelAnimStep - 1 < NUM_NEOPIXELS) {
            // Accendi il LED corrente
            pixels.setPixelColor(neopixelAnimStep - 1, targetColor);
            
            // Mantieni accesi anche i LED precedentes
            for (int i = 0; i < neopixelAnimStep - 1; i++) {
              pixels.setPixelColor(i, targetColor);
            }
            
            pixels.show();
          }
          break;
      }
      
      neopixelAnimStep++;
      
      // Se abbiamo completato l'animazione di tutti i LED + alcuni passi extra per stabilizzare
      if (neopixelAnimStep >= NEOPIXEL_ANIM_STEPS) {
        // Assicurati che tutti i LED siano accesi con il colore finale
        for (int i = 0; i < NUM_NEOPIXELS; i++) {
          pixels.setPixelColor(i, targetColor);
        }
        pixels.show();
        
        // Termina l'animazione
        neopixelModeChangeAnimation = false;
        Serial.println("Animazione cambio modalità completata");
      }
    }
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);

  loadConfig();

  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  delay(100);
  display.clearDisplay();
  
  // RIMOSSO: Schermata "MeteoVision by Davide Gatti"
  // Mostriamo direttamente il logo di Survival Hacking
  display.clearDisplay();
  display.drawBitmap((128 - logo_width) / 2, (32 - logo_height) / 2, logo_data, logo_width, logo_height, 1);
  display.display();
  delay(3000);

  pinMode(PIN_LED_SOLE, OUTPUT);
  pinMode(PIN_LED_NUVOLE, OUTPUT);
  pinMode(PIN_LED_PIOGGIA, OUTPUT);
  pinMode(PIN_LED_TEMPORALE, OUTPUT);
  pinMode(PIN_LED_LUNA, OUTPUT);
  AllLEDoff();

  pixels.begin();
  pixels.setBrightness(savedNeopixelBrightness);
  pixels.show();

  // Esegui l'animazione di accensione
  bootAnimation();

  pinMode(PIN_PULSANTE, INPUT_PULLUP);

  if (digitalRead(PIN_PULSANTE) == LOW) {
    Serial.println("Reset WiFi e configurazione.");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 12);
    display.println("Reset WiFi e Config...");
    display.display();
    delay(1000);

    preferences.begin("meteo-config", false);
    preferences.clear();
    preferences.end();
    wm.resetSettings();
    ESP.restart();
  }

  // ================
  // CONNESSIONE WIFI
  // ================

  bool wifiConnected = false;

  #if USE_MANUAL_WIFI
    Serial.println("Modalità: WIFI MANUALE");
    Serial.println("Configurazione da wifi_config.h");
    
    for(int attempt = 1; attempt <= WIFI_RETRY_COUNT && !wifiConnected; attempt++) {
      Serial.print("\nTentativo ");
      Serial.print(attempt);
      Serial.print(" di ");
      Serial.println(WIFI_RETRY_COUNT);
      
      wifiConnected = connectWiFiManual();
      
      if (!wifiConnected && attempt < WIFI_RETRY_COUNT) {
        Serial.println("Attesa 5 secondi prima del prossimo tentativo...");
        delay(5000);
      }
    }
    
    if (!wifiConnected) {
      Serial.println("\n⚠️ TUTTI I TENTATIVI MANUALI FALLITI!");
      Serial.println("Passaggio a WiFiManager...");
      
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("WiFi Manuale");
      display.println("FALLITO!");
      display.println("");
      display.println("Avvio portale");
      display.println("configurazione...");
      display.display();
      delay(3000);
      
      wm.setAPCallback(configModeCallback);
      wm.setSaveConfigCallback(saveConfigCallback);
      
      String savedApiKey = preferences.getString("api_key", "");
      if (savedApiKey.length() > 0) {
        custom_api_key.setValue(savedApiKey.c_str(), 60);
      }
      wm.addParameter(&custom_api_key);
      
      if (!wm.autoConnect("MeteoVision", "password")) {
        Serial.println("Fallita connessione WiFiManager, riavvio...");
        delay(3000);
        ESP.restart();
      }
      wifiConnected = true;
    }
  #else
    Serial.println("Modalità: WIFIMANAGER");
    Serial.println("Portale di configurazione attivo");
    
    wm.setAPCallback(configModeCallback);
    wm.setSaveConfigCallback(saveConfigCallback);

    String savedApiKey = preferences.getString("api_key", "");
    if (savedApiKey.length() > 0) {
      Serial.println("API key trovata nelle preferences: " + savedApiKey);
      custom_api_key.setValue(savedApiKey.c_str(), 60);
    } else {
      Serial.println("Nessuna API key salvata trovata, campo vuoto.");
    }
    wm.addParameter(&custom_api_key);

    Serial.println("\nConnessione Wi-Fi...");
    if (!wm.autoConnect("MeteoVision", "password")) {
      Serial.println("Fallita connessione, riavvio...");
      delay(3000);
      ESP.restart();
    }
    wifiConnected = true;
  #endif

  if (wifiConnected) {
    Serial.println("\n===========================");
    Serial.println("WIFI CONNESSO CON SUCCESSO!");
    Serial.println("===========================");
    Serial.print("Indirizzo IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
    Serial.println("========================================\n");


    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 8);
    display.println("         BY");
    display.setCursor(0, 16);
    display.println("     DAVIDE GATTI");
    display.setCursor(0, 24);
    display.println("   Survival Hacking");
    display.display();
    delay(3000);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 8);
    display.println("Connesso al WiFi!");
    display.setCursor(0, 16);
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.setCursor(0, 24);
    display.println("Accedi al webserver.");
    display.display();
    delay(5000);
    display.clearDisplay();

    Serial.print("Citta' configurata: ");
    Serial.println(savedCity);
    Serial.print("API Key usata: ");
    Serial.println(preferences.getString("api_key", "Errore"));
    Serial.print("Codice Paese (fisso IT): ");
    Serial.println(savedCountryCode);

    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    timeClient.begin();
    timeClient.update();

    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();

    // Verifica se esiste una API key valida prima di fare il primo fetch
    preferences.begin("meteo-config", false);
    String testApiKey = preferences.getString("api_key", "");
    preferences.end();

    if (testApiKey.length() > 0) {
      Serial.println("API key trovata, recupero meteo...");
      fetchWeather();
      lastWeatherFetch = millis(); // Inizializza il timer dopo il primo fetch
    } else {
      Serial.println("⚠️ ATTENZIONE: Nessuna API key configurata!");
      Serial.println("Configura l'API key tramite il portale web all'indirizzo:");
      Serial.println(WiFi.localIP());
      
      // Mostra messaggio sul display
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("ATTENZIONE!");
      display.setCursor(0, 10);
      display.println("Nessuna API key");
      display.setCursor(0, 18);
      display.println("Vai su:");
      display.setCursor(0, 26);
      display.println(WiFi.localIP());
      display.display();
      delay(4000);
      // Imposta valori di default per evitare errori
      weatherDescription = "API Key mancante";
      temperatureCelsius = 0.0;
      humidity = 0;
      windSpeed = 0.0;
      windDirection = 0;
      weatherConditionCode = 800;
    }

    SetRingColor(Currentmode, true); // MODIFICATO: Con animazione all'avvio
    lastScreenChange = millis();
  }
}

// --- Loop principale con gestione centralizzata dell'aggiornamento meteo ---
void loop() {
  #if !USE_MANUAL_WIFI
    wm.process();
  #endif

  server.handleClient();
  handleButtonPress();

  unsigned long currentTime = millis();

  // --- GESTIONE CENTRALIZZATA DELL'AGGIORNAMENTO METEO ---
  // SPOSTATO QUI IN CIMA: viene eseguito SEMPRE, anche durante regolazione luminosità o cambio modalità
  // Controlla se è necessario un aggiornamento del meteo
  if (forceWeatherUpdate || (currentTime - lastWeatherFetch >= WEATHER_FETCH_INTERVAL)) {
    if (forceWeatherUpdate) {
      Serial.println("Aggiornamento forzato del meteo richiesto");
    } else {
      Serial.println("Aggiornamento periodico del meteo (10 minuti)");
    }

    fetchWeather();
    lastWeatherFetch = currentTime;
    forceWeatherUpdate = false;

    // Dopo l'aggiornamento, mostra le informazioni testuali
    lastScreenChange = currentTime;
    showText = true;
    DisplayOneTime = false;

    // Se siamo in modalità AUTO, aggiorna anche i LED
    if (Currentmode == MODO_AUTO) {
      updateWeatherLEDs();
      // Esegui l'animazione a onda fluida con il colore del meteo
      neopixelModeChangeAnimation = true;
      neopixelAnimStep = 0;
      neopixelAnimLastUpdate = millis();
    }
  }

  // Se siamo in modalità regolazione luminosità, non eseguire il resto del loop normale
  if (isLongPressMode) {
    return;
  }

  checkNightMode();

  // Gestione dell'animazione cambio modalità NeoPixel
  runNeopixelModeChangeAnimation();

  // Gestione della visualizzazione del nome della modalità
  if (showModeName) {
    if (millis() - modeNameStartTime >= MODE_NAME_DISPLAY_DURATION) {
      showModeName = false;
      // Torna alla visualizzazione normale del meteo
      showText = true;
      DisplayOneTime = false;
    } else {
      // Se stiamo mostrando il nome della modalità, non fare altro
      // Il display non viene aggiornato, quindi rimane il nome della modalità
      return;
    }
  }

  if (keypressed == false) {
    // GESTIONE MODALITÀ CON COMPORTAMENTI DIVERSI
    if (Currentmode == MODO_NOTTE) {
      // Modalità notte: solo display attivo, tutti i LED spenti
      if (showText) {
        if (currentTime - lastScreenChange >= SCREEN_TEXT_DURATION) {
          showText = false;
          lastScreenChange = currentTime;
          display.clearDisplay();
          currentFrame = 0;
        }
        if (!DisplayOneTime) {
          displayWeatherInfo();
          DisplayOneTime = true;
        }
      } else {
        if (currentTime - lastScreenChange >= SCREEN_ANIMATION_DURATION) {
          showText = true;
          lastScreenChange = currentTime;
          display.clearDisplay();
        }
        displayWeatherAnimation();
        DisplayOneTime = false;
      }
    }
    else if (Currentmode == MODO_ALL_OFF) {
      // Modalità tutto spento: display spento, tutti i LED spenti
      display.clearDisplay();
      display.display();
      AllLEDoff();
      pixels.clear();
      pixels.show();
    }
    else if (Currentmode == MODO_LED_OFF) {
      // Modalità LED spenti: solo display attivo, LED individuali spenti, anello spento
      if (showText) {
        if (currentTime - lastScreenChange >= SCREEN_TEXT_DURATION) {
          showText = false;
          lastScreenChange = currentTime;
          display.clearDisplay();
          currentFrame = 0;
        }
        if (!DisplayOneTime) {
          displayWeatherInfo();
          DisplayOneTime = true;
        }
      } else {
        if (currentTime - lastScreenChange >= SCREEN_ANIMATION_DURATION) {
          showText = true;
          lastScreenChange = currentTime;
          display.clearDisplay();
        }
        displayWeatherAnimation();
        DisplayOneTime = false;
      }
      
      // Assicura che i LED siano spenti
      AllLEDoff();
      pixels.clear();
      pixels.show();
    }
    else if (Currentmode == MODO_RING_OFF) {
      // Modalità anello spento: display attivo, LED individuali attivi, anello spento
      if (showText) {
        if (currentTime - lastScreenChange >= SCREEN_TEXT_DURATION) {
          showText = false;
          lastScreenChange = currentTime;
          display.clearDisplay();
          currentFrame = 0;
        }
      } else {
        if (currentTime - lastScreenChange >= SCREEN_ANIMATION_DURATION) {
          showText = true;
          lastScreenChange = currentTime;
          display.clearDisplay();
        }
      }

      if (showText) {
        if (!DisplayOneTime) {
          displayWeatherInfo();
          DisplayOneTime = true;
        }
      } else {
        displayWeatherAnimation();
        DisplayOneTime = false;
      }
      
      // Anello spento ma LED individuali attivi
      pixels.clear();
      pixels.show();
    }
    else {
      // Altre modalità (AUTO, COLORI, ecc.) - comportamento normale
      if (showText) {
        if (currentTime - lastScreenChange >= SCREEN_TEXT_DURATION) {
          showText = false;
          lastScreenChange = currentTime;
          display.clearDisplay();
          currentFrame = 0;
        }
      } else {
        if (currentTime - lastScreenChange >= SCREEN_ANIMATION_DURATION) {
          showText = true;
          lastScreenChange = currentTime;
          display.clearDisplay();
        }
      }

      // Gestione NeoPixel per modalità AUTO
      if (!neopixelBootAnimationInProgress && !neopixelModeChangeAnimation) {
        if (Currentmode == MODO_AUTO) {
          if (windAnimationEnabled && windScanning) {
            runWindDirectionScan(windLedIndex);
          } else if (windAnimationEnabled && windAnimationActive) {
            runWindAnimation();
          } else {
            runNeopixelAnimation();
          }
        }
      }

      if (showText) {
        if (!DisplayOneTime) {
          displayWeatherInfo();
          DisplayOneTime = true;
        }
      } else {
        displayWeatherAnimation();
        DisplayOneTime = false;
      }
    }
  }
}

// --- FUNZIONE PULSANTE CORRETTA ---
void handleButtonPress() {
  bool reading = digitalRead(PIN_PULSANTE);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) {
        keypressed = true;
        buttonPressStartTime = millis();
        isLongPressMode = false;
        Serial.println("Pulsante premuto (inizia timer).");
      } else {
        unsigned long pressDuration = millis() - buttonPressStartTime;
        Serial.printf("Pulsante rilasciato. Durata: %lu ms.\n", pressDuration);

        if (!isLongPressMode && pressDuration < LONG_PRESS_THRESHOLD) {
          Serial.println("Pressione breve rilevata. Cambio modalita'.");
          Currentmode++;
          if (Currentmode > MAX_MODE) Currentmode = MODO_AUTO;
          showText = true;
          DisplayOneTime = false;

          // Mostra il nome della modalità per 2 secondi
          showModeName = true;
          modeNameStartTime = millis();

          SetRingColor(Currentmode, neopixelAnimationEnabled); // Con animazione se abilitata
          saveConfig();
          // Forza aggiornamento immediato quando si cambia modalità
          forceWeatherUpdate = true;
        }
        else if (isLongPressMode) {
          Serial.println("Uscita dalla regolazione luminosita'.");
          isLongPressMode = false;
          saveConfig();
          showText = true;
          DisplayOneTime = false;
        }
        
        keypressed = false;
      }
    }
  }

  lastButtonState = reading;

  if (buttonState == LOW && !isLongPressMode) {
    if (millis() - buttonPressStartTime >= LONG_PRESS_THRESHOLD) {
      isLongPressMode = true;
      Serial.println("Entrato in modalita' regolazione luminosita'.");
      lastBrightnessChangeTime = millis();
      drawBrightnessBar(savedNeopixelBrightness);
    }
  }

  if (isLongPressMode && buttonState == LOW) {
    if (millis() - lastBrightnessChangeTime >= BRIGHTNESS_CHANGE_INTERVAL) {
      lastBrightnessChangeTime = millis();

      savedNeopixelBrightness += 5;
      if (savedNeopixelBrightness > 255) {
        savedNeopixelBrightness = 10;
      }
      
      pixels.setBrightness(savedNeopixelBrightness);
      pixels.show();
      
      drawBrightnessBar(savedNeopixelBrightness);
    }
  }
}

// --- FUNZIONE PER DISEGNARE LA BARRA DI LUMINOSITÀ ---
void drawBrightnessBar(uint8_t brightness) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(25, 2);
  display.println("LUMINOSITA'");
  
  int barWidth = 100;
  int barHeight = 8;
  int barX = (OLED_WIDTH - barWidth) / 2;
  int barY = 15;
  
  display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);
  
  int fillWidth = map(brightness, 0, 255, 0, barWidth - 2);
  display.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, SSD1306_WHITE);
  
  display.setCursor(40, 25);
  display.print("Valore: ");
  display.print(brightness);
  display.print("/255");

  display.display();
}

void runWindDirectionScan(int windLedIndex) {
  static unsigned long lastScanUpdate = 0;
  static int lastFrameLogged = -1;
  const unsigned long scanStepDuration = WIND_SCAN_DURATION / NUM_NEOPIXELS;

  if (millis() - lastScanUpdate >= scanStepDuration) {
    lastScanUpdate = millis();

    pixels.clear();
    pixels.setPixelColor(windScanFrame, pixels.Color(0, 255, 0));
    pixels.show();

    if (windScanFrame != lastFrameLogged) {
      Serial.print("WindScanFrame: ");
      Serial.println(windScanFrame);
      lastFrameLogged = windScanFrame;
    }

    windScanFrame = (windScanFrame + 1) % NUM_NEOPIXELS;

    if (windScanFrame == windLedIndex) {
      windScanning = false;
      windAnimationActive = true;
      animationFrame = 0;
      windAnimationStartTime = millis();
      Serial.println("Scan completata, inizio animazione vento");
    }
  }
}

// --- Funzioni di gestione della configurazione ---
void saveConfig() {
  preferences.begin("meteo-config", false);
  preferences.putString("city", savedCity);
  preferences.putInt("brightness", savedNeopixelBrightness);
  preferences.putInt("Currentmode", Currentmode);
  // NON salviamo più forecastDay per forzare sempre "Attuale" all'accensione
  // preferences.putInt("forecastDay", forecastDay);
  preferences.putInt("forecastPeriod", forecastPeriod);
  preferences.putBool("nightModeEnabled", nightModeEnabled);
  preferences.putInt("nightStartHour", nightStartHour);
  preferences.putInt("nightStartMinute", nightStartMinute);
  preferences.putInt("nightEndHour", nightEndHour);
  preferences.putInt("nightEndMinute", nightEndMinute);
  preferences.putBool("windAnimEnabled", windAnimationEnabled);
  preferences.putBool("neopixelAnimEnabled", neopixelAnimationEnabled); // Nuova variabile
  preferences.end();

  Serial.println("Configurazione salvata.");
}

void loadConfig() {
  Serial.println("\n--- Caricamento configurazione salvata ---");
  preferences.begin("meteo-config", false);

  preferences.getString("city", savedCity, sizeof(savedCity));
  Serial.println("Città: " + String(savedCity));

  savedNeopixelBrightness = preferences.getInt("brightness", 200);
  Serial.println("Luminosità NeoPixel: " + String(savedNeopixelBrightness));

  Currentmode = preferences.getInt("Currentmode", 0);
  Serial.println("Modalità corrente: " + String(Currentmode));

  // FORZA SEMPRE PREVISIONI ATTUALI ALL'ACCENSIONE
  forecastDay = -1; // Sempre Attuale all'accensione
  
  forecastPeriod = preferences.getInt("forecastPeriod", 0);
  // Limita il valore a 0-2 (mattino, pomeriggio, sera)
  if (forecastPeriod < 0 || forecastPeriod > 2) forecastPeriod = 0;
  
  nightModeEnabled = preferences.getBool("nightModeEnabled", false);
  nightStartHour = preferences.getInt("nightStartHour", 22);
  nightStartMinute = preferences.getInt("nightStartMinute", 0);
  nightEndHour = preferences.getInt("nightEndHour", 7);
  nightEndMinute = preferences.getInt("nightEndMinute", 0);
  windAnimationEnabled = preferences.getBool("windAnimEnabled", true);
  neopixelAnimationEnabled = preferences.getBool("neopixelAnimEnabled", true); // Nuova variabile

  Serial.println("Modalità notte:");
  Serial.println(" - Abilitata: " + String(nightModeEnabled));
  Serial.println(" - Orario: " + String(nightStartHour) + ":" + String(nightStartMinute) + " -> " + String(nightEndHour) + ":" + String(nightEndMinute));
  Serial.println("Giorno previsione: ATTUALE (sempre forzato all'accensione)");
  Serial.println("Periodo previsione: " + String(forecastPeriod) + " (0:Mattino, 1:Pomeriggio, 2:Sera)");
  Serial.println("Animazione vento: " + String(windAnimationEnabled ? "Abilitata" : "Disabilitata"));
  Serial.println("Animazione cambio modalità: " + String(neopixelAnimationEnabled ? "Abilitata" : "Disabilitata")); // Nuova variabile

  String storedApiKey = preferences.getString("api_key", "");
  if (storedApiKey.length() > 0) {
    Serial.println("API key caricata: " + storedApiKey);
  } else {
    Serial.println("⚠️  Nessuna API key salvata trovata.");
  }
  
  preferences.end();

  Serial.println("--- Fine caricamento configurazione ---\n");
}

void saveConfigCallback() {
  Serial.println("📡 Salvataggio configurazione WiFiManager...");

  String newApiKey = custom_api_key.getValue();
  newApiKey.trim();

  if (newApiKey.length() > 0) {
    preferences.begin("meteo-config", false);
    String oldApiKey = preferences.getString("api_key", "");
    if (newApiKey != oldApiKey) {
      preferences.putString("api_key", newApiKey);
      Serial.println("✅ API key aggiornata e salvata: " + newApiKey);
    } else {
      Serial.println("ℹ️ API key invariata, nessun salvataggio necessario.");
    }
    preferences.end();
  } else {
    Serial.println("⚠️ Nessuna API key inserita — nessun salvataggio.");
  }
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entrato in modalita' configurazione AP");
  Serial.println(myWiFiManager->getConfigPortalSSID());

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 8);
  display.println("Configura WiFi:");
  display.setCursor(0, 16);
  display.println(myWiFiManager->getConfigPortalSSID());
  display.setCursor(0, 24);
  display.println("Connett.a questa rete");
  display.display();
}

// --- FUNZIONI METEO CORRETTE ---
void fetchWeather() {
  if (forecastDay == -1) {
    // Usa API meteo corrente
    fetchCurrentWeather();
  } else {
    // Usa API previsioni
    fetchForecastWeather();
  }
  
  DisplayOneTime = false;
}

void fetchCurrentWeather() {
  Serial.println("=== Fetching current weather (OpenWeather Current API) ===");
  
  HTTPClient http;

  preferences.begin("meteo-config", false);
  String currentApiKey = preferences.getString("api_key", OPENWEATHER_API_KEY);
  preferences.end();

  String apiCityQuery = savedCity;
  if (apiCityQuery.equalsIgnoreCase("Roma")) {
    apiCityQuery = "Rome";
  }

  apiCityQuery = urlEncode(apiCityQuery);

  String weatherUrl = "http://api.openweathermap.org/data/2.5/weather?q="
                      + apiCityQuery + "," + String(savedCountryCode)
                      + "&units=metric&appid=" + currentApiKey + "&lang=it";

  Serial.print("Current Weather URL: ");
  Serial.println(weatherUrl);

  http.begin(weatherUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Current Weather API Response received");

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      weatherDescription = "Errore JSON OWM.";
      return;
    }

    // Estrai dati meteo corrente
    weatherDescription = doc["weather"][0]["description"].as<String>();
    weatherConditionCode = doc["weather"][0]["id"].as<int>();
    temperatureCelsius = doc["main"]["temp"].as<float>();
    humidity = doc["main"]["humidity"].as<int>();
    windSpeed = doc["wind"]["speed"].as<float>() * 3.6; // Converti m/s in km/h
    windDirection = doc["wind"]["deg"].as<int>();
    sunriseTime = doc["sys"]["sunrise"].as<long>();
    sunsetTime = doc["sys"]["sunset"].as<long>();

    // Aggiorna i LED solo se non siamo in modalità che li richiedono spenti
    if (Currentmode != MODO_LED_OFF && Currentmode != MODO_ALL_OFF && Currentmode != MODO_NOTTE) {
      switch (Currentmode) {
        case MODO_AUTO:
          updateWeatherLEDs();
          // Esegui l'animazione a onda fluida con il colore del meteo
          neopixelModeChangeAnimation = true;
          neopixelAnimStep = 0;
          neopixelAnimLastUpdate = millis();
          break;
        case MODO_ROSSO:
        case MODO_VERDE:
        case MODO_BLU:
        case MODO_VIOLA:
        case MODO_CIANO:
        case MODO_GIALLO:
        case MODO_BIANCO:
        case MODO_RING_OFF:
          updateWeatherLEDs();
          break;
        default:
          break;
      }
    }

    Serial.println("--- Risultato Meteo Attuale ---");
    Serial.print("Descrizione Meteo: ");
    Serial.println(weatherDescription);
    Serial.print("Codice Condizione: ");
    Serial.println(weatherConditionCode);
    Serial.print("Temperatura: ");
    Serial.print(temperatureCelsius);
    Serial.println(" °C");
    Serial.print("Umidità: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.print("Velocità vento: ");
    Serial.println(windSpeed);
    Serial.print("Direzione vento: ");
    Serial.println(windDirection);
    Serial.println("-----------------------------");

  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    weatherDescription = "Errore HTTP OWM.";

    if (httpCode == 401) {
      Serial.println("API Key non valida o non attiva.");
      weatherDescription = "API Key Errata!";
    } else if (httpCode == 404) {
      Serial.println("Citta' o paese non trovato.");
      weatherDescription = "Citta' non trovata!";
    }
  }

  http.end();
}

void fetchForecastWeather() {
  Serial.println("=== Fetching weather (OpenWeather Forecast API) ===");
  Serial.print("Giorno previsione richiesto: ");
  switch(forecastDay) {
    case 0: Serial.println("OGGI"); break;
    case 1: Serial.println("DOMANI"); break;
    case 2: Serial.println("DOPODOMANI"); break;
    case 3: Serial.println("GIORNO 4"); break;
    case 4: Serial.println("GIORNO 5"); break;
  }
  
  Serial.print("Periodo previsione richiesto: ");
  switch(forecastPeriod) {
    case 0: Serial.println("MATTINO"); break;
    case 1: Serial.println("POMERIGGIO"); break;
    case 2: Serial.println("SERA"); break;
  }
  
  HTTPClient http;

  preferences.begin("meteo-config", false);
  String currentApiKey = preferences.getString("api_key", OPENWEATHER_API_KEY);
  preferences.end();

  String apiCityQuery = savedCity;
  if (apiCityQuery.equalsIgnoreCase("Roma")) {
    apiCityQuery = "Rome";
  }

  apiCityQuery = urlEncode(apiCityQuery);

  String weatherUrl = "http://api.openweathermap.org/data/2.5/forecast?q="
                      + apiCityQuery + "," + String(savedCountryCode)
                      + "&units=metric&appid=" + currentApiKey + "&lang=it&cnt=40";

  Serial.print("Forecast URL: ");
  Serial.println(weatherUrl);

  http.begin(weatherUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Forecast API Response received");

    DynamicJsonDocument doc(40 * 1024);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      weatherDescription = "Errore JSON OWM.";
      return;
    }

    // ANALISI DI TUTTI I PERIODI DEL RANGE SELEZIONATO
    int startIndex, endIndex;
    
    // Definisce i range per ogni periodo
    switch(forecastPeriod) {
      case 0: // Mattino: periodi 2 e 3 (06-12)
        startIndex = 2 + (forecastDay * 8);
        endIndex = 3 + (forecastDay * 8);
        break;
      case 1: // Pomeriggio: periodi 4 e 5 (12-18)
        startIndex = 4 + (forecastDay * 8);
        endIndex = 5 + (forecastDay * 8);
        break;
      case 2: // Sera: periodi 6 e 7 (18-00)
        startIndex = 6 + (forecastDay * 8);
        endIndex = 7 + (forecastDay * 8);
        break;
    }
    
    // Limita gli indici ai dati disponibili
    if (startIndex >= doc["list"].size()) startIndex = doc["list"].size() - 1;
    if (endIndex >= doc["list"].size()) endIndex = doc["list"].size() - 1;
    
    // Analizza tutti i periodi del range per trovare la condizione predominante
    int conditionCounts[1000] = {0}; // Array per contare le occorrenze di ogni codice meteo
    float totalTemp = 0;
    float totalHumidity = 0;
    float totalWindSpeed = 0;
    int totalWindDirection = 0;
    int periodCount = 0;
    
    for (int i = startIndex; i <= endIndex && i < doc["list"].size(); i++) {
      JsonObject forecast = doc["list"][i];
      int conditionCode = forecast["weather"][0]["id"].as<int>();
      conditionCounts[conditionCode]++;
      
      totalTemp += forecast["main"]["temp"].as<float>();
      totalHumidity += forecast["main"]["humidity"].as<int>();
      totalWindSpeed += forecast["wind"]["speed"].as<float>();
      totalWindDirection += forecast["wind"]["deg"].as<int>();
      periodCount++;
      
      Serial.print("Periodo ");
      Serial.print(i);
      Serial.print(": Condizione ");
      Serial.print(conditionCode);
      Serial.print(" - ");
      Serial.println(forecast["weather"][0]["description"].as<String>());
    }
    
    if (periodCount == 0) {
      // Fallback: usa l'ultimo periodo disponibile
      int index = startIndex;
      if (index >= doc["list"].size()) index = doc["list"].size() - 1;
      JsonObject forecast = doc["list"][index];
      
      weatherDescription = forecast["weather"][0]["description"].as<String>();
      weatherConditionCode = forecast["weather"][0]["id"].as<int>();
      temperatureCelsius = forecast["main"]["temp"].as<float>();
      humidity = forecast["main"]["humidity"].as<int>();
      windSpeed = forecast["wind"]["speed"].as<float>() * 3.6;
      windDirection = forecast["wind"]["deg"].as<int>();
      
      Serial.println("⚠️  Usando fallback - dati insufficienti");
    } else {
      // Trova il codice di condizione più frequente
      int maxCount = 0;
      int predominantCondition = 800; // Default: cielo sereno
      
      for (int i = 0; i < 1000; i++) {
        if (conditionCounts[i] > maxCount) {
          maxCount = conditionCounts[i];
          predominantCondition = i;
        }
      }
      
      // In caso di parità, preferisci condizioni peggiori (pioggia/temporale > nuvole > sole)
      if (maxCount > 0) {
        // Se c'è almeno una condizione di pioggia/temporale, preferiscila
        for (int i = 200; i < 700; i++) {
          if (conditionCounts[i] == maxCount) {
            predominantCondition = i;
            break;
          }
        }
      }
      
      weatherConditionCode = predominantCondition;
      weatherDescription = getWeatherDescriptionItalian(predominantCondition);
      temperatureCelsius = totalTemp / periodCount;
      humidity = totalHumidity / periodCount;
      windSpeed = (totalWindSpeed / periodCount) * 3.6;
      windDirection = totalWindDirection / periodCount;
    }

    // Per alba e tramonto, usa i dati della città (primo elemento)
    sunriseTime = doc["city"]["sunrise"].as<long>();
    sunsetTime = doc["city"]["sunset"].as<long>();

    // Aggiorna i LED solo se non siamo in modalità che li richiedono spenti
    if (Currentmode != MODO_LED_OFF && Currentmode != MODO_ALL_OFF && Currentmode != MODO_NOTTE) {
      switch (Currentmode) {
        case MODO_AUTO:
          updateWeatherLEDs();
          // Esegui l'animazione a onda fluida con il colore del meteo
          neopixelModeChangeAnimation = true;
          neopixelAnimStep = 0;
          neopixelAnimLastUpdate = millis();
          break;
        case MODO_ROSSO:
        case MODO_VERDE:
        case MODO_BLU:
        case MODO_VIOLA:
        case MODO_CIANO:
        case MODO_GIALLO:
        case MODO_BIANCO:
        case MODO_RING_OFF:
          updateWeatherLEDs();
          break;
        default:
          break;
      }
    }

    Serial.println("--- Risultato Previsione ---");
    Serial.print("ForecastDay: ");
    Serial.println(forecastDay);
    Serial.print("ForecastPeriod: ");
    Serial.println(forecastPeriod);
    Serial.print("Range analizzato: ");
    Serial.print(startIndex);
    Serial.print(" - ");
    Serial.println(endIndex);
    Serial.print("Periodi considerati: ");
    Serial.println(periodCount);
    Serial.print("Condizione predominante: ");
    Serial.println(weatherConditionCode);
    Serial.print("Descrizione Meteo: ");
    Serial.println(weatherDescription);
    Serial.print("Temperatura media: ");
    Serial.print(temperatureCelsius);
    Serial.println(" °C");
    Serial.print("Umidità media: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.print("Velocità vento media: ");
    Serial.println(windSpeed);
    Serial.print("Direzione vento media: ");
    Serial.println(windDirection);
    Serial.println("-----------------------------");

  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    weatherDescription = "Errore HTTP OWM.";

    if (httpCode == 401) {
      Serial.println("API Key non valida o non attiva.");
      weatherDescription = "API Key Errata!";
    } else if (httpCode == 404) {
      Serial.println("Citta' o paese non trovato.");
      weatherDescription = "Citta' non trovata!";
    }
  }

  http.end();
}

int getTextWidth(const String &text, int textSize) {
  return text.length() * 6 * textSize;
}

// --- FUNZIONE displayWeatherInfo CORRETTA per display 32px ---
void displayWeatherInfo() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // MODIFICATO: Layout compatto per display 32px
  
  // Linea 1: Città (centrata)
  display.setTextSize(1);
  String cityDisplay = savedCity;
  int cityWidth = getTextWidth(cityDisplay, 1);
  if (cityWidth > OLED_WIDTH) {
    // Tronca città troppo lunga
    int maxChars = (OLED_WIDTH - 18) / 6;
    if (maxChars < 3) maxChars = 3;
    if (cityDisplay.length() > maxChars) {
      cityDisplay = cityDisplay.substring(0, maxChars - 3) + "...";
    }
    cityWidth = getTextWidth(cityDisplay, 1);
  }
  int cityX = (OLED_WIDTH - cityWidth) / 2;
  display.setCursor(cityX, 0);
  display.println(cityDisplay);

  // Linea 2: Descrizione meteo (centrata)
  String desc = getWeatherDescriptionItalian(weatherConditionCode);
  int descWidth = getTextWidth(desc, 1);
  int descX = (OLED_WIDTH - descWidth) / 2;
  display.setCursor(descX, 8);
  display.println(desc);

  // Linea 3: Temperatura e Umidità (centrata)
  String tempHumStr = "T:" + String(temperatureCelsius, 1) + "C H:" + String(humidity) + "%";
  int tempHumWidth = getTextWidth(tempHumStr, 1);
  int tempHumX = (OLED_WIDTH - tempHumWidth) / 2;
  display.setCursor(tempHumX, 16);
  display.println(tempHumStr);

  // Linea 4: Vento e Ora (centrata)
  timeClient.update();
  time_t currentTime = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&currentTime);
  
  String windStr = "V:" + String(windSpeed, 1) + "km/h";
  char timeBuffer[6];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", timeinfo);
  String windTimeStr = windStr + " " + String(timeBuffer);
  
  int windTimeWidth = getTextWidth(windTimeStr, 1);
  int windTimeX = (OLED_WIDTH - windTimeWidth) / 2;
  display.setCursor(windTimeX, 24);
  display.println(windTimeStr);

  display.display();

  // Avvia animazione vento solo se siamo in modalità AUTO e se abilitata
  if (Currentmode == MODO_AUTO && windAnimationEnabled) {
    Serial.println("Mostro INFO METEO - Avvio animazione vento");
    int windLedIndex = 0;
    if (NUM_NEOPIXELS > 0) {
      windLedIndex = (360 - windDirection + 90) % 360;
      windLedIndex = map(windLedIndex, 0, 360, 0, NUM_NEOPIXELS);
    }

    windScanFrame = 0;
    windScanning = true;
    windAnimationActive = false;
  }
}

// --- FUNZIONE displayWeatherAnimation CORRETTA per display 32px con scritta giorno ---
void displayWeatherAnimation() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // CALCOLO CORRETTO DI isDaytime per TUTTI i giorni di previsione
  // CALCOLO CORRETTO DI isDaytime
  bool isDaytime;
  if (forecastDay == -1) {
    // Per ATTUALE: usa ora corrente e dati alba/tramonto reali
    timeClient.update();
    time_t currentTime = timeClient.getEpochTime();
    isDaytime = (currentTime >= sunriseTime && currentTime < sunsetTime);
  } else {
    // Per TUTTI i giorni di previsione (OGGI, DOMANI, DOPODOMANI, GIORNO 4, GIORNO 5):
    // usa il PERIODO selezionato per determinare giorno/notte
    switch(forecastPeriod) {
      case 0: // Mattino (06-12) - sempre giorno
      case 1: // Pomeriggio (12-18) - sempre giorno
        isDaytime = true;
        break;
      case 2: // Sera (18-00) - sempre notte
        isDaytime = false;
        break;
    }
  }

  // MOSTRA SCRITTA GIORNO/PERIODO IN ALTO
  display.setTextSize(1);
  String dayString = "";
  
  if (forecastDay == -1) {
    dayString = "ATTUALE";
  } else {
    switch(forecastDay) {
      case 0: dayString = "OGGI"; break;
      case 1: dayString = "DOMANI"; break;
      case 2: dayString = "DOPODOMANI"; break;
      case 3: dayString = "GIORNO 4"; break;
      case 4: dayString = "GIORNO 5"; break;
    }
    
    switch(forecastPeriod) {
      case 0: dayString += " MATTINO"; break;
      case 1: dayString += " POMERIGGIO"; break;
      case 2: dayString += " SERA"; break;
    }
  }
  
  // Centra la scritta del giorno
  int textWidth = dayString.length() * 6;
  int textX = (OLED_WIDTH - textWidth) / 2;
  display.setCursor(textX, 0);
  display.println(dayString);

  const unsigned char **currentAnimation = nullptr;
  const unsigned char **currentAnimation1 = nullptr;
  int currentAnimFrames = 0;
  int currentAnimWidth = 0;
  int currentAnimHeight = 0;

  if (weatherConditionCode >= 200 && weatherConditionCode < 300) {
    currentAnimFrames = STORM_ANIM_FRAMES;
    currentAnimWidth = STORM_ANIM_WIDTH;
    currentAnimHeight = STORM_ANIM_HEIGHT;
    currentAnimation = storm_animation;
    currentAnimation1 = rain_animation;
    dualAnimation = true;

  } else if ((weatherConditionCode >= 300 && weatherConditionCode < 400) || (weatherConditionCode >= 500 && weatherConditionCode < 600) || (weatherConditionCode >= 600 && weatherConditionCode < 700)) {
    currentAnimation = rain_animation;
    currentAnimFrames = RAIN_ANIM_FRAMES;
    currentAnimWidth = RAIN_ANIM_WIDTH;
    currentAnimHeight = RAIN_ANIM_HEIGHT;
    dualAnimation = false;

  } else if (weatherConditionCode == 800) {
    if (isDaytime) {
      currentAnimation = sun_animation;
      currentAnimFrames = SUN_ANIM_FRAMES;
      currentAnimWidth = SUN_ANIM_WIDTH;
      currentAnimHeight = SUN_ANIM_HEIGHT;
    } else {
      currentAnimation = moon_animation;
      currentAnimFrames = MOON_ANIM_FRAMES;
      currentAnimWidth = MOON_ANIM_WIDTH;
      currentAnimHeight = MOON_ANIM_HEIGHT;
    }
    dualAnimation = false;

  } else if ((weatherConditionCode >= 801 && weatherConditionCode <= 804) || (weatherConditionCode >= 700 && weatherConditionCode < 799)) {
    if (isDaytime) {
      currentAnimation = sun_animation;
      currentAnimFrames = SUN_ANIM_FRAMES;
      currentAnimWidth = SUN_ANIM_WIDTH;
      currentAnimHeight = SUN_ANIM_HEIGHT;
    } else {
      currentAnimation = moon_animation;
      currentAnimFrames = MOON_ANIM_FRAMES;
      currentAnimWidth = MOON_ANIM_WIDTH;
      currentAnimHeight = MOON_ANIM_HEIGHT;
    }
    currentAnimation1 = cloud_animation;
    dualAnimation = true;

  } else {
    display.setTextSize(1);
    display.setCursor(10, 12);
    display.println("No Animazione");
    display.setCursor(20, 20);
    display.println(weatherDescription);
  }

  if (currentAnimation != nullptr) {
    if (millis() - lastFrameChange >= ANIM_FRAME_DELAY) {
      currentFrame = (currentFrame + 1) % currentAnimFrames;
      lastFrameChange = millis();
    }

    // Posizionamento animazioni per display 32px con scritta in alto
    // Icone 24x24 pixel, posizionate a y=8 (dopo la scritta di 8px)
    int y_pos = 8; // Spazio per la scritta del giorno
    
    if (!dualAnimation) {
      int x_pos = (OLED_WIDTH - currentAnimWidth) / 2;
      display.drawBitmap(x_pos, y_pos, currentAnimation[currentFrame], currentAnimWidth, currentAnimHeight, SSD1306_WHITE);
    } else {
      // Per animazioni doppie
      int x_pos1 = (OLED_WIDTH / 3) - (currentAnimWidth / 2);
      int x_pos2 = (2 * OLED_WIDTH / 3) - (currentAnimWidth / 2);

      display.drawBitmap(x_pos1, y_pos, currentAnimation[currentFrame], currentAnimWidth, currentAnimHeight, SSD1306_WHITE);
      display.drawBitmap(x_pos2, y_pos, currentAnimation1[currentFrame], currentAnimWidth, currentAnimHeight, SSD1306_WHITE);
    }
  }

  display.display();
}

void AllLEDoff() {
  digitalWrite(PIN_LED_SOLE, LOW);
  digitalWrite(PIN_LED_NUVOLE, LOW);
  digitalWrite(PIN_LED_PIOGGIA, LOW);
  digitalWrite(PIN_LED_TEMPORALE, LOW);
  digitalWrite(PIN_LED_LUNA, LOW);
}

// --- FUNZIONE updateWeatherLEDs CORRETTA per TUTTI i giorni di previsione ---
void updateWeatherLEDs() {
  // Spegni i LED solo se non siamo in modalità che li richiedono spenti
  if (Currentmode != MODO_LED_OFF && Currentmode != MODO_ALL_OFF && Currentmode != MODO_NOTTE) {
    AllLEDoff();

    // CALCOLO CORRETTO DI isDaytime per TUTTI i giorni di previsione
    bool isDaytime;
    
    if (forecastDay == -1) {
      // Per ATTUALE: usa ora corrente e dati alba/tramonto reali
      timeClient.update();
      time_t currentTime = timeClient.getEpochTime();
      isDaytime = (currentTime >= sunriseTime && currentTime < sunsetTime);
    } else {
      // Per TUTTI i giorni di previsione (OGGI, DOMANI, DOPODOMANI, GIORNO 4, GIORNO 5):
      // usa il PERIODO selezionato per determinare giorno/notte
      switch(forecastPeriod) {
        case 0: // Mattino (06-12) - sempre giorno
        case 1: // Pomeriggio (12-18) - sempre giorno
          isDaytime = true;
          break;
        case 2: // Sera (18-00) - sempre notte
          isDaytime = false;
          break;
      }
    }

    uint32_t tempNeopixelBaseColor = pixels.Color(0, 0, 0);

    // LOGICA DI ACCENSIONE LED BASATA SUL CODICE METEO (ALLINEATA ALLE ANIMAZIONI)
    // TEMPORALE (200-299)
    if (weatherConditionCode >= 200 && weatherConditionCode < 300) {
      digitalWrite(PIN_LED_PIOGGIA, HIGH);
      digitalWrite(PIN_LED_TEMPORALE, HIGH);
      tempNeopixelBaseColor = pixels.Color(197, 0, 255);

    }
    // PIOGGERELLA (300-399)
    else if (weatherConditionCode >= 300 && weatherConditionCode < 400) {
      digitalWrite(PIN_LED_PIOGGIA, HIGH);
      digitalWrite(PIN_LED_NUVOLE, HIGH);
      tempNeopixelBaseColor = pixels.Color(0, 0, 255);

    }
    // PIOGGIA (500-599)
    else if (weatherConditionCode >= 500 && weatherConditionCode < 600) {
      digitalWrite(PIN_LED_PIOGGIA, HIGH);
      digitalWrite(PIN_LED_NUVOLE, HIGH);
      tempNeopixelBaseColor = pixels.Color(0, 180, 255);

    }
    // NEVE (600-699)
    else if (weatherConditionCode >= 600 && weatherConditionCode < 700) {
      digitalWrite(PIN_LED_PIOGGIA, HIGH);  // Usa LED pioggia anche per neve
      digitalWrite(PIN_LED_NUVOLE, HIGH);
      tempNeopixelBaseColor = pixels.Color(200, 200, 255);  // Colore più chiaro/azzurro per neve

    }
    // CONDIZIONI ATMOSFERICHE (700-799): Nebbia, Fumo, Foschia, Sabbia, Polvere, ecc.
    else if (weatherConditionCode >= 700 && weatherConditionCode < 800) {
      // CORREZIONE: Non è pioggia! Mostra SOLE/LUNA + NUVOLE come nell'animazione
      if (isDaytime) {
        digitalWrite(PIN_LED_SOLE, HIGH);
        digitalWrite(PIN_LED_NUVOLE, HIGH);
        tempNeopixelBaseColor = pixels.Color(200, 180, 100);  // Giallo sporco per atmosfera
      } else {
        digitalWrite(PIN_LED_LUNA, HIGH);
        digitalWrite(PIN_LED_NUVOLE, HIGH);
        tempNeopixelBaseColor = pixels.Color(100, 100, 120);  // Grigio bluastro
      }

    }
    // CIELO SERENO (800)
    else if (weatherConditionCode == 800) {
      if (isDaytime) {
        digitalWrite(PIN_LED_SOLE, HIGH);
        tempNeopixelBaseColor = pixels.Color(255, 227, 0);
      } else {
        digitalWrite(PIN_LED_LUNA, HIGH);
        tempNeopixelBaseColor = pixels.Color(128, 128, 128);
      }

    }
    // NUVOLE (801-804)
    else if (weatherConditionCode >= 801 && weatherConditionCode <= 804) {
      if (isDaytime) {
        digitalWrite(PIN_LED_SOLE, HIGH);
        digitalWrite(PIN_LED_NUVOLE, HIGH);
        tempNeopixelBaseColor = pixels.Color(255, 227, 0);
      } else {
        digitalWrite(PIN_LED_LUNA, HIGH);
        digitalWrite(PIN_LED_NUVOLE, HIGH);
        tempNeopixelBaseColor = pixels.Color(128, 128, 128);
      }
    }

    // CORREZIONE: Aggiorna neopixelBaseColor solo in modalità AUTO
    // In modalità colore manuale, neopixelBaseColor viene gestito da SetRingColor
    if (Currentmode == MODO_AUTO) {
      neopixelBaseColor = tempNeopixelBaseColor;
    }
    
    // DEBUG: Stampa lo stato dei LED per verifica
    Serial.println("=== STATO LED INDIVIDUALI ===");
    Serial.print("Giorno previsione: "); Serial.println(forecastDay);
    Serial.print("Periodo: "); Serial.println(forecastPeriod);
    Serial.print("SOLE: "); Serial.println(digitalRead(PIN_LED_SOLE));
    Serial.print("NUVOLE: "); Serial.println(digitalRead(PIN_LED_NUVOLE));
    Serial.print("PIOGGIA: "); Serial.println(digitalRead(PIN_LED_PIOGGIA));
    Serial.print("TEMPORALE: "); Serial.println(digitalRead(PIN_LED_TEMPORALE));
    Serial.print("LUNA: "); Serial.println(digitalRead(PIN_LED_LUNA));
    Serial.print("isDaytime: "); Serial.println(isDaytime);
    Serial.print("weatherConditionCode: "); Serial.println(weatherConditionCode);
    Serial.print("Descrizione: "); Serial.println(getWeatherDescriptionItalian(weatherConditionCode));
    Serial.println("=============================");
    
  } else {
    // Se siamo in modalità che richiedono LED spenti, assicuriamoci che siano spenti
    AllLEDoff();
  }
}

void setNeopixelColor(uint32_t color) {
  neopixelBaseColor = color;

  if (Currentmode != MODO_AUTO && Currentmode != MODO_LED_OFF && Currentmode != MODO_ALL_OFF && Currentmode != MODO_NOTTE) {
    pixels.setBrightness(savedNeopixelBrightness);
    for (int i = 0; i < NUM_NEOPIXELS; i++) {
      pixels.setPixelColor(i, neopixelBaseColor);
    }
    pixels.show();
  }
}

void animateNeopixelCircular() {
  uint32_t targetColor = neopixelBaseColor;

  pixels.clear();
  pixels.show();

  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    pixels.setPixelColor(i, targetColor);
    pixels.show();
    delay(NEOPIXEL_BOOT_ANIM_DELAY);
  }
}

void ActivateCircular(uint32_t targetColor) {
  pixels.clear();
  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    pixels.setPixelColor(i, targetColor);
  }
  pixels.show();
}

void runNeopixelAnimation() {
  // Se non siamo in modalità AUTO, non eseguire l'animazione basata sul meteo
  if (Currentmode != MODO_AUTO) {
    return;
  }
  
  uint8_t animatedRed = 0;
  uint8_t animatedGreen = 0;
  uint8_t animatedBlue = 0;

  static unsigned long lastUpdate = 0;
  unsigned long currentMillis = millis();

  static int currentBreathBrightness = NEOPIXEL_BREATH_MIN;
  static bool breathIncreasing = true;

  const unsigned long TARGET_CYCLE_DURATION_MS = 4000;
  const int ACTUAL_BRIGHTNESS_RANGE = NEOPIXEL_BREATH_MAX - NEOPIXEL_BREATH_MIN;

  unsigned long BREATH_UPDATE_INTERVAL_MS = 0;
  if (ACTUAL_BRIGHTNESS_RANGE > 0) {
    BREATH_UPDATE_INTERVAL_MS = TARGET_CYCLE_DURATION_MS / (2 * ACTUAL_BRIGHTNESS_RANGE);
  } else {
    BREATH_UPDATE_INTERVAL_MS = BREATH_UPDATE_SPEED;
  }

  if (BREATH_UPDATE_INTERVAL_MS == 0) {
    BREATH_UPDATE_INTERVAL_MS = 1;
  }

  if (currentMillis - lastUpdate >= BREATH_UPDATE_INTERVAL_MS) {
    lastUpdate = currentMillis;

    if (breathIncreasing) {
      currentBreathBrightness += 1;
      if (currentBreathBrightness >= NEOPIXEL_BREATH_MAX) {
        currentBreathBrightness = NEOPIXEL_BREATH_MAX;
        breathIncreasing = false;
      }
    } else {
      currentBreathBrightness -= 1;
      if (currentBreathBrightness <= NEOPIXEL_BREATH_MIN) {
        currentBreathBrightness = NEOPIXEL_BREATH_MIN;
        breathIncreasing = true;
      }
    }

    uint8_t red = (neopixelBaseColor >> 16) & 0xFF;
    uint8_t green = (neopixelBaseColor >> 8) & 0xFF;
    uint8_t blue = neopixelBaseColor & 0xFF;

    if (neopixelBaseColor == pixels.Color(0, 0, 0)) {
      pixels.clear();
    } else {

      int breathRange = NEOPIXEL_BREATH_MAX - NEOPIXEL_BREATH_MIN;
      if (breathRange <= 0) breathRange = 1;

      if (red == 0) {
        animatedRed = 0;
      } else {
        int minRed = NEOPIXEL_BREATH_MIN;
        if (minRed > red) minRed = red;
        animatedRed = minRed + (currentBreathBrightness - NEOPIXEL_BREATH_MIN) * (red - minRed) / breathRange;
      }

      if (green == 0) {
        animatedGreen = 0;
      } else {
        int minGreen = NEOPIXEL_BREATH_MIN;
        if (minGreen > green) minGreen = green;
        animatedGreen = minGreen + (currentBreathBrightness - NEOPIXEL_BREATH_MIN) * (green - minGreen) / breathRange;
      }

      if (blue == 0) {
        animatedBlue = 0;
      } else {
        int minBlue = NEOPIXEL_BREATH_MIN;
        if (minBlue > blue) minBlue = blue;
        animatedBlue = minBlue + (currentBreathBrightness - NEOPIXEL_BREATH_MIN) * (blue - minBlue) / breathRange;
      }

      for (int i = 0; i < NUM_NEOPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(animatedRed, animatedGreen, animatedBlue));
      }
    }
    pixels.show();
  }
}

void runWindAnimation() {
  static unsigned long lastTrailUpdate = 0;
  static unsigned long windAnimStartTime = 0;

  // VARIABILI STATICHE PER EVITARE CHE CAMBINO DURANTE L'ANIMAZIONE
  static int savedBeaufort = 0;
  static int savedTrailLength = 0;
  static uint8_t savedWindR = 0;
  static uint8_t savedWindG = 0;
  static uint8_t savedWindB = 0;
  static int savedWindLedIndex = 0;
  static int savedTotalFrames = 0;

  const unsigned long TRAIL_TOTAL_DURATION = 4000;
  const int FADE_FRAMES_PER_LED = 8;
  const int FADE_PEAK = FADE_FRAMES_PER_LED / 2;
  const unsigned long ANIMATION_TIMEOUT = 10000; // Timeout di sicurezza: 10 secondi

  // All'inizio dell'animazione (animationFrame == 0), salva tutti i parametri
  if (animationFrame == 0) {
    windAnimStartTime = millis();
    savedBeaufort = getBeaufortScale(windSpeed);
    savedTrailLength = getTrailLengthByBeaufort(savedBeaufort);
    getWindColorByBeaufort(savedBeaufort, savedWindR, savedWindG, savedWindB);

    savedWindLedIndex = (360 - windDirection + 90) % 360;
    savedWindLedIndex = map(savedWindLedIndex, 0, 360, 0, NUM_NEOPIXELS);

    savedTotalFrames = (savedTrailLength + 1) * FADE_FRAMES_PER_LED;

    Serial.print("Inizio animazione vento - Beaufort: ");
    Serial.print(savedBeaufort);
    Serial.print(", TrailLength: ");
    Serial.print(savedTrailLength);
    Serial.print(", TotalFrames: ");
    Serial.println(savedTotalFrames);
  }

  // Timeout di sicurezza: se l'animazione dura troppo, termina forzatamente
  if (millis() - windAnimStartTime > ANIMATION_TIMEOUT) {
    Serial.println("TIMEOUT animazione vento - Forzando terminazione");
    animationFrame = 0;
    windAnimationActive = false;
    return;
  }

  // Usa i valori salvati invece di ricalcolarli
  int beaufort = savedBeaufort;
  int trailLength = savedTrailLength;
  uint8_t windR = savedWindR;
  uint8_t windG = savedWindG;
  uint8_t windB = savedWindB;
  int windLedIndex = savedWindLedIndex;
  int totalFrames = savedTotalFrames;

  unsigned long frameDuration = TRAIL_TOTAL_DURATION / max(totalFrames, 1);

  uint8_t ledR[NUM_NEOPIXELS] = { 0 };
  uint8_t ledG[NUM_NEOPIXELS] = { 0 };
  uint8_t ledB[NUM_NEOPIXELS] = { 0 };

  for (int offset = 0; offset <= trailLength; offset++) {
    int startFrame = offset * FADE_FRAMES_PER_LED;

    for (int f = 0; f < FADE_FRAMES_PER_LED; f++) {
      int activeFrame = startFrame + f;

      if (animationFrame >= activeFrame) {
        float intensity;
        if (animationFrame <= activeFrame + FADE_PEAK) {
          intensity = (float)(animationFrame - activeFrame) / FADE_PEAK;
        } else {
          intensity = 1.0;
        }

        intensity = constrain(intensity, 0.0, 1.0);

        uint8_t r = windR * intensity;
        uint8_t g = windG * intensity;
        uint8_t b = windB * intensity;

        int leftIndex = (windLedIndex - offset + NUM_NEOPIXELS) % NUM_NEOPIXELS;
        int rightIndex = (windLedIndex + offset) % NUM_NEOPIXELS;

        ledR[leftIndex] = max(ledR[leftIndex], r);
        ledG[leftIndex] = max(ledG[leftIndex], g);
        ledB[leftIndex] = max(ledB[leftIndex], b);

        ledR[rightIndex] = max(ledR[rightIndex], r);
        ledG[rightIndex] = max(ledG[rightIndex], g);
        ledB[rightIndex] = max(ledB[rightIndex], b);
      }
    }
  }

  ledR[windLedIndex] = windR;
  ledG[windLedIndex] = windG;
  ledB[windLedIndex] = windB;

  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(ledR[i], ledG[i], ledB[i]));
  }
  pixels.show();

  if (millis() - lastTrailUpdate >= frameDuration) {
    lastTrailUpdate = millis();
    animationFrame++;

    if (animationFrame > totalFrames + FADE_FRAMES_PER_LED) {
      animationFrame = 0;
      windAnimationActive = false;
      Serial.println("Animazione scia vento terminata - Passaggio a NEOPIXEL BREATHING");
    }
  }
}

int getBeaufortScale(float windSpeed) {
  if (windSpeed < 1) return 0;
  else if (windSpeed <= 5) return 1;
  else if (windSpeed <= 11) return 2;
  else if (windSpeed <= 19) return 3;
  else if (windSpeed <= 28) return 4;
  else if (windSpeed <= 38) return 5;
  else if (windSpeed <= 49) return 6;
  else if (windSpeed <= 61) return 7;
  else if (windSpeed <= 74) return 8;
  else if (windSpeed <= 88) return 9;
  else if (windSpeed <= 102) return 10;
  else if (windSpeed <= 117) return 11;
  else return 12;
}

void getWindColorByBeaufort(int beaufort, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (beaufort == 0) {
    r = g = b = 0;
  } else if (beaufort <= 3) {
    r = 0;
    g = 255;
    b = 0;
  } else if (beaufort <= 6) {
    r = 255;
    g = 165;
    b = 0;
  } else if (beaufort <= 9) {
    r = 255;
    g = 0;
    b = 0;
  } else {
    r = 128;
    g = 0;
    b = 128;
  }
}

int getTrailLengthByBeaufort(int beaufort) {
  if (beaufort == 0) return 0;
  if (beaufort == 1) return 1;
  if (beaufort == 2) return 2;
  if (beaufort == 3) return 3;
  if (beaufort == 4) return 2;
  if (beaufort == 5) return 3;
  if (beaufort == 6) return 4;
  if (beaufort == 7) return 3;
  if (beaufort == 8) return 4;
  if (beaufort == 9) return 5;
  return 6;
}

int calcolaColoreFinale(int iniziale, int VAR, int valMin, int valMax) {
  if (iniziale == 0) return 0;

  int delta = 50;
  int coloreMin = iniziale - delta;
  if (coloreMin < 0) coloreMin = 0;

  int range = valMax - valMin;
  if (range <= 0) return iniziale;

  int incremento = (VAR - valMin) * (iniziale - coloreMin) / range;
  return coloreMin + incremento;
}

// --- FUNZIONE SetRingColor CORRETTA - Adattata per display 32px ---
void SetRingColor(int Mode, bool animate) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(5, 8);  // MODIFICATO: Posizione centrale per display 32px
  
  switch (Mode) {
    case MODO_AUTO:
      display.println("  AUTO");
      // Non cambiare neopixelBaseColor, perché sarà impostato dal meteo
      // Avvia animazione a onda fluida se abilitata
      if (animate && neopixelAnimationEnabled) {
        neopixelModeChangeAnimation = true;
        neopixelAnimStep = 0;
        neopixelAnimLastUpdate = millis();
      } else {
        // Senza animazione, accendi direttamente l'anello con il colore di base
        for (int i = 0; i < NUM_NEOPIXELS; i++) {
          pixels.setPixelColor(i, neopixelBaseColor);
        }
        pixels.show();
      }
      break;
    case MODO_ROSSO:
      display.println(" ROSSO");
      neopixelBaseColor = pixels.Color(255, 0, 0);
      break;
    case MODO_VERDE:
      display.println(" VERDE");
      neopixelBaseColor = pixels.Color(0, 255, 0);
      break;
    case MODO_BLU:
      display.println("  BLU");
      neopixelBaseColor = pixels.Color(0, 0, 255);
      break;
    case MODO_VIOLA:
      display.println(" VIOLA");
      neopixelBaseColor = pixels.Color(197, 0, 255);
      break;
    case MODO_CIANO:
      display.println("AZZURRO");
      neopixelBaseColor = pixels.Color(0, 255, 255);
      break;
    case MODO_GIALLO:
      display.println("GIALLO");
      neopixelBaseColor = pixels.Color(255, 227, 0);
      break;
    case MODO_BIANCO:
      display.println("BIANCO");
      neopixelBaseColor = pixels.Color(255, 255, 255);
      break;
    case MODO_RING_OFF:
      display.println("AN.SP.");
      neopixelBaseColor = pixels.Color(0, 0, 0);
      break;
    case MODO_LED_OFF:
      display.println("L.SPENTI");
      neopixelBaseColor = pixels.Color(0, 0, 0);
      break;
    case MODO_ALL_OFF:
      display.println(" SPENTO");
      neopixelBaseColor = pixels.Color(0, 0, 0);
      break;
    case MODO_NOTTE:
      display.println(" NOTTE");
      neopixelBaseColor = pixels.Color(0, 0, 0);
      break;
  }
  
  display.display();
  
  // CORREZIONE: Gestione consistente dei LED in tutte le modalità
  if (Mode == MODO_LED_OFF || Mode == MODO_ALL_OFF || Mode == MODO_NOTTE) {
    // Modalità che richiedono LED spenti
    AllLEDoff();
    pixels.clear();
    pixels.show();
  } else if (Mode == MODO_RING_OFF) {
    // Solo anello spento, LED individuali attivi (NON chiamare AllLEDoff() qui)
    pixels.clear();
    pixels.show();
    // I LED individuali mantengono il loro stato basato sul meteo
  } else if (Mode >= MODO_ROSSO && Mode <= MODO_BIANCO) {
    // Modalità colore: LED individuali rimangono attivi con lo stato meteo, anello colorato
    // NON chiamare AllLEDoff() qui - i LED individuali mantengono il loro stato
    if (animate && neopixelAnimationEnabled) {
      // Avvia animazione cambio modalità a onda fluida
      neopixelModeChangeAnimation = true;
      neopixelAnimStep = 0;
      neopixelAnimLastUpdate = millis();
    } else {
      // Imposta il colore direttamente senza animazione
      for (int i = 0; i < NUM_NEOPIXELS; i++) {
        pixels.setPixelColor(i, neopixelBaseColor);
      }
      pixels.show();
    }
  } else if (Mode == MODO_AUTO) {
    // Modalità AUTO: LED individuali gestiti dal meteo, anello con colore di base
    // L'animazione è già gestita nello switch sopra
  }
  
  // Applica sempre la luminosità corrente
  pixels.setBrightness(savedNeopixelBrightness);
  pixels.show();
}

void checkNightMode() {
  static bool nightModeActive = false;

  if (!nightModeEnabled) {
    if (nightModeActive) {
      Currentmode = savedMode;
      nightModeActive = false;
      SetRingColor(Currentmode, false); // Ripristina la modalità precedente senza animazione
      Serial.println("❌ Modalità notturna disattivata manualmente. Ripristino modalità precedente.");
    }
    return;
  }

  timeClient.update();
  time_t now = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&now);
  int currentMinutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
  int startMinutes = nightStartHour * 60 + nightStartMinute;
  int endMinutes = nightEndHour * 60 + nightEndMinute;

  bool isNight;
  if (startMinutes < endMinutes) {
    isNight = (currentMinutes >= startMinutes && currentMinutes < endMinutes);
  } else {
    isNight = (currentMinutes >= startMinutes || currentMinutes < endMinutes);
  }

  if (isNight && !nightModeActive) {
    if (Currentmode != MODO_NOTTE) {
      savedMode = Currentmode;
      Currentmode = MODO_NOTTE;
      nightModeActive = true;

      // Spegni tutto immediatamente
      AllLEDoff();
      windAnimationActive = false;
      windScanning = false;
      pixels.clear();
      pixels.show();

      Serial.println("🌙 Entrata in modalità notte: TUTTI i LED spenti, solo display attivo");
    }
  } else if (!isNight && nightModeActive) {
    Currentmode = savedMode;
    nightModeActive = false;
    
    SetRingColor(Currentmode, true); // MODIFICATO: Con animazione al ripristino
    Serial.println("🌅 Uscita dalla modalità notte. Ripristino modalità precedente.");
  }
}

// --- IMPLEMENTAZIONE GESTORI SERVER WEB STANDARD ---

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  // CORREZIONE: Aggiunto charset UTF-8 per le emoji
  html += "<meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>" + ProductName + " Config</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #000000; color: #ffffff; line-height: 1.6; padding: 20px; min-height: 100vh; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: #1a1a1a; border-radius: 20px; box-shadow: 0 15px 35px rgba(0, 0, 0, 0.8); overflow: hidden; }";
  html += ".header { background: linear-gradient(135deg, #2c3e50 0%, #3498db 100%); color: white; padding: 30px 20px; text-align: center; }";
  html += ".header h2 { font-size: 2.2em; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.2); }";
  html += ".header p { font-size: 1.1em; opacity: 0.9; }";
  html += ".content { padding: 30px; }";
  html += ".weather-card { background: linear-gradient(135deg, #34495e 0%, #2c3e50 100%); border-radius: 15px; padding: 25px; margin-bottom: 25px; box-shadow: 0 8px 25px rgba(0,0,0,0.5); }";
  html += ".weather-card h3 { color: #ffffff; margin-bottom: 15px; font-size: 1.4em; border-bottom: 2px solid rgba(255,255,255,0.3); padding-bottom: 8px; }";
  html += ".weather-info { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }";
  html += ".weather-item { background: rgba(255,255,255,0.1); padding: 12px; border-radius: 10px; text-align: center; }";
  html += ".weather-label { font-weight: bold; color: #ffffff; font-size: 0.9em; }";
  html += ".weather-value { font-size: 1.2em; color: #ffffff; font-weight: bold; margin-top: 5px; }";
  html += ".config-section { background: linear-gradient(135deg, #34495e 0%, #2c3e50 100%); border-radius: 15px; padding: 25px; margin-bottom: 25px; box-shadow: 0 8px 25px rgba(0,0,0,0.5); }";
  html += ".config-section h3 { color: #ffffff; margin-bottom: 20px; font-size: 1.4em; border-bottom: 2px solid rgba(255,255,255,0.3); padding-bottom: 8px; }";
  html += ".form-group { margin-bottom: 20px; }";
  html += "label { display: block; margin-bottom: 8px; font-weight: 600; color: #ffffff; }";
  html += "input[type=text], input[type=time], select { width: 100%; padding: 12px 15px; border: 2px solid #555; border-radius: 10px; font-size: 16px; transition: all 0.3s ease; background: #333333; color: #ffffff; }";
  html += "input[type=text]:focus, input[type=time]:focus, select:focus { border-color: #3498db; outline: none; box-shadow: 0 0 0 3px rgba(52, 152, 219, 0.3); }";
  html += "input[type=range] { width: 100%; height: 8px; border-radius: 5px; background: #555; outline: none; margin: 15px 0; }";
  html += "input[type=range]::-webkit-slider-thumb { appearance: none; width: 22px; height: 22px; border-radius: 50%; background: #3498db; cursor: pointer; box-shadow: 0 2px 6px rgba(0,0,0,0.2); }";
  html += ".range-value { text-align: center; font-weight: bold; color: #3498db; font-size: 1.2em; margin-top: -10px; }";
  html += ".toggle-switch { position: relative; display: inline-block; width: 60px; height: 34px; margin-left: 15px; }";
  html += ".toggle-switch input { opacity: 0; width: 0; height: 0; }";
  html += ".slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #555; transition: .4s; border-radius: 34px; }";
  html += ".slider:before { position: absolute; content: \"\"; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }";
  html += "input:checked + .slider { background: linear-gradient(135deg, #2c3e50 0%, #3498db 100%); }";
  html += "input:checked + .slider:before { transform: translateX(26px); }";
  html += ".toggle-label { display: flex; align-items: center; margin-bottom: 15px; }";
  html += "button { background: linear-gradient(135deg, #2c3e50 0%, #3498db 100%); color: white; border: none; padding: 15px 30px; border-radius: 50px; font-size: 18px; font-weight: bold; cursor: pointer; transition: all 0.3s ease; box-shadow: 0 5px 15px rgba(0,0,0,0.2); width: 100%; margin-top: 10px; }";
  html += "button:hover { transform: translateY(-2px); box-shadow: 0 8px 20px rgba(0,0,0,0.3); background: linear-gradient(135deg, #3498db 0%, #2c3e50 100%); }";
  html += ".form-row { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }";
  html += "@media (max-width: 768px) { .form-row { grid-template-columns: 1fr; } }";
  html += "small { display: block; margin-top: 5px; font-size: 0.85em; color: rgba(255,255,255,0.6); }";
  html += "</style>";
  html += "</head><body>";
  
  html += "<div class=\"container\">";
  html += "<div class=\"header\">";
  // CORREZIONE: Emoji sostituita con una più compatibile
  html += "<h2>⛅ " + ProductName + "</h2>";
  html += "<p>Configurazione Sistema Meteo</p>";
  html += "</div>";
  
  html += "<div class=\"content\">";
  
  // Sezione Informazioni Meteo Corrente
  html += "<div class=\"weather-card\">";
  html += "<h3>📊 Informazioni Meteo Corrente</h3>";
  html += "<div class=\"weather-info\">";
  html += "<div class=\"weather-item\"><div class=\"weather-label\">🌡️ Temperatura</div><div class=\"weather-value\">" + String(temperatureCelsius, 1) + " °C</div></div>";
  html += "<div class=\"weather-item\"><div class=\"weather-label\">💧 Umidità</div><div class=\"weather-value\">" + String(humidity) + " %</div></div>";
  html += "<div class=\"weather-item\"><div class=\"weather-label\">💨 Vento</div><div class=\"weather-value\">" + String(windSpeed, 1) + " km/h</div></div>";
  html += "<div class=\"weather-item\"><div class=\"weather-label\">🧭 Direzione</div><div class=\"weather-value\">" + String(windDirection) + "°</div></div>";
  html += "<div class=\"weather-item\"><div class=\"weather-label\">☁️ Condizioni</div><div class=\"weather-value\">" + getWeatherDescriptionItalian(weatherConditionCode) + "</div></div>";
  html += "<div class=\"weather-item\"><div class=\"weather-label\">📍 Città</div><div class=\"weather-value\">" + String(savedCity) + "</div></div>";
  html += "</div>";
  html += "</div>";
  
  html += "<form action=\"/save\" method=\"post\">";
  
  // Sezione Configurazione Base
  html += "<div class=\"config-section\">";
  html += "<h3>⚙️ Configurazione Base</h3>";
  
  html += "<div class=\"form-group\">";
  html += "<label for=\"brightness\">💡 Luminosità LED (" + String(savedNeopixelBrightness) + "):</label>";
  html += "<input type=\"range\" id=\"brightness\" name=\"brightness\" min=\"10\" max=\"255\" value=\"" + String(savedNeopixelBrightness) + "\" oninput=\"document.getElementById('brightnessValue').innerText = this.value\">";
  html += "<div class=\"range-value\" id=\"brightnessValue\">" + String(savedNeopixelBrightness) + "</div>";
  html += "</div>";
  
  html += "<div class=\"form-group\">";
  html += "<label for=\"mode_select\">🎨 Modalità Visualizzazione:</label>";
  html += "<select id=\"mode_select\" name=\"mode_select\">";
  String modes[] = {
    "Automatica", "Luce Rossa", "Luce Verde", "Luce Blu", "Luce Viola",
    "Luce Azzurra", "Luce Gialla", "Luce Bianca", "Anello Spento",
    "LED Spenti", "Tutto Spento", "Modalità Notte"
  };
  for (int i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
    html += "<option value=\"" + String(i) + "\"" + (Currentmode == i ? " selected" : "") + ">" + modes[i] + "</option>";
  }
  html += "</select>";
  html += "</div>";

  html += "<div class=\"toggle-label\">";
  html += "<label for=\"wind_anim\">💨 Abilita Animazione Vento (Anello NeoPixel):</label>";
  html += "<label class=\"toggle-switch\">";
  html += String("<input type=\"checkbox\" id=\"wind_anim\" name=\"wind_anim\" ") + (windAnimationEnabled ? "checked" : "") + ">";
  html += "<span class=\"slider\"></span>";
  html += "</label>";
  html += "</div>";

  // Nuovo toggle per animazione cambio modalità
  html += "<div class=\"toggle-label\">";
  html += "<label for=\"neopixel_anim\">✨ Abilita Animazione Cambio Modalità:</label>";
  html += "<label class=\"toggle-switch\">";
  html += String("<input type=\"checkbox\" id=\"neopixel_anim\" name=\"neopixel_anim\" ") + (neopixelAnimationEnabled ? "checked" : "") + ">";
  html += "<span class=\"slider\"></span>";
  html += "</label>";
  html += "</div>";

  html += "</div>";

  // Sezione Configurazione Notturna
  html += "<div class=\"config-section\">";
  html += "<h3>🌙 Configurazione Notturna</h3>";
  
  html += "<div class=\"toggle-label\">";
  html += "<label for=\"night_mode\">Attiva Modalità Notturna:</label>";
  html += "<label class=\"toggle-switch\">";
  html += String("<input type=\"checkbox\" id=\"night_mode\" name=\"night_mode\" ") + (nightModeEnabled ? "checked" : "") + ">";
  html += "<span class=\"slider\"></span>";
  html += "</label>";
  html += "</div>";
  
  html += "<div class=\"form-row\">";
  html += "<div class=\"form-group\">";
  html += "<label for=\"night_start\">Ora Inizio:</label>";
  html += String("<input type=\"time\" id=\"night_start\" name=\"night_start\" value=\"") + (nightStartHour < 10 ? "0" : "") + String(nightStartHour) + ":" + (nightStartMinute < 10 ? "0" : "") + String(nightStartMinute) + "\">";
  html += "</div>";
  
  html += "<div class=\"form-group\">";
  html += "<label for=\"night_end\">Ora Fine:</label>";
  html += String("<input type=\"time\" id=\"night_end\" name=\"night_end\" value=\"") + (nightEndHour < 10 ? "0" : "") + String(nightEndHour) + ":" + (nightEndMinute < 10 ? "0" : "") + String(nightEndMinute) + "\">";
  html += "</div>";
  html += "</div>";
  
  html += "</div>";
  
  // Sezione Località e Previsioni
  html += "<div class=\"config-section\">";
  html += "<h3>📍 Località e Previsioni</h3>";
  
  html += "<div class=\"form-group\">";
  html += "<label for=\"city_select\">Seleziona Città:</label>";
  html += "<select id=\"city_select\" name=\"city_select\">";
  html += "<option value=\"\">-- Seleziona --</option>";
  String cities[] = {
    "Agrigento", "Alessandria", "Ancona", "Aosta", "L'Aquila", "Arezzo", "Ascoli Piceno", "Asti", "Avellino", "Bari",
    "Barletta-Andria-Trani", "Belluno", "Benevento", "Bergamo", "Biella", "Bologna", "Bolzano", "Brescia", "Brindisi", "Cagliari",
    "Caltanissetta", "Campobasso", "Caserta", "Catania", "Catanzaro", "Chieti", "Como", "Cosenza", "Cremona", "Crotone",
    "Cuneo", "Enna", "Fermo", "Ferrara", "Firenze", "Foggia", "Forlì-Cesena", "Frosinone", "Genova", "Gorizia",
    "Grosseto", "Imperia", "Isernia", "La Spezia", "Latina", "Lecce", "Lecco", "Livorno", "Lodi", "Lucca",
    "Macerata", "Mantova", "Massa-Carrara", "Matera", "Messina", "Milano", "Modena", "Monza", "Napoli", "Novara",
    "Nuoro", "Oristano", "Padova", "Palermo", "Parma", "Pavia", "Perugia", "Pesaro e Urbino", "Pescara", "Piacenza",
    "Pisa", "Pistoia", "Pordenone", "Potenza", "Prato", "Ragusa", "Ravenna", "Reggio Calabria", "Reggio Emilia", "Rieti",
    "Rimini", "Roma", "Rovigo", "Sabaudia", "Salerno", "Sassari", "Savona", "Siena", "Siracusa", "Sondrio",
    "Taranto", "Taverna", "Teramo", "Terni", "Torino", "Trapani", "Trento", "Treviso", "Trieste", "Udine",
    "Varese", "Venezia", "Verbania", "Vercelli", "Verona", "Vibo Valentia", "Vicenza", "Viterbo"
  };
  for (int i = 0; i < sizeof(cities) / sizeof(cities[0]); i++) {
    html += "<option value=\"" + cities[i] + "\"" + (String(savedCity).equalsIgnoreCase(cities[i]) ? " selected" : "") + ">" + cities[i] + "</option>";
  }
  html += "</select>";
  html += "</div>";
  
  html += "<div class=\"form-group\">";
  html += "<label for=\"city_manual\">Inserisci Città Manualmente:</label>";
  html += "<input type=\"text\" id=\"city_manual\" name=\"city_manual\" value=\"" + String(savedCity) + "\" placeholder=\"Es: Milano, Torino, Napoli...\">";
  html += "</div>";
  
  html += "<div class=\"form-row\">";
  html += "<div class=\"form-group\">";
  html += "<label for=\"forecastDay\">📅 Giorno Previsione:</label>";
  html += "<select id=\"forecastDay\" name=\"forecastDay\">";
  String giorni[] = { "Attuale", "Oggi", "Domani", "Dopodomani", "Giorno 4", "Giorno 5" };
  for (int i = -1; i < 5; i++) {
    html += "<option value=\"" + String(i) + "\"" + (forecastDay == i ? " selected" : "") + ">" + giorni[i + 1] + "</option>";
  }
  html += "</select>";
  html += "</div>";
  
  html += "<div class=\"form-group\">";
  html += "<label for=\"forecastPeriod\">⏰ Periodo Giorno:</label>";
  html += "<select id=\"forecastPeriod\" name=\"forecastPeriod\">";
  String periodi[] = { "Mattino (06-12)", "Pomeriggio (12-18)", "Sera (18-00)" };
  for (int i = 0; i < 3; i++) {
    html += "<option value=\"" + String(i) + "\"" + (forecastPeriod == i ? " selected" : "") + ">" + periodi[i] + "</option>";
  }
  html += "</select>";
  html += "</div>";
  html += "</div>";
  
  html += "</div>";

  html += "<div class=\"config-section\">";
  html += "<h3>🔑 Configurazione API</h3>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"api_key\">OpenWeather API Key:</label>";

  // Recupera l'API key corrente
  preferences.begin("meteo-config", false);
  String currentApiKey = preferences.getString("api_key", "");
  preferences.end();

  // Mostra l'API key mascherata se presente
  String displayApiKey = currentApiKey;
  if (currentApiKey.length() > 8) {
    displayApiKey = currentApiKey.substring(0, 4) + "****" + currentApiKey.substring(currentApiKey.length() - 4);
  }

  html += "<input type=\"text\" id=\"api_key\" name=\"api_key\" value=\"" + currentApiKey + "\" placeholder=\"Inserisci la tua API key di OpenWeatherMap\">";

  // Messaggio di stato API key
  if (currentApiKey.length() > 0) {
    html += "<small style=\"color: #4CAF50; display: block; margin-top: 5px;\">✓ API key configurata</small>";
  } else {
    html += "<small style=\"color: #ff6b6b; display: block; margin-top: 5px;\">⚠ Nessuna API key configurata - Inseriscila per ricevere i dati meteo</small>";
  }
  html += "<small style=\"color: #888; display: block; margin-top: 3px;\">Ottieni la tua API key gratuita su: <a href=\"https://openweathermap.org/api\" target=\"_blank\" style=\"color: #3498db;\">openweathermap.org/api</a></small>";
  html += "</div>";
  html += "</div>";

  html += "<button type=\"submit\">💾 SALVA IMPOSTAZIONI</button>";
  html += "</form>";
  
  html += "</div>";
  html += "</div>";
  
  html += "<script>";
  html += "var cityManual = document.getElementById('city_manual');";
  html += "var citySelect = document.getElementById('city_select');";
  html += "citySelect.addEventListener('change', function() {";
  html += "  if (this.value !== '') { cityManual.value = ''; }";
  html += "});";
  html += "cityManual.addEventListener('input', function() {";
  html += "  if (this.value.trim() !== '') { citySelect.selectedIndex = 0; }";
  html += "});";
  html += "var forecastDaySelect = document.getElementById('forecastDay');";
  html += "var forecastPeriodSelect = document.getElementById('forecastPeriod');";
  html += "function updateForecastPeriod() {";
  html += "  if (forecastDaySelect.value == '-1') {";
  html += "    forecastPeriodSelect.disabled = true;";
  html += "    forecastPeriodSelect.style.opacity = '0.5';";
  html += "  } else {";
  html += "    forecastPeriodSelect.disabled = false;";
  html += "    forecastPeriodSelect.style.opacity = '1';";
  html += "  }";
  html += "}";
  html += "forecastDaySelect.addEventListener('change', updateForecastPeriod);";
  html += "updateForecastPeriod();";
  html += "</script>";
  
  html += "</body></html>";
  
  // CORREZIONE: Specificato charset UTF-8 nella risposta
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSave() {
  for(uint8_t i=0; i < server.args(); i++ ){
    Serial.print("Arg ");
    Serial.print(server.argName(i));
    Serial.print("=");
    Serial.println(server.arg(i));
  }

  if (server.hasArg("brightness")) {
    savedNeopixelBrightness = server.arg("brightness").toInt();
    pixels.setBrightness(savedNeopixelBrightness);
    pixels.show();
  }

  nightModeEnabled = (server.hasArg("night_mode") && server.arg("night_mode") == "on");

  bool oldWindAnimationEnabled = windAnimationEnabled;
  windAnimationEnabled = (server.hasArg("wind_anim") && server.arg("wind_anim") == "on");

  // Se l'animazione del vento viene disabilitata, ferma le animazioni in corso
  if (oldWindAnimationEnabled && !windAnimationEnabled) {
    windScanning = false;
    windAnimationActive = false;
    Serial.println("Animazione vento disabilitata - Fermando animazioni in corso");
  }

  // Gestione nuova variabile animazione cambio modalità
  bool oldNeopixelAnimationEnabled = neopixelAnimationEnabled;
  neopixelAnimationEnabled = (server.hasArg("neopixel_anim") && server.arg("neopixel_anim") == "on");

  if (server.hasArg("night_start")) {
    String startTime = server.arg("night_start");
    int sep = startTime.indexOf(':');
    if (sep != -1) {
      nightStartHour = startTime.substring(0, sep).toInt();
      nightStartMinute = startTime.substring(sep + 1).toInt();
    }
  }

  if (server.hasArg("night_end")) {
    String endTime = server.arg("night_end");
    int sep = endTime.indexOf(':');
    if (sep != -1) {
      nightEndHour = endTime.substring(0, sep).toInt();
      nightEndMinute = endTime.substring(sep + 1).toInt();
    }
  }

  Serial.print("Inizio notte: ");
  Serial.print(nightStartHour);
  Serial.print(":");
  Serial.println(nightStartMinute);

  Serial.print("Fine notte: ");
  Serial.print(nightEndHour);
  Serial.print(":");
  Serial.println(nightEndMinute);

  String modes[] = {
    "Automatica", "Luce rossa", "Luce Verde", "Luce Blu", "Luce Viola", "Luce Azzurra", "Luce Gialla", "Luce Bianca", "Anello Spento", "LED Spenti", "Tutto Spento", "Modalità Notte"
  };

  if (server.hasArg("mode_select")) {
    int selectedModeInt = server.arg("mode_select").toInt();

    if (selectedModeInt >= 0 && selectedModeInt <= MAX_MODE) {
      if (selectedModeInt != Currentmode) {
        Currentmode = selectedModeInt;

        // MODIFICATO: Mostra il nome della modalità per 2 secondi
        showModeName = true;
        modeNameStartTime = millis();
        showText = true;
        DisplayOneTime = false;

        SetRingColor(Currentmode, neopixelAnimationEnabled);
        // Forza aggiornamento immediato quando si cambia modalità
        forceWeatherUpdate = true;
      }
    }
  }

  String cityFromSelector = server.arg("city_select");
  String cityFromManualInput = server.arg("city_manual");
  String cityToSave = "";

  if (!cityFromManualInput.isEmpty()) {
    cityToSave = cityFromManualInput;
  } else {
    cityToSave = cityFromSelector;
  }

  bool cityChanged = false;
  if (String(savedCity) != cityToSave) {
    strncpy(savedCity, cityToSave.c_str(), sizeof(savedCity) - 1);
    savedCity[sizeof(savedCity) - 1] = '\0';
    cityChanged = true;
    Serial.println("Città cambiata: " + String(savedCity));
  }

  int oldForecastDay = forecastDay;
  if (server.hasArg("forecastDay")) {
    forecastDay = server.arg("forecastDay").toInt();
    if (forecastDay < -1 || forecastDay > 4) forecastDay = -1; // Se non valido, default a Attuale
  }

  int oldForecastPeriod = forecastPeriod;
  if (server.hasArg("forecastPeriod")) {
    forecastPeriod = server.arg("forecastPeriod").toInt();
    if (forecastPeriod < 0 || forecastPeriod > 2) forecastPeriod = 0;
  }

  if (forecastDay != oldForecastDay || forecastPeriod != oldForecastPeriod || cityChanged) {
    Serial.println("Giorno/periodo previsione cambiato o città modificata - Forzo aggiornamento immediato");
    forceWeatherUpdate = true;
    showText = true;
    DisplayOneTime = false;
  }

  if (server.hasArg("api_key")) {
    String newApiKey = server.arg("api_key");
    newApiKey.trim(); // Rimuovi spazi iniziali/finali
    
    preferences.begin("meteo-config", false);
    String oldApiKey = preferences.getString("api_key", "");
    
    if (newApiKey.length() > 0) {
      // API key inserita - salva
      if (newApiKey != oldApiKey) {
        preferences.putString("api_key", newApiKey);
        Serial.println("✅ API key aggiornata dalla pagina web: " + newApiKey);
        
        // Forza aggiornamento meteo con la nuova API key
        forceWeatherUpdate = true;
        showText = true;
        DisplayOneTime = false;
      } else {
        Serial.println("ℹ️ API key invariata");
      }
    } else {
      // Campo vuoto - rimuovi API key
      if (oldApiKey.length() > 0) {
        preferences.remove("api_key");
        Serial.println("🗑️ API key rimossa dalla configurazione");
        
        // Imposta messaggio di errore sul display
        weatherDescription = "API Key mancante";
        temperatureCelsius = 0.0;
        humidity = 0;
        windSpeed = 0.0;
        windDirection = 0;
        weatherConditionCode = 800;
        
        showText = true;
        DisplayOneTime = false;
      } else {
        Serial.println("ℹ️ Nessuna API key da rimuovere (già assente)");
      }
    }
    preferences.end();
  }


  saveConfig();

  preferences.begin("meteo-config",false);
  bool test = preferences.getBool("nightModeEnabled",false);
  preferences.end();
  Serial.print("nightmodeenabled salvato: ");
  Serial.println(test ? "TRUE":"FALSE");

  Serial.println("Valori salvati:");
  Serial.println(" - Brightness: " + String(savedNeopixelBrightness));
  Serial.println(" - Modalità: " + String(Currentmode));
  Serial.println(" - Città: " + String(savedCity));
  Serial.println(" - Forecast Day: " + String(forecastDay) + " (-1:Attuale, 0:Oggi, 1:Domani, 2:Dopodomani, 3:Giorno4, 4:Giorno5)");
  Serial.println(" - Forecast Period: " + String(forecastPeriod));
  Serial.println(" - Night Mode: " + String(nightModeEnabled));
  Serial.println(" - Notte: " + String(nightStartHour) + ":" + String(nightStartMinute) + " -> " + String(nightEndHour) + ":" + String(nightEndMinute));
  Serial.println(" - Wind Animation: " + String(windAnimationEnabled));
  Serial.println(" - NeoPixel Animation: " + String(neopixelAnimationEnabled)); // Nuova variabile

  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
  
  delay(100);
  
  if (forceWeatherUpdate) {
    Serial.println("Eseguendo aggiornamento meteo immediato...");
    fetchWeather();
    forceWeatherUpdate = false;
  }
}

String urlEncode(const String& str) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}