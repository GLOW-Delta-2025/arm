
// PingPong.h
#ifndef PING_PONG_H
#define PING_PONG_H

#include <Arduino.h>
#include "CmdLib.h"

// Global IDLE flag that can be checked from anywhere
extern bool PING_IDLE;

class PingPongHandler {
private:
  unsigned long lastPingTime;
  unsigned long idleTimeoutMs;
  bool initialized;
  Stream* serialPort; // Reference to the serial port to use

public:
  // Default constructor
  PingPongHandler() : initialized(false), idleTimeoutMs(30000), serialPort(&Serial) {
    lastPingTime = millis();
  }

  // Initialize with device ID and timeout
  void init(unsigned long timeoutMs = 30000, Stream* serial = &Serial) {
    idleTimeoutMs = timeoutMs;
    serialPort = serial;
    lastPingTime = millis();
    initialized = true;
    PING_IDLE = false;
  }
  
  // Process a raw command string
  void processRawCommand(const String& cmdString) {
    if (!initialized) return;
    
    cmdlib::Command cmd;
    String error;
    
    if (cmdlib::parse(cmdString, cmd, error)) {
      processCommand(cmd);
    }
  }
  
  // Process a parsed command
  void processCommand(const cmdlib::Command& cmd) {
    if (!initialized) return;
    
    // Check if this is a PING request
    if (cmd.msgKind == "REQUEST" && cmd.command == "PING") {
      lastPingTime = millis();
      PING_IDLE = false;
      
      cmdlib::Command response;
      // Send's back to who requested the PING
      response.addHeader(cmd.getHeader(0));
      response.msgKind = "CONFIRM";
      response.command = "PING";
      
      serialPort->println(response.toString());
    }
  }
  
  // Update the idle status (call this regularly)
  void update() {
    if (!initialized) return;
    
    unsigned long now = millis();
    
    // Check if we've exceeded the idle timeout
    if (now - lastPingTime > idleTimeoutMs) {
      PING_IDLE = true;
    }
  }
  
  // Get current idle status
  bool isIdle() const {
    return PING_IDLE;
  }
  
  // Force a ping response to a specific recipient
  void sendPing(const String& to) {
    if (!initialized) return;
    
    cmdlib::Command ping;
    ping.addHeader(to);      // TO
    ping.msgKind = "REQUEST";
    ping.command = "PING";
    
    serialPort->println(ping.toString());
  }
  
  // Get the current serial port
  Stream* getSerial() const {
    return serialPort;
  }
  
  // Set the serial port
  void setSerial(Stream* serial) {
    serialPort = serial;
  }
};

extern PingPongHandler PingPong;

#endif // PING_PONG_H
