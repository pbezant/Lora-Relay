# Technical Specifications

## LoRaWAN Implementation with LoraManager2

### Core Technical Requirements

#### LoRaWAN Protocol Compliance
- **Specification**: LoRaWAN 1.0.x / 1.1 compatible
- **Device Class**: Class C (TRUE continuous receive)
- **Activation**: OTAA (Over-The-Air-Activation) only
- **Region**: US915 (configurable for other regions)
- **Subband**: 2 (channels 8-15, configurable 1-8)

#### Hardware Platform
- **MCU**: ESP32-S3 (240MHz dual-core, 512KB SRAM)
- **Radio**: SX1262 (sub-GHz LoRa transceiver)
- **Board**: Heltec WiFi LoRa 32 V3
- **Power**: USB 5V or external 3.7V Li-Po
- **GPIO**: 8 relay control outputs (3.3V logic)

### Established Patterns

#### 1. Configuration Management Pattern
```cpp
// Standard configuration initialization
LoraConfig config;
config.devEui = "DEVICE_EUI_HEX_STRING";
config.appEui = "APPLICATION_EUI_HEX_STRING";  
config.appKey = "APPLICATION_KEY_HEX_STRING";
config.region = US915;
config.deviceClass = LORA_CLASS_C;
config.subBand = 2;
config.adrEnabled = false;
config.dataRate = 3;
config.txPower = 14;
config.joinTrials = 5;
config.publicNetwork = true;

// Hardware configuration uses library defaults
HardwareConfig hwConfig; // Heltec V3 optimized defaults
```

#### 2. Event-Driven Callback Pattern
```cpp
// Register all callbacks before initialization
lora.onJoined(onJoined);
lora.onJoinFailed(onJoinFailed);
lora.onDownlink(onDownlink);
lora.onClassChanged(onClassChanged);
lora.onTxComplete(onTxComplete);

// Initialize with configuration
if (lora.begin(config, hwConfig)) {
    // Initialization successful
}
```

#### 3. Main Loop Pattern
```cpp
void loop() {
    // 1. Handle serial input (debugging/testing)
    if (Serial.available()) {
        processSerialCommand(Serial.readStringUntil('\n'));
    }
    
    // 2. Process LoRa events (join, RX, TX)
    if (loraInitialized) {
        lora.loop(); // Essential for LoraManager2 operation
    }
    
    // 3. Handle application-specific timers
    checkRelayTimers();
    
    // 4. Send periodic status (if connected)
    if (loraJoined && shouldSendStatus()) {
        sendStatusPacket();
    }
    
    // 5. Yield for watchdog
    yield();
}
```

#### 4. Downlink Processing Pattern
```cpp
void onDownlink(const uint8_t* data, size_t size, int rssi, int snr) {
    // 1. Log reception details
    debugLog("RX", size, rssi, snr);
    
    // 2. Determine command format (size-based heuristic)
    if (size <= 3) {
        processHexCommand(data, size);
    } else {
        processJsonCommand(dataToString(data, size));
    }
    
    // 3. Send confirmation (status packet)
    sendStatusPacket();
}
```

#### 5. JSON Command Processing Pattern
```cpp
void processJsonCommand(String jsonString) {
    // 1. Parse with error handling
    JsonDocument doc;
    if (deserializeJson(doc, jsonString) != DeserializationError::Ok) {
        logError("JSON parse failed");
        return;
    }
    
    // 2. Validate required fields
    if (!doc.containsKey("relay") || !doc.containsKey("state")) {
        logError("Missing required fields");
        return;
    }
    
    // 3. Handle flexible state format
    bool state = parseStateValue(doc["state"]);
    int relay = doc["relay"];
    unsigned long duration = doc["duration"] | 0;
    
    // 4. Execute command with validation
    if (relay >= 1 && relay <= 8) {
        setRelay(relay - 1, state, duration * 1000);
    }
}
```

### Pin Configuration Standards

#### Standard Heltec V3 Pin Mapping
```cpp
// LoRa SPI Interface (managed by LoraManager2)
#define LORA_NSS    8   // SPI Chip Select
#define LORA_SCK    9   // SPI Clock
#define LORA_MISO   11  // SPI Master In Slave Out
#define LORA_MOSI   10  // SPI Master Out Slave In
#define LORA_RST    12  // Reset
#define LORA_DIO1   14  // Digital I/O 1 (interrupt)
#define LORA_BUSY   13  // Busy status

// Relay Control Interface (application managed)
const int RELAY_PINS[8] = {36, 35, 34, 33, 47, 48, 26, 21};
// Control Logic: LOW = Relay ON, HIGH = Relay OFF (inverted)
```

#### Pin Assignment Rules
1. **LoRa pins**: Use library defaults, don't override unless necessary
2. **Relay pins**: Use GPIO pins that support OUTPUT mode reliably
3. **Avoid conflicts**: Don't use pins reserved for SPI or system functions
4. **Power considerations**: Ensure GPIO can handle relay board current requirements

### Communication Protocols

#### 1. LoRaWAN Downlink Format

**JSON Command Structure** (Primary):
```json
{
  "relay": 1-8,           // Required: Relay number
  "state": 0|1|"on"|"off", // Required: Relay state (flexible format)
  "duration": 3600        // Optional: Auto-off timer in seconds
}
```

**HEX Command Structure** (Legacy):
```
Byte 0: Command Type (0x01 = relay control)
Byte 1: Relay Data (bits 0-2: relay number 0-7, bit 7: state)
Byte 2: Duration in seconds (optional)
```

#### 2. LoRaWAN Uplink Format

**Status Packet Structure**:
```
Byte 0: Relay State Bitmap (bit 0 = relay 1, bit 1 = relay 2, etc.)
Byte 1: Reserved for future use
Port: 2 (confirmed transmission for reliability)
```

#### 3. Serial Debug Interface

**Command Format**:
```
relay,<number>,<state>[,<duration>] - Control specific relay
status                              - Show system status
send                               - Force status packet transmission
test_json                          - Run JSON parsing tests
test_hex                           - Run HEX parsing tests
```

### Memory Management

#### Static Memory Allocation
```cpp
// Relay state management (minimal overhead)
bool relayStates[8];           // 8 bytes
unsigned long relayTimers[8];  // 32 bytes
const int RELAY_PINS[8];       // 32 bytes (flash)

// LoRaWAN buffers (managed by LoraManager2)
// ~1KB for LoRaWAN stack and buffers
```

#### Dynamic Memory Guidelines
- **Avoid**: Dynamic allocation in interrupt contexts
- **JSON parsing**: Use JsonDocument for automatic sizing
- **String operations**: Minimize String class usage in critical paths
- **Buffer reuse**: Reuse static buffers where possible

### Error Handling Patterns

#### 1. Initialization Error Pattern
```cpp
if (!lora.begin(config, hwConfig)) {
    Serial.println("LoRa initialization failed");
    // Continue operation in degraded mode
    // Allow serial commands for debugging
}
```

#### 2. Command Validation Pattern
```cpp
// Always validate input ranges
if (relayNum < 0 || relayNum >= 8) {
    logError("Invalid relay number");
    return false;
}

// Provide meaningful error messages
if (!loraJoined) {
    logError("Cannot send: not joined to network");
    return false;
}
```

#### 3. Timeout Handling Pattern
```cpp
// Use non-blocking timeouts
unsigned long startTime = millis();
while (condition && (millis() - startTime < TIMEOUT_MS)) {
    yield(); // Allow other tasks
}
```

### Performance Specifications

#### Timing Requirements
- **Relay Response**: <100ms from downlink reception to relay activation
- **Join Timeout**: 30 seconds maximum per join attempt
- **Status Interval**: 5 minutes (configurable)
- **Watchdog Yield**: Every loop iteration (<10ms)

#### Resource Limits
- **RAM Usage**: <80KB total (leave margin for stack)
- **Flash Usage**: <1MB program space
- **Power Consumption**: ~150mA continuous (Class C receive)
- **GPIO Drive**: 12mA per pin maximum (ESP32-S3 limit)

### Security Considerations

#### LoRaWAN Security
- **AES-128**: All LoRaWAN messages encrypted
- **Device Authentication**: OTAA prevents replay attacks
- **Key Management**: Credentials stored in flash (consider encryption)
- **Network Security**: Use private TTN applications when possible

#### Physical Security
- **GPIO Protection**: Consider optical isolation for relay outputs
- **Power Security**: Implement brownout detection
- **Debug Access**: Disable debug output in production builds

### Testing Standards

#### Unit Testing Requirements
```cpp
// Test each command format
testJsonCommand("{\"relay\":1,\"state\":1}");
testJsonCommand("{\"relay\":2,\"state\":\"on\",\"duration\":60}");
testHexCommand({0x01, 0x80, 0x10});

// Test edge cases
testInvalidRelay(0);   // Below range
testInvalidRelay(9);   // Above range
testMalformedJson("{relay:1}"); // Missing quotes
```

#### Integration Testing Requirements
- **Join Process**: Verify successful OTAA join
- **Class C Reception**: Confirm continuous receive operation
- **Relay Control**: Validate all 8 relays function correctly
- **Timer Operation**: Test duration-based auto-off functionality
- **Error Recovery**: Test behavior after power cycle/reset

#### Performance Testing Requirements
- **Latency Measurement**: Downlink to relay activation time
- **Memory Monitoring**: Check for memory leaks over time
- **Range Testing**: Verify operation at expected distances
- **Stress Testing**: Multiple rapid commands, extended operation

### Deployment Configuration

#### Production Settings
```cpp
#define DEBUG_MODE false      // Disable verbose logging
#define STATUS_INTERVAL 300000 // 5 minutes
#define JOIN_TRIALS 5         // Reasonable retry count
#define TX_POWER 14          // Maximum allowed for region
```

#### Development Settings
```cpp
#define DEBUG_MODE true       // Enable detailed logging
#define STATUS_INTERVAL 60000 // 1 minute for testing
#define JOIN_TRIALS 10        // More retries for debugging
#define TX_POWER 10          // Lower power for lab testing
```

### Compliance and Certification

#### FCC Compliance (US915)
- **Maximum TX Power**: 30 dBm (1W) EIRP
- **Duty Cycle**: Managed by LoRaWAN stack
- **Frequency Hopping**: Required for US915 operation
- **Antenna Gain**: Must be included in EIRP calculation

#### LoRaWAN Certification
- **LoRa Alliance**: Device must pass certification testing
- **Regional Parameters**: Comply with local spectrum regulations
- **Interoperability**: Test with multiple network servers 