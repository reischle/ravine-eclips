#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>

// ============================================================================
// CONFIGURATION & UUIDS
// ============================================================================
static const NimBLEUUID NORDIC_SERVICE_UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static const NimBLEUUID NORDIC_RX_CHAR_UUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"); // WRITE (Trigger)
static const NimBLEUUID NORDIC_TX_CHAR_UUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"); // NOTIFY (Empfang)

static const NimBLEUUID ZEPHYR_SMP_SERVICE_UUID("8D53DC1D-1DB7-4CD3-868B-8A527460AA84");
static const NimBLEUUID ZEPHYR_SMP_CHAR_UUID("DA2E7828-FBCE-4E01-AE9E-261174997C48");


// =========================IMPORTANT===================================================
const uint32_t BLE_PASSKEY = 111111; // Important: put your PIN here. It is the last 6 digits of the eclips serial number. Leave out leading zeroes
// =========================IMPORTANT===================================================
 

// ============================================================================
// NEOPIXEL RING CONFIGURATION
// ============================================================================
#define LED_PIN    23 
#define NUM_LEDS   12
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Globaler Speicher für den aktuell ausgelesenen Stromwert (iBat in mA)
volatile int currentIbat = 0;

// ============================================================================
// RINGPUFFER FÜR DEN DATENSTRUM (STABILITÄTSREGELN)
// ============================================================================
#define RING_BUF_SIZE 1024
uint8_t ringBuffer[RING_BUF_SIZE];
volatile size_t ringHead = 0;
volatile size_t ringTail = 0;

void pushToRingBuffer(uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        size_t nextHead = (ringHead + 1) % RING_BUF_SIZE;
        if (nextHead != ringTail) {
            ringBuffer[ringHead] = data[i];
            ringHead = nextHead;
        }
    }
}

bool popFromRingBuffer(uint8_t& val) {
    if (ringHead == ringTail) return false;
    val = ringBuffer[ringTail];
    ringTail = (ringTail + 1) % RING_BUF_SIZE;
    return true;
}

const uint8_t initFrame[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x06, 0xA0 };

// ============================================================================
// BLE VARIABLEN & CLASS CALLBACKS
// ============================================================================
NimBLEAdvertisedDevice* advDevice = nullptr;
NimBLEClient* pClient = nullptr;
NimBLERemoteCharacteristic* pSmpChar = nullptr;
NimBLERemoteCharacteristic* pTxChar = nullptr;
NimBLERemoteCharacteristic* pRxChar = nullptr;

volatile bool doConnect = false;
volatile bool connected = false;

void telemetryCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    pushToRingBuffer(pData, length);
}

void smpCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    // SMP-Pakete sind für die Anzeige im Datenstrom nicht nötig
}

class MyClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        Serial.println("[CLIENT] Verbunden!");
    }
    
    void onDisconnect(NimBLEClient* pClient, int reason) override {
        connected = false;
        pSmpChar = nullptr;
        pTxChar = nullptr;
        pRxChar = nullptr;
        Serial.printf("[CLIENT] Getrennt! Grund-Code: %d\n", reason);
    }

    void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
        Serial.println("[SECURITY] Passkey-Anforderung vom Fahrrad erhalten!");
        NimBLEDevice::injectPassKey(connInfo, BLE_PASSKEY);
    }

    void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t passkey) override {
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        if (connInfo.isEncrypted()) {
            Serial.println("[SECURITY] Kanal erfolgreich verschluesselt!");
        }
    }
};

class MyScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        std::string devName = advertisedDevice->getName();
        if (devName.find("Canyon") != std::string::npos || devName.find("PS") != std::string::npos) {
            Serial.printf("CANYON ECLIPS GEFUNDEN! RSSI: %d dBm\n", advertisedDevice->getRSSI());
            NimBLEDevice::getScan()->stop();
            if (advDevice != nullptr) {
                delete advDevice;
            }
            advDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
            doConnect = true;
        }
    }
};

// ============================================================================
// NEOPIXEL UPDATE FUNKTION (NON-BLOCKING)
// ============================================================================
void updateBargraph(int value) {
    strip.clear(); // Vorherige Anzeige zurücksetzen

    if (value > 0) {
        // Positiver Bereich: 1 bis 1500 mA wird auf 1 bis 12 LEDs gemappt (Grün)
        int numLedsToLight = map(value, 1, 1500, 1, NUM_LEDS);
        numLedsToLight = constrain(numLedsToLight, 1, NUM_LEDS);
        for (int i = 0; i < numLedsToLight; i++) {
            strip.setPixelColor(i, strip.Color(0, 255, 0)); // Grün
        }
    } else if (value < 0) {
        // Negativer Bereich: -1 bis -2500 mA wird auf 1 bis 12 LEDs gemappt (Rot)
        int numLedsToLight = map(abs(value), 1, 2500, 1, NUM_LEDS);
        numLedsToLight = constrain(numLedsToLight, 1, NUM_LEDS);
        for (int i = 0; i < numLedsToLight; i++) {
            strip.setPixelColor(i, strip.Color(255, 0, 0)); // Rot
        }
    }
    // Bei genau 0 bleiben alle LEDs aus (durch strip.clear())
    strip.show(); // Änderungen auf den Ring übertragen
}

// ============================================================================
// VERBINDUNGSAUFBAU & INITIALISIERUNG
// ============================================================================
bool connectAndStartFullSequence() {
    Serial.println("Verbinde mit Canyon Power Supply...");
    if (!pClient) {
        pClient = NimBLEDevice::createClient();
        pClient->setClientCallbacks(new MyClientCallbacks(), false);
    }

    if (!pClient->connect(advDevice)) {
        Serial.println("Fehler: Verbindung fehlgeschlagen.");
        return false;
    }

    Serial.println("Verhandle MTU-Größe...");
    pClient->exchangeMTU();
    delay(200);

    // --- SCHRITT 1: Services entdecken ---
    Serial.println("Suche Services...");
    NimBLERemoteService* pSmpSvc = pClient->getService(ZEPHYR_SMP_SERVICE_UUID);
    NimBLERemoteService* pNordicSvc = pClient->getService(NORDIC_SERVICE_UUID);
    
    if (!pNordicSvc) {
        Serial.println("Fehler: Nordic UART Service nicht auf dem Bike gefunden!");
        return false;
    }

    pTxChar = pNordicSvc->getCharacteristic(NORDIC_TX_CHAR_UUID);
    pRxChar = pNordicSvc->getCharacteristic(NORDIC_RX_CHAR_UUID);

    if (!pTxChar || !pRxChar) {
        Serial.println("Fehler: NUS TX oder RX Charakteristik fehlt!");
        return false;
    }

    // --- SCHRITT 2: Pairing & Verschlüsselung anfordern ---
    Serial.println("Fordere sichere Verbindung (Security Level 3/4) an...");
    pClient->secureConnection();
    
    Serial.println("Warte auf Schlüssel-Handshake...");
    unsigned long startWait = millis();
    bool isSecured = false;
    while (millis() - startWait < 8000) {
        if (pClient->getConnInfo().isEncrypted()) {
            isSecured = true;
            break;
        }
        delay(100);
    }
    
    if (!isSecured) {
        Serial.println("[SECURITY] WARNUNG: Handshake-Timeout!");
        return false;
    }

    // --- SCHRITT 3: SMP-Initialisierung (Weckruf) ---
    if (pSmpSvc) {
        pSmpChar = pSmpSvc->getCharacteristic(ZEPHYR_SMP_CHAR_UUID);
        if (pSmpChar) {
            pSmpChar->subscribe(true, smpCallback, false);
            delay(100);
            pSmpChar->writeValue(initFrame, sizeof(initFrame), true);
            delay(300);
        }
    }

    // --- SCHRITT 4: Nordic UART TX abonnieren ---
    Serial.print("Abonniere Telemetrie...");
    if (pTxChar->subscribe(true, telemetryCallback, true)) {
        Serial.println(" OK!");
    } else {
        if (pTxChar->subscribe(true, telemetryCallback, false)) {
            Serial.println(" OK (ohne Bestaetigung)!");
        } else {
            Serial.println(" Fehlgeschlagen!");
            return false;
        }
    }

    delay(250);

    // Trigger senden (0x0E\r\n)
    uint8_t triggerCmd[] = { 0x0E, 0x0D, 0x0A };
    if (pRxChar->writeValue(triggerCmd, sizeof(triggerCmd), false)) {
        Serial.println("Datenstrom-Trigger erfolgreich gesendet!");
    } else {
        Serial.println("Fehler beim Senden des Triggers.");
        return false;
    }

    connected = true;
    return true;
}

// ============================================================================
// MAIN SETUP & LOOP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- Canyon Eclips Telemetrie-Empfaenger (NeoPixel Ring Mode) ---");

    // NeoPixel Ring initialisieren
    strip.begin();
    strip.setBrightness(10); // Helligkeit drosseln (~12% Helligkeit)
    strip.clear();
    strip.show(); // Alle LEDs ausschalten

    NimBLEDevice::init("ESP32-Canyon-Receiver");
    NimBLEDevice::deleteAllBonds(); 
    NimBLEDevice::setMTU(498);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); 

    NimBLEDevice::setSecurityAuth(true, true, true); 
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
    NimBLEDevice::setSecurityPasskey(BLE_PASSKEY);

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(new MyScanCallbacks(), false);
    pScan->setActiveScan(true);
    pScan->setInterval(97);
    pScan->setWindow(37);
    
    Serial.println("Starte Bluetooth Scan nach Canyon Power Supply...");
    pScan->start(10, false);
}

void loop() {
    if (doConnect) {
        doConnect = false;
        if (!connectAndStartFullSequence()) {
            delay(2000);
            NimBLEDevice::getScan()->start(10, false);
        }
    }

    // Ringpuffer im Haupt-Thread auslesen und direkt als Datenstrom ausgeben
    static char lineBuffer[128];
    static size_t lineIndex = 0;
    uint8_t b;

    while (popFromRingBuffer(b)) {
        if (b == '\n' || b == '\r') {
            if (lineIndex > 0) {
                lineBuffer[lineIndex] = '\0';
                
                // Hier geben wir den reinen Datenstrom direkt aus
                Serial.println(lineBuffer);
                
                // --- PARSEN DES AKTUELLEN STROMS (iBat) ---
                const char* p = strstr(lineBuffer, "iBat:");
                if (p != nullptr) {
                    currentIbat = atoi(p + 5); // Liest den Integer-Wert nach "iBat: "
                }
                
                lineIndex = 0;
            }
        } else {
            if (lineIndex < sizeof(lineBuffer) - 1) {
                lineBuffer[lineIndex++] = (char)b;
            }
        }
        delay(0); // Erlaubt dem FreeRTOS-Scheduler kurze Pausen
    }

    // --- PERIODISCHE NEOPIXEL AKTUALISIERUNG ALLE 2 SEKUNDEN (NON-BLOCKING) ---
    static unsigned long lastPixelUpdate = 0;
    if (millis() - lastPixelUpdate > 2000) {
        lastPixelUpdate = millis();
        updateBargraph(currentIbat);
    }

    // Periodisches Polling-Signal senden, um den Datenstrom aktiv zu halten
    static unsigned long lastPing = 0;
    if (connected && (millis() - lastPing > 2000)) {
        lastPing = millis();
        if (pRxChar) {
            uint8_t pollCmd[] = { 0x0E, 0x0D, 0x0A };
            pRxChar->writeValue(pollCmd, sizeof(pollCmd), false);
        }
    }

    if (!connected && !doConnect && !NimBLEDevice::getScan()->isScanning()) {
        NimBLEDevice::getScan()->start(5, false);
    }

    delay(10);
}
