#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <set>

#include "app_context_test_support.h"
#include "cadenza/apps/apps.h"
#include "cadenza/host/headless_host.h"

namespace {

class GeometryDiagnostics final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    if (event.code == cadenza::DiagnosticCode::InvalidGeometry ||
        event.code == cadenza::DiagnosticCode::ClippedGeometry ||
        event.code == cadenza::DiagnosticCode::FullyClipped) {
      failed = true;
    }
  }
  bool failed = false;
};

bool blackIn(const cadenza::MonoFramebuffer& framebuffer,
             cadenza::Rect bounds) {
  for (int y = bounds.y; y < bounds.y + bounds.height; ++y) {
    for (int x = bounds.x; x < bounds.x + bounds.width; ++x) {
      if (framebuffer.pixel(x, y)) return true;
    }
  }
  return false;
}

struct SightFixture {
  cadenza::SightApp app;
  cadenza::system::SystemServiceHost services;
  cadenza::AppRuntime runtime{cadenza::FramebufferProfile::TEmbed};

  SightFixture() {
    REQUIRE(runtime.registerApp(
        cadenza::apps::kSightAppId, app, true,
        cadenza::apps::builtinAppCapabilities(cadenza::apps::kSightAppId)));
    REQUIRE(runtime.begin(cadenza::apps::kSightAppId));
  }

  void update(cadenza::InputFrame input = {},
              cadenza::Seconds dt = 1.0F / 60.0F) {
    cadenza::test::updateApp(app, dt, input, runtime, services,
                             cadenza::apps::kSightAppId);
  }
};

std::uint64_t renderHash(
    SightFixture& fixture,
    cadenza::FramebufferProfile profile =
        cadenza::FramebufferProfile::TEmbed,
    cadenza::DiagnosticSink* diagnostics = nullptr) {
  cadenza::MonoFramebuffer framebuffer{profile};
  cadenza::MonoCanvas canvas{framebuffer, diagnostics};
  cadenza::test::renderApp(fixture.app, canvas, fixture.runtime,
                           fixture.services);
  return cadenza::host::framebufferHash(framebuffer);
}

}  // namespace

TEST_CASE("spelled pitch preserves notation identity and MIDI mapping") {
  using namespace cadenza;
  CHECK(SpelledPitch{SightStep::C, SightAccidental::Natural, 4}.midiNote() == 60);
  CHECK(SpelledPitch{SightStep::F, SightAccidental::Sharp, 4}.midiNote() == 66);
  CHECK(SpelledPitch{SightStep::G, SightAccidental::Flat, 4}.midiNote() == 66);
  CHECK(SpelledPitch{SightStep::C, SightAccidental::Natural, 3}.midiNote() == 48);
  CHECK(SpelledPitch{SightStep::C, SightAccidental::Natural, 5}.midiNote() == 72);
}

TEST_CASE("level one shuffle bag covers C3 through C5 once") {
  SightFixture fixture;
  fixture.update({0, false, false, true, false, 0});
  std::set<int> notes;
  for (int index = 0; index < 15; ++index) {
    REQUIRE(fixture.app.screen() == cadenza::SightScreen::Question);
    notes.insert(fixture.app.card().pitches[0].midiNote());
    fixture.update({0, false, false, true, false, 0});
    REQUIRE(fixture.app.answerAction() == cadenza::SightAnswerAction::Next);
    fixture.update({0, false, false, true, false, 0});
    fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds);
  }
  CHECK(notes.size() == 15);
  CHECK(*notes.begin() == 48);
  CHECK(*notes.rbegin() == 72);
}

TEST_CASE("shuffle bag is deterministic and avoids a round-boundary repeat") {
  SightFixture first;
  SightFixture second;
  first.update({0, false, false, true, false, 0});
  second.update({0, false, false, true, false, 0});
  std::array<int, 16> sequence{};
  for (std::size_t index = 0; index < sequence.size(); ++index) {
    REQUIRE(first.app.screen() == cadenza::SightScreen::Question);
    REQUIRE(second.app.screen() == cadenza::SightScreen::Question);
    sequence[index] = first.app.card().pitches[0].midiNote();
    CHECK(second.app.card().pitches[0].midiNote() == sequence[index]);
    for (SightFixture* fixture : {&first, &second}) {
      fixture->update({0, false, false, true, false, 0});
      fixture->update({0, false, false, true, false, 0});
      fixture->update({}, cadenza::SightApp::kAnswerRevealSeconds);
    }
  }
  CHECK(sequence[14] != sequence[15]);
}

TEST_CASE("level picker gives semantic navigation start and return sounds") {
  SightFixture fixture;
  fixture.update({1, false, false, false, false, 0});
  CHECK(fixture.app.level() == cadenza::SightLevel::AllNotes);
  CHECK(fixture.services.sound().hasAcceptedCue());
  CHECK(fixture.services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Navigate);

  fixture.update({0, false, false, true, false, 0});
  CHECK(fixture.app.screen() == cadenza::SightScreen::Question);
  CHECK(fixture.services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Confirm);

  fixture.update({0, false, false, true, false, 0});
  fixture.update({1, false, false, false, false, 0});
  fixture.update({0, false, false, true, false, 0});
  CHECK(fixture.app.screen() == cadenza::SightScreen::LevelPicker);
  CHECK(fixture.services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Back);
}

TEST_CASE("all level picker identities render bounded at both profiles") {
  SightFixture fixture;
  for (int level = 0; level < 3; ++level) {
    if (level > 0) fixture.update({1, false, false, false, false, 0});
    for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                               cadenza::FramebufferProfile::Sharp}) {
      GeometryDiagnostics diagnostics;
      cadenza::MonoFramebuffer framebuffer{profile};
      cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
      cadenza::test::renderApp(fixture.app, canvas, fixture.runtime,
                               fixture.services);
      CHECK_FALSE(diagnostics.failed);
    }
  }
}

TEST_CASE("level selection wipes between identities and settles exactly") {
  SightFixture fixture;
  const std::uint64_t initial = renderHash(fixture);
  fixture.update({1, false, false, false, false, 0});
  const std::uint64_t start = renderHash(fixture);
  for (int frame = 0; frame < 5; ++frame) fixture.update();
  GeometryDiagnostics diagnostics;
  const std::uint64_t middle = renderHash(
      fixture, cadenza::FramebufferProfile::TEmbed, &diagnostics);
  CHECK_FALSE(diagnostics.failed);
  for (int frame = 0; frame < 10; ++frame) fixture.update();
  const std::uint64_t settled = renderHash(fixture);
  fixture.update();
  const std::uint64_t stable = renderHash(fixture);

  CHECK(initial != start);
  CHECK(start != middle);
  CHECK(middle != settled);
  CHECK(settled == stable);
}

TEST_CASE("reduced motion switches level identity without transition frames") {
  SightFixture reduced;
  REQUIRE(reduced.services.submit(cadenza::SystemCommand::setMotionProfile(
      cadenza::MotionProfile::Reduced)));
  reduced.services.commitCommands();
  reduced.update({1, false, false, false, false, 0});
  const std::uint64_t reducedHash = renderHash(reduced);

  SightFixture normal;
  normal.update({1, false, false, false, false, 0});
  for (int frame = 0; frame < 12; ++frame) normal.update();
  CHECK(reducedHash == renderHash(normal));
}

TEST_CASE("level and study pages wipe in opposite directions") {
  SightFixture fixture;
  const std::uint64_t level = renderHash(fixture);
  fixture.update({0, false, false, true, false, 0});
  CHECK(fixture.app.pageTransition() ==
        cadenza::SightPageTransition::ToQuestion);
  CHECK(renderHash(fixture) == level);
  fixture.update({}, cadenza::SightApp::kPageTransitionSeconds / 2.0F);
  const std::uint64_t entering = renderHash(fixture);
  CHECK(entering != level);
  fixture.update({}, cadenza::SightApp::kPageTransitionSeconds);
  CHECK(fixture.app.pageTransition() == cadenza::SightPageTransition::None);

  fixture.update({0, false, false, true, false, 0});
  fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds);
  fixture.update({1, false, false, false, false, 0});
  const std::uint64_t answer = renderHash(fixture);
  fixture.update({0, false, false, true, false, 0});
  CHECK(fixture.app.pageTransition() ==
        cadenza::SightPageTransition::ToLevel);
  CHECK(renderHash(fixture) == answer);
  fixture.update({}, cadenza::SightApp::kPageTransitionSeconds / 2.0F);
  const std::uint64_t returning = renderHash(fixture);
  CHECK(returning != answer);
  fixture.update({}, cadenza::SightApp::kPageTransitionSeconds);
  CHECK(fixture.app.pageTransition() == cadenza::SightPageTransition::None);
  CHECK(renderHash(fixture) == level);
}

TEST_CASE("question ignores turn and answer requires click") {
  SightFixture fixture;
  fixture.update({0, false, false, true, false, 0});
  fixture.update({}, cadenza::SightApp::kNoteEntranceSeconds);
  const auto original = fixture.app.card().pitches[0].midiNote();
  fixture.update({7, false, false, false, false, 0});
  CHECK(fixture.app.screen() == cadenza::SightScreen::Question);
  CHECK(fixture.app.card().pitches[0].midiNote() == original);

  fixture.update({0, false, false, true, false, 0});
  CHECK(fixture.app.screen() == cadenza::SightScreen::Answer);
  CHECK(fixture.app.answerAction() == cadenza::SightAnswerAction::Next);
  fixture.update({-1, false, false, false, false, 0});
  CHECK(fixture.app.answerAction() == cadenza::SightAnswerAction::Replay);
  CHECK(fixture.app.card().pitches[0].midiNote() == original);
  fixture.update({0, false, false, true, false, 0});
  CHECK(fixture.app.screen() == cadenza::SightScreen::Answer);
  CHECK(fixture.app.answerAction() == cadenza::SightAnswerAction::Replay);
  fixture.update({2, false, false, false, false, 0});
  CHECK(fixture.app.answerAction() == cadenza::SightAnswerAction::Level);
  fixture.update({0, false, false, true, false, 0});
  CHECK(fixture.app.screen() == cadenza::SightScreen::LevelPicker);
}

TEST_CASE("answer card pushes into place and then settles") {
  SightFixture fixture;
  fixture.update({0, false, false, true, false, 0});
  fixture.update({}, cadenza::SightApp::kNoteEntranceSeconds);
  cadenza::MonoFramebuffer question{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas questionCanvas{question};
  cadenza::test::renderApp(fixture.app, questionCanvas, fixture.runtime,
                           fixture.services);
  CHECK(question.pixel(300, 32));
  const std::uint64_t questionHash = renderHash(fixture);

  fixture.update({0, false, false, true, false, 0});
  cadenza::MonoFramebuffer entering{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas enteringCanvas{entering};
  cadenza::test::renderApp(fixture.app, enteringCanvas, fixture.runtime,
                           fixture.services);
  CHECK(entering.pixel(300, 32));
  const std::uint64_t start = renderHash(fixture);
  CHECK(start == questionHash);
  fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds / 2.0F);
  const std::uint64_t middle = renderHash(fixture);
  fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds);
  const std::uint64_t settled = renderHash(fixture);
  fixture.update();
  const std::uint64_t stable = renderHash(fixture);

  CHECK(start != middle);
  CHECK(middle != settled);
  CHECK(settled == stable);

  cadenza::MonoFramebuffer finalFrame{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas finalCanvas{finalFrame};
  cadenza::test::renderApp(fixture.app, finalCanvas, fixture.runtime,
                           fixture.services);
  CHECK(finalFrame.pixel(172, 32));
  CHECK(finalFrame.pixel(120, 160));

  const auto answeredNote = fixture.app.card().pitches[0].midiNote();
  fixture.update({0, false, false, true, false, 0});
  CHECK(fixture.app.screen() == cadenza::SightScreen::AnswerExiting);
  CHECK(fixture.app.card().pitches[0].midiNote() == answeredNote);
  const std::uint64_t exitStart = renderHash(fixture);
  fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds / 2.0F);
  const std::uint64_t exitMiddle = renderHash(fixture);
  CHECK(exitStart != exitMiddle);
  CHECK(fixture.app.card().pitches[0].midiNote() == answeredNote);
  fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds);
  CHECK(fixture.app.screen() == cadenza::SightScreen::Question);
  CHECK(fixture.app.card().pitches[0].midiNote() != answeredNote);

  SightFixture reduced;
  REQUIRE(reduced.services.submit(cadenza::SystemCommand::setMotionProfile(
      cadenza::MotionProfile::Reduced)));
  reduced.services.commitCommands();
  reduced.update({0, false, false, true, false, 0});
  reduced.update({0, false, false, true, false, 0});
  CHECK(renderHash(reduced) == settled);
}

TEST_CASE("new note grows in with a small overshoot and settles") {
  SightFixture fixture;
  fixture.update({0, false, false, true, false, 0});
  const std::uint64_t start = renderHash(fixture);
  fixture.update({}, cadenza::SightApp::kNoteEntranceSeconds / 2.0F);
  const std::uint64_t middle = renderHash(fixture);
  fixture.update({}, cadenza::SightApp::kNoteEntranceSeconds);
  const std::uint64_t settled = renderHash(fixture);
  fixture.update();

  CHECK(start != middle);
  CHECK(middle != settled);
  CHECK(renderHash(fixture) == settled);

  SightFixture reduced;
  REQUIRE(reduced.services.submit(cadenza::SystemCommand::setMotionProfile(
      cadenza::MotionProfile::Reduced)));
  reduced.services.commitCommands();
  reduced.update({0, false, false, true, false, 0});
  CHECK(renderHash(reduced) == settled);
}

TEST_CASE("common chords use three canonical spelled pitches") {
  SightFixture fixture;
  fixture.update({2, false, false, false, false, 0});
  CHECK(fixture.app.level() == cadenza::SightLevel::CommonChords);
  fixture.update({0, false, false, true, false, 0});
  CHECK(fixture.app.card().count == 3);
  const int root = fixture.app.card().pitches[0].midiNote();
  const int third = fixture.app.card().pitches[1].midiNote();
  const int fifth = fixture.app.card().pitches[2].midiNote();
  CHECK(third - root == (fixture.app.card().minor ? 3 : 4));
  CHECK(fifth - root == 7);
}

TEST_CASE("all common chord questions and answers stay bounded at both profiles") {
  SightFixture fixture;
  fixture.update({2, false, false, false, false, 0});
  fixture.update({0, false, false, true, false, 0});
  std::set<std::array<int, 4>> chords;
  bool sawSpelledDMajor = false;
  for (int card = 0; card < 24; ++card) {
    const auto& current = fixture.app.card();
    REQUIRE(current.count == 3);
    if (!current.minor && current.pitches[0].step == cadenza::SightStep::D &&
        current.pitches[0].accidental == cadenza::SightAccidental::Natural) {
      sawSpelledDMajor = true;
      CHECK(current.pitches[1].step == cadenza::SightStep::F);
      CHECK(current.pitches[1].accidental == cadenza::SightAccidental::Sharp);
      CHECK(current.pitches[2].step == cadenza::SightStep::A);
      CHECK(current.pitches[2].accidental == cadenza::SightAccidental::Natural);
    }
    chords.insert({current.pitches[0].midiNote(),
                   current.pitches[1].midiNote(),
                   current.pitches[2].midiNote(), current.minor ? 1 : 0});
    for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                               cadenza::FramebufferProfile::Sharp}) {
      GeometryDiagnostics diagnostics;
      cadenza::MonoFramebuffer framebuffer{profile};
      cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
      cadenza::test::renderApp(fixture.app, canvas, fixture.runtime,
                               fixture.services);
      CHECK_FALSE(diagnostics.failed);
    }
    fixture.update({0, false, false, true, false, 0});
    for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                               cadenza::FramebufferProfile::Sharp}) {
      GeometryDiagnostics diagnostics;
      cadenza::MonoFramebuffer framebuffer{profile};
      cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
      cadenza::test::renderApp(fixture.app, canvas, fixture.runtime,
                               fixture.services);
      CHECK_FALSE(diagnostics.failed);
    }
    fixture.update({0, false, false, true, false, 0});
    fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds);
  }
  CHECK(chords.size() == 24);
  CHECK(sawSpelledDMajor);
}

TEST_CASE("chord answers use conventional major and minor symbols") {
  using namespace cadenza;
  char symbol[12]{};

  SightCard major{};
  major.count = 3;
  major.pitches[0] = {SightStep::F, SightAccidental::Natural, 4};
  formatSightChordSymbol(major, symbol, sizeof(symbol));
  CHECK(std::strcmp(symbol, "F") == 0);

  SightCard flatMinor{};
  flatMinor.count = 3;
  flatMinor.minor = true;
  flatMinor.pitches[0] = {SightStep::B, SightAccidental::Flat, 3};
  formatSightChordSymbol(flatMinor, symbol, sizeof(symbol));
  CHECK(std::strcmp(symbol, "Bbm") == 0);

  SightCard sharpMinor{};
  sharpMinor.count = 3;
  sharpMinor.minor = true;
  sharpMinor.pitches[0] = {SightStep::F, SightAccidental::Sharp, 4};
  formatSightChordSymbol(sharpMinor, symbol, sizeof(symbol));
  CHECK(std::strcmp(symbol, "F#m") == 0);
}

TEST_CASE("musical note command validates bounds and uses droppable reserve") {
  using namespace cadenza::audio;
  CHECK(MusicalNoteSet::single(60).valid());
  CHECK_FALSE(MusicalNoteSet{}.valid());
  CHECK_FALSE(MusicalNoteSet::single(20).valid());
  AudioCommandQueue queue;
  for (std::size_t index = 0; index < AudioCommandQueue::kDroppableLimit;
       ++index) {
    REQUIRE(queue.tryPush(AudioCommand::playNotes(MusicalNoteSet::single(60))));
  }
  CHECK_FALSE(queue.tryPush(AudioCommand::playNotes(MusicalNoteSet::single(61))));
  CHECK(queue.tryPush(AudioCommand::setVolume(SoundVolume::High)));
  CHECK(queue.tryPush(AudioCommand::stopAll()));
}

TEST_CASE("musical note replay restarts without accumulating voices") {
  using namespace cadenza::audio;
  InteractionSoundService service;
  const MusicalNoteSet chord{{60, 64, 67, 0}, 3};
  REQUIRE(service.playNotes(chord));
  std::array<std::int16_t, 4096> samples{};
  service.render(samples.data(), 512);
  const auto firstAttack = samples;
  CHECK(service.activeVoiceCount() == 3);
  REQUIRE(service.playNotes(chord));
  service.render(samples.data(), 512);
  CHECK(service.activeVoiceCount() == 3);
  CHECK(std::equal(samples.begin(), samples.begin() + 512,
                   firstAttack.begin()));
  CHECK(std::any_of(samples.begin(), samples.begin() + 512,
                    [](std::int16_t sample) { return sample != 0; }));
  CHECK(InteractionSoundService::midiFrequency(60) ==
        doctest::Approx(261.6256F).epsilon(0.0001));

  service.render(samples.data(), samples.size());
  for (int block = 0; block < 8; ++block) {
    service.render(samples.data(), samples.size());
  }
  CHECK(service.activeVoiceCount() == 0);
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
}

TEST_CASE("bass-clef audition peak is not drowned by treble") {
  using namespace cadenza::audio;
  auto peakOf = [](std::uint8_t midi) {
    InteractionSoundService service;
    REQUIRE(service.setVolume(SoundVolume::Medium));
    REQUIRE(service.playNotes(MusicalNoteSet::single(midi)));
    std::array<std::int16_t, 2048> samples{};
    int peak = 0;
    // Skip attack; measure a mid-tone block where envelope is stable.
    service.render(samples.data(), samples.size());
    service.render(samples.data(), samples.size());
    for (const std::int16_t sample : samples) {
      peak = std::max(peak, std::abs(static_cast<int>(sample)));
    }
    return peak;
  };

  const int bassPeak = peakOf(41);   // F2 — bass staff / ledger
  const int treblePeak = peakOf(72); // C5
  CHECK(bassPeak > 0);
  CHECK(treblePeak > 0);
  // Audible enough, but not the "bass louder than treble" overshoot.
  CHECK(bassPeak * 100 >= treblePeak * 45);
  CHECK(bassPeak * 100 <= treblePeak * 130);
  CHECK(InteractionSoundService::midiFrequency(41) ==
        doctest::Approx(87.3071F).epsilon(0.0001));
}

TEST_CASE("four-note audition is deterministic bounded and returns to zero") {
  using namespace cadenza::audio;
  const MusicalNoteSet notes{{48, 60, 72, 84}, 4};
  InteractionSoundService first;
  InteractionSoundService second;
  REQUIRE(first.playNotes(notes));
  REQUIRE(second.playNotes(notes));
  std::array<std::int16_t, 4096> left{};
  std::array<std::int16_t, 4096> right{};
  int peak = 0;
  for (int block = 0; block < 10; ++block) {
    first.render(left.data(), left.size());
    second.render(right.data(), right.size());
    CHECK(left == right);
    for (const std::int16_t sample : left) {
      const int magnitude = std::abs(static_cast<int>(sample));
      peak = std::max(peak, magnitude);
    }
  }
  CHECK(peak > 0);
  CHECK(peak < 32767);
  CHECK(first.activeVoiceCount() == 0);
  CHECK(std::all_of(left.begin(), left.end(),
                    [](std::int16_t sample) { return sample == 0; }));
}

TEST_CASE("long press is not consumed and re-enter resets session state") {
  SightFixture fixture;
  fixture.update({2, false, false, false, false, 0});
  fixture.update({0, false, false, true, false, 0});
  const auto card = fixture.app.card();
  fixture.update({5, false, false, false, true, 650});
  CHECK(fixture.app.screen() == cadenza::SightScreen::Question);
  CHECK(fixture.app.card().pitches[0].midiNote() ==
        card.pitches[0].midiNote());

  fixture.app.onExit();
  fixture.app.onEnter();
  CHECK(fixture.app.screen() == cadenza::SightScreen::LevelPicker);
  CHECK(fixture.app.level() == cadenza::SightLevel::Level1);
  CHECK(fixture.app.answerAction() == cadenza::SightAnswerAction::Next);
}

TEST_CASE("system menu owns long press while audition remains finite") {
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  REQUIRE(host.runtime().open(cadenza::apps::kSightAppId));
  host.advance(0.81F);
  REQUIRE_FALSE(host.runtime().transitioning());
  cadenza::InputFrame click;
  click.clicked = true;
  host.step(click);  // Start Level 1.
  host.step(click);  // Reveal and audition.
  std::array<std::int16_t, 256> samples{};
  host.renderAudio(samples.data(), samples.size());
  CHECK(std::any_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample != 0; }));

  cadenza::InputFrame held;
  held.longPressed = true;
  held.heldMs = 650;
  host.step(held);
  cadenza::InputFrame released;
  released.released = true;
  host.step(released);
  CHECK(host.runtime().systemMenuActive());

  std::array<std::int16_t, 4096> tail{};
  for (int block = 0; block < 10; ++block) {
    host.renderAudio(tail.data(), tail.size());
  }
  CHECK(std::all_of(tail.begin(), tail.end(),
                    [](std::int16_t sample) { return sample == 0; }));
}

TEST_CASE("mute clears musical notes and does not resume history") {
  using namespace cadenza::audio;
  InteractionSoundService service;
  std::array<std::int16_t, 256> samples{};
  REQUIRE(service.playNotes(MusicalNoteSet::single(60)));
  service.render(samples.data(), samples.size());
  REQUIRE(service.activeVoiceCount() == 1);
  REQUIRE(service.setVolume(SoundVolume::Muted));
  service.render(samples.data(), samples.size());
  CHECK(service.activeVoiceCount() == 0);
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t value) { return value == 0; }));
  REQUIRE(service.setVolume(SoundVolume::High));
  service.render(samples.data(), samples.size());
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t value) { return value == 0; }));
}

TEST_CASE("musical notes are capability guarded and invalid sets are rejected") {
  cadenza::system::SystemServiceHost services;
  cadenza::AppCommandPort denied{cadenza::AppId{0x0300}, {}, services};
  CHECK_FALSE(denied.submit(cadenza::SystemCommand::playMusicalNotes(
      cadenza::audio::MusicalNoteSet::single(60))));
  CHECK(services.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::CapabilityDenied);
  CHECK(services.diagnostics().lastRejectedCommandType ==
        cadenza::SystemCommandType::PlayMusicalNotes);

  cadenza::audio::MusicalNoteSet invalid;
  invalid.count = 5;
  CHECK_FALSE(services.submit(cadenza::SystemCommand::playMusicalNotes(invalid)));
  CHECK(services.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::InvalidCommand);
}

TEST_CASE("headless Launcher registers Timer Sight Motion in order") {
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  const auto catalog = host.runtime().catalogView();
  REQUIRE(catalog.launcherAppCount() == 5);
  CHECK(catalog.launcherAppAt(0) == cadenza::apps::kTimerAppId);
  CHECK(catalog.launcherAppAt(1) == cadenza::apps::kSightAppId);
  CHECK(catalog.launcherAppAt(2) == cadenza::apps::kMotionAppId);
}

TEST_CASE("sight rendering stays within both framebuffer profiles") {
  for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                             cadenza::FramebufferProfile::Sharp}) {
    SightFixture fixture;
    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};
    fixture.update({0, false, false, true, false, 0});
    cadenza::test::renderApp(fixture.app, canvas, fixture.runtime,
                             fixture.services);
    CHECK(std::any_of(framebuffer.data(),
                      framebuffer.data() + framebuffer.sizeBytes(),
                      [](std::uint8_t value) { return value != 0; }));
  }
}

TEST_CASE("grand staff uses the vertical field without clef or answer overlap") {
  for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                             cadenza::FramebufferProfile::Sharp}) {
    SightFixture fixture;
    fixture.update({0, false, false, true, false, 0});
    fixture.update({}, cadenza::SightApp::kPageTransitionSeconds);
    cadenza::MonoFramebuffer question{profile};
    cadenza::MonoCanvas questionCanvas{question};
    cadenza::test::renderApp(fixture.app, questionCanvas, fixture.runtime,
                             fixture.services);

    if (profile == cadenza::FramebufferProfile::TEmbed) {
      CHECK(blackIn(question, {20, 28, 280, 12}));
      CHECK(blackIn(question, {20, 112, 280, 12}));
      CHECK_FALSE(blackIn(question, {24, 76, 18, 8}));
    } else {
      CHECK(blackIn(question, {20, 42, 360, 12}));
      CHECK(blackIn(question, {20, 164, 360, 12}));
      CHECK_FALSE(blackIn(question, {25, 108, 22, 12}));
    }

    fixture.update({0, false, false, true, false, 0});
    fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds);
    cadenza::MonoFramebuffer answer{profile};
    cadenza::MonoCanvas answerCanvas{answer};
    cadenza::test::renderApp(fixture.app, answerCanvas, fixture.runtime,
                             fixture.services);
    if (profile == cadenza::FramebufferProfile::TEmbed) {
      CHECK_FALSE(blackIn(answer, {173, 32, 8, 92}));
      CHECK(answer.pixel(182, 30));
      CHECK(answer.pixel(309, 121));
      CHECK(answer.pixel(190, 85));
    } else {
      CHECK_FALSE(blackIn(answer, {239, 44, 10, 132}));
      CHECK(answer.pixel(250, 42));
      CHECK(answer.pixel(389, 173));
      CHECK(answer.pixel(258, 121));
    }
  }
}

TEST_CASE("middle C renders on its ledger line in question and answer") {
  SightFixture fixture;
  fixture.update({0, false, false, true, false, 0});
  for (int attempt = 0; attempt < 15 &&
                        fixture.app.card().pitches[0].midiNote() != 60;
       ++attempt) {
    fixture.update({0, false, false, true, false, 0});
    fixture.update({0, false, false, true, false, 0});
    fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds);
  }
  REQUIRE(fixture.app.card().pitches[0].midiNote() == 60);
  fixture.update({}, cadenza::SightApp::kNoteEntranceSeconds);
  cadenza::MonoFramebuffer question{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas questionCanvas{question};
  cadenza::test::renderApp(fixture.app, questionCanvas, fixture.runtime,
                           fixture.services);
  CHECK(blackIn(question, {181, 67, 15, 1}));

  fixture.update({0, false, false, true, false, 0});
  fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds);
  cadenza::MonoFramebuffer answer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas answerCanvas{answer};
  cadenza::test::renderApp(fixture.app, answerCanvas, fixture.runtime,
                           fixture.services);
  CHECK(blackIn(answer, {104, 67, 15, 1}));
}

TEST_CASE("all-notes endpoints and answer layouts stay geometrically bounded") {
  SightFixture fixture;
  fixture.update({1, false, false, false, false, 0});
  fixture.update({0, false, false, true, false, 0});
  bool sawC2 = false;
  bool sawC6 = false;
  for (int card = 0; card < 29; ++card) {
    const int midi = fixture.app.card().pitches[0].midiNote();
    sawC2 = sawC2 || midi == 36;
    sawC6 = sawC6 || midi == 84;
    for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                               cadenza::FramebufferProfile::Sharp}) {
      GeometryDiagnostics diagnostics;
      cadenza::MonoFramebuffer framebuffer{profile};
      cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
      cadenza::test::renderApp(fixture.app, canvas, fixture.runtime,
                               fixture.services);
      CHECK_FALSE(diagnostics.failed);
    }
    fixture.update({0, false, false, true, false, 0});
    std::array<std::int16_t, 1> sample{};
    fixture.services.renderAudio(sample.data(), sample.size());
    for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                               cadenza::FramebufferProfile::Sharp}) {
      GeometryDiagnostics diagnostics;
      cadenza::MonoFramebuffer framebuffer{profile};
      cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
      cadenza::test::renderApp(fixture.app, canvas, fixture.runtime,
                               fixture.services);
      CHECK_FALSE(diagnostics.failed);
    }
    fixture.update({0, false, false, true, false, 0});
    fixture.update({}, cadenza::SightApp::kAnswerRevealSeconds);
  }
  CHECK(sawC2);
  CHECK(sawC6);
}
