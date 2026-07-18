#pragma once

#include <cstddef>
#include <cstdint>

#include "cadenza/animation/composition.h"
#include "cadenza/animation/frame_animation.h"
#include "cadenza/animation/spring.h"
#include "cadenza/animation/state_machine.h"
#include "cadenza/animation/tween.h"
#include "cadenza/core/app_runtime.h"
#include "cadenza/presentation/effects.h"
#include "cadenza/presentation/particles.h"

namespace cadenza {

class LauncherApp final : public App {
 public:
  const char* name() const noexcept override { return "Launcher"; }
  void onEnter() noexcept override;
  void update(Seconds dt, const InputFrame& input,
              AppRuntime& runtime) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRuntime& runtime) noexcept override;

 private:
  int selected_ = 0;
  float position_ = 0.0F;
  float pulse_ = 0.0F;
  float time_ = 0.0F;
};

class ClockApp final : public App {
 public:
  const char* name() const noexcept override { return "Clock"; }
  void onEnter() noexcept override;
  void update(Seconds dt, const InputFrame& input,
              AppRuntime& runtime) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRuntime& runtime) noexcept override;

 private:
  bool running_ = true;
  float elapsed_ = 0.0F;
  float phase_ = 0.0F;
};

class MotionApp final : public App {
 public:
  const char* name() const noexcept override { return "Motion"; }
  void onEnter() noexcept override;
  void update(Seconds dt, const InputFrame& input,
              AppRuntime& runtime) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRuntime& runtime) noexcept override;

 private:
  float target_ = 0.5F;
  float x_ = 0.5F;
  float velocity_ = 0.0F;
  float time_ = 0.0F;
};

class SettingsApp final : public App {
 public:
  const char* name() const noexcept override { return "Settings"; }
  void update(Seconds dt, const InputFrame& input,
              AppRuntime& runtime) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRuntime& runtime) noexcept override;

 private:
  int selected_ = 0;
  float time_ = 0.0F;
};

enum class GalleryMode : std::uint8_t { AutoPlay, Scrub };

class AnimationGalleryApp final : public App {
 public:
  static constexpr std::size_t kPageCount = 14;

  AnimationGalleryApp() noexcept;
  const char* name() const noexcept override { return "Animation Gallery"; }
  void onEnter() noexcept override;
  void update(Seconds dt, const InputFrame& input,
              AppRuntime& runtime) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRuntime& runtime) noexcept override;

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
