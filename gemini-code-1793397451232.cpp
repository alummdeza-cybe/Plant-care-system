#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>

// --- CONFIGURACIÓN RED Y GITHUB ---
const char* WIFI_SSID = "Plant care";
const char* WIFI_PASS = "Plant care";

const char* GIST_ID = "TU_GIST_ID_AQUI";
const char* GITHUB_TOKEN = "github_pat_TU_TOKEN_AQUI";

// --- PINES ---
#define DHTPIN 4
#define DHTTYPE DHT11
#define PIN_HIGROMETRO 34
#define PIN_NIVEL_AGUA 35
#define PIN_RELE 32
#define PIN_SERVO1 18
#define PIN_SERVO2 19

// --- INSTANCIAS ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
Servo servo1;
Servo servo2;

// Variables de temporización
unsigned long lastUpdateLCD = 0;
unsigned long lastUpdateGitHub = 0;

void setup() {
  Serial.begin(115200);

  // Pinout de Actuadores
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, HIGH); // Apagado por defecto (Lógica invertida)

  // Servos
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(PIN_SERVO1, 500, 2400);
  servo2.attach(PIN_SERVO2, 500, 2400);
  
  servo1.write(0); // Posición inicial
  servo2.write(0);

  // Inicialización I2C y LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Conectando WiFi");

  // Conexión WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  lcd.clear();
  lcd.print("WiFi Conectado!");
  delay(1500);
  
  dht.begin();
}

void loop() {
  // 1. Lectura de Sensores
  float temp = dht.readTemperature();
  float humAmb = dht.readHumidity();
  int humSueloRaw = analogRead(PIN_HIGROMETRO);
  int nivelAguaRaw = analogRead(PIN_NIVEL_AGUA);

  // Mapeo porcentual aproximado para sensores analógicos
  int humSueloPct = map(humSueloRaw, 4095, 1500, 0, 100);
  humSueloPct = constrain(humSueloPct, 0, 100);

  int nivelAguaPct = map(nivelAguaRaw, 0, 3000, 0, 100);
  nivelAguaPct = constrain(nivelAguaPct, 0, 100);

  // 2. Control Automatizado de Lógica
  // Control de Relé / Bomba según humedad de suelo
  if (humSueloPct < 30) {
    digitalWrite(PIN_RELE, LOW);  // Encender relé
  } else {
    digitalWrite(PIN_RELE, HIGH); // Apagar relé
  }

  // Control de Servos según temperatura
  if (temp > 28.0) {
    servo1.write(90); // Abrir ventana
    servo2.write(90);
  } else {
    servo1.write(0);  // Cerrar ventana
    servo2.write(0);
  }

  // 3. Actualización de Pantalla LCD (Cada 2 segundos)
  if (millis() - lastUpdateLCD >= 2000) {
    lastUpdateLCD = millis();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(isnan(temp) ? 0 : temp, 1);
    lcd.print("C  H:");
    lcd.print(isnan(humAmb) ? 0 : humAmb, 0);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("HS:");
    lcd.print(humSueloPct);
    lcd.print("%  AGUA:");
    lcd.print(nivelAguaPct);
    lcd.print("%");
  }

  // 4. Envío de Datos a GitHub Gist (Cada 15 segundos)
  if (millis() - lastUpdateGitHub >= 15000) {
    lastUpdateGitHub = millis();
    if (WiFi.status() == WL_CONNECTED) {
      enviarDatosAGitHub(temp, humAmb, humSueloPct, nivelAguaPct);
    }
  }
}

void enviarDatosAGitHub(float temp, float humAmb, int humSuelo, int nivelAgua) {
  HTTPClient http;
  String url = "https://api.github.com/gists/" + String(GIST_ID);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-Invernadero");
  http.addHeader("Authorization", "Bearer " + String(GITHUB_TOKEN));

  // Creación del payload JSON para la API de GitHub Gists
  String jsonPayload = "{";
  jsonPayload += "\"files\": {";
  jsonPayload += "\"data.json\": {";
  jsonPayload += "\"content\": \"{\\n";
  jsonPayload += "  \\\"temperatura\\\": " + String(temp, 1) + ",\\n";
  jsonPayload += "  \\\"humedad_ambiente\\\": " + String(humAmb, 1) + ",\\n";
  jsonPayload += "  \\\"humedad_suelo\\\": " + String(humSuelo) + ",\\n";
  jsonPayload += "  \\\"nivel_agua\\\": " + String(nivelAgua) + "\\n";
  jsonPayload += "}\"";
  jsonPayload += "}";
  jsonPayload += "}";
  jsonPayload += "}";

  int httpCode = http.PATCH(jsonPayload);

  if (httpCode > 0) {
    Serial.printf("Petición GitHub enviada, código: %d\n", httpCode);
  } else {
    Serial.printf("Error en HTTP: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}
