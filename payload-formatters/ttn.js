// Encoder function for downlinks
function encodeDownlink(input) {
    if (typeof input.data === 'object') {
      // Convert JSON to string and then to bytes
      const jsonString = JSON.stringify(input.data);
      const bytes = [];
      for (let i = 0; i < jsonString.length; i++) {
        bytes.push(jsonString.charCodeAt(i));
      }
      return {
        bytes: bytes,
        fPort: input.fPort || 2
      };
    } 
    // If input.data is already an array of bytes, use it directly
    else if (Array.isArray(input.data)) {
      return {
        bytes: input.data,
        fPort: input.fPort || 2
      };
    }
    // Default empty response
    return {
      bytes: [],
      fPort: input.fPort || 2
    };
  }
  
  // Decoder function for downlinks
  function decodeDownlink(input) {
    // If there are less than 4 bytes, it's likely a hex command
    if (input.bytes.length < 4) {
      return {
        data: {
          bytes: input.bytes,
          type: "command"
        }
      };
    }
    
    // Try to parse as JSON
    try {
      // Convert bytes to string
      let str = "";
      for (let i = 0; i < input.bytes.length; i++) {
        str += String.fromCharCode(input.bytes[i]);
      }
      
      // Parse as JSON
      const data = JSON.parse(str);
      
      return {
        data: data
      };
    } catch (err) {
      // If parsing fails, return raw bytes
      return {
        data: {
          bytes: input.bytes,
          type: "binary"
        }
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