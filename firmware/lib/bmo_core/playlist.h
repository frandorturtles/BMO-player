// Playlist navigation with shuffle / repeat. Pure C++ (host-testable).
#pragma once
#include <stdint.h>

namespace bmo {

enum RepeatMode : uint8_t { REP_OFF = 0, REP_ALL = 1, REP_ONE = 2 };

class PlaylistNav {
 public:
  static constexpr int MAXN = 400;

  void init(int count, uint32_t seed) {
    n_ = count > MAXN ? MAXN : count;
    rng_ = seed ? seed : 0x1234567u;
    for (int i = 0; i < n_; i++) order_[i] = (uint16_t)i;
    pos_ = 0;
    if (shuffle_) rebuild(0);
  }
  int count() const { return n_; }
  int current() const { return n_ ? order_[pos_] : -1; }
  bool shuffle() const { return shuffle_; }
  RepeatMode repeat() const { return rep_; }
  void setRepeat(RepeatMode r) { rep_ = r; }

  void setShuffle(bool on) {
    if (on == shuffle_) return;
    shuffle_ = on;
    if (!n_) return;
    int cur = current();
    if (on) rebuild(cur);
    else { for (int i = 0; i < n_; i++) order_[i] = (uint16_t)i; pos_ = cur; }
  }

  void jumpTo(int track) {
    if (track < 0 || track >= n_) return;
    for (int i = 0; i < n_; i++) if (order_[i] == track) { pos_ = i; return; }
  }

  // autoAdvance=true: called when a track ended by itself. Returns -1 when playback should stop.
  int next(bool autoAdvance) {
    if (!n_) return -1;
    if (autoAdvance && rep_ == REP_ONE) return current();
    if (pos_ + 1 < n_) { pos_++; return current(); }
    if (autoAdvance && rep_ == REP_OFF) return -1;
    int last = current();                                   // wrap
    if (shuffle_) { rebuild(-1); if (n_ > 1 && order_[0] == last) swap(0, 1 + (int)(rnd() % (uint32_t)(n_ - 1))); }
    pos_ = 0;
    return current();
  }

  int prev() {
    if (!n_) return -1;
    pos_ = (pos_ > 0) ? pos_ - 1 : n_ - 1;
    return current();
  }

 private:
  uint32_t rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }
  void swap(int a, int b) { uint16_t t = order_[a]; order_[a] = order_[b]; order_[b] = t; }
  // Fisher-Yates; if keepFirst>=0 that track is placed at position 0.
  void rebuild(int keepFirst) {
    for (int i = 0; i < n_; i++) order_[i] = (uint16_t)i;
    for (int i = n_ - 1; i > 0; i--) swap(i, (int)(rnd() % (uint32_t)(i + 1)));
    if (keepFirst >= 0) for (int i = 0; i < n_; i++) if (order_[i] == keepFirst) { swap(0, i); break; }
    pos_ = 0;
  }
  uint16_t order_[MAXN];
  int n_ = 0, pos_ = 0;
  bool shuffle_ = false;
  RepeatMode rep_ = REP_OFF;
  uint32_t rng_ = 1;
};

}  // namespace bmo
