#include <Arduino.h>
#include <LoRaManager.h>
#include <ArduinoJson.h>
#include "secrets.h"

// Debug mode
#define DEBUG_MODE true

// Relay pin configuration - Using digital pins that support OUTPUT mode
// const int RELAY_PINS[8] = {21, 26, 48, 47, 33, 34, 35, 36};
const int RELAY_PINS[8] = {36, 35, 34, 33, 47, 48, 26, 21};
bool relayStates[8] = {false};
unsigned long relayTimers[8] = {0};

// Create LoraManager instance
LoraManager lora;

// Status variables
bool loraInitialized = false;
bool loraJoined = false;
String lastCommand = "None";
unsigned long lastStatusSendTime = 0;
const unsigned long STATUS_SEND_INTERVAL = 300000; // 5 minutes

// Function prototypes
void onJoined();
void onJoinFailed();
void onDownlink(const uint8_t* data, size_t size, int rssi, int snr);
void onClassChanged(uint8_t deviceClass);
void onTxComplete(bool success);
void setRelay(uint8_t relayNum, bool state, unsigned long duration = 0);
void checkRelayTimers();
void sendStatusPacket();
void processSerialCommand(String command);
void processJsonCommand(String jsonString);
void processHexCommand(const uint8_t* payload, size_t size);
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

  Serial.println("\n\nLoRaWAN Class C Relay Controller (LoraManager2)");
  Serial.println("================================================");
  
  // Initialize relay pins
  Serial.println("Initializing relay pins...");
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], HIGH); // Initial state OFF (inverted logic)
    delay(10);
  }
  Serial.println("Relay pins initialized.");
  
  delay(100);
  
  // Initialize LoRa with LoraManager2
  Serial.println("Initializing LoRa module with LoraManager2...");
  
  // Configure LoRaWAN settings
  LoraConfig config;
  config.devEui = devEui;
  config.appEui = appEui;
  config.appKey = appKey;
  config.region = US915;
  config.deviceClass = LORA_CLASS_C;  // TRUE Class C operation
  config.subBand = 2;                 // Subband 2 (channels 8-15)
  config.adrEnabled = false;
  config.dataRate = 3;                // DR_3
  config.txPower = 14;                // 14 dBm
  config.joinTrials = 5;
  config.publicNetwork = true;
  
  // Hardware configuration (uses Heltec V3 defaults)
  HardwareConfig hwConfig;
  
  // Register event callbacks
  lora.onJoined(onJoined);
  lora.onJoinFailed(onJoinFailed);
  lora.onDownlink(onDownlink);
  lora.onClassChanged(onClassChanged);
  lora.onTxComplete(onTxComplete);
  
  // Initialize LoraManager2
  if (lora.begin(config, hwConfig)) {
    Serial.println("LoraManager2 initialized successfully!");
    Serial.println("Configured for TRUE Class C operation");
    loraInitialized = true;
  } else {
    Serial.println("Failed to initialize LoraManager2.");
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
    // Process LoRa events (this handles join attempts, receive windows, etc.)
    lora.loop();
    
    // Send periodic status if joined
    if (loraJoined) {
      unsigned long currentTime = millis();
      if (currentTime - lastStatusSendTime >= STATUS_SEND_INTERVAL) {
        sendStatusPacket();
      }
    }
  }
  
  // Check and update relay timers
  checkRelayTimers();
  
  // Small delay to prevent watchdog issues
  delay(10);
  yield();
}

// LoraManager2 event callbacks
void onJoined() {
  Serial.println("Successfully joined the LoRaWAN network!");
  loraJoined = true;
  
  // Send initial status packet
  sendStatusPacket();
}

void onJoinFailed() {
  Serial.println("Failed to join LoRaWAN network. Will retry automatically.");
  loraJoined = false;
}

void onDownlink(const uint8_t* data, size_t size, int rssi, int snr) {
  Serial.println("\n[DOWNLINK RECEIVED]");
  Serial.print("Size: ");
  Serial.println(size);
  Serial.print("RSSI: ");
  Serial.print(rssi);
  Serial.print(" dBm, SNR: ");
  Serial.print(snr);
  Serial.println(" dB");
  Serial.print("Payload (HEX): ");
  
  // Print the payload in hex for debugging
  for (size_t i = 0; i < size; i++) {
    if (data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Print as string if possible
  Serial.print("Payload (ASCII): ");
  for (size_t i = 0; i < size; i++) {
    if (isprint(data[i])) {
      Serial.print((char)data[i]);
    } else {
      Serial.print(".");
    }
  }
  Serial.println();
  
  // Determine if it's a hex command or JSON command
  if (size >= 1 && size <= 3) {
    Serial.println("Processing as HEX command");
    processHexCommand(data, size);
  } else {
    // Convert payload to string for JSON parsing
    String jsonString = "";
    for (size_t i = 0; i < size; i++) {
      jsonString += (char)data[i];
    }
    
    Serial.println("Processing as JSON command: " + jsonString);
    processJsonCommand(jsonString);
  }
  
  Serial.println("[END DOWNLINK]\n");
  
  // Send a status packet to confirm the change
  sendStatusPacket();
}

void onClassChanged(uint8_t deviceClass) {
  Serial.print("Device class changed to: ");
  switch(deviceClass) {
    case LORA_CLASS_A:
      Serial.println("Class A");
      break;
    case LORA_CLASS_B:
      Serial.println("Class B");
      break;
    case LORA_CLASS_C:
      Serial.println("Class C (continuous receive)");
      break;
    default:
      Serial.println("Unknown");
      break;
  }
}

void onTxComplete(bool success) {
  if (success) {
    debugPrint("Transmission completed successfully");
    lastStatusSendTime = millis();
  } else {
    debugPrint("Transmission failed");
  }
}

void processHexCommand(const uint8_t* payload, size_t size) {
  if (size < 1) {
    Serial.println("HEX command too short");
    return;
  }
  
  uint8_t commandType = payload[0];
  Serial.print("Command type: 0x");
  Serial.println(commandType, HEX);
  
  if (commandType == 0x01 && size >= 2) {
    // Relay control command
    uint8_t relayData = payload[1];
    uint8_t relayNum = relayData & 0x07;  // Extract bits 0-2 (relay number 0-7)
    bool state = (relayData & 0x80) > 0;  // Extract bit 7 (state)
    
    unsigned long duration = 0;
    if (size >= 3) {
      duration = payload[2] * 1000; // Convert to milliseconds
    }
    
    Serial.print("HEX command decoded: Relay ");
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
  Serial.println("Attempting to parse JSON: " + jsonString);
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    lastCommand = "Parse Error: " + String(error.c_str());
    return;
  }
  
  Serial.println("JSON parsed successfully");
  
  if (doc.containsKey("relay") && doc.containsKey("state")) {
    int relay = doc["relay"];
    
    // Handle both numeric and string state values
    bool state = false;
    if (doc["state"].is<int>()) {
      state = doc["state"].as<int>() == 1;
    } else if (doc["state"].is<String>()) {
      String stateStr = doc["state"].as<String>();
      stateStr.toLowerCase();
      state = (stateStr == "on" || stateStr == "1" || stateStr == "true");
    }
    
    unsigned long duration = 0;
    
    Serial.print("JSON command decoded: Relay ");
    Serial.print(relay);
    Serial.print(" -> ");
    Serial.println(state ? "ON" : "OFF");
    
    if (doc.containsKey("duration")) {
      duration = doc["duration"].as<unsigned long>() * 1000;
      Serial.print("Duration: ");
      Serial.print(duration / 1000);
      Serial.println(" seconds");
    }
    
    if (relay >= 1 && relay <= 8) {
      lastCommand = "Relay:" + String(relay) + " State:" + String(state ? "ON" : "OFF");
      if (duration > 0) {
        lastCommand += " Duration:" + String(duration / 1000) + "s";
      }
      
      setRelay(relay - 1, state, duration);
    } else {
      Serial.println("Invalid relay number (must be 1-8)");
      lastCommand = "Invalid relay: " + String(relay);
    }
  } else {
    Serial.println("Missing relay/state in JSON");
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
    
    // Invert the logic: HIGH (1) turns relay OFF, LOW (0) turns relay ON
    digitalWrite(RELAY_PINS[relayNum], state ? LOW : HIGH);
    
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
  if (!loraInitialized || !loraJoined) {
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
  
  // Send status packet with confirmed flag to ensure delivery
  if (lora.sendConfirmed(payload, sizeof(payload), 2)) {
    Serial.println("Status packet sent successfully");
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
    Serial.println("\n=== Device Status ===");
    Serial.println("LoRaWAN Status:");
    Serial.println("- Initialized: " + String(loraInitialized ? "Yes" : "No"));
    Serial.println("- Joined: " + String(loraJoined ? "Yes" : "No"));
    Serial.println("- Device Class: " + String(lora.getCurrentClass()));
    Serial.println("- Device EUI: " + lora.getDeviceEUI());
    Serial.println("- App EUI: " + lora.getAppEUI());
    Serial.println("- Last Command: " + lastCommand);
    Serial.println("- Last Status Send: " + String((millis() - lastStatusSendTime) / 1000) + "s ago");
    
    Serial.println("\nRelay States:");
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
    
    Serial.println("\nDevice Info:");
    Serial.print("- Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    Serial.println("===================\n");
  } else if (command == "send") {
    // Force send a status packet
    sendStatusPacket();
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
  } else {
    Serial.println("Unknown command. Available commands:");
    Serial.println("- relay,<number>,<state>[,<duration>] - Control a relay");
    Serial.println("- status - Show current status");
    Serial.println("- send - Force send status packet");
    Serial.println("- test_json - Test JSON command processing");
    Serial.println("- test_hex - Test HEX command processing");
  }
}