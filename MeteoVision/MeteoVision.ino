// Meteovision 1.0 by Davide Gatti - 2025 - www.survivalhacking.it
//
// Da un'idea di Roberto Proli e l'aiuto di MatixVision
//
// Stazione meteo visuale, con icone illuminate che indicano la previsione.
// Si connette via internet mediante WIFI da dove ricava tempo e previsioni da openweather
// E' necessario ottenere da openweather una APY KEY gratuita, iscrivendosi e seguendo le istruzioni nel file secret.h
// Una volta connessi, vi verrà indicato a display l'indirizzo IP da raggiungere con un browser
// Al fine di poter impostare la città di vostro interesse e altri parametri.
//
// Il pulsante consente di commutare in diverse modalità:
// - AUTO = Modalità automatica che mostra le informazioni meteo sia a display che con le icone e l'anello si accende con colori legati al meteo.
// - COLOR VARI = L'anello si accende fisso del colore indicato, in queste modalità l'anello si usa come illuminazione
// - ANELLO SP. = L'anello rimane spento e funziona solo il display e le icone
// - LED SPENTI = Tutti i LED e le icone sono spenti, si vede solo il display
// - SPENTO = Tutto spento
//
// Tenendo premuto a lungo il tasto è possibile regolare la luminosità
//
// Tenendo premuto il pulsante durante l'accensione del dispositivo verranno cancellate le impostazioni WIFI
//

// ---------------------------- ENG ---------------------------
// From an idea of Roberto Proli and the help of MatixVision
// 
// Visual weather station, with illuminated icons indicating the forecast.
// Connects via the internet via WIFI from where it gets weather and forecasts from openweather
// You need to get a free APY KEY from openweather by signing up and following the instructions in the secret.h file
// Once connected, you will be shown on the display the IP address to reach with a browser
// In order to be able to set the city of your interest and other parameters.
//
// The button allows you to switch to different modes:
// - AUTO = Automatic mode that shows the weather information both on the display and with icons and the ring lights up with colors related to the weather.
// - VARIOUS COLOR = The ring lights up fixed of the indicated color, in these modes the ring is used as lighting
// - ANELLO SP. = The ring remains off and only the display and icons work
// - LED SPENTI = All LEDs and icons are off, only the display can be seen
// - SPENTO = All off
//
// Holding the button down for a long time allows you to adjust the brightness
//
// Holding the button down while turning the device on will clear the WIFI settings
//


#include <Arduino.h>
#include <Wire.h>  
#include <Adafruit_GFX.h>       // Libreria grafica per display OLED
#include <Adafruit_SSD1306.h>   // Libreria per SSD1306 modificata per il supporto mirroring
#include <Adafruit_NeoPixel.h>  // Controllo anello LED WS2812

// Gestione connessione WiFi automatica
#include <WiFiManager.h>        // Permette setup Wi-Fi via interfaccia web

// Richieste HTTP per ottenere i dati meteo
#include <HTTPClient.h>
#include <ArduinoJson.h>        // Parsing JSON della risposta OpenWeather

#include "graphics.h"           // Contiene animazioni bitmap per il display

// -- LIBRERIE PER GESTIONE TEMPO --
// Tempo standard C
#include <ctime>                // Include strutture come time_t, struct tm
#include <time.h>               // Funzioni di conversione ora/data
#include <sys/time.h>           // Per strutture temporali compatibili con ESP32

// Librerie NTP (Network Time Protocol)
#include <NTPClient.h>          // Permette sincronizzazione ora via Internet
#include <WiFiUdp.h>            // Necessario per comunicazione UDP usata da NTPClient

// Librerie custom per server web
#include <WebServer.h>          // Server HTTP in esecuzione su ESP32
#include <Preferences.h>        // Per salvare configurazioni persistenti

#include "secret.h"             // File con chiavi/API sensibili

// -- DEFINIZIONI HARDWARE DISPLAY OLED --
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_ADDR 0x3C          // Indirizzo I2C predefinito per display SSD1306
#define OLED_RESET -1           // Se il pin RESET non è collegato

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
#define NUM_NEOPIXELS 24     // Numero LED sull’anello
Adafruit_NeoPixel pixels(NUM_NEOPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// -- DEFINIZIONE PULSANTE FISICO --
#define PIN_PULSANTE 10      // Pulsante per cambiare modalità/luminosità

// -- DEFINIZIONI PER TEMPISMO DI SCHERMATA E METEO --
#define SCREEN_TEXT_DURATION 5000              // Tempo visualizzazione info (ms)
#define SCREEN_ANIMATION_DURATION 10000        // Tempo visualizzazione animazione (ms)
#define WEATHER_FETCH_INTERVAL 10 * 60 * 1000  // Ogni 10 minuti (in ms)

// Parametri effetto “respiro” LED (NeoPixel)
#define NEOPIXEL_BREATH_MIN 25                 // Luminosità minima effetto respiro
#define NEOPIXEL_BREATH_MAX 255                // Luminosità massima
#define BREATH_UPDATE_SPEED 5                  // Velocità animazione
#define NEOPIXEL_BOOT_ANIM_DELAY 50            // Delay tra accensioni LED iniziali

// Variabili per la gestione del pulsante
unsigned long buttonPressStartTime = 0;        // Tempo d'inizio pressione
unsigned long buttonReleaseTime = 0;           // Tempo rilascio
bool buttonState = HIGH;                       // Stato attuale del pulsante (non premuto di default)
bool lastButtonState = HIGH;                   // Stato precedente, per debounce
unsigned long lastDebounceTime = 0;            // Ultimo cambio stabile dello stato
const unsigned long debounceDelay = 50;        // Ritardo debounce in ms
bool keypressed = false;                       // Flag se il pulsante è stato premuto

// Variabili per pressione lunga del pulsante
bool isLongPressMode = false;                     // Flag per modalità di regolazione luminosità
const unsigned long LONG_PRESS_THRESHOLD = 1000;  // Soglia pressione lunga (1s)
unsigned long lastScreenChange = 0;
bool showText = true;                             // Alterna tra testo e animazione

// Variabile per velocità cambio luminosità
const int BRIGHTNESS_CHANGE_INTERVAL = 20;        // Ogni quanto aggiornare luminosità (ms)
unsigned long lastBrightnessChangeTime = 0;

// -- VARIABILI DI CONFIGURAZIONE PERSISTENTE --
Preferences preferences;                          // Oggetto per salvare impostazioni in flash

// Valori persistenti da conservare
char savedCity[40] = "Monza";                     // Città di default
char savedCountryCode[5] = "IT";                  // Codice paese (IT → Italia)
int savedNeopixelBrightness = 10;                 // Luminosità LED predefinita
int savedMode = 0;                                // Modalità predefinita
bool DisplayOneTime = false;                      // Flag per evitare aggiornamenti duplicati del display
int Currentmode = 0;                              // Modalità corrente (auto, colori fissi ecc.)
#define MAX_MODE 10                               // Numero massimo di modalità definite

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


// -- VARIABILI GLOBALI NON SALVATE IN MEMORIA --
String ProductName = "MeteoVision";        // Nome identificativo del dispositivo
String weatherDescription = "Ricerca...";  // Descrizione del meteo attuale (inizializzazione)
int weatherConditionCode = 0;              // Codice numerico condizione meteo
float temperatureCelsius = 0.0;            // Temperatura corrente (gradi Celsius)
time_t sunriseTime = 0;                    // Orario alba (timestamp UNIX)
time_t sunsetTime = 0;                     // Orario tramonto (timestamp UNIX)

// -- GESTIONE RETE E SERVER WEB --
WiFiManager wm;                // Oggetto per gestione automatica WiFi (e portale captive)
WebServer server(80);          // Istanza di web server HTTP sulla porta 80

// -- CLIENT NTP PER L'ORARIO DI SISTEMA --
WiFiUDP ntpUDP;  // UDP socket (necessario per NTPClient)
// L'offset del NTPClient deve essere 0 dopo aver configurato il fuso orario con configTime
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // Offset 0, configTime gestirà il fuso orario

// -- VARIABILI PER ANIMAZIONE DELL'ANELLO NEOPIXEL --
bool neopixelAnimationActive = true;                  // Flag animazione attiva
uint32_t neopixelBaseColor = pixels.Color(0, 0, 0);   // Colore di base corrente (RGB packed)

// Flag che indica se è in corso l'animazione di accensione all'avvio
bool neopixelBootAnimationInProgress = false;


// --- Mappatura traduzioni OpenWeatherMap (da codice numerico a descrizione ITA) ---
String getWeatherDescriptionItalian(int conditionCode) {
  // Mappa intervalli di codice alle descrizioni corrispondenti
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

  return "Sconosciuto";  // Default se codice non mappato
}

// --- PROTOTIPI DI FUNZIONI ---
// Sono dichiarati qui per poterli usare ovunque
void saveConfig();
void loadConfig();
void saveConfigCallback();
void configModeCallback(WiFiManager *myWiFiManager);
void fetchWeather();
void displayWeatherInfo();
void displayWeatherAnimation();
void updateWeatherLEDs();
void runNeopixelAnimation();
void setNeopixelColor(uint32_t color);
void animateNeopixelCircular();
void handleButtonPress();
void onGetCommand(unsigned char device_id, const char *device_name, bool state, unsigned char value);
time_t getNtpTime();         // Wrapper per ottenere ora da NTPClient
void AllLEDoff();            // Spegne tutti i LED fisici di stato (non NeoPixel)
void SetRingColor(int mode); // Imposta il colore dell'anello NeoPixel in base alla modalità selezionata


// Gestori HTTP per il server web integrato
void handleRoot();  // Serve la homepage
void handleSave();  // Gestisce POST per salvare impostazioni

// Stato per l’animazione display
unsigned long lastFrameChange = 0;  // Per calcolare tempo tra i frame
int currentFrame = 0;               // Frame attuale per animazioni
bool dualAnimation = false;         // Quando serve combinare due animazioni
static int animationDirection = 1;  // 1 = avanti, -1 = indietro
const int ANIM_FRAME_DELAY = 200;   // Delay tra frame di animazione (ms)

// Wrapper semplificato per ottenere l’epoch time dal client NTP
time_t getNtpTime() {
  return timeClient.getEpochTime();  // Restituisce timestamp attuale
}

// --- Setup ---
void setup() {
  Serial.begin(115200);  // Avvia la comunicazione seriale per debug

  // Inizializza la memoria non volatile (NVS) per le impostazioni
  preferences.begin("meteo-config", false);  // "meteo-config" è il namespace personalizzato

  loadConfig();  // Carica le configurazioni salvate in precedenza

  Wire.begin();  // Inizializza la comunicazione I2C (usata da display)

  // Inizializza il display SSD1306 e verifica che sia stato allocato correttamente
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);  // Entra in loop infinito se fallisce l'inizializzazione
  }

  delay(100);  // Breve attesa prima di mostrare il logo
  display.clearDisplay();
  display.drawBitmap((128 - logo_width) / 2, (32 - logo_height) / 2, logo_data, logo_width, logo_height, 1);
  display.display();  // Visualizza il logo "Survival Hacking" all’avvio

  // Imposta tutti i pin LED meteo come OUTPUT
  pinMode(PIN_LED_SOLE, OUTPUT);
  pinMode(PIN_LED_NUVOLE, OUTPUT);
  pinMode(PIN_LED_PIOGGIA, OUTPUT);
  pinMode(PIN_LED_TEMPORALE, OUTPUT);
  pinMode(PIN_LED_LUNA, OUTPUT);
  AllLEDoff();  // Spegne tutti i LED all’avvio

  // Inizializza l’anello NeoPixel
  pixels.begin();
  pixels.setBrightness(savedNeopixelBrightness);  // Applica la luminosità salvata
  pixels.show();  // Assicura che l’anello sia spento inizialmente

  // Inizializza il pin del pulsante con resistenza pull-up interna
  pinMode(PIN_PULSANTE, INPUT_PULLUP);

  // Se il pulsante è tenuto premuto all'avvio → reset Wi-Fi e configurazioni
  if (digitalRead(PIN_PULSANTE) == LOW) {
    Serial.println("Reset WiFi e configurazione.");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Reset WiFi e Config...");
    display.display();
    delay(1000);

    preferences.clear();    // Elimina configurazioni salvate
    wm.resetSettings();     // Elimina reti WiFi salvate
    ESP.restart();          // Riavvia il microcontrollore
  }


    // --- Test visuale dei LED Meteo e dell’anello LED ---
  digitalWrite(PIN_LED_SOLE, HIGH);
  ActivateCircular(pixels.Color(125, 125, 0));  // Giallo
  delay(500);

  digitalWrite(PIN_LED_LUNA, HIGH);
  ActivateCircular(pixels.Color(125, 125, 125));  // Grigio chiaro
  delay(500);

  digitalWrite(PIN_LED_NUVOLE, HIGH);
  ActivateCircular(pixels.Color(125, 125, 125));
  delay(500);

  digitalWrite(PIN_LED_PIOGGIA, HIGH);
  ActivateCircular(pixels.Color(0, 0, 125));  // Blu
  delay(500);

  // Effetto lampeggio per LED TEMPORALE
  for (int r = 0; r < 6; r++) {
    digitalWrite(PIN_LED_TEMPORALE, LOW);
    ActivateCircular(pixels.Color(0, 0, 0));     // Spegne anello
    delay(100);
    digitalWrite(PIN_LED_TEMPORALE, HIGH);
    ActivateCircular(pixels.Color(197, 0, 255));  // Viola acceso
    delay(100);
  }

  delay(1000);  // Pausa finale prima di spegnere
  ActivateCircular(pixels.Color(0, 0, 0));  // Spegne anello
  AllLEDoff();  // Spegne tutti i LED meteo

  // --- Setup WiFiManager ---
  wm.setAPCallback(configModeCallback);          // Callback quando entra in modalità Access Point
  wm.setSaveConfigCallback(saveConfigCallback);  // Callback dopo salvataggio config

  // Aggiunge un parametro personalizzato per inserire la API key
  WiFiManagerParameter custom_api_key("api_key", "OpenWeatherMap API Key", OPENWEATHER_API_KEY, 60);
  wm.addParameter(&custom_api_key);

  Serial.println("\nConnessione Wi-Fi...");
  // autoConnect tenta di riconnettersi o avvia un captive portal (SSID, Password)
  if (!wm.autoConnect("MeteoVision", "password")) {
    Serial.println("Fallita connessione, riavvio...");
    delay(3000);
    ESP.restart();  // Riavvia se fallisce
  }

  Serial.println("Connesso!");
  Serial.print("IP del server web: ");
  Serial.println(WiFi.localIP());  // Stampa IP assegnato

  // --- Schermata di benvenuto ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("         BY");
  display.println("     DAVIDE GATTI");
  display.println("   Survival Hacking");
  display.display();
  delay(3000);

  // Mostra l’IP ottenuto dalla rete Wi-Fi
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connesso al WiFi!");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.println("Accedi al webserver.");
  display.display();
  delay(5000);  // Mostra messaggio per 5 secondi
  display.clearDisplay();  // Pulisce OLED

  // Salva in preferenze la API key inserita (dall’interfaccia web)
  char tempApiKey[60];
  strncpy(tempApiKey, custom_api_key.getValue(), sizeof(tempApiKey) - 1);
  tempApiKey[sizeof(tempApiKey) - 1] = '\0';
  preferences.putString("api_key", tempApiKey);  // Salva API

  saveConfig();  // Salva l'intera configurazione

  // Messaggi di debug
  Serial.print("Citta' configurata: ");
  Serial.println(savedCity);
  Serial.print("API Key usata: ");
  Serial.println(preferences.getString("api_key", "Errore"));
  Serial.print("Codice Paese (fisso IT): ");
  Serial.println(savedCountryCode);

  // --- Sincronizzazione oraria NTP con gestione ora legale ---
  // Configura il fuso orario per l'Italia (CET-1CEST, con regole ora legale)
  // 0, 0 indicano che l'offset GMT e l'offset per l'ora legale saranno gestiti dalla variabile TZ.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  // Imposta la variabile d'ambiente TZ per l'Italia.
  // CET-1CEST: Ora standard (Central European Time) -1h da UTC, Ora legale (Central European Summer Time).
  // M3.5.0: Inizio ora legale - ultima (5) domenica (0) di Marzo (3).
  // M10.5.0/3: Fine ora legale - ultima (5) domenica (0) di Ottobre (10) alle 3 del mattino.
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset(); // Applica la nuova configurazione del fuso orario

  timeClient.begin();     // Inizializza client NTP
  timeClient.update();    // Prima sincronizzazione ora

  // --- Setup server web ---
  server.on("/", HTTP_GET, handleRoot);       // Homepage
  server.on("/save", HTTP_POST, handleSave);  // Salva impostazioni
  server.begin();  // Avvia server

  fetchWeather();               // Ottiene subito i dati meteo iniziali
  SetRingColor(Currentmode);    // Imposta colore LED secondo modalità corrente
  lastScreenChange = millis();  // Imposta timer per visualizzazione
}


// --- Loop ---
void loop() {
  wm.process();             // Permette a WiFiManager di gestire eventuali richieste WiFi (es. captive portal)
  server.handleClient();    // Serve eventuali richieste HTTP per il webserver

  handleButtonPress();      // Controlla se il pulsante è stato premuto e agisce di conseguenza

  if (keypressed == false) {  // Solo se il pulsante non è attivamente premuto

    // Recupera il tempo attuale in millisecondi
    unsigned long currentTime = millis();

    // Alternanza tra testo e animazione sul display
    if (showText) {
      // Se il testo è stato mostrato abbastanza a lungo, passa ad animazione
      if (currentTime - lastScreenChange >= SCREEN_TEXT_DURATION) {
        showText = false;
        lastScreenChange = currentTime;
        display.clearDisplay();  // Pulisce il display per nuova animazione
        currentFrame = 0;        // Riparte da primo frame animazione
      }
    } else {
      // Se l’animazione è stata mostrata abbastanza, torna al testo
      if (currentTime - lastScreenChange >= SCREEN_ANIMATION_DURATION) {
        showText = true;
        lastScreenChange = currentTime;
        display.clearDisplay();  // Pulisce display
      }
    }

    // Aggiornamento meteo ogni 10 minuti circa
    static unsigned long lastWeatherFetch = 0;
    if (currentTime - lastWeatherFetch >= WEATHER_FETCH_INTERVAL) {
      fetchWeather();  // Scarica e aggiorna condizioni meteo

      // Se in modalità automatica, aggiorna animazione NeoPixel
      if (Currentmode == MODO_AUTO) {
        animateNeopixelCircular();  // Accende uno per volta i LED a cerchio
      }

      lastWeatherFetch = currentTime;  // Aggiorna tempo dell’ultimo fetch
    }

    // Visualizzazione display e animazione LED
    if (Currentmode != MODO_ALL_OFF) {  // Se non sei in modalità "tutto spento"

      if (showText) {
        if (DisplayOneTime == false) {
          displayWeatherInfo();   // Mostra testo: città, meteo, temperatura, ora
          DisplayOneTime = true;  // Evita ridisegni continui
        }
      } else {
        displayWeatherAnimation();  // Mostra animazione corrispondente (es. sole, pioggia)
        DisplayOneTime = false;
      }

      if (!neopixelBootAnimationInProgress) {
        if (Currentmode == MODO_AUTO) {
          runNeopixelAnimation();  // Attiva effetto respiro LED
        }
      }

    } else {
      // Modalità “Tutto spento”: spegni completamente il display
      display.clearDisplay();
      display.display();
    }
  }
}


// --- Funzioni di gestione della configurazione ---
// Questa funzione salva le impostazioni utente nella memoria persistente (flash)
// Le configurazioni vengono mantenute anche dopo un riavvio o spegnimento del dispositivo
void saveConfig() {
  preferences.putString("city", savedCity);                // Salva il nome della città
  preferences.putInt("brightness", savedNeopixelBrightness);  // Salva la luminosità dei NeoPixel
  preferences.putInt("Currentmode", Currentmode);          // Salva la modalità corrente (es. AUTO, ROSSO, ecc.)
  Serial.println("Configurazione salvata.");               // Messaggio di conferma sul monitor seriale
}


// --- Carica le configurazioni salvate in precedenza ---
void loadConfig() {
  // Recupera la città salvata; se non trovata, mantiene il valore di default ("Monza")
  preferences.getString("city", savedCity, sizeof(savedCity));

  // Recupera la luminosità NeoPixel; se assente, imposta default 200
  savedNeopixelBrightness = preferences.getInt("brightness", 200);

  // Recupera la modalità corrente; se non esistente, imposta AUTO (0)
  Currentmode = preferences.getInt("Currentmode", 0);

  Serial.println("Configurazione caricata.");
}

// --- Callback chiamata quando le credenziali WiFi sono state salvate ---
void saveConfigCallback() {
  Serial.println("Le credenziali WiFi sono state salvate.");
}

// --- Callback attivata quando si entra in modalità Access Point per la configurazione WiFi ---
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entrato in modalita' configurazione AP");
  Serial.println(myWiFiManager->getConfigPortalSSID());  // Stampa SSID su seriale

  // Mostra istruzioni di connessione sul display OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Configura WiFi:");
  display.println(myWiFiManager->getConfigPortalSSID());  // SSID dell'AP creato
  display.println("Connettiti a questa rete per configurare.");
  display.display();
}


// --- FUNZIONI METEO ---
// Funzione che contatta OpenWeatherMap per ottenere i dati meteo attuali
void fetchWeather() {
  Serial.println("Fetching weather from OpenWeatherMap...");
  HTTPClient http;

  // Recupera la chiave API attualmente salvata
  String currentApiKey = preferences.getString("api_key", OPENWEATHER_API_KEY);

  // Alcune città richiedono adattamento (es. Roma → Rome)
  String apiCityQuery = savedCity;
  if (apiCityQuery.equalsIgnoreCase("Roma")) {
    apiCityQuery = "Rome";
  }

  // Costruisce l’URL per interrogare OpenWeatherMap con unità metriche e lingua italiana
  String weatherUrl = "http://api.openweathermap.org/data/2.5/weather?q=" + apiCityQuery + "," + String(savedCountryCode) + "&units=metric&appid=" + currentApiKey + "&lang=it";

  Serial.print("OpenWeatherMap URL: ");
  Serial.println(weatherUrl);

  http.begin(weatherUrl);              // Inizializza la richiesta HTTP
  int httpCode = http.GET();          // Effettua la chiamata GET

  // Controlla se la risposta è valida (codice 200)
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();  // Ottiene il contenuto della risposta JSON
    Serial.println("OpenWeatherMap Response:");
    Serial.println(payload);

    // Parser JSON: prepara buffer di dimensione adeguata
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);

    // Gestisce eventuali errori di parsing JSON
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      weatherDescription = "Errore JSON OWM.";
      return;
    }

    // Estrae informazioni rilevanti dal JSON
    JsonObject weather_0 = doc["weather"][0];
    weatherDescription = weather_0["description"].as<String>();  // Es. "pioggia leggera"
    weatherConditionCode = weather_0["id"].as<int>();            // Codice meteo numerico OpenWeather

    // Dati aggiuntivi
    temperatureCelsius = doc["main"]["temp"].as<float>();         // Temperatura attuale
    sunriseTime = doc["sys"]["sunrise"].as<long>();              // Timestamp UNIX alba
    sunsetTime = doc["sys"]["sunset"].as<long>();                // Timestamp UNIX tramonto

    // Se sei in modalità automatica, aggiorna LED e attiva animazione cerchio
    if (Currentmode == MODO_AUTO) {
      updateWeatherLEDs();          // Imposta i LED meteo e colore base per l’anello
      animateNeopixelCircular();    // Avvia animazione cerchio all’accensione
    }

    // Debug seriale
    Serial.print("Descrizione Meteo: ");
    Serial.println(weatherDescription);
    Serial.print("Codice Condizione (ID): ");
    Serial.println(weatherConditionCode);
    Serial.print("Temperatura: ");
    Serial.println(temperatureCelsius);
    Serial.print("Alba: ");
    Serial.println(sunriseTime);
    Serial.print("Tramonto: ");
    Serial.println(sunsetTime);
    Serial.print("Mode: ");
    Serial.println(Currentmode);
  } else {
    // In caso di errore HTTP
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    weatherDescription = "Errore HTTP OWM.";

    // Errori specifici: API non valida o città non trovata
    if (httpCode == 401) {
      Serial.println("API Key non valida o non attiva. Controlla il portale o l'interfaccia web.");
      weatherDescription = "API Key Errata!";
    } else if (httpCode == 404) {
      Serial.println("Citta' o paese non trovato. Verifica il nome della citta' o il codice paese.");
      weatherDescription = "Citta' non trovata!";
    }
  }

  http.end();  // Termina la connessione HTTP
}


// --- FUNZIONE PER MOSTRARE METEO E ORARIO SUL DISPLAY OLED ---
void displayWeatherInfo() {
  display.clearDisplay();                          // Pulisce completamente il display
  display.setTextColor(SSD1306_WHITE);             // Imposta colore testo (bianco)

  // Riga superiore: nome città con font grande
  display.setTextSize(2);                          // Font grande
  display.setCursor(0, 0);
  display.println(savedCity);                      // Nome della città salvata

  // Riga intermedia: descrizione meteo (es. "Pioggia leggera")
  display.setTextSize(1);                          // Font normale
  display.setCursor(0, 16);
  display.println(getWeatherDescriptionItalian(weatherConditionCode));  // Traduce codice meteo

  // Riga inferiore: temperatura e ora
  timeClient.update();                             // Aggiorna orario da NTPClient
  // Ora, timeClient.getEpochTime() restituisce il tempo UTC.
  // Per ottenere l'ora locale (con ora legale gestita), usiamo localtime().
  time_t currentTime = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&currentTime);   // Converte in formato locale (gestisce DST)

  String tempStr = String(temperatureCelsius, 1) + " C";  // Converte la temperatura in stringa (es. "21.3 C")

  // Formatta l’orario in HH:MM
  String timeStr = "";
  char timeBuffer[6];                              // HH:MM + terminatore \0
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", timeinfo);  // Riempie il buffer con l'ora locale
  timeStr = String(timeBuffer);                    // Converte in String

  display.setCursor(0, 24);                         // Posizione riga inferiore
  display.print(tempStr);                           // Mostra temperatura
  display.print(" - ");
  display.println(timeStr);                         // Mostra orario

  display.display();                                // Aggiorna il contenuto del display

  // Se la modalità corrente è AUTO, aggiorna anche i LED NeoPixel
  if (Currentmode == MODO_AUTO) {
    // neopixelBootAnimationInProgress = true; // Non è necessario bloccare qui, gestito altrove
    updateWeatherLEDs();                            // Imposta LED meteo (sole, nuvole, ecc.)
    animateNeopixelCircular();                      // Accende l’anello LED in sequenza
    // neopixelBootAnimationInProgress = false; // Non è necessario resettare qui
  }
}


// --- FUNZIONE PER MOSTRARE ANIMAZIONE METEO GRAFICA SUL DISPLAY OLED ---
void displayWeatherAnimation() {
  display.clearDisplay();  // Pulisce lo schermo OLED
  display.setTextColor(SSD1306_WHITE);  // Colore testo (usato per le bitmap bianche)

  // Aggiorna orario per distinguere giorno/notte
  timeClient.update();
  time_t currentTime = timeClient.getEpochTime();

  // Confronta l'ora corrente con l'ora di alba e tramonto, ora entrambe in ora locale
  bool isDaytime = (currentTime >= sunriseTime && currentTime < sunsetTime);

  // Variabili per l’animazione corrente
  const unsigned char** currentAnimation = nullptr;   // Puntatore a bitmap
  const unsigned char** currentAnimation1 = nullptr;  // Per doppia animazione
  int currentAnimFrames = 0;
  int currentAnimWidth = 0;
  int currentAnimHeight = 0;

  // --- SCELTA ANIMAZIONE METEO ---
  if (weatherConditionCode >= 200 && weatherConditionCode < 300) {
    // Temporale
    currentAnimFrames = STORM_ANIM_FRAMES;
    currentAnimWidth = STORM_ANIM_WIDTH;
    currentAnimHeight = STORM_ANIM_HEIGHT;
    currentAnimation = storm_animation;
    currentAnimation1 = rain_animation;
    dualAnimation = true;
//    dualAnimation = false;

  } else if (weatherConditionCode >= 300 && weatherConditionCode < 400 ||
             weatherConditionCode >= 500 && weatherConditionCode < 600) {
    // Pioggia o pioggerella
    currentAnimation = rain_animation;
    currentAnimFrames = RAIN_ANIM_FRAMES;
    currentAnimWidth = RAIN_ANIM_WIDTH;
    currentAnimHeight = RAIN_ANIM_HEIGHT;
    dualAnimation = false;

  } else if (weatherConditionCode == 800) {
    // Cielo sereno → sole o luna in base a orario
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

  } else if (weatherConditionCode >= 801 && weatherConditionCode <= 802) {
    // Poche nuvole o nuvole sparse → sole/luna + nuvole insieme
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

  } else if (weatherConditionCode >= 803 && weatherConditionCode <= 804) {
    // Nuvole irregolari o cielo coperto
    currentAnimation = cloud_animation;
    currentAnimFrames = CLOUD_ANIM_FRAMES;
    currentAnimWidth = CLOUD_ANIM_WIDTH;
    currentAnimHeight = CLOUD_ANIM_HEIGHT;
    dualAnimation = false;

  } else {
    // Nessuna animazione nota → mostra testo
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("No Animazione");
    display.println(weatherDescription);  // Mostra comunque la descrizione del meteo
  }

  // --- GESTIONE DEL FRAME DELL’ANIMAZIONE ---
  if (currentAnimation != nullptr) {
    // Gestione avanzata del frame per animazione fluida
    if (millis() - lastFrameChange >= ANIM_FRAME_DELAY) {
      currentFrame = (currentFrame + 1) % currentAnimFrames;
      lastFrameChange = millis();
    }

    // --- DISEGNO DELLE IMMAGINI ---
    if (!dualAnimation) {
      // Mostra una singola animazione centrata
      int x_pos = (OLED_WIDTH - currentAnimWidth) / 2;
      int y_pos = (OLED_HEIGHT - currentAnimHeight) / 2;
      display.drawBitmap(x_pos, y_pos, currentAnimation[currentFrame], currentAnimWidth, currentAnimHeight, SSD1306_WHITE);

    } else {
      // Mostra due bitmap animate: una a sinistra e una a destra
      int x_pos1 = 20;
      int y_pos1 = (OLED_HEIGHT - currentAnimHeight) / 2;
      int x_pos2 = 128 - 32 - 20;
      int y_pos2 = (OLED_HEIGHT - currentAnimHeight) / 2;

      display.drawBitmap(x_pos1, y_pos1, currentAnimation[currentFrame], currentAnimWidth, currentAnimHeight, SSD1306_WHITE);
      display.drawBitmap(x_pos2, y_pos2, currentAnimation1[currentFrame], currentAnimWidth, currentAnimHeight, SSD1306_WHITE);
    }
  }

  display.display();  // Mostra su OLED tutto il frame appena composto
}


// --- Spegne tutti i LED meteo ---
void AllLEDoff() {
  digitalWrite(PIN_LED_SOLE, LOW);
  digitalWrite(PIN_LED_NUVOLE, LOW);
  digitalWrite(PIN_LED_PIOGGIA, LOW);
  digitalWrite(PIN_LED_TEMPORALE, LOW);
  digitalWrite(PIN_LED_LUNA, LOW);
}

// --- Aggiorna i LED fisici in base al meteo attuale ---
void updateWeatherLEDs() {
  AllLEDoff();  // Spegne tutto prima

  // Ottiene il tempo attuale per distinguere giorno/notte
  timeClient.update();
  time_t currentTime = timeClient.getEpochTime();

  // Confronta l'ora corrente con l'ora di alba e tramonto, ora entrambe in ora locale
  bool isDaytime = (currentTime >= sunriseTime && currentTime < sunsetTime);

  // Colore di base per l’anello NeoPixel, da applicare in animazione respiro
  uint32_t tempNeopixelBaseColor = pixels.Color(0, 0, 0);  // Default: spento

  // --- Mappa codici OpenWeather alle condizioni ---
  if (weatherConditionCode >= 200 && weatherConditionCode < 300) {
    // Temporale
    // digitalWrite(PIN_LED_NUVOLE, HIGH);
    digitalWrite(PIN_LED_PIOGGIA, HIGH);

    for (int r = 0; r < 6; r++) {
      digitalWrite(PIN_LED_TEMPORALE, LOW);
      ActivateCircular(pixels.Color(0, 0, 0)); // Non necessario spegnere e riaccendere l'anello qui
      delay(100);
      digitalWrite(PIN_LED_TEMPORALE, HIGH);
      ActivateCircular(pixels.Color(197, 0, 255)); // Non necessario riattivare l'anello qui
      delay(100);
    }

    tempNeopixelBaseColor = pixels.Color(197, 0, 255);

  } else if (weatherConditionCode >= 300 && weatherConditionCode < 400) {
    // Pioggerella
    digitalWrite(PIN_LED_PIOGGIA, HIGH);
    digitalWrite(PIN_LED_NUVOLE, HIGH);
    tempNeopixelBaseColor = pixels.Color(0, 0, 255);  // Blu classico

  } else if (weatherConditionCode >= 500 && weatherConditionCode < 600) {
    // Pioggia
    digitalWrite(PIN_LED_PIOGGIA, HIGH);
    digitalWrite(PIN_LED_NUVOLE, HIGH);
    tempNeopixelBaseColor = pixels.Color(0, 180, 255);  // Blu ciano brillante

  } else if (weatherConditionCode == 800) {
    // Cielo sereno
    if (isDaytime) {
      digitalWrite(PIN_LED_SOLE, HIGH);
      tempNeopixelBaseColor = pixels.Color(255, 227, 0);  // Giallo solare
    } else {
      digitalWrite(PIN_LED_LUNA, HIGH);
      tempNeopixelBaseColor = pixels.Color(128, 128, 128);  // Bianco/grigio tenue
    }

  } else if (weatherConditionCode >= 801 && weatherConditionCode <= 802) {
    // Sole o luna con nuvole sparse
    if (isDaytime) {
      digitalWrite(PIN_LED_SOLE, HIGH);
      digitalWrite(PIN_LED_NUVOLE, HIGH);
      tempNeopixelBaseColor = pixels.Color(255, 227, 0);  // Giorno: giallo
    } else {
      digitalWrite(PIN_LED_LUNA, HIGH);
      digitalWrite(PIN_LED_NUVOLE, HIGH);
      tempNeopixelBaseColor = pixels.Color(128, 128, 128);  // Notte: tenue
    }

  } else if (weatherConditionCode >= 803 && weatherConditionCode <= 804) {
    // Cielo molto nuvoloso o coperto
    digitalWrite(PIN_LED_NUVOLE, HIGH);
    tempNeopixelBaseColor = pixels.Color(255, 255, 255);  // Bianco

  }

  // Aggiorna il colore base da usare per effetto respiro o accensione LED
  neopixelBaseColor = tempNeopixelBaseColor;
}

// --- Imposta il colore dell’anello NeoPixel in modalità manuale ---
void setNeopixelColor(uint32_t color) {
  neopixelBaseColor = color;  // Aggiorna il colore base globale

  // Solo se NON siamo in modalità automatica
  if (Currentmode != MODO_AUTO) {
    pixels.setBrightness(savedNeopixelBrightness);  // Applica la luminosità salvata
    for (int i = 0; i < NUM_NEOPIXELS; i++) {
      pixels.setPixelColor(i, neopixelBaseColor);   // Imposta il colore di ogni singolo LED
    }
    pixels.show();  // Applica effettivamente i colori
  }
}


/// --- Accensione progressiva dei LED NeoPixel uno alla volta in sequenza circolare ---
void animateNeopixelCircular() {
  uint32_t targetColor = neopixelBaseColor;  // Colore verso cui vogliamo arrivare

  pixels.clear();  // Spegne tutti i LED all’inizio
  pixels.show();

  // Effetto: accende i LED uno alla volta in senso orario
  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    pixels.setPixelColor(i, targetColor);  // Imposta colore sul LED corrente
    pixels.show();                         // Mostra il cambiamento
    delay(NEOPIXEL_BOOT_ANIM_DELAY);       // Breve pausa (effetto a cascata)
  }
}


// --- Accende l’intero anello con un colore fisso e simultaneo ---
void ActivateCircular(uint32_t targetColor) {
  pixels.clear();  // Pulisce lo stato precedente
  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    pixels.setPixelColor(i, targetColor);  // Imposta il colore su tutti i LED
  }
  pixels.show();  // Aggiorna l’anello
}



// Funzione Respiro (NON MODIFICATA nella logica principale)
void runNeopixelAnimation() {
  uint8_t animatedRed =0;
  uint8_t animatedGreen =0;
  uint8_t animatedBlue =0;

  static unsigned long lastUpdate = 0;
  unsigned long currentMillis = millis();

  // Variabili per l'effetto respiro lineare (statiche per mantenere il loro stato)
  static int currentBreathBrightness = NEOPIXEL_BREATH_MIN; // La luminosità corrente
  static bool breathIncreasing = true; // Flag per la direzione (aumenta/diminuisce)

  // --- Configurazioni per l'effetto di respiro ---
  // Durata desiderata per un ciclo completo di respiro (luminosità da MIN a MAX e ritorno a MIN)
  const unsigned long TARGET_CYCLE_DURATION_MS = 4000; // 4 secondi

  // Calcola il range effettivo di luminosità che verrà percorso (es. se MIN=10, MAX=60, il range è 50)
  const int ACTUAL_BRIGHTNESS_RANGE = NEOPIXEL_BREATH_MAX - NEOPIXEL_BREATH_MIN;

  // Calcola l'intervallo di aggiornamento per ogni "passo unitario" di luminosità.
  // Un ciclo completo (da MIN a MAX e ritorno a MIN) percorre 2 * (range effettivo) passi.
  unsigned long BREATH_UPDATE_INTERVAL_MS = 0;
  if (ACTUAL_BRIGHTNESS_RANGE > 0) { // Evita divisioni per zero se min e max sono uguali
      BREATH_UPDATE_INTERVAL_MS = TARGET_CYCLE_DURATION_MS / (2 * ACTUAL_BRIGHTNESS_RANGE);
  } else {
      // Se il range è 0 (MIN == MAX), non c'è variazione, imposta un intervallo di default
      // per evitare che il ciclo vada in loop troppo velocemente o con divisioni per zero.
      BREATH_UPDATE_INTERVAL_MS = BREATH_UPDATE_SPEED;
  }

  // Assicurati che l'intervallo sia almeno 1ms per evitare valori zero o intervalli troppo veloci
  if (BREATH_UPDATE_INTERVAL_MS == 0) {
      BREATH_UPDATE_INTERVAL_MS = 1;
  }
  // --- Fine Configurazioni ---

  if (currentMillis - lastUpdate >= BREATH_UPDATE_INTERVAL_MS) {
    lastUpdate = currentMillis;

    // Logica di incremento/decremento unitario della luminosità
    if (breathIncreasing) {
      currentBreathBrightness += 1; // Incrementa la luminosità di 1 unità
      if (currentBreathBrightness >= NEOPIXEL_BREATH_MAX) {
        currentBreathBrightness = NEOPIXEL_BREATH_MAX; // Non superare il massimo impostato
        breathIncreasing = false; // Inverti la direzione: ora diminuisce
      }
    } else {
      currentBreathBrightness -= 1; // Decrementa la luminosità di 1 unità
      if (currentBreathBrightness <= NEOPIXEL_BREATH_MIN) {
        currentBreathBrightness = NEOPIXEL_BREATH_MIN; // Non scendere sotto il minimo impostato
        breathIncreasing = true; // Inverti la direzione: ora aumenta
      }
    }

    // Applica la luminosità calcolata al colore base dei NeoPixel
    uint8_t red = (neopixelBaseColor >> 16) & 0xFF;
    uint8_t green = (neopixelBaseColor >> 8) & 0xFF;
    uint8_t blue = neopixelBaseColor & 0xFF;

    if (neopixelBaseColor == pixels.Color(0,0,0)) {
        pixels.clear(); // Se il colore base è nero, spegni i pixel
    } else {

       int breathRange = NEOPIXEL_BREATH_MAX - NEOPIXEL_BREATH_MIN;
       if (breathRange <= 0) breathRange = 1;  // evita divisione per zero

        // RED
       if (red == 0) {
           animatedRed = 0;
       } else {
           int minRed = NEOPIXEL_BREATH_MIN;
           if (minRed > red) minRed = red;
           animatedRed = minRed + (currentBreathBrightness - NEOPIXEL_BREATH_MIN) * (red - minRed) / breathRange;
       }

       // GREEN
       if (green == 0) {
           animatedGreen = 0;
       } else {
           int minGreen = NEOPIXEL_BREATH_MIN;
           if (minGreen > green) minGreen = green;
           animatedGreen = minGreen + (currentBreathBrightness - NEOPIXEL_BREATH_MIN) * (green - minGreen) / breathRange;
       }

       // BLUE
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
    pixels.show(); // Aggiorna i NeoPixel con la nuova luminosità
  }
}

// Funzione per calcolare il valore finale del singolo canale
int calcolaColoreFinale(int iniziale, int VAR, int valMin, int valMax) {
    if (iniziale == 0) return 0;

    // Definisco un delta (es: variazione di 50)
    int delta = 50;
    int coloreMin = iniziale - delta;
    if (coloreMin < 0) coloreMin = 0;

    int range = valMax - valMin;
    if (range <= 0) return iniziale;

    // incremento proporzionale da coloreMin a iniziale
    int incremento = (VAR - valMin) * (iniziale - coloreMin) / range;
    return coloreMin + incremento;
}



// --- FUNZIONI PULSANTE
bool buttonCurrentlyPressed = false; // Stato attuale del pulsante

void handleButtonPress() {
  // Leggi lo stato attuale del pulsante
  bool reading = digitalRead(PIN_PULSANTE);

  // Debounce: verifica se lo stato è stabile
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Se lo stato è cambiato in modo stabile
    if (reading != buttonState) { // buttonState è lo stato "attuale stabile"
      buttonState = reading; // Aggiorna lo stato stabile

      if (buttonState == LOW) { // Pulsante è stato PREMUTO (da HIGH a LOW)
        keypressed=true;
        buttonPressStartTime = millis(); // Registra il tempo di inizio pressione
        Serial.println("Pulsante premuto (inizia timer).");
      } else { // buttonState == HIGH : Pulsante è stato RILASCIATO (da LOW a HIGH)
        unsigned long pressDuration = millis() - buttonPressStartTime;
        Serial.printf("Pulsante rilasciato. Durata: %lu ms.\n", pressDuration);

        if (isLongPressMode) {
          // Se eravamo in modalità pressione lunga, usciamo
          isLongPressMode = false;
          Serial.println("Modalita' regolazione luminosita' terminata.");
          // Pulisci il display o ripristina la visualizzazione normale
          display.clearDisplay(); // Pulisci l'area della barra
          // Potresti voler ricaricare qui la visualizzazione meteo/ora
          // o impostare un flag per farlo nel loop principale
          showText = true; // Assumi che showText ripristini la visualizzazione principale
          saveConfig(); // Salva la luminosità finale
          keypressed=false;
        } else {
          // Pressione breve (rilasciato prima della soglia di pressione lunga)
          Serial.println("Pressione breve rilevata. Cambio modalita'.");
          Currentmode++;
          if (Currentmode > MAX_MODE) Currentmode = MODO_AUTO;
          showText = true;

          SetRingColor(Currentmode); // Applica il colore della nuova modalità
          saveConfig(); // Salva le impostazioni
          fetchWeather(); // Esegui la tua azione originale
          keypressed=false;
        }
      }
    }
  }

  lastButtonState = reading; // Salva lo stato precedente per il debounce

  // Gestione della pressione lunga *mentre il pulsante è tenuto premuto*
  // e solo DOPO che la soglia è stata superata.
  if (buttonState == LOW && !isLongPressMode && (millis() - buttonPressStartTime) >= LONG_PRESS_THRESHOLD) {
    isLongPressMode = true; // Entra nella modalità pressione lunga
    Serial.println("Pressione lunga rilevata. Inizio regolazione luminosita'.");
    display.clearDisplay(); // Pulisci il display per la barra
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("    Luminosita':");
    display.display();
    lastBrightnessChangeTime = millis(); // Inizializza il timer per la luminosità
    // Applica subito la luminosità corrente per visualizzare lo stato iniziale
    pixels.setBrightness(savedNeopixelBrightness);
    pixels.show();           // forza il refresh immediato
  }

  // Se siamo in modalità pressione lunga (pulsante premuto per oltre 1 sec)
  if (isLongPressMode && buttonState == LOW) {
    if ((millis() - lastBrightnessChangeTime) > BRIGHTNESS_CHANGE_INTERVAL) {
      lastBrightnessChangeTime = millis();

      savedNeopixelBrightness += 1;
      if (savedNeopixelBrightness > 255) {
        savedNeopixelBrightness = 10; // Torna a zero
      }
      pixels.setBrightness(savedNeopixelBrightness);
      pixels.show();           // forza il refresh immediato
      drawBrightnessBar(savedNeopixelBrightness); 
    }
  }
}


// --- Funzione per aggiornare i NeoPixel con la nuova luminosità
// Questa funzione è utile per applicare solo la luminosità senza cambiare il colore di base
void updateNeoPixelBrightness(uint8_t brightness) {
    uint8_t r_base = (pixels.getPixelColor(0) >> 16) & 0xFF; // Prendi il colore base dal primo pixel
    uint8_t g_base = (pixels.getPixelColor(0) >> 8) & 0xFF;
    uint8_t b_base = (pixels.getPixelColor(0) & 0xFF);

    // Se i pixel erano spenti, potremmo voler dare un colore di default per la regolazione
    if (r_base == 0 && g_base == 0 && b_base == 0 && brightness > 0) {
        r_base = 0;
        g_base = 0;
        b_base = 255; // Blu di default per la regolazione se erano spenti
    }

    uint8_t r = (uint8_t)((float)r_base * (brightness / 255.0f));
    uint8_t g = (uint8_t)((float)g_base * (brightness / 255.0f));
    uint8_t b = (uint8_t)((float)b_base * (brightness / 255.0f));

    pixels.fill(pixels.Color(r, g, b)); // Applica il colore con la nuova luminosità a tutti i pixel
    pixels.show();
}


// --- Funzione per disegnare la barra di luminosità sul display OLED ---
void drawBrightnessBar(uint8_t brightness) {
  display.clearDisplay(); // Pulisci il display
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("    Luminosita':");

  // Disegna la barra di progresso
  int barWidth = map(brightness, 0, 255, 0, display.width() - 10); // Mappa 0-255 alla larghezza del display
  int barHeight = 10;
  int startX = 5;
  int startY = 20;

  display.drawRect(startX, startY, display.width() - 10, barHeight, SSD1306_WHITE); // Bordo della barra
  display.fillRect(startX, startY, barWidth, barHeight, SSD1306_WHITE); // Barra riempita

  display.display(); // Aggiorna il display
}


// Imposta il colore dell’anello NeoPixel in base alla modalità selezionata
void SetRingColor(int Mode) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2); // Dimensione testo 2
  display.setCursor(0, 10);
  switch (Mode) {
    case MODO_AUTO:  // auto
      display.println("   AUTO");
      break;
    case MODO_ROSSO:  // rosso
      neopixelBaseColor = pixels.Color(255, 0, 0);
      display.println("   ROSSO");
      break;
    case MODO_VERDE:  //verde
      neopixelBaseColor = pixels.Color(0, 255, 0);
      display.println("   VERDE");
      break;
    case MODO_BLU:  //blu
      neopixelBaseColor = pixels.Color(0, 0, 255);
      display.println("    BLU");
      break;
    case MODO_VIOLA:  //viola
      neopixelBaseColor = pixels.Color(255, 0, 255);
      display.println("   VIOLA");
      break;
    case MODO_CIANO:  //ciano
      neopixelBaseColor = pixels.Color(0, 255, 255);
      display.println("  AZZURRO");
      break;
    case MODO_GIALLO:  // giallo
      neopixelBaseColor = pixels.Color(255, 255, 0);
      display.println("   GIALLO");
      break;
    case MODO_BIANCO: // bianco
      neopixelBaseColor = pixels.Color(255, 255, 255);
      display.println("   BIANCO");
      break;
    case MODO_RING_OFF: // anello spento
      neopixelBaseColor = pixels.Color(0, 0, 0);
      display.println("ANELLO SP.");
      break;
    case MODO_LED_OFF: // LED spenti
      neopixelBaseColor = pixels.Color(0, 0, 0);
      AllLEDoff();
      display.println("LED SPENTI");
      break;
    case MODO_ALL_OFF: // Tutto Spento
      neopixelBaseColor = pixels.Color(0, 0, 0);
      AllLEDoff();
      display.println("   SPENTO");
      break;
  }
  display.display();
  animateNeopixelCircular(); // Esegue l'animazione anello che si accende
}


// --- IMPLEMENTAZIONE GESTORI SERVER WEB STANDARD ---

// DEVI ASSICURARTI CHE QUESTA VARIABILE SIA DICHIARATA GLOBALMENTE
// E AGGIORNATA DENTRO fetchWeather() DOPO AVER PARSATO LA RISPOSTA JSON.
// Esempio: int lastFetchedWeatherId = 800; // Inizializza con un valore di default
// extern int lastFetchedWeatherId; // Dichiarazione se definita altrove nel codice (commentato perché non usata direttamente qui)

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>"+ProductName+" Config</title><style>";
    html += "body{font-family: Arial, Helvetica, sans-serif;}";
    html += ".container{max-width:400px;margin:auto;padding:20px;border:1px solid #ddd;border-radius:8px;background-color:#f9f9f9;}";
    html += "input[type=text], select{width:100%;padding:10px;margin:5px 0 15px 0;display:inline-block;border:1px solid #ccc;border-radius:4px;box-sizing:border-box;}";
    html += "input[type=range]{width:100%;margin:10px 0;}";
    html += "label{margin-bottom:5px;display:block;}";
    html += "button{background-color:#4CAF50;color:white;padding:12px 20px;border:none;border-radius:4px;cursor:pointer;width:100%;font-size:16px;}";
    html += "button:hover{background-color:#45a049;}";
    html += ".toggle-switch{position:relative;display:inline-block;width:60px;height:34px;}";
    html += ".toggle-switch input{opacity:0;width:0;height:0;}";
    html += ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;-webkit-transition:.4s;transition:.4s;border-radius:34px;}";
    html += ".slider:before{position:absolute;content:\"\";height:26px;width:26px;left:4px;bottom:4px;background-color:white;-webkit-transition:.4s;transition:.4s;border-radius:50%;}";
    html += "input:checked + .slider{background-color:#2196F3;}";
    html += "input:focus + .slider{box-shadow:0 0 1px #2196F3;}";
    html += "input:checked + .slider:before{-webkit-transform:translateX(26px);-ms-transform:translateX(26px);transform:translateX(26px);}";
    html += "</style></head><body><div class=\"container\"><h2>"+ProductName+" Config</h2>";

    // AGGIUNTA QUI: Meteo Corrente
    // **CORREZIONE:** Usiamo il nome della funzione corretto e passiamo l'ID della condizione meteo
    html += "<p><strong>Meteo Corrente:</strong> " + getWeatherDescriptionItalian(weatherConditionCode) + "</p>";

    html += "<form action=\"/save\" method=\"post\">";

    // Luminosità NeoPixel
    html += "<label for=\"brightness\">Luminosita' Generale:</label>";
    html += "<input type=\"range\" id=\"brightness\" name=\"brightness\" min=\"10\" max=\"255\" value=\"" + String(savedNeopixelBrightness) + "\">";
    html += "<span id=\"brightnessValue\">" + String(savedNeopixelBrightness) + "</span>";

    // Elenco modi
    html += "<label for=\"mode_select\">Seleziona modalita':</label>";
    html += "<select id=\"mode_select\" name=\"mode_select\">";
    String modes[] = {
        "Automatica", "Luce rossa", "Luce Verde", "Luce Blu", "Luce Viola", "Luce Azzurra", "Luce Gialla", "Luce Bianca", "Anello Spento", "LED Spenti", "Tutto Spento"
    };
    for (int i = 0; i < sizeof(modes)/sizeof(modes[0]); i++) {
        html += "<option value=\"" + String(i) + "\"" + (Currentmode == i ? String(" selected") : String("")) + ">" + modes[i] + "</option>";
    }
    html += "</select>";

    // Elenco città italiane esteso
    html += "<label for=\"city_select\">Seleziona Citta':</label>";
    html += "<select id=\"city_select\" name=\"city_select\">";
    html += "<option value=\"\">-- Seleziona --</option>";
    String cities[] = {
        "Agrigento", "Alessandria", "Ancona", "Aosta", "L'Aquila", "Arezzo", "Ascoli Piceno", "Asti", "Avellino", "Bari", "Barletta-Andria-Trani", "Belluno", "Benevento", "Bergamo", "Biella",
        "Bologna", "Bolzano", "Brescia", "Brindisi", "Cagliari", "Caltanissetta", "Campobasso", "Caserta", "Catania", "Catanzaro", "Chieti", "Como", "Cosenza", "Cremona", "Crotone",
        "Cuneo", "Enna", "Fermo", "Ferrara", "Firenze", "Foggia", "Forlì-Cesena", "Frosinone", "Genova", "Gorizia", "Grosseto", "Imperia", "Isernia", "La Spezia", "Latina", "Lecce",
        "Lecco", "Livorno", "Lodi", "Lucca", "Macerata", "Mantova", "Massa-Carrara", "Matera", "Messina", "Milano", "Modena", "Monza", "Napoli", "Novara", "Nuoro",
        "Oristano", "Padova", "Palermo", "Parma", "Pavia", "Perugia", "Pesaro e Urbino", "Pescara", "Piacenza", "Pisa", "Pistoia", "Pordenone", "Potenza", "Prato", "Ragusa",
        "Ravenna", "Reggio Calabria", "Reggio Emilia", "Rieti", "Rimini", "Roma", "Rovigo", "Sabaudia", "Salerno", "Sassari", "Savona", "Siena", "Siracusa", "Sondrio", "Taranto", "Taverna", "Teramo",
        "Terni", "Torino", "Trapani", "Trento", "Treviso", "Trieste", "Udine", "Varese", "Venezia", "Verbania", "Vercelli", "Verona", "Vibo Valentia", "Vicenza", "Viterbo"
    };
    for (int i = 0; i < sizeof(cities)/sizeof(cities[0]); i++) {
        html += "<option value=\"" + cities[i] + "\"" + (String(savedCity).equalsIgnoreCase(cities[i]) ? String(" selected") : String("")) + ">" + cities[i] + "</option>";
    }
    html += "</select>";
    html += "<label for=\"city_manual\">Inserisci Citta' manualmente (se non presente nell'elenco):</label>";
    html += "<input type=\"text\" id=\"city_manual\" name=\"city_manual\" value=\"" + String(savedCity) + "\">";


    html += "<button type=\"submit\">Salva Impostazioni</button>";
    html += "</form>";
    html += "<script>";
    html += "var slider = document.getElementById('brightness');";
    html += "var output = document.getElementById('brightnessValue');";
    html += "output.innerHTML = slider.value;";
    html += "slider.oninput = function() { output.innerHTML = this.value; }";

    html += "</script>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
}


void handleSave() {

    // Gestione Luminosità 
    if (server.hasArg("brightness")) {
        savedNeopixelBrightness = server.arg("brightness").toInt();
        pixels.setBrightness(savedNeopixelBrightness);
        pixels.show();
    }

    // Gestione Modalità 
    String modes[] = {
        "Automatica", "Luce rossa", "Luce Verde", "Luce Blu", "Luce Viola", "Luce Azzurra", "Luce Gialla", "Luce Bianca", "Anello Spento", "LED Spenti", "Tutto Spento"
    };

    if (server.hasArg("mode_select")) {
        int selectedModeInt = server.arg("mode_select").toInt();

        if (selectedModeInt >= 0 && selectedModeInt <= MAX_MODE) {
            if (selectedModeInt != Currentmode) {
                Currentmode = selectedModeInt;

                if (Currentmode == 0) { // Se la modalità "Automatica" (indice 0) è selezionata
                    fetchWeather(); 
                } else {
                }

                SetRingColor(Currentmode); 
            } else {
            }
        } else {
        }
    }

    // Gestione Città 
    String cityFromSelector = server.arg("city_select");
    String cityFromManualInput = server.arg("city_manual");
    String cityToSave = "";

    if (!cityFromManualInput.isEmpty()) {
        cityToSave = cityFromManualInput;
    } else {
        cityToSave = cityFromSelector;
    }

    if (String(savedCity) != cityToSave) {
        strncpy(savedCity, cityToSave.c_str(), sizeof(savedCity) - 1);
        savedCity[sizeof(savedCity) - 1] = '\0';
        Serial.println(savedCity);
    } else {
    }

    saveConfig(); 

    // Reindirizzamento
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
    delay(5); // Aggiungi un piccolo ritardo per permettere al server di inviare la risposta
}