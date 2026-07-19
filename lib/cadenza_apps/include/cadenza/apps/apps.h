#pragma once

#include <cstddef>
#include <cstdint>

#include "cadenza/animation/composition.h"
#include "cadenza/animation/frame_animation.h"
#include "cadenza/animation/spring.h"
#include "cadenza/animation/state_machine.h"
#include "cadenza/animation/tween.h"
#include "cadenza/apps/builtin_app_ids.h"
#include "cadenza/core/app.h"
#include "cadenza/core/transition.h"
#include "cadenza/presentation/defaults.h"
#include "cadenza/presentation/effects.h"
#include "cadenza/presentation/particles.h"

namespace cadenza {

enum class SightStep : std::uint8_t { C, D, E, F, G, A, B };
enum class SightAccidental : std::int8_t { Flat = -1, Natural = 0, Sharp = 1 };

struct SpelledPitch {
  SightStep step = SightStep::C;
  SightAccidental accidental = SightAccidental::Natural;
  std::int8_t octave = 4;

  std::uint8_t midiNote() const noexcept;
};

struct SightCard {
  std::array<SpelledPitch, 3> pitches{};
  std::uint8_t count = 1;
  bool minor = false;
};

void formatSightChordSymbol(const SightCard& card, char* output,
                            std::size_t capacity) noexcept;

enum class SightLevel : std::uint8_t { Level1, AllNotes, CommonChords };
enum class SightScreen : std::uint8_t {
  LevelPicker,
  Question,
  Answer,
  AnswerExiting,
};
enum class SightAnswerAction : std::uint8_t { Replay, Next, Level };
enum class SightPageTransition : std::uint8_t { None, ToQuestion, ToLevel };

class SightApp final : public App {
 public:
  static constexpr Seconds kLevelTransitionSeconds = 0.18F;
  static constexpr Seconds kAnswerRevealSeconds = 0.20F;
  static constexpr Seconds kNoteEntranceSeconds = 0.24F;
  static constexpr Seconds kPageTransitionSeconds = 0.22F;

  const char* name() const noexcept override { return "SIGHT"; }
  void onEnter() noexcept override;
  void update(const AppUpdateContext& context) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRenderContext& context) noexcept override;
  bool renderLauncherCover(MonoCanvas& canvas,
                           Rect bounds) const noexcept override;

  SightLevel level() const noexcept { return level_; }
  SightScreen screen() const noexcept { return screen_; }
  SightAnswerAction answerAction() const noexcept { return answerAction_; }
  SightPageTransition pageTransition() const noexcept {
    return pageTransition_;
  }
  const SightCard& card() const noexcept { return card_; }
  std::uint8_t candidateCount() const noexcept;

 private:
  void resetBag() noexcept;
  void nextCard() noexcept;
  void playCurrent(const AppUpdateContext& context) noexcept;
  void renderStudyPage(MonoCanvas& canvas,
                       const AppRenderContext& context,
                       SightScreen studyScreen) noexcept;

  SightLevel level_ = SightLevel::Level1;
  SightLevel previousLevel_ = SightLevel::Level1;
  SightScreen screen_ = SightScreen::LevelPicker;
  SightAnswerAction answerAction_ = SightAnswerAction::Next;
  SightPageTransition pageTransition_ = SightPageTransition::None;
  SightCard card_{};
  std::array<std::uint8_t, 29> bag_{};
  std::uint8_t bagSize_ = 0;
  std::uint8_t bagPosition_ = 0;
  std::uint8_t previousCandidate_ = 0xFF;
  std::uint32_t randomState_ = 0x51A17U;
  Seconds levelTransitionElapsed_ = kLevelTransitionSeconds;
  Seconds answerRevealElapsed_ = kAnswerRevealSeconds;
  Seconds noteEntranceElapsed_ = kNoteEntranceSeconds;
  Seconds pageTransitionElapsed_ = kPageTransitionSeconds;
  std::int8_t levelTransitionDirection_ = 0;
};

class LauncherApp final : public App {
 public:
  const char* name() const noexcept override { return "Launcher"; }
  void onEnter() noexcept override;
  void update(const AppUpdateContext& context) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRenderContext& context) noexcept override;

  int selectedIndex() const noexcept { return selected_; }
  std::int64_t targetPosition() const noexcept { return targetPosition_; }
  float visualPosition() const noexcept { return position_; }
  bool settled() const noexcept { return settled_; }
 private:
  int selected_ = 0;
  std::int64_t targetPosition_ = 0;
  float position_ = 0.0F;
  float animationStart_ = 0.0F;
  Seconds motionElapsed_ = 0.0F;
  MotionProfile motionProfile_ = MotionProfile::Normal;
  bool settled_ = true;
};

enum class TimerPresentationState : std::uint8_t {
  None,
  Starting,
  Pausing,
  Resuming,
};

class TimerApp final : public App {
 public:
  const char* name() const noexcept override { return "TIMER"; }
  void onEnter() noexcept override;
  void onExit() noexcept override;
  void update(const AppUpdateContext& context) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRenderContext& context) noexcept override;
  bool renderLauncherCover(MonoCanvas& canvas,
                           Rect bounds) const noexcept override;
  bool renderLaunchFrame(MonoCanvas& canvas,
                         float progress,
                         const AppRenderContext& context) const noexcept override;

  TimerPresentationState presentationState() const noexcept {
    return presentationState_;
  }
  float presentationElapsed() const noexcept { return presentationElapsed_; }

 private:
  std::uint32_t selectedDurationMs_ =
      static_cast<std::uint32_t>(kTimerDefaultDurationMs);
  TimerState observedTimerState_ = TimerState::Ready;
  bool hasObservedTimerState_ = false;
  TimerPresentationState presentationState_ = TimerPresentationState::None;
  float presentationElapsed_ = 0.0F;
};

class MotionApp final : public App {
 public:
  const char* name() const noexcept override { return "Motion"; }
  void onEnter() noexcept override;
  void update(const AppUpdateContext& context) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRenderContext& context) noexcept override;
  bool renderLauncherCover(MonoCanvas& canvas,
                           Rect bounds) const noexcept override;
  bool renderLaunchFrame(MonoCanvas& canvas,
                         float progress,
                         const AppRenderContext& context) const noexcept override;

 private:
  float target_ = 0.5F;
  float x_ = 0.5F;
  float velocity_ = 0.0F;
  float time_ = 0.0F;
};

class SettingsApp final : public App {
 public:
  const char* name() const noexcept override { return "Settings"; }
  void onEnter() noexcept override;
  void update(const AppUpdateContext& context) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRenderContext& context) noexcept override;
  bool renderLauncherCover(MonoCanvas& canvas,
                           Rect bounds) const noexcept override;
  bool renderLaunchFrame(MonoCanvas& canvas,
                         float progress,
                         const AppRenderContext& context) const noexcept override;
  bool showingAbout() const noexcept { return showingAbout_; }

 private:
  int selected_ = 0;
  float time_ = 0.0F;
  bool resetConfirmArmed_ = false;
  bool showingAbout_ = false;
};

enum class GalleryMode : std::uint8_t { AutoPlay, Scrub };

class AnimationGalleryApp final : public App {
 public:
  static constexpr std::size_t kPageCount = 14;

  AnimationGalleryApp() noexcept;
  const char* name() const noexcept override { return "Animation Gallery"; }
  void onEnter() noexcept override;
  void update(const AppUpdateContext& context) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRenderContext& context) noexcept override;
  bool renderLauncherCover(MonoCanvas& canvas,
                           Rect bounds) const noexcept override;
  bool renderLaunchFrame(MonoCanvas& canvas,
                         float progress,
                         const AppRenderContext& context) const noexcept override;

  constexpr std::size_t pageCount() const noexcept { return kPageCount; }
  const char* pageName(std::size_t index) const noexcept;
  std::size_t currentPage() const noexcept { return page_; }
  GalleryMode mode() const noexcept { return mode_; }
  float progress() const noexcept { return progress_; }
  MotionProfile motionProfile() const noexcept { return motionProfile_; }

 private:
  static const std::uint8_t kSpritePixels[4];

  void resetDemo() noexcept;
  void restartDemoState() noexcept;
  void reconstructDemoState() noexcept;
  void advanceDemoState(Seconds delta) noexcept;
  void applyMotionProfile(MotionProfile profile) noexcept;
  void applyProgress() noexcept;
  void ensureTransitionBuffers(const MonoCanvas& canvas) noexcept;
  void renderHeader(MonoCanvas& canvas) noexcept;
  void renderEasing(MonoCanvas& canvas) noexcept;
  void renderComposition(MonoCanvas& canvas) noexcept;
  void renderSpring(MonoCanvas& canvas) noexcept;
  void renderTransition(MonoCanvas& canvas, const Transition& transition) noexcept;
  void renderSelection(MonoCanvas& canvas) noexcept;
  void renderCamera(MonoCanvas& canvas) noexcept;
  void renderParticles(MonoCanvas& canvas) noexcept;
  void renderSpriteState(MonoCanvas& canvas) noexcept;
  void renderDither(MonoCanvas& canvas) noexcept;
  void renderInitialFrame(MonoCanvas& canvas) const noexcept;

  std::size_t page_ = 0;
  GalleryMode mode_ = GalleryMode::AutoPlay;
  MotionProfile motionProfile_ = MotionProfile::Normal;
  float progress_ = 0.0F;
  std::int16_t demoWidth_ = 320;
  std::int16_t demoHeight_ = 170;
  Tween<float> firstTween_{0.0F, 1.0F, 0.8F, Easing::OutBack};
  Tween<float> secondTween_{0.0F, 1.0F, 0.8F, Easing::InOutCubic};
  Timeline<2> timeline_;
  Spring spring_{0.0F};
  SelectionFeedback selection_{MotionProfile::Normal};
  CameraPunch punch_{presentation_defaults::kCameraPunchAmplitude,
                     presentation_defaults::kCameraPunchDuration};
  CameraShake shake_{0xCADAU, presentation_defaults::kCameraShakeAmplitude,
                     presentation_defaults::kCameraShakeDuration};
  ParticlePool<24> particles_{ParticleFullPolicy::ReplaceOldest};
  ParticleEmitter emitter_{0x1B17U};
  SpriteAtlas<4> spriteAtlas_{{kSpritePixels, 8, 4, 1}};
  FrameAnimation idle_{spriteAtlas_, 0, 2, 0.18F,
                       FramePlayback::PingPong};
  FrameAnimation active_{spriteAtlas_, 2, 2, 0.10F,
                         FramePlayback::PingPong};
  AnimationStateMachine<2, 2> spriteMachine_;
  MonoFramebuffer transitionFrom_{FramebufferProfile::TEmbed};
  MonoFramebuffer transitionTo_{FramebufferProfile::TEmbed};
};

}  // namespace cadenza
