// V1.0 - 06/2025 - Meteovision by Davide Gatti - 2025 - www.survivalhacking.it
// V1.1 - 09/2025 - L'inserimento della città manualemnte via WEB non funzionava
// V1.2 - 09/2025 - Aggiunta la possibilità di vedere la previsione del giorno corrente / del giorno dopo e del giorno dopo ancora, mediante menù di configurazione WEB
// V1.3 - 10/2025 - Non erano gestite diverse tipologie di meteo tipo Neve da 600 a 699 e nebbia fumo e altre condizioni particolari da 700 a 799
// V1.4 - 10/2025 - Aggiunta scritta in mezzo alle due icone per evidenziare il giorno relativo alle previsioni
// V1.5 - 10/2025 - (By Marco Camerani) Aggiunta Modalità Notturna con orario impostabile da interfaccia web, Indicazione intensità e direzione vento, fix meteo led che non si aggiornano se in modalità diversa auto
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
#include <WiFiManager.h>  // Permette setup Wi-Fi via interfaccia web

// Richieste HTTP per ottenere i dati meteo
#include <HTTPClient.h>
#include <ArduinoJson.h>  // Parsing JSON della risposta OpenWeather

#include "graphics.h"  // Contiene animazioni bitmap per il display

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

#include "secret.h"  // File con chiavi/API sensibili

// -- DEFINIZIONI HARDWARE DISPLAY OLED --
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
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
#define NUM_NEOPIXELS 24  // Numero LED sull’anello
Adafruit_NeoPixel pixels(NUM_NEOPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// -- DEFINIZIONE PULSANTE FISICO --
#define PIN_PULSANTE 10  // Pulsante per cambiare modalità/luminosità

// -- DEFINIZIONI PER TEMPISMO DI SCHERMATA E METEO --
#define SCREEN_TEXT_DURATION 5000              // Tempo visualizzazione info (ms)
#define SCREEN_ANIMATION_DURATION 10000        // Tempo visualizzazione animazione (ms)
#define WEATHER_FETCH_INTERVAL 10 * 60 * 1000  // Ogni 10 minuti (in ms)

// Parametri effetto “respiro” LED (NeoPixel)
#define NEOPIXEL_BREATH_MIN 25       // Luminosità minima effetto respiro
#define NEOPIXEL_BREATH_MAX 255      // Luminosità massima
#define BREATH_UPDATE_SPEED 5        // Velocità animazione
#define NEOPIXEL_BOOT_ANIM_DELAY 50  // Delay tra accensioni LED iniziali



// Variabili per la gestione del pulsante
unsigned long buttonPressStartTime = 0;  // Tempo d'inizio pressione
unsigned long buttonReleaseTime = 0;     // Tempo rilascio
bool buttonState = HIGH;                 // Stato attuale del pulsante (non premuto di default)
bool lastButtonState = HIGH;             // Stato precedente, per debounce
unsigned long lastDebounceTime = 0;      // Ultimo cambio stabile dello stato
const unsigned long debounceDelay = 50;  // Ritardo debounce in ms
bool keypressed = false;                 // Flag se il pulsante è stato premuto

// Variabili per pressione lunga del pulsante
bool isLongPressMode = false;                     // Flag per modalità di regolazione luminosità
const unsigned long LONG_PRESS_THRESHOLD = 1000;  // Soglia pressione lunga (1s)
unsigned long lastScreenChange = 0;
bool showText = true;  // Alterna tra testo e animazione

// Variabile per velocità cambio luminosità
const int BRIGHTNESS_CHANGE_INTERVAL = 20;  // Ogni quanto aggiornare luminosità (ms)
unsigned long lastBrightnessChangeTime = 0;

// -- VARIABILI DI CONFIGURAZIONE PERSISTENTE --
Preferences preferences;  // Oggetto per salvare impostazioni in flash

// Valori persistenti da conservare
char savedCity[40] = "Monza";      // Città di default
char savedCountryCode[5] = "IT";   // Codice paese (IT → Italia)
int savedNeopixelBrightness = 10;  // Luminosità LED predefinita
int savedMode = 0;                 // Modalità predefinita
bool DisplayOneTime = false;       // Flag per evitare aggiornamenti duplicati del display
int Currentmode = 0;               // Modalità corrente (auto, colori fissi ecc.)
int forecastDay = 0;               // 0 = oggi, 1 = domani, 2 = dopodomani
#define MAX_MODE 10                // Numero massimo di modalità definite
bool nightModeEnabled = false;     // Abilita la modalità notturna
int nightStartHour = 22;           // Orario di inizio modalità notturna
int nightStartMinute = 00;
int nightEndHour = 07;  //Orario di fine modalità notturna
int nightEndMinute = 00;

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
float windSpeed = 0;                       // Velocitaà del vento
int windDirection = 0;                     // Direzione del vento
int weatherConditionCode = 0;              // Codice numerico condizione meteo
float temperatureCelsius = 0.0;            // Temperatura corrente (gradi Celsius)
time_t sunriseTime = 0;                    // Orario alba (timestamp UNIX)
time_t sunsetTime = 0;                     // Orario tramonto (timestamp UNIX)
bool showTemperature = true;               // Alterna tra true (mostra temp) e false (mostra direzione vento)
int animationFrame = 0;
const unsigned long WIND_SCAN_DURATION = 1000;  // 1 secondo per completare il giro (regolabile)
bool windScanning = false;                      // Indica se è in corso la rotazione iniziale
bool windAnimationActive = false;               // Indica se è attiva l'animazione della scia
int windScanFrame = 0;                          // Frame corrente del giro iniziale
unsigned long windAnimationStartTime = 0;       // Tempo inizio animazione scia
unsigned long animationStartTime = 0;           // Tempo di inizio animazione vento
int windLedIndex = 0;                           // Indice LED che indica la direzione del vento

// -- GESTIONE RETE E SERVER WEB --
WiFiManager wm;        // Oggetto per gestione automatica WiFi (e portale captive)
WebServer server(80);  // Istanza di web server HTTP sulla porta 80

// -- CLIENT NTP PER L'ORARIO DI SISTEMA --
WiFiUDP ntpUDP;  // UDP socket (necessario per NTPClient)
// L'offset del NTPClient deve essere 0 dopo aver configurato il fuso orario con configTime
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // Offset 0, configTime gestirà il fuso orario

// -- VARIABILI PER ANIMAZIONE DELL'ANELLO NEOPIXEL --
bool neopixelAnimationActive = true;                 // Flag animazione attiva
uint32_t neopixelBaseColor = pixels.Color(0, 0, 0);  // Colore di base corrente (RGB packed)

// Flag che indica se è in corso l'animazione di accensione all'avvio
bool neopixelBootAnimationInProgress = false;

// Parametro personalizzato globale per WiFiManager
WiFiManagerParameter custom_api_key("api_key", "OpenWeatherMap API Key", "", 60);

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
time_t getNtpTime();                                                            // Wrapper per ottenere ora da NTPClient
void AllLEDoff();                                                               // Spegne tutti i LED fisici di stato (non NeoPixel)
void SetRingColor(int mode);                                                    // Imposta il colore dell'anello NeoPixel in base alla modalità selezionata
int getBeaufortScale(float windSpeed);                                          //Genera scala Beaufort a partire dalla velocità del vento
void getWindColorByBeaufort(int beaufort, uint8_t &r, uint8_t &g, uint8_t &b);  //Colore in base alla scala Beaufort
int getTrailLengthByBeaufort(int beaufort);                                     //Lunghezza scia vento
void runWindAnimation();                                                        // Animazione Vento
void runWindDirectionScan(int windLedIndex);                                    // Effetto radar direzione vento
void checkNightMode();                                                          // Controllo Modalità Notturna
String urlEncode(const String& str);

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

  loadConfig();  // Carica le configurazioni salvate in precedenza

  Wire.begin();  // Inizializza la comunicazione I2C (usata da display)

  // Inizializza il display SSD1306 e verifica che sia stato allocato correttamente
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Entra in loop infinito se fallisce l'inizializzazione
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
  pixels.show();                                  // Assicura che l’anello sia spento inizialmente

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

    preferences.begin("meteo-config", false);
    preferences.clear();  // Elimina configurazioni salvate
    preferences.end();
    wm.resetSettings();   // Elimina reti WiFi salvate
    ESP.restart();        // Riavvia il microcontrollore
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
    ActivateCircular(pixels.Color(0, 0, 0));  // Spegne anello
    delay(100);
    digitalWrite(PIN_LED_TEMPORALE, HIGH);
    ActivateCircular(pixels.Color(197, 0, 255));  // Viola acceso
    delay(100);
  }

  delay(1000);                              // Pausa finale prima di spegnere
  ActivateCircular(pixels.Color(0, 0, 0));  // Spegne anello
  AllLEDoff();                              // Spegne tutti i LED meteo


  // --- Setup WiFiManager ---

  // Dichiarazioni globali (devono stare FUORI dal setup, in alto nel file):
  // WiFiManager wm;
  // Preferences preferences;
  // WiFiManagerParameter custom_api_key("api_key", "OpenWeatherMap API Key", "", 60);

  // --- Avvio WiFiManager con callback ---
  wm.setAPCallback(configModeCallback);          // Callback quando entra in modalità Access Point
  wm.setSaveConfigCallback(saveConfigCallback);  // Callback dopo salvataggio config

  // Recupera eventuale API key salvata in precedenza
  String savedApiKey = preferences.getString("api_key", "");
  if (savedApiKey.length() > 0) {
    Serial.println("API key trovata nelle preferences: " + savedApiKey);
    custom_api_key.setValue(savedApiKey.c_str(), 60);
  } else {
    Serial.println("Nessuna API key salvata trovata, campo vuoto.");
  }

  // Aggiunge il parametro al portale di configurazione
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
  delay(5000);             // Mostra messaggio per 5 secondi
  display.clearDisplay();  // Pulisce OLED

  // Salva anche la configurazione generale (se serve)
  //saveConfig();

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
  tzset();  // Applica la nuova configurazione del fuso orario

  timeClient.begin();   // Inizializza client NTP
  timeClient.update();  // Prima sincronizzazione ora

  // --- Setup server web ---
  server.on("/", HTTP_GET, handleRoot);       // Homepage
  server.on("/save", HTTP_POST, handleSave);  // Salva impostazioni
  server.begin();                             // Avvia server

  fetchWeather();               // Ottiene subito i dati meteo iniziali
  SetRingColor(Currentmode);    // Imposta colore LED secondo modalità corrente
  lastScreenChange = millis();  // Imposta timer per visualizzazione
}


// --- Loop ---
void loop() {
  wm.process();           // Permette a WiFiManager di gestire eventuali richieste WiFi (es. captive portal)
  server.handleClient();  // Serve eventuali richieste HTTP per il webserver

  handleButtonPress();  // Controlla se il pulsante è stato premuto e agisce di conseguenza

  if (keypressed == false) {  // Solo se il pulsante non è attivamente premuto

    checkNightMode();

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



      // === GESTIONE NEOPIXEL ===
      if (!neopixelBootAnimationInProgress) {
        if (Currentmode == MODO_AUTO) {
          if (windScanning) {
            runWindDirectionScan(windLedIndex);  // Fase 1: rotazione iniziale
          } else if (windAnimationActive) {
            runWindAnimation();  // Fase 2: scia vento
          } else {
            runNeopixelAnimation();  // Fase 3: effetto respiro LED
          }
        }
      }

      if (showText) {
        if (!DisplayOneTime) {
          displayWeatherInfo();  // Mostra città, meteo, temperatura o vento

          if (!showTemperature) {
            // Stai mostrando il vento: attiva la sequenza vento
            windScanning = true;
            windAnimationActive = false;
            animationFrame = 0;
            windScanFrame = 0;
            animationStartTime = millis();
          }

          // Alterna tra temperatura e vento (una volta per ciclo)
          showTemperature = !showTemperature;
          DisplayOneTime = true;
        }
      } else {
        displayWeatherAnimation();  // Mostra animazione meteo (sole, pioggia, ecc.)
        DisplayOneTime = false;
      }
    } else {
      // Modalità “Tutto spento”: spegni completamente il display
      display.clearDisplay();
      display.display();
    }
  }
}

void runWindDirectionScan(int windLedIndex) {
  static unsigned long lastScanUpdate = 0;
  static int lastFrameLogged = -1;
  const unsigned long scanStepDuration = WIND_SCAN_DURATION / NUM_NEOPIXELS;

  if (millis() - lastScanUpdate >= scanStepDuration) {
    lastScanUpdate = millis();

    pixels.clear();
    pixels.setPixelColor(windScanFrame, pixels.Color(0, 255, 0));  // LED verde che gira
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
// Questa funzione salva le impostazioni utente nella memoria persistente (flash)
// Le configurazioni vengono mantenute anche dopo un riavvio o spegnimento del dispositivo
void saveConfig() {
  preferences.begin("meteo-config", false);
  preferences.putString("city", savedCity);                   // Salva il nome della città
  preferences.putInt("brightness", savedNeopixelBrightness);  // Salva la luminosità dei NeoPixel
  preferences.putInt("Currentmode", Currentmode);             // Salva la modalità corrente
  preferences.putInt("forecastDay", forecastDay);             // Salva il giorno scelto
  preferences.putBool("nightModeEnabled", nightModeEnabled);  // Abilita il night mode
  preferences.putInt("nightStartHour", nightStartHour);
  preferences.putInt("nightStartMinute", nightStartMinute);
  preferences.putInt("nightEndHour", nightEndHour);
  preferences.putInt("nightEndMinute", nightEndMinute);
  preferences.end();

  Serial.println("Configurazione salvata.");  // Messaggio di conferma sul monitor seriale
}


// --- Carica le configurazioni salvate in precedenza ---
void loadConfig() {
  Serial.println("\n--- Caricamento configurazione salvata ---");
  preferences.begin("meteo-config", false);

  // Recupera la città salvata; se non trovata, mantiene "Monza" come default
  preferences.getString("city", savedCity, sizeof(savedCity));
  Serial.println("Città: " + String(savedCity));

  // Recupera la luminosità NeoPixel; se assente, imposta default 200
  savedNeopixelBrightness = preferences.getInt("brightness", 200);
  Serial.println("Luminosità NeoPixel: " + String(savedNeopixelBrightness));

  // Recupera la modalità corrente; se non esistente, imposta AUTO (0)
  Currentmode = preferences.getInt("Currentmode", 0);
  Serial.println("Modalità corrente: " + String(Currentmode));

  // Recupera impostazioni modalità notte
  forecastDay = preferences.getInt("forecastDay", 0);  // Default = oggi
  nightModeEnabled = preferences.getBool("nightModeEnabled", false);
  nightStartHour = preferences.getInt("nightStartHour", 22);
  nightStartMinute = preferences.getInt("nightStartMinute", 0);
  nightEndHour = preferences.getInt("nightEndHour", 7);
  nightEndMinute = preferences.getInt("nightEndMinute", 0);

  Serial.println("Modalità notte:");
  Serial.println(" - Abilitata: " + String(nightModeEnabled));
  Serial.println(" - Orario: " + String(nightStartHour) + ":" + String(nightStartMinute) + " -> " + String(nightEndHour) + ":" + String(nightEndMinute));

  // 🔹 Recupera la API key salvata in precedenza
  String storedApiKey = preferences.getString("api_key", "");
  if (storedApiKey.length() > 0) {
    Serial.println("API key caricata: " + storedApiKey);
  } else {
    Serial.println("⚠️  Nessuna API key salvata trovata. Sarà richiesta al prossimo accesso al portale WiFiManager.");
  }
  
  preferences.end();

  Serial.println("--- Fine caricamento configurazione ---\n");
}

// --- Callback chiamata quando le credenziali WiFi o i parametri sono stati salvati ---
void saveConfigCallback() {
  Serial.println("📡 Salvataggio configurazione WiFiManager...");

  String newApiKey = custom_api_key.getValue();
  newApiKey.trim();  // Rimuove eventuali spazi o caratteri extra

  if (newApiKey.length() > 0) {
    // Recupera l'API key attualmente salvata per evitare scritture inutili
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
  Serial.println("=== Fetching weather (OpenWeather Forecast API) ===");
  HTTPClient http;

  // Recupera la chiave API attualmente salvata
  preferences.begin("meteo-config", false);
  String currentApiKey = preferences.getString("api_key", OPENWEATHER_API_KEY);
  preferences.end();

  // Alcune città richiedono adattamento (es. Roma → Rome)
  String apiCityQuery = savedCity;
  if (apiCityQuery.equalsIgnoreCase("Roma")) {
    apiCityQuery = "Rome";
  }

  apiCityQuery = urlEncode(apiCityQuery); // <--- encoding qui!

  // Costruisce l’URL per interrogare OpenWeatherMap con unità metriche e lingua italiana
  String weatherUrl = "http://api.openweathermap.org/data/2.5/forecast?q="
                      + apiCityQuery + "," + String(savedCountryCode)
                      + "&units=metric&appid=" + currentApiKey + "&lang=it";

  Serial.print("Forecast URL: ");
  Serial.println(weatherUrl);

  http.begin(weatherUrl);     // Inizializza la richiesta HTTP
  int httpCode = http.GET();  // Effettua la chiamata GET

  // Controlla se la risposta è valida (codice 200)
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();  // Ottiene il contenuto della risposta JSON
    Serial.println("Forecast API Response:");
    Serial.println(payload);

    // Parser JSON: prepara buffer di dimensione adeguata
    DynamicJsonDocument doc(30 * 1024);
    DeserializationError error = deserializeJson(doc, payload);

    // Gestisce eventuali errori di parsing JSON
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      weatherDescription = "Errore JSON OWM.";
      return;
    }

    // Calcola indice del blocco in base al giorno selezionato
    int index = forecastDay * 8;                 // 0=oggi, 1=domani (8 blocchi=24h), 2=dopodomani (16 blocchi=48h)
    if (index >= doc["list"].size()) index = 0;  // fallback di sicurezza
    JsonObject forecast = doc["list"][index];

    // Estrae informazioni rilevanti dal JSON
    weatherDescription = forecast["weather"][0]["description"].as<String>();  // Es. "pioggia leggera"
    weatherConditionCode = forecast["weather"][0]["id"].as<int>();            // Codice meteo numerico
    temperatureCelsius = forecast["main"]["temp"].as<float>();                // Temperatura prevista
    windSpeed = forecast["wind"]["speed"].as<float>();                        // Velocità del vento in m/s
    windDirection = forecast["wind"]["deg"].as<int>();                        // Direzione del vento in gradi
    windSpeed = windSpeed * 3.6;                                              // Converto in km/h

    // Alba e tramonto disponibili solo nella sezione "city"
    sunriseTime = doc["city"]["sunrise"].as<long>();
    sunsetTime = doc["city"]["sunset"].as<long>();

    switch (Currentmode) {
      case MODO_AUTO:
        updateWeatherLEDs();        // Aggiorna LED meteo
        animateNeopixelCircular();  // Anello attivo con animazione
        break;
      case MODO_ROSSO:
      case MODO_VERDE:
      case MODO_BLU:
      case MODO_VIOLA:
      case MODO_CIANO:
      case MODO_GIALLO:
      case MODO_BIANCO:
      case MODO_RING_OFF:
        updateWeatherLEDs();  // Solo LED meteo attivi
        break;
      case MODO_LED_OFF:
      case MODO_ALL_OFF:
        // Nessuna funzione chiamata: tutto spento
        break;
    }

    // Debug seriale dettagliato
    Serial.println("--- Risultato Previsione ---");
    Serial.print("ForecastDay (0=oggi,1=domani,2=dopodomani): ");
    Serial.println(forecastDay);
    Serial.print("Indice lista usato: ");
    Serial.println(index);
    Serial.print("Descrizione Meteo: ");
    Serial.println(weatherDescription);
    Serial.print("Codice Condizione (ID): ");
    Serial.println(weatherConditionCode);
    Serial.print("Temperatura: ");
    Serial.print(temperatureCelsius);
    Serial.println(" °C");
    Serial.print("Velocità del vento m/s: ");
    Serial.println(windSpeed);
    Serial.print("Direzione: ");
    Serial.println(windDirection);
    Serial.print("Alba (da city): ");
    Serial.println(sunriseTime);
    Serial.print("Tramonto (da city): ");
    Serial.println(sunsetTime);
    Serial.println("-----------------------------");

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
  display.clearDisplay();               // Pulisce completamente il display
  display.setTextColor(SSD1306_WHITE);  // Imposta colore testo (bianco)

  // Riga superiore: nome città con font grande
  display.setTextSize(2);  // Font grande
  display.setCursor(0, 0);
  display.println(savedCity);  // Nome della città salvata

  // Riga intermedia: descrizione meteo (es. "Pioggia leggera")
  display.setTextSize(1);  // Font normale
  display.setCursor(0, 16);
  display.println(getWeatherDescriptionItalian(weatherConditionCode));  // Traduce codice meteo

  // Riga inferiore: temperatura e ora
  timeClient.update();  // Aggiorna orario da NTPClient
  // Ora, timeClient.getEpochTime() restituisce il tempo UTC.
  // Per ottenere l'ora locale (con ora legale gestita), usiamo localtime().
  time_t currentTime = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&currentTime);  // Converte in formato locale (gestisce DST)

  String tempStr = String(temperatureCelsius, 1) + " C";  // Converte la temperatura in stringa (es. "21.3 C")
  String windStr = String(windSpeed, 1) + "km/h";

  // Formatta l’orario in HH:MM
  String timeStr = "";
  char timeBuffer[6];                                           // HH:MM + terminatore \0
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", timeinfo);  // Riempie il buffer con l'ora locale
  timeStr = String(timeBuffer);                                 // Converte in String

  display.setCursor(0, 24);  // Posizione riga inferiore

  if (showTemperature) {
    display.print(tempStr);  // Mostra temperatura
  } else {
    display.print(windStr);  // Mostra velocità vento
  }
  display.print(" - ");
  display.println(timeStr);  // Mostra orario

  display.display();  // Aggiorna il contenuto del display

  // Se la modalità corrente è AUTO, aggiorna anche i LED NeoPixel
  if (Currentmode == MODO_AUTO) {
    // neopixelBootAnimationInProgress = true; // Non è necessario bloccare qui, gestito altrove
    updateWeatherLEDs();  // Imposta LED meteo (sole, nuvole, ecc.)
    if (showTemperature) {
      Serial.println("Mostro TEMPERATURA - Eseguo animateNeopixelCircular()");
      animateNeopixelCircular();  // Mostra effetto per temperatura
    } else {
      Serial.println("Mostro VENTO - Avvio animazione vento");
      // Calcola LED direzione vento
      int windLedIndex = 0;
      if (NUM_NEOPIXELS > 0) {
        windLedIndex = (360 - windDirection + 90) % 360;
        windLedIndex = map(windLedIndex, 0, 360, 0, NUM_NEOPIXELS);
      }

      windScanFrame = 0;
      windScanning = true;
      windAnimationActive = false;
    }

    // neopixelBootAnimationInProgress = false; // Non è necessario resettare qui
  }
}


// --- FUNZIONE PER MOSTRARE ANIMAZIONE METEO GRAFICA SUL DISPLAY OLED ---
void displayWeatherAnimation() {
  display.clearDisplay();               // Pulisce lo schermo OLED
  display.setTextColor(SSD1306_WHITE);  // Colore testo (usato per le bitmap bianche)

  // Aggiorna orario per distinguere giorno/notte
  timeClient.update();
  time_t currentTime = timeClient.getEpochTime();

  // Confronta l'ora corrente con l'ora di alba e tramonto, ora entrambe in ora locale
  bool isDaytime = (currentTime >= sunriseTime && currentTime < sunsetTime);

  // Variabili per l’animazione corrente
  const unsigned char **currentAnimation = nullptr;   // Puntatore a bitmap
  const unsigned char **currentAnimation1 = nullptr;  // Per doppia animazione
  int currentAnimFrames = 0;
  int currentAnimWidth = 0;
  int currentAnimHeight = 0;

  display.setTextSize(1);  // Font normale

  if (forecastDay == 0) {
    display.setCursor(54, 14);
    display.print("OGGI");
  } else if (forecastDay == 1) {
    display.setCursor(48, 14);
    display.print("DOMANI");
  } else if (forecastDay == 2) {
    display.setCursor(35, 14);
    display.print("DOPODOMANI");
  }

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

  } else if (weatherConditionCode >= 300 && weatherConditionCode < 400 || weatherConditionCode >= 500 && weatherConditionCode < 600 || weatherConditionCode >= 600 && weatherConditionCode < 700) {  //Neve
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

  } else if (weatherConditionCode >= 801 && weatherConditionCode <= 804 || weatherConditionCode >= 700 && weatherConditionCode < 799) {  //Nebbia
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
      // int x_pos = (OLED_WIDTH - currentAnimWidth) / 2;
      int x_pos = 128 - 32 - 00;
      int y_pos = (OLED_HEIGHT - currentAnimHeight) / 2;
      display.drawBitmap(x_pos, y_pos, currentAnimation[currentFrame], currentAnimWidth, currentAnimHeight, SSD1306_WHITE);

    } else {
      // Mostra due bitmap animate: una a sinistra e una a destra
      int x_pos1 = 00;
      int y_pos1 = (OLED_HEIGHT - currentAnimHeight) / 2;
      int x_pos2 = 128 - 32 - 00;
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
      ActivateCircular(pixels.Color(0, 0, 0));  // Non necessario spegnere e riaccendere l'anello qui
      delay(100);
      digitalWrite(PIN_LED_TEMPORALE, HIGH);
      ActivateCircular(pixels.Color(197, 0, 255));  // Non necessario riattivare l'anello qui
      delay(100);
    }

    tempNeopixelBaseColor = pixels.Color(197, 0, 255);

  } else if (weatherConditionCode >= 300 && weatherConditionCode < 400 || weatherConditionCode >= 700 && weatherConditionCode < 799) {
    // Pioggerella + nebbia + fumo
    digitalWrite(PIN_LED_PIOGGIA, HIGH);
    digitalWrite(PIN_LED_NUVOLE, HIGH);
    tempNeopixelBaseColor = pixels.Color(0, 0, 255);  // Blu classico

  } else if (weatherConditionCode >= 500 && weatherConditionCode < 700) {
    // Pioggia o neve
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

  } else if (weatherConditionCode >= 801 && weatherConditionCode <= 804) {
    // Sole o luna con nuvole sparse, celo coperto, nuovole
    if (isDaytime) {
      digitalWrite(PIN_LED_SOLE, HIGH);
      digitalWrite(PIN_LED_NUVOLE, HIGH);
      tempNeopixelBaseColor = pixels.Color(255, 227, 0);  // Giorno: giallo
    } else {
      digitalWrite(PIN_LED_LUNA, HIGH);
      digitalWrite(PIN_LED_NUVOLE, HIGH);
      tempNeopixelBaseColor = pixels.Color(128, 128, 128);  // Notte: tenue
    }
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
      pixels.setPixelColor(i, neopixelBaseColor);  // Imposta il colore di ogni singolo LED
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
  uint8_t animatedRed = 0;
  uint8_t animatedGreen = 0;
  uint8_t animatedBlue = 0;

  static unsigned long lastUpdate = 0;
  unsigned long currentMillis = millis();

  // Variabili per l'effetto respiro lineare (statiche per mantenere il loro stato)
  static int currentBreathBrightness = NEOPIXEL_BREATH_MIN;  // La luminosità corrente
  static bool breathIncreasing = true;                       // Flag per la direzione (aumenta/diminuisce)

  // --- Configurazioni per l'effetto di respiro ---
  // Durata desiderata per un ciclo completo di respiro (luminosità da MIN a MAX e ritorno a MIN)
  const unsigned long TARGET_CYCLE_DURATION_MS = 4000;  // 4 secondi

  // Calcola il range effettivo di luminosità che verrà percorso (es. se MIN=10, MAX=60, il range è 50)
  const int ACTUAL_BRIGHTNESS_RANGE = NEOPIXEL_BREATH_MAX - NEOPIXEL_BREATH_MIN;

  // Calcola l'intervallo di aggiornamento per ogni "passo unitario" di luminosità.
  // Un ciclo completo (da MIN a MAX e ritorno a MIN) percorre 2 * (range effettivo) passi.
  unsigned long BREATH_UPDATE_INTERVAL_MS = 0;
  if (ACTUAL_BRIGHTNESS_RANGE > 0) {  // Evita divisioni per zero se min e max sono uguali
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
      currentBreathBrightness += 1;  // Incrementa la luminosità di 1 unità
      if (currentBreathBrightness >= NEOPIXEL_BREATH_MAX) {
        currentBreathBrightness = NEOPIXEL_BREATH_MAX;  // Non superare il massimo impostato
        breathIncreasing = false;                       // Inverti la direzione: ora diminuisce
      }
    } else {
      currentBreathBrightness -= 1;  // Decrementa la luminosità di 1 unità
      if (currentBreathBrightness <= NEOPIXEL_BREATH_MIN) {
        currentBreathBrightness = NEOPIXEL_BREATH_MIN;  // Non scendere sotto il minimo impostato
        breathIncreasing = true;                        // Inverti la direzione: ora aumenta
      }
    }

    // Applica la luminosità calcolata al colore base dei NeoPixel
    uint8_t red = (neopixelBaseColor >> 16) & 0xFF;
    uint8_t green = (neopixelBaseColor >> 8) & 0xFF;
    uint8_t blue = neopixelBaseColor & 0xFF;

    if (neopixelBaseColor == pixels.Color(0, 0, 0)) {
      pixels.clear();  // Se il colore base è nero, spegni i pixel
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
    pixels.show();  // Aggiorna i NeoPixel con la nuova luminosità
  }
}

void runWindAnimation() {
  static unsigned long lastTrailUpdate = 0;

  const unsigned long SCIA_TOTAL_DURATION = 4000;  // Durata totale animazione scia (ms)
  const int FADE_FRAMES_PER_LED = 8;
  const int FADE_PEAK = FADE_FRAMES_PER_LED / 2;

  int beaufort = getBeaufortScale(windSpeed);
  int trailLength = getTrailLengthByBeaufort(beaufort);
  uint8_t windR, windG, windB;
  getWindColorByBeaufort(beaufort, windR, windG, windB);

  // Calcolo posizione del LED per la direzione del vento
  int windLedIndex = (360 - windDirection + 90) % 360;
  windLedIndex = map(windLedIndex, 0, 360, 0, NUM_NEOPIXELS);

  int totalFrames = (trailLength + 1) * FADE_FRAMES_PER_LED;
  unsigned long frameDuration = SCIA_TOTAL_DURATION / max(totalFrames, 1);

  // Buffer per i colori dei LED
  uint8_t ledR[NUM_NEOPIXELS] = { 0 };
  uint8_t ledG[NUM_NEOPIXELS] = { 0 };
  uint8_t ledB[NUM_NEOPIXELS] = { 0 };

  // Effetto scia
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

  // LED centrale sempre acceso
  ledR[windLedIndex] = windR;
  ledG[windLedIndex] = windG;
  ledB[windLedIndex] = windB;

  // Aggiorna i LED
  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(ledR[i], ledG[i], ledB[i]));
  }
  pixels.show();

  // Avanza frame animazione
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
    r = g = b = 0;  // LED spento per calma
  } else if (beaufort <= 3) {
    r = 0;
    g = 255;
    b = 0;  // Verde
  } else if (beaufort <= 6) {
    r = 255;
    g = 165;
    b = 0;  // Arancione
  } else if (beaufort <= 9) {
    r = 255;
    g = 0;
    b = 0;  // Rosso
  } else {
    r = 128;
    g = 0;
    b = 128;  // Viola
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
  return 6;  // 10, 11, 12
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
bool buttonCurrentlyPressed = false;  // Stato attuale del pulsante

void handleButtonPress() {
  // Leggi lo stato attuale del pulsante
  bool reading = digitalRead(PIN_PULSANTE);

  // Debounce: verifica se lo stato è stabile
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Se lo stato è cambiato in modo stabile
    if (reading != buttonState) {  // buttonState è lo stato "attuale stabile"
      buttonState = reading;       // Aggiorna lo stato stabile

      if (buttonState == LOW) {  // Pulsante è stato PREMUTO (da HIGH a LOW)
        keypressed = true;
        buttonPressStartTime = millis();  // Registra il tempo di inizio pressione
        Serial.println("Pulsante premuto (inizia timer).");
      } else {  // buttonState == HIGH : Pulsante è stato RILASCIATO (da LOW a HIGH)
        unsigned long pressDuration = millis() - buttonPressStartTime;
        Serial.printf("Pulsante rilasciato. Durata: %lu ms.\n", pressDuration);

        if (isLongPressMode) {
          // Se eravamo in modalità pressione lunga, usciamo
          isLongPressMode = false;
          Serial.println("Modalita' regolazione luminosita' terminata.");
          // Pulisci il display o ripristina la visualizzazione normale
          display.clearDisplay();  // Pulisci l'area della barra
          // Potresti voler ricaricare qui la visualizzazione meteo/ora
          // o impostare un flag per farlo nel loop principale
          showText = true;  // Assumi che showText ripristini la visualizzazione principale
          saveConfig();     // Salva la luminosità finale
          keypressed = false;
        } else {
          // Pressione breve (rilasciato prima della soglia di pressione lunga)
          Serial.println("Pressione breve rilevata. Cambio modalita'.");
          Currentmode++;
          if (Currentmode > MAX_MODE) Currentmode = MODO_AUTO;
          showText = true;

          SetRingColor(Currentmode);  // Applica il colore della nuova modalità
          saveConfig();               // Salva le impostazioni
          fetchWeather();             // Esegui la tua azione originale
          keypressed = false;
        }
      }
    }
  }

  lastButtonState = reading;  // Salva lo stato precedente per il debounce

  // Gestione della pressione lunga *mentre il pulsante è tenuto premuto*
  // e solo DOPO che la soglia è stata superata.
  if (buttonState == LOW && !isLongPressMode && (millis() - buttonPressStartTime) >= LONG_PRESS_THRESHOLD) {
    isLongPressMode = true;  // Entra nella modalità pressione lunga
    Serial.println("Pressione lunga rilevata. Inizio regolazione luminosita'.");
    display.clearDisplay();  // Pulisci il display per la barra
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("    Luminosita':");
    display.display();
    lastBrightnessChangeTime = millis();  // Inizializza il timer per la luminosità
    // Applica subito la luminosità corrente per visualizzare lo stato iniziale
    pixels.setBrightness(savedNeopixelBrightness);
    pixels.show();  // forza il refresh immediato
  }

  // Se siamo in modalità pressione lunga (pulsante premuto per oltre 1 sec)
  if (isLongPressMode && buttonState == LOW) {
    if ((millis() - lastBrightnessChangeTime) > BRIGHTNESS_CHANGE_INTERVAL) {
      lastBrightnessChangeTime = millis();

      savedNeopixelBrightness += 1;
      if (savedNeopixelBrightness > 255) {
        savedNeopixelBrightness = 10;  // Torna a zero
      }
      pixels.setBrightness(savedNeopixelBrightness);
      pixels.show();  // forza il refresh immediato
      drawBrightnessBar(savedNeopixelBrightness);
    }
  }
}


// --- Funzione per aggiornare i NeoPixel con la nuova luminosità
// Questa funzione è utile per applicare solo la luminosità senza cambiare il colore di base
void updateNeoPixelBrightness(uint8_t brightness) {
  uint8_t r_base = (pixels.getPixelColor(0) >> 16) & 0xFF;  // Prendi il colore base dal primo pixel
  uint8_t g_base = (pixels.getPixelColor(0) >> 8) & 0xFF;
  uint8_t b_base = (pixels.getPixelColor(0) & 0xFF);

  // Se i pixel erano spenti, potremmo voler dare un colore di default per la regolazione
  if (r_base == 0 && g_base == 0 && b_base == 0 && brightness > 0) {
    r_base = 0;
    g_base = 0;
    b_base = 255;  // Blu di default per la regolazione se erano spenti
  }

  uint8_t r = (uint8_t)((float)r_base * (brightness / 255.0f));
  uint8_t g = (uint8_t)((float)g_base * (brightness / 255.0f));
  uint8_t b = (uint8_t)((float)b_base * (brightness / 255.0f));

  pixels.fill(pixels.Color(r, g, b));  // Applica il colore con la nuova luminosità a tutti i pixel
  pixels.show();
}


// --- Funzione per disegnare la barra di luminosità sul display OLED ---
void drawBrightnessBar(uint8_t brightness) {
  display.clearDisplay();  // Pulisci il display
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("    Luminosita':");

  // Disegna la barra di progresso
  int barWidth = map(brightness, 0, 255, 0, display.width() - 10);  // Mappa 0-255 alla larghezza del display
  int barHeight = 10;
  int startX = 5;
  int startY = 20;

  display.drawRect(startX, startY, display.width() - 10, barHeight, SSD1306_WHITE);  // Bordo della barra
  display.fillRect(startX, startY, barWidth, barHeight, SSD1306_WHITE);              // Barra riempita

  display.display();  // Aggiorna il display
}


// Imposta il colore dell’anello NeoPixel in base alla modalità selezionata
void SetRingColor(int Mode) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);  // Dimensione testo 2
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
    case MODO_BIANCO:  // bianco
      neopixelBaseColor = pixels.Color(255, 255, 255);
      display.println("   BIANCO");
      break;
    case MODO_RING_OFF:  // anello spento
      neopixelBaseColor = pixels.Color(0, 0, 0);
      display.println("ANELLO SP.");
      break;
    case MODO_LED_OFF:  // LED spenti
      neopixelBaseColor = pixels.Color(0, 0, 0);
      AllLEDoff();
      display.println("LED SPENTI");
      break;
    case MODO_ALL_OFF:  // Tutto Spento
      neopixelBaseColor = pixels.Color(0, 0, 0);
      AllLEDoff();
      display.println("   SPENTO");
      break;
  }
  display.display();
  animateNeopixelCircular();  // Esegue l'animazione anello che si accende
}

void checkNightMode() {
  static bool nightModeActive = false;

  if (!nightModeEnabled) {
    if (nightModeActive) {
      Currentmode = savedMode;
      nightModeActive = false;
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
    if (Currentmode != MODO_RING_OFF) {
      savedMode = Currentmode;
      Currentmode = MODO_RING_OFF;
      nightModeActive = true;

      // 🔴 Spegni LED e ferma animazioni
      windAnimationActive = false;
      windScanning = false;
      pixels.clear();  // Spegne tutti i LED all’inizio
      pixels.show();

      Serial.println("🕯️ Entrata in modalità notturna (MODO_RING_OFF): LED spenti, animazioni fermate");
    }
  } else if (!isNight && nightModeActive) {
    Currentmode = savedMode;
    nightModeActive = false;
    Serial.println("🌅 Uscita dalla modalità notturna. Ripristino modalità precedente.");
  }
}


// --- IMPLEMENTAZIONE GESTORI SERVER WEB STANDARD ---

// DEVI ASSICURARTI CHE QUESTA VARIABILE SIA DICHIARATA GLOBALMENTE
// E AGGIORNATA DENTRO fetchWeather() DOPO AVER PARSATO LA RISPOSTA JSON.
// Esempio: int lastFetchedWeatherId = 800; // Inizializza con un valore di default
// extern int lastFetchedWeatherId; // Dichiarazione se definita altrove nel codice (commentato perché non usata direttamente qui)


void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>" + ProductName + " Config</title><style>";
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
  html += "</style></head><body><div class=\"container\"><h2>" + ProductName + " Config</h2>";

  // Meteo Corrente
  html += "<p><strong>Meteo Corrente:</strong> " + getWeatherDescriptionItalian(weatherConditionCode) + "</p>";

  html += "<form action=\"/save\" method=\"post\">";

  // Luminosità
  html += "<label for=\"brightness\">Luminosita' Generale:</label>";
  html += "<input type=\"range\" id=\"brightness\" name=\"brightness\" min=\"10\" max=\"255\" value=\"" + String(savedNeopixelBrightness) + "\">";
  html += "<span id=\"brightnessValue\">" + String(savedNeopixelBrightness) + "</span>";

  // Attenuazione notturna (checkbox)
  html += "<label for=\"night_mode\">Attenuazione Notturna:</label>";
  html += "<label class=\"toggle-switch\">";
  html += String("<input type=\"checkbox\" id=\"night_mode\" name=\"night_mode\" ") + (nightModeEnabled ? "checked" : "") + ">";
  html += "<span class=\"slider\"></span></label>";

  // Orari di inizio/fine (con zero-padding)
  html += "<label for=\"night_start\">Ora Inizio Attenuazione:</label>";
  html += String("<input type=\"time\" id=\"night_start\" name=\"night_start\" value=\"") + (nightStartHour < 10 ? "0" : "") + String(nightStartHour) + ":" + (nightStartMinute < 10 ? "0" : "") + String(nightStartMinute) + "\">";

  html += "<label for=\"night_end\">Ora Fine Attenuazione:</label>";
  html += String("<input type=\"time\" id=\"night_end\" name=\"night_end\" value=\"") + (nightEndHour < 10 ? "0" : "") + String(nightEndHour) + ":" + (nightEndMinute < 10 ? "0" : "") + String(nightEndMinute) + "\">";

  // Modalità
  html += "<label for=\"mode_select\">Seleziona modalita':</label>";
  html += "<select id=\"mode_select\" name=\"mode_select\">";
  String modes[] = {
    "Automatica", "Luce rossa", "Luce Verde", "Luce Blu", "Luce Viola",
    "Luce Azzurra", "Luce Gialla", "Luce Bianca", "Anello Spento",
    "LED Spenti", "Tutto Spento"
  };
  for (int i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
    html += "<option value=\"" + String(i) + "\"" + (Currentmode == i ? " selected" : "") + ">" + modes[i] + "</option>";
  }
  html += "</select>";

  // Elenco città italiane
  html += "<label for=\"city_select\">Seleziona Citta':</label>";
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

  // Campo città manuale
  html += "<label for=\"city_manual\">Inserisci Citta' manualmente (se non presente nell'elenco):</label>";
  html += "<input type=\"text\" id=\"city_manual\" name=\"city_manual\" value=\"" + String(savedCity) + "\">";

  // Giorno previsione
  html += "<label for=\"forecastDay\">Previsione da mostrare:</label>";
  html += "<select id=\"forecastDay\" name=\"forecastDay\">";
  String giorni[] = { "Oggi", "Domani", "Dopodomani" };
  for (int i = 0; i < 3; i++) {
    html += "<option value=\"" + String(i) + "\"" + (forecastDay == i ? " selected" : "") + ">" + giorni[i] + "</option>";
  }
  html += "</select>";

  // Bottone salva
  html += "<button type=\"submit\">Salva Impostazioni</button>";
  html += "</form>";

  // Script: slider + sincronizzazione città
  html += "<script>";
  html += "var slider = document.getElementById('brightness');";
  html += "var output = document.getElementById('brightnessValue');";
  html += "output.innerHTML = slider.value;";
  html += "slider.oninput = function() { output.innerHTML = this.value; };";

  html += "var cityManual = document.getElementById('city_manual');";
  html += "var citySelect = document.getElementById('city_select');";

  html += "citySelect.addEventListener('change', function() {";
  html += "  if (this.value !== '') { cityManual.value = ''; }";
  html += "});";

  html += "cityManual.addEventListener('input', function() {";
  html += "  if (this.value.trim() !== '') { citySelect.selectedIndex = 0; }";
  html += "});";
  html += "</script>";

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}




void handleSave() {

  for(uint8_t i=0; i < server.args(); i++ ){
    Serial.print("Arg ");
    Serial.print(server.argName(i));
    Serial.print("=");
    Serial.println(server.arg(i));
  }

  // Gestione Luminosità
  if (server.hasArg("brightness")) {
    savedNeopixelBrightness = server.arg("brightness").toInt();
    pixels.setBrightness(savedNeopixelBrightness);
    pixels.show();
  }

  nightModeEnabled = (server.hasArg("night_mode") && server.arg("night_mode") == "on");

  // Ricezione orari
  if (server.hasArg("night_start")) {
    String startTime = server.arg("night_start");  // formato "HH:MM"
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

  // Gestione Modalità
  String modes[] = {
    "Automatica", "Luce rossa", "Luce Verde", "Luce Blu", "Luce Viola", "Luce Azzurra", "Luce Gialla", "Luce Bianca", "Anello Spento", "LED Spenti", "Tutto Spento"
  };

  if (server.hasArg("mode_select")) {
    int selectedModeInt = server.arg("mode_select").toInt();

    if (selectedModeInt >= 0 && selectedModeInt <= MAX_MODE) {
      if (selectedModeInt != Currentmode) {
        Currentmode = selectedModeInt;

        if (Currentmode == 0) {  // Se la modalità "Automatica" (indice 0) è selezionata
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

  // Gestione Giorno Previsione
  if (server.hasArg("forecastDay")) {
    forecastDay = server.arg("forecastDay").toInt();
    if (forecastDay < 0 || forecastDay > 2) forecastDay = 0;  // fallback
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
  Serial.println(" - Night Mode: " + String(nightModeEnabled));
  Serial.println(" - Notte: " + String(nightStartHour) + ":" + String(nightStartMinute) + " -> " + String(nightEndHour) + ":" + String(nightEndMinute));



  // Reindirizzamento
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
  delay(5);  // Aggiungi un piccolo ritardo per permettere al server di inviare la risposta
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

