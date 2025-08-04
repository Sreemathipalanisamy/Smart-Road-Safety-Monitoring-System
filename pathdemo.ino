#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// Wi-Fi credentials
const char* ssid = "1v";
const char* password = "jaiV1plus";

// Firebase Realtime Database details
const char* firebaseHost = "smart-road-safety-monitor-default-rtdb.firebaseio.com";
const char* firebaseAuth = "IPd0AuOHlu6b5Y4Qe6J3axe0AhP2npVpez1z5qkL";

// Ultrasonic Sensor Pins
#define TRIG_1 15  // D8
#define ECHO_1 14  // D5
#define TRIG_2 5   // D1
#define ECHO_2 12  // D6
#define TRIG_3 4   // D2
#define ECHO_3 13  // D7

// Vibration Sensor Pin
#define VIBRATION_PIN 16  // D0

// Sensor height from ground (cm)
#define SENSOR_1_HEIGHT 5
#define SENSOR_2_HEIGHT 10
#define SENSOR_3_HEIGHT 10

// GPS Configuration
#define GPS_RX 0  // D3
#define GPS_TX 2  // D4
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
TinyGPSPlus gps;

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600);

  // Wi-Fi connection
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");

  client.setInsecure(); // Disable SSL cert verification

  // Sensor pin setups
  pinMode(TRIG_1, OUTPUT);
  pinMode(ECHO_1, INPUT);
  pinMode(TRIG_2, OUTPUT);
  pinMode(ECHO_2, INPUT);
  pinMode(TRIG_3, OUTPUT);
  pinMode(ECHO_3, INPUT);

  pinMode(VIBRATION_PIN, INPUT); // Vibration sensor input
}

// Function to classify pothole severity
String getSeverity(int sensorHeight, int distance) {
  int depth = distance - sensorHeight;
  if (depth <= 0) {
    return "No pothole";
  } else if (depth < 25) {
    return "Less severe";
  } else if (depth < 50) {
    return "Moderately severe";
  } else {
    return "Highly severe";
  }
}

// Send data to Firebase
void sendDataToFirebase(String rawGps, int d1, int d2, int d3, String severity1, String severity2, String severity3, bool vibrationDetected) {
  Serial.println("Connecting to Firebase...");

  if (client.connected()) {
    client.stop();
  }

  if (!client.connect(firebaseHost, 443)) {
    Serial.println("Firebase connection failed!");
    return;
  }

  rawGps.replace("\n", "");
  rawGps.replace("\r", "");

  String url = "/sensor_data.json?auth=" + String(firebaseAuth);
  String payload = "{";
  payload += "\"raw_gps\": \"" + rawGps + "\", ";
  payload += "\"distance_1\": " + String(d1) + ", ";
  payload += "\"severity_1\": \"" + severity1 + "\", ";
  payload += "\"distance_2\": " + String(d2) + ", ";
  payload += "\"severity_2\": \"" + severity2 + "\", ";
  payload += "\"distance_3\": " + String(d3) + ", ";
  payload += "\"severity_3\": \"" + severity3 + "\", ";
  payload += "\"vibration_detected\": " + String(vibrationDetected ? "true" : "false");
  payload += "}";

  Serial.println("Sending data to Firebase...");
  Serial.println(payload);

  client.print("POST " + url + " HTTP/1.1\r\n");
  client.print("Host: " + String(firebaseHost) + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: " + String(payload.length()) + "\r\n");
  client.print("\r\n");
  client.print(payload);

  unsigned long timeout = millis();
  while (client.available() == 0 && millis() - timeout < 5000) {
    delay(10);
  }

  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }

  Serial.println("Sensor data sent to Firebase!");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected! Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nReconnected to Wi-Fi!");
  }

  // Read GPS
  String rawGps = "";
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    rawGps += c;
  }

  // Measure distances
  int d1 = getDistance(TRIG_1, ECHO_1);
  int d2 = getDistance(TRIG_2, ECHO_2);
  int d3 = getDistance(TRIG_3, ECHO_3);

  // Get pothole severity
  String severity1 = getSeverity(SENSOR_1_HEIGHT, d1);
  String severity2 = getSeverity(SENSOR_2_HEIGHT, d2);
  String severity3 = getSeverity(SENSOR_3_HEIGHT, d3);

  // Vibration detection
  bool vibrationDetected = digitalRead(VIBRATION_PIN) == HIGH;

  Serial.print("Vibration: ");
  Serial.println(vibrationDetected ? "Detected!" : "None");

  sendDataToFirebase(rawGps, d1, d2, d3, severity1, severity2, severity3, vibrationDetected);

  delay(5000); // Adjust delay as needed
}

// Ultrasonic distance function
int getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return (duration * 0.034 / 2);
}
