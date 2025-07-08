# System Architecture

## High-Level Architecture

```
┌─────────────────┐    LoRaWAN     ┌─────────────────┐
│                 │◄──────────────►│                 │
│  Heltec ESP32   │                │ The Things      │
│  LoRa V3        │                │ Network (TTN)   │
│                 │                │                 │
└─────────┬───────┘                └─────────────────┘
          │ GPIO                             ▲
          │ Control                          │ Internet
          ▼                                  ▼
┌─────────────────┐                ┌─────────────────┐
│                 │                │                 │
│   8-Channel     │                │   User/Client   │
│   Relay Board   │                │   Application   │
│                 │                │                 │
└─────────────────┘                └─────────────────┘
```

## Component Relationships

### Core Components

#### 1. LoRaWAN Communication Layer
**Component**: LoraManager2 + RadioLib
- **Responsibility**: LoRaWAN protocol implementation, Class C operation
- **Interfaces**: 
  - SPI to SX1262 radio chip
  - Callback interface to application layer
- **Dependencies**: RadioLib for low-level radio operations

#### 2. Application Control Layer
**Component**: main.cpp application logic
- **Responsibility**: Command processing, relay control, state management
- **Interfaces**:
  - Downlink callback from LoraManager2
  - GPIO control to relay board
  - Serial interface for debugging/testing
- **Dependencies**: LoraManager2, ArduinoJson

#### 3. Relay Control Layer
**Component**: GPIO control subsystem
- **Responsibility**: Physical relay switching, timer management
- **Interfaces**: Digital GPIO pins to relay board
- **Dependencies**: ESP32 GPIO HAL

#### 4. Command Processing Layer
**Component**: JSON/HEX parsers
- **Responsibility**: Parse and validate incoming commands
- **Interfaces**: String/byte array input, structured command output
- **Dependencies**: ArduinoJson library

### Data Flow

#### Downlink Command Flow
```
TTN Downlink → LoraManager2 → processDownlink() → 
JSON Parser → setRelay() → GPIO Control → Physical Relay
```

#### Uplink Status Flow
```
Relay States → sendStatusPacket() → LoraManager2 → 
LoRaWAN Network → TTN → User Application
```

#### Serial Command Flow
```
Serial Input → processSerialCommand() → setRelay() → 
GPIO Control → Physical Relay
```

## Module Boundaries

### LoRaWAN Module
- **Encapsulates**: LoRaWAN protocol, radio management, network joining
- **Exposes**: Simple send/receive API, connection status
- **Isolation**: Hardware-specific radio operations hidden from application

### Relay Control Module
- **Encapsulates**: GPIO operations, timer management, safety logic
- **Exposes**: High-level relay control API (setRelay, checkTimers)
- **Isolation**: Hardware pin assignments abstracted

### Command Processing Module
- **Encapsulates**: Protocol parsing, validation logic
- **Exposes**: Structured command objects
- **Isolation**: Wire protocol details hidden from control logic

## System Interfaces

### External Interfaces

#### LoRaWAN Network Interface
- **Protocol**: LoRaWAN 1.0.x / 1.1
- **Class**: C (continuous receive)
- **Frequency**: US915, Subband 2
- **Security**: AES-128 encryption, device authentication

#### Relay Board Interface
- **Protocol**: Digital GPIO (3.3V logic)
- **Pins**: 8 output pins (configurable)
- **Logic**: Inverted (LOW=activate, HIGH=deactivate)
- **Isolation**: Recommended optical isolation for safety

#### Serial Debug Interface
- **Protocol**: UART, 115200 baud
- **Purpose**: Development, testing, diagnostics
- **Commands**: Human-readable text commands

### Internal Interfaces

#### LoraManager2 Callback Interface
```cpp
void processDownlink(uint8_t* payload, size_t size, uint8_t port);
```

#### Relay Control Interface
```cpp
void setRelay(uint8_t relayNum, bool state, unsigned long duration = 0);
void checkRelayTimers();
```

#### Command Processing Interface
```cpp
void processJsonCommand(String jsonString);
void processHexCommand(uint8_t* payload, size_t size);
```

## Dependency Management

### Compile-Time Dependencies
- LoraManager2 (GitHub)
- RadioLib (registry)
- ArduinoJson (registry)
- ESP32 Arduino Core
- Heltec board support

### Runtime Dependencies
- TTN network infrastructure
- LoRaWAN gateway coverage
- External power supply for relays

## Scalability Considerations

### Horizontal Scaling
- Multiple devices can operate independently
- Each device has unique DevEUI/credentials
- TTN application can manage fleet of devices

### Vertical Scaling
- GPIO expansion possible with I2C/SPI expanders
- Additional sensor inputs could be added
- Display interface available but currently disabled

### Performance Characteristics
- **Class C Latency**: ~1-2 seconds typical
- **Power Consumption**: ~150mA continuous (Class C)
- **Memory Usage**: ~50KB program, ~10KB RAM
- **Relay Switching**: <10ms response time 