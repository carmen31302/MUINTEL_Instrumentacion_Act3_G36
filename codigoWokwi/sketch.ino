// SISTEMA PARA EL CONTROL DE UN ASCENSOR DE 5 PLANTAS en ACME S.A.

// Librerías necesarias
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <IRremote.h>
#include <EEPROM.h>

// Definición de los pines de conexiones del Hardware
#define PIN_DHT 2                     // Pin para el sensor DHT22
#define PIN_IR 3                      // Pin para el receptor de infrarrojos
#define PIN_TRIG 7                    // Pines para el sensor HC-SR04
#define PIN_ECHO 8
#define PIN_SERVO 9                   // Pin para el servomotor
#define PIN_LDR A0                    // Pin para el sensor LDR
#define LATCH 10                      // Pines para los 74HC595
#define DATA 11
#define CLOCK 12

// Definición de sensores y actuadores
DHT dht(PIN_DHT, DHT22);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo ascensor;

// Variables globales
int plantaActual = 1;                 // Plantas
float tempDeseada = 25.0;             // Temperatura
float zonaMuertaTemp = 2.0;
float humDeseada = 80.0;              // Humedad
float zonaMuertaHum = 5.0;
float tempAnt = 25;                   // Variables de estado anterior para el LCD
float humAnt = 80;
int luzAnt = -1;
int plantaAnt = -1;

bool actualizarPantalla = true;

bool falloDHT = false;
bool falloLDR = false;

// Inicialización
void setup() {
  Serial.begin(9600);                 // Puerto serie

  dht.begin();                        // Iniciar sensores
  IrReceiver.begin(PIN_IR, ENABLE_LED_FEEDBACK);
  pinMode(PIN_TRIG, OUTPUT);          // Disparo como entrada
  pinMode(PIN_ECHO, INPUT);           // Salida del HC-SR04
  pinMode(DATA, OUTPUT);              // Pines del 74HC595 como salidas
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);

  actualizarLEDs(0, 0);               // Leds apagados al empezar
  ascensor.attach(PIN_SERVO);         // Iniciar servo como actuador

  lcd.init();                         // Iniciar LCD
  lcd.backlight();
  lcd.setCursor(0,0);                 // Pantalla de inicio para el LCD
  lcd.print("ASCENSOR ACME S.A. ");
  lcd.setCursor(0,1);
  lcd.print("Inicializando...");
  
  delay(2000);

  plantaActual = EEPROM.read(0);              // Planta guardada
  if(plantaActual < 1 || plantaActual > 5){
    plantaActual = 1;
  }
  moverAscensor(plantaActual, map(plantaActual,1,5,0,180));

  lcd.clear();
}

// Programa principal
void loop() {
  // Lectura de sensores de temperatura y humedad y de iluminación
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int luz = analogRead(PIN_LDR);

  falloDHT = false;
  falloLDR = false;
  if (isnan(t) || isnan(h)) {
    falloDHT = true;
    t = tempAnt;
    h = humAnt;
    Serial.println("ERROR DHT22");
  }

  if (luz < 0 || luz > 1023) {
    falloLDR = true;
    luz = luzAnt;
    Serial.println("ERROR LDR");
  }

  // Lógica tras la lectura
  byte salidaHT = calcularHT(t, h);
  byte salidaLuz = calcularLuz(luz);
  actualizarLEDs(salidaHT, salidaLuz);

  // Almacenar el estado para sacar por el puerto serie
  int nivelLuz = __builtin_popcount(salidaLuz);
  String estadoLuz = "";
  if (nivelLuz >= 6) {
    estadoLuz = "OSCURA";
  } else if (nivelLuz >= 3) {
    estadoLuz = "MEDIA";
  } else {
    estadoLuz = "ALTA";
  }

  // Detección de presencia con  el ultrasonidos si la distancia está entre 0 y 100
  float distancia = ultrasonico();
  bool usuarioDetectado = (distancia > 0 && distancia < 100);

  // Funcionamiento de los infrarrojos para indicar la planta
  if (IrReceiver.decode()) {
    int plantaMarcada = IrReceiver.decodedIRData.command;
    switch(plantaMarcada) {
      case 48: moverAscensor(1, 0); break;    // Planta 1
      case 24: moverAscensor(2, 45); break;   // Planta 2
      case 122: moverAscensor(3, 90); break;  // Planta 3
      case 16: moverAscensor(4, 135); break;  // Planta 4
      case 56: moverAscensor(5, 180); break;  // Planta 5
      case 162:                               // Subir temperatura (+)
        tempDeseada++;
        Serial.print("Temp SP: ");
        Serial.println(tempDeseada);
        actualizarPantalla = true;
        break;
      case 226:                                // Bajar temperatura (-)
        tempDeseada--;
        Serial.print("Temp SP: ");
        Serial.println(tempDeseada);
        actualizarPantalla = true;
        break;
      case 194:                                // Subir humedad (>>)
        humDeseada += 5;
        Serial.print("Hum SP: ");
        Serial.println(humDeseada);
        actualizarPantalla = true;
        break;
      case 2:                                 // Bajar humedad (<<)
        humDeseada -= 5;
        Serial.print("Hum SP: ");
        Serial.println(humDeseada);
        actualizarPantalla = true;
        break;
    }
    IrReceiver.resume();
  }

  // Actualizar el display LCD (evita parpadeos al sobreescribir)
  if ( abs(t - tempAnt) > 0.3 || abs(h- humAnt) > 1 || abs(luz - luzAnt) > 10 || plantaActual != plantaAnt || actualizarPantalla) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("P: "); lcd.print(plantaActual);
    lcd.print(" T:"); lcd.print(t,1); lcd.print((char)223); lcd.print("C ");
    lcd.setCursor(0,1);
    lcd.print("H:"); lcd.print(h,0); lcd.print("%");
    lcd.print(" L:"); lcd.print(estadoLuz);

    if(falloDHT){
      lcd.print(" DHT!");
    }
    else if(falloLDR){
      lcd.print(" LDR!");
    }

    Serial.print("Planta: "); Serial.println(plantaActual);
    Serial.print("Iluminacion: "); Serial.println(estadoLuz);
    Serial.print("Temperatura: "); Serial.println(t);
    Serial.print("Humedad: "); Serial.println(h);
    Serial.println("Presencia: "); Serial.println(usuarioDetectado ? "SI" : "NO");
    Serial.print("SP Temp: "); Serial.println(tempDeseada);
    Serial.print("SP Hum: "); Serial.println(humDeseada);
    Serial.println("--------------------");

    // Guardar valores anteriores
    tempAnt = t;
    humAnt = h;
    luzAnt = luz;
    plantaAnt = plantaActual;

    actualizarPantalla =  false;
  }
  delay(200);
}

// Calcular los bytes de salida para los registros de movimiento de los LEDs
byte calcularHT(float t, float h) {
  byte estadoHT = 0;

  if (t > tempDeseada + zonaMuertaTemp) estadoHT |= B00001000;    // Refrigeración
  if (t < tempDeseada - zonaMuertaTemp) estadoHT |= B00000100;    // Calefacción
  if (h > humDeseada + zonaMuertaHum) estadoHT |= B00000010;      // Deshumidificar
  if (h < humDeseada - zonaMuertaHum) estadoHT |= B00000001;      // Humidificar

  return estadoHT;
}
byte calcularLuz(int luz) {
  // Iluminación artificial: a mayor luz ambiente, menos LEDs encendidos. 8 niveles
  byte estadoL = 0;   
                           
  int nivel = map(luz, 0, 1023, 0, 8);                // Convertir luz a número de LEDs
  nivel = constrain(nivel, 0, 8);                     // Limitar el valor de núm. de LEDs
  for (int i = 0; i < nivel; i++) {
    estadoL |= (1 << i);                              // Calcular LEDs encendidos necesarios
  }

  return estadoL;
}

// Función para actualizar el estado de los LEDs
void actualizarLEDs(byte salidaHT, byte salidaLuz) {
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLOCK, MSBFIRST, salidaHT);          // Primero el que no se conecta directamente al Arduino
  shiftOut(DATA, CLOCK, MSBFIRST, salidaLuz);
  digitalWrite(LATCH, HIGH);
}

// Función movimiento ascensor
void moverAscensor(int planta, int angulo) {
  ascensor.write(angulo);                         // Simular el movimiento con el ángulo del servo 
  plantaActual = planta;                          // Ya ha llegado a la planta
  EEPROM.update(0, plantaActual);                 // Almacenar la planta
  actualizarPantalla = true;                      // Actualizar el display
  Serial.print("Ascensor en planta ");
  Serial.println(plantaActual);
}

// Función para el sensor de ultrasonidos
float ultrasonico() {
  digitalWrite(PIN_TRIG, LOW);                    // Sensor primero en reposo 
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);                   // Enviar onda de ultrasonidos
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long tiempo = pulseIn(PIN_ECHO, HIGH, 30000);    // Tiempo en alto de ECHO incluyendo timeout
  float distancia = tiempo * 0.034 / 2;           // Calcular: V_sonido = D / T

  return distancia;
}
