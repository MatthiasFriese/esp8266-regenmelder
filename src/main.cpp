#include <ConfigManager.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

const char *settingsHTML = (char *)"/settings.html";
const char *stylesCSS = (char *)"/styles.css";
const char *mainJS = (char *)"/main.js";

struct Config {
  char mqttserver[32];
  char customTopic[255];
  char mqttUsername[255];
  char mqttPassword[255];
} config;

struct Metadata {
  int8_t version;
} meta;


int RAINQUANTITYCOUNTER_PIN = 5;    // select the input pin for the potentiometer
#define RAINCOUNTER_MAX            0x7FFF
#define LED_ON 0
#define LED_OFF 1

volatile bool triggered;
int raincounter = 0;
unsigned long lastRainQuantityCounterFlip = 0;
unsigned long minFlipDelay = 500;

ICACHE_RAM_ATTR void rainquantitycounterISR() {
  Serial.print("Interrupt triggered...");
  unsigned long currentMillis = millis();
  if (currentMillis - lastRainQuantityCounterFlip >= minFlipDelay) {
    Serial.println("update flag.");
    lastRainQuantityCounterFlip = currentMillis;
    triggered = true;
  } else {
    Serial.println("ignore trigger.");
  }
}

ConfigManager configManager;
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
bool mqttConfigured = false;

void APCallback(WebServer *server) {
    server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
        configManager.streamFile(stylesCSS, mimeCSS);
    });

    DebugPrintln(F("AP Mode Enabled. You can call other functions that should run after a mode is enabled ... "));
}


void APICallback(WebServer *server) {
  server->on("/", HTTPMethod::HTTP_GET, [server](){
    server->sendHeader("Location", "/settings.html",true); //Redirect to our html web page 
    server->send(302, "text/plain",""); 
  });
  server->on("/disconnect", HTTPMethod::HTTP_GET, [server](){
    configManager.clearWifiSettings(false);
  });
  
  server->on("/settings.html", HTTPMethod::HTTP_GET, [server](){
    configManager.streamFile(settingsHTML, mimeHTML);
  });
  
  // NOTE: css/js can be embedded in a single page HTML 
  server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
    configManager.streamFile(stylesCSS, mimeCSS);
  });
  
  server->on("/main.js", HTTPMethod::HTTP_GET, [server](){
    configManager.streamFile(mainJS, mimeJS);
  });

}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char value[length]; 
  for (unsigned int i = 0; i < length; i++) {
    value[i] = (char)payload[i];
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strstr(topic, "rainCounter") != NULL) {
    int newCounter = atoi(value);
    if (raincounter != newCounter) {
      Serial.print("Update rainCounter with ");
      Serial.println(newCounter);
      raincounter = newCounter;
    } else {
      Serial.println("Ignore update of rainCounter");
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  Serial.println("Will start reconnection");
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection with clientId ");
    // Create a random client ID
    const char *clientId = "ESP8266Client-Regenmesser";
    Serial.print(clientId);
    Serial.print("...");
    // Attempt to connect
    if (client.connect(clientId, config.mqttUsername, config.mqttPassword)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      String lwtTopic = config.customTopic;
      lwtTopic += "/LWT";
      char connectedString[] = "connected";
      client.publish(lwtTopic.c_str(), connectedString);
      // ... and resubscribe
      String subsribedTopic = config.customTopic;
      subsribedTopic += "/rainCounter";
      client.subscribe(subsribedTopic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200); // Öffnet die serielle Schnittstelle bei 9600 Bit/s:
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RAINQUANTITYCOUNTER_PIN, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(RAINQUANTITYCOUNTER_PIN), rainquantitycounterISR, RISING);

  digitalWrite(LED_BUILTIN, LOW);
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  DebugPrintln(F(""));

  meta.version = 6;

  // Setup config manager
  configManager.setAPName("ESP-Regenmelder");
  configManager.setAPFilename("/index.html");

  // Settings variables 
  configManager.addParameter("mqttserver", config.mqttserver, 32);
  configManager.addParameter("customTopic", config.customTopic, 255);
  configManager.addParameter("mqttUsername", config.mqttUsername, 255);
  configManager.addParameter("mqttPassword", config.mqttPassword, 255);

  // Meta Settings
  configManager.addParameter("version", &meta.version, get);

  // Init Callbacks
  configManager.setAPCallback(APCallback);
  configManager.setAPICallback(APICallback);

  configManager.begin(config);
}

// the loop function runs over and over again forever
void loop() {
  configManager.loop();

  if (mqttConfigured == false && configManager.getMode() == Mode::api) {
    mqttConfigured = true;
    Serial.print("configure MQTT Server: ");
    Serial.print(config.mqttserver);
    client.setServer(config.mqttserver, 1883);
    client.setCallback(callback);
    delay(500);
    Serial.println("configured MQTT Server");
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (triggered == true) {
    triggered = false;
    raincounter++;
    Serial.print("Rain counter is:");
    Serial.print(raincounter);
    Serial.println();
    char stringValue[5];
    itoa(raincounter, stringValue, 10);
    String topic = config.customTopic;
    topic += "/rainCounter";
    client.publish(topic.c_str(), stringValue);
    Serial.println("published on MQTT");
  }
  
  if (raincounter % 2 == 0) {
     digitalWrite(LED_BUILTIN, LED_OFF);
  } else {
    digitalWrite(LED_BUILTIN, LED_ON);
  }
}
