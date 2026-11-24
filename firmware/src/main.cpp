// BMO PLAYER - portable MP3 player firmware (ESP32 + ST7789 + PCM5102A + microSD)
// by @frandorturtles
//
// Threads: Arduino loop() (core 1) = UI/buttons/screen, audioTask (core 0) = decoder + I2S.
// The SD card is only touched by the audio task while playing (library scan happens before playback).
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Audio.h>
#include "bmo_core.h"
#include "pins.h"

static const char* FW_VERSION = "1.0.0";
static const char* MUSIC_DIR = "/music";

// ----------------------------------------------------------------------------- display
static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static const uint16_t COL_BG = rgb(190, 238, 212);     // BMO screen mint
static const uint16_t COL_BG2 = rgb(214, 250, 232);
static const uint16_t COL_INK = rgb(20, 56, 44);       // dark green ink
static const uint16_t COL_MID = rgb(96, 160, 132);
static const uint16_t COL_RED = rgb(214, 60, 78);

static constexpr int CW = 160, CH = 120;               // canvas (pixel-art), shown 2x on the 320x240 panel
static SPIClass hspi(HSPI);
static Adafruit_ST7789 tft(&hspi, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
static GFXcanvas16* cv = nullptr;
static uint16_t lineBuf[CW * 2];

static void flushCanvas() {
  const uint16_t* src = cv->getBuffer();
  tft.startWrite();
  tft.setAddrWindow(0, 0, CW * 2, CH * 2);
  for (int y = 0; y < CH; y++) {
    for (int x = 0; x < CW; x++) { uint16_t c = src[y * CW + x]; lineBuf[2 * x] = c; lineBuf[2 * x + 1] = c; }
    tft.writePixels(lineBuf, CW * 2);
    tft.writePixels(lineBuf, CW * 2);
  }
  tft.endWrite();
}

// ----------------------------------------------------------------------------- library
struct Track { char path[80]; char title[40]; char artist[28]; };
static Track* tracks = nullptr;
static int nTracks = 0;
static bool sdOk = false;
static bmo::PlaylistNav nav;

static int cmpTrack(const void* a, const void* b) {
  const Track* x = (const Track*)a; const Track* y = (const Track*)b;
  int c = strcasecmp(x->artist, y->artist);
  return c ? c : strcasecmp(x->title, y->title);
}

struct FileReader {
  File* f;
  size_t read(uint32_t off, uint8_t* dst, size_t n) { if (!f->seek(off)) return 0; return f->read(dst, n); }
};

static void scanDir(const char* dirPath, int depth) {
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) return;
  for (File f = dir.openNextFile(); f && nTracks < bmo::PlaylistNav::MAXN; f = dir.openNextFile()) {
    const char* nm = f.name();
    if (f.isDirectory()) {
      if (depth < 2 && nm[0] != '.') {
        char sub[80];
        if (snprintf(sub, sizeof(sub), "%s/%s", dirPath, nm) < (int)sizeof(sub)) { f.close(); scanDir(sub, depth + 1); continue; }
      }
      f.close();
      continue;
    }
    if (nm[0] == '.' || !bmo::endsWithMp3(nm)) { f.close(); continue; }
    Track& t = tracks[nTracks];
    if (snprintf(t.path, sizeof(t.path), "%s/%s", dirPath, nm) >= (int)sizeof(t.path)) { f.close(); continue; }
    bmo::Id3Info id;
    FileReader rd{&f};
    bmo::readId3(rd, id);
    if (id.hasTitle) strlcpy(t.title, id.title, sizeof(t.title)); else bmo::titleFromPath(t.path, t.title, sizeof(t.title));
    strlcpy(t.artist, id.hasArtist ? id.artist : "", sizeof(t.artist));
    f.close();
    nTracks++;
  }
  dir.close();
}

static void scanLibrary() {
  nTracks = 0;
  if (!sdOk) return;
  scanDir(MUSIC_DIR, 0);
  if (nTracks > 1) qsort(tracks, nTracks, sizeof(Track), cmpTrack);
}

// ----------------------------------------------------------------------------- audio task
enum CmdType : uint8_t { CMD_PLAY, CMD_TOGGLE, CMD_STOP, CMD_VOLUME };
struct AudioCmd { CmdType type; int32_t a; char path[80]; };
static Audio audio;
static QueueHandle_t qCmd;
static volatile bool gEof = false, gError = false;
static volatile uint32_t gPos = 0, gDur = 0;
static char gId3Title[40] = "", gId3Artist[28] = "";
static volatile bool gId3New = false;

void audio_eof_mp3(const char*) { gEof = true; }
void audio_id3data(const char* info) {
  const char* p = strchr(info, ':');
  if (!p) return;
  p++;
  while (*p == ' ') p++;
  if (!strncasecmp(info, "Title", 5)) { bmo::utf8ToAscii((const uint8_t*)p, strlen(p), gId3Title, sizeof(gId3Title)); gId3New = true; }
  else if (!strncasecmp(info, "Artist", 6)) { bmo::utf8ToAscii((const uint8_t*)p, strlen(p), gId3Artist, sizeof(gId3Artist)); gId3New = true; }
}

static void audioTask(void*) {
  AudioCmd c;
  for (;;) {
    while (xQueueReceive(qCmd, &c, 0) == pdTRUE) {
      switch (c.type) {
        case CMD_PLAY:
          audio.stopSong();
          gEof = false; gError = false; gPos = 0; gDur = 0;
          if (!audio.connecttoFS(SD, c.path)) gError = true;
          break;
        case CMD_TOGGLE: audio.pauseResume(); break;
        case CMD_STOP: audio.stopSong(); break;
        case CMD_VOLUME: audio.setVolume((uint8_t)c.a); break;
      }
    }
    audio.loop();
    if (audio.isRunning()) { gPos = audio.getAudioCurrentTime(); gDur = audio.getAudioFileDuration(); }
    vTaskDelay(1);
  }
}

static void sendCmd(CmdType t, int32_t a = 0, const char* path = nullptr) {
  AudioCmd c; memset(&c, 0, sizeof(c));
  c.type = t; c.a = a;
  if (path) strlcpy(c.path, path, sizeof(c.path));
  xQueueSend(qCmd, &c, 0);
}

// ----------------------------------------------------------------------------- state / prefs
enum Screen : uint8_t { SC_NOW, SC_MENU, SC_LIB, SC_SETTINGS, SC_ABOUT };
enum PlayState : uint8_t { PS_STOPPED, PS_PLAYING, PS_PAUSED };
static Screen screen = SC_NOW;
static PlayState pstate = PS_STOPPED;
static Preferences prefs;
static uint8_t volume = 6;                       // 0..21 (default kept low to protect your ears)
static const uint8_t VOL_MAX = 21;
static int curTrack = -1;
static char curTitle[40] = "", curArtist[28] = "";
static int menuSel = 0, setSel = 0, libSel = 0, libTop = 0;
static bool prefDirty = false; static uint32_t prefStamp = 0;
static char toast[28] = ""; static uint32_t toastUntil = 0;
static uint32_t frameNo = 0;
static int batMv = 0, batPct = 0;
static uint32_t lastActivity = 0;

static void showToast(const char* s, uint32_t ms = 1200) { strlcpy(toast, s, sizeof(toast)); toastUntil = millis() + ms; }
static void markPrefs() { prefDirty = true; prefStamp = millis(); }
static void savePrefs() {
  prefs.putUChar("vol", volume);
  prefs.putBool("shuf", nav.shuffle());
  prefs.putUChar("rep", (uint8_t)nav.repeat());
  prefs.putUShort("last", (uint16_t)(curTrack < 0 ? 0 : curTrack));
  prefDirty = false;
}

static void startTrack(int idx) {
  if (idx < 0 || idx >= nTracks) return;
  nav.jumpTo(idx);
  curTrack = idx;
  strlcpy(curTitle, tracks[idx].title, sizeof(curTitle));
  strlcpy(curArtist, tracks[idx].artist, sizeof(curArtist));
  gId3Title[0] = gId3Artist[0] = 0; gId3New = false;
  sendCmd(CMD_PLAY, 0, tracks[idx].path);
  pstate = PS_PLAYING;
  markPrefs();
}

static void togglePlay() {
  if (nTracks == 0) { showToast("Sin musica"); return; }
  if (pstate == PS_STOPPED) startTrack(curTrack >= 0 ? curTrack : nav.current());
  else { sendCmd(CMD_TOGGLE); pstate = (pstate == PS_PLAYING) ? PS_PAUSED : PS_PLAYING; }
}
static void skipNext() { int n = nav.next(false); if (n >= 0) startTrack(n); }
static void skipPrev() { int n = nav.prev(); if (n >= 0) startTrack(n); }
static void changeVolume(int d) {
  int v = (int)volume + d;
  if (v < 0) v = 0; if (v > VOL_MAX) v = VOL_MAX;
  if (v != volume) { volume = (uint8_t)v; sendCmd(CMD_VOLUME, volume); markPrefs(); }
}
static const char* repName() { return nav.repeat() == bmo::REP_OFF ? "No" : nav.repeat() == bmo::REP_ALL ? "Todo" : "Una"; }
static void cycleRepeat() {
  nav.setRepeat((bmo::RepeatMode)(((int)nav.repeat() + 1) % 3));
  char b[28]; snprintf(b, sizeof(b), "Repetir: %s", repName()); showToast(b); markPrefs();
}
static void toggleShuffle() {
  nav.setShuffle(!nav.shuffle());
  showToast(nav.shuffle() ? "Aleatorio: SI" : "Aleatorio: NO"); markPrefs();
}

static void rescanAndReset() {
  sendCmd(CMD_STOP); pstate = PS_STOPPED; delay(150);
  sdOk = SD.begin(PIN_SD_CS, SPI, 16000000);
  scanLibrary();
  nav.init(nTracks, esp_random());
  curTrack = nTracks ? 0 : -1;
  if (curTrack >= 0) { strlcpy(curTitle, tracks[0].title, sizeof(curTitle)); strlcpy(curArtist, tracks[0].artist, sizeof(curArtist)); }
  libSel = libTop = 0;
}

// ----------------------------------------------------------------------------- buttons
struct Button { uint8_t pin; bool down; uint32_t tDown, tRep, tEdge; bool longFired; };
enum Ev : uint8_t { EV_NONE, EV_PRESS, EV_LONG, EV_REPEAT };
static Button bUp{PIN_BTN_UP}, bDown{PIN_BTN_DOWN}, bLeft{PIN_BTN_LEFT}, bRight{PIN_BTN_RIGHT},
              bOk{PIN_BTN_OK}, bBack{PIN_BTN_BACK}, bMode{PIN_BTN_MODE};

static Ev pollBtn(Button& b) {
  bool raw = digitalRead(b.pin) == LOW;
  uint32_t now = millis();
  if (raw && !b.down) {
    if (now - b.tEdge < 25) return EV_NONE;
    b.down = true; b.tDown = b.tRep = b.tEdge = now; b.longFired = false;
    return EV_PRESS;
  }
  if (!raw && b.down) {
    if (now - b.tEdge < 25) return EV_NONE;
    b.down = false; b.tEdge = now;
    return EV_NONE;
  }
  if (raw && b.down) {
    if (!b.longFired && now - b.tDown > 700) { b.longFired = true; return EV_LONG; }
    if (now - b.tDown > 400 && now - b.tRep > 90) { b.tRep = now; return EV_REPEAT; }
  }
  return EV_NONE;
}

// ----------------------------------------------------------------------------- pixel-art sprites
enum Mood : uint8_t { M_HAPPY, M_BLINK, M_SLEEPY, M_DANCE_A, M_DANCE_B, M_SURPRISED, M_SAD, M_HEY };

static void caret(int x, int y) {                       // ^ eye, 7 px wide
  for (int i = 0; i < 4; i++) { cv->drawPixel(x + 3 - i, y + i, COL_INK); cv->drawPixel(x + 3 + i, y + i, COL_INK); cv->drawPixel(x + 3 - i, y + i + 1, COL_INK); cv->drawPixel(x + 3 + i, y + i + 1, COL_INK); }
}
static void smile(int x, int y, bool up) {              // 11 px wide
  static const int8_t dy[11] = {0, 1, 2, 2, 3, 3, 3, 2, 2, 1, 0};
  for (int i = 0; i < 11; i++) {
    int yy = up ? y + dy[i] : y + 3 - dy[i];
    cv->drawPixel(x + i, yy, COL_INK); cv->drawPixel(x + i, yy + 1, COL_INK);
  }
}
static void drawFace(int x, int y, Mood m) {            // 44 x 34 panel
  cv->fillRect(x, y, 44, 34, COL_BG2);
  cv->drawRect(x, y, 44, 34, COL_INK);
  int ex1 = x + 11, ex2 = x + 29, ey = y + 9;
  switch (m) {
    case M_HAPPY: cv->fillRect(ex1, ey, 3, 5, COL_INK); cv->fillRect(ex2, ey, 3, 5, COL_INK); smile(x + 16, y + 20, true); break;
    case M_BLINK: cv->fillRect(ex1 - 1, ey + 2, 5, 1, COL_INK); cv->fillRect(ex2 - 1, ey + 2, 5, 1, COL_INK); smile(x + 16, y + 20, true); break;
    case M_SLEEPY: cv->fillRect(ex1 - 1, ey + 3, 5, 1, COL_INK); cv->fillRect(ex2 - 1, ey + 3, 5, 1, COL_INK); cv->fillRect(x + 19, y + 23, 6, 1, COL_INK); break;
    case M_DANCE_A: caret(ex1 - 2, ey); caret(ex2 - 2, ey); cv->fillRect(x + 16, y + 20, 12, 5, COL_INK); cv->fillRect(x + 18, y + 22, 8, 3, COL_RED); break;
    case M_DANCE_B: caret(ex1 - 2, ey); caret(ex2 - 2, ey); smile(x + 16, y + 20, true); break;
    case M_SURPRISED: cv->drawRect(ex1 - 1, ey - 1, 5, 6, COL_INK); cv->drawRect(ex2 - 1, ey - 1, 5, 6, COL_INK); cv->drawCircle(x + 22, y + 24, 3, COL_INK); break;
    case M_SAD: cv->fillRect(ex1, ey + 1, 3, 4, COL_INK); cv->fillRect(ex2, ey + 1, 3, 4, COL_INK); smile(x + 16, y + 20, false); break;
    case M_HEY: cv->fillRect(ex1, ey, 3, 5, COL_INK); cv->fillRect(ex2, ey, 3, 5, COL_INK); cv->fillRect(x + 16, y + 20, 12, 7, COL_INK); cv->fillRect(x + 19, y + 24, 6, 3, COL_RED); break;
  }
}
static void drawNote(int x, int y) {
  cv->fillRect(x + 3, y, 1, 7, COL_INK); cv->fillRect(x, y + 6, 4, 3, COL_INK); cv->fillRect(x + 3, y, 4, 2, COL_INK);
}
static void drawBattery(int x, int y, int pct) {
  cv->drawRect(x, y, 14, 7, COL_INK); cv->fillRect(x + 14, y + 2, 2, 3, COL_INK);
  int w = (pct * 10 + 50) / 100;
  if (w > 0) cv->fillRect(x + 2, y + 2, w, 3, pct <= 15 ? COL_RED : COL_INK);
}
static void printClipped(int x, int y, const char* s, int maxChars, uint32_t tick, uint16_t color) {
  int len = (int)strlen(s);
  int st = bmo::marqueeStart(len, maxChars, tick);
  char b[48]; int n = 0;
  for (int i = st; i < len && n < maxChars && n < (int)sizeof(b) - 1; i++) b[n++] = s[i];
  b[n] = 0;
  cv->setTextColor(color); cv->setCursor(x, y); cv->print(b);
}

static void statusBar(const char* title) {
  cv->fillRect(0, 0, CW, 11, COL_BG);
  cv->drawFastHLine(0, 11, CW, COL_INK);
  cv->setTextColor(COL_INK); cv->setCursor(3, 2); cv->print(title);
  int x = 78;
  if (nav.shuffle()) { cv->setCursor(x, 2); cv->print("SH"); x += 14; }
  if (nav.repeat() != bmo::REP_OFF) { cv->setCursor(x, 2); cv->print(nav.repeat() == bmo::REP_ONE ? "R1" : "RA"); }
  char b[8]; snprintf(b, sizeof(b), "%d%%", batPct);
  cv->setCursor(CW - 17 - 6 * (int)strlen(b) - 3, 2); cv->print(b);
  drawBattery(CW - 18, 2, batPct);
}

// ----------------------------------------------------------------------------- screens
static void drawNow() {
  statusBar("BMO");
  Mood m;
  if (pstate == PS_PLAYING) m = ((frameNo / 8) & 1) ? M_DANCE_A : M_DANCE_B;
  else if (pstate == PS_PAUSED) m = (millis() - lastActivity > 30000) ? M_SLEEPY : M_HAPPY;
  else m = (frameNo % 70 > 66) ? M_BLINK : M_HAPPY;
  if (!sdOk || nTracks == 0) m = M_SAD;
  int bob = (pstate == PS_PLAYING && ((frameNo / 8) & 1)) ? 1 : 0;
  drawFace(5, 16 + bob, m);
  if (pstate == PS_PAUSED && millis() - lastActivity > 30000) { cv->setCursor(52, 16); cv->print("z Z"); }

  const char* ttl = curTitle[0] ? curTitle : "(sin cancion)";
  if (gId3New || gId3Title[0]) { if (gId3Title[0]) ttl = gId3Title; }
  const char* art = (gId3Artist[0]) ? gId3Artist : curArtist;
  printClipped(56, 18, ttl, 17, frameNo / 2, COL_INK);
  printClipped(56, 30, art[0] ? art : "Artista desconocido", 17, frameNo / 2, COL_MID);
  char b[32];
  if (nTracks) snprintf(b, sizeof(b), "%d/%d", (curTrack < 0 ? 0 : curTrack + 1), nTracks); else snprintf(b, sizeof(b), "0/0");
  cv->setCursor(56, 42); cv->setTextColor(COL_INK); cv->print(b);

  uint32_t pos = gPos, dur = gDur;
  if (pstate == PS_STOPPED) pos = dur = 0;
  snprintf(b, sizeof(b), "%u:%02u / %u:%02u", (unsigned)(pos / 60), (unsigned)(pos % 60), (unsigned)(dur / 60), (unsigned)(dur % 60));
  cv->setCursor(6, 56); cv->print(b);
  cv->drawRect(6, 68, CW - 12, 6, COL_INK);
  if (dur > 0) cv->fillRect(8, 70, (int)((uint64_t)(CW - 16) * pos / dur), 2, COL_INK);

  cv->setCursor(6, 80); cv->print("VOL");
  for (int i = 0; i < VOL_MAX; i++) { int h = 2 + i / 4; cv->fillRect(28 + i * 5, 88 - h, 3, h, i < volume ? COL_INK : COL_MID); }

  // equalizer + notes
  for (int i = 0; i < 8; i++) {
    int h = (pstate == PS_PLAYING) ? 2 + (int)((sinf(frameNo * 0.35f + i * 1.7f) + 1.0f) * 7.0f) : 2;
    cv->fillRect(6 + i * 7, 108 - h, 5, h, COL_INK);
  }
  if (pstate == PS_PLAYING) { int ny = 100 - (int)((frameNo * 2) % 24); drawNote(90, ny); drawNote(120, 100 - (int)((frameNo * 2 + 12) % 24)); }
  cv->setCursor(66, 112); cv->setTextColor(COL_MID); cv->print("OK:play <>:skip");
  if (!sdOk) { cv->fillRect(0, 44, CW, 28, COL_BG2); cv->drawRect(0, 44, CW, 28, COL_INK); cv->setTextColor(COL_INK); cv->setCursor(8, 50); cv->print("Inserta microSD con"); cv->setCursor(8, 61); cv->print("MP3 en /music y OK"); }
  else if (nTracks == 0) { cv->fillRect(0, 44, CW, 28, COL_BG2); cv->drawRect(0, 44, CW, 28, COL_INK); cv->setTextColor(COL_INK); cv->setCursor(8, 50); cv->print("No hay MP3 en /music"); }
}

static const char* MENU_ITEMS[] = {"Reproduciendo", "Biblioteca", "Aleatorio", "Repetir", "Ajustes", "Acerca de"};
static constexpr int MENU_N = 6;
static void drawMenu() {
  statusBar("MENU");
  for (int i = 0; i < MENU_N; i++) {
    int y = 18 + i * 14;
    bool sel = (i == menuSel);
    if (sel) cv->fillRect(2, y - 2, 104, 13, COL_INK);
    cv->setTextColor(sel ? COL_BG : COL_INK); cv->setCursor(6, y); cv->print(MENU_ITEMS[i]);
    if (i == 2) { cv->setCursor(84, y); cv->print(nav.shuffle() ? "SI" : "NO"); }
    if (i == 3) { cv->setCursor(84, y); cv->print(repName()); }
  }
  drawFace(112, 24, (frameNo % 60 > 56) ? M_BLINK : M_HAPPY);
}

static void drawLib() {
  char hdr[24]; snprintf(hdr, sizeof(hdr), "BIBLIOTECA %d", nTracks);
  statusBar(hdr);
  const int rows = 8;
  if (libSel < libTop) libTop = libSel;
  if (libSel >= libTop + rows) libTop = libSel - rows + 1;
  for (int r = 0; r < rows; r++) {
    int i = libTop + r; if (i >= nTracks) break;
    int y = 15 + r * 12;
    bool sel = (i == libSel);
    if (sel) cv->fillRect(0, y - 2, CW, 12, COL_INK);
    char line[72];
    if (tracks[i].artist[0]) snprintf(line, sizeof(line), "%s - %s", tracks[i].artist, tracks[i].title); else strlcpy(line, tracks[i].title, sizeof(line));
    printClipped(4, y, line, 25, sel ? frameNo / 2 : 0, sel ? COL_BG : (i == curTrack ? COL_RED : COL_INK));
  }
  if (nTracks == 0) { cv->setTextColor(COL_INK); cv->setCursor(8, 50); cv->print("Biblioteca vacia"); }
}

static const char* SET_ITEMS[] = {"Escanear SD", "Restaurar ajustes", "Bateria", "Volver"};
static constexpr int SET_N = 4;
static void drawSettings() {
  statusBar("AJUSTES");
  for (int i = 0; i < SET_N; i++) {
    int y = 18 + i * 14; bool sel = (i == setSel);
    if (sel) cv->fillRect(2, y - 2, CW - 4, 13, COL_INK);
    cv->setTextColor(sel ? COL_BG : COL_INK); cv->setCursor(6, y); cv->print(SET_ITEMS[i]);
    if (i == 2) { char b[16]; snprintf(b, sizeof(b), "%d.%02dV", batMv / 1000, (batMv % 1000) / 10); cv->setCursor(CW - 6 - 6 * (int)strlen(b), y); cv->print(b); }
  }
}

static void drawAbout() {
  statusBar("ACERCA DE");
  drawFace(58, 16, (frameNo / 6) % 2 ? M_HEY : M_HAPPY);
  cv->setTextColor(COL_INK);
  cv->setCursor(40, 58); cv->print("BMO PLAYER v"); cv->print(FW_VERSION);
  cv->setCursor(44, 70); cv->print("@frandorturtles");
  char b[32]; snprintf(b, sizeof(b), "%d canciones", nTracks);
  cv->setCursor(48, 86); cv->print(b);
  cv->setCursor(20, 112); cv->setTextColor(COL_MID); cv->print("BACK: volver");
}

static void drawFrame() {
  cv->fillScreen(COL_BG);
  cv->setTextSize(1); cv->setTextWrap(false);
  switch (screen) {
    case SC_NOW: drawNow(); break;
    case SC_MENU: drawMenu(); break;
    case SC_LIB: drawLib(); break;
    case SC_SETTINGS: drawSettings(); break;
    case SC_ABOUT: drawAbout(); break;
  }
  if (millis() < toastUntil) {
    int w = (int)strlen(toast) * 6 + 10;
    int x = (CW - w) / 2;
    cv->fillRect(x, 92, w, 15, COL_INK); cv->setTextColor(COL_BG); cv->setCursor(x + 5, 96); cv->print(toast);
  }
  flushCanvas();
}

// ----------------------------------------------------------------------------- input handling
static void onInput() {
  Ev eUp = pollBtn(bUp), eDn = pollBtn(bDown), eL = pollBtn(bLeft), eR = pollBtn(bRight);
  Ev eOk = pollBtn(bOk), eBk = pollBtn(bBack), eMd = pollBtn(bMode);
  bool any = eUp || eDn || eL || eR || eOk || eBk || eMd;
  if (any) lastActivity = millis();
  auto rep = [](Ev e) { return e == EV_PRESS || e == EV_REPEAT; };

  switch (screen) {
    case SC_NOW:
      if (rep(eUp)) changeVolume(+1);
      if (rep(eDn)) changeVolume(-1);
      if (eL == EV_PRESS) skipPrev();
      if (eR == EV_PRESS) skipNext();
      if (eOk == EV_PRESS) { if (!sdOk) { rescanAndReset(); } else togglePlay(); }
      if (eMd == EV_PRESS) toggleShuffle();
      if (eMd == EV_LONG) cycleRepeat();
      if (eBk == EV_PRESS) screen = SC_MENU;
      break;
    case SC_MENU:
      if (rep(eUp)) menuSel = (menuSel + MENU_N - 1) % MENU_N;
      if (rep(eDn)) menuSel = (menuSel + 1) % MENU_N;
      if (eBk == EV_PRESS) screen = SC_NOW;
      if (eOk == EV_PRESS || eR == EV_PRESS) {
        switch (menuSel) {
          case 0: screen = SC_NOW; break;
          case 1: screen = SC_LIB; libSel = curTrack >= 0 ? curTrack : 0; break;
          case 2: toggleShuffle(); break;
          case 3: cycleRepeat(); break;
          case 4: screen = SC_SETTINGS; setSel = 0; break;
          case 5: screen = SC_ABOUT; break;
        }
      }
      break;
    case SC_LIB:
      if (nTracks) {
        if (rep(eUp)) libSel = (libSel + nTracks - 1) % nTracks;
        if (rep(eDn)) libSel = (libSel + 1) % nTracks;
        if (eL == EV_PRESS) libSel = max(0, libSel - 8);
        if (eR == EV_PRESS) libSel = min(nTracks - 1, libSel + 8);
        if (eOk == EV_PRESS) { startTrack(libSel); screen = SC_NOW; }
      }
      if (eBk == EV_PRESS) screen = SC_MENU;
      break;
    case SC_SETTINGS:
      if (rep(eUp)) setSel = (setSel + SET_N - 1) % SET_N;
      if (rep(eDn)) setSel = (setSel + 1) % SET_N;
      if (eBk == EV_PRESS) screen = SC_MENU;
      if (eOk == EV_PRESS) {
        if (setSel == 0) { showToast("Escaneando...", 3000); drawFrame(); rescanAndReset(); showToast("Listo", 1200); }
        else if (setSel == 1) { prefs.clear(); volume = 6; sendCmd(CMD_VOLUME, volume); nav.setShuffle(false); nav.setRepeat(bmo::REP_OFF); showToast("Ajustes restaurados"); }
        else if (setSel == 3) screen = SC_MENU;
      }
      break;
    case SC_ABOUT:
      if (eBk == EV_PRESS || eOk == EV_PRESS) screen = SC_MENU;
      break;
  }
}

static void sampleBattery() {
  uint32_t acc = 0;
  for (int i = 0; i < 16; i++) acc += analogReadMilliVolts(PIN_VBAT);
  int mv = bmo::batteryMvFromAdc((int)(acc / 16), VBAT_DIVIDER);
  batMv = batMv ? (batMv * 7 + mv) / 8 : mv;            // light smoothing
  batPct = bmo::batteryPercent(batMv);
}

// ----------------------------------------------------------------------------- setup / loop
static void splash() {
  for (int f = 0; f < 30; f++) {
    cv->fillScreen(COL_BG);
    drawFace(58, 26, (f == 14 || f == 15) ? M_BLINK : (f > 22 ? M_HEY : M_HAPPY));
    cv->setTextSize(1); cv->setTextColor(COL_INK);
    cv->setCursor(46, 70); cv->print("BMO PLAYER");
    cv->setCursor(40, 84); cv->print("@frandorturtles");
    flushCanvas();
    delay(40);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_UP, INPUT_PULLUP); pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_LEFT, INPUT_PULLUP); pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_OK, INPUT_PULLUP); pinMode(PIN_BTN_BACK, INPUT_PULLUP); pinMode(PIN_BTN_MODE, INPUT_PULLUP);
  analogSetPinAttenuation(PIN_VBAT, ADC_11db);

  // allocate big buffers first (before the audio library fragments the heap)
  cv = new GFXcanvas16(CW, CH);
  tracks = (Track*)malloc(sizeof(Track) * bmo::PlaylistNav::MAXN);

  hspi.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.init(240, 320, TFT_SPI_MODE);
  tft.setSPISpeed(40000000);
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(COL_BG);
  splash();

  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  sdOk = SD.begin(PIN_SD_CS, SPI, 16000000);
  prefs.begin("bmo", false);
  volume = prefs.getUChar("vol", 6); if (volume > VOL_MAX) volume = 6;
  scanLibrary();
  nav.init(nTracks, esp_random());
  nav.setShuffle(prefs.getBool("shuf", false));
  nav.setRepeat((bmo::RepeatMode)(prefs.getUChar("rep", 0) % 3));
  int last = prefs.getUShort("last", 0);
  if (nTracks) {
    if (last >= nTracks) last = 0;
    nav.jumpTo(last); curTrack = last;
    strlcpy(curTitle, tracks[last].title, sizeof(curTitle)); strlcpy(curArtist, tracks[last].artist, sizeof(curArtist));
  }

  audio.setPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
  audio.setVolume(volume);
  qCmd = xQueueCreate(8, sizeof(AudioCmd));
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 3, nullptr, 0);
  sampleBattery();
  lastActivity = millis();
}

void loop() {
  static uint32_t lastFrame = 0, lastBat = 0;
  uint32_t now = millis();
  onInput();
  if (gEof) {                                              // track finished by itself
    gEof = false;
    int n = nav.next(true);
    if (n >= 0) startTrack(n); else { pstate = PS_STOPPED; }
  }
  if (gError) { gError = false; pstate = PS_STOPPED; showToast("Error al abrir MP3", 1800); }
  if (now - lastBat > 2000) { lastBat = now; sampleBattery(); }
  if (prefDirty && now - prefStamp > 1500) savePrefs();
  if (now - lastFrame >= 40) { lastFrame = now; frameNo++; drawFrame(); }
  delay(2);
}
