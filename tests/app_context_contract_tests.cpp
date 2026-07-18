#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <type_traits>

#include "cadenza/core/app_context.h"
#include "cadenza/core/app_runtime.h"
#include "cadenza/system/system_service_host.h"

namespace {
template <typename T, typename = void>
struct HasRuntimeSoundAccessor : std::false_type {};

template <typename T>
struct HasRuntimeSoundAccessor<
    T, std::void_t<decltype(std::declval<T&>().sound())>> : std::true_type {};

class NarrowApp final : public cadenza::App {
 public:
  const char* name() const noexcept override { return "Narrow"; }
  void update(const cadenza::AppUpdateContext& context) noexcept override {
    sawInput = context.input.clicked;
    context.commands.submit(
        cadenza::SystemCommand::playSound(cadenza::audio::SoundCue::Confirm));
  }
  void render(cadenza::MonoCanvas&,
              const cadenza::AppRenderContext& context) noexcept override {
    sawSnapshot = context.system.motionProfile == cadenza::MotionProfile::Normal;
  }

  bool sawInput = false;
  bool sawSnapshot = false;
};
}  // namespace

static_assert(!std::is_convertible_v<cadenza::AppUpdateContext&,
                                     cadenza::AppRuntime&>);
static_assert(!std::is_base_of_v<cadenza::SystemCommandSink,
                                 cadenza::AppRuntime>);
static_assert(!HasRuntimeSoundAccessor<cadenza::AppRuntime>::value);
static_assert(!HasRuntimeSoundAccessor<cadenza::AppUpdateContext>::value);
static_assert(!HasRuntimeSoundAccessor<cadenza::AppRenderContext>::value);

TEST_CASE("App narrow contexts preserve input command and snapshot behavior") {
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  NarrowApp app;
  constexpr cadenza::AppId kHome{0x7000};
  REQUIRE(runtime.registerApp(
      kHome, app, false,
      cadenza::AppCapabilitySet{cadenza::AppCapability::SoundPlay}));
  REQUIRE(runtime.configureHome(kHome));
  REQUIRE(runtime.begin(kHome));

  cadenza::InputFrame input;
  input.clicked = true;
  const auto& updateSnapshot = services.beginFrame(0.01F);
  runtime.updateWithSystem(0.01F, input, updateSnapshot, services);
  services.commitCommands();
  CHECK(app.sawInput);
  CHECK(services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Confirm);

  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  runtime.renderWithSystem(canvas, services.snapshot());
  CHECK(app.sawSnapshot);
}
