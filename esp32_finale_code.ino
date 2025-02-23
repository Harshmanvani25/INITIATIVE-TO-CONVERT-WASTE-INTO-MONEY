#include "HX711.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Replace with your Wi-Fi credentials
const char* ssid = "Harsh";
const char* password = "1234554321";

// Replace with your Firebase project details
const char* FIREBASE_API_KEY = "AIzaSyCCUFL2o-xKRnvSSdJucc6l0qNtn7Y_dio"; // Web API Key
const char* FIREBASE_PROJECT_ID = "esp32-firebase-demo-8744d"; // Your Firebase Project ID

// User credentials for authentication
const char* userEmail = "testuser@example.com";
const char* userPassword = "testpassword123";

// HX711 Pins
#define DT 16
#define SCK 4

HX711 scale;

// Switch Pins
#define SWITCH_25 33
#define SWITCH_26 26
#define SWITCH_27 27

// Document Path in Firestore
String UID = ""; // UID will be dynamically updated
String documentPath = "";

// Global variable to store the ID token
String idToken = "";

// State variables
bool qrScanned = false;
bool weightAssigned = false;
bool creditUpdated = false;

// Debounce variables
unsigned long lastDebounceTime25 = 0;
unsigned long lastDebounceTime26 = 0;
unsigned long lastDebounceTime27 = 0;
const unsigned long debounceDelay = 50; // Debounce delay in milliseconds

void setup() {
    Serial.begin(115200);
    scale.begin(DT, SCK);
    scale.set_scale(54400); // Replace with your calculated scale factor
    scale.tare(); // Reset scale to zero

    // Initialize switches as inputs
    pinMode(SWITCH_25, INPUT_PULLUP);
    pinMode(SWITCH_26, INPUT_PULLUP);
    pinMode(SWITCH_27, INPUT_PULLUP);

    connectToWiFi();
}

void loop() {
    // Handle Switch 25 (QR scanning phase)
    if (digitalRead(SWITCH_25) == LOW && !qrScanned && (millis() - lastDebounceTime25) > debounceDelay) {
        lastDebounceTime25 = millis(); // Update debounce time
        Serial.println("Switch 25 pressed. Waiting for QR scanning...");
        waitForUID();
        qrScanned = true;
    }

    // Handle Switch 26 (weight assignment phase)
    if (qrScanned && digitalRead(SWITCH_26) == LOW && !weightAssigned && (millis() - lastDebounceTime26) > debounceDelay) {
        lastDebounceTime26 = millis(); // Update debounce time
        Serial.println("Switch 26 pressed. Assigning fixed credit...");
        int creditPoints = 50; // Fixed credit for testing
        Serial.println("Assigned Credit Points: " + String(creditPoints));
        weightAssigned = true;
    }

    // Handle Switch 27 (credit update phase)
    if (weightAssigned && digitalRead(SWITCH_27) == LOW && !creditUpdated && (millis() - lastDebounceTime27) > debounceDelay) {
        lastDebounceTime27 = millis(); // Update debounce time
        Serial.println("Switch 27 pressed. Updating credit points...");

        // Authenticate and fetch ID token
        if (authenticateUser(userEmail, userPassword)) {
            Serial.println("Authentication successful!");

            // Fetch and display current credit points
            int currentCredit = fetchCreditPoints(documentPath);
            if (currentCredit >= 0) {
                Serial.println("Current Credit Points: " + String(currentCredit));

                // Update Firestore data (add credit points)
                updateFirestoreData(documentPath, 50); // Add fixed 50 points to 'credit'
            }
        } else {
            Serial.println("Authentication failed. Cannot proceed.");
        }

        creditUpdated = true;
    }

    // Reset the system for the next user after Switch 27 is pressed
    if (creditUpdated) {
        Serial.println("Process completed. Waiting for Switch 25 to start again...");
        UID = "";
        documentPath = "";
        qrScanned = false;
        weightAssigned = false;
        creditUpdated = false;
    }
}

// Function to wait for UID input
void waitForUID() {
    while (UID == "") {
        Serial.println("Waiting to receive UID...");
        delay(1000);

        if (Serial.available() > 0) {
            UID = Serial.readStringUntil('\n');
            UID.trim(); // Ensure no extra spaces or newline characters
            if (UID != "") {
                Serial.println("Received UID: " + UID);
                documentPath = "users/" + UID;
            } else {
                Serial.println("Invalid UID. Please try again.");
            }
        }
    }
}

// Function to connect to Wi-Fi
void connectToWiFi() {
    Serial.print("\nConnecting to Wi-Fi");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nConnected to Wi-Fi!");
}

// Function to authenticate the user
bool authenticateUser(const char* email, const char* password) {
    HTTPClient http;
    String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + String(FIREBASE_API_KEY);
    Serial.println("Authenticating with URL: " + url);

    DynamicJsonDocument authPayload(512);
    authPayload["email"] = email;
    authPayload["password"] = password;
    authPayload["returnSecureToken"] = true;

    String requestBody;
    serializeJson(authPayload, requestBody);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode == HTTP_CODE_OK) {
        String response = http.getString();
        Serial.println("Authentication response:");
        Serial.println(response);

        DynamicJsonDocument responseDoc(1024);
        DeserializationError error = deserializeJson(responseDoc, response);

        if (!error) {
            idToken = responseDoc["idToken"].as<String>();
            http.end();
            return true;
        } else {
            Serial.println("JSON Parsing Error: " + String(error.c_str()));
        }
    } else {
        Serial.println("Error in authentication, HTTP Response Code: " + String(httpResponseCode));
    }

    http.end();
    return false;
}

// Function to fetch the current credit points
int fetchCreditPoints(String documentPath) {
    if (idToken == "") {
        Serial.println("ID token is missing. Authentication is required.");
        return -1;
    }

    HTTPClient http;
    String url = "https://firestore.googleapis.com/v1/projects/" + String(FIREBASE_PROJECT_ID) + "/databases/(default)/documents/" + documentPath;
    Serial.println("Fetching data from: " + url);

    http.begin(url);
    http.addHeader("Authorization", "Bearer " + idToken);

    int httpResponseCode = http.GET();

    if (httpResponseCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Data fetched successfully:");
        Serial.println(payload);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            if (doc["fields"]["credit"].isNull()) {
                Serial.println("Credit field does not exist. Defaulting to 0.");
                return 0;
            } else {
                return doc["fields"]["credit"]["integerValue"].as<int>();
            }
        } else {
            Serial.println("JSON Parsing Error: " + String(error.c_str()));
        }
    } else {
        Serial.println("Error fetching data, HTTP Response Code: " + String(httpResponseCode));
    }

    http.end();
    return -1;
}

// Function to update or add credit field in Firestore
void updateFirestoreData(String documentPath, int creditPointsToAdd) {
    if (idToken == "") {
        Serial.println("ID token is missing. Authentication is required.");
        return;
    }

    int currentCredit = fetchCreditPoints(documentPath);
    if (currentCredit < 0) {
        Serial.println("Failed to fetch current credit points. Aborting update.");
        return;
    }

    int updatedCredit = currentCredit + creditPointsToAdd;

    HTTPClient http;
    String url = "https://firestore.googleapis.com/v1/projects/" + String(FIREBASE_PROJECT_ID) + "/databases/(default)/documents/" + documentPath;
    Serial.println("Updating data at: " + url);

    DynamicJsonDocument updatePayload(512);
    updatePayload["fields"]["credit"]["integerValue"] = updatedCredit;

    String requestBody;
    serializeJson(updatePayload, requestBody);

    http.begin(url + "?updateMask.fieldPaths=credit");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + idToken);
    int httpResponseCode = http.PATCH(requestBody);

    if (httpResponseCode == HTTP_CODE_OK) {
        Serial.println("Data updated successfully!");
        Serial.println("Updated Credit Points: " + String(updatedCredit));
    } else {
        Serial.println("Error updating data, HTTP Response Code: " + String(httpResponseCode));
        Serial.println("Error response: " + http.getString());
    }

    http.end();
}
