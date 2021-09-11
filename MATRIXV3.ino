// -*-c++-*-
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>



//int outData =   13; // D7
//int outClock =  13; // D5
//int outCommit = 0; // D3

int outData =   2; // D4
int outClock =  5; // D1
int outCommit = 4; // D2


int outLed = 13;

WiFiUDP Udp;
unsigned port = 4444;
char buf[256] = {0};

#define DISPLAYS 4
#define LINES    8
#define BYTES (DISPLAYS * LINES)

#define CHARACTERS 8

byte scratch[BYTES] = {0};
byte current[BYTES] = {0};

void setup() {
    Serial.begin(115200);

    pinMode(outData,   OUTPUT); // MAX7219 DIN
    pinMode(outClock,  OUTPUT); // MAX7219 CLK
    pinMode(outCommit, OUTPUT); // MAX7219 LOAD / Â¬CS

    pinMode(outLed,    OUTPUT);

    display_init();

    WiFi.begin("uaifai_2G", "NET16LUZ");
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Not connected.");
        // Toggle first square.
        for (int l = 0; l < LINES; ++l) {
            scratch[i + l * DISPLAYS] ^= 0xff;
        }
        refresh_display();
        delay(250);
    }
    Serial.println("Connected!");

    Udp.begin(port);
}

void loop() {
    if (Udp.parsePacket() == 0) return;
    int count = Udp.read(buf, sizeof buf);
    Serial.printf("Received %d bytes; [0] = 0x%02x.\r\n",
                  count, buf[0]);
    onReceive(count);
}

static void display_init() {
    pushInstrAll(0x900);        // Decode none
    pushInstrAll(0xa01);        // Set intensity
    pushInstrAll(0xb07);        // Scan limit all
    pushInstrAll(0xc01);        // Shutdown off
    pushInstrAll(0xf00);        // Display test off
}

static void setPin(int pin, int value) {
    value = value ? HIGH : LOW;
    digitalWrite(pin, value);
    digitalWrite(outLed, 1);
    digitalWrite(outLed, 0);
}

static void pushBit(int value) {
    setPin(outData, value);
    setPin(outClock, 1);
    setPin(outClock, 0);
}

static void commit() {
    setPin(outCommit, 1);
    setPin(outCommit, 0);
}

static void pushInstr(unsigned short opcode, bool do_commit = true) {
    for (int i = 0; i < 16; ++i) {
        pushBit(opcode & 0x8000);
        opcode <<= 1;
    }
    if (do_commit) commit();
}

static void pushInstrAll(unsigned short opcode) {
    pushInstr(opcode, false);
    pushInstr(opcode, false);
    pushInstr(opcode, false);
    pushInstr(opcode, true);
}

static byte revbyte(byte b) {
    byte out = 0x00;
    for (int i = 0; i < 8; ++i) {
        out <<= 1;
        out |= (b & 0x1);
        b >>= 1;
    }

    return out;
}

static void refresh_display() {
    byte *b = scratch;
    unsigned short opcode = 0x800;
    for (int l = 0; l < LINES; ++l, opcode -= 0x100, b += DISPLAYS) {
        pushInstr(opcode | revbyte(b[3]), false);
        pushInstr(opcode | revbyte(b[2]), false);
        pushInstr(opcode | revbyte(b[1]), false);
        pushInstr(opcode | revbyte(b[0]), true);
    }

    memcpy(current, scratch, BYTES);
}

byte font[] = {
    0b1110,
    0b1010,
    0b1010,
    0b1010,
    0b1010,
    0b1010,
    0b1010,
    0b1110,

    0b0100,
    0b1100,
    0b0100,
    0b0100,
    0b0100,
    0b0100,
    0b0100,
    0b1110,

    0b1110,
    0b0010,
    0b0010,
    0b1110,
    0b1000,
    0b1000,
    0b1000,
    0b1110,

    0b1110,
    0b0010,
    0b0010,
    0b1110,
    0b0010,
    0b0010,
    0b0010,
    0b1110,

    0b1010,
    0b1010,
    0b1010,
    0b1110,
    0b0010,
    0b0010,
    0b0010,
    0b0010,

    0b1110,
    0b1000,
    0b1000,
    0b1110,
    0b0010,
    0b0010,
    0b0010,
    0b1110,

    0b1110,
    0b1000,
    0b1000,
    0b1110,
    0b1010,
    0b1010,
    0b1010,
    0b1110,

    0b1110,
    0b0010,
    0b0010,
    0b0010,
    0b0010,
    0b0010,
    0b0010,
    0b0010,

    0b1110,
    0b1010,
    0b1010,
    0b1110,
    0b1010,
    0b1010,
    0b1010,
    0b1110,

    0b1110,
    0b1010,
    0b1010,
    0b1110,
    0b0010,
    0b0010,
    0b0010,
    0b1110,

    // [10]
    0b0000,
    0b0000,
    0b0100,
    0b0000,
    0b0000,
    0b0100,
    0b0000,
    0b0000,


    // Default
    0b0000,
    0b0000,
    0b0000,
    0b0000,
    0b0000,
    0b0000,
    0b0000,
    0b0000,
};


byte *fontAt(char c) {
    switch (c) {
    case '0': return &font[LINES * 0];
    case '1': return &font[LINES * 1];
    case '2': return &font[LINES * 2];
    case '3': return &font[LINES * 3];
    case '4': return &font[LINES * 4];
    case '5': return &font[LINES * 5];
    case '6': return &font[LINES * 6];
    case '7': return &font[LINES * 7];
    case '8': return &font[LINES * 8];
    case '9': return &font[LINES * 9];

    case ':': return &font[LINES * 10];

    default: return &font[LINES * 11];
    }
}

void compositeDigit(byte *p, int offset) {
    bool shift = (offset % 2) == 0;

    byte *dst = scratch + offset / 2;
    for (int i = 0; i < LINES; ++i) {
        dst[DISPLAYS * i] |= shift ? p[i] << 4 : p[i];
    }
}

static void onReceive(int bytes) {
    if (bytes < 1) return;

    byte instr = buf[0];
    --bytes;

    switch (instr) {
    case 0x01: display_init(); break;     // Init
    case 0x02: memset(scratch, 0, BYTES); // Clear
    case 0x03: refresh_display(); break;  // Commit

    case 0x80: {                // Write (text)
        if (bytes < CHARACTERS) return;

        memset(scratch, 0, BYTES);
        for (int i = 0; i < CHARACTERS; ++i) {
            byte *c = fontAt(buf[i + 1]);
            compositeDigit(c, i);
        }
        refresh_display();

        break;
    }

    case 0x81: {                // Set font
        if (bytes < LINES + 1) return;
        byte c = buf[1];             // Character to write
        byte *fontPoint = fontAt(c); // Never fails

        for (int i = 0; i < LINES; ++i) {
            fontPoint[i] = buf[i + 2];
        }
        break;
    }
    }
}
