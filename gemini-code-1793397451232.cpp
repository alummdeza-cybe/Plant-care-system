
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>

// CONFIGURACIÓN DE RED Y GITHUB
// Coloca aquí tus credenciales NUEVAS y privadas.
const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASS = "TU_CONTRASENA";

const char* GIST_ID = "4ae4ddbdcce9d3aea056a706b1fd07eb";
const char* GITHUB_TOKEN = "TU_TOKEN_NUEVO";

// PINES
#define DHTPIN 4
#define DHTTYPE DHT11
#define PIN_HIGROMETRO 34
#define PIN_NIVEL_AGUA 35
#define PIN_RELE 32
#define PIN_SERVO1 18
#define PIN_SERVO2 19

// COMPONENTES
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
Servo servo1;
Servo servo2;

// TEMPORIZADORES
unsigned long lastUpdateLCD = 0;
unsigned long lastUpdateGitHub = 0;
unsigned long lastSimulation = 0;

// VALORES SIMULADOS
float temp = 23.0;
float humAmb = 60.0;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, HIGH);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);

  servo1.attach(PIN_SERVO1, 500, 2400);
  servo2.attach(PIN_SERVO2, 500, 2400);

  servo1.write(0);
  servo2.write(0);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.print("Plant Care");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  dht.begin();

  randomSeed(micros());

  // Conexión Wi-Fi sin bloquear la bomba
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long inicioWiFi = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - inicioWiFi < 5000) {
    delay(500);
  }

  lcd.clear();

  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("WiFi conectado");
  } else {
    lcd.print("Modo sin WiFi");
  }

  delay(1000);
}

void loop() {

  // 1. TEMPERATURA Y HUMEDAD SIMULADAS
  // Cambian cada 5 segundos.
  if (millis() - lastSimulation >= 5000) {
    lastSimulation = millis();

    temp = random(220, 251) / 10.0;
    humAmb = random(55, 71);
  }

  // 2. SENSORES REALES
  int humSueloRaw = analogRead(PIN_HIGROMETRO);
  int nivelAguaRaw = analogRead(PIN_NIVEL_AGUA);

  // Humedad del suelo invertida
  int humSueloPct = map(
    humSueloRaw,
    4095, 1500,
    100, 0
  );

  humSueloPct = constrain(humSueloPct, 0, 100);

  // Nivel de agua
  int nivelAguaPct = map(
    nivelAguaRaw,
    0, 3000,
    0, 100
  );

  nivelAguaPct = constrain(nivelAguaPct, 0, 100);

  // 3. CONTROL DE BOMBA
  if (humSueloPct < 30) {
    digitalWrite(PIN_RELE, LOW);
  } else {
    digitalWrite(PIN_RELE, HIGH);
  }

  // 4. CONTROL DE VENTANAS
  if (temp > 28.0) {
    servo1.write(90);
    servo2.write(90);
  } else {
    servo1.write(0);
    servo2.write(0);
  }

  // 5. PANTALLA LCD CADA 2 SEGUNDOS
  if (millis() - lastUpdateLCD >= 2000) {
    lastUpdateLCD = millis();

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("C H:");
    lcd.print(humAmb, 0);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("HS:");
    lcd.print(humSueloPct);
    lcd.print("% A:");
    lcd.print(nivelAguaPct);
    lcd.print("%");
  }

  // 6. ENVIAR DATOS A GITHUB
  if (millis() - lastUpdateGitHub >= 15000) {
    lastUpdateGitHub = millis();

    if (WiFi.status() == WL_CONNECTED) {
      enviarDatosAGitHub(
        temp,
        humAmb,
        humSueloPct,
        nivelAguaPct
      );
    }
  }
}

void enviarDatosAGitHub(
  float temp,
  float humAmb,
  int humSuelo,
  int nivelAgua
) {

  if (String(GITHUB_TOKEN) == "TU_TOKEN_NUEVO") {
    Serial.println("Configura un token privado");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Solo para prueba; validar certificado en producción.

  HTTPClient http;

  String url = "https://api.github.com/gists/";
  url += GIST_ID;

  if (!http.begin(client, url)) {
    Serial.println("Error al iniciar HTTP");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-PlantCare");
  http.addHeader(
    "Authorization",
    "Bearer " + String(GITHUB_TOKEN)
  );

  String contenido = "{";
  contenido += "\"temperatura\":" + String(temp, 1) + ",";
  contenido += "\"humedad_ambiente\":" + String(humAmb, 1) + ",";
  contenido += "\"humedad_suelo\":" + String(humSuelo) + ",";
  contenido += "\"nivel_agua\":" + String(nivelAgua);
  contenido += "}";

  // Escapar contenido para JSON
  String escapado = "";

  for (unsigned int i = 0; i < contenido.length(); i++) {
    char c = contenido[i];

    if (c == '"' || c == '\\') {
      escapado += '\\';
    }

    escapado += c;
  }

  String jsonPayload =
    "{\"files\":{\"data.json\":{\"content\":\"" +
    escapado +
    "\"}}}";

  int httpCode = http.PATCH(jsonPayload);

  Serial.print("Respuesta GitHub: ");
  Serial.println(httpCode);

  http.end();
}
