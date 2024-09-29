#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "WiFi.h"
#include <Wiegand.h>
#include <Servo.h>

// Wiegand RFID pin definitions
#define D0_PIN 14  // Wiegand D0 connected to GPIO 14
#define D1_PIN 13  // Wiegand D1 connected to GPIO 13

// Servo pin definition
#define SERVO_PIN 2  // Servo connected to GPIO 2

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "myesplock"
#define AWS_IOT_SUBSCRIBE_TOPIC "myesplock"

// Set up WiFiClientSecure and PubSubClient
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Wiegand instance
WIEGAND wg;

// Servo motor instance
Servo doorLockServo;

// Array of authorized card IDs and corresponding owner names
struct CardOwner {
  String cardID;
  String ownerName;
};

// Initialize the 5 cards with corresponding owner names
CardOwner authorizedCards[] = {
  {"123456", "Khair"},
  {"654321", "Afandi"},
  {"112233", "Nabil"},
  {"445566", "Kamalul"},
  {"778899", "Fahmi"}
};

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

void publishToAWS(String cardID, String status, String ownerName = "") {
  // If the card is unauthorized, send a different message
  String message = "";
  if (status == "authorized") {
    message = "{\"cardID\":\"" + cardID + "\",\"status\":\"" + status + "\",\"owner\":\"" + ownerName + "\"}";
  } else {
    message = "{\"cardID\":\"" + cardID + "\",\"status\":\"" + status + "\",\"message\":\"Unauthorized Access\"}";
  }

  // Publish the message to AWS IoT
  client.publish(AWS_IOT_PUBLISH_TOPIC, message.c_str());
}

void setup() {
  Serial.begin(9600);

  // Initialize WiFi and AWS IoT connection
  connectWiFi();
  connectAWS();

  // Initialize Wiegand
  wg.begin(D0_PIN, D1_PIN);

  // Initialize servo motor
  doorLockServo.attach(SERVO_PIN);
  doorLockServo.write(0);  // Lock initially
}

void loop() {
  if (wg.available()) {
    // RFID card detected
    String cardID = String(wg.getCode());
    Serial.println("Card detected: " + cardID);

    // Check if card is authorized and retrieve owner name
    bool isAuthorized = false;
    String ownerName = "";
    for (CardOwner card : authorizedCards) {
      if (card.cardID == cardID) {
        isAuthorized = true;
        ownerName = card.ownerName;
        break;
      }
    }

    if (isAuthorized) {
      Serial.println("Access granted to " + ownerName);
      doorLockServo.write(90);  // Unlock door
      publishToAWS(cardID, "authorized", ownerName);
    } else {
      Serial.println("Access denied");
      publishToAWS(cardID, "unauthorized");
    }

    delay(5000);  // Delay before locking the door again
    doorLockServo.write(0);  // Lock door
  }

  client.loop();  // Keep MQTT connection alive
}
