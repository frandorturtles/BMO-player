// Minimal ID3v2 (v2.2 / v2.3 / v2.4) title + artist reader. Pure C++ (host-testable).
// Reader must provide:  size_t read(uint32_t offset, uint8_t* dst, size_t n)
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "text_util.h"

namespace bmo {

struct Id3Info {
  char title[40];
  char artist[28];
  bool hasTitle;
  bool hasArtist;
  void clear() { title[0] = artist[0] = 0; hasTitle = hasArtist = false; }
};

namespace id3_detail {
inline uint32_t syncsafe(const uint8_t* p) {
  return ((uint32_t)(p[0] & 0x7F) << 21) | ((uint32_t)(p[1] & 0x7F) << 14) | ((uint32_t)(p[2] & 0x7F) << 7) | (p[3] & 0x7F);
}
inline uint32_t be32(const uint8_t* p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
inline void decodeText(const uint8_t* d, size_t n, char* out, size_t outSize) {
  if (n < 1) { if (outSize) out[0] = 0; return; }
  uint8_t enc = d[0];
  switch (enc) {
    case 0:  latin1ToAscii(d + 1, n - 1, out, outSize); break;
    case 1:  utf16ToAscii(d + 1, n - 1, false, out, outSize); break;
    case 2:  utf16ToAscii(d + 1, n - 1, true, out, outSize); break;
    default: utf8ToAscii(d + 1, n - 1, out, outSize); break;
  }
}
}  // namespace id3_detail

template <class Reader>
bool readId3(Reader& rd, Id3Info& out) {
  using namespace id3_detail;
  out.clear();
  uint8_t h[10];
  if (rd.read(0, h, 10) != 10) return false;
  if (h[0] != 'I' || h[1] != 'D' || h[2] != '3') return false;
  uint8_t major = h[3];
  if (major < 2 || major > 4) return false;
  uint8_t flags = h[5];
  uint32_t tagSize = syncsafe(h + 6);
  uint32_t off = 10;
  uint32_t end = 10 + tagSize;
  if (major >= 3 && (flags & 0x40)) {                      // extended header
    uint8_t eh[6];
    if (rd.read(off, eh, 4) != 4) return false;
    uint32_t es = (major == 4) ? syncsafe(eh) : be32(eh) + 4;
    off += es;
  }
  const size_t hdr = (major == 2) ? 6 : 10;
  for (int guard = 0; guard < 96 && off + hdr <= end; guard++) {
    uint8_t fh[10];
    if (rd.read(off, fh, hdr) != hdr) break;
    if (fh[0] == 0) break;                                   // padding
    char id[5] = {0};
    uint32_t size;
    if (major == 2) {
      memcpy(id, fh, 3);
      size = ((uint32_t)fh[3] << 16) | ((uint32_t)fh[4] << 8) | fh[5];
    } else {
      memcpy(id, fh, 4);
      size = (major == 4) ? syncsafe(fh + 4) : be32(fh + 4);
    }
    if (size == 0 || off + hdr + size > end + 1) break;
    bool isTitle = (!strcmp(id, "TIT2") || !strcmp(id, "TT2"));
    bool isArtist = (!strcmp(id, "TPE1") || !strcmp(id, "TP1"));
    if (isTitle || isArtist) {
      uint8_t buf[160];
      size_t n = size < sizeof(buf) ? size : sizeof(buf);
      if (rd.read(off + hdr, buf, n) != n) break;
      if (isTitle) { decodeText(buf, n, out.title, sizeof(out.title)); out.hasTitle = out.title[0] != 0; }
      else { decodeText(buf, n, out.artist, sizeof(out.artist)); out.hasArtist = out.artist[0] != 0; }
      if (out.hasTitle && out.hasArtist) break;
    }
    off += hdr + size;
  }
  return out.hasTitle || out.hasArtist;
}

}  // namespace bmo
