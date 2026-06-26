#include <Arduino.h>
#include <TFT_eSPI.h> // Include the graphics library
#include <SPI.h>
#include <deque>
#include <vector>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
// --- Your Button Definitions ---
#define BTN_A 4
#define BTN_B 5
#define BTN_SELECT 8
#define BTN_DOWN 46
#define BTN_RIGHT 10
#define BTN_UP 16
#define BTN_LEFT 21
#define BTN_START 6

// ========== BlazeCore Web Gamepad Integration ==========
// WiFi Configuration
const char* WIFI_SSID = ""; // put your WiFi SSID here (optional)
const char* WIFI_PASS = ""; // put your WiFi password here (optional)
const char* AP_SSID_BASE = "BLAZECORE-";
const char* AP_PASSWORD = "blaze1234";

// Web button mapping
enum WebBtnIndex {
  WEB_BTN_A_IDX = 0,
  WEB_BTN_B_IDX,
  WEB_BTN_SELECT_IDX,
  WEB_BTN_START_IDX,
  WEB_BTN_UP_IDX,
  WEB_BTN_DOWN_IDX,
  WEB_BTN_LEFT_IDX,
  WEB_BTN_RIGHT_IDX,
  WEB_BTN_COUNT
};

const char* WEB_BTN_NAMES[WEB_BTN_COUNT] = {
  "A","B","SELECT","START","UP","DOWN","LEFT","RIGHT"
};

// Web server and WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Web button states
volatile uint8_t webButtonState[WEB_BTN_COUNT] = {0};
volatile unsigned long webButtonTs[WEB_BTN_COUNT] = {0};

// Web button helper functions
void setWebButtonState(int idx, uint8_t value) {
  if (idx < 0 || idx >= WEB_BTN_COUNT) return;
  webButtonState[idx] = value;
  webButtonTs[idx] = millis();
}

uint8_t getWebButtonState(int idx) {
  if (idx < 0 || idx >= WEB_BTN_COUNT) return 0;
  return webButtonState[idx];
}

int btnNameToIndex(const String &name) {
  for (int i=0;i<WEB_BTN_COUNT;i++) 
    if (name.equalsIgnoreCase(WEB_BTN_NAMES[i])) return i;
  return -1;
}

String buildStateJSON() {
  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  JsonObject s = root.createNestedObject("state");
  for (int i=0;i<WEB_BTN_COUNT;i++) 
    s[WEB_BTN_NAMES[i]] = (int)webButtonState[i];
  String out;
  serializeJson(doc, out);
  return out;
}

void broadcastState() {
  String msg = buildStateJSON();
  ws.textAll(msg);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, (const char*)data, len);
  if (err) return;
  const char* btn = doc["btn"];
  int state = doc["state"] | 0;
  if (!btn) return;
  int idx = btnNameToIndex(String(btn));
  if (idx >= 0) {
    setWebButtonState(idx, state ? 1 : 0);
    broadcastState();
  }
}

void onWsEvent(AsyncWebSocket *serverPtr, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    String s = buildStateJSON();
    client->text(s);
  } else if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>BlazeCore Webpad</title>
<style>
  * {margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent;}
  :root{--gb-body:#9FA8A3;--gb-screen:#8B956D;--gb-darkgray:#1C1C1C;--gb-button:#2B2B2B;--gb-red:#B71C1C;--gb-accent:#D32F2F;}
  html,body{width:100%;height:100%;overflow:hidden;font-family:Arial,sans-serif;background:#1a1a1a;}
  body{display:flex;align-items:center;justify-content:center;}
  .gameboy{background:var(--gb-body);border-radius:12px 12px 40px 40px;box-shadow:0 8px 30px rgba(0,0,0,0.6),inset 0 2px 0 rgba(255,255,255,0.2);position:relative;display:flex;flex-direction:column;padding:20px;}
  .screen-area{background:var(--gb-darkgray);border-radius:8px;padding:12px;box-shadow:inset 0 4px 8px rgba(0,0,0,0.5);margin-bottom:20px;}
  .screen{background:var(--gb-screen);padding:10px;border-radius:4px;text-align:center;font-weight:bold;color:#0f380f;font-size:18px;box-shadow:inset 0 2px 4px rgba(0,0,0,0.3);}
  .hud{display:flex;gap:8px;align-items:center;justify-content:center;margin-top:6px;}
  .led{width:8px;height:8px;border-radius:50%;background:#2b3740;box-shadow:0 1px 2px rgba(0,0,0,0.4);}
  .led.on{background:#ff3333;box-shadow:0 0 8px rgba(255,51,51,0.8);}
  #status{font-size:11px;color:rgba(255,255,255,0.6);}
  .controls{display:flex;justify-content:space-between;gap:20px;align-items:flex-end;}
  .left-section,.right-section{display:flex;flex-direction:column;gap:12px;align-items:center;}
  .dpad-container{position:relative;width:180px;height:180px;}
  .dpad{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);width:170px;height:170px;}
  .dpad button{position:absolute;background:var(--gb-darkgray);border:none;color:#fff;font-weight:bold;font-size:20px;box-shadow:0 4px 0 #000,inset 0 1px 0 rgba(255,255,255,0.1);cursor:pointer;user-select:none;}
  .dpad button:active{box-shadow:0 1px 0 #000,inset 0 2px 4px rgba(0,0,0,0.5);transform:translateY(2px);}
  .dpad .up,.dpad .down{width:55px;height:60px;left:50%;transform:translateX(-50%);border-radius:6px 6px 0 0;}
  .dpad .down{top:auto;bottom:0;border-radius:0 0 6px 6px;transform:translateX(-50%);}
  .dpad .up{top:0;}
  .dpad .left,.dpad .right{width:60px;height:55px;top:50%;transform:translateY(-50%);border-radius:6px 0 0 6px;}
  .dpad .right{left:auto;right:0;border-radius:0 6px 6px 0;transform:translateY(-50%);}
  .dpad .left{left:0;}
  .dpad-center{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);width:55px;height:55px;background:var(--gb-darkgray);border-radius:50%;box-shadow:inset 0 2px 4px rgba(0,0,0,0.5);pointer-events:none;}
  .action-buttons{display:flex;gap:18px;align-items:center;transform:rotate(-15deg);}
  .action-btn{width:65px;height:65px;border-radius:50%;border:none;background:var(--gb-red);color:#fff;font-weight:bold;font-size:18px;box-shadow:0 4px 0 #8B1C1C,inset 0 2px 0 rgba(255,255,255,0.2);cursor:pointer;user-select:none;}
  .action-btn:active{box-shadow:0 2px 0 #8B1C1C,inset 0 3px 6px rgba(0,0,0,0.4);transform:translateY(2px);}
  .action-btn.pressed{background:var(--gb-accent);box-shadow:0 2px 0 #8B1C1C,inset 0 3px 6px rgba(0,0,0,0.4);transform:translateY(2px);}
  .small-buttons{display:flex;gap:20px;margin-top:8px;}
  .small-btn{width:45px;height:18px;border-radius:20px;border:none;background:var(--gb-button);color:#999;font-size:9px;font-weight:bold;box-shadow:inset 0 2px 3px rgba(0,0,0,0.6);cursor:pointer;user-select:none;}
  .small-btn:active{box-shadow:inset 0 3px 5px rgba(0,0,0,0.8);}
  .brand{position:absolute;bottom:12px;left:20px;font-size:11px;font-weight:bold;color:rgba(0,0,0,0.3);font-style:italic;}
  .dpad button.pressed{background:var(--gb-accent);box-shadow:0 1px 0 #000,inset 0 2px 4px rgba(0,0,0,0.5);transform:translateY(2px);}
  .small-btn.pressed{background:#444;box-shadow:inset 0 3px 5px rgba(0,0,0,0.8);}
  @media(orientation:landscape)and (max-height:500px){
    .gameboy{flex-direction:row;border-radius:40px 12px 12px 40px;padding:15px 20px;max-width:95vw;max-height:90vh;}
    .screen-area{margin-bottom:0;margin-right:20px;flex:1;display:flex;flex-direction:column;justify-content:center;}
    .controls{flex:1;gap:30px;}
    .dpad-container{width:100px;height:100px;}
    .dpad{width:90px;height:90px;}
    .dpad .up,.dpad .down{width:30px;height:35px;}
    .dpad .left,.dpad .right{width:35px;height:30px;}
    .dpad-center{width:30px;height:30px;}
    .action-btn{width:55px;height:55px;font-size:16px;}
    .small-btn{width:40px;height:16px;font-size:8px;}
  }
  @media(orientation:portrait){
    .gameboy{width:min(380px,95vw);padding:20px;}
    .screen-area{width:100%;}
  }
</style>
</head>
<body>
<div class="gameboy">
  <div class="screen-area">
    <div class="screen">BLAZECORE</div>
    <div class="hud">
      <div class="led" id="led-ws"></div>
      <div id="status">Connecting</div>
    </div>
  </div>
  <div class="controls">
    <div class="left-section">
      <div class="dpad-container">
        <div class="dpad">
          <button class="up" data-btn="UP">&uarr;</button>
          <button class="down" data-btn="DOWN">&darr;</button>
          <button class="left" data-btn="LEFT">&larr;</button>
          <button class="right" data-btn="RIGHT">&rarr;</button>
          <div class="dpad-center"></div>
        </div>
      </div>
      <div class="small-buttons">
        <button class="small-btn" data-btn="SELECT">SELECT</button>
        <button class="small-btn" data-btn="START">START</button>
      </div>
    </div>
    <div class="right-section">
      <div class="action-buttons">
        <button class="action-btn" data-btn="B">B</button>
        <button class="action-btn" data-btn="A">A</button>
      </div>
    </div>
  </div>
  <div class="brand">BlazeCore&trade;</div>
</div>
<script>
(() => {
  const ws = new WebSocket('ws://' + location.host + '/ws');
  const status = document.getElementById('status');
  const led = document.getElementById('led-ws');
  const btnEls = document.querySelectorAll('[data-btn]');
  const pressMap = {};
  
  function sendPress(btnName, state) {
    const msg = JSON.stringify({btn: btnName, state: state?1:0});
    if (ws.readyState === WebSocket.OPEN) ws.send(msg);
  }
  
  btnEls.forEach(b => {
    const name = b.getAttribute('data-btn');
    
    b.addEventListener('pointerdown', e => {
      e.preventDefault();
      if (pressMap[name]) return;
      pressMap[name] = true;
      b.classList.add('pressed');
      sendPress(name, 1);
    });
    
    b.addEventListener('pointerup', e => {
      e.preventDefault();
      pressMap[name] = false;
      b.classList.remove('pressed');
      sendPress(name, 0);
    });
    
    b.addEventListener('pointerleave', e => {
      if (pressMap[name]) {
        pressMap[name] = false;
        b.classList.remove('pressed');
        sendPress(name, 0);
      }
    });
    
    b.addEventListener('touchstart', e => {
      e.preventDefault();
    });
  });
  
  document.addEventListener('keydown', e => {
    let m = null;
    if (e.key === 'ArrowUp') m = 'UP';
    if (e.key === 'ArrowDown') m = 'DOWN';
    if (e.key === 'ArrowLeft') m = 'LEFT';
    if (e.key === 'ArrowRight') m = 'RIGHT';
    if (e.key === 'z' || e.key === 'Z') m = 'A';
    if (e.key === 'x' || e.key === 'X') m = 'B';
    if (e.key === 'Enter') m = 'START';
    if (e.key === 'Shift') m = 'SELECT';
    if (m) {
      e.preventDefault();
      if (!pressMap[m]) {
        pressMap[m] = true;
        const el = document.querySelector('[data-btn="'+m+'"]');
        if (el) el.classList.add('pressed');
        sendPress(m,1);
      }
    }
  });
  
  document.addEventListener('keyup', e => {
    let m = null;
    if (e.key === 'ArrowUp') m = 'UP';
    if (e.key === 'ArrowDown') m = 'DOWN';
    if (e.key === 'ArrowLeft') m = 'LEFT';
    if (e.key === 'ArrowRight') m = 'RIGHT';
    if (e.key === 'z' || e.key === 'Z') m = 'A';
    if (e.key === 'x' || e.key === 'X') m = 'B';
    if (e.key === 'Enter') m = 'START';
    if (e.key === 'Shift') m = 'SELECT';
    if (m) {
      pressMap[m] = false;
      const el = document.querySelector('[data-btn="'+m+'"]');
      if (el) el.classList.remove('pressed');
      sendPress(m,0);
    }
  });
  
  ws.addEventListener('open', () => {
    status.textContent = "Connected";
    led.classList.add('on');
  });
  
  ws.addEventListener('close', () => {
    status.textContent = "Disconnected";
    led.classList.remove('on');
  });
  
  ws.addEventListener('message', ev => {
    try {
      const d = JSON.parse(ev.data);
      if (d.state) {
        for (const k of Object.keys(d.state)) {
          const v = d.state[k];
          const el = document.querySelector('[data-btn="'+k+'"]');
          if (!el) continue;
          if (v) el.classList.add('pressed');
          else el.classList.remove('pressed');
        }
      }
    } catch (e) {}
  });
})();
</script>
</body>
</html>
)rawliteral";

void initWebServer() {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", INDEX_HTML);
    response->addHeader("Cache-Control", "no-store, must-revalidate");
    request->send(response);
  });
  
  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
    String s = buildStateJSON();
    request->send(200, "application/json", s);
  });
  
  server.begin();
}

String makeAPName() {
  uint64_t chipid = ESP.getEfuseMac();
  char buf[32];
  sprintf(buf, "%s%04X", AP_SSID_BASE, (unsigned int)(chipid & 0xFFFF));
  return String(buf);
}

void startWiFi() {
  if (strlen(WIFI_SSID) > 0) {
    Serial.print("Attempting to join WiFi '"); 
    Serial.print(WIFI_SSID); 
    Serial.println("' ...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long start = millis();
    const unsigned long timeout = 10000;
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeout) {
      delay(200);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.print("Connected. IP: ");
      Serial.println(WiFi.localIP());
      return;
    } else {
      Serial.println();
      Serial.println("STA connect failed, falling back to AP mode.");
    }
  }
  
  String apn = makeAPName();
  Serial.printf("Starting AP '%s' (pw='%s')...\n", apn.c_str(), AP_PASSWORD);
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(apn.c_str(), AP_PASSWORD);
  if (!ok) {
    Serial.println("softAP failed");
  } else {
    IPAddress ip = WiFi.softAPIP();
    Serial.print("AP IP: ");
    Serial.println(ip);
  }
}

// Unified button reading: combines physical button + web button
int readButton(int physicalPin, int webBtnIdx) {
  int physical = digitalRead(physicalPin);
  int web = getWebButtonState(webBtnIdx);
  // LOW = pressed for physical (INPUT_PULLUP), 1 = pressed for web
  // Return LOW if either is pressed
  return (physical == LOW || web == 1) ? LOW : HIGH;
}

// ========== End Web Gamepad Integration ==========

// --- Global Objects ---
TFT_eSPI tft = TFT_eSPI(); // Create the TFT object

// --- State Machine ---
enum AppState {
    STATE_SPLASH,
    STATE_MENU,
    STATE_GAME_SNAKE,
    STATE_GAME_PONG,
    STATE_GAME_FLAPPY,
    STATE_GAME_GEOMETRY,
    STATE_GAME_PACMAN,
    STATE_GAME_TETRIS,
    STATE_GAME_SNAKE2P,
    STATE_GAME_PONG2P,
    STATE_GAME_BLACKJACK,
    STATE_GAME_BLACKJACK2P,
    STATE_GAME_POKER,
    STATE_GAME_POKER2P
};

AppState currentState = STATE_SPLASH;
bool needsRedraw = true;

// --- Menu Navigation Variables ---
const int numGames = 12;
const char *gameNames[] = {"Snake", "Pong", "Flappy Bird", "Geometry Clone", "Pac-man", "Tetris", "Snake 2P", "Pong 2P", "Blackjack", "Blackjack 2P", "Poker", "Poker 2P"};
int selectedGame = 0; // 0=Snake, 1=Pong, etc.

// --- Layout & visuals ---
int menuX = 16;
int menuY = 60;
int menuW = 296;
int menuH = 160;

int selectorOffset = 0;   // current center y of selector
int selectorTarget = 0;   // target center y (snapped immediately)

bool menuInitialized = false;

// Debounce + edge detection
unsigned long lastNavMillis = 0;
const unsigned long navDebounceMs = 60; // tweak up if you see repeats (ms)

// Keep previous button states for edge detection (HIGH = unpressed with INPUT_PULLUP)
int prevUpState = HIGH;
int prevDownState = HIGH;
int prevAState = HIGH;
int prevStartState = HIGH;
int prevSelectState = HIGH;

uint16_t accent = TFT_ORANGE; // selector color

// --- Function Prototypes ---
void loopSplash();
void drawMenu();
void loopMenu();
void runSnakeFull();
void runPongFull();
void runFlappyFull();
void runGeometryFull();
void runPacmanFull();
void runTetrisFull();
void runSnake2PFull();
void runPong2PFull();
void runBlackjackSP();
void runBlackjack2P();
void runPokerSP();
void runPoker2P();

void drawVerticalGradient(uint16_t topColor, uint16_t bottomColor);
uint16_t lerpColor(uint16_t a, uint16_t b, float t);
void drawRoundedMenuCard();
void drawMenuRow(int i, bool selected);
void drawSelectorAt(int yCenter);
void redrawRowsOverlappingRect(int rectTop, int rectH);

// =====================================================================
//   SETUP
// =====================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("BLAZECORE Booting...");

    // --- Button Setup ---
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_START, INPUT_PULLUP);

    // --- TFT Setup ---
    tft.init();
    tft.setRotation(3); // <<< SETTING ROTATION TO 3 AS REQUESTED
    tft.fillScreen(TFT_BLACK);

    // Set text datum to Middle Center.
    tft.setTextDatum(MC_DATUM);
    tft.setSwapBytes(true);

    // initialize prev button states (read current)
    prevUpState = readButton(BTN_UP, WEB_BTN_UP_IDX);
    prevDownState = readButton(BTN_DOWN, WEB_BTN_DOWN_IDX);
    prevAState = readButton(BTN_A, WEB_BTN_A_IDX);
    prevStartState = readButton(BTN_START, WEB_BTN_START_IDX);
    prevSelectState = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    
    // --- Web Gamepad Setup ---
    Serial.println("Starting WiFi & Web Gamepad...");
    startWiFi();
    initWebServer();
    Serial.println("Web gamepad ready. Connect via browser.");
}

// =====================================================================
//   MAIN LOOP
// =====================================================================
void loop() {
    // The main state machine
    switch (currentState) {
        case STATE_SPLASH:
            loopSplash();
            break;
        case STATE_MENU:
            loopMenu();
            break;
        case STATE_GAME_SNAKE:
            runSnakeFull();
            break;
        case STATE_GAME_PONG:
            runPongFull();
            break;
        case STATE_GAME_FLAPPY:
            runFlappyFull();
            break;
        case STATE_GAME_GEOMETRY:
            runGeometryFull();
            break;
        case STATE_GAME_PACMAN:
            runPacmanFull();
            break;
        case STATE_GAME_TETRIS:
            runTetrisFull();
            break;
        case STATE_GAME_SNAKE2P:
            runSnake2PFull();
            break;
        case STATE_GAME_PONG2P:
            runPong2PFull();
            break;
        case STATE_GAME_BLACKJACK:
            runBlackjackSP();
            break;
        case STATE_GAME_BLACKJACK2P:
            runBlackjack2P();
            break;
        case STATE_GAME_POKER:
            runPokerSP();
            break;
        case STATE_GAME_POKER2P:
            runPoker2P();
            break;
    }
    
    // tiny delay to avoid 100% CPU tight loop
    delay(8); 
}

// =====================================================================
//   SPLASH SCREEN (ORIGINAL - restored exactly)
// =====================================================================
void loopSplash() {
    static unsigned long startTime = 0;
    static bool inited = false;

    // Use the global prevStartState for edge detection.
    // Make sure prevStartState is declared globally (you already have it).

    if (!inited) {
        startTime = millis();
        inited = true;
        needsRedraw = true;

        // Important: capture current START state so we don't immediately react to a stale press
        prevStartState = readButton(BTN_START, WEB_BTN_START_IDX);
    }

    if (needsRedraw) {
        // Fancy gradient background
        drawVerticalGradient(TFT_NAVY, TFT_BLACK);

        // Big logo block (center)
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(6);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("BLAZE", tft.width() / 2, tft.height() / 2 - 40);
        tft.setTextSize(4);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("CORE", tft.width() / 2, tft.height() / 2 + 6);

        // Subtitle
        tft.setTextSize(1);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.drawString("Handheld Retro Console", tft.width() / 2, tft.height() / 2 + 40);

        needsRedraw = false;
    }

    // Pulsing "Press START" hint
    float t = (sin((millis() - startTime) / 350.0) + 1.0) / 2.0; // 0..1
    uint16_t hintColor = lerpColor(TFT_LIGHTGREY, TFT_WHITE, t);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(hintColor, TFT_BLACK);
    tft.drawString("Press START to continue", tft.width() / 2, tft.height() - 48);

    // Small animated progress bar under the hint
    int barW = 140;
    int barX = (tft.width() - barW) / 2;
    int barY = tft.height() - 36;
    int p = (millis() / 250) % (barW + 20);
    tft.drawRoundRect(barX - 2, barY - 2, barW + 4, 12, 6, TFT_DARKGREY);
    tft.fillRoundRect(barX, barY, barW, 8, 4, TFT_DARKGREY);
    int headW = 28;
    int head = p - headW;
    if (head < 0) head = 0;
    if (p > barW) p = barW;
    tft.fillRoundRect(barX + head, barY, p - head, 8, 4, accent);

    // Use edge-detect instead of raw check to prevent immediate re-trigger:
    int curStart = readButton(BTN_START, WEB_BTN_START_IDX);
    if (prevStartState == HIGH && curStart == LOW) {
        // Clean falling edge: go to menu
        currentState = STATE_MENU;
        needsRedraw = true;
        // reset init so next time splash enters cleanly
        inited = false;
        // small debounce guard
        delay(140);
    }
    prevStartState = curStart;
}

// =====================================================================
//   HELPERS (drawing + small optimized redraws)
// =====================================================================

// Linear interpolate between two 16-bit TFT colors (RGB565)
uint16_t lerpColor(uint16_t a, uint16_t b, float t) {
    uint8_t r1 = (a >> 11) & 0x1F;
    uint8_t g1 = (a >> 5) & 0x3F;
    uint8_t b1 = a & 0x1F;

    uint8_t r2 = (b >> 11) & 0x1F;
    uint8_t g2 = (b >> 5) & 0x3F;
    uint8_t b2 = b & 0x1F;

    uint8_t r = r1 + (r2 - r1) * t;
    uint8_t g = g1 + (g2 - g1) * t;
    uint8_t bb = b1 + (b2 - b1) * t;

    return (r << 11) | (g << 5) | bb;
}

void drawVerticalGradient(uint16_t topColor, uint16_t bottomColor) {
    int h = tft.height();
    for (int y = 0; y < h; y++) {
        float t = (float)y / (h - 1);
        tft.drawFastHLine(0, y, tft.width(), lerpColor(topColor, bottomColor, t));
    }
}

void drawRoundedMenuCard() {
    tft.fillRoundRect(menuX, menuY, menuW, menuH, 8, TFT_BLACK);
    tft.drawRoundRect(menuX, menuY, menuW, menuH, 8, TFT_DARKGREY);
    tft.fillRect(menuX + 8, menuY + 8, menuW - 16, 6, accent);
}

// Draw a single row (i). If selected==true we do NOT draw the selected-name (selector will draw it).
// ---------- REPLACEMENT: Scrolling menu (paste over old menu code) ----------

// How many rows visible in the menu window at once (fits your layout)
const int MENU_VISIBLE_ROWS = 4;
const int MENU_ROW_H = 34;

// Top-most index currently visible (0 .. max(0, numGames-MENU_VISIBLE_ROWS))
int menuTopIndex = 0;

// Draw a single menu row at visible slot idx (0..MENU_VISIBLE_ROWS-1) which corresponds to real index = menuTopIndex + idx
// Reuses existing drawMenuRow but we'll inline here to ensure consistent visuals
void drawMenuRowAtSlot(int slotIdx, bool isSelected) {
    int realIdx = menuTopIndex + slotIdx;
    int entryX = menuX + 12;
    int entryY = menuY + 26;
    int y = entryY + slotIdx * MENU_ROW_H;
    int icX = entryX;
    int icY = y;

    // Clear row area
    tft.fillRect(entryX - 8, y - 2, menuW - 20, 32, TFT_BLACK);

    // Icon area
    tft.fillRoundRect(icX, icY, 28, 28, 6, TFT_BLACK);
    tft.drawRoundRect(icX, icY, 28, 28, 6, TFT_DARKGREY);

    // Draw small icons for known slots (if realIdx exceeds numGames, leave empty)
    if (realIdx < numGames) {
        if (realIdx == 0) { // Snake
            tft.fillRect(icX + 6, icY + 6, 6, 6, TFT_GREEN);
            tft.fillRect(icX + 14, icY + 6, 6, 6, TFT_GREEN);
            tft.fillRect(icX + 6, icY + 14, 6, 6, TFT_GREEN);
        } else if (realIdx == 1) { // Pong
            tft.fillRect(icX + 4, icY + 6, 4, 16, TFT_WHITE);
            tft.fillCircle(icX + 20, icY + 14, 3, TFT_WHITE);
        } else if (realIdx == 2) { // Flappy Bird
            tft.fillTriangle(icX + 6, icY + 18, icX + 12, icY + 6, icX + 22, icY + 16, TFT_YELLOW);
        } else if (realIdx == 3) { // Geometry Clone
            tft.fillRect(icX + 10, icY + 10, 8, 8, TFT_CYAN);
            tft.drawRect(icX + 10, icY + 10, 8, 8, TFT_WHITE);
        } else if (realIdx == 4) { // Pac-man
            tft.fillCircle(icX + 14, icY + 14, 7, TFT_YELLOW);
            tft.fillTriangle(icX + 14, icY + 14, icX + 20, icY + 10, icX + 20, icY + 18, TFT_BLACK);
        } else if (realIdx == 5) { // Tetris
            tft.fillRect(icX + 8, icY + 8, 4, 4, TFT_MAGENTA);
            tft.fillRect(icX + 12, icY + 8, 4, 4, TFT_MAGENTA);
            tft.fillRect(icX + 16, icY + 8, 4, 4, TFT_MAGENTA);
            tft.fillRect(icX + 12, icY + 12, 4, 4, TFT_MAGENTA);
        } else if (realIdx == 6) { // Snake 2P
            tft.fillRect(icX + 4, icY + 6, 4, 4, TFT_GREEN);
            tft.fillRect(icX + 8, icY + 6, 4, 4, TFT_GREEN);
            tft.fillRect(icX + 16, icY + 18, 4, 4, TFT_BLUE);
            tft.fillRect(icX + 20, icY + 18, 4, 4, TFT_BLUE);
        } else if (realIdx == 7) { // Pong 2P
            tft.fillRect(icX + 4, icY + 8, 2, 12, TFT_WHITE);
            tft.fillRect(icX + 22, icY + 8, 2, 12, TFT_WHITE);
            tft.fillCircle(icX + 14, icY + 14, 2, TFT_WHITE);
        } else if (realIdx == 8) { // Blackjack
            tft.fillRoundRect(icX + 8, icY + 8, 12, 16, 2, TFT_WHITE);
            tft.setTextDatum(MC_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_RED);
            tft.drawString("A", icX + 14, icY + 15);
        } else if (realIdx == 9) { // Blackjack 2P
            tft.fillRoundRect(icX + 4, icY + 8, 10, 14, 2, TFT_WHITE);
            tft.fillRoundRect(icX + 16, icY + 8, 10, 14, 2, TFT_WHITE);
            tft.setTextDatum(MC_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_BLACK);
            tft.drawString("A", icX + 9, icY + 14);
            tft.setTextColor(TFT_RED);
            tft.drawString("K", icX + 21, icY + 14);
        } else if (realIdx == 10) { // Poker
            tft.fillRoundRect(icX + 6, icY + 6, 8, 12, 1, TFT_WHITE);
            tft.fillRoundRect(icX + 12, icY + 10, 8, 12, 1, TFT_WHITE);
            tft.setTextDatum(TL_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_RED);
            tft.drawString("Q", icX + 7, icY + 7);
        } else if (realIdx == 11) { // Poker 2P
            tft.fillRoundRect(icX + 4, icY + 6, 8, 11, 1, TFT_WHITE);
            tft.fillRoundRect(icX + 10, icY + 10, 8, 11, 1, TFT_WHITE);
            tft.fillRoundRect(icX + 16, icY + 6, 8, 11, 1, TFT_WHITE);
            tft.setTextDatum(TL_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_BLACK);
            tft.drawString("J", icX + 5, icY + 7);
            tft.setTextColor(TFT_RED);
            tft.drawString("Q", icX + 17, icY + 7);
        } else {
            tft.drawRect(icX + 6, icY + 6, 8, 8, TFT_CYAN);
            tft.drawCircle(icX + 22, icY + 14, 4, TFT_CYAN);
        }

        // Draw text: if this is selected we do NOT draw the text here because selector draws selected text with contrast
        if (!isSelected) {
            tft.setTextDatum(TL_DATUM);
            tft.setTextSize(2);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(gameNames[realIdx], entryX + 40, y + 6);
        }
    }
}

// Draw selector at slot (slot index 0..MENU_VISIBLE_ROWS-1)
void drawSelectorAtSlot(int slotIdx) {
    int entryX = menuX + 12;
    int selX = entryX - 6;
    int selW = menuW - 36;
    int selH = 30;
    int entryY = menuY + 26;
    int top = entryY + slotIdx * MENU_ROW_H - 4;

    // Draw selector background and border
    tft.fillRoundRect(selX, top, selW, selH, 6, accent);
    tft.drawRoundRect(selX, top, selW, selH, 6, TFT_WHITE);
    tft.fillTriangle(entryX - 2, top + 8, entryX + 6, top + 14, entryX - 2, top + 20, TFT_WHITE);

    // selected name in contrasting color
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK, accent);
    int centerX = selX + selW/2;
    int centerY = top + selH/2 - 1;
    int realIdx = menuTopIndex + slotIdx;
    if (realIdx < numGames) {
        tft.drawString(gameNames[realIdx], centerX, centerY);
    }
}

// Full draw (when entering menu or heavy redraw)
void drawMenu() {
    // background
    drawVerticalGradient(TFT_DARKGREY, TFT_BLACK);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("GAMES", tft.width() / 2, 26);

    drawRoundedMenuCard();

    // Clamp menuTopIndex
    int maxTop = max(0, numGames - MENU_VISIBLE_ROWS);
    if (menuTopIndex > maxTop) menuTopIndex = maxTop;
    if (menuTopIndex < 0) menuTopIndex = 0;

    // Draw visible rows
    for (int s = 0; s < MENU_VISIBLE_ROWS; ++s) {
        bool selected = (menuTopIndex + s == selectedGame);
        drawMenuRowAtSlot(s, selected);
    }

    // selector slot (center when possible)
    int selectorSlot = selectedGame - menuTopIndex;
    if (selectorSlot < 0) selectorSlot = 0;
    if (selectorSlot >= MENU_VISIBLE_ROWS) selectorSlot = MENU_VISIBLE_ROWS - 1;
    drawSelectorAtSlot(selectorSlot);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("A = Select   SELECT = Exit   START = Splash", tft.width() / 2, tft.height() - 14);

    menuInitialized = true;
    needsRedraw = false;
}

// Loop: handle navigation with scrolling and minimal redraws
void loopMenu() {
    // initial draw
    if (!menuInitialized || needsRedraw) { drawMenu(); return; }

    unsigned long now = millis();

    int curUp = readButton(BTN_UP, WEB_BTN_UP_IDX);
    int curDown = readButton(BTN_DOWN, WEB_BTN_DOWN_IDX);
    int curA = readButton(BTN_A, WEB_BTN_A_IDX);
    int curStart = readButton(BTN_START, WEB_BTN_START_IDX);

    bool navHappened = false;
    int oldSelected = selectedGame;
    int oldTop = menuTopIndex;

    // Edge detect UP
    if (prevUpState == HIGH && curUp == LOW) {
        if (now - lastNavMillis >= navDebounceMs) {
            selectedGame--;
            if (selectedGame < 0) selectedGame = numGames - 1;
            lastNavMillis = now;
            navHappened = true;
        }
    }

    // Edge detect DOWN
    if (prevDownState == HIGH && curDown == LOW) {
        if (now - lastNavMillis >= navDebounceMs) {
            selectedGame++;
            if (selectedGame >= numGames) selectedGame = 0;
            lastNavMillis = now;
            navHappened = true;
        }
    }

    prevUpState = curUp;
    prevDownState = curDown;

    // If selection moved, adjust menuTopIndex to keep selection visible (center when possible)
    if (navHappened) {
        int maxTop = max(0, numGames - MENU_VISIBLE_ROWS);
        // If possible, attempt to center selected item in the visible window
        int idealTop = selectedGame - (MENU_VISIBLE_ROWS / 2);
        if (idealTop < 0) idealTop = 0;
        if (idealTop > maxTop) idealTop = maxTop;

        menuTopIndex = idealTop;

        // If top changed, we need to redraw all visible rows
        if (menuTopIndex != oldTop) {
            for (int s = 0; s < MENU_VISIBLE_ROWS; ++s) {
                drawMenuRowAtSlot(s, (menuTopIndex + s == selectedGame));
            }
        } else {
            // Top unchanged: only two rows changed (oldSelected slot and newSelected slot)
            int oldSlot = oldSelected - menuTopIndex;
            int newSlot = selectedGame - menuTopIndex;
            if (oldSlot >= 0 && oldSlot < MENU_VISIBLE_ROWS) drawMenuRowAtSlot(oldSlot, false);
            if (newSlot >= 0 && newSlot < MENU_VISIBLE_ROWS) drawMenuRowAtSlot(newSlot, true);
        }

        // redraw selector at the new slot
        int selectorSlot = selectedGame - menuTopIndex;
        drawSelectorAtSlot(selectorSlot);
    }

    // A -> Enter selected game
    if (prevAState == HIGH && curA == LOW) {
        switch (selectedGame) {
            case 0: currentState = STATE_GAME_SNAKE; break;
            case 1: currentState = STATE_GAME_PONG; break;
            case 2: currentState = STATE_GAME_FLAPPY; break;
            case 3: currentState = STATE_GAME_GEOMETRY; break;
            case 4: currentState = STATE_GAME_PACMAN; break;
            case 5: currentState = STATE_GAME_TETRIS; break;
            case 6: currentState = STATE_GAME_SNAKE2P; break;
            case 7: currentState = STATE_GAME_PONG2P; break;
            case 8: currentState = STATE_GAME_BLACKJACK; break;
            case 9: currentState = STATE_GAME_BLACKJACK2P; break;
            case 10: currentState = STATE_GAME_POKER; break;
            case 11: currentState = STATE_GAME_POKER2P; break;
            default: currentState = STATE_GAME_SNAKE; break;
        }
        needsRedraw = true;
        menuInitialized = false;
        lastNavMillis = now;
    }
    prevAState = curA;

    // START -> back to splash (edge)
    if (prevStartState == HIGH && curStart == LOW) {
        currentState = STATE_SPLASH;
        needsRedraw = true;
        menuInitialized = false;
        lastNavMillis = now;
    }
    prevStartState = curStart;
}


// =====================================================================
//   GAME PLACEHOLDER FUNCTIONS (kept simple)
// =====================================================================

// ------------------- FULLSCREEN GAMES + RUNNERS (Snake, Pong, Flappy, Geometry) -------------------
// Paste this block into your sketch to replace the earlier game implementations.

// ------------------- GAME: SNAKE (Single Player) -------------------
namespace SnakeGame {
    const int COLS = 20;
    const int ROWS = 16;
    int cellW, cellH;
    int originX, originY;
    enum Dir { UP, RIGHT, DOWN, LEFT } dir;
    struct P { uint8_t x, y; };
    static P *snake = nullptr;
    static int snakeLen = 0;
    static P food;
    unsigned long lastMove = 0;
    unsigned long moveInterval = 120;
    bool alive = true;
    int score = 0;

    void alloc() { if (!snake) snake = (P*)malloc(sizeof(P) * COLS * ROWS); }

void computeLayout() {
        const int TOP_HUD_HEIGHT = 22; 
        const int BOTTOM_HUD_HEIGHT = 22; 
        int playW = tft.width() - 8;
        int playH = tft.height() - TOP_HUD_HEIGHT - BOTTOM_HUD_HEIGHT;
        cellW = playW / COLS;
        cellH = playH / ROWS;
        if (cellW < 4) cellW = 4;
        if (cellH < 4) cellH = 4;
        originX = (tft.width() - cellW * COLS) / 2;
        originY = TOP_HUD_HEIGHT + (playH - cellH * ROWS) / 2;
    }

    inline void drawCellAtGrid(int gx, int gy, uint16_t col) {
        int x = originX + gx*cellW + 1;
        int y = originY + gy*cellH + 1;
        tft.fillRect(x, y, cellW-2, cellH-2, col);
    }

    bool isBody(int x, int y) {
        for (int i = 0; i < snakeLen; ++i) if (snake[i].x == x && snake[i].y == y) return true;
        return false;
    }

    void placeFood() {
        int tries = 0;
        do {
            food.x = random(0, COLS);
            food.y = random(0, ROWS);
            tries++;
            if (tries > 2000) break;
        } while (isBody(food.x, food.y));
        drawCellAtGrid(food.x, food.y, TFT_RED);
    }

    void drawGridAndHUD() {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(originX - 1, originY - 1, cellW * COLS + 2, cellH * ROWS + 2, TFT_DARKGREY);
        
        for (int x = 0; x <= COLS; ++x) tft.drawFastVLine(originX + x*cellW, originY, cellH*ROWS, TFT_DARKGREY);
        for (int y = 0; y <= ROWS; ++y) tft.drawFastHLine(originX, originY + y*cellH, cellW*COLS, TFT_DARKGREY);
        
        tft.fillRect(4,4,140,18,TFT_BLACK); tft.drawRect(4,4,140,18,TFT_WHITE);
        tft.setTextDatum(ML_DATUM);
        tft.setTextSize(1); 
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        char buf[32]; sprintf(buf,"SNAKE  Score: %d", score);
        tft.drawString(buf,8,13);

        tft.fillRect(4, tft.height()-20, 160, 16, TFT_BLACK);
        tft.setTextDatum(ML_DATUM);
        tft.setTextSize(1); 
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.drawString("SELECT=Exit", 8, tft.height()-12);
    }

    void restart() {
        alloc();
        computeLayout();
        snakeLen = 5;
        for (int i = 0; i < snakeLen; ++i) { snake[i].x = 2 + snakeLen - 1 - i; snake[i].y = ROWS/2; }
        dir = RIGHT;
        alive = true;
        moveInterval = 120;
        score = 0;
        lastMove = millis();
        
        drawGridAndHUD();
        
        for (int i = 0; i < snakeLen; ++i) {
            drawCellAtGrid(snake[i].x, snake[i].y, (i==0)?TFT_YELLOW:TFT_GREEN);
        }
        placeFood();
    }

    void step() {
        if (!alive) return;
        unsigned long now = millis();
        if (now - lastMove < moveInterval) return;
        lastMove = now;

        P newHead = snake[0];
        if (dir == UP) { newHead.y = (newHead.y == 0) ? (ROWS - 1) : (newHead.y - 1); }
        else if (dir == DOWN) { newHead.y = (newHead.y + 1) % ROWS; }
        else if (dir == LEFT) { newHead.x = (newHead.x == 0) ? (COLS - 1) : (newHead.x - 1); }
        else if (dir == RIGHT) { newHead.x = (newHead.x + 1) % COLS; }

        for (int i = 0; i < snakeLen; ++i) if (snake[i].x == newHead.x && snake[i].y == newHead.y) { alive=false; return; }

        bool ate = (newHead.x == food.x && newHead.y == food.y);

        int oldTailX = snake[snakeLen-1].x, oldTailY = snake[snakeLen-1].y;
        
        if (!ate) {
            drawCellAtGrid(oldTailX, oldTailY, TFT_BLACK);
        }

        for (int i = snakeLen - 1; i > 0; --i) snake[i] = snake[i-1];
        snake[0] = newHead;

        if (ate) {
            if (snakeLen < COLS*ROWS - 1) {
                snake[snakeLen].x = oldTailX;
                snake[snakeLen].y = oldTailY;
                snakeLen++;
            }
            score++;
            if (moveInterval > 40) moveInterval -= 4;
            
            placeFood();

            tft.fillRect(4,4,140,18,TFT_BLACK); tft.drawRect(4,4,140,18,TFT_WHITE);
            tft.setTextDatum(ML_DATUM); 
            tft.setTextSize(1); 
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            char buf[32]; sprintf(buf,"SNAKE  Score: %d", score); 
            tft.drawString(buf,8,13);
        }

        drawCellAtGrid(snake[0].x, snake[0].y, TFT_YELLOW);
        if (snakeLen > 1) drawCellAtGrid(snake[1].x, snake[1].y, TFT_GREEN);
    }
}

// Runner
void runSnakeFull() {
static bool inited = false;
    static int prevUp = HIGH, prevDown = HIGH, prevLeft = HIGH, prevRight = HIGH;
    if (!inited || needsRedraw) {
        SnakeGame::restart();
        needsRedraw = false;
        inited = true;
        prevUp = readButton(BTN_UP, WEB_BTN_UP_IDX); 
        prevDown = readButton(BTN_DOWN, WEB_BTN_DOWN_IDX);
        prevLeft = readButton(BTN_LEFT, WEB_BTN_LEFT_IDX); 
        prevRight = readButton(BTN_RIGHT, WEB_BTN_RIGHT_IDX);
    }

    int curUp = readButton(BTN_UP, WEB_BTN_UP_IDX);
    int curDown = readButton(BTN_DOWN, WEB_BTN_DOWN_IDX);
    int curLeft = readButton(BTN_LEFT, WEB_BTN_LEFT_IDX);
    int curRight = readButton(BTN_RIGHT, WEB_BTN_RIGHT_IDX);
    if (prevUp == HIGH && curUp == LOW && SnakeGame::dir != SnakeGame::DOWN) SnakeGame::dir = SnakeGame::UP;
    if (prevDown == HIGH && curDown == LOW && SnakeGame::dir != SnakeGame::UP) SnakeGame::dir = SnakeGame::DOWN;
    if (prevLeft == HIGH && curLeft == LOW && SnakeGame::dir != SnakeGame::RIGHT) SnakeGame::dir = SnakeGame::LEFT;
    if (prevRight == HIGH && curRight == LOW && SnakeGame::dir != SnakeGame::LEFT) SnakeGame::dir = SnakeGame::RIGHT;
    prevUp = curUp; prevDown = curDown; prevLeft = curLeft; prevRight = curRight;

    static int prevSel = HIGH;
    int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) {
        currentState = STATE_MENU; needsRedraw = true; menuInitialized = false; delay(120); inited = false; prevSel = curSel; return;
    }
    prevSel = curSel;

    SnakeGame::step();
}

// ------------------- GAME: SNAKE 2 PLAYER -------------------
// Player 1: Physical buttons - Green snake
// Player 2: Web buttons - Yellow snake
namespace Snake2PGame {
    const int COLS = 20;
    const int ROWS = 16;
    int cellW, cellH;
    int originX, originY;
    
    enum Dir { UP, RIGHT, DOWN, LEFT };
    struct P { uint8_t x, y; };
    
    static P *snake1 = nullptr;
    static P *snake2 = nullptr;
    int snake1Len = 0;
    int snake2Len = 0;
    Dir dir1, dir2;
    unsigned long lastMove1 = 0, lastMove2 = 0;
    int score1 = 0, score2 = 0;
    bool alive1 = true, alive2 = true;
    
    static P food;
    unsigned long moveInterval = 120;
    bool gameOver = false;

    void alloc() { 
        if (!snake1) snake1 = (P*)malloc(sizeof(P) * COLS * ROWS); 
        if (!snake2) snake2 = (P*)malloc(sizeof(P) * COLS * ROWS);
    }

    void computeLayout() {
        const int TOP_HUD_HEIGHT = 22; 
        const int BOTTOM_HUD_HEIGHT = 22; 
        int playW = tft.width() - 8;
        int playH = tft.height() - TOP_HUD_HEIGHT - BOTTOM_HUD_HEIGHT;
        cellW = playW / COLS;
        cellH = playH / ROWS;
        if (cellW < 4) cellW = 4;
        if (cellH < 4) cellH = 4;
        originX = (tft.width() - cellW * COLS) / 2;
        originY = TOP_HUD_HEIGHT + (playH - cellH * ROWS) / 2;
    }

    inline void drawCellAtGrid(int gx, int gy, uint16_t col) {
        int x = originX + gx*cellW + 1;
        int y = originY + gy*cellH + 1;
        tft.fillRect(x, y, cellW-2, cellH-2, col);
    }

    bool isBody1(int x, int y) {
        for (int i = 0; i < snake1Len; ++i) if (snake1[i].x == x && snake1[i].y == y) return true;
        return false;
    }
    
    bool isBody2(int x, int y) {
        for (int i = 0; i < snake2Len; ++i) if (snake2[i].x == x && snake2[i].y == y) return true;
        return false;
    }

    void placeFood() {
        int tries = 0;
        do {
            food.x = random(0, COLS);
            food.y = random(0, ROWS);
            tries++;
            if (tries > 2000) break;
        } while (isBody1(food.x, food.y) || isBody2(food.x, food.y));
        drawCellAtGrid(food.x, food.y, TFT_RED);
    }

    void drawGridAndHUD() {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(originX - 1, originY - 1, cellW * COLS + 2, cellH * ROWS + 2, TFT_DARKGREY);
        
        for (int x = 0; x <= COLS; ++x) tft.drawFastVLine(originX + x*cellW, originY, cellH*ROWS, TFT_DARKGREY);
        for (int y = 0; y <= ROWS; ++y) tft.drawFastHLine(originX, originY + y*cellH, cellW*COLS, TFT_DARKGREY);
        
        tft.fillRect(4,4,180,18,TFT_BLACK); tft.drawRect(4,4,180,18,TFT_WHITE);
        tft.setTextDatum(ML_DATUM);
        tft.setTextSize(1); 
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        char buf[32]; sprintf(buf,"P1:%d", score1);
        tft.drawString(buf,8,13);
        
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        sprintf(buf," P2:%d", score2);
        tft.drawString(buf,90,13);

        tft.fillRect(4, tft.height()-20, 160, 16, TFT_BLACK);
        tft.setTextDatum(ML_DATUM);
        tft.setTextSize(1); 
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.drawString("SELECT=Exit", 8, tft.height()-12);
    }

    void restart() {
        alloc();
        computeLayout();
        
        // Player 1 (left side) - Green
        snake1Len = 4;
        for (int i = 0; i < snake1Len; ++i) { 
            snake1[i].x = 2 + snake1Len - 1 - i; 
            snake1[i].y = ROWS/2 - 2; 
        }
        dir1 = RIGHT;
        score1 = 0;
        alive1 = true;
        lastMove1 = millis();
        
        // Player 2 (right side) - Yellow
        snake2Len = 4;
        for (int i = 0; i < snake2Len; ++i) { 
            snake2[i].x = COLS - 3 - snake2Len + 1 + i; 
            snake2[i].y = ROWS/2 + 2; 
        }
        dir2 = LEFT;
        score2 = 0;
        alive2 = true;
        lastMove2 = millis();
        
        moveInterval = 150;
        gameOver = false;
        
        drawGridAndHUD();
        
        // Draw both snakes
        for (int i = 0; i < snake1Len; ++i) {
            drawCellAtGrid(snake1[i].x, snake1[i].y, TFT_GREEN);
        }
        for (int i = 0; i < snake2Len; ++i) {
            drawCellAtGrid(snake2[i].x, snake2[i].y, TFT_YELLOW);
        }
        
        placeFood();
    }

    void moveSnake(P *snake, int &len, Dir &dir, unsigned long &lastMove, int &score, bool &alive, uint16_t color) {
        unsigned long now = millis();
        if (now - lastMove < moveInterval) return;
        lastMove = now;
        
        if (!alive) return;

        P newHead = snake[0];
        if (dir == UP) { newHead.y = (newHead.y == 0) ? (ROWS - 1) : (newHead.y - 1); }
        else if (dir == DOWN) { newHead.y = (newHead.y + 1) % ROWS; }
        else if (dir == LEFT) { newHead.x = (newHead.x == 0) ? (COLS - 1) : (newHead.x - 1); }
        else if (dir == RIGHT) { newHead.x = (newHead.x + 1) % COLS; }

        // Check collision with self
        for (int i = 0; i < len; ++i) {
            if (snake[i].x == newHead.x && snake[i].y == newHead.y) { 
                alive = false; 
                gameOver = true;
                return; 
            }
        }
        
        // Check collision with other snake
        if (snake == snake1) {
            for (int i = 0; i < snake2Len; ++i) {
                if (snake2[i].x == newHead.x && snake2[i].y == newHead.y) { 
                    alive = false; 
                    gameOver = true;
                    return; 
                }
            }
        } else {
            for (int i = 0; i < snake1Len; ++i) {
                if (snake1[i].x == newHead.x && snake1[i].y == newHead.y) { 
                    alive = false; 
                    gameOver = true;
                    return; 
                }
            }
        }

        bool ate = (newHead.x == food.x && newHead.y == food.y);
        int oldTailX = snake[len-1].x, oldTailY = snake[len-1].y;
        
        if (!ate) {
            drawCellAtGrid(oldTailX, oldTailY, TFT_BLACK);
        }

        for (int i = len - 1; i > 0; --i) snake[i] = snake[i-1];
        snake[0] = newHead;

        if (ate) {
            if (len < COLS*ROWS/2 - 1) {
                snake[len].x = oldTailX;
                snake[len].y = oldTailY;
                len++;
            }
            score++;
            
            placeFood();

            // Update HUD
            tft.fillRect(4,4,180,18,TFT_BLACK); tft.drawRect(4,4,180,18,TFT_WHITE);
            tft.setTextDatum(ML_DATUM); 
            tft.setTextSize(1); 
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            char buf[32]; sprintf(buf,"P1:%d", score1); 
            tft.drawString(buf,8,13);
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            sprintf(buf," P2:%d", score2);
            tft.drawString(buf,90,13);
        }

        drawCellAtGrid(snake[0].x, snake[0].y, color);
    }

    void step() {
        if (gameOver) return;
        
        moveSnake(snake1, snake1Len, dir1, lastMove1, score1, alive1, TFT_GREEN);
        moveSnake(snake2, snake2Len, dir2, lastMove2, score2, alive2, TFT_YELLOW);
        
        if (!alive1 || !alive2) {
            gameOver = true;
            delay(1000);
            
            // Show winner
            tft.fillRect(tft.width()/2 - 80, tft.height()/2 - 40, 160, 80, TFT_BLACK);
            tft.drawRect(tft.width()/2 - 80, tft.height()/2 - 40, 160, 80, TFT_WHITE);
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(2);
            
            if (!alive1 && !alive2) {
                tft.setTextColor(TFT_ORANGE);
                tft.drawString("TIE!", tft.width()/2, tft.height()/2 - 10);
            } else if (!alive1) {
                tft.setTextColor(TFT_YELLOW);
                tft.drawString("PLAYER 2", tft.width()/2, tft.height()/2 - 10);
                tft.setTextColor(TFT_WHITE);
                tft.setTextSize(1);
                tft.drawString("WINS!", tft.width()/2, tft.height()/2 + 15);
            } else {
                tft.setTextColor(TFT_GREEN);
                tft.drawString("PLAYER 1", tft.width()/2, tft.height()/2 - 10);
                tft.setTextColor(TFT_WHITE);
                tft.setTextSize(1);
                tft.drawString("WINS!", tft.width()/2, tft.height()/2 + 15);
            }
        }
    }
}

// Runner for 2P Snake
void runSnake2PFull() {
    static bool inited = false;
    static int prevUp1 = HIGH, prevDown1 = HIGH, prevLeft1 = HIGH, prevRight1 = HIGH;
    static int prevUp2 = HIGH, prevDown2 = HIGH, prevLeft2 = HIGH, prevRight2 = HIGH;
    
    if (!inited || needsRedraw) {
        Snake2PGame::restart();
        needsRedraw = false;
        inited = true;
        
        // Read initial states
        prevUp1 = digitalRead(BTN_UP);
        prevDown1 = digitalRead(BTN_DOWN);
        prevLeft1 = digitalRead(BTN_LEFT);
        prevRight1 = digitalRead(BTN_RIGHT);
        
        prevUp2 = getWebButtonState(WEB_BTN_UP_IDX);
        prevDown2 = getWebButtonState(WEB_BTN_DOWN_IDX);
        prevLeft2 = getWebButtonState(WEB_BTN_LEFT_IDX);
        prevRight2 = getWebButtonState(WEB_BTN_RIGHT_IDX);
    }

    // Player 1 controls (Physical buttons only)
    int curUp1 = digitalRead(BTN_UP);
    int curDown1 = digitalRead(BTN_DOWN);
    int curLeft1 = digitalRead(BTN_LEFT);
    int curRight1 = digitalRead(BTN_RIGHT);
    
    if (prevUp1 == HIGH && curUp1 == LOW && Snake2PGame::dir1 != Snake2PGame::DOWN) Snake2PGame::dir1 = Snake2PGame::UP;
    if (prevDown1 == HIGH && curDown1 == LOW && Snake2PGame::dir1 != Snake2PGame::UP) Snake2PGame::dir1 = Snake2PGame::DOWN;
    if (prevLeft1 == HIGH && curLeft1 == LOW && Snake2PGame::dir1 != Snake2PGame::RIGHT) Snake2PGame::dir1 = Snake2PGame::LEFT;
    if (prevRight1 == HIGH && curRight1 == LOW && Snake2PGame::dir1 != Snake2PGame::LEFT) Snake2PGame::dir1 = Snake2PGame::RIGHT;
    
    prevUp1 = curUp1; prevDown1 = curDown1; prevLeft1 = curLeft1; prevRight1 = curRight1;

    // Player 2 controls (Web buttons only)
    int curUp2 = getWebButtonState(WEB_BTN_UP_IDX);
    int curDown2 = getWebButtonState(WEB_BTN_DOWN_IDX);
    int curLeft2 = getWebButtonState(WEB_BTN_LEFT_IDX);
    int curRight2 = getWebButtonState(WEB_BTN_RIGHT_IDX);
    
    if (prevUp2 == 0 && curUp2 == 1 && Snake2PGame::dir2 != Snake2PGame::DOWN) Snake2PGame::dir2 = Snake2PGame::UP;
    if (prevDown2 == 0 && curDown2 == 1 && Snake2PGame::dir2 != Snake2PGame::UP) Snake2PGame::dir2 = Snake2PGame::DOWN;
    if (prevLeft2 == 0 && curLeft2 == 1 && Snake2PGame::dir2 != Snake2PGame::RIGHT) Snake2PGame::dir2 = Snake2PGame::LEFT;
    if (prevRight2 == 0 && curRight2 == 1 && Snake2PGame::dir2 != Snake2PGame::LEFT) Snake2PGame::dir2 = Snake2PGame::RIGHT;
    
    prevUp2 = curUp2; prevDown2 = curDown2; prevLeft2 = curLeft2; prevRight2 = curRight2;

    // Exit with SELECT
    static int prevSel = HIGH;
    int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) {
        currentState = STATE_MENU;
        needsRedraw = true;
        menuInitialized = false;
        delay(120);
        inited = false;
        prevSel = curSel;
        return;
    }
    prevSel = curSel;

    Snake2PGame::step();
}


// ------------------- PONG (optimized) -------------------
namespace PongGame {
    int paddleH;
    int leftY, rightY;
    float ballX, ballY, ballVX, ballVY;
    unsigned long lastStep = 0;
    const unsigned long stepMs = 16;
    int scoreL = 0, scoreR = 0;
    
    // --- FIX: Define the 'ceiling' (bottom of the score area) ---
    const int HUD_TOP_Y = 20; // Score is in a box from y=0 to y=18. 20 is safe.

    // previous positions for minimal erase
    int prevLeftY, prevRightY, prevBallX, prevBallY;

    void restart() {
        paddleH = max(20, tft.height() / 8);
        leftY = tft.height() / 2; rightY = leftY;
        ballX = tft.width() / 2; ballY = tft.height() / 2;
        ballVX = (random(0,2) ? 2.2f : -2.2f);
        ballVY = (random(-120,120) / 100.0f) * 1.2f;
        lastStep = millis();
        scoreL = scoreR = 0;
        // initial draw
        tft.fillScreen(TFT_BLACK);
        // Draw center line *below* the HUD
        for (int y = HUD_TOP_Y; y < tft.height(); y += 12) tft.drawFastVLine(tft.width()/2, y, 6, TFT_DARKGREY);
        
        prevLeftY = leftY; prevRightY = rightY;
        prevBallX = (int)ballX; prevBallY = (int)ballY;
        tft.fillRoundRect(6, leftY - paddleH/2, 8, paddleH, 4, TFT_WHITE);
        tft.fillRoundRect(tft.width()-14, rightY - paddleH/2, 8, paddleH, 4, TFT_LIGHTGREY);
        tft.fillCircle(prevBallX, prevBallY, 5, TFT_ORANGE);
        
        // HUD
        tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_WHITE,TFT_BLACK);
        char b[32]; sprintf(b,"%d  -  %d", scoreL, scoreR);
        tft.drawString(b,tft.width()/2, 10); // y=10 is correct
    }

    void step() {
        unsigned long now = millis();
        if (now - lastStep < stepMs) return;
        lastStep = now;

        // --- Input (Player) ---
        if (readButton(BTN_UP, WEB_BTN_UP_IDX) == LOW) leftY -= 5;
        if (readButton(BTN_DOWN, WEB_BTN_DOWN_IDX) == LOW) leftY += 5;
        // --- FIX: Constrain paddle below HUD ---
        leftY = constrain(leftY, HUD_TOP_Y + paddleH/2, tft.height() - paddleH/2);

        // --- AI ---
        if (ballY < rightY - 6) rightY -= 3; else if (ballY > rightY + 6) rightY += 3;
        // --- FIX: Constrain paddle below HUD ---
        rightY = constrain(rightY, HUD_TOP_Y + paddleH/2, tft.height() - paddleH/2);

        // --- Move ball ---
        ballX += ballVX; ballY += ballVY;
        // --- FIX: Bounce ball off bottom of HUD ---
        if (ballY <= HUD_TOP_Y || ballY >= tft.height() - 6) {
            ballY = max(ballY, (float)HUD_TOP_Y); // prevent getting stuck
            ballVY = -ballVY;
        }

        // --- Paddle collisions (Classic Pong reflection) ---
        if (ballX <= 14 && ballX >= 6 && abs(ballY - leftY) <= paddleH/2 + 5 && ballVX < 0) {
            ballVX = -ballVX;
            ballX = 16;
            // Classic angle change based on hit position
            float hitPos = (ballY - leftY) / (paddleH/2.0f);
            ballVY = hitPos * 3.0f;
        }
        if (ballX >= tft.width()-14 && ballX <= tft.width()-6 && abs(ballY - rightY) <= paddleH/2 + 5 && ballVX > 0) {
            ballVX = -ballVX;
            ballX = tft.width()-16;
            // Classic angle change based on hit position
            float hitPos = (ballY - rightY) / (paddleH/2.0f);
            ballVY = hitPos * 3.0f;
        }

        // --- Check for scoring ---
        bool scored = false;
        if (ballX < 0) { scoreR++; scored = true; }
        if (ballX > tft.width()) { scoreL++; scored = true; }

        if (scored) {
            // Update score display
            tft.fillRect(0,0,tft.width(),18,TFT_BLACK);
            tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_WHITE,TFT_BLACK);
            char bb[32]; sprintf(bb,"%d  -  %d", scoreL, scoreR);
            tft.drawString(bb,tft.width()/2, 10);
            
            delay(500); // Pause to show score

            // Reset ball
            ballX = tft.width() / 2; ballY = tft.height() / 2;
            ballVX = (random(0,2) ? 2.2f : -2.2f);
            ballVY = (random(-120,120) / 100.0f) * 1.2f;
            
            tft.fillCircle(prevBallX, prevBallY, 6, TFT_BLACK); // Erase old ball
            // Redraw center line
            for (int y = HUD_TOP_Y; y < tft.height(); y += 12) tft.drawFastVLine(tft.width()/2, y, 6, TFT_DARKGREY);
            return;
        }

        // --- Erase previous frame ---
        tft.fillRoundRect(6, prevLeftY - paddleH/2, 8, paddleH, 4, TFT_BLACK);
        tft.fillRoundRect(tft.width()-14, prevRightY - paddleH/2, 8, paddleH, 4, TFT_BLACK);
        tft.fillCircle(prevBallX, prevBallY, 6, TFT_BLACK);
        
        // Redraw center line (only where erased)
        // A bit inefficient, but simple.
        for (int y = HUD_TOP_Y; y < tft.height(); y += 12) {
             // Only redraw if the ball or paddle might have erased it
            if (abs(tft.width()/2 - prevBallX) < 10 || 
                abs(tft.width()/2 - 10) < 10 || 
                abs(tft.width()/2 - (tft.width()-10)) < 10) 
            {
                tft.drawFastVLine(tft.width()/2, y, 6, TFT_DARKGREY);
            }
        }
        // This is safer/simpler:
        // for (int y = HUD_TOP_Y; y < tft.height(); y += 12) tft.drawFastVLine(tft.width()/2, y, 6, TFT_DARKGREY);


        // --- Draw new frame ---
        tft.fillRoundRect(6, leftY - paddleH/2, 8, paddleH, 4, TFT_WHITE);
        tft.fillRoundRect(tft.width()-14, rightY - paddleH/2, 8, paddleH, 4, TFT_LIGHTGREY);
        int bX = (int)ballX, bY = (int)ballY;
        tft.fillCircle(bX, bY, 5, TFT_ORANGE);

        // HUD update (this is redundant if no one scored, but safe)
        tft.fillRect(0,0,tft.width(),18,TFT_BLACK);
        tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_WHITE,TFT_BLACK);
        char bb[32]; sprintf(bb,"%d  -  %d", scoreL, scoreR);
        tft.drawString(bb,tft.width()/2, 10); 

        // save prevs
        prevLeftY = leftY; prevRightY = rightY; prevBallX = bX; prevBallY = bY;
    }
}
void runPongFull() {
    static bool inited = false;
    if (!inited || needsRedraw) { PongGame::restart(); PongGame::step(); needsRedraw=false; inited=true; }
    static int prevSel = HIGH;
    int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) { currentState=STATE_MENU; needsRedraw=true; menuInitialized=false; delay(120); inited=false; prevSel=curSel; return; }
    prevSel = curSel;
    PongGame::step();
}


// ------------------- PONG 2 PLAYER -------------------
// Player 1 (Left): Physical buttons
// Player 2 (Right): Web buttons
namespace Pong2PGame {
    int paddleH;
    int leftY, rightY;
    float ballX, ballY, ballVX, ballVY;
    unsigned long lastStep = 0;
    const unsigned long stepMs = 16;
    int scoreL = 0, scoreR = 0;
    bool gameOver = false;
    const int winScore = 5;
    
    const int HUD_TOP_Y = 20;
    int prevLeftY, prevRightY, prevBallX, prevBallY;

    void restart() {
        paddleH = max(20, tft.height() / 8);
        leftY = tft.height() / 2; rightY = leftY;
        ballX = tft.width() / 2; ballY = tft.height() / 2;
        ballVX = (random(0,2) ? 2.2f : -2.2f);
        ballVY = (random(-120,120) / 100.0f) * 1.2f;
        lastStep = millis();
        scoreL = scoreR = 0;
        gameOver = false;
        
        tft.fillScreen(TFT_BLACK);
        for (int y = HUD_TOP_Y; y < tft.height(); y += 12) tft.drawFastVLine(tft.width()/2, y, 6, TFT_DARKGREY);
        
        prevLeftY = leftY; prevRightY = rightY;
        prevBallX = (int)ballX; prevBallY = (int)ballY;
        tft.fillRoundRect(6, leftY - paddleH/2, 8, paddleH, 4, TFT_GREEN);
        tft.fillRoundRect(tft.width()-14, rightY - paddleH/2, 8, paddleH, 4, TFT_YELLOW);
        tft.fillCircle(prevBallX, prevBallY, 5, TFT_ORANGE);
        
        tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_WHITE,TFT_BLACK);
        char b[32]; sprintf(b,"%d  -  %d", scoreL, scoreR);
        tft.drawString(b,tft.width()/2, 10);
    }

    void step() {
        if (gameOver) return;
        
        unsigned long now = millis();
        if (now - lastStep < stepMs) return;
        lastStep = now;

        // Player 1 (Left paddle) - Physical buttons only
        if (digitalRead(BTN_UP) == LOW) leftY -= 5;
        if (digitalRead(BTN_DOWN) == LOW) leftY += 5;
        leftY = constrain(leftY, HUD_TOP_Y + paddleH/2, tft.height() - paddleH/2);

        // Player 2 (Right paddle) - Web buttons only
        if (getWebButtonState(WEB_BTN_UP_IDX) == 1) rightY -= 5;
        if (getWebButtonState(WEB_BTN_DOWN_IDX) == 1) rightY += 5;
        rightY = constrain(rightY, HUD_TOP_Y + paddleH/2, tft.height() - paddleH/2);

        ballX += ballVX; ballY += ballVY;
        
        if (ballY <= HUD_TOP_Y || ballY >= tft.height() - 6) {
            ballY = max(ballY, (float)HUD_TOP_Y);
            ballVY = -ballVY;
        }

        // Paddle collisions (Classic Pong reflection)
        if (ballX <= 14 && ballX >= 6 && abs(ballY - leftY) <= paddleH/2 + 5 && ballVX < 0) {
            ballVX = -ballVX;
            ballX = 16;
            float hitPos = (ballY - leftY) / (paddleH/2.0f);
            ballVY = hitPos * 3.0f;
        }
        if (ballX >= tft.width()-14 && ballX <= tft.width()-6 && abs(ballY - rightY) <= paddleH/2 + 5 && ballVX > 0) {
            ballVX = -ballVX;
            ballX = tft.width()-16;
            float hitPos = (ballY - rightY) / (paddleH/2.0f);
            ballVY = hitPos * 3.0f;
        }

        bool scored = false;
        if (ballX < 0) { scoreR++; scored = true; }
        if (ballX > tft.width()) { scoreL++; scored = true; }

        if (scored) {
            tft.fillRect(0,0,tft.width(),18,TFT_BLACK);
            tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_WHITE,TFT_BLACK);
            char bb[32]; sprintf(bb,"%d  -  %d", scoreL, scoreR);
            tft.drawString(bb,tft.width()/2, 10);
            
            // Check for winner
            if (scoreL >= winScore || scoreR >= winScore) {
                gameOver = true;
                delay(1000);
                
                tft.fillRect(tft.width()/2 - 90, tft.height()/2 - 50, 180, 100, TFT_BLACK);
                tft.drawRect(tft.width()/2 - 90, tft.height()/2 - 50, 180, 100, TFT_WHITE);
                tft.setTextDatum(MC_DATUM);
                tft.setTextSize(2);
                
                if (scoreL >= winScore) {
                    tft.setTextColor(TFT_GREEN);
                    tft.drawString("PLAYER 1", tft.width()/2, tft.height()/2 - 20);
                } else {
                    tft.setTextColor(TFT_YELLOW);
                    tft.drawString("PLAYER 2", tft.width()/2, tft.height()/2 - 20);
                }
                
                tft.setTextColor(TFT_WHITE);
                tft.setTextSize(1);
                tft.drawString("WINS!", tft.width()/2, tft.height()/2 + 10);
                sprintf(bb,"Final: %d - %d", scoreL, scoreR);
                tft.drawString(bb, tft.width()/2, tft.height()/2 + 30);
                return;
            }
            
            delay(500);

            ballX = tft.width() / 2; ballY = tft.height() / 2;
            ballVX = (random(0,2) ? 2.2f : -2.2f);
            ballVY = (random(-120,120) / 100.0f) * 1.2f;
            
            tft.fillCircle(prevBallX, prevBallY, 6, TFT_BLACK);
            for (int y = HUD_TOP_Y; y < tft.height(); y += 12) tft.drawFastVLine(tft.width()/2, y, 6, TFT_DARKGREY);
            return;
        }

        // Erase previous frame
        tft.fillRoundRect(6, prevLeftY - paddleH/2, 8, paddleH, 4, TFT_BLACK);
        tft.fillRoundRect(tft.width()-14, prevRightY - paddleH/2, 8, paddleH, 4, TFT_BLACK);
        tft.fillCircle(prevBallX, prevBallY, 6, TFT_BLACK);
        
        for (int y = HUD_TOP_Y; y < tft.height(); y += 12) {
            if (abs(tft.width()/2 - prevBallX) < 10 || 
                abs(tft.width()/2 - 10) < 10 || 
                abs(tft.width()/2 - (tft.width()-10)) < 10) 
            {
                tft.drawFastVLine(tft.width()/2, y, 6, TFT_DARKGREY);
            }
        }

        // Draw new frame
        tft.fillRoundRect(6, leftY - paddleH/2, 8, paddleH, 4, TFT_GREEN);
        tft.fillRoundRect(tft.width()-14, rightY - paddleH/2, 8, paddleH, 4, TFT_YELLOW);
        int bX = (int)ballX, bY = (int)ballY;
        tft.fillCircle(bX, bY, 5, TFT_ORANGE);

        tft.fillRect(0,0,tft.width(),18,TFT_BLACK);
        tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_WHITE,TFT_BLACK);
        char bb[32]; sprintf(bb,"%d  -  %d", scoreL, scoreR);
        tft.drawString(bb,tft.width()/2, 10);

        prevLeftY = leftY; prevRightY = rightY; prevBallX = bX; prevBallY = bY;
    }
}

void runPong2PFull() {
    static bool inited = false;
    
    if (!inited || needsRedraw) { 
        Pong2PGame::restart(); 
        Pong2PGame::step(); 
        needsRedraw = false; 
        inited = true; 
    }
    
    static int prevSel = HIGH;
    int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) { 
        currentState = STATE_MENU; 
        needsRedraw = true; 
        menuInitialized = false; 
        delay(120); 
        inited = false; 
        prevSel = curSel; 
        return; 
    }
    prevSel = curSel;
    
    Pong2PGame::step();
}


// ------------------- FLAPPY (optimized) -------------------
namespace FlappyGame {
    float bx, by, vy;
    const float gravity = 0.12f;
    const float flapImpulse = -3.2f;
    unsigned long lastStep = 0;
    const unsigned long stepMs = 20;

    struct Pipe { int x; int gapY; bool passed; int prevX; };
    const int numPipes = 3;
    Pipe pipes[numPipes];
    int pipeSpacing;
    const int pipeW = 32;
    const int pipeGapH = 100;

    int score = 0;
    bool alive = true;
    int prevScore = -1;

    enum State { RUNNING, GAMEOVER_WAIT_PRESS } ;
    State state = RUNNING;

    int prevBirdX = -999, prevBirdY = -999;
    const int birdW = 12, birdH = 10;

    bool flapButtonDown = false;

    // Softer colors
    const uint16_t SKY_COLOR = 0x6D9E;     // Soft sky blue
    const uint16_t GROUND_COLOR = 0xA2C5;  // Muted brown
    const uint16_t GRASS_COLOR = 0x5463;   // Soft green
    const uint16_t PIPE_COLOR = 0x4AC8;    // Darker muted green
    const uint16_t BIRD_COLOR = 0xFE60;    // Soft yellow/orange

    void drawGround() {
        tft.fillRect(0, tft.height()-20, tft.width(), 20, GROUND_COLOR);
        for (int i = 0; i < tft.width(); i += 4) tft.fillRect(i, tft.height()-22, 2, 2, GRASS_COLOR);
    }

    void drawPipe(int x, int gapY, uint16_t color) {
        int groundY = tft.height() - 20;
        int topH = gapY - 20;
        int botH = groundY - (gapY + pipeGapH);
        if (x + pipeW < 0 || x > tft.width()) return;
        if (topH > 0) { tft.fillRect(x, 20, pipeW, topH, color); tft.fillRoundRect(x - 2, 20 + max(0, topH - 6), pipeW + 4, 10, 3, color); }
        if (botH > 0) { tft.fillRect(x, gapY + pipeGapH, pipeW, botH, color); tft.fillRoundRect(x - 2, gapY + pipeGapH - 4, pipeW + 4, 10, 3, color); }
    }

    void drawBird(int x, int y, uint16_t color) {
        tft.fillRoundRect(x - birdW/2, y - birdH/2, birdW, birdH, 3, color);
        if (color != SKY_COLOR) { tft.drawPixel(x + 2, y - 1, TFT_BLACK); tft.drawFastHLine(x - 2, y + 1, 3, TFT_WHITE); }
    }

    void drawHUD() {
        if (score != prevScore) {
            tft.fillRect(0, 0, tft.width(), 20, TFT_BLACK);
            tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK);
            char b[32]; sprintf(b, "SCORE: %d", score);
            tft.drawString(b, tft.width()/2, 10);
            prevScore = score;
        }
    }

    void drawGameOverOverlay() {
        tft.fillRoundRect(tft.width()/2 - 88, tft.height()/2 - 62, 176, 124, 8, TFT_BLACK);
        tft.drawRoundRect(tft.width()/2 - 88, tft.height()/2 - 62, 176, 124, 8, TFT_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED); tft.setTextSize(3); tft.drawString("GAME OVER", tft.width()/2, tft.height()/2 - 28);
        tft.setTextColor(TFT_WHITE); tft.setTextSize(2);
        char buf[32]; sprintf(buf, "Score: %d", score); tft.drawString(buf, tft.width()/2, tft.height()/2 + 6);
        tft.setTextSize(1); tft.drawString("Press A to continue", tft.width()/2, tft.height()/2 + 36);
    }

    void restart() {
        bx = tft.width() * 0.25f;
        by = tft.height() * 0.5f;
        vy = 0;
        pipeSpacing = (tft.width() + pipeW) / numPipes + 50;
        for (int i = 0; i < numPipes; ++i) { 
            pipes[i].x = tft.width() + i * pipeSpacing; 
            pipes[i].gapY = random(50, tft.height() - 70 - pipeGapH); 
            pipes[i].passed = false; 
            pipes[i].prevX = pipes[i].x;
        }
        score = 0; 
        prevScore = -1;
        alive = true; 
        flapButtonDown = (readButton(BTN_A, WEB_BTN_A_IDX) == LOW);
        state = RUNNING;
        lastStep = millis();
        tft.fillScreen(SKY_COLOR);
        drawGround();
        for (int i = 0; i < numPipes; ++i) drawPipe(pipes[i].x, pipes[i].gapY, PIPE_COLOR);
        prevBirdX = (int)bx; prevBirdY = (int)by;
        drawBird(prevBirdX, prevBirdY, BIRD_COLOR);
        drawHUD();
    }

    bool checkCollision() {
        int birdLeft = (int)bx - birdW/2;
        int birdRight = (int)bx + birdW/2;
        int birdTop = (int)by - birdH/2;
        int birdBot = (int)by + birdH/2;
        if (birdTop <= 20 || birdBot >= tft.height() - 20) return true;
        for (int i=0;i<numPipes;++i) {
            int px = pipes[i].x;
            int gapTop = pipes[i].gapY;
            int gapBottom = gapTop + pipeGapH;
            int pipeLeft = px;
            int pipeRight = px + pipeW;
            if (birdRight >= pipeLeft && birdLeft <= pipeRight) {
                if (birdTop <= gapTop || birdBot >= gapBottom) return true;
            }
        }
        return false;
    }

    void step() {
        unsigned long now = millis();
        if (now - lastStep < stepMs) return;
        lastStep = now;

        // handle flap input (single-press)
        int aState = readButton(BTN_A, WEB_BTN_A_IDX);
        if (aState == LOW) {
            if (!flapButtonDown) { vy = flapImpulse; flapButtonDown = true; }
        } else flapButtonDown = false;

        // physics update only when running
        if (state == RUNNING) {
            vy += gravity;
            by += vy;
        }

        // erase previous bird
        if (prevBirdX >= 0) {
            int ex = prevBirdX - birdW/2 - 2;
            int ey = prevBirdY - birdH/2 - 2;
            int ew = birdW + 4, eh = birdH + 4;
            if (ex < 0) ex = 0;
            if (ey < 20) ey = 20;
            if (ex + ew > tft.width()) ew = tft.width() - ex;
            if (ey + eh > tft.height()) eh = tft.height() - ey;
            tft.fillRect(ex, ey, ew, eh, SKY_COLOR);
        }

        // move & redraw pipes only when they actually move
        for (int i=0;i<numPipes;++i) {
            int oldX = pipes[i].prevX;
            
            if (state == RUNNING) {
                // Erase old pipe position
                if (oldX + pipeW >= 0 && oldX <= tft.width()) {
                    tft.fillRect(oldX - 2, 20, pipeW + 4, tft.height() - 40, SKY_COLOR);
                }
                
                // Move pipe
                pipes[i].x -= 2;
                
                // Wrap pipe if needed
                if (pipes[i].x < -pipeW) {
                    int maxX = pipes[0].x;
                    for (int j=1;j<numPipes;++j) if (pipes[j].x > maxX) maxX = pipes[j].x;
                    pipes[i].x = maxX + pipeSpacing;
                    pipes[i].gapY = random(50, tft.height() - 70 - pipeGapH);
                    pipes[i].passed = false;
                }
                
                // Draw pipe at new position
                drawPipe(pipes[i].x, pipes[i].gapY, PIPE_COLOR);
                pipes[i].prevX = pipes[i].x;
                
                // Check if passed
                if (!pipes[i].passed && pipes[i].x + pipeW < bx) { 
                    pipes[i].passed = true; 
                    score++; 
                    drawHUD(); 
                }
            }
        }

        // restore ground
        drawGround();

        // collision handling
        if (state == RUNNING && checkCollision()) {
            // set gameover state and freeze motion
            state = GAMEOVER_WAIT_PRESS;
            alive = false;
            // draw crash bird
            int bxi = (int)bx, byi = (int)by;
            tft.fillRoundRect(bxi - birdW/2, byi - birdH/2, birdW, birdH, 3, TFT_RED);
            prevBirdX = -999; prevBirdY = -999;
            // draw overlay right away
            drawGameOverOverlay();
            return;
        }

        // draw bird if running
        if (state == RUNNING) {
            int bxi = (int)bx, byi = (int)by;
            drawBird(bxi, byi, BIRD_COLOR);
            prevBirdX = bxi; prevBirdY = byi;
        }
    }
} // namespace FlappyGame

// Runner for the Flappy game (non-blocking waiting for A to continue)
void runFlappyFull() {
    static bool inited = false;
    static int prevAState = HIGH;

    if (!inited || needsRedraw) {
        FlappyGame::restart();
        needsRedraw = false;
        inited = true;
        prevAState = readButton(BTN_A, WEB_BTN_A_IDX);
    }

    // If game running, update normally
    if (FlappyGame::state == FlappyGame::RUNNING) {
        FlappyGame::step();
    } else {
        // GAMEOVER_WAIT_PRESS: don't step physics; just watch for A edge to restart
        int curA = readButton(BTN_A, WEB_BTN_A_IDX);
        if (prevAState == HIGH && curA == LOW) {
            // pressed A -> restart
            FlappyGame::restart();
        }
        prevAState = curA;
    }

    // SELECT edge-detect to exit to menu (local)
    static int prevSel = HIGH;
    int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) {
        FlappyGame::state = FlappyGame::RUNNING; // ensure consistent state when returning later
        currentState = STATE_MENU;
        needsRedraw = true;
        menuInitialized = false;
        delay(120);
        inited = false;
    }
    prevSel = curSel;
}


// --- End of FlappyGame Namespace ---

// void runFlappyFull() {
//     static bool inited = false;
//     if (!inited || needsRedraw) { FlappyGame::restart(); needsRedraw=false; inited=true; }
//     static int prevSel = HIGH;
//     int curSel = digitalRead(BTN_SELECT);
//     if (prevSel == HIGH && curSel == LOW) { currentState=STATE_MENU; needsRedraw=true; menuInitialized=false; delay(120); inited=false; prevSel=curSel; return; }
//     prevSel = curSel;
//     FlappyGame::step();
// }


// ------------------- GEOMETRY DASH CLONE -------------------
namespace GeometryGame {
    // Player
    float px, py;
    float vy = 0;
    const int playerSize = 16;
    const float gravity = 0.45f;
    const float jumpVelocity = -6.5f;
    bool onGround = false;
    bool jumpPressed = false;
    
    // Level
    const int groundY = 200;
    float cameraX = 0;
    const float scrollSpeed = 3.5f;
    
    // Obstacles
    struct Obstacle {
        float x;
        int type; // 0=spike, 1=block
        int height;
    };
    const int MAX_OBSTACLES = 12;
    Obstacle obstacles[MAX_OBSTACLES];
    
    int score = 0;
    bool alive = true;
    unsigned long lastStep = 0;
    const unsigned long stepMs = 16;
    
    // Colors (Geometry Dash style)
    const uint16_t BG_COLOR = 0x18C3;      // Dark blue-gray
    const uint16_t GROUND_COLOR = 0x2945;  // Dark gray
    const uint16_t PLAYER_COLOR = 0x07FF;  // Cyan
    const uint16_t SPIKE_COLOR = 0xF800;   // Red
    const uint16_t BLOCK_COLOR = 0xFFE0;   // Yellow
    const uint16_t GRID_COLOR = 0x4228;    // Darker gray

    void drawBackground() {
        // Draw gradient background
        for (int y = 0; y < groundY; y += 2) {
            uint16_t c = (y * 0x0821) / groundY + BG_COLOR;
            tft.drawFastHLine(0, y, tft.width(), c);
        }
        // Draw grid lines for depth
        for (int x = 0; x < tft.width(); x += 30) {
            int offsetX = ((int)cameraX % 30);
            tft.drawFastVLine(x - offsetX, 0, groundY, GRID_COLOR);
        }
    }

    void drawGround() {
        tft.fillRect(0, groundY, tft.width(), tft.height() - groundY, GROUND_COLOR);
        // Ground pattern
        for (int x = 0; x < tft.width(); x += 20) {
            int offsetX = ((int)cameraX % 20);
            tft.drawFastVLine(x - offsetX, groundY, tft.height() - groundY, 0x3186);
        }
    }

    void drawPlayer(int screenX, int screenY, uint16_t color) {
        // Draw cube with rotation effect
        tft.fillRect(screenX - playerSize/2, screenY - playerSize/2, playerSize, playerSize, color);
        tft.drawRect(screenX - playerSize/2, screenY - playerSize/2, playerSize, playerSize, TFT_WHITE);
        // Inner detail
        tft.drawLine(screenX - playerSize/2 + 2, screenY - playerSize/2 + 2, 
                     screenX + playerSize/2 - 2, screenY + playerSize/2 - 2, TFT_BLACK);
    }

    void drawObstacle(const Obstacle& obs) {
        int screenX = (int)(obs.x - cameraX);
        if (screenX < -40 || screenX > tft.width() + 20) return;
        
        if (obs.type == 0) { // Spike
            int baseY = groundY;
            int spikeW = 20;
            int spikeH = obs.height;
            // Draw triangle spike pointing up
            tft.fillTriangle(screenX, baseY - spikeH, 
                           screenX - spikeW/2, baseY,
                           screenX + spikeW/2, baseY, SPIKE_COLOR);
            tft.drawTriangle(screenX, baseY - spikeH, 
                           screenX - spikeW/2, baseY,
                           screenX + spikeW/2, baseY, TFT_WHITE);
        } else { // Block
            int blockW = 30;
            int blockH = obs.height;
            int baseY = groundY - blockH;
            tft.fillRect(screenX - blockW/2, baseY, blockW, blockH, BLOCK_COLOR);
            tft.drawRect(screenX - blockW/2, baseY, blockW, blockH, TFT_WHITE);
        }
    }

    void restart() {
        px = 60;
        py = groundY - playerSize/2 - 2;
        vy = 0;
        cameraX = 0;
        onGround = true;
        alive = true;
        score = 0;
        jumpPressed = false;
        
        // Generate obstacles
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            obstacles[i].x = 200 + i * 80 + random(0, 40);
            obstacles[i].type = random(0, 2);
            obstacles[i].height = (obstacles[i].type == 0) ? 30 : (20 + random(0, 3) * 15);
        }
        
        lastStep = millis();
        
        // Draw initial scene
        tft.fillScreen(BG_COLOR);
        drawBackground();
        drawGround();
        for (int i = 0; i < MAX_OBSTACLES; i++) drawObstacle(obstacles[i]);
        drawPlayer(px - cameraX, py, PLAYER_COLOR);
        
        // HUD
        tft.fillRect(0, 0, tft.width(), 18, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Tap/Hold to Jump - SELECT=Exit", tft.width()/2, 8);
    }

    bool checkCollision() {
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            float dx = px - obstacles[i].x;
            if (abs(dx) > 25) continue;
            
            if (obstacles[i].type == 0) { // Spike
                // Triangle collision
                int spikeTop = groundY - obstacles[i].height;
                int spikeW = 20;
                if (py + playerSize/2 > spikeTop - 5 && abs(dx) < spikeW/2 - 3) {
                    return true;
                }
            } else { // Block
                int blockTop = groundY - obstacles[i].height;
                int blockW = 30;
                if (py + playerSize/2 > blockTop && abs(dx) < blockW/2 - 2) {
                    return true;
                }
            }
        }
        return false;
    }

    void step() {
        unsigned long now = millis();
        if (now - lastStep < stepMs) return;
        lastStep = now;

        // Input - tap or hold to jump
        int jumpBtn = readButton(BTN_A, WEB_BTN_A_IDX);
        if (jumpBtn == LOW) {
            if (!jumpPressed && onGround) {
                vy = jumpVelocity;
                onGround = false;
            }
            jumpPressed = true;
        } else {
            jumpPressed = false;
        }

        // Physics
        if (!onGround) vy += gravity;
        py += vy;

        // Ground collision
        if (py >= groundY - playerSize/2 - 2) {
            py = groundY - playerSize/2 - 2;
            vy = 0;
            onGround = true;
        }

        // Scroll camera and player together
        cameraX += scrollSpeed;
        px += scrollSpeed;
        score = (int)(cameraX / 10);

        // Move obstacles that go off-screen
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            if (obstacles[i].x < cameraX - 50) {
                // Find furthest obstacle
                float maxX = obstacles[0].x;
                for (int j = 0; j < MAX_OBSTACLES; j++) {
                    if (obstacles[j].x > maxX) maxX = obstacles[j].x;
                }
                obstacles[i].x = maxX + 80 + random(0, 40);
                obstacles[i].type = random(0, 2);
                obstacles[i].height = (obstacles[i].type == 0) ? 30 : (20 + random(0, 3) * 15);
            }
        }

        // Check collision
        if (checkCollision()) {
            alive = false;
            tft.fillRect(tft.width()/2 - 80, tft.height()/2 - 40, 160, 80, TFT_BLACK);
            tft.drawRect(tft.width()/2 - 80, tft.height()/2 - 40, 160, 80, TFT_RED);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_RED);
            tft.setTextSize(3);
            tft.drawString("CRASH!", tft.width()/2, tft.height()/2 - 15);
            tft.setTextColor(TFT_WHITE);
            tft.setTextSize(1);
            char buf[32];
            sprintf(buf, "Score: %d", score);
            tft.drawString(buf, tft.width()/2, tft.height()/2 + 15);
            delay(1500);
            restart();
            return;
        }

        // Redraw scene
        tft.fillScreen(BG_COLOR);
        drawBackground();
        drawGround();
        
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            drawObstacle(obstacles[i]);
        }
        
        drawPlayer(px - cameraX, py, PLAYER_COLOR);
        
        // Update HUD
        tft.fillRect(0, 0, tft.width(), 18, TFT_BLACK);
        tft.setTextDatum(ML_DATUM);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        char buf[64];
        sprintf(buf, "Score: %d", score);
        tft.drawString(buf, 6, 8);
    }
}

void runGeometryFull() {
    static bool inited = false;
    if (!inited || needsRedraw) {
        GeometryGame::restart();
        needsRedraw = false;
        inited = true;
    }
    
    static int prevSel = HIGH;
    int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) {
        currentState = STATE_MENU;
        needsRedraw = true;
        menuInitialized = false;
        delay(120);
        inited = false;
        prevSel = curSel;
        return;
    }
    prevSel = curSel;
    
    GeometryGame::step();
}

// ------------------- PACMAN CLONE (namespace + runner) -------------------
// ------------------- PacmanGame (fixed redraw + improved ghost AI) -------------------
namespace PacmanGame {
    const int COLS = 21;
    const int ROWS = 15;

    struct Tile { uint8_t wall : 1; uint8_t pellet : 1; uint8_t power : 1; };
    static Tile grid[ROWS][COLS];

    const char *layout[ROWS] = {
        "#####################",
        "#.....##...##...#...#",
        "#.###.##.#.#.##.#.#.#",
        "#o#...#.....#...#...#",
        "#.#.#.###.###.##.#.##",
        "#...#.....#.....#..o#",
        "###.#####.#.#####.###",
        "#...#.#.....#.#...#.#",
        "###.#.#####.#.#####.#",
        "#o....#..o..#....o..#",
        "#.##.#### ####.##.###",
        "#......#.....#.....#",
        "###.#########.#.####",
        "#......#.....#.....#",
        "#####################"
    };

    int cellW = 0, cellH = 0, originX = 0, originY = 0;
    struct P { int r,c; };
    P player;
    int dir = -1;
    int nextDir = -1;
    unsigned long lastMove = 0;
    unsigned long moveInterval = 160;

    struct Ghost { int r,c; int dir; uint16_t color; bool frightened; unsigned long frightUntil; int prevR, prevC; };
    const int GHOSTS = 3;
    Ghost ghosts[GHOSTS];

    uint16_t wallColor = TFT_BLUE;
    uint16_t bgColor = TFT_BLACK;
    uint16_t pelletColor = TFT_YELLOW;
    uint16_t powerColor = TFT_ORANGE;
    uint16_t playerColor = TFT_YELLOW;
    uint16_t frightColor = TFT_SILVER;

    int score = 0;
    int lives = 3;

    int prevPlayerR = -1, prevPlayerC = -1;
    unsigned long lastGhostMove = 0;
    unsigned long ghostMoveInterval = 260;
    bool inited = false;

    inline bool inside(int r,int c){ return r>=0 && r<ROWS && c>=0 && c<COLS; }

    void computeLayout() {
        cellW = (tft.width() - 8) / COLS;
        cellH = (tft.height() - 28) / ROWS;
        if (cellW < 6) cellW = 6;
        if (cellH < 6) cellH = 6;
        originX = (tft.width() - cellW*COLS) / 2;
        originY = 18;
    }

    void loadGrid() {
        for (int r=0;r<ROWS;++r) for (int c=0;c<COLS;++c) {
            char ch = layout[r][c];
            grid[r][c].wall  = (ch == '#') ? 1 : 0;
            grid[r][c].pellet = (ch == '.' || ch == 'o') ? 1 : 0;
            grid[r][c].power = (ch == 'o') ? 1 : 0;
        }
    }

    void drawCellBg(int r,int c) {
        int x = originX + c*cellW;
        int y = originY + r*cellH;
        tft.fillRect(x, y, cellW, cellH, bgColor);
    }
    void drawWall(int r,int c) {
        int x = originX + c*cellW;
        int y = originY + r*cellH;
        tft.fillRect(x, y, cellW, cellH, wallColor);
    }
    void drawPellet(int r,int c) {
        int cx = originX + c*cellW + cellW/2;
        int cy = originY + r*cellH + cellH/2;
        if (grid[r][c].power) {
            tft.fillCircle(cx, cy, min(cellW,cellH)/3, powerColor);
        } else if (grid[r][c].pellet) {
            tft.fillCircle(cx, cy, 2, pelletColor);
        }
    }
    void drawHUD() {
        tft.fillRect(0,0,tft.width(),18,bgColor);
        tft.setTextDatum(ML_DATUM);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE,bgColor);
        char buf[48];
        sprintf(buf,"Score: %d   Lives: %d", score, lives);
        tft.drawString(buf, 6, 3);
    }

    void drawMaze() {
        tft.fillScreen(bgColor);
        drawHUD();
        for (int r=0;r<ROWS;++r) {
            for (int c=0;c<COLS;++c) {
                if (grid[r][c].wall) drawWall(r,c);
                else {
                    drawCellBg(r,c);
                    if (grid[r][c].pellet) drawPellet(r,c);
                }
            }
        }
    }

    void gridCenter(int r,int c,int &x,int &y) {
        x = originX + c*cellW + cellW/2;
        y = originY + r*cellH + cellH/2;
    }

    bool freeTile(int r,int c) {
        if(!inside(r,c)) return false;
        return !grid[r][c].wall;
    }

    bool canMoveDir(int r,int c,int d) {
        int nr=r, nc=c;
        if (d==0) nr--;
        else if (d==1) nc++;
        else if (d==2) nr++;
        else if (d==3) nc--;
        return freeTile(nr,nc);
    }

    void resetEntities() {
        player.r = ROWS/2; player.c = COLS/2;
        while(grid[player.r][player.c].wall) {
            player.c++; if(player.c>=COLS){player.c=0; player.r++; if(player.r>=ROWS) player.r=0;}
        }
        // initialize prev player to current so first move clears properly
        prevPlayerR = player.r; prevPlayerC = player.c;
        dir = -1; nextDir = -1;
        ghosts[0] = {1,1,1, TFT_RED, false, 0, -1,-1};
        ghosts[1] = {1,COLS-2,3, TFT_MAGENTA, false, 0, -1,-1};
        ghosts[2] = {ROWS-2,1,1, TFT_CYAN, false, 0, -1,-1};
    }

    void drawPlayerAt(int r,int c) {
        int x,y; gridCenter(r,c,x,y);
        tft.fillCircle(x,y, min(cellW,cellH)/2 - 2, playerColor);
    }
    void clearPlayerAt(int r,int c) {
        // clear the entire cell then restore pellet if any
        int x = originX + c*cellW, y = originY + r*cellH;
        tft.fillRect(x, y, cellW, cellH, bgColor);
        if (grid[r][c].pellet) drawPellet(r,c);
    }
    void drawGhostAt(const Ghost &g) {
        int x,y; gridCenter(g.r,g.c,x,y);
        uint16_t color = g.frightened ? frightColor : g.color;
        tft.fillRoundRect(x - (cellW/2 - 2), y - (cellH/2 - 2), cellW - 4, cellH - 4, 4, color);
    }
    void clearGhostAt(const Ghost &g) {
        int x = originX + g.c*cellW, y = originY + g.r*cellH;
        tft.fillRect(x, y, cellW, cellH, bgColor);
        if (grid[g.r][g.c].pellet) drawPellet(g.r,g.c);
    }

    // BFS shortest path from (sr,sc) to (tr,tc); returns direction to take from (sr,sc) toward target, or -1 if none.
    int bfsShortestDir(int sr,int sc,int tr,int tc) {
        static int pr[ROWS][COLS], pc[ROWS][COLS];
        static bool seen[ROWS][COLS];
        for (int r=0;r<ROWS;++r) for (int c=0;c<COLS;++c) { seen[r][c]=false; pr[r][c]=-1; pc[r][c]=-1; }
        std::deque<std::pair<int,int>> q;
        q.emplace_back(sr,sc);
        seen[sr][sc]=true;
        const int dr[4] = {-1,0,1,0};
        const int dc[4] = {0,1,0,-1};
        bool found=false;
        while(!q.empty()) {
            auto p = q.front(); q.pop_front();
            int r=p.first;
            int c=p.second;
            if (r==tr && c==tc) { found=true; break; }
            for (int d=0;d<4;++d) {
                int nr=r+dr[d], nc=c+dc[d];
                if (nr<0||nr>=ROWS||nc<0||nc>=COLS) continue;
                if (seen[nr][nc]) continue;
                if (grid[nr][nc].wall) continue;
                seen[nr][nc]=true; pr[nr][nc]=r; pc[nr][nc]=c;
                q.emplace_back(nr,nc);
            }
        }
        if (!found) return -1;
        // backtrack from target to source to find first step
        int r=tr,c=tc;
        int prev_r=pr[r][c], prev_c=pc[r][c];
        while (!(prev_r==sr && prev_c==sc) && !(r==sr && c==sc)) {
            int rr=pr[r][c], cc=pc[r][c];
            if (rr==-1) break;
            r=rr; c=cc;
            prev_r = pr[r][c]; prev_c = pc[r][c];
        }
        // now (r,c) should be the neighbor of source (or source if adjacent)
        if (r==sr && c==sc) {
            // target is source or immediate neighbor; we need to get direction directly
            // find neighbor among 4
            if (tr == sr-1 && tc == sc) return 0;
            if (tr == sr && tc == sc+1) return 1;
            if (tr == sr+1 && tc == sc) return 2;
            if (tr == sr && tc == sc-1) return 3;
            return -1;
        }
        // find direction from (sr,sc) to (r,c)
        if (r == sr-1 && c == sc) return 0;
        if (r == sr && c == sc+1) return 1;
        if (r == sr+1 && c == sc) return 2;
        if (r == sr && c == sc-1) return 3;
        return -1;
    }

    // pick a direction for frightened ghost: choose neighbor that increases manhattan distance to player (fallback random legal)
    int frightenedDirPref(int gr,int gc) {
        const int dr[4] = {-1,0,1,0};
        const int dc[4] = {0,1,0,-1};
        int bestD = -1, bestDir = -1;
        // measure current distance
        int curDist = abs(gr - player.r) + abs(gc - player.c);
        // test neighbors
        for (int d=0; d<4; ++d) {
            int nr = gr + dr[d], nc = gc + dc[d];
            if (!freeTile(nr,nc)) continue;
            int nd = abs(nr - player.r) + abs(nc - player.c);
            if (nd > bestD) { bestD = nd; bestDir = d; }
        }
        if (bestDir >= 0 && bestD > curDist) return bestDir;
        // fallback: any legal neighbor
        for (int d=0;d<4;++d) {
            int nr = gr + dr[d], nc = gc + dc[d];
            if (freeTile(nr,nc)) return d;
        }
        return -1;
    }

    void stepGhosts() {
        unsigned long now = millis();
        if (now - lastGhostMove < ghostMoveInterval) return;
        lastGhostMove = now;

        for (int i=0;i<GHOSTS;++i) {
            Ghost &g = ghosts[i];
            g.prevR = g.r; g.prevC = g.c;
            if (g.frightened && millis() > g.frightUntil) g.frightened = false;

            // decide direction
            int nd = -1;
            if (g.frightened) {
                nd = frightenedDirPref(g.r, g.c);
            } else {
                // normal: BFS shortest path toward player
                nd = bfsShortestDir(g.r, g.c, player.r, player.c);
            }
            if (nd != -1) g.dir = nd;

            int nr=g.r, nc=g.c;
            if (g.dir==0) nr--; else if (g.dir==1) nc++; else if (g.dir==2) nr++; else if (g.dir==3) nc--;

            if (freeTile(nr,nc)) {
                // clear old ghost cell then move
                clearGhostAt(g);
                g.r = nr; g.c = nc;
                drawGhostAt(g);
            } else {
                // cannot move: try to pick alternative legal dir (avoid standstill)
                // prefer choosing a different legal direction
                for (int d=0; d<4; ++d) {
                    int tr = g.r + (d==0?-1:(d==2?1:0));
                    int tc = g.c + (d==1?1:(d==3?-1:0));
                    if (freeTile(tr,tc)) { g.dir = d; clearGhostAt(g); g.r = tr; g.c = tc; drawGhostAt(g); break; }
                }
            }

            // collision with player
            if (g.r == player.r && g.c == player.c) {
                if (g.frightened) {
                    score += 50;
                    clearGhostAt(g);
                    // teleport ghost to its corner (simple)
                    if (i==0) { g.r=1; g.c=1; } 
                    else if (i==1) { g.r=1; g.c=COLS-2; } 
                    else { g.r=ROWS-2; g.c=1; }
                    g.frightened = false;
                    drawGhostAt(g);
                    drawHUD();
                } else {
                    // player dies: handle in player/ghost order - do immediate effects here
                    lives--;
                    drawHUD();
                    clearPlayerAt(player.r, player.c);
                    int x,y; gridCenter(player.r, player.c, x, y);
                    tft.fillRect(x - cellW/2 + 2, y - cellH/2 + 2, cellW-4, cellH-4, TFT_RED);
                    delay(400);
                    // reset positions
                    resetEntities();
                    drawPlayerAt(player.r, player.c);
                    for (int k=0;k<GHOSTS;++k) { drawGhostAt(ghosts[k]); }
                    if (lives <= 0) {
                        tft.fillRoundRect(tft.width()/2 - 80, tft.height()/2 - 40, 160, 80, 8, TFT_BLACK);
                        tft.drawRoundRect(tft.width()/2 - 80, tft.height()/2 - 40, 160, 80, 8, TFT_WHITE);
                        tft.setTextDatum(MC_DATUM);
                        tft.setTextColor(TFT_RED); tft.setTextSize(2);
                        tft.drawString("GAME OVER", tft.width()/2, tft.height()/2 - 8);
                        tft.setTextColor(TFT_WHITE); tft.setTextSize(1);
                        tft.drawString("Press A to continue", tft.width()/2, tft.height()/2 + 12);
                        while (readButton(BTN_A, WEB_BTN_A_IDX) == LOW) delay(8);
                        while (readButton(BTN_A, WEB_BTN_A_IDX) == HIGH) delay(8);
                        loadGrid();
                        computeLayout();
                        drawMaze();
                        score = 0; lives = 3;
                        resetEntities();
                        drawPlayerAt(player.r, player.c);
                        for (int k=0;k<GHOSTS;++k) drawGhostAt(ghosts[k]);
                        drawHUD();
                    }
                    return;
                }
            }
        }
    }

    void stepPlayer() {
        unsigned long now = millis();
        if (now - lastMove < moveInterval) return;
        lastMove = now;

        // attempt to commit nextDir first
        if (nextDir != -1 && nextDir != dir) {
            if (canMoveDir(player.r, player.c, nextDir)) dir = nextDir;
        }
        if (dir == -1) return;
        if (!canMoveDir(player.r, player.c, dir)) return;

        // clear the previous player cell (prevPlayerR/C initialized at start to current pos)
        if (prevPlayerR >= 0) clearPlayerAt(prevPlayerR, prevPlayerC);

        // move player
        if (dir == 0) player.r--;
        else if (dir == 1) player.c++;
        else if (dir == 2) player.r++;
        else if (dir == 3) player.c--;

        // wrap
        if (player.r < 0) player.r = ROWS-1;
        if (player.r >= ROWS) player.r = 0;
        if (player.c < 0) player.c = COLS-1;
        if (player.c >= COLS) player.c = 0;

        // collect pellet
        if (grid[player.r][player.c].pellet) {
            bool wasPower = grid[player.r][player.c].power;
            grid[player.r][player.c].pellet = 0;
            grid[player.r][player.c].power = 0;
            drawCellBg(player.r, player.c);
            score += wasPower ? 50 : 10;
            if (wasPower) {
                for (int i=0;i<GHOSTS;++i) {
                    ghosts[i].frightened = true;
                    ghosts[i].frightUntil = millis() + 5000;
                }
            }
            drawHUD();
        }

        // draw player at new pos
        drawPlayerAt(player.r, player.c);
        prevPlayerR = player.r; prevPlayerC = player.c;
    }

    void start() {
        computeLayout();
        loadGrid();
        drawMaze();
        score = 0; lives = 3;
        resetEntities();
        drawPlayerAt(player.r, player.c);
        for (int k=0;k<GHOSTS;++k) drawGhostAt(ghosts[k]);
        drawHUD();
        inited = true;
        lastMove = millis();
        lastGhostMove = millis();
    }
} // end namespace PacmanGame

// Runner for Pacman (use in state machine)
void runPacmanFull() {
    static bool started = false;
    if (!started || needsRedraw) {
        PacmanGame::start();
        needsRedraw = false;
        started = true;
    }

    // read directional inputs (set nextDir)
    if (readButton(BTN_UP, WEB_BTN_UP_IDX) == LOW) PacmanGame::nextDir = 0;
    else if (readButton(BTN_RIGHT, WEB_BTN_RIGHT_IDX) == LOW) PacmanGame::nextDir = 1;
    else if (readButton(BTN_DOWN, WEB_BTN_DOWN_IDX) == LOW) PacmanGame::nextDir = 2;
    else if (readButton(BTN_LEFT, WEB_BTN_LEFT_IDX) == LOW) PacmanGame::nextDir = 3;

    // step: player first, ghosts second so collisions are handled predictably
    PacmanGame::stepPlayer();
    PacmanGame::stepGhosts();

    // SELECT edge to exit to menu
    static int prevSel = HIGH;
    int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) {
        currentState = STATE_MENU;
        needsRedraw = true;
        menuInitialized = false;
        delay(140);
        started = false;
    }
    prevSel = curSel;
}

// --------------------------------------------------------------
//                     TETRIS GAME (PRETTY)
//               clean draw, zero flicker, game over
//               press A to continue / retry
// --------------------------------------------------------------
// --------------------------------------------------------------
//                     TETRIS GAME (with boundary, squares, grid)
// --------------------------------------------------------------
namespace TetrisGame {

    // ---- board size ----
    const int W = 10;
    const int H = 20;

    // screen layout
    int cell;
    int ox, oy;
    int fieldWpx, fieldHpx;

    // board (0 empty, >0 filled color)
    uint16_t board[H][W];

    // piece definitions (relative coords + color)
    struct Tet { int r[4], c[4]; uint16_t col; };
    const Tet shapes[7] = {
        {{0,0,0,0},{-1,0,1,2}, TFT_CYAN},      // I
        {{0,1,1,0},{0,0,1,1}, TFT_YELLOW},     // O
        {{0,0,0,1},{-1,0,1,1}, TFT_GREEN},     // S
        {{0,0,0,-1},{-1,0,1,1}, TFT_RED},      // Z
        {{0,0,0,1},{-1,0,1,0}, TFT_BLUE},      // J
        {{0,0,0,-1},{-1,0,1,0}, TFT_ORANGE},   // L
        {{0,0,0,1},{-1,0,1,0}, TFT_MAGENTA}    // T
    };

    // game state
    int curX, curY;
    Tet cur;
    bool alive = true;
    int score = 0;
    unsigned long fallTimer = 0;
    int fallSpeed = 800;
    
    unsigned long lastMoveTime = 0;
    unsigned long lastRotateTime = 0;
    const unsigned long moveDelay = 150;
    const unsigned long rotateDelay = 200;

    enum State { RUNNING, GAMEOVER_WAIT_A };
    State state = RUNNING;

    // ----------------------------------------------------------
    // helpers
    // ----------------------------------------------------------
    inline bool inside(int r,int c){ return r>=0 && r<H && c>=0 && c<W; }

    bool canPlace(const Tet &t, int x, int y) {
        for (int i=0;i<4;i++) {
            int rr = y + t.r[i];
            int cc = x + t.c[i];
            if (!inside(rr,cc)) return false;
            if (board[rr][cc] != 0) return false;
        }
        return true;
    }

    Tet rotate(const Tet &t) {
        Tet o = t;
        for(int i=0;i<4;i++){
            int rr = t.r[i], cc = t.c[i];
            o.r[i] = -cc;
            o.c[i] = rr;
        }
        return o;
    }

    void lockPiece(const Tet &t, int x, int y) {
        for (int i=0;i<4;i++) {
            int rr = y + t.r[i];
            int cc = x + t.c[i];
            if (inside(rr,cc)) board[rr][cc] = t.col;
        }
        // Check for game over: if any locked block is at row 0 or above
        for (int c=0;c<W;c++) {
            if (board[0][c] != 0) {
                alive = false;
                state = GAMEOVER_WAIT_A;
                return;
            }
        }
    }

    // square cell drawing (filled square + subtle inner highlight)
    void drawCellSquare(int r,int c, uint16_t col) {
        int xx = ox + c*cell;
        int yy = oy + r*cell;
        // border
        tft.drawRect(xx, yy, cell, cell, TFT_DARKGREY);
        // filled square inset 1px for crisp grid look
        tft.fillRect(xx+1, yy+1, cell-2, cell-2, col);
        // small inner highlight - 1px lighter at top-left
        // (we avoid specifying custom shades; use white scaled by transparency not available)
        // Draw tiny highlight pixels for subtle 3D, safe on cheap displays:
        tft.fillRect(xx+2, yy+2, max(1, cell/6), 1, TFT_WHITE);
    }

    void eraseCellSquare(int r,int c) {
        int xx = ox + c*cell;
        int yy = oy + r*cell;
        // fill cell with background (black), then draw grid line by drawing border in dark grey
        tft.fillRect(xx, yy, cell, cell, TFT_BLACK);
        tft.drawRect(xx, yy, cell, cell, TFT_DARKGREY);
    }

    void drawGridLines() {
        // draw vertical and horizontal thin grid lines inside the field (use subtle dark grey)
        for (int i = 1; i < W; ++i) {
            int x = ox + i*cell;
            tft.drawFastVLine(x, oy, fieldHpx, TFT_DARKGREY);
        }
        for (int j = 1; j < H; ++j) {
            int y = oy + j*cell;
            tft.drawFastHLine(ox, y, fieldWpx, TFT_DARKGREY);
        }
    }

    void drawBoundary() {
        // thick boundary rectangle around the playfield
        tft.drawRect(ox-3, oy-3, fieldWpx+6, fieldHpx+6, TFT_WHITE);
        tft.drawRect(ox-4, oy-4, fieldWpx+8, fieldHpx+8, TFT_LIGHTGREY);
    }

    void drawBoard() {
        // draw background for field
        tft.fillRect(ox, oy, fieldWpx, fieldHpx, TFT_BLACK);
        // draw cells
        for (int r=0;r<H;r++){
            for (int c=0;c<W;c++){
                if (board[r][c]) drawCellSquare(r,c, board[r][c]);
                else eraseCellSquare(r,c);
            }
        }
        // grid lines & boundary on top for crispness
        drawGridLines();
        drawBoundary();
    }

    void drawPiece(const Tet &t, int x, int y, uint16_t col) {
        for (int i=0;i<4;i++){
            int rr = y + t.r[i];
            int cc = x + t.c[i];
            if (inside(rr,cc)) drawCellSquare(rr,cc,col);
        }
    }

    void erasePiece(const Tet &t, int x, int y) {
        for (int i=0;i<4;i++){
            int rr = y + t.r[i];
            int cc = x + t.c[i];
            if (inside(rr,cc)) eraseCellSquare(rr,cc);
        }
        // redraw gridlines/boundary slice only for affected area for safety
        // (simple approach: redraw full grid lines & boundary)
        drawGridLines();
        drawBoundary();
    }

    void clearLines() {
        int cleared = 0;
        for (int r=H-1;r>=0;r--) {
            bool full = true;
            for (int c=0;c<W;c++) if (board[r][c]==0) full=false;
            if (!full) continue;

            cleared++;
            for (int rr=r; rr>0; rr--)
                for (int cc=0; cc<W; cc++)
                    board[rr][cc] = board[rr-1][cc];
            for (int cc=0; cc<W; cc++) board[0][cc]=0;
            r++; // recheck same row
        }
        if (cleared > 0) {
            score += cleared * 100;
            // simple flash animation: invert row area briefly
            drawBoard();
            delay(80);
        }
    }

    void spawnPiece() {
        cur = shapes[random(0,7)];
        curX = W/2;
        curY = 0;
        // Don't check game over here - only check after locking
    }

    void drawHUD() {
        // left or top HUD area
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.drawString("SCORE", 10, 8);
        char b[32]; sprintf(b,"%d",score);
        tft.drawString(b, 10, 32);
    }

    void drawGameOver() {
        tft.fillRoundRect(20, tft.height()/2 - 60, tft.width()-40, 120, 8, TFT_BLACK);
        tft.drawRoundRect(20, tft.height()/2 - 60, tft.width()-40, 120, 8, TFT_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED); tft.setTextSize(3);
        tft.drawString("GAME OVER", tft.width()/2, tft.height()/2 - 20);
        tft.setTextColor(TFT_WHITE); tft.setTextSize(2);
        tft.drawString("Press A to continue", tft.width()/2, tft.height()/2 + 22);
    }

    // ----------------------------------------------------------
    //                 GAME CONTROL
    // ----------------------------------------------------------

    void start() {
        memset(board, 0, sizeof(board));

        // compute cell size and offsets (leave margin for HUD)
        cell = min((tft.width()-120)/W, (tft.height()-40)/H); // leave room for HUD on left
        fieldWpx = cell*W;
        fieldHpx = cell*H;
        ox = (tft.width() - fieldWpx) / 2;
        oy = (tft.height() - fieldHpx) / 2;

        // clear full screen
        tft.fillScreen(TFT_BLACK);

        // draw HUD and field

        drawHUD();
        drawBoard();

        spawnPiece();
        drawPiece(cur,curX,curY,cur.col);
        fallTimer = millis();
        alive = true;
        state = RUNNING;
        score = 0;
    }

    void step() {
        if (state != RUNNING) return;

        // gravity
        unsigned long now = millis();
        if (now - fallTimer >= fallSpeed) {
            fallTimer = now;
            // move down
            erasePiece(cur,curX,curY);
            if (canPlace(cur,curX,curY+1)) {
                curY++;
                drawPiece(cur,curX,curY,cur.col);
            } else {
                // lock
                lockPiece(cur,curX,curY);
                clearLines();
                drawBoard();
                spawnPiece();
                if (state == GAMEOVER_WAIT_A) {
                    drawGameOver();
                } else {
                    drawPiece(cur,curX,curY,cur.col);
                }
            }
        }
    }

    void moveLeft() {
        if (state!=RUNNING) return;
        unsigned long now = millis();
        if (now - lastMoveTime < moveDelay) return;
        lastMoveTime = now;
        erasePiece(cur,curX,curY);
        if (canPlace(cur,curX-1,curY)) curX--;
        drawPiece(cur,curX,curY,cur.col);
    }

    void moveRight() {
        if (state!=RUNNING) return;
        unsigned long now = millis();
        if (now - lastMoveTime < moveDelay) return;
        lastMoveTime = now;
        erasePiece(cur,curX,curY);
        if (canPlace(cur,curX+1,curY)) curX++;
        drawPiece(cur,curX,curY,cur.col);
    }

    void rotatePiece() {
        if (state!=RUNNING) return;
        unsigned long now = millis();
        if (now - lastRotateTime < rotateDelay) return;
        lastRotateTime = now;
        Tet r = rotate(cur);
        erasePiece(cur,curX,curY);
        if (canPlace(r,curX,curY)) cur = r;
        drawPiece(cur,curX,curY,cur.col);
    }

    void softDrop() {
        if (state!=RUNNING) return;
        erasePiece(cur,curX,curY);
        if (canPlace(cur,curX,curY+1)) curY++;
        drawPiece(cur,curX,curY,cur.col);
    }

    void hardDrop() {
        if (state!=RUNNING) return;
        erasePiece(cur,curX,curY);
        while (canPlace(cur,curX,curY+1)) curY++;
        drawPiece(cur,curX,curY,cur.col);
        lockPiece(cur,curX,curY);
        clearLines();
        drawBoard();
        spawnPiece();
        if (state == RUNNING) drawPiece(cur,curX,curY,cur.col);
        else drawGameOver();
    }

} // namespace TetrisGame


// --------------------------------------------------------------
//                   RUNNER: runTetrisFull()
// --------------------------------------------------------------
void runTetrisFull() {
    static bool inited = false;
    static int prevA = HIGH;
    static int prevLeft = HIGH, prevRight = HIGH, prevUp = HIGH;

    if (!inited || needsRedraw) {
        TetrisGame::start();
        needsRedraw = false;
        inited = true;
        prevA = readButton(BTN_A, WEB_BTN_A_IDX);
        prevLeft = readButton(BTN_LEFT, WEB_BTN_LEFT_IDX);
        prevRight = readButton(BTN_RIGHT, WEB_BTN_RIGHT_IDX);
        prevUp = readButton(BTN_UP, WEB_BTN_UP_IDX);
    }

    if (TetrisGame::state == TetrisGame::RUNNING) {
        int curLeft = readButton(BTN_LEFT, WEB_BTN_LEFT_IDX);
        int curRight = readButton(BTN_RIGHT, WEB_BTN_RIGHT_IDX);
        int curUp = readButton(BTN_UP, WEB_BTN_UP_IDX);
        
        if (prevLeft == HIGH && curLeft == LOW) TetrisGame::moveLeft();
        if (prevRight == HIGH && curRight == LOW) TetrisGame::moveRight();
        if (prevUp == HIGH && curUp == LOW) TetrisGame::rotatePiece();
        if (readButton(BTN_DOWN, WEB_BTN_DOWN_IDX)==LOW) TetrisGame::softDrop();
        if (readButton(BTN_START, WEB_BTN_START_IDX)==LOW) TetrisGame::hardDrop();
        
        prevLeft = curLeft;
        prevRight = curRight;
        prevUp = curUp;
        
        TetrisGame::step();
    }
    else {
        int a = readButton(BTN_A, WEB_BTN_A_IDX);
        if (prevA==HIGH && a==LOW) {
            TetrisGame::start();
        }
        prevA = a;
    }

    static int prevSel = HIGH;
    int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel==HIGH && curSel==LOW) {
        currentState = STATE_MENU;
        needsRedraw = true;
        menuInitialized = false;
        inited = false;
        delay(120);
    }
    prevSel = curSel;
}

// ===============================================================
//                    BLACKJACK (Single Player)
// ===============================================================
namespace BlackjackGame {
    struct Card { int value; char suit; char rank; };
    const int MAX_CARDS = 10;
    Card playerHand[MAX_CARDS], dealerHand[MAX_CARDS];
    int playerCards = 0, dealerCards = 0, playerScore = 0, dealerScore = 0;
    int playerMoney = 1000, currentBet = 50;
    enum GameState { BETTING, PLAYER_TURN, DEALER_TURN, SHOW_RESULT };
    GameState state = BETTING;
    char resultText[64] = "";
    
    Card drawCard() {
        int r = random(1, 14);
        char suits[] = {'H', 'D', 'C', 'S'};
        Card c; c.suit = suits[random(0, 4)];
        if (r == 1) { c.rank = 'A'; c.value = 11; }
        else if (r == 10) { c.rank = 'T'; c.value = 10; }
        else if (r < 10) { c.rank = '0' + r; c.value = r; }
        else { c.rank = (r == 11 ? 'J' : (r == 12 ? 'Q' : 'K')); c.value = 10; }
        return c;
    }
    
    int calculateScore(Card* hand, int numCards) {
        int score = 0, aces = 0;
        for (int i = 0; i < numCards; i++) {
            score += hand[i].value;
            if (hand[i].rank == 'A') aces++;
        }
        while (score > 21 && aces > 0) { score -= 10; aces--; }
        return score;
    }
    
    void drawCardUI(int x, int y, Card c, bool faceDown) {
        const int cardW = 40, cardH = 56;
        if (faceDown) {
            tft.fillRoundRect(x, y, cardW, cardH, 4, 0x0010);
            tft.drawRoundRect(x, y, cardW, cardH, 4, TFT_WHITE);
        } else {
            tft.fillRoundRect(x, y, cardW, cardH, 4, TFT_WHITE);
            tft.drawRoundRect(x, y, cardW, cardH, 4, TFT_BLACK);
            uint16_t color = (c.suit == 'H' || c.suit == 'D') ? TFT_RED : TFT_BLACK;
            tft.setTextDatum(TL_DATUM); tft.setTextSize(2); tft.setTextColor(color);
            char rankStr[3] = {c.rank, '\0', '\0'};
            if (c.rank == 'T') { rankStr[0] = '1'; rankStr[1] = '0'; }
            tft.drawString(rankStr, x + 3, y + 3);
        }
    }
    
    void drawUI() {
        tft.fillScreen(0x0320);
        tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_YELLOW, 0x0320);
        tft.drawString("BLACKJACK", tft.width()/2, 15);
        tft.setTextSize(1); tft.setTextColor(TFT_WHITE, 0x0320);
        char buf[32]; sprintf(buf, "Money: $%d", playerMoney); tft.drawString(buf, tft.width()/2, 35);
        
        if (state == BETTING) {
            sprintf(buf, "Bet: $%d", currentBet); tft.drawString(buf, tft.width()/2, 50);
            tft.drawString("UP/DOWN: Bet", tft.width()/2, 120);
            tft.drawString("A: Deal", tft.width()/2, 135);
        } else {
            tft.setTextDatum(ML_DATUM); tft.setTextColor(TFT_CYAN, 0x0320);
            tft.drawString("DEALER", 10, 70);
            for (int i = 0; i < dealerCards; i++)
                drawCardUI(10 + i * 45, 85, dealerHand[i], (state == PLAYER_TURN && i == 1));
            
            tft.setTextColor(TFT_GREEN, 0x0320);
            tft.drawString("PLAYER", 10, 160);
            for (int i = 0; i < playerCards; i++)
                drawCardUI(10 + i * 45, 175, playerHand[i], false);
            
            tft.setTextDatum(MR_DATUM); tft.setTextColor(TFT_WHITE, 0x0320);
            if (state != PLAYER_TURN) { sprintf(buf, "%d", dealerScore); tft.drawString(buf, tft.width() - 10, 110); }
            sprintf(buf, "%d", playerScore); tft.drawString(buf, tft.width() - 10, 200);
        }
        
        if (state == PLAYER_TURN) {
            tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_YELLOW, 0x0320);
            tft.drawString("A:HIT B:STAND", tft.width()/2, 145);
        }
        
        if (state == SHOW_RESULT && strlen(resultText) > 0) {
            tft.fillRoundRect(tft.width()/2 - 70, tft.height()/2 - 25, 140, 50, 8, TFT_BLACK);
            tft.drawRoundRect(tft.width()/2 - 70, tft.height()/2 - 25, 140, 50, 8, TFT_YELLOW);
            tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_YELLOW);
            tft.drawString(resultText, tft.width()/2, tft.height()/2 - 10);
            tft.setTextSize(1); tft.drawString("Press A", tft.width()/2, tft.height()/2 + 15);
        }
        
        tft.setTextDatum(MC_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_DARKGREY, 0x0320);
        tft.drawString("SELECT: Exit", tft.width()/2, tft.height() - 10);
    }
    
    void dealInitialCards() {
        playerCards = dealerCards = 2;
        for (int i = 0; i < 2; i++) {
            playerHand[i] = drawCard();
            dealerHand[i] = drawCard();
        }
        playerScore = calculateScore(playerHand, playerCards);
        dealerScore = calculateScore(dealerHand, dealerCards);
        state = PLAYER_TURN;
    }
    
    void playerHit() {
        if (playerCards < MAX_CARDS) {
            playerHand[playerCards++] = drawCard();
            playerScore = calculateScore(playerHand, playerCards);
            if (playerScore > 21) {
                state = SHOW_RESULT; playerMoney -= currentBet; strcpy(resultText, "BUST!");
            }
        }
    }
    
    void playerStand() {
        state = DEALER_TURN;
        while (dealerScore < 17 && dealerCards < MAX_CARDS) {
            dealerHand[dealerCards++] = drawCard();
            dealerScore = calculateScore(dealerHand, dealerCards);
        }
        state = SHOW_RESULT;
        if (dealerScore > 21) { playerMoney += currentBet; strcpy(resultText, "You Win!"); }
        else if (dealerScore > playerScore) { playerMoney -= currentBet; strcpy(resultText, "Dealer Wins"); }
        else if (playerScore > dealerScore) { playerMoney += currentBet; strcpy(resultText, "You Win!"); }
        else strcpy(resultText, "Push");
    }
    
    void restart() {
        state = BETTING; playerCards = dealerCards = 0; resultText[0] = '\0';
        if (playerMoney <= 0) { playerMoney = 1000; currentBet = 50; }
        currentBet = min(currentBet, playerMoney);
        drawUI();
    }
}

void runBlackjackSP() {
    static bool inited = false;
    static int prevA = HIGH, prevB = HIGH, prevUp = HIGH, prevDown = HIGH;
    if (!inited || needsRedraw) { BlackjackGame::restart(); needsRedraw = false; inited = true; }
    
    int curA = readButton(BTN_A, WEB_BTN_A_IDX), curB = readButton(BTN_B, WEB_BTN_B_IDX);
    int curUp = readButton(BTN_UP, WEB_BTN_UP_IDX), curDown = readButton(BTN_DOWN, WEB_BTN_DOWN_IDX);
    
    if (BlackjackGame::state == BlackjackGame::BETTING) {
        if (prevUp == HIGH && curUp == LOW) { BlackjackGame::currentBet = min(BlackjackGame::currentBet + 50, BlackjackGame::playerMoney); BlackjackGame::drawUI(); }
        if (prevDown == HIGH && curDown == LOW) { BlackjackGame::currentBet = max(BlackjackGame::currentBet - 50, 50); BlackjackGame::drawUI(); }
        if (prevA == HIGH && curA == LOW && BlackjackGame::currentBet <= BlackjackGame::playerMoney) {
            BlackjackGame::dealInitialCards(); BlackjackGame::drawUI();
        }
    } else if (BlackjackGame::state == BlackjackGame::PLAYER_TURN) {
        if (prevA == HIGH && curA == LOW) { BlackjackGame::playerHit(); BlackjackGame::drawUI(); }
        if (prevB == HIGH && curB == LOW) { BlackjackGame::playerStand(); BlackjackGame::drawUI(); }
    } else if (BlackjackGame::state == BlackjackGame::SHOW_RESULT && prevA == HIGH && curA == LOW) {
        BlackjackGame::restart();
    }
    
    prevA = curA; prevB = curB; prevUp = curUp; prevDown = curDown;
    
    static int prevSel = HIGH; int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) {
        currentState = STATE_MENU; needsRedraw = true; menuInitialized = false; inited = false; delay(120);
    }
    prevSel = curSel;
}

// ===============================================================
//                    BLACKJACK 2P
// ===============================================================
namespace Blackjack2P {
    struct Card { int value; char suit; char rank; };
    const int MAX_CARDS = 10;
    Card player1Hand[MAX_CARDS], player2Hand[MAX_CARDS];
    int player1Cards = 0, player2Cards = 0, player1Score = 0, player2Score = 0;
    int player1Money = 1000, player2Money = 1000, bet = 100;
    enum GameState { WAITING, PLAYER1_TURN, PLAYER2_TURN, SHOW_RESULT };
    GameState state = WAITING;
    char resultText[64] = "";
    
    Card drawCard() {
        int r = random(1, 14);
        char suits[] = {'H', 'D', 'C', 'S'};
        Card c; c.suit = suits[random(0, 4)];
        if (r == 1) { c.rank = 'A'; c.value = 11; }
        else if (r == 10) { c.rank = 'T'; c.value = 10; }
        else if (r < 10) { c.rank = '0' + r; c.value = r; }
        else { c.rank = (r == 11 ? 'J' : (r == 12 ? 'Q' : 'K')); c.value = 10; }
        return c;
    }
    
    int calculateScore(Card* hand, int numCards) {
        int score = 0, aces = 0;
        for (int i = 0; i < numCards; i++) {
            score += hand[i].value;
            if (hand[i].rank == 'A') aces++;
        }
        while (score > 21 && aces > 0) { score -= 10; aces--; }
        return score;
    }
    
    void drawCardUI(int x, int y, Card c) {
        const int cardW = 35, cardH = 50;
        tft.fillRoundRect(x, y, cardW, cardH, 3, TFT_WHITE);
        tft.drawRoundRect(x, y, cardW, cardH, 3, TFT_BLACK);
        uint16_t color = (c.suit == 'H' || c.suit == 'D') ? TFT_RED : TFT_BLACK;
        tft.setTextDatum(TL_DATUM); tft.setTextSize(1); tft.setTextColor(color);
        char rankStr[3] = {c.rank, '\0', '\0'};
        if (c.rank == 'T') { rankStr[0] = '1'; rankStr[1] = '0'; }
        tft.drawString(rankStr, x + 2, y + 2);
    }
    
    void drawUI() {
        tft.fillScreen(0x0320);
        tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_YELLOW, 0x0320);
        tft.drawString("BLACKJACK 2P", tft.width()/2, 10);
        
        tft.setTextDatum(ML_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_CYAN, 0x0320);
        char buf[32]; sprintf(buf, "P1: $%d", player1Money); tft.drawString(buf, 10, 30);
        if (player1Cards > 0) {
            for (int i = 0; i < player1Cards; i++) drawCardUI(10 + i * 38, 45, player1Hand[i]);
            tft.setTextDatum(MR_DATUM); sprintf(buf, "%d", player1Score); tft.drawString(buf, tft.width() - 10, 70);
        }
        
        tft.setTextDatum(ML_DATUM); tft.setTextColor(TFT_GREEN, 0x0320);
        sprintf(buf, "P2(Web): $%d", player2Money); tft.drawString(buf, 10, 135);
        if (player2Cards > 0) {
            for (int i = 0; i < player2Cards; i++) drawCardUI(10 + i * 38, 150, player2Hand[i]);
            tft.setTextDatum(MR_DATUM); sprintf(buf, "%d", player2Score); tft.drawString(buf, tft.width() - 10, 175);
        }
        
        tft.setTextDatum(MC_DATUM); tft.setTextSize(1);
        if (state == WAITING) { tft.setTextColor(TFT_YELLOW, 0x0320); tft.drawString("Press A to Deal", tft.width()/2, 105); }
        else if (state == PLAYER1_TURN) { tft.setTextColor(TFT_CYAN, 0x0320); tft.drawString("P1: A=HIT B=STAND", tft.width()/2, 105); }
        else if (state == PLAYER2_TURN) { tft.setTextColor(TFT_GREEN, 0x0320); tft.drawString("P2: A=HIT B=STAND", tft.width()/2, 105); }
        
        if (state == SHOW_RESULT && strlen(resultText) > 0) {
            tft.fillRoundRect(20, tft.height()/2 - 20, tft.width() - 40, 40, 6, TFT_BLACK);
            tft.drawRoundRect(20, tft.height()/2 - 20, tft.width() - 40, 40, 6, TFT_YELLOW);
            tft.setTextColor(TFT_YELLOW);
            tft.drawString(resultText, tft.width()/2, tft.height()/2 - 8);
            tft.drawString("Press A", tft.width()/2, tft.height()/2 + 8);
        }
        
        tft.setTextColor(TFT_DARKGREY, 0x0320); tft.drawString("SELECT: Exit", tft.width()/2, tft.height() - 5);
    }
    
    void dealInitialCards() {
        player1Cards = player2Cards = 2;
        for (int i = 0; i < 2; i++) {
            player1Hand[i] = drawCard();
            player2Hand[i] = drawCard();
        }
        player1Score = calculateScore(player1Hand, player1Cards);
        player2Score = calculateScore(player2Hand, player2Cards);
        state = PLAYER1_TURN;
    }
    
    void restart() {
        state = WAITING; player1Cards = player2Cards = 0; resultText[0] = '\0';
        if (player1Money <= 0) player1Money = 1000;
        if (player2Money <= 0) player2Money = 1000;
        drawUI();
    }
}

void runBlackjack2P() {
    static bool inited = false, prevWebA = 0, prevWebB = 0;
    static int prevA = HIGH, prevB = HIGH;
    if (!inited || needsRedraw) { Blackjack2P::restart(); needsRedraw = false; inited = true; }
    
    int curA = readButton(BTN_A, WEB_BTN_A_IDX), curB = readButton(BTN_B, WEB_BTN_B_IDX);
    
    if (Blackjack2P::state == Blackjack2P::WAITING && prevA == HIGH && curA == LOW) {
        Blackjack2P::dealInitialCards(); Blackjack2P::drawUI();
    } else if (Blackjack2P::state == Blackjack2P::PLAYER1_TURN) {
        if (prevA == HIGH && curA == LOW && Blackjack2P::player1Cards < Blackjack2P::MAX_CARDS) {
            Blackjack2P::player1Hand[Blackjack2P::player1Cards++] = Blackjack2P::drawCard();
            Blackjack2P::player1Score = Blackjack2P::calculateScore(Blackjack2P::player1Hand, Blackjack2P::player1Cards);
            if (Blackjack2P::player1Score > 21) {
                Blackjack2P::state = Blackjack2P::SHOW_RESULT; Blackjack2P::player1Money -= Blackjack2P::bet;
                Blackjack2P::player2Money += Blackjack2P::bet; strcpy(Blackjack2P::resultText, "P1 BUST!");
            } else if (Blackjack2P::player1Score == 21) {
                Blackjack2P::state = Blackjack2P::PLAYER2_TURN;
            }
            Blackjack2P::drawUI();
        }
        if (prevB == HIGH && curB == LOW) { Blackjack2P::state = Blackjack2P::PLAYER2_TURN; Blackjack2P::drawUI(); }
    } else if (Blackjack2P::state == Blackjack2P::PLAYER2_TURN) {
        int webA = getWebButtonState(WEB_BTN_A_IDX), webB = getWebButtonState(WEB_BTN_B_IDX);
        if (!prevWebA && webA && Blackjack2P::player2Cards < Blackjack2P::MAX_CARDS) {
            Blackjack2P::player2Hand[Blackjack2P::player2Cards++] = Blackjack2P::drawCard();
            Blackjack2P::player2Score = Blackjack2P::calculateScore(Blackjack2P::player2Hand, Blackjack2P::player2Cards);
            if (Blackjack2P::player2Score > 21) {
                Blackjack2P::state = Blackjack2P::SHOW_RESULT; Blackjack2P::player2Money -= Blackjack2P::bet;
                Blackjack2P::player1Money += Blackjack2P::bet; strcpy(Blackjack2P::resultText, "P2 BUST!");
            } else if (Blackjack2P::player2Score == 21) {
                Blackjack2P::state = Blackjack2P::SHOW_RESULT;
                if (Blackjack2P::player2Score > Blackjack2P::player1Score) {
                    Blackjack2P::player2Money += Blackjack2P::bet; Blackjack2P::player1Money -= Blackjack2P::bet; strcpy(Blackjack2P::resultText, "P2 Wins!");
                } else if (Blackjack2P::player1Score > Blackjack2P::player2Score) {
                    Blackjack2P::player1Money += Blackjack2P::bet; Blackjack2P::player2Money -= Blackjack2P::bet; strcpy(Blackjack2P::resultText, "P1 Wins!");
                } else strcpy(Blackjack2P::resultText, "Push!");
            }
            Blackjack2P::drawUI();
        }
        if (!prevWebB && webB) {
            Blackjack2P::state = Blackjack2P::SHOW_RESULT;
            if (Blackjack2P::player2Score > Blackjack2P::player1Score) {
                Blackjack2P::player2Money += Blackjack2P::bet; Blackjack2P::player1Money -= Blackjack2P::bet; strcpy(Blackjack2P::resultText, "P2 Wins!");
            } else if (Blackjack2P::player1Score > Blackjack2P::player2Score) {
                Blackjack2P::player1Money += Blackjack2P::bet; Blackjack2P::player2Money -= Blackjack2P::bet; strcpy(Blackjack2P::resultText, "P1 Wins!");
            } else strcpy(Blackjack2P::resultText, "Push!");
            Blackjack2P::drawUI();
        }
        prevWebA = webA; prevWebB = webB;
    } else if (Blackjack2P::state == Blackjack2P::SHOW_RESULT && prevA == HIGH && curA == LOW) {
        Blackjack2P::restart();
    }
    
    prevA = curA; prevB = curB;
    
    static int prevSel = HIGH; int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) {
        currentState = STATE_MENU; needsRedraw = true; menuInitialized = false; inited = false; delay(120);
    }
    prevSel = curSel;
}

// ===============================================================
//                    POKER (Single Player)
// ===============================================================
namespace PokerGame {
    struct Card { int value; char suit; char rank; };
    const int MAX_HAND = 5;
    Card playerHand[MAX_HAND], dealerHand[MAX_HAND];
    int playerMoney = 1000, currentBet = 50;
    enum GameState { BETTING, DEALING, PLAYER_DISCARD, DEALER_TURN, SHOW_RESULT };
    GameState state = BETTING;
    char resultText[64] = "";
    bool playerDiscard[MAX_HAND] = {false};
    int discardCount = 0;
    
    Card drawCard() {
        int r = random(1, 14);
        char suits[] = {'H', 'D', 'C', 'S'};
        Card c; c.suit = suits[random(0, 4)];
        if (r == 1) { c.rank = 'A'; c.value = 14; }
        else if (r == 10) { c.rank = 'T'; c.value = 10; }
        else if (r < 10) { c.rank = '0' + r; c.value = r; }
        else { c.rank = (r == 11 ? 'J' : (r == 12 ? 'Q' : 'K')); c.value = r; }
        return c;
    }
    
    int evaluateHand(Card* hand) {
        // Simplified: count pairs, three of a kind, etc.
        int vals[5]; for(int i=0;i<5;i++) vals[i] = hand[i].value;
        for(int i=0;i<4;i++) for(int j=i+1;j<5;j++) if(vals[i]>vals[j]) { int t=vals[i]; vals[i]=vals[j]; vals[j]=t; }
        
        // Check for pairs, three of a kind, four of a kind
        int counts[15] = {0}; for(int i=0;i<5;i++) counts[hand[i].value]++;
        int pairs=0, three=0, four=0;
        for(int i=0;i<15;i++) { if(counts[i]==2) pairs++; if(counts[i]==3) three=1; if(counts[i]==4) four=1; }
        
        if(four) return 7; // Four of a kind
        if(three && pairs) return 6; // Full house
        if(three) return 3; // Three of a kind
        if(pairs==2) return 2; // Two pair
        if(pairs==1) return 1; // One pair
        return 0; // High card
    }
    
    void drawCardUI(int x, int y, Card c, bool selected) {
        const int cardW = 28, cardH = 40;
        tft.fillRoundRect(x, y, cardW, cardH, 3, selected ? TFT_YELLOW : TFT_WHITE);
        tft.drawRoundRect(x, y, cardW, cardH, 3, TFT_BLACK);
        uint16_t color = (c.suit == 'H' || c.suit == 'D') ? TFT_RED : TFT_BLACK;
        tft.setTextDatum(TL_DATUM); tft.setTextSize(1); tft.setTextColor(color);
        char rankStr[3] = {c.rank, '\0', '\0'};
        if (c.rank == 'T') { rankStr[0] = '1'; rankStr[1] = '0'; }
        tft.drawString(rankStr, x + 2, y + 2);
    }
    
    void drawUI() {
        tft.fillScreen(0x0320);
        tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_YELLOW, 0x0320);
        tft.drawString("POKER", tft.width()/2, 10);
        
        char buf[32]; tft.setTextSize(1);
        if (state == BETTING) {
            sprintf(buf, "Money: $%d", playerMoney); tft.setTextColor(TFT_WHITE, 0x0320); tft.drawString(buf, tft.width()/2, 35);
            sprintf(buf, "Bet: $%d", currentBet); tft.setTextColor(TFT_CYAN, 0x0320); tft.drawString(buf, tft.width()/2, 60);
            tft.setTextColor(TFT_YELLOW, 0x0320); tft.drawString("UP/DOWN: Bet", tft.width()/2, 100);
            tft.drawString("A: Deal", tft.width()/2, 115);
        } else {
            sprintf(buf, "Money: $%d  Bet: $%d", playerMoney, currentBet);
            tft.setTextColor(TFT_WHITE, 0x0320); tft.drawString(buf, tft.width()/2, 30);
            
            tft.setTextDatum(ML_DATUM); tft.setTextColor(TFT_CYAN, 0x0320); tft.drawString("YOU", 10, 50);
            for (int i = 0; i < MAX_HAND; i++) drawCardUI(10 + i * 32, 65, playerHand[i], playerDiscard[i]);
            
            if (state == SHOW_RESULT) {
                tft.setTextColor(TFT_GREEN, 0x0320); tft.drawString("DEALER", 10, 130);
                for (int i = 0; i < MAX_HAND; i++) drawCardUI(10 + i * 32, 145, dealerHand[i], false);
            }
        }
        
        if (state == PLAYER_DISCARD) {
            tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_YELLOW, 0x0320);
            tft.drawString("LEFT/RIGHT:Select START:Discard B:Done", tft.width()/2, 110);
        }
        
        if (state == SHOW_RESULT && strlen(resultText) > 0) {
            tft.fillRoundRect(tft.width()/2 - 70, tft.height()/2 - 20, 140, 40, 6, TFT_BLACK);
            tft.drawRoundRect(tft.width()/2 - 70, tft.height()/2 - 20, 140, 40, 6, TFT_YELLOW);
            tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_YELLOW);
            tft.drawString(resultText, tft.width()/2, tft.height()/2 - 5);
            tft.setTextSize(1); tft.drawString("Press A", tft.width()/2, tft.height()/2 + 10);
        }
        
        tft.setTextDatum(MC_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_DARKGREY, 0x0320);
        tft.drawString("SELECT: Exit", tft.width()/2, tft.height() - 5);
    }
    
    void dealInitialCards() {
        for (int i = 0; i < MAX_HAND; i++) {
            playerHand[i] = drawCard();
            dealerHand[i] = drawCard();
            playerDiscard[i] = false;
        }
        discardCount = 0;
        state = PLAYER_DISCARD;
    }
    
    void restart() {
        state = BETTING; resultText[0] = '\0';
        if (playerMoney <= 0) { playerMoney = 1000; currentBet = 50; }
        currentBet = min(currentBet, playerMoney);
        drawUI();
    }
}

void runPokerSP() {
    static bool inited = false;
    static int prevA = HIGH, prevB = HIGH, prevUp = HIGH, prevDown = HIGH, prevLeft = HIGH, prevRight = HIGH, prevStart = HIGH;
    if (!inited || needsRedraw) { PokerGame::restart(); needsRedraw = false; inited = true; }
    
    int curA = readButton(BTN_A, WEB_BTN_A_IDX), curB = readButton(BTN_B, WEB_BTN_B_IDX);
    int curUp = readButton(BTN_UP, WEB_BTN_UP_IDX), curDown = readButton(BTN_DOWN, WEB_BTN_DOWN_IDX);
    int curLeft = readButton(BTN_LEFT, WEB_BTN_LEFT_IDX), curRight = readButton(BTN_RIGHT, WEB_BTN_RIGHT_IDX);
    int curStart = readButton(BTN_START, WEB_BTN_START_IDX);
    
    if (PokerGame::state == PokerGame::BETTING) {
        if (prevUp == HIGH && curUp == LOW) { PokerGame::currentBet = min(PokerGame::currentBet + 50, PokerGame::playerMoney); PokerGame::drawUI(); }
        if (prevDown == HIGH && curDown == LOW) { PokerGame::currentBet = max(PokerGame::currentBet - 50, 50); PokerGame::drawUI(); }
        if (prevA == HIGH && curA == LOW && PokerGame::currentBet <= PokerGame::playerMoney) {
            PokerGame::dealInitialCards(); PokerGame::drawUI();
        }
    } else if (PokerGame::state == PokerGame::PLAYER_DISCARD) {
        static int selected = 0;
        if (prevLeft == HIGH && curLeft == LOW) { selected = (selected - 1 + PokerGame::MAX_HAND) % PokerGame::MAX_HAND; }
        if (prevRight == HIGH && curRight == LOW) { selected = (selected + 1) % PokerGame::MAX_HAND; }
        if (prevStart == HIGH && curStart == LOW) { 
            PokerGame::playerDiscard[selected] = !PokerGame::playerDiscard[selected]; 
            PokerGame::discardCount += PokerGame::playerDiscard[selected] ? 1 : -1;
        }
        if (prevB == HIGH && curB == LOW) {
            for (int i = 0; i < PokerGame::MAX_HAND; i++) if (PokerGame::playerDiscard[i]) PokerGame::playerHand[i] = PokerGame::drawCard();
            PokerGame::state = PokerGame::SHOW_RESULT;
            int playerScore = PokerGame::evaluateHand(PokerGame::playerHand);
            int dealerScore = PokerGame::evaluateHand(PokerGame::dealerHand);
            if (playerScore > dealerScore) { PokerGame::playerMoney += PokerGame::currentBet; strcpy(PokerGame::resultText, "You Win!"); }
            else if (dealerScore > playerScore) { PokerGame::playerMoney -= PokerGame::currentBet; strcpy(PokerGame::resultText, "Dealer Wins"); }
            else strcpy(PokerGame::resultText, "Push");
            PokerGame::drawUI();
        }
        
        // Update selection display
        for (int i = 0; i < PokerGame::MAX_HAND; i++) {
            bool sel = (i == selected) || PokerGame::playerDiscard[i];
            PokerGame::drawCardUI(10 + i * 32, 65, PokerGame::playerHand[i], sel);
        }
    } else if (PokerGame::state == PokerGame::SHOW_RESULT && prevA == HIGH && curA == LOW) {
        PokerGame::restart();
    }
    
    prevA = curA; prevB = curB; prevUp = curUp; prevDown = curDown; prevLeft = curLeft; prevRight = curRight; prevStart = curStart;
    
    static int prevSel = HIGH; int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) {
        currentState = STATE_MENU; needsRedraw = true; menuInitialized = false; inited = false; delay(120);
    }
    prevSel = curSel;
}

// ===============================================================
//                    POKER 2P
// ===============================================================
namespace Poker2P {
    struct Card { int value; char suit; char rank; };
    const int MAX_HAND = 5;
    Card player1Hand[MAX_HAND], player2Hand[MAX_HAND];
    int player1Money = 1000, player2Money = 1000, bet = 100;
    enum GameState { WAITING, P1_DISCARD, P2_DISCARD, SHOW_RESULT };
    GameState state = WAITING;
    char resultText[64] = "";
    bool p1Discard[MAX_HAND] = {false}, p2Discard[MAX_HAND] = {false};
    
    Card drawCard() {
        int r = random(1, 14);
        char suits[] = {'H', 'D', 'C', 'S'};
        Card c; c.suit = suits[random(0, 4)];
        if (r == 1) { c.rank = 'A'; c.value = 14; }
        else if (r == 10) { c.rank = 'T'; c.value = 10; }
        else if (r < 10) { c.rank = '0' + r; c.value = r; }
        else { c.rank = (r == 11 ? 'J' : (r == 12 ? 'Q' : 'K')); c.value = r; }
        return c;
    }
    
    int evaluateHand(Card* hand) {
        int vals[5]; for(int i=0;i<5;i++) vals[i] = hand[i].value;
        for(int i=0;i<4;i++) for(int j=i+1;j<5;j++) if(vals[i]>vals[j]) { int t=vals[i]; vals[i]=vals[j]; vals[j]=t; }
        int counts[15] = {0}; for(int i=0;i<5;i++) counts[hand[i].value]++;
        int pairs=0, three=0, four=0;
        for(int i=0;i<15;i++) { if(counts[i]==2) pairs++; if(counts[i]==3) three=1; if(counts[i]==4) four=1; }
        if(four) return 7; if(three && pairs) return 6; if(three) return 3;
        if(pairs==2) return 2; if(pairs==1) return 1; return 0;
    }
    
    void drawCardUI(int x, int y, Card c, bool selected) {
        const int cardW = 28, cardH = 40;
        tft.fillRoundRect(x, y, cardW, cardH, 3, selected ? TFT_YELLOW : TFT_WHITE);
        tft.drawRoundRect(x, y, cardW, cardH, 3, TFT_BLACK);
        uint16_t color = (c.suit == 'H' || c.suit == 'D') ? TFT_RED : TFT_BLACK;
        tft.setTextDatum(TL_DATUM); tft.setTextSize(1); tft.setTextColor(color);
        char rankStr[3] = {c.rank, '\0', '\0'};
        if (c.rank == 'T') { rankStr[0] = '1'; rankStr[1] = '0'; }
        tft.drawString(rankStr, x + 2, y + 2);
    }
    
    void drawUI() {
        tft.fillScreen(0x0320);
        tft.setTextDatum(MC_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_YELLOW, 0x0320);
        tft.drawString("POKER 2P", tft.width()/2, 5);
        
        char buf[32]; tft.setTextSize(1);
        tft.setTextDatum(ML_DATUM); tft.setTextColor(TFT_CYAN, 0x0320);
        sprintf(buf, "P1: $%d", player1Money); tft.drawString(buf, 10, 25);
        if (state != WAITING) for (int i = 0; i < MAX_HAND; i++) drawCardUI(10 + i * 32, 40, player1Hand[i], p1Discard[i]);
        
        tft.setTextColor(TFT_GREEN, 0x0320);
        sprintf(buf, "P2(Web): $%d", player2Money); tft.drawString(buf, 10, 110);
        if (state != WAITING) for (int i = 0; i < MAX_HAND; i++) drawCardUI(10 + i * 32, 125, player2Hand[i], p2Discard[i]);
        
        tft.setTextDatum(MC_DATUM);
        if (state == WAITING) { tft.setTextColor(TFT_YELLOW, 0x0320); tft.drawString("Press A to Deal", tft.width()/2, 85); }
        else if (state == P1_DISCARD) { tft.setTextColor(TFT_CYAN, 0x0320); tft.drawString("P1: L/R:Sel START:Mark B:Done", tft.width()/2, 85); }
        else if (state == P2_DISCARD) { tft.setTextColor(TFT_GREEN, 0x0320); tft.drawString("P2: L/R:Sel START:Mark B:Done", tft.width()/2, 85); }
        
        if (state == SHOW_RESULT && strlen(resultText) > 0) {
            tft.fillRoundRect(20, tft.height()/2 - 15, tft.width() - 40, 30, 6, TFT_BLACK);
            tft.drawRoundRect(20, tft.height()/2 - 15, tft.width() - 40, 30, 6, TFT_YELLOW);
            tft.setTextColor(TFT_YELLOW); tft.drawString(resultText, tft.width()/2, tft.height()/2);
        }
        
        tft.setTextColor(TFT_DARKGREY, 0x0320); tft.drawString("SELECT: Exit", tft.width()/2, tft.height() - 5);
    }
    
    void dealInitialCards() {
        for (int i = 0; i < MAX_HAND; i++) {
            player1Hand[i] = drawCard(); player2Hand[i] = drawCard();
            p1Discard[i] = false; p2Discard[i] = false;
        }
        state = P1_DISCARD;
    }
    
    void restart() {
        state = WAITING; resultText[0] = '\0';
        if (player1Money <= 0) player1Money = 1000;
        if (player2Money <= 0) player2Money = 1000;
        drawUI();
    }
}

void runPoker2P() {
    static bool inited = false, prevWebL = 0, prevWebR = 0, prevWebStart = 0, prevWebB = 0;
    static int prevA = HIGH, prevB = HIGH, prevLeft = HIGH, prevRight = HIGH, prevStart = HIGH;
    static int p1Sel = 0, p2Sel = 0;
    if (!inited || needsRedraw) { Poker2P::restart(); needsRedraw = false; inited = true; }
    
    int curA = readButton(BTN_A, WEB_BTN_A_IDX), curB = readButton(BTN_B, WEB_BTN_B_IDX);
    int curLeft = readButton(BTN_LEFT, WEB_BTN_LEFT_IDX), curRight = readButton(BTN_RIGHT, WEB_BTN_RIGHT_IDX);
    int curStart = readButton(BTN_START, WEB_BTN_START_IDX);
    
    if (Poker2P::state == Poker2P::WAITING && prevA == HIGH && curA == LOW) {
        Poker2P::dealInitialCards(); Poker2P::drawUI();
    } else if (Poker2P::state == Poker2P::P1_DISCARD) {
        if (prevLeft == HIGH && curLeft == LOW) { p1Sel = (p1Sel - 1 + Poker2P::MAX_HAND) % Poker2P::MAX_HAND; }
        if (prevRight == HIGH && curRight == LOW) { p1Sel = (p1Sel + 1) % Poker2P::MAX_HAND; }
        if (prevStart == HIGH && curStart == LOW) { Poker2P::p1Discard[p1Sel] = !Poker2P::p1Discard[p1Sel]; }
        if (prevB == HIGH && curB == LOW) {
            for (int i = 0; i < Poker2P::MAX_HAND; i++) if (Poker2P::p1Discard[i]) Poker2P::player1Hand[i] = Poker2P::drawCard();
            Poker2P::state = Poker2P::P2_DISCARD; p2Sel = 0;
        }
        for (int i = 0; i < Poker2P::MAX_HAND; i++) {
            bool sel = (i == p1Sel) || Poker2P::p1Discard[i];
            Poker2P::drawCardUI(10 + i * 32, 40, Poker2P::player1Hand[i], sel);
        }
        Poker2P::drawUI();
    } else if (Poker2P::state == Poker2P::P2_DISCARD) {
        int webL = getWebButtonState(WEB_BTN_LEFT_IDX), webR = getWebButtonState(WEB_BTN_RIGHT_IDX);
        int webStart = getWebButtonState(WEB_BTN_START_IDX), webB = getWebButtonState(WEB_BTN_B_IDX);
        if (!prevWebL && webL) { p2Sel = (p2Sel - 1 + Poker2P::MAX_HAND) % Poker2P::MAX_HAND; }
        if (!prevWebR && webR) { p2Sel = (p2Sel + 1) % Poker2P::MAX_HAND; }
        if (!prevWebStart && webStart) { Poker2P::p2Discard[p2Sel] = !Poker2P::p2Discard[p2Sel]; }
        if (!prevWebB && webB) {
            for (int i = 0; i < Poker2P::MAX_HAND; i++) if (Poker2P::p2Discard[i]) Poker2P::player2Hand[i] = Poker2P::drawCard();
            Poker2P::state = Poker2P::SHOW_RESULT;
            int p1Score = Poker2P::evaluateHand(Poker2P::player1Hand);
            int p2Score = Poker2P::evaluateHand(Poker2P::player2Hand);
            if (p1Score > p2Score) { Poker2P::player1Money += Poker2P::bet; Poker2P::player2Money -= Poker2P::bet; strcpy(Poker2P::resultText, "P1 Wins!"); }
            else if (p2Score > p1Score) { Poker2P::player2Money += Poker2P::bet; Poker2P::player1Money -= Poker2P::bet; strcpy(Poker2P::resultText, "P2 Wins!"); }
            else strcpy(Poker2P::resultText, "Push!");
        }
        for (int i = 0; i < Poker2P::MAX_HAND; i++) {
            bool sel = (i == p2Sel) || Poker2P::p2Discard[i];
            Poker2P::drawCardUI(10 + i * 32, 125, Poker2P::player2Hand[i], sel);
        }
        prevWebL = webL; prevWebR = webR; prevWebStart = webStart; prevWebB = webB;
        Poker2P::drawUI();
    } else if (Poker2P::state == Poker2P::SHOW_RESULT && prevA == HIGH && curA == LOW) {
        Poker2P::restart();
    }
    
    prevA = curA; prevB = curB; prevLeft = curLeft; prevRight = curRight; prevStart = curStart;
    
    static int prevSel = HIGH; int curSel = readButton(BTN_SELECT, WEB_BTN_SELECT_IDX);
    if (prevSel == HIGH && curSel == LOW) {
        currentState = STATE_MENU; needsRedraw = true; menuInitialized = false; inited = false; delay(120);
    }
    prevSel = curSel;
}

