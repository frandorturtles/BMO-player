// LiPo state-of-charge estimate. Generic single-cell curve (resting voltage).
// The reading is taken under load, so treat it as an indication, not a fuel gauge.
#pragma once
#include <stdint.h>

namespace bmo {

inline int batteryPercent(int mv) {
  static const int16_t V[] = {4200, 4150, 4110, 4080, 4020, 3980, 3950, 3910, 3870, 3850, 3840,
                              3820, 3800, 3790, 3770, 3750, 3730, 3710, 3690, 3610, 3270};
  static const int8_t P[]  = { 100,   95,   90,   85,   80,   75,   70,   65,   60,   55,   50,
                                45,   40,   35,   30,   25,   20,   15,   10,    5,    0};
  const int n = sizeof(V) / sizeof(V[0]);
  if (mv >= V[0]) return 100;
  if (mv <= V[n - 1]) return 0;
  for (int i = 0; i < n - 1; i++) {
    if (mv <= V[i] && mv > V[i + 1]) {
      int span = V[i] - V[i + 1];
      return P[i + 1] + (P[i] - P[i + 1]) * (mv - V[i + 1]) / span;
    }
  }
  return 0;
}

// adcMv: millivolts measured at the ADC pin; divider = (Rtop+Rbottom)/Rbottom (2.0 for 100k/100k)
inline int batteryMvFromAdc(int adcMv, float divider) { return (int)(adcMv * divider + 0.5f); }

}  // namespace bmo
