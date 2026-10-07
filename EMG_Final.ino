/*
  MEPA Hand Controller  v3.0
  ESP32 S3 + PCA9685 + 3 x MG995 + EMG module + 5 x FSR

  Features
   - Web app (orange and black) with bottom navigation, served from the ESP32
   - EMG is OFF by default, switched on from the web app (Activate EMG)
   - EMG pipeline: baseline removal, RMS, Extended Kalman Filter (log state),
     hysteresis with hold times and minimum state time
   - Optional EMG control of the hand (separate switch, OFF by default)
   - Live EMG graph, live FSR graph, SVG hand with pressure and fold animation
   - Data logging with timestamps (EMG, servo angles, FSR forces) in LittleFS
     Oldest data is deleted automatically when storage is full
   - CSV export (opens in Excel)
   - Settings and calibration from the web app (saved in flash)
   - Grip force limit, emergency stop, event markers, OTA, AP fallback, mDNS

  Libraries needed (Library Manager): Adafruit PWM Servo Driver Library
  Board: ESP32S3 Dev Module. Choose a partition scheme that gives LittleFS
  (SPIFFS) a large area, for example "Default 4MB with spiffs" or larger.

  Required files in the same folder: MEPA_Hand_Controller.ino and webpage.h
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <sys/time.h>
#include "webpage.h"

#define FW_VERSION "3.0"

// =====================================================================
//  USER CONFIGURATION
// =====================================================================
// WiFi credentials live in secrets.h (not committed). Copy
// secrets.h.example to secrets.h and edit it before flashing.
#include "secrets.h"

const char* WIFI_SSID_V = WIFI_SSID;
const char* WIFI_PASS_V = WIFI_PASS;

// If the router is not found, the hand starts its own network
const char* AP_SSID_V = AP_SSID;
const char* AP_PASS_V = AP_PASS;

const long  TZ_OFFSET_SEC = 3600;      // Nigeria = UTC+1, used for CSV timestamps

// ---- Pins ----
#define SDA_PIN 8
#define SCL_PIN 9
#define EMG_PIN 1                      // ADC1
// Five force sensors, one per fingertip: thumb, index, middle, ring, little.
// GPIO 37 to 42 cannot do analog reads on the ESP32 S3, so ADC1 pins are used.
const uint8_t FSR_PINS[5] = { 2, 3, 4, 5, 7 };
#define FSR_COUNT 5

// ---- Servo driver ----
#define SERVOMIN 150                   // pulse count for 0 degrees
#define SERVOMAX 600                   // pulse count for 180 degrees
#define CH_THUMB        0
#define CH_INDEX_MIDDLE 1
#define CH_RING_LITTLE  2

// ---- EMG processing ----
#define EMG_FS_HZ      1000
#define EMG_OVERSAMPLE 4
#define EMG_WINDOW     50              // RMS window in samples (50 ms)
#define EKF_Q          0.01f           // process noise on log activation
#define EKF_R_REL      0.15f           // measurement noise, relative to RMS
#define CHART_SIZE     512             // 100 Hz samples kept for the web graph

// ---- Logging ----
#define SEG_RECS       2048            // records per log file
#define LOG_BUF_RECS   64              // RAM buffer before a flash write
#define FREE_MARGIN    (96UL * 1024UL) // always keep this much flash free

// =====================================================================
//  SETTINGS (saved in flash)
// =====================================================================
struct Settings {
  uint32_t magic;
  int16_t  startA[3];     // position at power on
  int16_t  openA[3];
  int16_t  closedA[3];
  uint8_t  stepMs;        // ms per degree
  float    emgRest;       // RMS mV at rest
  float    emgFlex;       // RMS mV at flex
  uint8_t  closeThr;      // percent
  uint8_t  openThr;       // percent
  uint16_t closeHold;     // ms
  uint16_t openHold;      // ms
  uint16_t minState;      // ms
  uint8_t  fsrMask;       // bit per connected sensor
  float    fsrZero[FSR_COUNT];   // mV with no load
  float    fsrFull[FSR_COUNT];   // mV at 100 percent
  uint8_t  gripOn;
  uint8_t  gripLimit;     // percent
  uint8_t  logHz;
  uint16_t logMaxKB;      // 0 = automatic
};
#define CFG_MAGIC (0x4D455041UL + 4)

Settings cfg;
Preferences prefs;
volatile bool savePending = false;

void setDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CFG_MAGIC;
  cfg.openA[0] = 80;   cfg.closedA[0] = 0;   cfg.startA[0] = 0;
  cfg.openA[1] = 170;  cfg.closedA[1] = 90;  cfg.startA[1] = 90;
  cfg.openA[2] = 180;  cfg.closedA[2] = 90;  cfg.startA[2] = 180;
  cfg.stepMs = 15;
  cfg.emgRest = 20.0f;
  cfg.emgFlex = 300.0f;
  cfg.closeThr = 50;
  cfg.openThr = 30;
  cfg.closeHold = 80;
  cfg.openHold = 150;
  cfg.minState = 300;
  cfg.fsrMask = 0x1F;
  for (int i = 0; i < FSR_COUNT; i++) { cfg.fsrZero[i] = 0; cfg.fsrFull[i] = 2000; }
  cfg.gripOn = 1;
  cfg.gripLimit = 80;
  cfg.logHz = 10;
  cfg.logMaxKB = 0;
}

void loadCfg() {
  setDefaults();
  Settings tmp;
  prefs.begin("mepa", true);
  size_t n = prefs.getBytesLength("cfg");
  if (n == sizeof(tmp)) {
    prefs.getBytes("cfg", &tmp, sizeof(tmp));
    if (tmp.magic == CFG_MAGIC) cfg = tmp;
  }
  prefs.end();
}

void saveCfg() {
  prefs.begin("mepa", false);
  prefs.putBytes("cfg", &cfg, sizeof(cfg));
  prefs.end();
}

// Only the servo defaults (used by the reset button)
void resetServoDefaults() {
  Settings d; Settings keep = cfg;
  setDefaults(); d = cfg; cfg = keep;
  for (int i = 0; i < 3; i++) {
    cfg.startA[i] = d.startA[i]; cfg.openA[i] = d.openA[i]; cfg.closedA[i] = d.closedA[i];
  }
  cfg.stepMs = d.stepMs;
}

// =====================================================================
//  SHARED LIVE STATE
// =====================================================================
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
WebServer server(80);

// ---- Servos ----
volatile int  curDeg[3];
volatile int  tgtDeg[3];
volatile bool gripHeld[3] = { false, false, false };
uint32_t lastStepT[3] = { 0, 0, 0 };

// ---- EMG ----
volatile bool  emgActive = false;
volatile bool  emgControl = false;
volatile bool  handClosed = false;
volatile float emgRaw = 0, emgRms = 0, emgEnv = 0, emgActPct = 0;
float    baseline = 0; bool baseInit = false;
double   sumSq = 0; float win[EMG_WINDOW]; int winIdx = 0;
float    ekfX = 0, ekfP = 1; bool ekfInit = false;
uint32_t sampleCount = 0;
uint32_t closeStart = 0, openStart = 0; bool closeArm = false, openArm = false;
uint32_t lastChange = 0;

// chart ring buffer (100 Hz)
volatile uint32_t chartSeq = 0;
int8_t chartRms[CHART_SIZE];   // percent of range, raw RMS
int8_t chartEkf[CHART_SIZE];   // percent of range, EKF

// EMG calibration
volatile uint8_t calMode = 0;  // 0 idle, 1 rest, 2 flex
uint32_t calStart = 0;
float    calBuf[300]; int calN = 0;
char     calMsg[48] = "";
volatile float calProgress = 0;

// ---- FSR ----
float fsrMv[FSR_COUNT];
volatile float fsrPct[FSR_COUNT];
volatile uint8_t fsrCalMode = 0;   // 0 idle, 1 tare, 2 max
uint32_t fsrCalStart = 0;
float fsrCalAcc[FSR_COUNT]; int fsrCalN = 0;
float fsrCalPeak[FSR_COUNT];

// ---- Events (written into the log) ----
enum { EV_NONE = 0, EV_MARKER = 1, EV_GRIP = 2, EV_EMG_CLOSE = 3, EV_EMG_OPEN = 4,
       EV_CAL = 5, EV_ESTOP = 6, EV_CMD_OPEN = 7, EV_CMD_CLOSE = 8 };
volatile uint8_t pendingEvent = EV_NONE;

// ---- Logging ----
#pragma pack(push, 1)
struct LogRec {
  uint32_t tSec;
  uint16_t tMs;
  uint8_t  flags;      // bit0 time synced, bit1 EMG active, bit2 hand closed
  uint8_t  event;
  uint16_t emgRms;     // mV x 10
  uint16_t emgAct;     // percent x 10
  uint8_t  servo[3];
  uint8_t  pad;
  uint16_t fsr[FSR_COUNT];   // percent x 10
};
#pragma pack(pop)
static_assert(sizeof(LogRec) == 26, "LogRec size");
#define SEG_BYTES (SEG_RECS * sizeof(LogRec))

bool     logOn = false;
bool     fsOk = false;
LogRec   logBuf[LOG_BUF_RECS]; int logBufN = 0;
uint32_t segFirst = 1, segLast = 0;   // segLast = 0 means no segment yet
uint32_t curSegBytes = 0;
uint32_t lastLogT = 0, lastFlushT = 0;
uint32_t totalRecs = 0;
uint32_t autoMaxKB = 512;

// =====================================================================
//  SERVO CONTROL (non blocking, runs in the control task)
// =====================================================================
void writeServo(uint8_t ch, int angle) {
  angle = constrain(angle, 0, 180);
  pwm.setPWM(ch, 0, map(angle, 0, 180, SERVOMIN, SERVOMAX));
}

void setTarget(int ch, int angle) {
  angle = constrain(angle, 0, 180);
  gripHeld[ch] = false;
  tgtDeg[ch] = angle;
}

void commandAll(bool close) {
  for (int i = 0; i < 3; i++) setTarget(i, close ? cfg.closedA[i] : cfg.openA[i]);
  pendingEvent = close ? EV_CMD_CLOSE : EV_CMD_OPEN;
}

float gripForce(int servo) {
  float p[FSR_COUNT];
  for (int i = 0; i < FSR_COUNT; i++) p[i] = fsrPct[i];
  switch (servo) {
    case 0: return p[0];
    case 1: return (p[1] > p[2]) ? p[1] : p[2];
    default: return (p[3] > p[4]) ? p[3] : p[4];
  }
}

void stepServos(uint32_t now) {
  for (int i = 0; i < 3; i++) {
    int cur = curDeg[i], tgt = tgtDeg[i];
    if (cur == tgt) continue;
    if (now - lastStepT[i] < cfg.stepMs) continue;
    lastStepT[i] = now;
    int dir = (tgt > cur) ? 1 : -1;
    int closeDir = (cfg.closedA[i] > cfg.openA[i]) ? 1 : -1;
    if (cfg.gripOn && dir == closeDir && gripForce(i) >= cfg.gripLimit) {
      tgtDeg[i] = cur;               // hold, do not squeeze harder
      if (!gripHeld[i]) { gripHeld[i] = true; pendingEvent = EV_GRIP; }
      continue;
    }
    cur += dir;
    curDeg[i] = cur;
    writeServo(i, cur);
  }
}

// =====================================================================
//  EMG PROCESSING
// =====================================================================
void ekfStep(float z) {
  if (!ekfInit) { ekfX = logf(max(z, 1.0f)); ekfP = 1.0f; ekfInit = true; }
  ekfP += EKF_Q;                                   // predict
  float pred = expf(ekfX);                         // h(x) = exp(x)
  float H = pred;                                  // dh/dx
  float R = EKF_R_REL * max(z, 1.0f);
  R = R * R;
  float S = H * H * ekfP + R;
  float K = ekfP * H / S;
  ekfX += K * (z - pred);                          // update
  ekfP = (1.0f - K * H) * ekfP;
  ekfX = constrain(ekfX, -5.0f, 10.0f);
  if (ekfP < 1e-4f) ekfP = 1e-4f;
  emgEnv = expf(ekfX);
}

float toPct(float mv) {
  float span = max(cfg.emgFlex - cfg.emgRest, 1.0f);
  return (mv - cfg.emgRest) / span * 100.0f;
}

void setHand(bool close, uint32_t now) {
  handClosed = close;
  lastChange = now;
  pendingEvent = close ? EV_EMG_CLOSE : EV_EMG_OPEN;
  if (emgControl) {
    for (int i = 0; i < 3; i++) setTarget(i, close ? cfg.closedA[i] : cfg.openA[i]);
  }
}

void emgFinishCal(uint32_t now) {
  int n = calN;
  if (n < 50) { strcpy(calMsg, "Not enough data"); calMode = 0; return; }
  float sum = 0; for (int i = 0; i < n; i++) sum += calBuf[i];
  if (calMode == 1) {
    cfg.emgRest = sum / n;
    strcpy(calMsg, "Rest saved");
  } else {
    // sort, take the mean of the top half
    for (int i = 1; i < n; i++) { float v = calBuf[i]; int j = i - 1; while (j >= 0 && calBuf[j] > v) { calBuf[j + 1] = calBuf[j]; j--; } calBuf[j + 1] = v; }
    float s = 0; int c = 0; for (int i = n / 2; i < n; i++) { s += calBuf[i]; c++; }
    float flex = s / c;
    if (flex < cfg.emgRest * 1.5f + 5.0f) { strcpy(calMsg, "Flex too low, retry"); calMode = 0; return; }
    cfg.emgFlex = flex;
    strcpy(calMsg, "Flex saved");
  }
  savePending = true;
  pendingEvent = EV_CAL;
  calMode = 0;
}

void emgProcessEnvelope(uint32_t now) {      // called at 100 Hz
  ekfStep(emgRms);
  emgActPct = constrain(toPct(emgEnv), 0.0f, 100.0f);

  uint32_t idx = chartSeq % CHART_SIZE;
  chartRms[idx] = (int8_t)constrain(toPct(emgRms), -100.0f, 127.0f);
  chartEkf[idx] = (int8_t)constrain(toPct(emgEnv), -100.0f, 127.0f);
  chartSeq++;

  // calibration capture
  if (calMode) {
    uint32_t el = now - calStart;
    calProgress = min(el / 4000.0f, 1.0f);
    if (el >= 1000 && calN < 300) calBuf[calN++] = emgEnv;
    if (el >= 4000) emgFinishCal(now);
  }

  // decision with hysteresis
  float a = emgActPct;
  if (!handClosed) {
    if (a >= cfg.closeThr) {
      if (!closeArm) { closeArm = true; closeStart = now; }
      if (now - closeStart >= cfg.closeHold && now - lastChange >= cfg.minState) { setHand(true, now); closeArm = false; }
    } else closeArm = false;
  } else {
    if (a <= cfg.openThr) {
      if (!openArm) { openArm = true; openStart = now; }
      if (now - openStart >= cfg.openHold && now - lastChange >= cfg.minState) { setHand(false, now); openArm = false; }
    } else openArm = false;
  }
}

void emgResetState() {
  baseInit = false; ekfInit = false; sumSq = 0; winIdx = 0;
  for (int i = 0; i < EMG_WINDOW; i++) win[i] = 0;
  closeArm = openArm = false; handClosed = false;
  emgRaw = emgRms = emgEnv = emgActPct = 0;
  calMode = 0;
}

void emgSample(uint32_t now) {
  uint32_t acc = 0;
  for (int i = 0; i < EMG_OVERSAMPLE; i++) acc += analogReadMilliVolts(EMG_PIN);
  float raw = acc / (float)EMG_OVERSAMPLE;
  emgRaw = raw;
  if (!baseInit) { baseline = raw; baseInit = true; }
  baseline += 0.001f * (raw - baseline);
  float x = raw - baseline;
  sumSq -= win[winIdx];
  win[winIdx] = x * x;
  sumSq += win[winIdx];
  winIdx = (winIdx + 1) % EMG_WINDOW;
  emgRms = sqrtf(max((float)sumSq, 0.0f) / EMG_WINDOW);
  if (++sampleCount % 10 == 0) emgProcessEnvelope(now);
}

// =====================================================================
//  FSR
// =====================================================================
void fsrRead(uint32_t now) {                  // called at 50 Hz
  for (int i = 0; i < FSR_COUNT; i++) {
    if (!(cfg.fsrMask & (1 << i))) { fsrMv[i] = 0; fsrPct[i] = 0; continue; }
    float mv = analogReadMilliVolts(FSR_PINS[i]);
    fsrMv[i] += 0.35f * (mv - fsrMv[i]);
    float span = max(cfg.fsrFull[i] - cfg.fsrZero[i], 50.0f);
    fsrPct[i] = constrain((fsrMv[i] - cfg.fsrZero[i]) / span * 100.0f, 0.0f, 100.0f);
  }
  if (fsrCalMode) {
    uint32_t el = now - fsrCalStart;
    if (fsrCalMode == 1) {                    // tare, 1.5 s
      for (int i = 0; i < FSR_COUNT; i++) fsrCalAcc[i] += fsrMv[i];
      fsrCalN++;
      if (el >= 1500) {
        for (int i = 0; i < FSR_COUNT; i++) cfg.fsrZero[i] = fsrCalAcc[i] / max(fsrCalN, 1);
        savePending = true; pendingEvent = EV_CAL; fsrCalMode = 0;
      }
    } else {                                  // peak capture, 4 s
      for (int i = 0; i < FSR_COUNT; i++) if (fsrMv[i] > fsrCalPeak[i]) fsrCalPeak[i] = fsrMv[i];
      if (el >= 4000) {
        for (int i = 0; i < FSR_COUNT; i++)
          if (cfg.fsrMask & (1 << i)) cfg.fsrFull[i] = max(fsrCalPeak[i], cfg.fsrZero[i] + 100.0f);
        savePending = true; pendingEvent = EV_CAL; fsrCalMode = 0;
      }
    }
  }
}

// =====================================================================
//  CONTROL TASK (1 kHz): EMG, FSR, servo stepping
// =====================================================================
void controlTask(void*) {
  TickType_t last = xTaskGetTickCount();
  uint32_t lastFsr = 0;
  for (;;) {
    vTaskDelayUntil(&last, 1);
    uint32_t now = millis();
    if (emgActive) emgSample(now);
    if (now - lastFsr >= 20) { lastFsr = now; fsrRead(now); }
    stepServos(now);
  }
}

// =====================================================================
//  LOGGING (runs in the main loop)
// =====================================================================
String segPath(uint32_t seq) { char b[32]; snprintf(b, sizeof(b), "/log/%06lu.bin", (unsigned long)seq); return String(b); }

void scanSegments() {
  segFirst = 0xFFFFFFFF; segLast = 0; curSegBytes = 0; totalRecs = 0;
  File dir = LittleFS.open("/log");
  if (dir && dir.isDirectory()) {
    File f = dir.openNextFile();
    while (f) {
      String n = f.name(); int s = n.lastIndexOf('/'); if (s >= 0) n = n.substring(s + 1);
      uint32_t seq = (uint32_t)n.toInt();
      if (seq > 0) { if (seq < segFirst) segFirst = seq; if (seq > segLast) segLast = seq; }
      f = dir.openNextFile();
    }
  }
  if (segLast == 0) { segFirst = 1; return; }
  File l = LittleFS.open(segPath(segLast), "r");
  if (l) { curSegBytes = l.size(); l.close(); }
}

uint32_t segCount() { return segLast == 0 ? 0 : (segLast - segFirst + 1); }
uint32_t logBytes() { return segCount() == 0 ? 0 : (segCount() - 1) * SEG_BYTES + curSegBytes; }
uint32_t fsFree() { return LittleFS.totalBytes() - LittleFS.usedBytes(); }
uint32_t maxLogBytes() { return (cfg.logMaxKB ? cfg.logMaxKB : autoMaxKB) * 1024UL; }

void deleteOldest() {
  if (segCount() <= 1) return;
  LittleFS.remove(segPath(segFirst));
  segFirst++;
}

void makeRoom() {
  while (segCount() > 1 && (fsFree() < FREE_MARGIN || logBytes() + SEG_BYTES > maxLogBytes())) deleteOldest();
}

void flushLog() {
  if (!fsOk || logBufN == 0) return;
  int idx = 0;
  while (idx < logBufN) {
    if (segLast == 0 || curSegBytes >= SEG_BYTES) {      // start a new segment
      segLast = (segLast == 0) ? 1 : segLast + 1;
      if (segFirst == 0 || segFirst > segLast) segFirst = segLast;
      curSegBytes = 0;
      makeRoom();
    }
    uint32_t room = (SEG_BYTES - curSegBytes) / sizeof(LogRec);
    int n = min((int)room, logBufN - idx);
    if (fsFree() < FREE_MARGIN / 2 && segCount() > 1) deleteOldest();
    File f = LittleFS.open(segPath(segLast), "a");
    if (!f) break;
    size_t w = f.write((const uint8_t*)&logBuf[idx], n * sizeof(LogRec));
    f.close();
    curSegBytes += w;
    totalRecs += w / sizeof(LogRec);
    idx += n;
    if (w == 0) break;
  }
  logBufN = 0;
  lastFlushT = millis();
}

void logTick(uint32_t now) {
  if (!logOn || !fsOk) return;
  uint32_t period = 1000 / max((int)cfg.logHz, 1);
  if (now - lastLogT < period) return;
  lastLogT = now;
  LogRec r; memset(&r, 0, sizeof(r));
  struct timeval tv; gettimeofday(&tv, nullptr);
  bool synced = tv.tv_sec > 1700000000;
  if (synced) { r.tSec = tv.tv_sec; r.tMs = tv.tv_usec / 1000; }
  else { r.tSec = now / 1000; r.tMs = now % 1000; }
  r.flags = (synced ? 1 : 0) | (emgActive ? 2 : 0) | (handClosed ? 4 : 0);
  r.event = pendingEvent; pendingEvent = EV_NONE;
  r.emgRms = (uint16_t)constrain(emgRms * 10.0f, 0.0f, 65000.0f);
  r.emgAct = (uint16_t)(emgActPct * 10.0f);
  for (int i = 0; i < 3; i++) r.servo[i] = curDeg[i];
  for (int i = 0; i < FSR_COUNT; i++) r.fsr[i] = (uint16_t)(fsrPct[i] * 10.0f);
  logBuf[logBufN++] = r;
  if (logBufN >= LOG_BUF_RECS || now - lastFlushT > 5000) flushLog();
}

void clearLog() {
  logBufN = 0;
  File dir = LittleFS.open("/log");
  if (dir && dir.isDirectory()) {
    for (uint32_t s = segFirst; s <= segLast && segLast; s++) LittleFS.remove(segPath(s));
  }
  segFirst = 1; segLast = 0; curSegBytes = 0; totalRecs = 0;
}

// =====================================================================
//  WEB HELPERS
// =====================================================================
void sendJson(const String& s, int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", s);
}
void sendOk(const char* msg = "ok") { sendJson(String("{\"ok\":1,\"msg\":\"") + msg + "\"}"); }
void sendErr(const char* msg) { sendJson(String("{\"ok\":0,\"msg\":\"") + msg + "\"}", 400); }

String settingsJson() {
  String s; s.reserve(900);
  s += "{";
  s += "\"start\":[" + String(cfg.startA[0]) + "," + cfg.startA[1] + "," + cfg.startA[2] + "],";
  s += "\"open\":[" + String(cfg.openA[0]) + "," + cfg.openA[1] + "," + cfg.openA[2] + "],";
  s += "\"closed\":[" + String(cfg.closedA[0]) + "," + cfg.closedA[1] + "," + cfg.closedA[2] + "],";
  s += "\"ms\":" + String(cfg.stepMs) + ",";
  s += "\"rest\":" + String(cfg.emgRest, 1) + ",\"flex\":" + String(cfg.emgFlex, 1) + ",";
  s += "\"cth\":" + String(cfg.closeThr) + ",\"oth\":" + String(cfg.openThr) + ",";
  s += "\"chold\":" + String(cfg.closeHold) + ",\"ohold\":" + String(cfg.openHold) + ",\"minst\":" + String(cfg.minState) + ",";
  s += "\"mask\":" + String(cfg.fsrMask) + ",\"gon\":" + String(cfg.gripOn) + ",\"glim\":" + String(cfg.gripLimit) + ",";
  s += "\"zero\":[";
  for (int i = 0; i < FSR_COUNT; i++) { if (i) s += ","; s += String(cfg.fsrZero[i], 0); }
  s += "],\"full\":[";
  for (int i = 0; i < FSR_COUNT; i++) { if (i) s += ","; s += String(cfg.fsrFull[i], 0); }
  s += "],\"lhz\":" + String(cfg.logHz) + ",\"lmax\":" + String(cfg.logMaxKB) + ",\"lauto\":" + String(autoMaxKB);
  s += "}";
  return s;
}

void handleState() {
  uint32_t since = server.hasArg("since") ? (uint32_t)server.arg("since").toInt() : 0;
  uint32_t seq = chartSeq;
  if (since > seq) since = seq;                    // device restarted
  if (seq - since > CHART_SIZE - 8) since = seq - 100;
  if (since == 0 && seq > 100) since = seq - 100;

  String s; s.reserve(1500);
  s += "{\"up\":" + String(millis() / 1000);
  s += ",\"emg\":{\"on\":" + String(emgActive ? 1 : 0) + ",\"ctl\":" + String(emgControl ? 1 : 0)
     + ",\"closed\":" + String(handClosed ? 1 : 0)
     + ",\"raw\":" + String(emgRaw, 0) + ",\"rms\":" + String(emgRms, 1) + ",\"env\":" + String(emgEnv, 1)
     + ",\"act\":" + String(emgActPct, 0)
     + ",\"cal\":" + String((int)calMode) + ",\"calp\":" + String(calProgress, 2) + ",\"calmsg\":\"" + calMsg + "\"}";
  s += ",\"servo\":[" + String((int)curDeg[0]) + "," + (int)curDeg[1] + "," + (int)curDeg[2] + "]";
  s += ",\"hold\":[" + String(gripHeld[0] ? 1 : 0) + "," + (gripHeld[1] ? 1 : 0) + "," + (gripHeld[2] ? 1 : 0) + "]";
  s += ",\"fsr\":[";
  for (int i = 0; i < FSR_COUNT; i++) { if (i) s += ","; s += String((int)fsrPct[i]); }
  s += "],\"fcal\":" + String((int)fsrCalMode);
  struct timeval tv; gettimeofday(&tv, nullptr);
  s += ",\"log\":{\"on\":" + String(logOn ? 1 : 0) + ",\"hz\":" + String(cfg.logHz)
     + ",\"kb\":" + String(logBytes() / 1024) + ",\"max\":" + String(maxLogBytes() / 1024)
     + ",\"free\":" + String(fsFree() / 1024) + ",\"segs\":" + String(segCount())
     + ",\"synced\":" + String(tv.tv_sec > 1700000000 ? 1 : 0) + ",\"fs\":" + String(fsOk ? 1 : 0) + "}";
  s += ",\"seq\":" + String(seq);
  s += ",\"r\":[";
  for (uint32_t i = since; i < seq; i++) { if (i != since) s += ","; s += String((int)chartRms[i % CHART_SIZE]); }
  s += "],\"k\":[";
  for (uint32_t i = since; i < seq; i++) { if (i != since) s += ","; s += String((int)chartEkf[i % CHART_SIZE]); }
  s += "]}";
  sendJson(s);
}

void handleSettingsSet() {
  auto ai = [&](const char* k, int lo, int hi, int& v) {
    if (server.hasArg(k)) v = constrain(server.arg(k).toInt(), lo, hi);
  };
  for (int i = 0; i < 3; i++) {
    int v;
    String a = "st" + String(i), b = "op" + String(i), c = "cl" + String(i);
    v = cfg.startA[i];  ai(a.c_str(), 0, 180, v); cfg.startA[i] = v;
    v = cfg.openA[i];   ai(b.c_str(), 0, 180, v); cfg.openA[i] = v;
    v = cfg.closedA[i]; ai(c.c_str(), 0, 180, v); cfg.closedA[i] = v;
  }
  int v;
  v = cfg.stepMs;     ai("ms", 1, 100, v);      cfg.stepMs = v;
  v = cfg.closeThr;   ai("cth", 10, 100, v);    cfg.closeThr = v;
  v = cfg.openThr;    ai("oth", 0, 95, v);      cfg.openThr = v;
  v = cfg.closeHold;  ai("chold", 0, 2000, v);  cfg.closeHold = v;
  v = cfg.openHold;   ai("ohold", 0, 2000, v);  cfg.openHold = v;
  v = cfg.minState;   ai("minst", 0, 5000, v);  cfg.minState = v;
  v = cfg.fsrMask;    ai("mask", 0, 31, v);     cfg.fsrMask = v;
  v = cfg.gripOn;     ai("gon", 0, 1, v);       cfg.gripOn = v;
  v = cfg.gripLimit;  ai("glim", 5, 100, v);    cfg.gripLimit = v;
  v = cfg.logHz;      ai("lhz", 1, 20, v);      cfg.logHz = v;
  v = cfg.logMaxKB;   ai("lmax", 0, 16000, v);  cfg.logMaxKB = v;
  if (cfg.logMaxKB && cfg.logMaxKB < 3 * SEG_BYTES / 1024) cfg.logMaxKB = 3 * SEG_BYTES / 1024;
  if (cfg.openThr >= cfg.closeThr) cfg.openThr = cfg.closeThr - 5;
  saveCfg();
  sendJson(settingsJson());
}

// ---- CSV export ----
const char* EV_NAMES[] = { "", "marker", "grip limit", "emg close", "emg open", "calibration", "estop", "cmd open", "cmd close" };

void handleExport() {
  flushLog();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Content-Disposition", "attachment; filename=\"mepa_log.csv\"");
  server.send(200, "text/csv", "");
  server.sendContent("timestamp,emg_active,emg_rms_mV,emg_activation_pct,hand_state,"
                     "servo_thumb_deg,servo_index_middle_deg,servo_ring_little_deg,"
                     "fsr_thumb_pct,fsr_index_pct,fsr_middle_pct,fsr_ring_pct,fsr_little_pct,event\n");
  static char out[1500];
  static LogRec buf[24];
  for (uint32_t seq = segFirst; segLast && seq <= segLast; seq++) {
    File f = LittleFS.open(segPath(seq), "r");
    if (!f) continue;
    while (f.available()) {
      int got = f.read((uint8_t*)buf, sizeof(buf)) / sizeof(LogRec);
      if (got <= 0) break;
      int len = 0;
      for (int i = 0; i < got; i++) {
        const LogRec& r = buf[i];
        char ts[40];
        if (r.flags & 1) {
          time_t t = (time_t)r.tSec + TZ_OFFSET_SEC; struct tm tmv; gmtime_r(&t, &tmv);
          snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d.%03d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                   tmv.tm_hour, tmv.tm_min, tmv.tm_sec, r.tMs);
        } else {
          snprintf(ts, sizeof(ts), "T+%lu.%03d s", (unsigned long)r.tSec, r.tMs);
        }
        len += snprintf(out + len, sizeof(out) - len,
          "%s,%d,%.1f,%.1f,%s,%d,%d,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%s\n",
          ts, (r.flags & 2) ? 1 : 0, r.emgRms / 10.0f, r.emgAct / 10.0f, (r.flags & 4) ? "closed" : "open",
          r.servo[0], r.servo[1], r.servo[2],
          r.fsr[0] / 10.0f, r.fsr[1] / 10.0f, r.fsr[2] / 10.0f, r.fsr[3] / 10.0f, r.fsr[4] / 10.0f,
          EV_NAMES[r.event <= 8 ? r.event : 0]);
        if (len > (int)sizeof(out) - 260) { server.sendContent(out, len); len = 0; }
      }
      if (len) server.sendContent(out, len);
      yield();
    }
    f.close();
  }
  server.sendContent("");
}

void setupRoutes() {
  server.on("/", HTTP_GET, []() { server.sendHeader("Cache-Control", "no-cache"); server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/settings", HTTP_GET, []() { sendJson(settingsJson()); });
  server.on("/api/settings/set", HTTP_GET, handleSettingsSet);
  server.on("/api/settings/reset", HTTP_GET, []() { resetServoDefaults(); saveCfg(); sendJson(settingsJson()); });

  server.on("/api/emg", HTTP_GET, []() {
    bool on = server.arg("on") == "1";
    if (on && !emgActive) { emgResetState(); analogSetPinAttenuation(EMG_PIN, ADC_11db); emgActive = true; }
    if (!on) { emgActive = false; emgControl = false; emgResetState(); }
    sendOk();
  });
  server.on("/api/control", HTTP_GET, []() {
    bool on = server.arg("on") == "1";
    if (on && !emgActive) { sendErr("Activate EMG first"); return; }
    emgControl = on; sendOk();
  });
  server.on("/api/cal", HTTP_GET, []() {
    String w = server.arg("what");
    if (w == "rest" || w == "flex") {
      if (!emgActive) { sendErr("Activate EMG first"); return; }
      calN = 0; calMsg[0] = 0; calProgress = 0; calStart = millis(); calMode = (w == "rest") ? 1 : 2;
    } else if (w == "fsrtare" || w == "fsrmax") {
      for (int i = 0; i < FSR_COUNT; i++) { fsrCalAcc[i] = 0; fsrCalPeak[i] = 0; }
      fsrCalN = 0; fsrCalStart = millis(); fsrCalMode = (w == "fsrtare") ? 1 : 2;
    } else { sendErr("bad request"); return; }
    sendOk();
  });
  server.on("/api/move", HTTP_GET, []() {
    int ch = server.arg("ch").toInt();
    if (ch < 0 || ch > 2) { sendErr("bad channel"); return; }
    String p = server.arg("pos");
    int lo = min(cfg.openA[ch], cfg.closedA[ch]), hi = max(cfg.openA[ch], cfg.closedA[ch]);
    if (p == "open") setTarget(ch, cfg.openA[ch]);
    else if (p == "closed") setTarget(ch, cfg.closedA[ch]);
    else setTarget(ch, constrain(p.toInt(), lo, hi));
    sendOk();
  });
  server.on("/api/all", HTTP_GET, []() { commandAll(server.arg("pos") == "closed"); sendOk(); });
  server.on("/api/speed", HTTP_GET, []() {
    cfg.stepMs = constrain(server.arg("ms").toInt(), 1, 100); saveCfg(); sendOk();
  });
  server.on("/api/estop", HTTP_GET, []() {
    emgControl = false;
    for (int i = 0; i < 3; i++) tgtDeg[i] = curDeg[i];
    pendingEvent = EV_ESTOP; sendOk("stopped");
  });

  server.on("/api/log", HTTP_GET, []() {
    if (server.hasArg("on")) {
      bool on = server.arg("on") == "1";
      if (on && !fsOk) { sendErr("Storage not ready"); return; }
      if (!on) flushLog();
      logOn = on; lastLogT = 0;
    }
    sendOk();
  });
  server.on("/api/log/marker", HTTP_GET, []() { pendingEvent = EV_MARKER; sendOk(); });
  server.on("/api/log/clear", HTTP_GET, []() { clearLog(); sendOk(); });
  server.on("/export.csv", HTTP_GET, handleExport);

  server.on("/api/time", HTTP_GET, []() {
    // The phone sends its clock, so timestamps work without internet
    uint64_t ms = strtoull(server.arg("ms").c_str(), nullptr, 10);
    struct timeval tv; gettimeofday(&tv, nullptr);
    bool synced = tv.tv_sec > 1700000000;
    int64_t diff = (int64_t)ms - ((int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000);
    if (ms > 1700000000000ULL && (!synced || llabs(diff) > 2000)) {
      tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000; settimeofday(&tv, nullptr);
    }
    sendOk();
  });
  server.on("/api/sys", HTTP_GET, []() {
    String s = "{\"fw\":\"" FW_VERSION "\",\"ip\":\"";
    s += (WiFi.getMode() & WIFI_MODE_STA) && WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    s += "\",\"net\":\""; s += (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : String(AP_SSID_V);
    s += "\",\"mode\":\""; s += (WiFi.status() == WL_CONNECTED) ? "Station" : "Access point";
    s += "\",\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
    s += ",\"heap\":" + String(ESP.getFreeHeap() / 1024);
    s += ",\"fsTotal\":" + String(fsOk ? LittleFS.totalBytes() / 1024 : 0);
    s += ",\"up\":" + String(millis() / 1000) + "}";
    sendJson(s);
  });
  server.on("/api/reboot", HTTP_GET, []() { sendOk("rebooting"); delay(300); ESP.restart(); });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
}

// =====================================================================
//  SETUP AND LOOP
// =====================================================================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("MEPA-Hand");
  WiFi.begin(WIFI_SSID_V, WIFI_PASS_V);
  Serial.print("Connecting to WiFi");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) { delay(300); Serial.print("."); }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConnected. Open http://"); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nRouter not found, starting own network");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID_V, AP_PASS_V);
    Serial.print("Join WiFi '"); Serial.print(AP_SSID_V); Serial.print("' then open http://"); Serial.println(WiFi.softAPIP());
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nMEPA Hand Controller v" FW_VERSION);

  loadCfg();

  analogReadResolution(12);
  analogSetPinAttenuation(EMG_PIN, ADC_11db);
  for (int i = 0; i < FSR_COUNT; i++) { analogSetPinAttenuation(FSR_PINS[i], ADC_11db); fsrMv[i] = 0; fsrPct[i] = 0; }

  // Servos jump straight to the start position at power on
  Wire.begin(SDA_PIN, SCL_PIN);
  pwm.begin();
  pwm.setPWMFreq(50);
  delay(10);
  for (int i = 0; i < 3; i++) {
    curDeg[i] = tgtDeg[i] = cfg.startA[i];
    writeServo(i, cfg.startA[i]);
  }

  // Storage
  fsOk = LittleFS.begin(true);
  if (fsOk) {
    if (!LittleFS.exists("/log")) LittleFS.mkdir("/log");
    uint32_t total = LittleFS.totalBytes();
    autoMaxKB = (total > 256UL * 1024UL) ? (uint32_t)((total - 160UL * 1024UL) * 0.9 / 1024) : 64;
    scanSegments();
    Serial.printf("Storage: %lu KB total, %lu KB log data\n", (unsigned long)(total / 1024), (unsigned long)(logBytes() / 1024));
  } else {
    Serial.println("LittleFS failed, logging disabled");
  }

  connectWiFi();
  if (MDNS.begin("mepa-hand")) Serial.println("Also reachable at http://mepa-hand.local");

  ArduinoOTA.setHostname("MEPA-Hand");
  // ArduinoOTA.setPassword("mepa1234");   // uncomment to require a password when flashing
  ArduinoOTA.onStart([]() { emgActive = false; logOn = false; flushLog(); Serial.println("OTA start"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA done"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) { Serial.printf("OTA %u%%\r", p * 100 / t); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA error %u\n", e); });
  ArduinoOTA.begin();

  setupRoutes();
  server.begin();

  xTaskCreatePinnedToCore(controlTask, "control", 6144, nullptr, 2, nullptr, 1);
  Serial.println("Ready");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  uint32_t now = millis();
  logTick(now);
  if (savePending) { savePending = false; saveCfg(); }
  delay(1);
}