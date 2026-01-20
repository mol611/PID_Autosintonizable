#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>            // Librería para WiFi
#include <HTTPClient.h>      // Librería para enviar datos a la nube

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// --- CONFIGURACIÓN DE RED Y NUBE ---
const char* ssid = "red wifi";        // <---  RED AQUÍ
const char* password = "12345678"; // <--- CLAVE AQUÍ
String apiKey = "HSTFRFHMGLO14V9X";             // <--- PEGA TU API KEY DE THINGSPEAK AQUÍ
const char* server = "http://api.thingspeak.com/update";

// --- PANTALLA OLED SH1106 ---
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- VARIABLES GLOBALES ---
int displayBPM = 0;
int displayBattery = 0;
String displayStatus = "Iniciando...";
bool wifiConnected = false;

// Tiempos para control de envío
unsigned long lastCloudUpload = 0;
const long cloudInterval = 20000; // Subir datos cada 20 segundos (ThingSpeak requiere >15s)

// --- BLE VARIABLES ---
static const char* DEVICE_NAME = "H6M 08012";
static bool doConnect = false;
static bool connected = false;

static const char* HR_SERVICE_UUID = "0000180d-0000-1000-8000-00805f9b34fb";
static const char* HR_CHARACTERISTIC_UUID = "00002a37-0000-1000-8000-00805f9b34fb";
static const char* BATTERY_SERVICE_UUID = "0000180f-0000-1000-8000-00805f9b34fb";
static const char* BATTERY_CHARACTERISTIC_UUID = "00002a19-0000-1000-8000-00805f9b34fb";

static BLEClient* pClient = nullptr;
static BLEAdvertisedDevice* pAdvertisedDevice = nullptr;
static BLERemoteCharacteristic* pHRCharacteristic = nullptr;
static BLERemoteCharacteristic* pBatteryCharacteristic = nullptr;

// -------------------------------------
// FUNCIÓN PARA SUBIR A THINGSPEAK
// -------------------------------------
// -------------------------------------
// FUNCIÓN PARA SUBIR A THINGSPEAK (CORREGIDA PARA TUS CAMPOS)
// -------------------------------------
void subirDatosCloud() {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // CAMBIOS IMPORTANTES AQUÍ:
    // field1 = 1 (Enviamos un '1' fijo para indicar que el "Estado" es activo)
    // field2 = displayBattery (Tu batería)
    // field3 = displayBPM (Tu ritmo cardíaco, que antes iba al field1)
    
    String url = String(server) + "?api_key=" + apiKey + 
                 "&field1=1" + 
                 "&field2=" + String(displayBattery) + 
                 "&field3=" + String(displayBPM);
    
    http.begin(url);
    int httpCode = http.GET(); // Enviamos la petición
    
    if (httpCode > 0) {
      Serial.printf("[HTTP] Éxito (%d). Datos subidos.\n", httpCode);
    } else {
      Serial.printf("[HTTP] Falló la conexión: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Error: WiFi desconectado, no se puede subir datos.");
  }
}

// -------------------------------------
// ACTUALIZAR PANTALLA
// -------------------------------------
void actualizarPantalla() {
  u8g2.clearBuffer();
  
  // Icono WiFi (simple texto)
  u8g2.setFont(u8g2_font_u8glib_4_tf); 
  u8g2.setCursor(110, 8);
  if(WiFi.status() == WL_CONNECTED) u8g2.print("WIFI");
  else u8g2.print("NO W");

  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(0, 10);
  u8g2.print(displayStatus);

  if (connected) {
    u8g2.setFont(u8g2_font_fub30_tn);
    int xPos = (displayBPM > 99) ? 30 : 45; 
    u8g2.setCursor(xPos, 45); 
    u8g2.print(displayBPM);
    
    u8g2.setFont(u8g2_font_u8glib_4_tf); 
    u8g2.setCursor(xPos + 55, 45);
    u8g2.print("BPM");

    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(0, 60);
    u8g2.print("Bat: ");
    u8g2.print(displayBattery);
    u8g2.print("%");
  }
  u8g2.sendBuffer();
}

// -------------------------------------
// CALLBACKS BLE (Igual que antes)
// -------------------------------------
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        Serial.println("BLE Conectado");
        displayStatus = "BLE Conectado";
        connected = true;
    }
    void onDisconnect(BLEClient* pclient) {
        connected = false;
        Serial.println("BLE Desconectado");
        displayStatus = "Desconectado";
        displayBPM = 0;
        doConnect = true; 
    }
};
static MyClientCallback clientCallback;

void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length == 0) return;
    uint8_t flags = pData[0]; 
    int offset = 1;
    int hrValue = 0;
    if (flags & 0x01) {
        hrValue = pData[offset] | (pData[offset + 1] << 8);
        offset += 2;
    } else {
        hrValue = pData[offset];
        offset += 1;
    }
    displayBPM = hrValue; 
    // Serial.printf("BPM: %d\n", hrValue); // Comentado para limpiar el monitor serial
}

// -------------------------------------
// CONEXIÓN BLE Y SCAN (Resumido)
// -------------------------------------
bool connectToServer() {
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(&clientCallback);
    if (!pClient->connect(pAdvertisedDevice)) return false;

    BLERemoteService* pRemoteService = pClient->getService(HR_SERVICE_UUID);
    if (pRemoteService == nullptr) return false;
    
    pHRCharacteristic = pRemoteService->getCharacteristic(HR_CHARACTERISTIC_UUID);
    if (pHRCharacteristic == nullptr) return false;

    if (pHRCharacteristic->canNotify()) pHRCharacteristic->registerForNotify(notifyCallback);

    // Battery Service
    pRemoteService = pClient->getService(BATTERY_SERVICE_UUID);
    if (pRemoteService != nullptr) {
        pBatteryCharacteristic = pRemoteService->getCharacteristic(BATTERY_CHARACTERISTIC_UUID);
    }
    return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.getName() == DEVICE_NAME) {
            BLEDevice::getScan()->stop();
            pAdvertisedDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true; 
        }
    }
};
static MyAdvertisedDeviceCallbacks advertisedDeviceCallbacks;

// -------------------------------------
// SETUP
// -------------------------------------
void setup() {
    Serial.begin(115200);
    u8g2.begin();
    u8g2.enableUTF8Print();

    // 1. Conexión WiFi
    Serial.print("Conectando a WiFi");
    displayStatus = "Conectando WiFi";
    actualizarPantalla();
    
    WiFi.begin(ssid, password);
    int intent = 0;
    while (WiFi.status() != WL_CONNECTED && intent < 20) { // Intentar por 10 segs
        delay(500);
        Serial.print(".");
        intent++;
    }
    
    if(WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Conectado!");
        displayStatus = "WiFi OK. Scan BLE";
    } else {
        Serial.println("\nFallo WiFi. Iniciando solo BLE.");
        displayStatus = "Sin WiFi. Scan BLE";
    }
    actualizarPantalla();

    // 2. Inicio BLE
    BLEDevice::init("");
    BLEScan* pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&advertisedDeviceCallbacks);
    pScan->setActiveScan(true); 
    pScan->setInterval(100);
    pScan->setWindow(99); 
}

// -------------------------------------
// LOOP
// -------------------------------------
void loop() {
    // A. Lógica BLE
    if (doConnect) {
        if (connectToServer()) doConnect = false;
        else delay(5000);
    }
    
    // B. Leer batería BLE cada 5s
    if (connected) {
        static unsigned long lastBatteryRead = 0;
        if (pBatteryCharacteristic != nullptr && (millis() - lastBatteryRead > 5000)) {
            std::string value = pBatteryCharacteristic->readValue().c_str(); 
            if (value.length() > 0) displayBattery = (int)value[0];
            lastBatteryRead = millis();
        }
    } else if (!doConnect) {
        // Re-escanear si se desconectó
        BLEDevice::getScan()->start(5); 
    }

    // C. SUBIR DATOS A LA NUBE (Cada 20 segundos)
    if (connected && WiFi.status() == WL_CONNECTED) {
        if (millis() - lastCloudUpload > cloudInterval) {
            Serial.println("Subiendo datos a ThingSpeak...");
            subirDatosCloud();
            lastCloudUpload = millis();
        }
    }

    // D. Refrescar Pantalla (Suave)
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 200) {
        actualizarPantalla();
        lastDisplayUpdate = millis();
    }
}