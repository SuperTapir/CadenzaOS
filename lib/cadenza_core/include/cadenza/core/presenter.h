#pragma once

#include "cadenza/core/mono_framebuffer.h"

namespace cadenza {

class FramebufferPresenter {
 public:
  virtual ~FramebufferPresenter() = default;
  virtual bool present(const MonoFramebuffer& framebuffer) noexcept = 0;
};

}  // namespace cadenza
