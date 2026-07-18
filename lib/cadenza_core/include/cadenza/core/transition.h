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
  explicit constexpr VenetianBlindsTransition(
      std::uint8_t bladeCount = 8) noexcept
      : bladeCount_(bladeCount < 1 ? 1 : (bladeCount > 32 ? 32 : bladeCount)) {}
  void compose(const MonoFramebuffer& outgoing,
               const MonoFramebuffer& incoming, MonoCanvas& output,
               float progress) const noexcept override;
  std::uint8_t bladeCount() const noexcept { return bladeCount_; }

 private:
  std::uint8_t bladeCount_ = 8;
};

class DipTransition final : public Transition {
 public:
  void compose(const MonoFramebuffer& outgoing,
               const MonoFramebuffer& incoming, MonoCanvas& output,
               float progress) const noexcept override;
};

class HorizontalWipeTransition final : public Transition {
 public:
  void compose(const MonoFramebuffer& outgoing,
               const MonoFramebuffer& incoming, MonoCanvas& output,
               float progress) const noexcept override;
};

class DiagonalWipeTransition final : public Transition {
 public:
  void compose(const MonoFramebuffer& outgoing,
               const MonoFramebuffer& incoming, MonoCanvas& output,
               float progress) const noexcept override;
};

class IrisTransition final : public Transition {
 public:
  void compose(const MonoFramebuffer& outgoing,
               const MonoFramebuffer& incoming, MonoCanvas& output,
               float progress) const noexcept override;
};

class CheckerDissolveTransition final : public Transition {
 public:
  void compose(const MonoFramebuffer& outgoing,
               const MonoFramebuffer& incoming, MonoCanvas& output,
               float progress) const noexcept override;
};

extern const CutTransition kCutTransition;
extern const VenetianBlindsTransition kVenetianBlindsTransition;
extern const DipTransition kDipTransition;
extern const HorizontalWipeTransition kHorizontalWipeTransition;
extern const DiagonalWipeTransition kDiagonalWipeTransition;
extern const IrisTransition kIrisTransition;
extern const CheckerDissolveTransition kCheckerDissolveTransition;

}  // namespace cadenza
