
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ========== WIFI ==========
const char* WIFI_SSID = "Iphone";
const char* WIFI_PASS = "applepi";

// ========== PINES ==========
#define PIN_HIGROMETRO 34
#define PIN_NIVEL_AGUA 35
#define PIN_RELE 32
#define PIN_SERVO1 18
#define PIN_SERVO2 19

// ========== COMPONENTES ==========
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo1;
Servo servo2;
WebServer server(80);

// ========== VARIABLES ==========
bool modoManual = false;
bool ventanasAbiertas = false;

float temperatura = 25.0;
int humedadAmbiente = 75;
int humedadSuelo = 0;
int nivelAgua = 0;

unsigned long ultimoSensor = 0;
unsigned long ultimaPantalla = 0;
unsigned long ultimaSimulacion = 0;

// Valores provisionales de calibracion
const int ADC_SECO = 1500;
const int ADC_MOJADO = 4095;

// ========== MOVER VENTANAS ==========
void moverVentanas(bool abrir) {
  ventanasAbiertas = abrir;

  if (abrir) {
    servo1.write(90);
    servo2.write(90);
    Serial.println("VENTANAS ABIERTAS");
  } else {
    servo1.write(0);
    servo2.write(0);
    Serial.println("VENTANAS CERRADAS");
  }
}

// ========== PAGINA WEB ==========
String paginaWeb() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Plant Care</title>
<style>
body {
  background: #071b15;
  color: white;
  font-family: Arial, sans-serif;
  text-align: center;
  padding: 25px;
}
.panel {
  background: #12382b;
  padding: 25px;
  border-radius: 20px;
  max-width: 420px;
  margin: 30px auto;
}
button {
  border: none;
  border-radius: 12px;
  padding: 15px 20px;
  margin: 8px;
  font-size: 17px;
  color: white;
  cursor: pointer;
}
.abrir { background: #16844b; }
.cerrar { background: #b44736; }
.auto { background: #376d91; }
</style>
</head>
<body>

<h1>PLANT CARE</h1>
<p>Control de compuertas</p>

<div class="panel">
  <h2>Ventanas del invernadero</h2>

  <h2 id="estado">Consultando...</h2>
  <p id="modo"></p>

  <button class="abrir" onclick="orden('/abrir')">
    ABRIR
  </button>

  <button class="cerrar" onclick="orden('/cerrar')">
    CERRAR
  </button>

  <p>
    <button class="auto" onclick="orden('/automatico')">
      MODO AUTOMATICO
    </button>
  </p>

  <p id="mensaje"></p>
</div>

<script>
async function actualizar() {
  try {
    const r = await fetch('/estado');
    const d = await r.json();

    document.getElementById('estado').textContent =
      d.abiertas ? 'VENTANAS ABIERTAS' : 'VENTANAS CERRADAS';

    document.getElementById('modo').textContent =
      d.manual ? 'Modo manual' : 'Modo automatico';

  } catch (e) {
    document.getElementById('mensaje').textContent =
      'Sin conexion con ESP32';
  }
}

async function orden(ruta) {
  try {
    const r = await fetch(ruta, { method: 'POST' });

    if (!r.ok) throw new Error();

    document.getElementById('mensaje').textContent =
      'Orden enviada correctamente';

    actualizar();

  } catch (e) {
    document.getElementById('mensaje').textContent =
      'No se pudo enviar la orden';
  }
}

actualizar();
setInterval(actualizar, 2000);
</script>

</body>
</html>
)rawliteral";
}

// ========== SERVIDOR WEB ==========
void configurarServidor() {

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", paginaWeb());
  });

  server.on("/abrir", HTTP_POST, []() {
    modoManual = true;
    moverVentanas(true);
    server.send(200, "text/plain", "Abiertas");
  });

  server.on("/cerrar", HTTP_POST, []() {
    modoManual = true;
    moverVentanas(false);
    server.send(200, "text/plain", "Cerradas");
  });

  server.on("/automatico", HTTP_POST, []() {
    modoManual = false;
    moverVentanas(temperatura > 28.0);
    server.send(200, "text/plain", "Automatico");
  });

  server.on("/estado", HTTP_GET, []() {
    String json = "{";

    json += "\"manual\":";
    json += modoManual ? "true" : "false";

    json += ",\"abiertas\":";
    json += ventanasAbiertas ? "true" : "false";

    json += "}";

    server.send(200, "application/json", json);
  });

  server.begin();
}

// ========== SETUP ==========
void setup() {

  Serial.begin(115200);

  // Bomba apagada durante prueba de ventanas
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, HIGH);

  // Servomotores
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);

  servo1.attach(PIN_SERVO1, 500, 2400);
  servo2.attach(PIN_SERVO2, 500, 2400);

  moverVentanas(false);

  // Pantalla LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.print("Plant Care");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  randomSeed(micros());

  // Conexion WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long inicio = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - inicio < 10000
  ) {
    delay(300);
    Serial.print(".");
  }

  lcd.clear();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WIFI CONECTADO");
    Serial.print("ABRE ESTA DIRECCION: http://");
    Serial.println(WiFi.localIP());

    lcd.print("WiFi conectado");
    configurarServidor();

  } else {
    Serial.println("WiFi no conectado");
    lcd.print("Sin WiFi");
  }

  delay(1000);
}

// ========== LOOP ==========
void loop() {

  // Atender botones de la pagina web
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  // Temperatura y humedad ambiental simuladas
  if (millis() - ultimaSimulacion >= 5000) {
    ultimaSimulacion = millis();

    temperatura = random(240, 281) / 10.0;
    humedadAmbiente = random(65, 86);
  }

  // Leer sensores
  if (millis() - ultimoSensor >= 1000) {
    ultimoSensor = millis();

    long suma = 0;

    for (int i = 0; i < 10; i++) {
      suma += analogRead(PIN_HIGROMETRO);
      delay(3);
    }

    int lectura = suma / 10;

    humedadSuelo = map(
      lectura,
      ADC_SECO,
      ADC_MOJADO,
      0,
      100
    );

    humedadSuelo = constrain(humedadSuelo, 0, 100);

    int aguaRaw = analogRead(PIN_NIVEL_AGUA);

    nivelAgua = constrain(
      map(aguaRaw, 0, 3000, 0, 100),
      0,
      100
    );

    // Bomba apagada en esta prueba
    digitalWrite(PIN_RELE, HIGH);

    Serial.print("ADC suelo: ");
    Serial.println(lectura);
  }

  // Modo automatico
  if (!modoManual) {
    bool debeAbrir = temperatura > 28.0;

    if (debeAbrir != ventanasAbiertas) {
      moverVentanas(debeAbrir);
    }
  }

  // Pantalla LCD
  if (millis() - ultimaPantalla >= 2000) {
    ultimaPantalla = millis();

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperatura, 1);
    lcd.print(" H:");
    lcd.print(humedadAmbiente);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print(modoManual ? "MANUAL " : "AUTO ");
    lcd.print(ventanasAbiertas ? "ABIERTAS" : "CERRADAS");
  }
}
