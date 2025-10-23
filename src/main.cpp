#include <Arduino.h>
#include <FastLED.h>

#include "CmdLib.h"
#include "PingPong.h"

// =============================================================
// FUNCTIES
// =============================================================
void sendConfirm(const char* cmdName);
CRGB parseColor(String c, int val);

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

#define RX_PIN 16
#define TX_PIN 17

// =============================================================
// VARIABELEN
// =============================================================
int micBrightness = 0;
int sendBrightness = 0;
int sendSize = 8;
int sendSpeed = 3;
CRGB sendColor = CRGB::Yellow;

String serialLine;

void readSerial(void);
void parseCommand(String line);

PingPongHandler PingPong;
bool PING_IDLE = true;

bool starIsMade = false;

// =============================================================
// SETUP
// =============================================================
void setup() {
    Serial.begin(9600);
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    delay(1000);

    FastLED.addLeds<WS2811, PIN_SIDE_ARM, RGB>(sideArm, NUM_SIDE_ARM);
    FastLED.addLeds<WS2811, PIN_TOP_ARM, RGB>(topArm, NUM_TOP_ARM);
    FastLED.addLeds<WS2811, PIN_BOTTOM_ARM, RGB>(bottomArm, NUM_BOTTOM_ARM);
    FastLED.addLeds<WS2811, PIN_MIC_STAR, BGR>(micStar, NUM_MIC_STAR);

    FastLED.clear();
    FastLED.show();

    PingPong.init(45000, &Serial2);  // 45s timeout
    Serial2.println("ESP Ready: ARM + MIC STAR (FastLED + CmdLib active)");
}

// =============================================================
// MAIN LOOP
// =============================================================
void loop() {
    readSerial();
    // PingPong.update();  // handle PING/PONG idle detection

    // if (PING_IDLE) {  // optional reaction if idle
    //   Serial.println("!!WARN{message=CONNECTION_IDLE}##");
    // }
}

// =============================================================
// SERIAL PARSER
// =============================================================
void readSerial() {
    while (Serial2.available()) {
        char c = Serial2.read();
        serialLine += c;
        Serial.println(serialLine);  // echo naar hoofdserial
        if (serialLine.endsWith("##")) {
            PingPong.processRawCommand(serialLine);
            parseCommand(serialLine);
            serialLine = "";
        }
    }
}

void parseCommand(String line) {
    String err;
    cmdlib::Command parsedCmd;

    if (!cmdlib::parse(line.c_str(), parsedCmd, err)) {
        Serial2.print("!!ERROR{message=");
        Serial2.print(err);
        Serial2.println("}##");
        return;
    }

    if (parsedCmd.msgKind != "REQUEST") {
        Serial2.println("!!ERROR{message=Invalid message kind}##");
        return;
    }

    if (parsedCmd.command == "MAKE_STAR" && starIsMade == false) {
        starIsMade = true;
        micBrightness = parsedCmd.getNamed("brightness", "50").toInt();
        micBrightness = constrain(micBrightness, 0, 255);
        fill_solid(micStar, NUM_MIC_STAR, CRGB(micBrightness, micBrightness, 0));
        FastLED.show();
        sendConfirm("MAKE_STAR");
    } else if (parsedCmd.command == "UPDATE_STAR" && starIsMade == true) {
        micBrightness = parsedCmd.getNamed("brightness", String(micBrightness)).toInt();
        micBrightness = constrain(micBrightness, 0, 255);
        fill_solid(micStar, NUM_MIC_STAR, CRGB(micBrightness, micBrightness, 0));
        FastLED.show();
        sendConfirm("UPDATE_STAR");
    } else if (parsedCmd.command == "SEND_STAR") {
        starIsMade = false;
        // Direct confirm sturen
        sendConfirm("SEND_STAR");

        sendBrightness = parsedCmd.getNamed("brightness", String(sendBrightness)).toInt();
        sendBrightness = constrain(sendBrightness, 0, 255);
        sendSize = parsedCmd.getNamed("size", String(sendSize)).toInt();
        sendSpeed = parsedCmd.getNamed("speed", String(sendSpeed)).toInt();

        if (sendSpeed < 1 || sendSpeed > 10) {
            Serial2.print("!!ERROR{message=SPEED_OUT_OF_RANGE (1-10), received=");
            Serial2.print(sendSpeed);
            Serial2.println("}##");
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
    } else {
        Serial2.print("!!ERROR{message=Unknown command: ");
        Serial2.print(parsedCmd.command);
        Serial2.println("}##");
    }
}

// =============================================================
// HELPERS
// =============================================================
void sendConfirm(const char* cmdName) {
    cmdlib::Command confirm;
    confirm.msgKind = "MASTER:CONFIRM";
    confirm.command = String(cmdName).c_str();
    Serial2.println(confirm.toString());
}

// =============================================================
// KLEUR PARSER (PIN 18 blijft geel)
// =============================================================
CRGB parseColor(String c, int val) {
    c.toLowerCase();
    if (c == "blue") return CRGB(val, 0, 0);
    if (c == "red") return CRGB(0, val, 0);
    if (c == "green") return CRGB(0, 0, val);
    if (c == "white") return CRGB(val, val, val);
    if (c == "yellow") return CRGB(0, val, val);
    return CRGB(0, val, val);  // fallback geel
}