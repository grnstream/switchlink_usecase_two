/************************************************************
 *  ESP32 Smart Garage Monitor & Control (SwitchLink)
 *
 *  Example Scenario:
 *  Imagine you want to monitor and control your garage
 *  remotely. You install an ESP32 in your garage with:
 *    - Sensors to detect door/lock status
 *    - A temperature & humidity sensor (AHT20)
 *    - Relays or outputs to control lights or locks
 *
 *  Using the SwitchLink mobile app, you can:
 *    - Check if your garage is locked or unlocked
 *    - View live temperature and humidity
 *    - Remotely turn devices ON or OFF
 *
 *  ---------------------------------------------------------
 *  WHAT THIS CODE DOES
 *  ---------------------------------------------------------
 *
 *  - Connects ESP32 to Wi-Fi
 *  - Logs into Firebase using Email/Password
 *  - Creates a device node using DEVICE_NAME ("Garage")
 *  - Sends sensor + input data to Firebase
 *  - Reads control commands from SwitchLink
 *  - Controls output pins (relays, lights, etc.)
 *
 *  ---------------------------------------------------------
 *  HOW DATA IS ORGANIZED (IMPORTANT)
 *  ---------------------------------------------------------
 *
 *  All data is stored in:
 *
 *    DEVICE_NAME / Data /
 *
 *  Example:
 *    Garage/Data/
 *
 *  Variable naming format:
 *
 *    w_<index>_<name>   → Sent from ESP32 (displayed in app)
 *    r_<index>_<name>   → Controlled from app (read by ESP32)
 *
 *  Example:
 *    w_0_pushbtn_0  → Door/lock status
 *    w_1_pushbtn_1  → Another input
 *    w_2_temp_0     → Temperature value
 *    w_3_humid_0    → Humidity value
 *    r_0_switch_0   → Controls OUTPUT_0
 *    r_1_switch_1   → Controls OUTPUT_1
 *
 *  ---------------------------------------------------------
 *  DISPLAY ORDER IN SWITCHLINK
 *  ---------------------------------------------------------
 *
 *  The number in the variable name (w_0, r_0, w_1, r_1...)
 *  determines the order in which items appear in the app.
 *
 *  This applies to BOTH:
 *    - w_ variables (sensor/data display)
 *    - r_ variables (controls/switches)
 *
 *  For example:
 *    w_0_... / r_0_... → shown first
 *    w_1_... / r_1_... → shown second
 *    w_2_...           → shown third
 *
 *  This lets you fully control the UI layout directly
 *  from the firmware.
 *
 *  ---------------------------------------------------------
 *  SENSOR NOTE (AHT20)
 *  ---------------------------------------------------------
 *
 *  This example uses an AHT20 sensor to read:
 *    - Temperature
 *    - Humidity
 *
 *  You can replace it with ANY compatible sensor.
 *  Just update:
 *    - The sensor reading code
 *    - The Firebase variable names/values
 *
 *  ---------------------------------------------------------
 *  HOW IT RUNS
 *  ---------------------------------------------------------
 *
 *  Every UPDATE_INTERVAL milliseconds:
 *
 *    1. Read inputs (buttons / sensors)
 *    2. Read temperature & humidity from AHT20
 *    3. Send all values to Firebase (w_ variables)
 *    4. Read control values from Firebase (r_ variables)
 *    5. Update output pins accordingly
 *
 *  ---------------------------------------------------------
 *  BEFORE YOU START
 *  ---------------------------------------------------------
 *
 *  You MUST fill in:
 *
 *  - WIFI_SSID and WIFI_PASSWORD
 *  - FIREBASE_API_KEY
 *  - FIREBASE_DATABASE_URL
 *  - FIREBASE_USER_EMAIL
 *  - FIREBASE_USER_PASSWORD
 *
 *  Otherwise, the device will not connect.
 *
 *  ---------------------------------------------------------
 *  WHAT YOU CAN MODIFY
 *  ---------------------------------------------------------
 *
 *  - DEVICE_NAME (e.g., "Garage", "Home", etc.)
 *  - GPIO pins (INPUT_x, OUTPUT_x)
 *  - UPDATE_INTERVAL (how often data updates)
 *  - Add/remove sensors or inputs
 *  - Rename variables to change app display
 *
 *  ---------------------------------------------------------
 *  EXPANDING THIS PROJECT
 *  ---------------------------------------------------------
 *
 *  You can easily extend this system:
 *
 *  - Add more sensors → w_4_, w_5_, etc.
 *  - Add more controls → r_Switch_2, etc.
 *  - Use different data types (float, int, string)
 *
 *  The SwitchLink app will automatically detect and
 *  display everything based on variable names.
 *
 ************************************************************/

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include "ExampleFunctions.h"
#include <Adafruit_AHTX0.h>

/* ---------------- GPIO DEFINITIONS ---------------- */

// Outputs controlled by the mobile app
#define OUTPUT_0 2
#define OUTPUT_1 13

// Inputs / sensors read by the ESP32
#define INPUT_0 34
#define INPUT_1 35

/* ---------------- WIFI CREDENTIALS ---------------- */

#define WIFI_SSID     "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"

/* ---------------- FIREBASE SETTINGS ---------------- */

#define FIREBASE_API_KEY       "YourFirebaseAPIKey"
#define FIREBASE_DATABASE_URL  "https://your-project-id-default-rtdb.region.firebasedatabase.app/"

#define FIREBASE_USER_EMAIL    "user@email.com"
#define FIREBASE_USER_PASSWORD "password"

// Firebase update interval (default: 5 seconds)
#define UPDATE_INTERVAL 5000

/* ---------------- DEVICE IDENTIFIER ---------------- */

// This name becomes the root node in Firebase
// Example path: Garage/Data/
#define DEVICE_NAME "Garage"

/* ---------------- USER VARIABLES ---------------- */

// Outputs (read from Firebase)
bool boolOut_0;
bool boolOut_1;

// Inputs (written to Firebase)
bool boolIn_0;
bool boolIn_1;

// Optional variables for future expansion
String string_var;
float sensor_float;
int sensor_int;

// Timer variable
unsigned long lastMillis;

/* ---------------- FIREBASE AUTH ---------------- */

// Email / Password authentication
UserAuth user_auth(FIREBASE_API_KEY, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD, 3000);

// Secure client used by Firebase
SSL_CLIENT ssl_client;

/* ---------------- FIREBASE OBJECTS ---------------- */

FirebaseApp app;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

/* ---------------- FIREBASE PATHS ---------------- */

// Root path for this device
String FIREBASE_DEVICE_PATH = DEVICE_NAME;

// All data lives under this node
String FIREBASE_DATA_PATH = FIREBASE_DEVICE_PATH + "/Data";

Adafruit_AHTX0 aht;
sensors_event_t humidity, temp;

/* ==================================================
 *  SETUP
 * ================================================== */
void setup() {
  Serial.begin(115200);

  // Configure GPIO directions
  pinMode(OUTPUT_0, OUTPUT);
  pinMode(OUTPUT_1, OUTPUT);
  pinMode(INPUT_0, INPUT);
  pinMode(INPUT_1, INPUT);

  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    // while (1) delay(10);
  }

  // Connect to Wi-Fi
  wifiConnect();
  delay(2000);

  // Initialize Firebase
  initFirebase();

  // Wait until Firebase is fully ready
  while (!app.ready()) {}

  // Create initial database structure
  setupDatabase();
}

/* ==================================================
 *  MAIN LOOP
 * ================================================== */
void loop() {
  // Required for Firebase background tasks
  app.loop();

  if (millis() >= lastMillis + UPDATE_INTERVAL) {
    lastMillis = millis();

      
    aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

    // Read hardware inputs
    boolIn_0 = digitalRead(INPUT_0);
    boolIn_1 = digitalRead(INPUT_1);

    // Send input states to Firebase
    setData();

    // Read control states from Firebase
    getData();

    // Apply Firebase-controlled outputs
    digitalWrite(OUTPUT_0, boolOut_0);
    digitalWrite(OUTPUT_1, boolOut_1);
  }
}

/* ==================================================
 *  WIFI CONNECTION
 * ================================================== */
void wifiConnect() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/* ==================================================
 *  FIREBASE INITIALIZATION
 * ================================================== */
void initFirebase() {
  // Disable SSL certificate verification (simpler for demo use)
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);

  // Initialize Firebase app with authentication
  initializeApp(aClient, app, getAuth(user_auth));

  // Attach Realtime Database service
  app.getApp<RealtimeDatabase>(Database);
  Database.url(FIREBASE_DATABASE_URL);
}

/* ==================================================
 *  DATABASE SETUP
 * ================================================== */
void setupDatabase() {
  // Push initial input values
  setData();

  // Create default control values (read by ESP32)
  Database.set<bool>(aClient, FIREBASE_DATA_PATH + "/r_Switch_0", false);
  Database.set<bool>(aClient, FIREBASE_DATA_PATH + "/r_Switch_1", false);

  // Example datatypes (optional use)
  // Database.set<String>(aClient, FIREBASE_DATA_PATH + "/r_String_0", "SwitchLink");
  // Database.set<int>(aClient, FIREBASE_DATA_PATH + "/r_int_0", 1234);
  // Database.set<float>(aClient, FIREBASE_DATA_PATH + "/r_float_0", 12.34);
}

/* ==================================================
 *  SEND DATA TO FIREBASE
 * ================================================== */
void setData() {
  if (app.ready()) {
    delay(100);

    // Inputs / sensors written to Firebase
    Database.set<String>(aClient, FIREBASE_DATA_PATH + "/w_0_pushbtn_0", boolIn_0 ? "Locked" : "Unlocked");
    Database.set<String>(aClient, FIREBASE_DATA_PATH + "/w_1_pushbtn_1", boolIn_1 ? "Locked" : "Unlocked");
    Database.set<float>(aClient, FIREBASE_DATA_PATH + "/w_2_temp_0", temp.temperature);
    Database.set<float>(aClient, FIREBASE_DATA_PATH + "/w_3_humid_0", humidity.relative_humidity);

    Serial.println("Firebase set complete");
  }
}

/* ==================================================
 *  READ DATA FROM FIREBASE
 * ================================================== */
void getData() {
  if (app.ready()) {
    delay(100);

    // Read control values from Firebase
    boolOut_0 = Database.get<bool>(aClient, FIREBASE_DATA_PATH + "/r_0_Switch_0");
    boolOut_1 = Database.get<bool>(aClient, FIREBASE_DATA_PATH + "/r_1_Switch_1");

    Serial.println("Firebase get complete");
  }
}
