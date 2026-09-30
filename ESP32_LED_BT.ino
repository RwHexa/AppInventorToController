/*
  ESP32_LED_BT – LED per Bluetooth (App Inventor) schalten und dimmen
  -------------------------------------------------------------------
  Gegenstück zur App "LedSteuerung.aia".

  Hardware : klassischer ESP32 (z. B. ESP32-DevKitC / WROOM-32).
             ESP32-S3 und ESP32-C3 haben KEIN klassisches Bluetooth
             und funktionieren mit diesem Sketch nicht (dort BLE nötig).
  LED      : GPIO 2 (Onboard-LED vieler DevKits) oder externe LED
             mit 220-Ω-Vorwiderstand gegen GND.
  Core     : Arduino-ESP32 ab Version 3.x (ledcAttach/ledcWrite mit Pin).

  Protokoll (Textzeilen, Abschluss mit '\n'):
    App -> ESP32   "ON"        LED einschalten
                   "OFF"       LED ausschalten
                   "B:0..255"  Helligkeit setzen
                   "?"         nur Status anfordern
    ESP32 -> App   "S;B;T"     z. B. "1;128;41.5"
                   S = tatsächlicher Ausgang (1 = PWM-Tastgrad > 0)
                   B = eingestellte Helligkeit 0..255
                   T = Temperatur in °C
  Der Status wird nach jedem Befehl und zusätzlich jede Sekunde gesendet.
*/

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Klassisches Bluetooth ist fuer dieses Board nicht verfuegbar (ESP32-S3/C3?)."
#endif

BluetoothSerial BT;

const char* GERAETENAME   = "ESP32-LED";   // so erscheint der ESP32 am Handy
const int   LED_PIN       = 2;
const int   PWM_FREQUENZ  = 5000;          // Hz
const int   PWM_AUFLOESUNG = 8;            // Bit -> 0..255

bool          ledAn         = false;
int           helligkeit    = 128;
String        puffer;
unsigned long letzterStatus = 0;

// Schreibt den Sollzustand auf den PWM-Ausgang
void ausgangSetzen() {
  ledcWrite(LED_PIN, ledAn ? helligkeit : 0);
}

// Liest den TATSÄCHLICHEN Ausgang (PWM-Register) und sendet den Status
void statusSenden() {
  int   istTastgrad = ledcRead(LED_PIN);
  int   s           = (istTastgrad > 0) ? 1 : 0;
  float temperatur  = temperatureRead();   // interner Chip-Sensor, nur grob
  // Später durch echten Sensor ersetzen, z. B. DS18B20 oder DHT22.
  BT.printf("%d;%d;%.1f\n", s, helligkeit, temperatur);
}

void befehlAusfuehren(String befehl) {
  befehl.trim();
  if (befehl == "ON") {
    ledAn = true;
  } else if (befehl == "OFF") {
    ledAn = false;
  } else if (befehl.startsWith("B:")) {
    helligkeit = constrain(befehl.substring(2).toInt(), 0, 255);
  } else if (befehl == "?") {
    // nur Status
  } else {
    Serial.printf("Unbekannter Befehl: %s\n", befehl.c_str());
    return;
  }
  ausgangSetzen();
  statusSenden();
  Serial.printf("Befehl: %-8s -> an=%d, hell=%d\n", befehl.c_str(), ledAn, helligkeit);
}

void setup() {
  Serial.begin(115200);
  ledcAttach(LED_PIN, PWM_FREQUENZ, PWM_AUFLOESUNG);
  ausgangSetzen();
  BT.begin(GERAETENAME);
  Serial.printf("Bluetooth gestartet als \"%s\"\n", GERAETENAME);
}

void loop() {
  // Eingehende Zeichen sammeln, bei Zeilenende auswerten
  while (BT.available()) {
    char c = BT.read();
    if (c == '\n') {
      befehlAusfuehren(puffer);
      puffer = "";
    } else if (c != '\r') {
      puffer += c;
      if (puffer.length() > 32) puffer = "";   // Schutz vor Müll
    }
  }

  // Regelmäßiger Status, solange ein Handy verbunden ist
  if (BT.hasClient() && millis() - letzterStatus >= 1000) {
    letzterStatus = millis();
    statusSenden();
  }
}
