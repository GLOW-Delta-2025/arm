# PingPongHandler

A lightweight keep-alive mechanism for Arduino that implements a bidirectional PING/PONG protocol to detect connection timeouts and idle states.

---

## Overview

`PingPongHandler` monitors the timing of PING requests and responses to track whether a connection is still active. If no PING is received within a configurable timeout window, the system marks itself as idle (`PING_IDLE = true`), allowing your application to detect and respond to connection loss.

---

## Features

- **Timeout detection**: Automatically tracks idle state based on elapsed time since last PING
- **Bidirectional PING/PONG**: Responds to incoming PING requests and can send PINGs to other devices
- **Global idle flag**: `PING_IDLE` boolean accessible from anywhere in your sketch
- **Configurable timeout**: Set custom idle timeout in milliseconds (default: 30 seconds)
- **Serial port abstraction**: Use any `Stream` object (Serial, Serial1, SoftwareSerial, etc.)
- **CmdLib integration**: Uses structured command format for reliability and extensibility

---

## Quick Start

### Setup

```cpp
#include "PingPong.h"

void setup() {
  Serial.begin(115200);
  
  // Initialize with 30-second timeout (default)
  PingPong.init();
  
  // Or with custom timeout and serial port:
  PingPong.init(45000, &Serial);
}

void loop() {
  // Call update() regularly to check for timeouts
  PingPong.update();
  
  // Read incoming commands and process them
  if (Serial.available()) {
    String incoming = Serial.readStringUntil('\n');
    PingPong.processRawCommand(incoming);
  }
  
  // Check idle status
  if (PingPong.isIdle()) {
    Serial.println("Connection idle - attempting recovery...");
  }
}
```

---

## API Reference

### Initialization

**`void init(unsigned long timeoutMs = 30000, Stream* serial = &Serial)`**

Initializes the PingPongHandler with optional timeout and serial port configuration.

- `timeoutMs` — Milliseconds of inactivity before marking as idle (default: 30000ms)
- `serial` — Pointer to the Stream object to use for output (default: &Serial)

**Example:**
```cpp
PingPong.init(60000, &Serial1);  // 60-second timeout on Serial1
```

---

### Command Processing

**`void processRawCommand(const String& cmdString)`**

Parses a command string and processes it if it's a PING request.

- `cmdString` — Raw command string (should follow CmdLib format: `!!HEADER:REQUEST:PING##`)

**Example:**
```cpp
String received = "!!MASTER:REQUEST:PING##";
PingPong.processRawCommand(received);
```

**`void processCommand(const cmdlib::Command& cmd)`**

Processes an already-parsed `Command` object. Checks for PING requests and responds automatically.

**Example:**
```cpp
cmdlib::Command cmd;
String err;
if (cmdlib::parse(incomingString, cmd, err)) {
  PingPong.processCommand(cmd);
}
```

---

### Polling & Status

**`void update()`**

Call this regularly in your main loop to check if the idle timeout has been exceeded. Should be called as frequently as possible (ideally every loop iteration).

**Example:**
```cpp
void loop() {
  PingPong.update();  // Call every iteration
  // ... rest of loop
}
```

**`bool isIdle() const`**

Returns the current idle status.

- **Returns**: `true` if no PING has been received within the timeout window; `false` otherwise

**Example:**
```cpp
if (PingPong.isIdle()) {
  handleConnectionLoss();
}
```

---

### Sending PINGs

**`void sendPing(const String& to)`**

Sends a PING request to a specific recipient.

- `to` — Header/destination address (e.g., "MASTER", "ARM#1")

**Example:**
```cpp
PingPong.sendPing("MASTER");  // Sends: !!MASTER:REQUEST:PING##
```

---

### Serial Port Management

**`Stream* getSerial() const`**

Returns the currently configured serial port.

**`void setSerial(Stream* serial)`**

Changes the serial port at runtime.

**Example:**
```cpp
PingPong.setSerial(&Serial2);  // Switch to Serial2
```

---

## Global Idle Flag

The module exposes a global `bool PING_IDLE` that tracks connection state:

```cpp
extern bool PING_IDLE;
```

Access from any file in your project without including PingPong.h:

```cpp
extern bool PING_IDLE;

void setup() {
  // ... other setup
}

void loop() {
  if (PING_IDLE) {
    // Handle disconnection
  }
}
```

---

## How It Works

1. **Initialization**: `init()` records the current time as `lastPingTime` and sets `PING_IDLE = false`
2. **Receiving PING**: When a `REQUEST:PING` command arrives:
   - `lastPingTime` is updated to current time
   - `PING_IDLE` is set to `false`
   - A `CONFIRM:PING` response is sent back to the sender
3. **Update & Timeout**: Each call to `update()` checks if `(now - lastPingTime) > idleTimeoutMs`:
   - If yes: `PING_IDLE = true` (connection idle)
   - If no: `PING_IDLE` remains as-is
4. **Polling**: Your application polls `isIdle()` or checks `PING_IDLE` to detect disconnections

---

## Command Format (CmdLib)

PING and PONG commands follow the CmdLib protocol:

**Incoming PING (request):**
```
!!SOURCE:REQUEST:PING##
```

**Outgoing PONG (response):**
```
!!DESTINATION:CONFIRM:PING##
```

For more details on the command format, see the [CmdLib README](../CommandLibary/README.md).

---

## Error Handling

If `init()` has not been called, the handler silently ignores all commands and update calls. Always ensure `init()` is called before using the handler.

```cpp
void setup() {
  Serial.begin(115200);
  PingPong.init();  // Must be called first
}
```

---

## Example: Complete Keep-Alive Sketch

```cpp
#include "PingPong.h"

unsigned long lastPingCheck = 0;

void setup() {
  Serial.begin(115200);
  PingPong.init(45000);  // 45-second idle timeout
  Serial.println("Keep-alive initialized");
}

void loop() {
  // Update timeout checking
  PingPong.update();
  
  // Handle incoming serial data
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    PingPong.processRawCommand(cmd);
  }
  
  // Periodically check and report idle status
  if (millis() - lastPingCheck > 5000) {
    lastPingCheck = millis();
    if (PingPong.isIdle()) {
      Serial.println("WARNING: Connection idle");
    } else {
      Serial.println("Connection active");
    }
  }
}
```

---

## See Also

- [CmdLib README](../CommandLibary/README.md) — Command parsing and building
