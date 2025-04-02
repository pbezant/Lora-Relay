#include <Arduino.h>
#include <LoRaManager.h>
#include <ArduinoJson.h>

// Debug mode
#define DEBUG_MODE true

// LoRa pins for HELTEC WiFi LoRa 32 V3
#define LORA_CS   8     // NSS pin
#define LORA_DIO1 14    // DIO1 pin
#define LORA_RST  12    // RESET pin
#define LORA_BUSY 13    // BUSY pin

// Define the frequency band and subband (1-8)
#define LORAWAN_FREQUENCY_BAND US915
#define LORAWAN_SUBBAND 2  // Subband 2 (channels 8-15)

// LoRaWAN credentials - Replace with your values from TTN
uint64_t appEui = 0x70B3D57ED800410A; // Join EUI (AppEUI) from TTN
uint64_t devEui = 0x70B3D57ED800410A;  // Device EUI from TTN
uint8_t appKey[] = { 0x38, 0x6A, 0xCC, 0x7F, 0x0E, 0x22, 0xEE, 0x60, 0x7C, 0xF0, 0xB2, 0x94, 0x66, 0xD3, 0x9C, 0xB5 }; // App Key from TTN
uint8_t nwkKey[] = { 0x38, 0x6A, 0xCC, 0x7F, 0x0E, 0x22, 0xEE, 0x60, 0x7C, 0xF0, 0xB2, 0x94, 0x66, 0xD3, 0x9C, 0xB5 }; // Network Key from TTN

// Relay pin configuration - Using digital pins that support OUTPUT mode
const int RELAY_PINS[8] = {2, 4, 5, 15, 16, 17, 18, 19}; // Changed to safer GPIO pins for ESP32-S3
bool relayStates[8] = {false};
unsigned long relayTimers[8] = {0};

// Create LoRaManager instance
LoRaManager lora(LORAWAN_FREQUENCY_BAND, LORAWAN_SUBBAND);

// Status variables
bool loraInitialized = false;
bool loraConnected = false;
String lastCommand = "None";
unsigned long lastStatusSendTime = 0;
const unsigned long STATUS_SEND_INTERVAL = 300000; // 5 minutes
unsigned long lastJoinAttempt = 0;

// Function prototypes
void processDownlink(uint8_t* payload, size_t size, uint8_t port);
void setRelay(uint8_t relayNum, bool state, unsigned long duration = 0);
void checkRelayTimers();
void sendStatusPacket();
void processSerialCommand(String command);
void processJsonCommand(String jsonString);
void processHexCommand(uint8_t* payload, size_t size);
void debugPrint(String message);

// Debug print helper
void debugPrint(String message) {
  if (DEBUG_MODE) {
    Serial.println("[DEBUG] " + message);
  }
}

void setup() {
  // Initialize serial at high speed
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\nLoRaWAN Class C Relay Controller");
  Serial.println("================================");
  
  // Initialize relay pins
  Serial.println("Initializing relay pins...");
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
    delay(10);
  }
  Serial.println("Relay pins initialized.");
  
  delay(100);
  
  // Initialize LoRa with simplified approach
  Serial.println("Initializing LoRa module...");
  
  // Set up SPI pins
  SPI.begin(); // Initialize SPI first
  
  // Reset sequence for the LoRa module
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, LOW);
  delay(10);
  digitalWrite(LORA_RST, HIGH);
  delay(100);
  
  // Try to initialize
  if (lora.begin(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY)) {
    Serial.println("LoRa module initialized successfully!");
    loraInitialized = true;
    
    // Set credentials
    lora.setCredentials(appEui, devEui, appKey, nwkKey);
    lora.setDownlinkCallback(processDownlink);
    
    // Don't try joining here - do it in the loop
    Serial.println("LoRa setup complete. Will attempt joining in main loop.");
  } else {
    Serial.println("Failed to initialize LoRa module.");
  }
  
  Serial.println("Setup complete - entering main loop.");
}

void loop() {
  // Always handle any serial input first
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    processSerialCommand(command);
  }
  
  // Handle LoRa operations if initialized
  if (loraInitialized) {
    // Process any LoRa events
    lora.handleEvents();
    
    // Join if not connected (check only every 2 minutes)
    unsigned long currentTime = millis();
    if (!loraConnected && (currentTime - lastJoinAttempt > 120000 || lastJoinAttempt == 0)) {
      Serial.println("Attempting to join LoRaWAN network...");
      
      if (lora.joinNetwork()) {
        Serial.println("Successfully joined the network!");
        loraConnected = true;
        sendStatusPacket();
      } else {
        Serial.println("Failed to join network. Will retry later.");
      }
      
      lastJoinAttempt = currentTime;
    }
    
    // Send periodic status if connected
    if (loraConnected && (currentTime - lastStatusSendTime >= STATUS_SEND_INTERVAL)) {
      sendStatusPacket();
    }
  }
  
  // Check and update relay timers
  checkRelayTimers();
  
  // Small delay to prevent watchdog issues
  delay(10);
  yield();
}

void processDownlink(uint8_t* payload, size_t size, uint8_t port) {
  Serial.print("Received downlink on port ");
  Serial.print(port);
  Serial.print(", size: ");
  Serial.print(size);
  Serial.print(" bytes: ");
  
  // Print the payload in hex for debugging
  for (size_t i = 0; i < size; i++) {
    if (payload[i] < 16) Serial.print("0");
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Determine if it's a hex command or JSON command
  if (size >= 1 && size <= 3) {
    // Likely a hex command
    processHexCommand(payload, size);
  } else {
    // Convert payload to string for JSON parsing
    String jsonString = "";
    for (size_t i = 0; i < size; i++) {
      jsonString += (char)payload[i];
    }
    
    Serial.println("Parsing as JSON: " + jsonString);
    processJsonCommand(jsonString);
  }
}

void processHexCommand(uint8_t* payload, size_t size) {
  // Hex command format:
  // Byte 0: Command type (0x01 = relay control)
  // Byte 1: Relay number (0-7) in bits 0-2, state in bit 7 (0=off, 1=on)
  // Byte 2 (optional): Duration in seconds (0-255)
  
  if (size < 1) return;
  
  uint8_t commandType = payload[0];
  
  if (commandType == 0x01 && size >= 2) {
    // Relay control command
    uint8_t relayData = payload[1];
    uint8_t relayNum = relayData & 0x07;  // Extract bits 0-2 (relay number 0-7)
    bool state = (relayData & 0x80) > 0;  // Extract bit 7 (state)
    
    unsigned long duration = 0;
    if (size >= 3) {
      duration = payload[2] * 1000; // Convert to milliseconds
    }
    
    Serial.print("HEX command: Relay ");
    Serial.print(relayNum + 1);
    Serial.print(" -> ");
    Serial.print(state ? "ON" : "OFF");
    if (duration > 0) {
      Serial.print(" for ");
      Serial.print(duration / 1000);
      Serial.println(" seconds");
    } else {
      Serial.println();
    }
    
    // Set the relay state
    setRelay(relayNum, state, duration);
    lastCommand = "HEX:R" + String(relayNum + 1) + ":" + (state ? "ON" : "OFF");
  } else {
    Serial.println("Unknown HEX command type");
  }
}

void processJsonCommand(String jsonString) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    lastCommand = "Parse Error: " + String(error.c_str());
    return;
  }
  
  if (doc.containsKey("relay") && doc.containsKey("state")) {
    int relay = doc["relay"];
    int state = doc["state"];
    unsigned long duration = 0;
    
    if (doc.containsKey("duration")) {
      duration = doc["duration"].as<unsigned long>() * 1000;
    }
    
    if (relay >= 1 && relay <= 8) {
      lastCommand = "Relay:" + String(relay) + " State:" + String(state);
      if (duration > 0) {
        lastCommand += " Duration:" + String(duration / 1000) + "s";
      }
      
      setRelay(relay - 1, state == 1, duration);
    } else {
      lastCommand = "Invalid relay: " + String(relay);
    }
  } else {
    lastCommand = "Missing relay/state";
  }
}

void setRelay(uint8_t relayNum, bool state, unsigned long duration) {
  if (relayNum < 8) {
    relayStates[relayNum] = state;
    
    if (duration > 0 && state) {
      relayTimers[relayNum] = millis() + duration;
      debugPrint("Set timer for relay " + String(relayNum + 1) + " for " + String(duration / 1000) + " seconds");
    } else {
      relayTimers[relayNum] = 0;
    }
    
    digitalWrite(RELAY_PINS[relayNum], state ? HIGH : LOW);
    
    Serial.print("Relay ");
    Serial.print(relayNum + 1);
    Serial.print(" set to ");
    Serial.println(state ? "ON" : "OFF");
    if (duration > 0 && state) {
      Serial.print(" for ");
      Serial.print(duration / 1000);
      Serial.println(" seconds");
    }
  }
}

void checkRelayTimers() {
  unsigned long currentTime = millis();
  
  for (int i = 0; i < 8; i++) {
    if (relayTimers[i] > 0 && relayTimers[i] <= currentTime && relayStates[i]) {
      debugPrint("Timer expired for relay " + String(i + 1));
      setRelay(i, false);
    }
  }
}

void sendStatusPacket() {
  if (!loraInitialized || !loraConnected) {
    debugPrint("Cannot send status: LoRaWAN not ready");
    return;
  }
  
  uint8_t relayStateByte = 0;
  for (int i = 0; i < 8; i++) {
    if (relayStates[i]) {
      relayStateByte |= (1 << i);
    }
  }
  
  uint8_t payload[2] = {relayStateByte, 0};
  
  if (lora.sendData(payload, sizeof(payload), 2, false)) {
    Serial.println("Status packet sent successfully");
    lastStatusSendTime = millis();
  } else {
    Serial.println("Failed to send status packet");
  }
}

void processSerialCommand(String command) {
  if (command.startsWith("relay,")) {
    // Parse relay command: relay,<number>,<state>[,<duration>]
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    int thirdComma = command.indexOf(',', secondComma + 1);
    
    if (secondComma > firstComma) {
      int relayNum = command.substring(firstComma + 1, secondComma).toInt();
      int state = command.substring(secondComma + 1, thirdComma > 0 ? thirdComma : command.length()).toInt();
      unsigned long duration = 0;
      
      if (thirdComma > 0) {
        duration = command.substring(thirdComma + 1).toInt() * 1000;
      }
      
      if (relayNum >= 1 && relayNum <= 8) {
        setRelay(relayNum - 1, state == 1, duration);
      } else {
        Serial.println("Invalid relay number (must be 1-8)");
      }
    }
  } else if (command == "status") {
    // Show current status
    Serial.println("Current Status:");
    Serial.println("LoRaWAN Initialized: " + String(loraInitialized ? "Yes" : "No"));
    Serial.println("LoRaWAN Connected: " + String(loraConnected ? "Yes" : "No"));
    Serial.println("Relay States:");
    
    for (int i = 0; i < 8; i++) {
      Serial.print("Relay ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(relayStates[i] ? "ON" : "OFF");
      
      if (relayTimers[i] > 0 && relayStates[i]) {
        unsigned long remaining = (relayTimers[i] - millis()) / 1000;
        Serial.print(" (turns off in ");
        Serial.print(remaining);
        Serial.print(" seconds)");
      }
      
      Serial.println();
    }
  } else if (command == "join") {
    // Force join attempt
    if (loraInitialized) {
      Serial.println("Attempting to join LoRaWAN network...");
      
      if (lora.joinNetwork()) {
        loraConnected = true;
        Serial.println("Successfully joined the network!");
      } else {
        Serial.println("Failed to join network.");
      }
      
      lastJoinAttempt = millis();
    } else {
      Serial.println("Cannot join: LoRaWAN not initialized");
    }
  } else if (command == "test_json") {
    // Test JSON command processing
    Serial.println("Running JSON command test...");
    
    String testJson = "{\"relay\":1,\"state\":1}";
    Serial.println("Test command: " + testJson);
    processJsonCommand(testJson);
    
    delay(2000);
    
    testJson = "{\"relay\":1,\"state\":0}";
    Serial.println("Test command: " + testJson);
    processJsonCommand(testJson);
  } else if (command == "test_hex") {
    // Test HEX command processing
    Serial.println("Running HEX command test...");
    
    // Test turning relay 1 ON
    uint8_t hexCmd1[] = {0x01, 0x80};  // Command 0x01, Relay 0, ON
    Serial.println("Test command: 01 80");
    processHexCommand(hexCmd1, 2);
    
    delay(2000);
    
    // Test turning relay 1 OFF
    uint8_t hexCmd2[] = {0x01, 0x00};  // Command 0x01, Relay 0, OFF
    Serial.println("Test command: 01 00");
    processHexCommand(hexCmd2, 2);
  } else if (command == "reset") {
    Serial.println("Resetting device...");
    delay(500);
    ESP.restart();
  } else {
    Serial.println("Available commands:");
    Serial.println("  relay,<number>,<state>[,<duration>] - Control relay");
    Serial.println("  status - Show current status");
    Serial.println("  join - Force join attempt");
    Serial.println("  test_json - Run test JSON commands");
    Serial.println("  test_hex - Run test HEX commands");
    Serial.println("  reset - Reset the device");
  }
}