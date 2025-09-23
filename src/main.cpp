#include <Arduino.h>
#include <HardwareSerial.h>
#include <config.h>

// Function prototypes
bool testAT();
bool checkSIM();
bool enterPIN();
bool waitForNetwork();
bool configureGPRS();
bool connectToInternet();
void printConnectionInfo();
void checkConnectionStatus();
bool waitForResponse(String expectedResponse, unsigned long timeout);
void printATResponse(String label);
void sendATCommand(String command);
void sendWaterMeterData(const char* serverUrl);

// Define pins for A7670E module (based on  LILYGO board)
#define MODEM_UART_BAUD   115200
#define MODEM_DTR_PIN     25
#define MODEM_TX_PIN      26
#define MODEM_RX_PIN      27
#define MODEM_PWRKEY_PIN  4
#define MODEM_POWER_ON_PIN 23


HardwareSerial SerialAT(1);

void setup() {
  Serial.begin(115200);
  delay(10000);
   
  // Initialize modem power pins
  pinMode(MODEM_PWRKEY_PIN, OUTPUT);
  pinMode(MODEM_POWER_ON_PIN, OUTPUT);
  pinMode(MODEM_DTR_PIN, OUTPUT);
  
  // Power on the modem
  digitalWrite(MODEM_POWER_ON_PIN, HIGH);
  digitalWrite(MODEM_DTR_PIN, LOW);
  
  // Power key sequence
  digitalWrite(MODEM_PWRKEY_PIN, HIGH);
  delay(100);
  digitalWrite(MODEM_PWRKEY_PIN, LOW);
  delay(1000);
  digitalWrite(MODEM_PWRKEY_PIN, HIGH);
  
  delay(10000); // Wait for modem to boot
  
  // Initialize serial communication with modem
  SerialAT.begin(MODEM_UART_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    
  // Test AT communication
  if (testAT()) {
    Serial.println("Modem response OK");
  } else {
    Serial.println("Modem not responding. Check connections.");
    return;
  }
  
  // Check SIM card
  if (checkSIM()) {
    Serial.println("SIM card detected");
  } else {
    Serial.println("SIM card not detected");
    return;
  }
  
  // Enter SIM PIN if required
  if (enterPIN()) {
    Serial.println("SIM PIN entered successfully");
  } else {
    Serial.println("Failed to enter SIM PIN");
    return;
  }
  
  // Wait for network registration
  if (waitForNetwork()) {
    Serial.println("Registered on network");
  } else {
    Serial.println("Failed to register on network");
    return;
  }
  
  // Configure GPRS settings
  if (configureGPRS()) {
    Serial.println("GPRS configured successfully");
  } else {
    Serial.println("Failed to configure GPRS");
    return;
  }
  
  // Connect to internet
  if (connectToInternet()) {
    Serial.println("Connected to internet!");
    printConnectionInfo();
  } else {
    Serial.println("Failed to connect to internet");
    return;
  }
}

void loop() {
  // Check connection status and send data every 60 seconds
  static unsigned long lastAction = 0;
  const unsigned long interval = 60000; // 60 seconds
  
  if (millis() - lastAction >= interval) {
    lastAction = millis();
    
    // Check connection status
    checkConnectionStatus();
    
    // Send water meter data
    Serial.println("Sending data...");
    sendWaterMeterData(serverUrl);
  }
  
  delay(1000);
}

bool testAT() {
  SerialAT.println("AT");
  return waitForResponse("OK", 3000);
}

bool checkSIM() {
  SerialAT.println("AT+CPIN?");
  delay(1000);
  
  String response = "";
  while (SerialAT.available()) {
    response += (char)SerialAT.read();
  }
  
  Serial.println("SIM Status: " + response);
  return response.indexOf("READY") != -1 || response.indexOf("SIM PIN") != -1;
}

bool enterPIN() {
  // Check if PIN is required
  SerialAT.println("AT+CPIN?");
  delay(1000);
  
  String response = "";
  while (SerialAT.available()) {
    response += (char)SerialAT.read();
  }
  
  if (response.indexOf("SIM PIN") != -1) {
    Serial.println("PIN required, entering PIN...");
    SerialAT.print("AT+CPIN=");
    SerialAT.println(simPIN);
    delay(3000);
    
    // Check if PIN was accepted
    SerialAT.println("AT+CPIN?");
    return waitForResponse("READY", 5000);
  }
  
  return true; // PIN not required or already entered
}

bool waitForNetwork() {
  Serial.println("Waiting for network registration...");
  
  for (int i = 0; i < 60; i++) { // Wait up to 60 seconds
    SerialAT.println("AT+CREG?");
    delay(1000);
    
    String response = "";
    while (SerialAT.available()) {
      response += (char)SerialAT.read();
    }
    
    // Check for home network (1) or roaming (5)
    if (response.indexOf("+CREG: 0,1") != -1 || response.indexOf("+CREG: 0,5") != -1) {
      Serial.println("Network registration status: " + response);
      return true;
    }
    
    Serial.print(".");
    delay(1000);
  }
  
  return false;
}

bool configureGPRS() {
  Serial.println("Configuring GPRS settings...");
  
  // Set APN
  SerialAT.print("AT+CGDCONT=1,\"IP\",\"");
  SerialAT.print(apn);
  SerialAT.println("\"");
  
  if (!waitForResponse("OK", 5000)) {
    Serial.println("Failed to set APN");
    return false;
  }
  
  // Activate PDP context
  SerialAT.println("AT+CGACT=1,1");
  if (!waitForResponse("OK", 10000)) {
    Serial.println("Failed to activate PDP context");
    return false;
  }
  
  return true;
}

bool connectToInternet() {
  Serial.println("Connecting to internet...");
  
  // Method 1: Try AT+NETOPEN (SIMCom style)
  SerialAT.println("AT+NETOPEN");
  delay(2000);
  
  String response = "";
  while (SerialAT.available()) {
    response += (char)SerialAT.read();
  }
  Serial.println("NETOPEN Response: " + response);
  
  if (response.indexOf("Network opened") != -1 || response.indexOf("already opened") != -1) {
    return true;
  }
  
  // Method 2: Try alternative approach with CGATT
  Serial.println("Trying alternative method...");
  
  // Attach to GPRS service
  SerialAT.println("AT+CGATT=1");
  if (!waitForResponse("OK", 10000)) {
    Serial.println("Failed to attach to GPRS");
    return false;
  }
  
  // Get PDP context status
  SerialAT.println("AT+CGPADDR=1");
  delay(2000);
  
  response = "";
  while (SerialAT.available()) {
    response += (char)SerialAT.read();
  }
  Serial.println("PDP Address Response: " + response);
  
  // If we have an IP address, we're connected
  if (response.indexOf("CGPADDR") != -1 && response.indexOf("0.0.0.0") == -1) {
    return true;
  }
  
  // Method 3: Manual PDP context activation
  Serial.println("Trying manual PDP activation...");
  SerialAT.println("AT+CGACT=1,1");
  delay(5000);
  
  // Check if we got an IP
  SerialAT.println("AT+CGPADDR=1");
  delay(2000);
  
  response = "";
  while (SerialAT.available()) {
    response += (char)SerialAT.read();
  }
  Serial.println("Final PDP Address: " + response);
  
  return (response.indexOf("CGPADDR") != -1 && response.indexOf("0.0.0.0") == -1);
}

void printConnectionInfo() {
  Serial.println("\n=== Connection Information ===");
  
  // Get signal quality
  SerialAT.println("AT+CSQ");
  delay(1000);
  printATResponse("Signal Quality");
  
  // Get network info
  SerialAT.println("AT+COPS?");
  delay(1000);
  printATResponse("Network Operator");
  
  // Get IP address using CGPADDR
  SerialAT.println("AT+CGPADDR=1");
  delay(1000);
  printATResponse("IP Address");
  
  // Also try alternative IP command
  SerialAT.println("AT+IPADDR");
  delay(1000);
  printATResponse("Alternative IP Address");
  
  Serial.println("===============================\n");
}

void checkConnectionStatus() {
  // Check PDP context status
  SerialAT.println("AT+CGPADDR=1");
  delay(1000);
  
  String response = "";
  while (SerialAT.available()) {
    response += (char)SerialAT.read();
  }
  
  if (response.indexOf("CGPADDR") != -1 && response.indexOf("0.0.0.0") == -1) {
    Serial.println("✓ Internet connection active");
    
    // Also check with NETOPEN status
    SerialAT.println("AT+NETOPEN?");
    delay(1000);
    
    String netResponse = "";
    while (SerialAT.available()) {
      netResponse += (char)SerialAT.read();
    }
    Serial.println("Network status: " + netResponse);
    
  } else {
    Serial.println("✗ Internet connection lost - IP: " + response);
    // Attempt to reconnect
    connectToInternet();
  }
}

bool waitForResponse(String expectedResponse, unsigned long timeout) {
  unsigned long startTime = millis();
  String response = "";
  
  while (millis() - startTime < timeout) {
    if (SerialAT.available()) {
      response += (char)SerialAT.read();
    }
    
    if (response.indexOf(expectedResponse) != -1) {
      return true;
    }
  }
  
  return false;
}

void printATResponse(String label) {
  String response = "";
  while (SerialAT.available()) {
    response += (char)SerialAT.read();
  }
  
  if (response.length() > 0) {
    Serial.println(label + ": " + response);
  }
}

// Helper function to send AT command and print response
void sendATCommand(String command) {
  Serial.println("Sending: " + command);
  SerialAT.println(command);
  delay(2000);
  
  String response = "";
  while (SerialAT.available()) {
    response += (char)SerialAT.read();
  }
  
  Serial.println("Response: " + response);
  Serial.println("---");
}

void sendWaterMeterData(const char* serverUrl) {
    // Initialize HTTP service
    SerialAT.println("AT+HTTPINIT");
    delay(1000);

    // Set HTTP parameters
    SerialAT.print("AT+HTTPPARA=\"URL\",\"");
    SerialAT.print(serverUrl);
    SerialAT.println("\"");
    delay(1000);

    SerialAT.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
    delay(1000);

    // Set POST data
    String jsonData = R"({
        "deviceName": "ESP32_Meter_01",
        "devEui": "ABC123456789",
        "address": "123 Main St",
        "totalConsumption": 123.45,
        "rechargeBalance": 50.0,
        "rechargeTimes": 3,
        "waterTemperature": 22.5,
        "pipeBurstAlarm": false,
        "battery1Alarm": false,
        "battery2Alarm": false,
        "emptyTubeAlarm": false,
        "waterTemperatureAlarm": false,
        "reverseFlowAlarm": false,
        "pipeLeakageAlarm": false,
        "overRangeAlarm": false,
        "valveStatus": true,
        "datetime": "2025-09-18 12:00:00",
        "cardId": "CARD123",
        "controlCode": "CTRL01",
        "dataId": "DATA001"
    })";

    SerialAT.print("AT+HTTPDATA=");
    SerialAT.print(jsonData.length());
    SerialAT.println(",10000");
    delay(500);
    SerialAT.print(jsonData);
    delay(10000);

    // Start POST
    SerialAT.println("AT+HTTPACTION=1");
    delay(5000);

    // Read response
    SerialAT.println("AT+HTTPREAD");
    delay(2000);
    String response = "";
    while (SerialAT.available()) {
        response += (char)SerialAT.read();
    }
    Serial.println("HTTP Response: " + response);

    // Terminate HTTP service
    SerialAT.println("AT+HTTPTERM");
    delay(1000);
}