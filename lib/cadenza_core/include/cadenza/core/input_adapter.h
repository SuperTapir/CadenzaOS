#pragma once

#include "cadenza/core/input.h"

namespace cadenza {

class MonotonicClock {
 public:
  virtual ~MonotonicClock() = default;
  virtual MonotonicMillis nowMs() const noexcept = 0;
};

class RawInputSource {
 public:
  virtual ~RawInputSource() = default;

  // Returns only currently available events, in nondecreasing timestamp order.
  virtual bool poll(RawInputEvent& event) noexcept = 0;
};

inline void pumpInput(RawInputSource& source, const MonotonicClock& clock,
                      InputReducer& reducer) noexcept {
  RawInputEvent event;
  while (source.poll(event)) {
    reducer.push(event);
  }
  reducer.advanceTo(clock.nowMs());
}

}  // namespace cadenza
