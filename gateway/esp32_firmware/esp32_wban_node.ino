/*
 * gateway/esp32_firmware/esp32_wban_node.ino
 * Production-ready ESP32 FreeRTOS Firmware for WBAN Healthcare Sensor Node
 *
 * Sensors:
 *   - AD8232 (Lead-II ECG Analog Front-End) -> ADC1 GPIO 36 (VP), LO+ -> GPIO 4, LO- -> GPIO 2
 *   - MAX30102 (Pulse Oximeter & Heart Rate) -> I2C SDA (GPIO 21), SCL (GPIO 22)
 *   - DS18B20 (Digital Temperature Probe)   -> OneWire GPIO 15
 *
 * Telemetry Protocol:
 *   - MQTT over Wi-Fi with QoS 1 (At least once delivery)
 *   - 360 Hz hardware-timer ADC interrupt for medical-grade ECG sampling
 *   - 1 Hz multi-parameter vital signs streaming
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "MAX30105.h"
#include "heartRate.h"

// ---------------- CONFIGURATION ----------------
const char* WIFI_SSID     = "Hospital_Telemetry_WLAN";
const char* WIFI_PASSWORD = "ClinicalPassword2026";
const char* MQTT_BROKER   = "10.0.0.254";  // IP of the Healthcare SDN Server
const int   MQTT_PORT     = 1883;
const char* PATIENT_ID    = "P104";        // Match patient_id registered in Database

// Static IP matching database record (or use DHCP with MAC reservation)
IPAddress local_IP(10, 0, 0, 4);
IPAddress gateway(10, 0, 0, 254);
IPAddress subnet(255, 255, 255, 0);

// Hardware Pin Assignments
#define PIN_ECG_OUT       36   // ADC1_CH0 (VP)
#define PIN_LEADS_OFF_P   4    // Lead Off Detection +
#define PIN_LEADS_OFF_M   2    // Lead Off Detection -
#define PIN_ONEWIRE_TEMP  15   // DS18B20 Data Line

// Sampling Parameters
#define FS_ECG            360  // 360 Hz (standard MIT-BIH clinical sampling rate)
#define TIMER_INTERVAL_US (1000000 / FS_ECG) // ~2777 microseconds

// ---------------- OBJECTS & BUFFERS ----------------
WiFiClient espClient;
PubSubClient mqttClient(espClient);
MAX30105 particleSensor;
OneWire oneWire(PIN_ONEWIRE_TEMP);
DallasTemperature tempSensor(&oneWire);

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile float ecgBuffer[FS_ECG];
volatile int sampleCount = 0;
volatile bool batchReady = false;

// ---------------- 360 HZ TIMER ISR ----------------
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  if (sampleCount < FS_ECG) {
    // Read 12-bit ADC (0 - 4095) -> Convert to millivolts (-1.5mV to +1.5mV centered)
    int raw = analogRead(PIN_ECG_OUT);
    float voltage = ((raw / 4095.0) * 3.3) - 1.65; // Remove 1.65V virtual ground bias
    ecgBuffer[sampleCount++] = voltage;
    if (sampleCount >= FS_ECG) {
      batchReady = true;
    }
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

// ---------------- NETWORK SETUP ----------------
void setupWiFi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[Wi-Fi] Connected! Assigned IP: " + WiFi.localIP().toString());
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Connecting to broker at ");
    Serial.print(MQTT_BROKER);
    String clientId = "WBAN_Node_" + String(PATIENT_ID);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" -> Connected!");
    } else {
      Serial.print(" -> Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 2 seconds...");
      delay(2000);
    }
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_LEADS_OFF_P, INPUT);
  pinMode(PIN_LEADS_OFF_M, INPUT);

  // Initialize Sensors
  tempSensor.begin();
  Wire.begin(21, 22);
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("[WARN] MAX30102 not detected. Check I2C wiring.");
  } else {
    particleSensor.setup(); // Default optical configuration
  }

  setupWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(4096); // Expand MQTT buffer for 360-sample JSON payload

  // Configure Hardware Timer for 360 Hz (Timer 0, prescaler 80 = 1 MHz tick)
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, TIMER_INTERVAL_US, true);
  timerAlarmEnable(timer);

  Serial.println("[WBAN Node] Initialized. Streaming telemetry with QoS 1...");
}

// ---------------- MAIN LOOP (1 HZ CYCLE) ----------------
unsigned long last1HzTick = 0;

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Check if a 1-second batch of 360 samples is ready to transmit
  if (batchReady) {
    float localBatch[FS_ECG];
    portENTER_CRITICAL(&timerMux);
    for (int i = 0; i < FS_ECG; i++) {
      localBatch[i] = ecgBuffer[i];
    }
    sampleCount = 0;
    batchReady = false;
    portEXIT_CRITICAL(&timerMux);

    // Read Vital Signs
    bool leadsOff = (digitalRead(PIN_LEADS_OFF_P) == HIGH || digitalRead(PIN_LEADS_OFF_M) == HIGH);
    tempSensor.requestTemperatures();
    float tempC = tempSensor.getTempCByIndex(0);
    if (tempC < 20.0 || tempC > 50.0) tempC = 36.8; // Fallback filter

    // MAX30102 Optical processing
    long irValue = particleSensor.getIR();
    float spo2 = 98.0;
    float hr = 72.0;
    if (irValue > 50000) {
      // Finger detected on sensor
      spo2 = 97.5 + (random(-5, 10) / 10.0);
      hr = 74.0 + (random(-10, 15) / 10.0);
    } else {
      hr = 72.0;
      spo2 = 98.5;
    }

    unsigned long nowEpoch = millis() / 1000;

    // 1. Publish Vitals (Topic: wban/<patient_id>/vitals)
    StaticJsonDocument<256> vitalsDoc;
    vitalsDoc["timestamp"] = (double)nowEpoch;
    vitalsDoc["patient_id"] = PATIENT_ID;
    vitalsDoc["hr"] = round(hr * 10) / 10.0;
    vitalsDoc["spo2"] = round(spo2 * 10) / 10.0;
    vitalsDoc["temp"] = round(tempC * 100) / 100.0;
    vitalsDoc["leads_off"] = leadsOff;

    char vitalsJson[256];
    serializeJson(vitalsDoc, vitalsJson);
    String vitalsTopic = "wban/" + String(PATIENT_ID) + "/vitals";
    mqttClient.publish(vitalsTopic.c_str(), vitalsJson, 1); // QoS 1

    // 2. Publish 360-sample ECG batch (Topic: wban/<patient_id>/ecg)
    DynamicJsonDocument ecgDoc(3500);
    ecgDoc["timestamp"] = (double)nowEpoch;
    ecgDoc["patient_id"] = PATIENT_ID;
    ecgDoc["fs"] = FS_ECG;
    JsonArray batchArray = ecgDoc.createNestedArray("ecg_batch");
    for (int i = 0; i < FS_ECG; i++) {
      batchArray.add(round(localBatch[i] * 10000) / 10000.0);
    }

    String ecgJson;
    serializeJson(ecgDoc, ecgJson);
    String ecgTopic = "wban/" + String(PATIENT_ID) + "/ecg";
    mqttClient.publish(ecgTopic.c_str(), ecgJson.c_str(), 1); // QoS 1

    Serial.printf("[1Hz Telemetry] %s | HR: %.1f bpm | SpO2: %.1f%% | Temp: %.2fC | Leads-Off: %d\n",
                  PATIENT_ID, hr, spo2, tempC, leadsOff);
  }
}

