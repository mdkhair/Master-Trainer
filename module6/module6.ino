#include "secrets.h"         // Contains your Wi-Fi and AWS IoT credentials
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "WiFi.h"
#include "DHT.h"

// DHT11 sensor settings
#define DHTPIN 15          // GPIO pin connected to the DHT11 data pin
#define DHTTYPE DHT11      // DHT11 sensor type

// Soil moisture sensor pin
#define SOIL_MOISTURE_PIN 34  // Analog pin connected to the soil moisture sensor

// Water pump control pin (using a relay)
#define PUMP_PIN 5            // GPIO pin connected to the relay controlling the water pump

// Threshold for soil moisture (adjust based on your calibration)
#define MOISTURE_THRESHOLD 500

// AWS IoT topics
#define AWS_IOT_PUBLISH_TOPIC   "smartgarden/data"
#define AWS_IOT_SUBSCRIBE_TOPIC "smartgarden/commands"  // If needed for future use

// WiFi and MQTT clients
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// DHT sensor instance
DHT dht(DHTPIN, DHTTYPE);

// Variables to hold sensor data
float temperature = 0.0;
float humidity = 0.0;
int soilMoistureValue = 0;

void connectWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to Wi-Fi!");
}

void connectAWS() {
  // Ensure we're connected to Wi-Fi first
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  client.setServer(AWS_IOT_ENDPOINT, 8883);  // Set AWS IoT as the server
  // client.setCallback(messageHandler);     // Uncomment if you have a callback function

  Serial.print("Connecting to AWS IoT...");

  while (!client.connected()) {
    if (client.connect(THINGNAME)) {
      Serial.println("Connected to AWS IoT!");
      // client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);  // Subscribe to a topic if needed
    } else {
      Serial.print("AWS IoT connection failed, client state: ");
      Serial.println(client.state());
      delay(2000);  // Wait before retrying
    }
  }
}

void publishToAWS() {
  // Create JSON payload
  String payload = "{";
  payload += "\"temperature\":" + String(temperature, 2) + ",";
  payload += "\"humidity\":" + String(humidity, 2) + ",";
  payload += "\"soilMoisture\":" + String(soilMoistureValue);
  payload += "}";

  // Publish to AWS IoT
  client.publish(AWS_IOT_PUBLISH_TOPIC, payload.c_str());
  Serial.println("Data published to AWS IoT:");
  Serial.println(payload);
}

void setup() {
  Serial.begin(9600);

  // Initialize Wi-Fi and AWS IoT connection
  connectWiFi();
  connectAWS();

  // Initialize DHT sensor
  dht.begin();

  // Initialize soil moisture sensor pin
  pinMode(SOIL_MOISTURE_PIN, INPUT);

  // Initialize water pump control pin
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);  // Ensure the pump is off initially
}

void loop() {
  // Read temperature and humidity from DHT11
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // Check if any reads failed
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    humidity = 0.0;
    temperature = 0.0;
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("°C  Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
  }

  // Read soil moisture value
  soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  Serial.print("Soil Moisture Value: ");
  Serial.println(soilMoistureValue);

  // Publish data to AWS IoT
  publishToAWS();

  // Check soil moisture level and control the water pump
  if (soilMoistureValue < MOISTURE_THRESHOLD) {
    // Soil is dry, activate the water pump
    Serial.println("Soil moisture is low. Activating water pump.");
    digitalWrite(PUMP_PIN, HIGH);  // Turn on the pump
    delay(10000);                  // Run the pump for 10 seconds
    digitalWrite(PUMP_PIN, LOW);   // Turn off the pump
    Serial.println("Water pump deactivated.");

    // Send notification via AWS SNS
    sendSNSNotification();
  } else {
    Serial.println("Soil moisture is sufficient. Pump is off.");
  }

  client.loop();  // Maintain MQTT connection
  delay(5000);    // Wait for 5 seconds before next reading
}

void sendSNSNotification() {
  // Since the ESP32 cannot directly interact with AWS SNS, we'll use an AWS IoT Rule to trigger the SNS notification
  // The rule will be configured on AWS IoT Core to send a notification when soilMoisture < MOISTURE_THRESHOLD
  Serial.println("Notification sent via AWS SNS (configured on AWS side).");
}

