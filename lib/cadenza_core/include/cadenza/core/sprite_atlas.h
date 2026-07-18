#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "cadenza/core/diagnostics.h"
#include "cadenza/core/mono_canvas.h"

namespace cadenza {

struct SpriteFrame {
  const char* name = nullptr;
  Rect source;
};

template <std::size_t Capacity>
class SpriteAtlas {
 public:
  explicit SpriteAtlas(BitmapView bitmap,
                       DiagnosticSink* diagnostics = nullptr) noexcept
      : bitmap_(bitmap), diagnostics_(diagnostics) {}

  bool add(const char* name, Rect source) noexcept {
    if (size_ == Capacity) {
      emit(DiagnosticCode::CapacityExceeded, "sprite atlas is full");
      return false;
    }
    if (!name || name[0] == '\0' || !validSource(source)) {
      emit(DiagnosticCode::InvalidGeometry, "invalid sprite frame");
      return false;
    }
    if (findIndex(name) < size_) {
      emit(DiagnosticCode::InvalidOperation, "duplicate sprite frame name");
      return false;
    }
    frames_[size_++] = {name, source};
    return true;
  }

  const SpriteFrame* frame(std::size_t index) const noexcept {
    if (index >= size_) {
      emit(DiagnosticCode::InvalidOperation, "sprite frame index not found");
      return nullptr;
    }
    return &frames_[index];
  }

  const SpriteFrame* find(const char* name) const noexcept {
    const std::size_t index = name ? findIndex(name) : size_;
    if (index == size_) {
      emit(DiagnosticCode::InvalidOperation, "sprite frame name not found");
      return nullptr;
    }
    return &frames_[index];
  }

  const BitmapView& bitmap() const noexcept { return bitmap_; }
  std::size_t size() const noexcept { return size_; }
  static constexpr std::size_t capacity() noexcept { return Capacity; }

 private:
  bool validSource(Rect source) const noexcept {
    if (!bitmap_.valid() || source.x < 0 || source.y < 0 ||
        source.width <= 0 || source.height <= 0) {
      return false;
    }
    return static_cast<std::int64_t>(source.x) + source.width <= bitmap_.width &&
           static_cast<std::int64_t>(source.y) + source.height <= bitmap_.height;
  }

  std::size_t findIndex(const char* name) const noexcept {
    for (std::size_t index = 0; index < size_; ++index) {
      if (std::strcmp(frames_[index].name, name) == 0) return index;
    }
    return size_;
  }

  void emit(DiagnosticCode code, const char* message) const noexcept {
    if (diagnostics_) {
      diagnostics_->emit({DiagnosticCategory::Graphics, code, message,
                          static_cast<std::int32_t>(size_)});
    }
  }

  BitmapView bitmap_;
  DiagnosticSink* diagnostics_ = nullptr;
  std::array<SpriteFrame, Capacity> frames_{};
  std::size_t size_ = 0;
};

}  // namespace cadenza
