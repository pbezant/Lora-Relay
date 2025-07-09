// Encoder function for downlinks
function encodeDownlink(input) {
  let data = input.data;
  
  // Check if this is a multi-relay command with 'relays' array
  if (data && data.relays && Array.isArray(data.relays)) {
    // Convert multi-relay JSON to compact binary format
    const relays = data.relays;
    const bytes = [];
    
    // Magic byte to indicate multi-relay binary command
    bytes.push(0xFF);
    
    // Relay count
    bytes.push(relays.length);
    
    // Encode each relay (4 bytes per relay)
    for (const relay of relays) {
      bytes.push((relay.relay || 1) - 1);     // Convert 1-based to 0-based indexing (1-8 → 0-7)
      bytes.push(relay.state ? 1 : 0);        // State (0/1)
      
      const duration = relay.duration || 0;
      bytes.push(duration & 0xFF);            // Duration low byte
      bytes.push((duration >> 8) & 0xFF);     // Duration high byte
    }
    
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }
  
  // Single object or other format - use JSON
  if (typeof data === 'object' && data !== null) {
    const jsonString = JSON.stringify(data);
    const bytes = [];
    for (let i = 0; i < jsonString.length; i++) {
      bytes.push(jsonString.charCodeAt(i));
    }
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }
  
  // Default empty response
  return {
    bytes: [],
    fPort: input.fPort || 1
  };
}

// Decoder function for downlinks
function decodeDownlink(input) {
  // Check if this is a multi-relay binary command (starts with 0xFF)
  if (input.bytes.length >= 2 && input.bytes[0] === 0xFF) {
    const relayCount = input.bytes[1];
    const expectedLength = 2 + (relayCount * 4);
    
    if (input.bytes.length === expectedLength) {
      const relays = [];
      
      for (let i = 0; i < relayCount; i++) {
        const offset = 2 + (i * 4);
        const relay = input.bytes[offset];
        const state = input.bytes[offset + 1];
        const durationLow = input.bytes[offset + 2];
        const durationHigh = input.bytes[offset + 3];
        const duration = durationLow + (durationHigh << 8);
        
        relays.push({
          relay: relay + 1,  // Convert back to 1-based for display
          state: state,
          duration: duration
        });
      }
      
      return {
        data: { relays: relays },
        info: `Multi-relay binary command: ${relayCount} relays, ${input.bytes.length} bytes`
      };
    } else {
      return {
        data: { bytes: input.bytes, type: "invalid_binary" },
        info: `Invalid multi-relay binary format: expected ${expectedLength} bytes, got ${input.bytes.length}`
      };
    }
  }
  
  // Try to parse as JSON (single-relay or legacy)
  try {
    let str = "";
    for (let i = 0; i < input.bytes.length; i++) {
      str += String.fromCharCode(input.bytes[i]);
    }
    const data = JSON.parse(str);
    return {
      data: data,
      info: 'Single-relay JSON command'
    };
  } catch (err) {
    // If parsing fails, return raw bytes
    return {
      data: {
        bytes: input.bytes,
        type: "binary"
      },
      info: `Failed to parse: ${err.message}`
    };
  }
}
  
  // Decoder function for uplinks
  function decodeUplink(input) {
    // Convert bytes to hex string
    let hexStr = "";
    for (let i = 0; i < input.bytes.length; i++) {
      const hex = input.bytes[i].toString(16).padStart(2, '0');
      hexStr += hex;
    }
    
    return {
      data: {
        hex: hexStr
      }
    };
  }