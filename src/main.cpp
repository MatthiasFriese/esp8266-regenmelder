#include <ESP8266WiFi.h>          // Replace with WiFi.h for ESP32
#include <ESP8266WebServer.h>     // Replace with WebServer.h for ESP32
#include <AutoConnect.h>
#include <ESP8266mDNS.h>

int RAINQUANTITYCOUNTER_PIN = 5;    // select the input pin for the potentiometer
#define RAINCOUNTER_MAX            0x7FFF
#define LED_ON 0
#define LED_OFF 1

volatile bool triggered;
int raincounter = 0;
unsigned long lastRainQuantityCounterFlip = 0;
unsigned long minFlipDelay = 500;

AutoConnect portal;
AutoConnectConfig  config;  

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

bool startCP(IPAddress ip) {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("C.P. started, IP:" + WiFi.localIP().toString());
  return true;
}

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200); // Öffnet die serielle Schnittstelle bei 9600 Bit/s:
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RAINQUANTITYCOUNTER_PIN, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(RAINQUANTITYCOUNTER_PIN), rainquantitycounterISR, RISING);

  digitalWrite(LED_BUILTIN, LOW);
  config.portalTimeout = 180000;
  config.apid = "ESP-" + String(ESP.getChipId(), HEX);
  config.psk = "autoconnect";
  portal.config(config);
  portal.onDetect(startCP);
  if (portal.begin()) {
    if (MDNS.begin(config.apid)) {
      MDNS.addService("http", "tcp", 80);
    }
    digitalWrite(LED_BUILTIN, LOW);
  }
}

// the loop function runs over and over again forever
void loop() {
  portal.handleClient();

  if (triggered == true) {
    triggered = false;
    raincounter++;
    Serial.print("Rain counter is:");
    Serial.print(raincounter);
    Serial.println();
  }
  
  if (raincounter % 2 == 0) {
     digitalWrite(LED_BUILTIN, LED_OFF);
  } else {
    digitalWrite(LED_BUILTIN, LED_ON);
  }
}
