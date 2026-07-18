#pragma once

#include <TFT_eSPI.h>

#include "cadenza/core/presenter.h"

class TftPresenter final : public cadenza::FramebufferPresenter {
 public:
  explicit TftPresenter(TFT_eSPI& display) noexcept : display_(display) {}

  bool present(const cadenza::MonoFramebuffer& framebuffer) noexcept override;

 private:
  TFT_eSPI& display_;
};
