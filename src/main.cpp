#include <Arduino.h>
#include <FastLED.h>

#include "CmdLib.h"
#include "PingPong.h"

// =============================================================
// FUNCTIES
// =============================================================
void sendConfirm(const char* cmdName);
void sendRequest(const char* cmdName);
CRGB parseColor(String c, int val);
void readSerial(void);
void parseCommand(String line);
void handleIdleAnimation(void);
void updateAnimation(void);

// =============================================================
// PIN CONFIGURATIE & LED-STRIPS
// =============================================================
#define PIN_SIDE_ARM 19
#define PIN_TOP_ARM 21
#define PIN_BOTTOM_ARM 22
#define PIN_MIC_STAR 18

#define NUM_SIDE_ARM 200
#define NUM_TOP_ARM 120
#define NUM_BOTTOM_ARM 150
#define NUM_MIC_STAR 200

CRGB sideArm[NUM_SIDE_ARM];
CRGB topArm[NUM_TOP_ARM];
CRGB bottomArm[NUM_BOTTOM_ARM];
CRGB micStar[NUM_MIC_STAR];

uint8_t STAR_R = 255;
uint8_t STAR_G = 191;
uint8_t STAR_B = 3;

CRGB idleColor = CRGB(STAR_R, STAR_G, STAR_B);

#define RX_PIN 16
#define TX_PIN 17

#define IDLE_ANIMATION_INTERVAL 10000  // 10 seconds for testing, can be adjusted
#define PING_PONG_TIMEOUT_MS 45000
// =============================================================
// VARIABELEN
// =============================================================
int micBrightness = 0;
int sendBrightness = 0;
int sendSize = 8;
int sendSpeed = 3;
CRGB sendColor = CRGB(STAR_R, STAR_G, STAR_B);

String serialLine;

bool starIsMade = false;

unsigned long lastIdleAnimationTimestamp = 0;

HardwareSerial* MySerial = &Serial2;  // Change to prefered Serial port

// Animation state machine
enum AnimState {
  ANIM_IDLE,
  FADE_MIC,
  DELAY_AFTER_FADE,
  ANIM_ARM
};
AnimState currentState = ANIM_IDLE;

unsigned long lastAnimUpdate = 0;
int currentDelay = 0;

int fadeCurrent = 0;
int fadeTarget = 0;
int fadeStep = 0;
int fadeInterval = 0;

enum NextFadeAction {
  NO_NEXT,
  FADE_TO_DOWN,
  FADE_TO_DELAY
};
NextFadeAction nextFadeAction = NO_NEXT;

int armPos = 0;
int armEndPos = 0;
int armSize = 0;
int armInterval = 0;
CRGB armColor;
bool sendArrivedNeeded = false;

// =============================================================
// SETUP
// =============================================================
void setup() {
    // MySerial->begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    MySerial->begin(115200);

    delay(1000);

    FastLED.addLeds<WS2811, PIN_SIDE_ARM, BRG>(sideArm, NUM_SIDE_ARM);
    FastLED.addLeds<WS2811, PIN_TOP_ARM, BRG>(topArm, NUM_TOP_ARM);
    FastLED.addLeds<WS2811, PIN_BOTTOM_ARM, BRG>(bottomArm, NUM_BOTTOM_ARM);
    FastLED.addLeds<WS2811, PIN_MIC_STAR, BRG>(micStar, NUM_MIC_STAR);

    FastLED.clear();
    FastLED.show();

    PingPong.init(PING_PONG_TIMEOUT_MS, MySerial);
    MySerial->println("ESP Ready: ARM + MIC STAR (FastLED + CmdLib active)");
}

// =============================================================
// MAIN LOOP
// =============================================================
void loop() {
    PingPong.update();  // handle PING/PONG idle detection
    readSerial();
    updateAnimation();

    if (PING_IDLE) {  // optional reaction if idle
        cmdlib::Command errResp;
        errResp.addHeader("MASTER");
        errResp.msgKind = "ERROR";
        errResp.command = "PING_IDLE";
        MySerial->println(errResp.toString());
        handleIdleAnimation();
    }
}

// =============================================================
// SERIAL PARSER
// =============================================================
void readSerial() {
    while (MySerial->available()) {
        char c = MySerial->read();
        serialLine += c;
        if (serialLine.endsWith("##")) {
            parseCommand(serialLine);
            // MySerial->println(serialLine);  // echo naar hoofdserial
            serialLine = "";
        }
    }
}

void parseCommand(String line) {
    String err;
    cmdlib::Command parsedCmd;

    if (!cmdlib::parse(line, parsedCmd, err)) {
        cmdlib::Command errResp;
        errResp.addHeader("MASTER");
        errResp.msgKind = "ERROR";
        errResp.command = parsedCmd.command;
        errResp.setNamed("message", err);
        MySerial->println(errResp.toString());
        return;
    }
    if (parsedCmd.command == "PING") {
        PingPong.processCommand(parsedCmd);
        return;
    }

    if (parsedCmd.msgKind != "REQUEST") {
        cmdlib::Command errResp;
        errResp.addHeader("MASTER");
        errResp.msgKind = "ERROR";
        errResp.command = parsedCmd.command;
        errResp.setNamed("message", "Invalid message kind");
        MySerial->println(errResp.toString());
        return;
    }

    /**
     * Should sending a star away reset the starIsMade flag?
     */
    if (parsedCmd.command == "MAKE_STAR") {
        starIsMade = true;
        micBrightness = parsedCmd.getNamed("BRIGHTNESS", "50").toInt();
        if (micBrightness < 0 || micBrightness > 255) {
            cmdlib::Command errResp;
            errResp.addHeader("MASTER");
            errResp.msgKind = "ERROR";
            errResp.command = parsedCmd.command;
            errResp.setNamed("message", "BRIGHTNESS_OUT_OF_RANGE (0-255), received=" + micBrightness);
            MySerial->println(errResp.toString());
            return;
        }
        sendConfirm("MAKE_STAR");
        FastLED.setBrightness(micBrightness);
        fill_solid(micStar, NUM_MIC_STAR, idleColor);
        FastLED.show();
    } else if (parsedCmd.command == "UPDATE_STAR") {
        if (starIsMade == true) {
            micBrightness = parsedCmd.getNamed("BRIGHTNESS", String(micBrightness)).toInt();
            if (micBrightness < 0 || micBrightness > 255) {
                cmdlib::Command errResp;
                errResp.addHeader("MASTER");
                errResp.msgKind = "ERROR";
                errResp.command = parsedCmd.command;
                errResp.setNamed("message", "BRIGHTNESS_OUT_OF_RANGE (0-255), received=" + micBrightness);
                MySerial->println(errResp.toString());
                return;
            }
            sendConfirm("UPDATE_STAR");
            FastLED.setBrightness(micBrightness);
            fill_solid(micStar, NUM_MIC_STAR, idleColor);
            FastLED.show();

        } else {
            cmdlib::Command errResp;
            errResp.addHeader("MASTER");
            errResp.msgKind = "ERROR";
            errResp.command = parsedCmd.command;
            errResp.setNamed("message", "STAR_NOT_MADE_YET");
            MySerial->println(errResp.toString());
            return;
        }
    } else if (parsedCmd.command == "SEND_STAR") {
        starIsMade = false;
        // Direct confirm sturen
        sendConfirm("SEND_STAR");

        sendBrightness = parsedCmd.getNamed("BRIGHTNESS", String(sendBrightness)).toInt();
        sendBrightness = constrain(sendBrightness, 0, 255);
        sendSize = parsedCmd.getNamed("SIZE", String(sendSize)).toInt();
        sendSpeed = parsedCmd.getNamed("SPEED", String(sendSpeed)).toInt();

        if (sendSpeed < 1 || sendSpeed > 10) {
            cmdlib::Command errResp;
            errResp.addHeader("MASTER");
            errResp.msgKind = "ERROR";
            errResp.command = parsedCmd.command;
            errResp.setNamed("message", "SPEED_OUT_OF_RANGE (1-10), received=" + sendSpeed);
            MySerial->println(errResp.toString());
            return;
        }

        String colorStr = parsedCmd.getNamed("COLOR", "yellow");
        sendColor = parseColor(colorStr, sendBrightness);

        // Start non-blocking animation
        fadeCurrent = micBrightness;
        fadeTarget = 0;
        fadeStep = -1;
        fadeInterval = 5;
        nextFadeAction = FADE_TO_DELAY;
        armColor = sendColor;
        armSize = sendSize;
        armInterval = map(sendSpeed, 1, 10, 40, 5);
        sendArrivedNeeded = true;

        // Perform first fade step immediately
        fill_solid(micStar, NUM_MIC_STAR, CRGB(fadeCurrent, fadeCurrent, 0));
        FastLED.show();
        fadeCurrent += fadeStep;
        currentDelay = fadeInterval;
        lastAnimUpdate = millis();
        currentState = FADE_MIC;
    } else {
        cmdlib::Command errResp;
        errResp.addHeader("MASTER");
        errResp.msgKind = "ERROR";
        errResp.command = parsedCmd.command;
        errResp.setNamed("message", "Unknown command: " + parsedCmd.command);
        MySerial->println(errResp.toString());
    }
}

// =============================================================
// IDLE ANIMATIE
// =============================================================

void handleIdleAnimation() {
    unsigned long now = millis();
    if (now - lastIdleAnimationTimestamp > IDLE_ANIMATION_INTERVAL && currentState == ANIM_IDLE) {
        starIsMade = false;

        int randomStarBrightness = random(50, 256);
        fadeCurrent = 0;
        fadeTarget = randomStarBrightness;
        fadeStep = 1;
        fadeInterval = 17;
        nextFadeAction = FADE_TO_DOWN;
        armColor = idleColor;
        armSize = sendSize;
        armInterval = map(sendSpeed, 1, 10, 40, 5);
        sendArrivedNeeded = false;

        // Perform first fade step immediately
        fill_solid(micStar, NUM_MIC_STAR, CRGB(fadeCurrent, fadeCurrent, 0));
        FastLED.show();
        fadeCurrent += fadeStep;
        currentDelay = fadeInterval;
        lastAnimUpdate = millis();
        currentState = FADE_MIC;

        lastIdleAnimationTimestamp = now;
    }
}

void updateAnimation() {
    if (currentState == ANIM_IDLE) return;
    if (millis() - lastAnimUpdate < (unsigned long)currentDelay) return;

    lastAnimUpdate = millis();

    switch (currentState) {
        case FADE_MIC: {
            fill_solid(micStar, NUM_MIC_STAR, CRGB(fadeCurrent, fadeCurrent, 0));
            FastLED.show();
            fadeCurrent += fadeStep;
            bool done = (fadeStep > 0 && fadeCurrent > fadeTarget) || (fadeStep < 0 && fadeCurrent < fadeTarget);
            if (done) {
                fadeCurrent = fadeTarget;
                if (nextFadeAction == FADE_TO_DOWN) {
                    fadeTarget = 0;
                    fadeStep = -1;
                    fadeInterval = 17;
                    nextFadeAction = FADE_TO_DELAY;
                } else if (nextFadeAction == FADE_TO_DELAY) {
                    nscale8_video(sideArm, NUM_SIDE_ARM, 0);
                    nscale8_video(topArm, NUM_TOP_ARM, 0);
                    nscale8_video(bottomArm, NUM_BOTTOM_ARM, 0);
                    FastLED.show();
                    micBrightness = 0;
                    currentDelay = 20;
                    currentState = DELAY_AFTER_FADE;
                }
            } else {
                currentDelay = fadeInterval;
            }
            break;
        }
        case DELAY_AFTER_FADE: {
            armPos = NUM_SIDE_ARM - 1;
            armEndPos = -armSize;
            // Perform first arm step immediately
            fill_solid(sideArm, NUM_SIDE_ARM, CRGB::Black);
            fill_solid(topArm, NUM_TOP_ARM, CRGB::Black);
            fill_solid(bottomArm, NUM_BOTTOM_ARM, CRGB::Black);

            for (int j = 0; j < armSize; j++) {
                int pos = armPos + j;
                if (pos >= 0 && pos < NUM_SIDE_ARM) sideArm[pos] = armColor;
                if (pos >= 0 && pos < NUM_TOP_ARM) topArm[pos] = armColor;
                if (pos >= 0 && pos < NUM_BOTTOM_ARM) bottomArm[pos] = armColor;
            }

            FastLED.show();
            armPos--;
            if (armPos < armEndPos) {
                fill_solid(sideArm, NUM_SIDE_ARM, CRGB::Black);
                fill_solid(topArm, NUM_TOP_ARM, CRGB::Black);
                fill_solid(bottomArm, NUM_BOTTOM_ARM, CRGB::Black);
                FastLED.show();
                if (sendArrivedNeeded) {
                    sendRequest("STAR_ARRIVED");
                }
                currentState = ANIM_IDLE;
            } else {
                currentState = ANIM_ARM;
                currentDelay = armInterval;
            }
            break;
        }
        case ANIM_ARM: {
            fill_solid(sideArm, NUM_SIDE_ARM, CRGB::Black);
            fill_solid(topArm, NUM_TOP_ARM, CRGB::Black);
            fill_solid(bottomArm, NUM_BOTTOM_ARM, CRGB::Black);

            for (int j = 0; j < armSize; j++) {
                int pos = armPos + j;
                if (pos >= 0 && pos < NUM_SIDE_ARM) sideArm[pos] = armColor;
                if (pos >= 0 && pos < NUM_TOP_ARM) topArm[pos] = armColor;
                if (pos >= 0 && pos < NUM_BOTTOM_ARM) bottomArm[pos] = armColor;
            }

            FastLED.show();
            armPos--;
            if (armPos < armEndPos) {
                fill_solid(sideArm, NUM_SIDE_ARM, CRGB::Black);
                fill_solid(topArm, NUM_TOP_ARM, CRGB::Black);
                fill_solid(bottomArm, NUM_BOTTOM_ARM, CRGB::Black);
                FastLED.show();
                if (sendArrivedNeeded) {
                    sendRequest("STAR_ARRIVED");
                }
                currentState = ANIM_IDLE;
            } else {
                currentDelay = armInterval;
            }
            break;
        }
        default:
            break;
    }
}

// =============================================================
// HELPERS
// =============================================================
void sendConfirm(const char* cmdName) {
    cmdlib::Command confirm;
    confirm.msgKind = "MASTER:CONFIRM";
    confirm.command = String(cmdName);
    MySerial->println(confirm.toString());
}

void sendRequest(const char* cmdName) {
    cmdlib::Command request;
    request.msgKind = "MASTER:REQUEST";
    request.command = String(cmdName);
    MySerial->println(request.toString());
}

// =============================================================
// KLEUR PARSER (PIN 18 blijft geel)
// =============================================================
CRGB parseColor(String c, int val) {
    c.toLowerCase();
    if (c == "blue") return CRGB(0, 0, val);   // B
    if (c == "green") return CRGB(val, 0, 0);  // G
    if (c == "red") return CRGB(0, val, 0);    // R
    if (c == "white") return CRGB(val, val, val);
    if (c == "yellow") CRGB(STAR_R, STAR_G, STAR_B);
    return CRGB(val, val, 0);  // fallback yellow
}
