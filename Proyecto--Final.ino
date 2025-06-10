/**
 * @file sistema_confort.ino
 * @brief Sistema de monitoreo y control de confort térmico en un salón.
 * 
 * Este sistema mide la temperatura, humedad y nivel de luz de un entorno
 * (por ejemplo, un salón de clases) y responde con acciones como activar
 * un ventilador o emitir alarmas. El acceso está protegido por contraseña
 * y se puede modificar el estado térmico percibido mediante tarjetas RFID.
 * 
 * @authors 
 * - Juliana Andrea Urbano Palomino
 * - Maria Alejandra Solarte
 * - Ronal Santiago Valdez
 * - Juan Esteban Mera Mera
 * 
 * @date 2025-06-10
 */

#include <LiquidCrystal.h>
#include <Keypad.h>
#include "DHT.h"
#include "AsyncTaskLib.h"
#include <SPI.h>
#include <MFRC522.h>

/** @name Configuración del LCD */
///@{
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
///@}

/** @name Pines de salida y dispositivos */
///@{
#define LED_ROJO 24
#define LED_VERDE 48
#define LED_AZUL  46
#define VENTILADOR A5
#define BUZZER  8
///@}

/** @name Sensor DHT */
///@{
#define DHTPIN 22
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
///@}

/** @name Sensor de luz */
///@{
#define PIN_LUZ A0
int luzValue = 0;
///@}

/** @name Módulo RFID */
///@{
#define RST_PIN 32
#define SS_PIN  53
MFRC522 mfrc522(SS_PIN, RST_PIN);
///@}

/** @enum Estado
 *  @brief Estados posibles del sistema
 */
enum Estado {
  INIT,       ///< Solicita clave al usuario
  MONITOREO,  ///< Monitoreo ambiental activo
  ALARMA,     ///< Condición anómala detectada
  BLOQUEADO   ///< Sistema bloqueado por fallos
};
Estado estadoActual = INIT;

/** @name Gestión de clave */
///@{
char contrasenia[9] = "1234DCBA"; ///< Clave predefinida
char ingreso[9]; ///< Clave ingresada por el usuario
int i = 0; ///< Índice para ingreso de clave
int intentosClave = 0; ///< Contador de intentos
///@}

/** @name Teclado matricial */
///@{
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {26, 28, 30, 34};
byte colPins[COLS] = {36, 38, 40, 42};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
///@}

/** @name Tareas asincrónicas */
///@{
void readLuz(void);
void readTemperatura(void);
void readHumedad(void);
void mostrarInfo(void);
AsyncTask TaskLuz(1000, true, readLuz);
AsyncTask TaskTemp(1500, true, readTemperatura);
AsyncTask TaskHum(2000, true, readHumedad);
AsyncTask TaskDisplay(3000, true, mostrarInfo);
///@}

/** @name Variables globales */
///@{
float temperatura = 0; ///< Temperatura en grados Celsius
float humedad = 0; ///< Humedad relativa
float PMV = 0; ///< Índice PMV (Predicted Mean Vote)
int contadorAlarmas = 0; ///< Conteo de activaciones de alarma
///@}

/**
 * @brief Apaga todos los LEDs.
 */
void resetLEDs() {
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);
  digitalWrite(LED_ROJO, LOW);
}

/**
 * @brief Configuración inicial del sistema.
 */
void setup() {
  lcd.begin(16, 2);
  Serial.begin(9600);
  dht.begin();
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(VENTILADOR, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  resetLEDs();
  digitalWrite(LED_ROJO, HIGH);
  digitalWrite(VENTILADOR, LOW);
  digitalWrite(BUZZER, LOW);

  lcd.print("Digite su clave:");
}

/**
 * @brief Bucle principal del sistema.
 */
void loop() {
  switch (estadoActual) {
    case INIT:
      verificarClave();
      break;
    case MONITOREO:
      TaskLuz.Update();
      TaskTemp.Update();
      TaskHum.Update();
      TaskDisplay.Update();
      monitorearConfort();
      break;
    case ALARMA:
      activarAlarma();
      break;
    case BLOQUEADO:
      bloquearSistema();
      break;
  }
}

/**
 * @brief Verifica la clave ingresada por el usuario.
 */
void verificarClave() {
  char key = keypad.getKey();

  if (key && key != '*') {
    if (i < 8) {
      ingreso[i] = key;
      lcd.setCursor(i, 1);
      lcd.print('*');
      i++;
    }
  } else if (key == '*') {
    ingreso[i] = '\0';
    if (strcmp(contrasenia, ingreso) == 0) {
      lcd.clear();
      lcd.print("Acceso correcto");

      resetLEDs();
      digitalWrite(LED_VERDE, HIGH);

      estadoActual = MONITOREO;

      TaskLuz.Start();
      TaskTemp.Start();
      TaskHum.Start();
      TaskDisplay.Start();

      intentosClave = 0;
      contadorAlarmas = 0;
    } else {
      lcd.clear();
      lcd.print("Clave incorrecta");

      resetLEDs();
      digitalWrite(LED_AZUL, HIGH);

      delay(2000);
      resetLEDs();
      lcd.clear();
      lcd.print("Digite su clave:");

      intentosClave++;
      if (intentosClave >= 3) {
        estadoActual = BLOQUEADO;
      }
    }
    i = 0;
  }
}

/**
 * @brief Lee el valor del sensor de luz.
 */
void readLuz() {
  luzValue = analogRead(PIN_LUZ);
  Serial.print("Luz: ");
  Serial.println(luzValue);
}

/**
 * @brief Lee la temperatura desde el sensor DHT11.
 */
void readTemperatura() {
  temperatura = dht.readTemperature();
  Serial.print("Temp: ");
  Serial.println(temperatura);
}

/**
 * @brief Lee la humedad desde el sensor DHT11.
 */
void readHumedad() {
  humedad = dht.readHumidity();
  Serial.print("Hum: ");
  Serial.println(humedad);
}

/**
 * @brief Muestra temperatura, humedad y luz en el LCD.
 */
void mostrarInfo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperatura, 1);
  lcd.print(" H:");
  lcd.print(humedad, 0);
  lcd.setCursor(0, 1);
  lcd.print("Luz:");
  lcd.print(luzValue);
}

/**
 * @brief Evalúa el confort térmico y actúa en consecuencia.
 */
void monitorearConfort() {
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    byte tarjetaUID[] = {0xE6, 0xC5, 0xD4, 0x38};
    byte gotaUID[]    = {0xE1, 0xB8, 0xAA, 0x00};
    bool esTarjeta = true;
    bool esGota = true;

    for (byte j = 0; j < 4; j++) {
      if (mfrc522.uid.uidByte[j] != tarjetaUID[j]) {
        esTarjeta = false;
      }
      if (mfrc522.uid.uidByte[j] != gotaUID[j]) {
        esGota = false;
      }
    }

    Serial.print("Card UID: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    if (esTarjeta) {
      PMV = 2.0;
    } else if (esGota) {
      PMV = -2.0;
    } else {
      PMV = 0;
    }

    Serial.print("PMV: ");
    Serial.println(PMV);

    if (PMV > 1) {
      lcd.clear();
      lcd.print("Confort Alto");
      resetLEDs();
      digitalWrite(VENTILADOR, HIGH);
      delay(5000);
      digitalWrite(VENTILADOR, LOW);
    } else if (PMV < -1) {
      lcd.clear();
      lcd.print("Confort Bajo");
      resetLEDs();
      digitalWrite(LED_AZUL, HIGH);
      digitalWrite(LED_ROJO, HIGH);
      delay(4000);
      resetLEDs();
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  if (temperatura > 26 && luzValue < 100) {
    estadoActual = ALARMA;
  }
}

/**
 * @brief Activa la alarma por condiciones fuera del confort.
 */
void activarAlarma() {
  lcd.clear();
  lcd.print("** ALARMA **");
  resetLEDs();

  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED_ROJO, HIGH);
    delay(800);
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED_ROJO, LOW);
    delay(200);
  }

  contadorAlarmas++;
  Serial.print("Intentos de alarma: ");
  Serial.println(contadorAlarmas);

  if (contadorAlarmas >= 3) {
    estadoActual = BLOQUEADO;
  } else {
    estadoActual = MONITOREO;
  }
}

/**
 * @brief Bloquea el sistema tras múltiples fallos de clave o alarmas.
 */
void bloquearSistema() {
  lcd.clear();
  lcd.print("Sistema BLOQUEADO");
  resetLEDs();

  unsigned long startTime = millis();
  while (millis() - startTime < 7000) {
    digitalWrite(LED_AZUL, HIGH);
    digitalWrite(LED_VERDE, HIGH);
    delay(200);
    digitalWrite(LED_AZUL, LOW);
    digitalWrite(LED_VERDE, HIGH);
    delay(200);
  }

  resetLEDs();
  intentosClave = 0;
  contadorAlarmas = 0;
  lcd.clear();
  lcd.print("Digite su clave:");
  estadoActual = INIT;
}
