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
