#include <Arduino.h>
#include <WiFi.h> //libreria wifi
#include <PubSubClient.h> // libreria que permite comunicarse con un servidor MQTT
//#include <AdafruitIO.h>
//#include <AdafruitIO_WiFi.h>

// Credenciales Red WiFi 
//CAMBIAR RESPECTO DONDE ESTE LA RED 
#define WIFI_SSID "iPhone_DR" //Nombre de la red WiFi
#define WIFI_PASSWORD "DanielRH6"// Contraseña de la red WiFi

// Credenciales Adafruit IO
#define ADAFRUIT_USER "Daniel_Rojas" // Usuario de Adafruit IO
#define ADAFRUIT_KEY "aio_xxsd4504oKTFu320Rms58hdUZ5QL" // Clave de API de Adafruit IO
#define ADAFRUIT_SERVER "io.adafruit.com" // Servidor de Adafruit IO
#define ADAFRUIT_PORT 1883 // Puerto del servidor de Adafruit IO para la conexión MQTT
char ADAFRUIT_ID[30]; // Variable para almacenar el ID de Adafruit IO
const int pinSalida = 15; // Número del pin de salida (nuevo GPIO15)
int valv = 0; // Variable para almacenar el estado de la válvula
// Publicar
#define ADAFRUIT_FEED_HUM  ADAFRUIT_USER "/feeds/humedad" // Nombre del feed para la humedad
#define ADAFRUIT_DATA_IN   ADAFRUIT_USER "/feeds/data_in" // Nombre del feed para los datos de entrada
#define ADAFRUIT_FEED_TIM_R_HUM  ADAFRUIT_USER "/feeds/tiempo_r_h"

const int humsuelo = 8;    // Número del pin del sensor de humedad del suelo (nuevo GPIO8)
float ValHum = 0; // Variable para almacenar el valor de la humedad del suelo
float TIM_R_Hum = 0; // Variable para medir el valor en tiempo real del sensor

const int Sensor_h = 9; // Pin del sensor de humedad (nuevo GPIO9)

WiFiClient espClient; // Parámetros para Cliente WiFi
PubSubClient client(espClient); // Cliente MQTT

long lastMsg = 0;

void setup() {
    Serial.begin(115200);
    // Declaración de las entradas y salidas
    pinMode(Sensor_h, INPUT); 
    pinMode(humsuelo, INPUT);
    pinMode(pinSalida, OUTPUT);
    get_MQTT_ID();
    setup_wifi();
    client.setServer(ADAFRUIT_SERVER, ADAFRUIT_PORT);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
        reconnect();  // Reconectar si se pierde la conexión MQTT
    }

    client.loop();

    long now = millis();
    if (now - lastMsg > 120000) {
        lastMsg = now;
        TIM_R_Hum = map(analogRead(Sensor_h), 4095, 0, 0, 100);
        ValHum = map(analogRead(humsuelo), 4095, 0, 0, 100); 
        // Imprimir valor
        Serial.print("Humedad del suelo: ");
        Serial.print(ValHum);
        Serial.println(" %");
        if (ValHum > 40) {
            digitalWrite(pinSalida, LOW);  // Si ValHum es mayor que 40, apagar la salida
            Serial.println("valvula cerrada");
            valv = 0;
        } else {
            digitalWrite(pinSalida, HIGH);  // Si ValHum es menor o igual a 40, encender la salida
            Serial.println("valvula abierta");
            valv = 1;
        }
        mqtt_publish(ADAFRUIT_FEED_TIM_R_HUM, TIM_R_Hum);
        mqtt_publish(ADAFRUIT_FEED_HUM, ValHum);
    }
}

void mqtt_publish(String feed, int val) {
    String value = String(val);
    if (client.connected()) {
        client.publish(feed.c_str(), value.c_str());
        Serial.println("Publicando al tópico: " + String(feed) + " | mensaje: " + value);        
    }
}

//*****************************
//***    CONEXION WIFI      ***
//*****************************
void setup_wifi() {
    delay(10);
    
    // Nos conectamos a nuestra red Wifi
    Serial.println();
    Serial.print("Conectando a ");
    Serial.println(String(WIFI_SSID));

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("Conectado a red WiFi!");
    Serial.println("Dirección IP: ");
    Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length) {
    String mensaje  = "";
    String str_topic(topic); 

    for (uint16_t i = 0; i < length; i++) {
        mensaje += (char)payload[i];
    }

    mensaje.trim();

    Serial.println("Tópico: " + str_topic);
    Serial.println("Mensaje: " + mensaje); 
}

void get_MQTT_ID() {
    uint64_t chipid = ESP.getEfuseMac();    
    snprintf(ADAFRUIT_ID, sizeof(ADAFRUIT_ID), "%llu", chipid);
}

void reconnect() {
    while (!client.connected()) {   
        if (client.connect(ADAFRUIT_ID, ADAFRUIT_USER, ADAFRUIT_KEY)) {
            Serial.println("MQTT conectado!");
            client.subscribe(ADAFRUIT_DATA_IN);
            Serial.println("Suscrito al tópico: " + String(ADAFRUIT_DATA_IN));
        } else {
            Serial.print("falló :( con error -> ");
            Serial.print(client.state());
            Serial.println(" Intentamos de nuevo en 5 segundos");
            delay(5000);
        }
    }
}

//MODELO PARA PODER DORMIR LA ESP32

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR int bootCount = 0;

void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
        default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
    }
}

void dormir() {
    ++bootCount;
    Serial.println("Boot number: " + String(bootCount));
    client.loop();
    print_wakeup_reason();

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

    Serial.println("Going to sleep now");
    Serial.flush(); 
    esp_deep_sleep_start();
}
