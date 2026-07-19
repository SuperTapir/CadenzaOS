#include "cadenza/apps/apps.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace cadenza {
namespace {

constexpr std::array<const char*, AnimationGalleryApp::kPageCount> kPageNames{{
    "EASING",
    "COMPOSITION",
    "SPRING",
    "DIP",
    "HORIZONTAL WIPE",
    "DIAGONAL WIPE",
    "IRIS",
    "VENETIAN BLINDS",
    "CHECKER DISSOLVE",
    "SELECTION",
    "CAMERA",
    "PARTICLES",
    "SPRITE STATE",
    "DITHER",
}};

int wrapPage(int value) noexcept {
  value %= static_cast<int>(AnimationGalleryApp::kPageCount);
  return value < 0 ? value + static_cast<int>(AnimationGalleryApp::kPageCount)
                   : value;
}

}  // namespace

const std::uint8_t AnimationGalleryApp::kSpritePixels[4] = {
    0x6D,
    0xFB,
    0x9F,
    0xE6,
};

AnimationGalleryApp::AnimationGalleryApp() noexcept {
  timeline_.add(firstTween_, 0.0F);
  timeline_.add(secondTween_, 0.35F);
  spriteAtlas_.add("idle-a", {0, 0, 2, 4});
  spriteAtlas_.add("idle-b", {2, 0, 2, 4});
  spriteAtlas_.add("active-a", {4, 0, 2, 4});
  spriteAtlas_.add("active-b", {6, 0, 2, 4});
  spriteMachine_.addState("idle", idle_);
  spriteMachine_.addState("active", active_);
  spriteMachine_.addTransition("idle", "toggle", "active");
  spriteMachine_.addTransition("active", "toggle", "idle");
  spriteMachine_.start("idle");
  resetDemo();
}

const char* AnimationGalleryApp::pageName(std::size_t index) const noexcept {
  return index < kPageNames.size() ? kPageNames[index] : "INVALID";
}

void AnimationGalleryApp::onEnter() noexcept {
  page_ = 0;
  mode_ = GalleryMode::AutoPlay;
  progress_ = 0.0F;
  resetDemo();
}

void AnimationGalleryApp::update(const AppUpdateContext& context) noexcept {
  const Seconds dt = context.dt;
  const InputFrame& input = context.input;
  if (demoWidth_ != context.displayWidth ||
      demoHeight_ != context.displayHeight) {
    demoWidth_ = context.displayWidth;
    demoHeight_ = context.displayHeight;
    transitionScenesReady_ = false;
    resetDemo();
  }
  if (context.system.motionProfile != motionProfile_) {
    applyMotionProfile(context.system.motionProfile);
  }
  if (input.clicked) {
    mode_ = mode_ == GalleryMode::AutoPlay ? GalleryMode::Scrub
                                           : GalleryMode::AutoPlay;
    context.commands.submit(SystemCommand::playSound(
        mode_ == GalleryMode::Scrub ? audio::SoundCue::ToggleOn
                                    : audio::SoundCue::ToggleOff));
  }
  if (input.turn != 0) {
    if (mode_ == GalleryMode::Scrub) {
      const float previous = progress_;
      progress_ = std::max(
          0.0F, std::min(1.0F, progress_ + input.turn *
                         presentation_defaults::kScrubStep));
      reconstructDemoState();
      context.commands.submit(SystemCommand::playSound(
          progress_ == previous ? audio::SoundCue::Boundary
                                : audio::SoundCue::Navigate));
    } else {
      page_ = static_cast<std::size_t>(
          wrapPage(static_cast<int>(page_) + input.turn));
      resetDemo();
      context.commands.submit(
          SystemCommand::playSound(audio::SoundCue::Navigate));
    }
  }
  if (mode_ == GalleryMode::Scrub) return;

  const Seconds delta = std::max(0.0F, dt);
  const double unwrappedProgress =
      static_cast<double>(progress_) +
      static_cast<double>(delta) /
          static_cast<double>(presentation_defaults::kGalleryCycleDuration);
  progress_ = static_cast<float>(std::fmod(unwrappedProgress, 1.0));
  Seconds demoDelta = delta;
  if (unwrappedProgress >= 1.0) {
    restartDemoState();
    demoDelta = progress_ * presentation_defaults::kGalleryCycleDuration;
  }
  applyProgress();
  advanceDemoState(demoDelta);
}

void AnimationGalleryApp::applyMotionProfile(MotionProfile profile) noexcept {
  motionProfile_ = profile;
  selection_ = SelectionFeedback{profile};
  punch_ = CameraPunch{presentation_defaults::kCameraPunchAmplitude,
                       presentation_defaults::kCameraPunchDuration, profile};
  shake_ = CameraShake{0xCADAU,
                       presentation_defaults::kCameraShakeAmplitude,
                       presentation_defaults::kCameraShakeDuration, profile};
  selection_.trigger();
  punch_.trigger();
  shake_.trigger();
}

void AnimationGalleryApp::resetDemo() noexcept {
  progress_ = 0.0F;
  restartDemoState();
  applyProgress();
}

void AnimationGalleryApp::restartDemoState() noexcept {
  firstTween_.reset();
  secondTween_.reset();
  timeline_.reset();
  spring_.reset(0.0F);
  spring_.setTarget(1.0F);
  selection_ = SelectionFeedback{motionProfile_};
  selection_.trigger();
  punch_.trigger();
  shake_.trigger();
  idle_.reset();
  active_.reset();
  spriteMachine_.start("idle");
  particles_ = ParticlePool<24>{ParticleFullPolicy::ReplaceOldest};
  emitter_ = ParticleEmitter{0x1B17U};
  Particle particle;
  particle.position = {static_cast<float>(demoWidth_) * 0.5F,
                       static_cast<float>(demoHeight_) * 0.5F};
  particle.velocity = {0.0F, -8.0F};
  particle.acceleration = {0.0F, 12.0F};
  particle.lifetime = 2.0F;
  particle.visual = ParticleVisual::Circle;
  particle.radius = 2;
  emitter_.emit(particles_, particle, 12, 18.0F);
}

void AnimationGalleryApp::reconstructDemoState() noexcept {
  restartDemoState();
  applyProgress();
  advanceDemoState(progress_ *
                   presentation_defaults::kGalleryCycleDuration);
}

void AnimationGalleryApp::advanceDemoState(Seconds delta) noexcept {
  constexpr Seconds kPresentationStep = 1.0F / 60.0F;
  Seconds remaining = std::max(0.0F, delta);
  while (remaining > 0.000001F) {
    const Seconds step = std::min(kPresentationStep, remaining);
    spring_.update(step);
    selection_.update(step);
    punch_.update(step);
    shake_.update(step);
    particles_.update(step);
    remaining -= step;
  }
}

void AnimationGalleryApp::applyProgress() noexcept {
  timeline_.seekTime(progress_ * timeline_.duration());
  if (progress_ >= 0.5F &&
      std::strcmp(spriteMachine_.currentState(), "idle") == 0) {
    spriteMachine_.trigger("toggle");
  } else if (progress_ < 0.5F &&
             std::strcmp(spriteMachine_.currentState(), "active") == 0) {
    spriteMachine_.trigger("toggle");
  }
  idle_.seek(progress_);
  active_.seek(progress_);
}

void AnimationGalleryApp::ensureTransitionBuffers(
    const MonoCanvas& canvas) noexcept {
  if (transitionFrom_.width() == canvas.width() &&
      transitionFrom_.height() == canvas.height()) {
    return;
  }
  const FramebufferProfile profile = canvas.width() == 400
                                         ? FramebufferProfile::Sharp
                                         : FramebufferProfile::TEmbed;
  transitionFrom_ = MonoFramebuffer{profile};
  transitionTo_ = MonoFramebuffer{profile};
  transitionScenesReady_ = false;
}

void AnimationGalleryApp::renderHeader(MonoCanvas& canvas) noexcept {
  const int width = canvas.width();
  const int height = canvas.height();
  canvas.fillRect(0, 0, width, 25, true);
  char pageLabel[20];
  std::snprintf(pageLabel, sizeof(pageLabel), "%02u/%02u",
                static_cast<unsigned>(page_ + 1),
                static_cast<unsigned>(kPageCount));
  canvas.fillRect(0, height - 23, width, 23, false);
  canvas.line(0, height - 23, width - 1, height - 23, true);
  canvas.text(mode_ == GalleryMode::Scrub ? "SCRUB: TURN  PRESS: PLAY"
                                          : "TURN: PAGE  PRESS: SCRUB",
              8, height - 11, 1, true, TextAlign::MiddleLeft,
              TextRole::Footer);
  canvas.fillRect(width - 70, height - 15,
                  static_cast<int>(progress_ * 62.0F), 7, true);
  canvas.rect(width - 70, height - 15, 62, 7, true);
  canvas.fillRect(0, 25, width, 23, false);
  canvas.text(pageName(page_), width / 2, 36, 1, true,
              TextAlign::MiddleCenter, TextRole::Caption);
  canvas.text("ANIMATION GALLERY", 8, 13, 1, false, TextAlign::MiddleLeft,
              TextRole::Compact);
  canvas.text(pageLabel, width - 8, 13, 1, false, TextAlign::MiddleRight,
              TextRole::Caption);
}

void AnimationGalleryApp::renderEasing(MonoCanvas& canvas) noexcept {
  const int left = 20;
  const int right = canvas.width() - 20;
  const int top = 48;
  const int bottom = canvas.height() - 32;
  canvas.rect(left, top, right - left, bottom - top, true);
  int previousX = left;
  int previousY = bottom;
  for (int x = left + 1; x < right; ++x) {
    const float p = static_cast<float>(x - left) / (right - left - 1);
    const float value = ease(Easing::OutBack, p);
    const int y = bottom - static_cast<int>(value * (bottom - top - 5));
    canvas.line(previousX, previousY, x, y, true);
    previousX = x;
    previousY = y;
  }
  const int marker = left + static_cast<int>(progress_ * (right - left));
  canvas.line(marker, top, marker, bottom, true);
}

void AnimationGalleryApp::renderComposition(MonoCanvas& canvas) noexcept {
  const int width = canvas.width();
  const int y1 = canvas.height() / 2 - 18;
  const int y2 = canvas.height() / 2 + 22;
  canvas.line(24, y1, width - 24, y1);
  canvas.line(24, y2, width - 24, y2);
  canvas.fillCircle(24 + static_cast<int>(firstTween_.value() * (width - 48)),
                    y1, 8, true);
  canvas.fillCircle(24 + static_cast<int>(secondTween_.value() * (width - 48)),
                    y2, 8, true);
}

void AnimationGalleryApp::renderSpring(MonoCanvas& canvas) noexcept {
  const int width = canvas.width();
  const int y = canvas.height() / 2;
  const int x = 25 + static_cast<int>(spring_.value() * (width - 50));
  for (int segment = 0; segment < 12; ++segment) {
    const int x0 = 20 + (x - 20) * segment / 12;
    const int x1 = 20 + (x - 20) * (segment + 1) / 12;
    canvas.line(x0, y + (segment % 2 ? -9 : 9), x1,
                y + (segment % 2 ? 9 : -9), true);
  }
  canvas.fillCircle(x, y, 12, true);
}

void AnimationGalleryApp::renderTransition(
    MonoCanvas& canvas, const Transition& transition) noexcept {
  ensureTransitionBuffers(canvas);
  if (!transitionScenesReady_) {
    MonoCanvas from{transitionFrom_};
    MonoCanvas to{transitionTo_};
    from.clear(false);
    to.clear(true);
    from.fillCircle(canvas.width() / 3, canvas.height() / 2, 35, true);
    from.text("A", canvas.width() / 3, canvas.height() / 2, 1, false,
              TextAlign::MiddleCenter, TextRole::Title);
    to.fillRect(canvas.width() * 2 / 3 - 35, canvas.height() / 2 - 35, 70, 70,
                false);
    to.text("B", canvas.width() * 2 / 3, canvas.height() / 2, 1, true,
            TextAlign::MiddleCenter, TextRole::Title);
    transitionScenesReady_ = true;
  }
  transition.compose(transitionFrom_, transitionTo_, canvas, progress_);
}

void AnimationGalleryApp::renderSelection(MonoCanvas& canvas) noexcept {
  const float scale = std::max(0.05F, selection_.scale());
  const int width = std::max(8, static_cast<int>(120.0F * scale));
  const int height = std::max(8, static_cast<int>(52.0F * scale));
  const int x = (canvas.width() - width) / 2;
  const int y = (canvas.height() - height) / 2;
  canvas.fillRoundedRect(x, y, width, height, 6, true);
  canvas.text("SELECTED", canvas.width() / 2, canvas.height() / 2, 1, false,
              TextAlign::MiddleCenter, TextRole::Body);
}

void AnimationGalleryApp::renderCamera(MonoCanvas& canvas) noexcept {
  const int dx = static_cast<int>(punch_.offset() + shake_.x());
  const int dy = static_cast<int>(shake_.y());
  const int cx = canvas.width() / 2 + dx;
  const int cy = canvas.height() / 2 + dy;
  canvas.rect(cx - 60, cy - 35, 120, 70, true);
  canvas.line(cx - 60, cy, cx + 60, cy);
  canvas.line(cx, cy - 35, cx, cy + 35);
  canvas.fillCircle(cx, cy, 8, true);
}

void AnimationGalleryApp::renderParticles(MonoCanvas& canvas) noexcept {
  canvas.line(20, canvas.height() - 35, canvas.width() - 20,
              canvas.height() - 35, true);
  particles_.render(canvas);
}

void AnimationGalleryApp::renderSpriteState(MonoCanvas& canvas) noexcept {
  const SpriteFrame* frame = spriteMachine_.animation()->frame();
  if (!frame) return;
  const BitmapView& bitmap = spriteAtlas_.bitmap();
  const int scale = 14;
  const int left = canvas.width() / 2 - frame->source.width * scale / 2;
  const int top = canvas.height() / 2 - frame->source.height * scale / 2;
  for (int y = 0; y < frame->source.height; ++y) {
    for (int x = 0; x < frame->source.width; ++x) {
      if (bitmap.pixel(frame->source.x + x, frame->source.y + y)) {
        canvas.fillRect(left + x * scale, top + y * scale, scale, scale, true);
      }
    }
  }
  canvas.text(spriteMachine_.currentState(), canvas.width() / 2,
              canvas.height() - 35, 1, true, TextAlign::MiddleCenter,
              TextRole::Caption);
}

void AnimationGalleryApp::renderDither(MonoCanvas& canvas) noexcept {
  const Rect region{20, 48, canvas.width() - 40, canvas.height() - 82};
  canvas.fillDither(region, kOrderedDither8x8,
                    static_cast<std::uint8_t>(progress_ * 64.0F),
                    static_cast<int>(progress_ * 8.0F), 0, true);
  canvas.rect(region.x, region.y, region.width, region.height, true);
}

void AnimationGalleryApp::renderInitialFrame(
    MonoCanvas& canvas) const noexcept {
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  const int left = 20;
  const int right = width - 20;
  const int top = 48;
  const int bottom = height - 32;
  canvas.rect(left, top, right - left, bottom - top, true);
  int previousX = left;
  int previousY = bottom;
  for (int x = left + 1; x < right; ++x) {
    const float p = static_cast<float>(x - left) / (right - left - 1);
    const float value = ease(Easing::OutBack, p);
    const int y = bottom - static_cast<int>(value * (bottom - top - 5));
    canvas.line(previousX, previousY, x, y, true);
    previousX = x;
    previousY = y;
  }
  canvas.line(left, top, left, bottom, true);

  canvas.fillRect(0, 0, width, 25, true);
  canvas.fillRect(0, height - 23, width, 23, false);
  canvas.line(0, height - 23, width - 1, height - 23, true);
  canvas.text("TURN: PAGE  PRESS: SCRUB", 8, height - 11, 1, true,
              TextAlign::MiddleLeft, TextRole::Footer);
  canvas.rect(width - 70, height - 15, 62, 7, true);
  canvas.fillRect(0, 25, width, 23, false);
  canvas.text("EASING", width / 2, 36, 1, true,
              TextAlign::MiddleCenter, TextRole::Caption);
  canvas.text("ANIMATION GALLERY", 8, 13, 1, false,
              TextAlign::MiddleLeft, TextRole::Compact);
  canvas.text("01/14", width - 8, 13, 1, false,
              TextAlign::MiddleRight, TextRole::Caption);
}

void AnimationGalleryApp::render(MonoCanvas& canvas,
                                 const AppRenderContext&) noexcept {
  canvas.clear(false);
  switch (page_) {
    case 0: renderEasing(canvas); break;
    case 1: renderComposition(canvas); break;
    case 2: renderSpring(canvas); break;
    case 3: renderTransition(canvas, kDipTransition); break;
    case 4: renderTransition(canvas, kHorizontalWipeTransition); break;
    case 5: renderTransition(canvas, kDiagonalWipeTransition); break;
    case 6: renderTransition(canvas, kIrisTransition); break;
    case 7: renderTransition(canvas, kVenetianBlindsTransition); break;
    case 8: renderTransition(canvas, kCheckerDissolveTransition); break;
    case 9: renderSelection(canvas); break;
    case 10: renderCamera(canvas); break;
    case 11: renderParticles(canvas); break;
    case 12: renderSpriteState(canvas); break;
    case 13: renderDither(canvas); break;
    default: break;
  }
  renderHeader(canvas);
}

}  // namespace cadenza
