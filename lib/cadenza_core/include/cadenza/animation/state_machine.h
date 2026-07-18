#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "cadenza/animation/frame_animation.h"
#include "cadenza/core/diagnostics.h"

namespace cadenza {

enum class AnimationResetPolicy : std::uint8_t {
  Restart,
  Preserve,
};

enum class AnimationTransitionResult : std::uint8_t {
  Changed,
  NoMatch,
  NotStarted,
};

template <std::size_t StateCapacity, std::size_t TransitionCapacity>
class AnimationStateMachine {
 public:
  explicit AnimationStateMachine(
      DiagnosticSink* diagnostics = nullptr) noexcept
      : diagnostics_(diagnostics) {}

  bool addState(const char* name, FrameAnimation& animation) noexcept {
    if (!validName(name) || !animation.valid()) {
      emit(DiagnosticCode::InvalidOperation, "invalid animation state");
      return false;
    }
    if (findState(name) < stateCount_) {
      emit(DiagnosticCode::InvalidOperation, "duplicate animation state");
      return false;
    }
    if (stateCount_ == StateCapacity) {
      emit(DiagnosticCode::CapacityExceeded, "animation state table is full");
      return false;
    }
    states_[stateCount_++] = {name, &animation};
    return true;
  }

  bool addTransition(
      const char* from, const char* trigger, const char* to,
      AnimationResetPolicy reset = AnimationResetPolicy::Restart) noexcept {
    const std::size_t fromIndex = validName(from) ? findState(from) : stateCount_;
    const std::size_t toIndex = validName(to) ? findState(to) : stateCount_;
    if (!validName(trigger) || fromIndex == stateCount_ ||
        toIndex == stateCount_) {
      emit(DiagnosticCode::InvalidOperation, "invalid animation transition");
      return false;
    }
    for (std::size_t index = 0; index < transitionCount_; ++index) {
      if (transitions_[index].from == fromIndex &&
          same(transitions_[index].trigger, trigger)) {
        emit(DiagnosticCode::InvalidOperation,
             "duplicate animation transition trigger");
        return false;
      }
    }
    if (transitionCount_ == TransitionCapacity) {
      emit(DiagnosticCode::CapacityExceeded,
           "animation transition table is full");
      return false;
    }
    transitions_[transitionCount_++] = {fromIndex, trigger, toIndex, reset};
    return true;
  }

  bool start(const char* state) noexcept {
    const std::size_t index = validName(state) ? findState(state) : stateCount_;
    if (index == stateCount_) {
      emit(DiagnosticCode::InvalidOperation, "animation state not found");
      return false;
    }
    current_ = index;
    states_[current_].animation->reset();
    return true;
  }

  AnimationTransitionResult trigger(const char* triggerName) noexcept {
    if (current_ == kNoState) return AnimationTransitionResult::NotStarted;
    if (!validName(triggerName)) {
      emit(DiagnosticCode::InvalidOperation, "invalid animation trigger");
      return AnimationTransitionResult::NoMatch;
    }
    for (std::size_t index = 0; index < transitionCount_; ++index) {
      const Transition& transition = transitions_[index];
      if (transition.from == current_ &&
          same(transition.trigger, triggerName)) {
        current_ = transition.to;
        if (transition.reset == AnimationResetPolicy::Restart) {
          states_[current_].animation->reset();
        }
        return AnimationTransitionResult::Changed;
      }
    }
    return AnimationTransitionResult::NoMatch;
  }

  void update(Seconds delta) noexcept {
    if (FrameAnimation* currentAnimation = animation()) {
      currentAnimation->update(delta);
    }
  }

  const char* currentState() const noexcept {
    return current_ == kNoState ? nullptr : states_[current_].name;
  }

  FrameAnimation* animation() noexcept {
    return current_ == kNoState ? nullptr : states_[current_].animation;
  }

  const FrameAnimation* animation() const noexcept {
    return current_ == kNoState ? nullptr : states_[current_].animation;
  }

  std::size_t stateCount() const noexcept { return stateCount_; }
  std::size_t transitionCount() const noexcept { return transitionCount_; }

 private:
  struct State {
    const char* name = nullptr;
    FrameAnimation* animation = nullptr;
  };

  struct Transition {
    std::size_t from = 0;
    const char* trigger = nullptr;
    std::size_t to = 0;
    AnimationResetPolicy reset = AnimationResetPolicy::Restart;
  };

  static constexpr std::size_t kNoState = StateCapacity;

  static bool validName(const char* name) noexcept {
    return name != nullptr && name[0] != '\0';
  }

  static bool same(const char* left, const char* right) noexcept {
    return std::strcmp(left, right) == 0;
  }

  std::size_t findState(const char* name) const noexcept {
    for (std::size_t index = 0; index < stateCount_; ++index) {
      if (same(states_[index].name, name)) return index;
    }
    return stateCount_;
  }

  void emit(DiagnosticCode code, const char* message) const noexcept {
    if (diagnostics_) {
      diagnostics_->emit({DiagnosticCategory::Animation, code, message,
                          static_cast<std::int32_t>(stateCount_)});
    }
  }

  DiagnosticSink* diagnostics_ = nullptr;
  std::array<State, StateCapacity> states_{};
  std::array<Transition, TransitionCapacity> transitions_{};
  std::size_t stateCount_ = 0;
  std::size_t transitionCount_ = 0;
  std::size_t current_ = kNoState;
};

}  // namespace cadenza
