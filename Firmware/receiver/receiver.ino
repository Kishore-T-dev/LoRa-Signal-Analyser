#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ── Screen geometry ──────────────────────────────────────────────────────────
#define SCREEN_W           240
#define SCREEN_H           320

// ── Top bar ──────────────────────────────────────────────────────────────────
#define TOP_BAR_H           28
#define TOP_BAR_SEP_Y       28       // horizontal separator y-position

// ── RSSI card  (x, y, w, h) ──────────────────────────────────────────────────
#define RSSI_CARD_X         10
#define RSSI_CARD_Y         35
#define RSSI_CARD_W        130
#define RSSI_CARD_H        100
// inner value zone (cleared on every update)
#define RSSI_VAL_X         (RSSI_CARD_X + 4)
#define RSSI_VAL_Y         (RSSI_CARD_Y + 16)
#define RSSI_VAL_W         (RSSI_CARD_W - 8)
#define RSSI_VAL_H          70

// ── Signal-bars card  (x, y, w, h) ───────────────────────────────────────────
#define SIG_CARD_X         150
#define SIG_CARD_Y          35
#define SIG_CARD_W          80
#define SIG_CARD_H         100
#define SIG_INNER_X        (SIG_CARD_X + 4)
#define SIG_INNER_Y        (SIG_CARD_Y + 16)
#define SIG_INNER_W        (SIG_CARD_W - 8)
#define SIG_INNER_H         76

// ── Mini metric cards (row starts) ───────────────────────────────────────────
#define MINI_ROW_Y         145
#define MINI_CARD_H         50
#define MINI_CARD_W         65
#define MINI_SNR_X          10
#define MINI_LAT_X          85
#define MINI_LOSS_X        160
// value zone within each mini card
#define MINI_VAL_INNER_Y   (MINI_ROW_Y + 17)
#define MINI_VAL_INNER_H    18

// ── Status bar ────────────────────────────────────────────────────────────────
#define STATUS_BAR_X        10
#define STATUS_BAR_Y       205
#define STATUS_BAR_W       220
#define STATUS_BAR_H        32

// ── Graph panel ───────────────────────────────────────────────────────────────
#define GRAPH_PANEL_X       10
#define GRAPH_PANEL_Y      245
#define GRAPH_PANEL_W      220
#define GRAPH_PANEL_H       68
#define GRAPH_DATA_X        20          // origin x of bar-data area
#define GRAPH_DATA_Y       297          // bottom y of bar-data area
#define GRAPH_DATA_W       200          // usable pixel columns (= GRAPH_WIDTH)
#define GRAPH_DATA_H        40          // usable pixel rows   (= GRAPH_HEIGHT)

// ── Battery display in top bar ───────────────────────────────────────────────
#define BAT_LABEL_X        168
#define BAT_LABEL_Y         10
#define BAT_VAL_X          196
#define BAT_VAL_Y           10
#define BAT_CLEAR_X        196
#define BAT_CLEAR_Y          5
#define BAT_CLEAR_W         38
#define BAT_CLEAR_H         18

// ── "NO SIGNAL" overlay inside RSSI card ─────────────────────────────────────
#define NOSIG_X            (RSSI_CARD_X + 4)
#define NOSIG_Y            (RSSI_CARD_Y + 36)
#define NOSIG_W            (RSSI_CARD_W - 8)
#define NOSIG_H             26


/* ── Base UI ── */
#define COL_BG          0x0000
#define COL_CARD        0x18E3
#define COL_GRID        0x4208
#define COL_TEXT        0xFFFF
#define COL_SUBTEXT     0x8410

/* ── Signal Strength ── */
#define COL_SIG_STRONG  0xF81F
#define COL_SIG_MED     0x0400
#define COL_SIG_WEAK    0x07E0
#define COL_SIG_NONE    0x6B4D

/* ── Graph ── */
#define COL_GRAPH       0xF81F
#define COL_GRAPH_FADE  0x6B4D

/* ── Alerts ── */
#define COL_WARNING     0x0400
#define COL_CRITICAL    0x07FF

/* ── Accent ── */
#define COL_ACCENT      0x39E7

#define C_BG        COL_BG
#define C_PANEL     COL_CARD
#define C_SEP       COL_GRID
#define C_ACCENT    COL_ACCENT
#define C_TEXT      COL_TEXT
#define C_DIM       COL_SUBTEXT
#define C_GOOD      COL_SIG_STRONG
#define C_WARN      COL_SIG_MED
#define C_BAD       COL_SIG_WEAK

/* ── Semantic aliases ── */
#define CP_BACKGROUND   COL_BG
#define CP_SURFACE      COL_CARD
#define CP_BORDER       COL_GRID
#define CP_PRIMARY      COL_TEXT
#define CP_SECONDARY    COL_SUBTEXT
#define CP_ACCENT       COL_ACCENT
#define CP_SUCCESS      COL_SIG_STRONG
#define CP_WARNING      COL_SIG_MED
#define CP_ERROR        COL_SIG_WEAK
#define CP_INACTIVE     COL_GRAPH_FADE

/* ── Additional functional colours ── */
#define C_AMBER_DIM   COL_GRAPH_FADE
#define C_GRAPH_GRID  COL_GRID
#define C_NOSIG_BG    0x07E0

#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST   14

#define LORA_CS    5
#define LORA_RST   4
#define LORA_DIO0  6
#define LORA_FREQ  433E6

#define SPI_SCK   12
#define SPI_MISO  13
#define SPI_MOSI  11

#define BAT_ADC    1
#define BAT_MIN   3.0f
#define BAT_MAX   4.2f

#define SIGNAL_TIMEOUT_MS   2000UL   // ms without a packet → inactive
#define RSSI_FLOOR          -120     // value injected when inactive

// Signal-state enum (clean state machine)
enum class SignalState : uint8_t {
    ACTIVE,         // receiving packets normally
    TIMEOUT,        // no packet within SIGNAL_TIMEOUT_MS
    INITIALIZING    // waiting for first packet ever
};

SignalState  signalState     = SignalState::INITIALIZING;
unsigned long lastRxTime     = 0;   // millis() of most recent valid packet

/* Returns the current signal state based on elapsed time.
   Call once per loop() before using signalState. */
SignalState evalSignalState() {
    if (signalState == SignalState::INITIALIZING) return SignalState::INITIALIZING;
    return (millis() - lastRxTime < SIGNAL_TIMEOUT_MS)
           ? SignalState::ACTIVE
           : SignalState::TIMEOUT;
}

int effectiveRSSI(float avg) {
    return (signalState == SignalState::ACTIVE) ? (int)avg : RSSI_FLOOR;
}


inline void spiSelectLoRa() {
    digitalWrite(TFT_CS, HIGH);    // de-assert TFT before touching LoRa bus
    // LORA_CS driven by LoRa library internally
}

inline void spiSelectTFT() {
    // De-assert LoRa CS just in case library left it asserted on error
    digitalWrite(LORA_CS, HIGH);
    // TFT_CS driven by Adafruit library internally
}

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Forward declarations for original functions called by new sections
void drawCenteredStr(const char* str, int bx, int by, int bw, int bh,
                     uint8_t sz, uint16_t col);

void drawBoundedStr(const char* str, int cx, int cy, int cw, int ch,
                    uint8_t sz, uint16_t col) {
    int charW = 6 * sz;
    int maxChars = cw / charW;

    char buf[16];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if ((int)strlen(buf) > maxChars) buf[maxChars] = '\0';

    int tw = strlen(buf) * charW;
    int th = 8 * sz;
    int tx = cx + (cw - tw) / 2;
    int ty = cy + (ch - th) / 2;

    tft.fillRect(cx, cy, cw, ch, C_PANEL);
    tft.setTextColor(col);
    tft.setTextSize(sz);
    tft.setCursor(tx, ty);
    tft.print(buf);
}


void drawBoundedMiniValue(int cardX, float val, int decimals, uint16_t col = C_TEXT) {
    char buf[10];
    if (decimals == 0)      snprintf(buf, sizeof(buf), "%d",   (int)roundf(val));
    else if (decimals == 1) snprintf(buf, sizeof(buf), "%.1f", val);
    else                    snprintf(buf, sizeof(buf), "%.2f", val);

    // Clear + draw inside the value zone of the mini card
    drawBoundedStr(buf,
                   cardX + 3,            // zone x
                   MINI_VAL_INNER_Y,     // zone y   (163 original)
                   MINI_CARD_W - 6,      // zone w   (59 original)
                   MINI_VAL_INNER_H,     // zone h   (19 original)
                   2,                    // textSize=2  → 12 px/char
                   col);
}

void animateProgressBar_nonblocking(int barX, int barY, int barW) {
    tft.drawRoundRect(barX, barY, barW, 10, 6, C_SEP);

    int filled = 0;
    unsigned long stepStart = millis();
    const unsigned long STEP_MS = 15;   // ~15 ms per pixel ≈ 2.4 s total

    while (filled < barW - 4) {
        if (millis() - stepStart >= STEP_MS) {
            stepStart = millis();
            tft.fillRect(barX + 2 + filled, barY + 2, 1, 6, C_ACCENT);
            filled++;
        }
        yield();   // keep RTOS watchdog happy on ESP32
    }
    tft.drawRoundRect(barX, barY, barW, 10, 6, C_ACCENT);

    // Non-blocking 800 ms hold
    unsigned long holdStart = millis();
    while (millis() - holdStart < 800UL) yield();
}

static bool noSigOverlayVisible = false;

void showNoSignalOverlay() {
    if (noSigOverlayVisible) return;
    noSigOverlayVisible = true;

    // Dark-red tinted backing strip
    tft.fillRect(NOSIG_X, NOSIG_Y, NOSIG_W, NOSIG_H, C_NOSIG_BG);
    tft.drawRect(NOSIG_X, NOSIG_Y, NOSIG_W, NOSIG_H, COL_CRITICAL);

    // Centered "NO SIGNAL" text (sz=1 → 6 px/char)
    drawCenteredStr("NO SIGNAL", NOSIG_X, NOSIG_Y, NOSIG_W, NOSIG_H, 1, COL_CRITICAL);
}

/* Removes the overlay (called when signal recovers). */
void clearNoSignalOverlay() {
    if (!noSigOverlayVisible) return;
    noSigOverlayVisible = false;
    tft.fillRect(NOSIG_X, NOSIG_Y, NOSIG_W, NOSIG_H, C_PANEL);
}

/* Dim the graph panel border to visually communicate inactive state. */
void drawGraphPanelBorder(bool active) {
    uint16_t col = active ? C_SEP : C_AMBER_DIM;
    tft.drawRoundRect(GRAPH_PANEL_X, GRAPH_PANEL_Y,
                      GRAPH_PANEL_W, GRAPH_PANEL_H, 6, col);
}

uint16_t calcLostPackets(uint16_t lastID, uint16_t newID) {
    // Signed delta in 16-bit space handles rollover naturally
    int32_t delta = (int32_t)newID - (int32_t)lastID;
    if (delta <= 0) return 0;           // duplicate / out-of-order: ignore
    if (delta == 1) return 0;           // exactly the next packet: no loss
    return (uint16_t)(delta - 1);      // gap → lost count
}

/* ================= METRICS ================= */
uint16_t lastPacketID = 0;
bool     firstPacket  = true;

uint32_t totalPackets = 0;
uint32_t lostPackets  = 0;

float    rssiAvg = -100;
float    snrAvg  =    0;
uint32_t latAvg  =    0;

const int AVG_WINDOW = 8;

unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 400;

bool          rxBlink    = false;
unsigned long rxBlinkTime = 0;

/* ================= GRAPH ================= */
// NOTE: constants below now alias the Section-A macros for full compatibility
#define GRAPH_WIDTH   GRAPH_DATA_W    // ← aliased (was literal 200)
#define GRAPH_HEIGHT  GRAPH_DATA_H    // ← aliased (was literal  40)
const int graphX   = GRAPH_DATA_X;   // ← aliased (was literal  20)
const int graphY   = GRAPH_DATA_Y;   // ← aliased (was literal 297)
int graphIndex     = 0;

float readBatteryPercent() {

    uint32_t raw = 0;

    for (int i = 0; i < 16; i++) {
        raw += analogReadMilliVolts(BAT_ADC);
    }

    float vPin = (raw / 16.0f) / 1000.0f;

    // 100k / 100k divider scaling
    float v = vPin * 2.0f;

    v = constrain(v, BAT_MIN, BAT_MAX);

    // Convert voltage to percentage
    float percent = ((v - BAT_MIN) / (BAT_MAX - BAT_MIN)) * 100.0f;

    // Smoothing filter
    static float filtered = 0;

    if (filtered == 0) {
        filtered = percent;
    }

    filtered = filtered * 0.9f + percent * 0.1f;

    return filtered;
}

// Centers a string inside a box (bx, by, bw, bh).
void drawCenteredStr(const char* str, int bx, int by, int bw, int bh,
                     uint8_t sz, uint16_t col) {
    int tw = strlen(str) * 6 * sz;
    int th = 8 * sz;
    int tx = bx + (bw - tw) / 2;
    int ty = by + (bh - th) / 2;
    tft.fillRect(bx, by, bw, bh, C_PANEL);
    tft.setTextColor(col);
    tft.setTextSize(sz);
    tft.setCursor(tx, ty);
    tft.print(str);
}

// Returns strong / medium / weak signal colour depending on RSSI quality
uint16_t rssiColor(int rssi) {
    if (rssi >= -70) return COL_SIG_STRONG;
    if (rssi >= -90) return COL_SIG_MED;
    return COL_SIG_WEAK;
}

// Returns strong / medium / weak colour depending on battery %
uint16_t batColor(float pct) {
    return COL_TEXT;
}

void bootScreen() {
    tft.fillScreen(C_BG);

    int cardX = 30, cardY = 80, cardW = 180, cardH = 100;
    tft.fillRoundRect(cardX, cardY, cardW, cardH, 6, C_PANEL);
    tft.drawRoundRect(cardX, cardY, cardW, cardH, 6, C_SEP);

    tft.setTextColor(C_ACCENT); tft.setTextSize(3);
    tft.setCursor(60, 95);  tft.print("LORA");

    tft.setTextColor(C_TEXT); tft.setTextSize(2);
    tft.setCursor(50, 130); tft.print("ANALYZER");

    tft.setTextColor(C_DIM); tft.setTextSize(1);
    tft.setCursor(68, 165); tft.print("INITIALIZING");

    int barX = 40, barY = 220, barW = 160;

    animateProgressBar_nonblocking(barX, barY, barW);
}

void drawStaticUI() {
    tft.fillScreen(C_BG);

    /* ── TOP BAR ── */
    tft.fillRect(0, 0, 240, 28, C_PANEL);
    tft.drawFastHLine(0, 28, 240, C_SEP);

    tft.setTextColor(C_ACCENT); tft.setTextSize(1);
    tft.setCursor(8, 10); tft.print("RX ANALYZER");

    tft.setTextColor(C_DIM); tft.setTextSize(1);
    tft.setCursor(168, 10); tft.print("BAT:");

    /* ── RSSI CARD ── */
    tft.fillRoundRect(10, 35, 130, 100, 6, C_PANEL);
    tft.drawRoundRect(10, 35, 130, 100, 6, C_SEP);
    drawCenteredStr("RSSI", 10, 37, 130, 14, 1, C_DIM);
    drawCenteredStr("dBm",  10, 122, 130, 13, 1, C_DIM);

    /* ── SIGNAL CARD ── */
    tft.fillRoundRect(150, 35, 80, 100, 6, C_PANEL);
    tft.drawRoundRect(150, 35, 80, 100, 6, C_SEP);
    drawCenteredStr("SIG", 150, 37, 80, 14, 1, C_DIM);

    /* ── MINI METRIC CARDS ── */
    auto drawMiniCard = [&](int cx, const char* label, const char* unit) {
        tft.fillRoundRect(cx, 145, 65, 50, 4, C_PANEL);
        tft.drawRoundRect(cx, 145, 65, 50, 4, C_SEP);
        drawCenteredStr(label, cx, 148, 65, 12, 1, C_DIM);
        drawCenteredStr(unit,  cx, 183, 65, 10, 1, C_DIM);
    };

    drawMiniCard(10,  "SNR",  "dB");
    drawMiniCard(85,  "LAT",  "ms");
    drawMiniCard(160, "LOSS", "%");

    /* ── STATUS BAR ── */
    tft.fillRoundRect(10, 205, 220, 32, 6, C_PANEL);
    tft.drawRoundRect(10, 205, 220, 32, 6, C_SEP);

    /* ── GRAPH PANEL ── */
    tft.fillRoundRect(10, 245, 220, 68, 6, C_PANEL);
    tft.drawRoundRect(10, 245, 220, 68, 6, C_SEP);
    drawCenteredStr("RSSI  HISTORY", 10, 247, 220, 12, 1, C_DIM);
}


/* ── Battery % in top bar ── */
void drawBattery(float pct) {
    char buf[5];
    snprintf(buf, sizeof(buf), "%3d%%", (int)pct);
    tft.fillRect(196, 5, 38, 18, C_PANEL);
    tft.setTextColor(batColor(pct));
    tft.setTextSize(1);
    tft.setCursor(196, 10);
    tft.print(buf);
}

/* ── RSSI big value ── */
void drawRSSI(int rssi) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", rssi);

    int tw = strlen(buf) * 18;
    int tx = 14 + (122 - tw) / 2;
    int ty = 51 + (70 - 24) / 2;

    tft.fillRect(14, 51, 122, 70, C_PANEL);
    tft.setTextSize(3);
    tft.setTextColor(COL_TEXT);
    tft.setCursor(tx, ty);
    tft.print(buf);
}

/* ── Signal bars ── */
void drawSignalBars(int rssi) {
    int bars = map(rssi, -120, -40, 0, 5);
    bars = constrain(bars, 0, 5);

    tft.fillRect(154, 51, 72, 76, C_PANEL);

    int baseX = 157;
    int baseY = 127;

    for (int i = 0; i < 5; i++) {
        int h = (i + 1) * 11;
        uint16_t col = (i < bars) ? rssiColor(rssi) : COL_SIG_NONE;
        tft.fillRoundRect(baseX + i * 14, baseY - h, 10, h, 2, col);
    }
}

/* ── Single mini-card value  [ORIGINAL – kept for compatibility] ── */
void drawMiniValue(int cx, float val, int decimals) {
    char buf[10];
    if (decimals == 0) snprintf(buf, sizeof(buf), "%d",   (int)roundf(val));
    else               snprintf(buf, sizeof(buf), "%.1f", val);

    int tw = strlen(buf) * 12;
    int tx = cx + (65 - tw) / 2;

    tft.fillRect(cx + 3, 162, 59, 19, C_PANEL);
    tft.setTextSize(2);
    tft.setTextColor(C_TEXT);
    tft.setCursor(tx, 163);
    tft.print(buf);
}

void drawMetrics(float snr, uint32_t latency, float loss) {
    drawBoundedMiniValue(MINI_SNR_X,  snr,           1, COL_TEXT);
    drawBoundedMiniValue(MINI_LAT_X,  (float)latency, 0, COL_TEXT);
    drawBoundedMiniValue(MINI_LOSS_X, loss,           1, COL_TEXT);
}

/* ── Status bar ── */
void drawStatus(bool connected) {
    tft.fillRect(14, 209, 212, 24, C_PANEL);
    if (connected) {
        drawCenteredStr("CONNECTED", 14, 209, 212, 24, 2, COL_SIG_STRONG);
    } else {
        drawCenteredStr("NO LINK",   14, 209, 212, 24, 2, COL_CRITICAL);
    }
}

void drawGraph(int rssi) {
    int h = map(rssi, -120, -40, 0, GRAPH_HEIGHT);
    h = constrain(h, 0, GRAPH_HEIGHT);

    int x = graphX + graphIndex;

    tft.drawFastVLine(x, graphY - GRAPH_HEIGHT, GRAPH_HEIGHT, C_PANEL);

    uint16_t barCol = (signalState == SignalState::ACTIVE)
                      ? COL_GRAPH
                      : COL_GRAPH_FADE;

    int drawH = (h < 1) ? 1 : h;
    tft.drawFastVLine(x, graphY - drawH, drawH, barCol);

    graphIndex = (graphIndex + 1) % GRAPH_WIDTH;
}

void setup() {
    Serial.begin(115200);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    // Hard-reset TFT
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);  delay(50);
    digitalWrite(TFT_RST, HIGH); delay(50);
    pinMode(TFT_CS,  OUTPUT); digitalWrite(TFT_CS,  HIGH);
    pinMode(LORA_CS, OUTPUT); digitalWrite(LORA_CS, HIGH);

    tft.init(240, 320);
    tft.setRotation(2);

    bootScreen();
    drawStaticUI();

    // Init LoRa
    spiSelectLoRa();  
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(LORA_FREQ)) {
        spiSelectTFT();   
        tft.fillScreen(COL_CRITICAL);
        tft.setTextColor(COL_TEXT);
        tft.setTextSize(2);
        tft.setCursor(40, 145); tft.print("LORA INIT");
        tft.setCursor(60, 170); tft.print("FAILED");
        while (1);
    }

    // ── LoRa parameters — must match transmitter exactly ────────────────────
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(5);
    LoRa.setSyncWord(0xF3);

    LoRa.receive();
    analogReadResolution(12);
    analogSetPinAttenuation(BAT_ADC, ADC_11db);
}
void loop() {

    /* ── RX ── */
    int packetSize = LoRa.parsePacket();

    if (packetSize) {
        uint16_t pktID;
        uint32_t txTime;

        LoRa.readBytes((uint8_t*)&pktID,   2);
        LoRa.readBytes((uint8_t*)&txTime,  4);

        uint32_t rxTime = millis();
        float rssi = LoRa.packetRssi();
        float snr  = LoRa.packetSnr();
        uint32_t lat = rxTime - txTime;

        if (!firstPacket) {
            lostPackets += calcLostPackets(lastPacketID, pktID);
        }
        firstPacket  = false;
        lastPacketID = pktID;
        totalPackets++;

        lastRxTime  = rxTime;
        signalState = SignalState::ACTIVE;

        rssiAvg = (rssiAvg * (AVG_WINDOW - 1) + rssi) / AVG_WINDOW;
        snrAvg  = (snrAvg  * (AVG_WINDOW - 1) + snr)  / AVG_WINDOW;
        latAvg  = (latAvg  * (AVG_WINDOW - 1) + lat)  / AVG_WINDOW;

        rxBlink    = true;
        rxBlinkTime = millis();

        Serial.printf("%u,%.1f,%.1f,%lu\n", pktID, rssi, snr, lat);
    }

    signalState = evalSignalState();

    /* ── DISPLAY UPDATE ── */
    if (millis() - lastDisplayUpdate > DISPLAY_INTERVAL) {
        lastDisplayUpdate = millis();

        spiSelectTFT();   

        // Battery (top bar)
        drawBattery(readBatteryPercent());
        int dispRSSI = effectiveRSSI(rssiAvg);  // ← NEW

        // Main panels
        drawRSSI(dispRSSI);           
        drawSignalBars(dispRSSI);     

        if (signalState != SignalState::ACTIVE && signalState != SignalState::INITIALIZING) {
            showNoSignalOverlay();
            drawGraphPanelBorder(false);
        } else {
            clearNoSignalOverlay();
            drawGraphPanelBorder(true);
        }

        float lossPercent = (totalPackets + lostPackets > 0)
                            ? (lostPackets * 100.0f / (totalPackets + lostPackets))
                            : 0.0f;

        drawMetrics(snrAvg, latAvg, lossPercent);
        drawStatus(signalState == SignalState::ACTIVE);  

        drawGraph(dispRSSI);  
    }
}
