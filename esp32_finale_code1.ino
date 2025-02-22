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

// Document Path in Firestore
String UID = "1v1qsy28crRuFk8ihQDCKCLEpsS2"; // Replace with your UID
String documentPath = "users/" + UID;

// Global variable to store the ID token
String idToken = "";

void setup() {
  Serial.begin(115200);
  connectToWiFi();

  // Authenticate and fetch ID token
  if (authenticateUser(userEmail, userPassword)) {
    Serial.println("Authentication successful!");

    // Update Firestore data (add credit points)
    updateFirestoreData(documentPath, 50); // Add 50 points to 'credit'
  } else {
    Serial.println("Authentication failed. Cannot proceed.");
  }
}

void loop() {
  // Add loop code if needed
}

// Function to connect to Wi-Fi
void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi");
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

  // Create the JSON payload for authentication
  DynamicJsonDocument authPayload(256);
  authPayload["email"] = email;
  authPayload["password"] = password;
  authPayload["returnSecureToken"] = true;

  String requestBody;
  serializeJson(authPayload, requestBody);

  // Send POST request
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Authentication response:");
    Serial.println(response);

    // Parse the JSON response
    DynamicJsonDocument responseDoc(1024);
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error) {
      idToken = responseDoc["idToken"].as<String>();
      http.end();
      return true;
    } else {
      Serial.print("JSON Parsing Error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.println("Error in authentication");
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
  }

  http.end();
  return false;
}

// Function to fetch the current credit points
int fetchCreditPoints(String documentPath) {
  if (idToken == "") {
    Serial.println("ID token is missing. Authentication is required.");
    return 0;
  }

  HTTPClient http;

  // Construct the Firestore REST API URL
  String url = "https://firestore.googleapis.com/v1/projects/" + String(FIREBASE_PROJECT_ID) + "/databases/(default)/documents/" + documentPath;
  Serial.println("Fetching data from: " + url);

  // Set Authorization header
  http.begin(url);
  http.addHeader("Authorization", "Bearer " + idToken);

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println("Data fetched successfully:");
    Serial.println(payload);

    // Parse JSON response
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      if (doc["fields"]["credit"].isNull()) {
        Serial.println("Credit field does not exist. Defaulting to 0.");
        return 0;
      } else {
        int credit = doc["fields"]["credit"]["integerValue"].as<int>();
        Serial.println("Current Credit: " + String(credit));
        return credit;
      }
    } else {
      Serial.print("JSON Parsing Error: ");
      Serial.println(error.c_str());
      return 0;
    }
  } else {
    Serial.println("Error fetching data");
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
  }

  http.end();
  return 0;
}

// Function to update or add credit field in Firestore
void updateFirestoreData(String documentPath, int creditPointsToAdd) {
  if (idToken == "") {
    Serial.println("ID token is missing. Authentication is required.");
    return;
  }

  // Fetch existing credit points
  int currentCredit = fetchCreditPoints(documentPath);

  // Calculate the new credit points
  int updatedCredit = currentCredit + creditPointsToAdd;

  HTTPClient http;

  // Construct the Firestore REST API URL
  String url = "https://firestore.googleapis.com/v1/projects/" + String(FIREBASE_PROJECT_ID) + "/databases/(default)/documents/" + documentPath;
  Serial.println("Updating data at: " + url);

  // Create JSON payload for updating
  DynamicJsonDocument updatePayload(1024);

  // Add or update the credit field
  updatePayload["fields"]["credit"]["integerValue"] = updatedCredit;

  String requestBody;
  serializeJson(updatePayload, requestBody);

  // Send PATCH request
  http.begin(url + "?updateMask.fieldPaths=credit"); // Add updateMask to specify only the 'credit' field
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + idToken);

  int httpResponseCode = http.PATCH(requestBody);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Update response:");
    Serial.println(response);

    // Display updated credit
    Serial.println("Updated Credit Points: " + String(updatedCredit));
  } else {
    Serial.println("Error updating data");
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
  }

  http.end();
}
