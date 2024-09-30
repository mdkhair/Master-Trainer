#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "WiFi.h"
#include <Servo.h>

// Servo pin definition
#define SERVO_PIN 2  // Servo connected to GPIO 2
#define FAN_PIN 5    // Fan connected to GPIO 5

// MQ6 gas sensor pin definition
#define MQ6_PIN A0  // MQ6 connected to analog pin A0

// Threshold for gas detection (adjust based on your requirements)
#define GAS_THRESHOLD 300

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "mykitchen/gas"
#define AWS_IOT_SUBSCRIBE_TOPIC "mykitchen/gas"

// Set up WiFiClientSecure and PubSubClient
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Servo motor instance
Servo windowServo;

void connectWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Try to connect to Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // WiFi connected
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
  client.setCallback(messageHandler);

  Serial.print("Connecting to AWS IoT...");

  while (!client.connected()) {
    if (client.connect(THINGNAME)) {
      Serial.println("AWS IoT Connected!");
      client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);  // Subscribe to the topic
    } else {
      Serial.print("AWS IoT Connection failed, client state: ");
      Serial.print(client.state());
      delay(2000);  // Wait for 2 seconds before retrying
    }
  }
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void publishToAWS(String gasStatus, int gasLevel) {
  String message = "{\"status\":\"" + gasStatus + "\",\"gasLevel\":\"" + String(gasLevel) + "\"}";
  client.publish(AWS_IOT_PUBLISH_TOPIC, message.c_str());
}

void setup() {
  Serial.begin(9600);

  // Initialize WiFi and AWS IoT connection
  connectWiFi();
  connectAWS();

  // Initialize servo motor
  windowServo.attach(SERVO_PIN);
  windowServo.write(0);  // Initially keep the window closed

  // Initialize fan pin
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);  // Initially turn off the fan
}

void loop() {
  int gasValue = analogRead(MQ6_PIN);  // Read the value from the MQ6 gas sensor
  Serial.print("Gas value: ");
  Serial.println(gasValue);

  if (gasValue > GAS_THRESHOLD) {
    // Gas detected, activate safety mechanisms
    Serial.println("Gas leak detected! Opening window and turning on fan.");
    windowServo.write(90);  // Open the window
    digitalWrite(FAN_PIN, HIGH);  // Turn on the fan
    publishToAWS("Gas Detected", gasValue);
  } else {
    // No gas detected, deactivate safety mechanisms
    Serial.println("Gas levels are safe. Closing window and turning off fan.");
    windowServo.write(0);  // Close the window
    digitalWrite(FAN_PIN, LOW);  // Turn off the fan
    publishToAWS("Gas Cleared", gasValue);
  }

  client.loop();  // Keep MQTT connection alive
  delay(5000);  // Check every 5 seconds
}
