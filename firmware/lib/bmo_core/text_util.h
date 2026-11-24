// Hardware-independent text helpers. Pure C++ (host-testable).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace bmo {

// Fold a Unicode code point to one printable ASCII char (the 5x7 GFX font is ASCII only).
inline char foldCodepoint(uint32_t cp) {
  static const char up[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTs";   // U+00C0..U+00DF
  static const char lo[] = "aaaaaaaceeeeiiiidnooooo/ouuuuyty";   // U+00E0..U+00FF
  if (cp >= 32 && cp < 127) return (char)cp;
  if (cp >= 0xC0 && cp <= 0xDF) return up[cp - 0xC0];
  if (cp >= 0xE0 && cp <= 0xFF) return lo[cp - 0xE0];
  if (cp == 0x2018 || cp == 0x2019) return '\'';
  if (cp == 0x201C || cp == 0x201D) return '"';
  if (cp == 0x2013 || cp == 0x2014) return '-';
  if (cp == 0xA1) return '!';
  if (cp == 0xBF) return '?';
  if (cp < 32) return ' ';
  return '?';
}

inline void putc_safe(char* out, size_t outSize, size_t& o, char c) {
  if (o + 1 < outSize) out[o++] = c;
}

inline size_t latin1ToAscii(const uint8_t* s, size_t n, char* out, size_t outSize) {
  size_t o = 0;
  for (size_t i = 0; i < n && s[i]; i++) putc_safe(out, outSize, o, foldCodepoint(s[i]));
  if (outSize) out[o] = 0;
  return o;
}

inline size_t utf8ToAscii(const uint8_t* s, size_t n, char* out, size_t outSize) {
  size_t o = 0, i = 0;
  while (i < n && s[i]) {
    uint32_t cp = s[i];
    int extra = 0;
    if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
    else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
    else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
    i++;
    while (extra-- > 0 && i < n && (s[i] & 0xC0) == 0x80) cp = (cp << 6) | (s[i++] & 0x3F);
    putc_safe(out, outSize, o, foldCodepoint(cp));
  }
  if (outSize) out[o] = 0;
  return o;
}

// UTF-16 with optional BOM. bigEndianDefault is used when no BOM is present.
inline size_t utf16ToAscii(const uint8_t* s, size_t n, bool bigEndianDefault, char* out, size_t outSize) {
  size_t o = 0, i = 0;
  bool be = bigEndianDefault;
  if (n >= 2 && s[0] == 0xFF && s[1] == 0xFE) { be = false; i = 2; }
  else if (n >= 2 && s[0] == 0xFE && s[1] == 0xFF) { be = true; i = 2; }
  while (i + 1 < n) {
    uint32_t cp = be ? ((s[i] << 8) | s[i + 1]) : ((s[i + 1] << 8) | s[i]);
    i += 2;
    if (cp == 0) break;
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < n) {            // surrogate pair -> '?'
      i += 2;
      cp = 0x3F;
    }
    putc_safe(out, outSize, o, foldCodepoint(cp));
  }
  if (outSize) out[o] = 0;
  return o;
}

// "/music/Artist/Song Name.mp3" -> "Song Name"
inline void titleFromPath(const char* path, char* out, size_t outSize) {
  const char* base = path;
  for (const char* p = path; *p; p++) if (*p == '/') base = p + 1;
  size_t len = 0;
  while (base[len]) len++;
  if (len > 4 && base[len - 4] == '.') len -= 4;
  uint8_t tmp[96];
  size_t n = len < sizeof(tmp) ? len : sizeof(tmp);
  for (size_t i = 0; i < n; i++) tmp[i] = (uint8_t)base[i];
  utf8ToAscii(tmp, n, out, outSize);
}

inline bool endsWithMp3(const char* name) {
  size_t n = 0;
  while (name[n]) n++;
  if (n < 5) return false;
  const char* e = name + n - 4;
  return e[0] == '.' && (e[1] | 32) == 'm' && (e[2] | 32) == 'p' && e[3] == '3';
}

// Marquee: first visible character index for a string of `len` chars shown in `visible` columns.
inline int marqueeStart(int len, int visible, uint32_t tick, int pauseTicks = 12) {
  if (len <= visible) return 0;
  int span = len - visible;
  uint32_t cycle = (uint32_t)(span + 2 * pauseTicks);
  int t = (int)(tick % cycle);
  if (t < pauseTicks) return 0;
  if (t < pauseTicks + span) return t - pauseTicks;
  return span;
}

}  // namespace bmo
