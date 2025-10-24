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
CRGB idleColor = CRGB::Yellow;

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
CRGB sendColor = CRGB::Yellow;

String serialLine;
PingPongHandler PingPong;

bool PING_IDLE = false;
bool starIsMade = false;

unsigned long lastIdleAnimationTimestamp = 0;

HardwareSerial* MySerial = &Serial2;  // Change to prefered Serial port

// =============================================================
// SETUP
// =============================================================
void setup() {
    // MySerial->begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    MySerial->begin(9600);

    delay(1000);

    FastLED.addLeds<WS2811, PIN_SIDE_ARM, BGR>(sideArm, NUM_SIDE_ARM);
    FastLED.addLeds<WS2811, PIN_TOP_ARM, BGR>(topArm, NUM_TOP_ARM);
    FastLED.addLeds<WS2811, PIN_BOTTOM_ARM, BGR>(bottomArm, NUM_BOTTOM_ARM);
    FastLED.addLeds<WS2811, PIN_MIC_STAR, BGR>(micStar, NUM_MIC_STAR);

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
        micBrightness = parsedCmd.getNamed("brightness", "50").toInt();
        /**
         * Sends error message if brightness is out of range, but constrain already normalizes value even if its
         * higher than 255. Either send error and constrain for safety, or just constrain without error
         */
        if (micBrightness < 0 || micBrightness > 255) {
            cmdlib::Command errResp;
            errResp.addHeader("MASTER");
            errResp.msgKind = "ERROR";
            errResp.command = parsedCmd.command;
            errResp.setNamed("message", "BRIGHTNESS_OUT_OF_RANGE (0-255), received=" + micBrightness);
            MySerial->println(errResp.toString());
            return;
        }
        micBrightness = constrain(micBrightness, 0, 255);
        fill_solid(micStar, NUM_MIC_STAR, CRGB(micBrightness, micBrightness, 0));
        FastLED.show();
        sendConfirm("MAKE_STAR");
    } else if (parsedCmd.command == "UPDATE_STAR") {
        if (starIsMade == true) {
            micBrightness = parsedCmd.getNamed("brightness", String(micBrightness)).toInt();
            /**
             * Same issue here with the constrain
             */
            if (micBrightness < 0 || micBrightness > 255) {
                cmdlib::Command errResp;
                errResp.addHeader("MASTER");
                errResp.msgKind = "ERROR";
                errResp.command = parsedCmd.command;
                errResp.setNamed("message", "BRIGHTNESS_OUT_OF_RANGE (0-255), received=" + micBrightness);
                MySerial->println(errResp.toString());
                return;
            }
            micBrightness = constrain(micBrightness, 0, 255);
            fill_solid(micStar, NUM_MIC_STAR, CRGB(micBrightness, micBrightness, 0));
            FastLED.show();
            sendConfirm("UPDATE_STAR");
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

        sendBrightness = parsedCmd.getNamed("brightness", String(sendBrightness)).toInt();
        sendBrightness = constrain(sendBrightness, 0, 255);
        sendSize = parsedCmd.getNamed("size", String(sendSize)).toInt();
        sendSpeed = parsedCmd.getNamed("speed", String(sendSpeed)).toInt();

        if (sendSpeed < 1 || sendSpeed > 10) {
            cmdlib::Command errResp;
            errResp.addHeader("MASTER");
            errResp.msgKind = "ERROR";
            errResp.command = parsedCmd.command;
            errResp.setNamed("message", "SPEED_OUT_OF_RANGE (1-10), received=" + sendSpeed);
            MySerial->println(errResp.toString());
            return;
        }

        String colorStr = parsedCmd.getNamed("color", "yellow");
        sendColor = parseColor(colorStr, sendBrightness);

        // Mic dimmen
        for (int b = micBrightness; b >= 0; b--) {
            fill_solid(micStar, NUM_MIC_STAR, CRGB(b, b, 0));
            FastLED.show();
            delay(5);
        }
        micBrightness = 0;

        // ARM strips volledig dimmen
        nscale8_video(sideArm, NUM_SIDE_ARM, 0);
        nscale8_video(topArm, NUM_TOP_ARM, 0);
        nscale8_video(bottomArm, NUM_BOTTOM_ARM, 0);
        FastLED.show();
        delay(20);

        // Send star animatie
        int delayPerStep = map(sendSpeed, 1, 10, 40, 5);
        for (int i = NUM_SIDE_ARM - 1; i >= -sendSize; i--) {
            fill_solid(sideArm, NUM_SIDE_ARM, CRGB::Black);
            fill_solid(topArm, NUM_TOP_ARM, CRGB::Black);
            fill_solid(bottomArm, NUM_BOTTOM_ARM, CRGB::Black);

            for (int j = 0; j < sendSize; j++) {
                int pos = i + j;
                if (pos >= 0 && pos < NUM_SIDE_ARM) sideArm[pos] = sendColor;
                if (pos >= 0 && pos < NUM_TOP_ARM) topArm[pos] = sendColor;
                if (pos >= 0 && pos < NUM_BOTTOM_ARM) bottomArm[pos] = sendColor;
            }

            FastLED.show();
            delay(delayPerStep);
            yield();
        }

        // ARM strips resetten
        fill_solid(sideArm, NUM_SIDE_ARM, CRGB::Black);
        fill_solid(topArm, NUM_TOP_ARM, CRGB::Black);
        fill_solid(bottomArm, NUM_BOTTOM_ARM, CRGB::Black);
        FastLED.show();

        sendRequest("STAR_ARRIVED");
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
    if (now - lastIdleAnimationTimestamp > IDLE_ANIMATION_INTERVAL) {
        starIsMade = false;
        FastLED.show();

        int randomStarBrightness = random(50, 256);
        for (int b = 0; b < randomStarBrightness; b += 1) {
            fill_solid(micStar, NUM_MIC_STAR, CRGB(b, b, 0));
            FastLED.show();
            delay(17);
        }

        for (int b = randomStarBrightness; b >= 0; b -= 1) {
            fill_solid(micStar, NUM_MIC_STAR, CRGB(b, b, 0));
            FastLED.show();
            delay(17);
        }
        fill_solid(micStar, NUM_MIC_STAR, CRGB(0, 0, 0));

        // ARM strips volledig dimmen
        nscale8_video(sideArm, NUM_SIDE_ARM, 0);
        nscale8_video(topArm, NUM_TOP_ARM, 0);
        nscale8_video(bottomArm, NUM_BOTTOM_ARM, 0);
        FastLED.show();
        delay(20);

        // Send star animatie
        int delayPerStep = map(sendSpeed, 1, 10, 40, 5);
        for (int i = NUM_SIDE_ARM - 1; i >= -sendSize; i--) {
            fill_solid(sideArm, NUM_SIDE_ARM, CRGB::Black);
            fill_solid(topArm, NUM_TOP_ARM, CRGB::Black);
            fill_solid(bottomArm, NUM_BOTTOM_ARM, CRGB::Black);

            for (int j = 0; j < sendSize; j++) {
                int pos = i + j;
                if (pos >= 0 && pos < NUM_SIDE_ARM) sideArm[pos] = idleColor;
                if (pos >= 0 && pos < NUM_TOP_ARM) topArm[pos] = idleColor;
                if (pos >= 0 && pos < NUM_BOTTOM_ARM) bottomArm[pos] = idleColor;
            }

            FastLED.show();
            delay(delayPerStep);
            yield();
        }

        // ARM strips resetten
        fill_solid(sideArm, NUM_SIDE_ARM, CRGB::Black);
        fill_solid(topArm, NUM_TOP_ARM, CRGB::Black);
        fill_solid(bottomArm, NUM_BOTTOM_ARM, CRGB::Black);
        FastLED.show();

        lastIdleAnimationTimestamp = now;
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
    if (c == "yellow") return CRGB(val, val, 0);
    return CRGB(val, val, 0);  // fallback yellow
}
