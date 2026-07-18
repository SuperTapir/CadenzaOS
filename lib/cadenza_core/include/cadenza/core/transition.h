#pragma once

#include "cadenza/core/mono_canvas.h"
#include "cadenza/core/mono_framebuffer.h"

namespace cadenza {

class Transition {
 public:
  virtual ~Transition() = default;
  virtual void compose(const MonoFramebuffer& outgoing,
                       const MonoFramebuffer& incoming, MonoCanvas& output,
                       float progress) const noexcept = 0;
};

class CutTransition final : public Transition {
 public:
  void compose(const MonoFramebuffer& outgoing,
               const MonoFramebuffer& incoming, MonoCanvas& output,
               float progress) const noexcept override;
};

class VenetianBlindsTransition final : public Transition {
 public:
  void compose(const MonoFramebuffer& outgoing,
               const MonoFramebuffer& incoming, MonoCanvas& output,
               float progress) const noexcept override;
};

extern const CutTransition kCutTransition;
extern const VenetianBlindsTransition kVenetianBlindsTransition;

}  // namespace cadenza
