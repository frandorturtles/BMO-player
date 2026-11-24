// Host-side unit tests for the hardware-independent firmware core.
// Build & run:  make -C firmware/test/host   (needs only g++)
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "../../lib/bmo_core/bmo_core.h"

using namespace bmo;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

struct MemReader {
  std::vector<uint8_t> d;
  size_t read(uint32_t off, uint8_t* dst, size_t n) {
    if (off >= d.size()) return 0;
    size_t m = std::min(n, d.size() - off);
    memcpy(dst, d.data() + off, m);
    return m;
  }
};

static void put32ss(std::vector<uint8_t>& v, uint32_t n) {
  v.push_back((n >> 21) & 0x7F); v.push_back((n >> 14) & 0x7F); v.push_back((n >> 7) & 0x7F); v.push_back(n & 0x7F);
}
static void put32be(std::vector<uint8_t>& v, uint32_t n) {
  v.push_back(n >> 24); v.push_back(n >> 16); v.push_back(n >> 8); v.push_back(n);
}
static std::vector<uint8_t> frame(int major, const char* id, const std::vector<uint8_t>& body) {
  std::vector<uint8_t> f(id, id + (major == 2 ? 3 : 4));
  if (major == 2) { f.push_back(body.size() >> 16); f.push_back(body.size() >> 8); f.push_back(body.size()); }
  else { if (major == 4) put32ss(f, body.size()); else put32be(f, body.size()); f.push_back(0); f.push_back(0); }
  f.insert(f.end(), body.begin(), body.end());
  return f;
}
static std::vector<uint8_t> text(uint8_t enc, const std::vector<uint8_t>& s) {
  std::vector<uint8_t> b{enc}; b.insert(b.end(), s.begin(), s.end()); return b;
}
static std::vector<uint8_t> bytes(const char* s) { return std::vector<uint8_t>(s, s + strlen(s)); }
static MemReader tag(int major, const std::vector<std::vector<uint8_t>>& frames, bool extHdr = false) {
  std::vector<uint8_t> body;
  if (extHdr) { put32ss(body, 6); body.push_back(1); body.push_back(0); }  // v2.4 ext header (6 bytes)
  for (auto& f : frames) body.insert(body.end(), f.begin(), f.end());
  body.insert(body.end(), 20, 0);                                                            // padding
  MemReader r;
  r.d = {'I', 'D', '3', (uint8_t)major, 0, (uint8_t)(extHdr ? 0x40 : 0)};
  put32ss(r.d, body.size());
  r.d.insert(r.d.end(), body.begin(), body.end());
  r.d.insert(r.d.end(), 64, 0xFF);                                                           // fake audio
  return r;
}

static void test_text() {
  char o[40];
  const uint8_t lat[] = {'C', 'a', 'n', 'c', 'i', 0xF3, 'n', ' ', 0xD1, 'u', 0xF1, 'o'};     // Cancion Nuno (accents)
  latin1ToAscii(lat, sizeof(lat), o, sizeof(o)); CHECK(!strcmp(o, "Cancion Nuno"));
  const char* u8 = "Beyonc\xC3\xA9 - Se\xC3\xB1orita \xE2\x80\x93 \xE2\x80\x9CHi\xE2\x80\x9D \xF0\x9F\x8E\xB5";
  utf8ToAscii((const uint8_t*)u8, strlen(u8), o, sizeof(o)); CHECK(!strcmp(o, "Beyonce - Senorita - \"Hi\" ?"));
  const uint8_t u16le[] = {0xFF, 0xFE, 'H', 0, 0xE9, 0, 'y', 0};
  utf16ToAscii(u16le, sizeof(u16le), false, o, sizeof(o)); CHECK(!strcmp(o, "Hey"));
  const uint8_t u16be[] = {0, 'O', 0, 'k'};
  utf16ToAscii(u16be, sizeof(u16be), true, o, sizeof(o)); CHECK(!strcmp(o, "Ok"));
  char small[5]; utf8ToAscii((const uint8_t*)"abcdefgh", 8, small, sizeof(small)); CHECK(!strcmp(small, "abcd"));
  titleFromPath("/music/Artist/My Song.MP3", o, sizeof(o)); CHECK(!strcmp(o, "My Song"));
  CHECK(endsWithMp3("a.mp3")); CHECK(endsWithMp3("A.MP3")); CHECK(!endsWithMp3("a.wav")); CHECK(!endsWithMp3(".mp3x"));
  CHECK(marqueeStart(10, 20, 5) == 0);
  CHECK(marqueeStart(30, 20, 0) == 0);
  CHECK(marqueeStart(30, 20, 12 + 4) == 4);
  CHECK(marqueeStart(30, 20, 12 + 10 + 3) == 10);
}

static void test_id3() {
  Id3Info i;
  auto r3 = tag(3, {frame(3, "TIT2", text(0, bytes("Song A"))), frame(3, "TPE1", text(0, bytes("Band A")))});
  CHECK(readId3(r3, i) && !strcmp(i.title, "Song A") && !strcmp(i.artist, "Band A"));
  auto r4 = tag(4, {frame(4, "TPE1", text(3, bytes("Beyonc\xC3\xA9"))), frame(4, "TIT2", text(3, bytes("Se\xC3\xB1orita")))});
  CHECK(readId3(r4, i) && !strcmp(i.title, "Senorita") && !strcmp(i.artist, "Beyonce"));
  auto r2 = tag(2, {frame(2, "TT2", text(0, bytes("Old Song"))), frame(2, "TP1", text(0, bytes("Old Band")))});
  CHECK(readId3(r2, i) && !strcmp(i.title, "Old Song") && !strcmp(i.artist, "Old Band"));
  std::vector<uint8_t> u16 = {0xFF, 0xFE, 'H', 0, 'i', 0};
  auto rU = tag(3, {frame(3, "TIT2", text(1, u16))});
  CHECK(readId3(rU, i) && !strcmp(i.title, "Hi") && !i.hasArtist);
  std::vector<uint8_t> apic(5000, 7);                                    // big cover art BEFORE the text frames
  auto rA = tag(3, {frame(3, "APIC", apic), frame(3, "TIT2", text(0, bytes("After Art"))), frame(3, "TPE1", text(0, bytes("Z")))});
  CHECK(readId3(rA, i) && !strcmp(i.title, "After Art") && !strcmp(i.artist, "Z"));
  auto rE = tag(4, {frame(4, "TIT2", text(3, bytes("Ext")))}, true);
  CHECK(readId3(rE, i) && !strcmp(i.title, "Ext"));
  MemReader none; none.d = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 0, 0, 0, 0};
  CHECK(!readId3(none, i));
  MemReader trunc = r3; trunc.d.resize(14);
  CHECK(!readId3(trunc, i));
  std::string longT(300, 'x');
  auto rL = tag(3, {frame(3, "TIT2", text(0, bytes(longT.c_str())))});
  CHECK(readId3(rL, i) && strlen(i.title) == sizeof(i.title) - 1);
}

static void test_playlist() {
  PlaylistNav p; p.init(5, 42);
  CHECK(p.current() == 0 && p.next(false) == 1 && p.next(true) == 2 && p.prev() == 1);
  p.jumpTo(4); CHECK(p.next(true) == -1);                 // end, repeat off, auto -> stop
  CHECK(p.next(false) == 0);                              // manual skip wraps
  p.setRepeat(REP_ALL); p.jumpTo(4); CHECK(p.next(true) == 0);
  p.setRepeat(REP_ONE); p.jumpTo(2); CHECK(p.next(true) == 2 && p.next(false) == 3);
  p.setRepeat(REP_OFF);
  // shuffle: every track exactly once per cycle, current stays first
  PlaylistNav s; s.init(50, 7); s.jumpTo(10); s.setShuffle(true);
  CHECK(s.current() == 10);
  std::set<int> seen{10};
  for (int i = 0; i < 49; i++) { int t = s.next(true); CHECK(t >= 0); seen.insert(t); }
  CHECK(seen.size() == 50);
  CHECK(s.next(true) == -1);                              // cycle finished, repeat off
  s.setRepeat(REP_ALL);
  int last = s.current(); int t = s.next(true); CHECK(t != last);   // reshuffle never repeats immediately
  s.setShuffle(false); CHECK(s.current() == t);
  PlaylistNav e; e.init(0, 1); CHECK(e.current() == -1 && e.next(false) == -1 && e.prev() == -1);
  PlaylistNav one; one.init(1, 1); one.setShuffle(true); CHECK(one.next(false) == 0 && one.prev() == 0);
  PlaylistNav big; big.init(1000, 1); CHECK(big.count() == PlaylistNav::MAXN);
}

static void test_battery() {
  CHECK(batteryPercent(4300) == 100 && batteryPercent(4200) == 100 && batteryPercent(3000) == 0);
  int prev = 101;
  for (int mv = 4200; mv >= 3200; mv -= 10) { int p = batteryPercent(mv); CHECK(p <= prev && p >= 0 && p <= 100); prev = p; }
  CHECK(batteryPercent(3850) == 55 && batteryPercent(3700) > 10 && batteryPercent(3700) < 15);
  CHECK(batteryMvFromAdc(1900, 2.0f) == 3800);
}

int main() {
  test_text(); test_id3(); test_playlist(); test_battery();
  printf(fails ? "\n%d FAILED\n" : "all core tests passed\n", fails);
  return fails ? 1 : 0;
}
