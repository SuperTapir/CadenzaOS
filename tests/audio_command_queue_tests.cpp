#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>

#include "cadenza/audio/audio_command_queue.h"

TEST_CASE("audio command queue preserves FIFO order") {
  cadenza::audio::AudioCommandQueue queue;
  REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::play(
      cadenza::audio::SoundCue::Navigate)));
  REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::setVolume(
      cadenza::audio::SoundVolume::Low)));
  REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::stopAll()));

  cadenza::audio::AudioCommand command;
  REQUIRE(queue.tryPop(command));
  CHECK(command.kind == cadenza::audio::AudioCommandKind::PlayCue);
  CHECK(command.cue == cadenza::audio::SoundCue::Navigate);
  REQUIRE(queue.tryPop(command));
  CHECK(command.kind == cadenza::audio::AudioCommandKind::SetVolume);
  CHECK(command.volume == cadenza::audio::SoundVolume::Low);
  REQUIRE(queue.tryPop(command));
  CHECK(command.kind == cadenza::audio::AudioCommandKind::StopAll);
  CHECK_FALSE(queue.tryPop(command));
}
TEST_CASE("droppable commands cannot consume critical reserve") {
  cadenza::audio::AudioCommandQueue queue;
  for (std::size_t index = 0;
       index < cadenza::audio::AudioCommandQueue::kDroppableLimit; ++index) {
    REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::play(
        cadenza::audio::SoundCue::Navigate)));
  }
  CHECK_FALSE(queue.tryPush(cadenza::audio::AudioCommand::play(
      cadenza::audio::SoundCue::Boundary)));
  CHECK(queue.overflowCount() == 1);
  CHECK(queue.sizeApprox() ==
        cadenza::audio::AudioCommandQueue::kDroppableLimit);

  REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::play(
      cadenza::audio::SoundCue::Confirm)));
  REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::play(
      cadenza::audio::SoundCue::Back)));
  REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::setVolume(
      cadenza::audio::SoundVolume::Muted)));
  REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::stopAll()));
  CHECK(queue.sizeApprox() == cadenza::audio::AudioCommandQueue::kCapacity);
  CHECK_FALSE(queue.tryPush(cadenza::audio::AudioCommand::play(
      cadenza::audio::SoundCue::Reject)));
  CHECK(queue.overflowCount() == 2);
}

TEST_CASE("critical reserve does not reorder existing commands") {
  cadenza::audio::AudioCommandQueue queue;
  for (std::size_t index = 0;
       index < cadenza::audio::AudioCommandQueue::kDroppableLimit; ++index) {
    const auto cue = index % 2 == 0 ? cadenza::audio::SoundCue::Navigate
                                    : cadenza::audio::SoundCue::Boundary;
    REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::play(cue)));
  }
  REQUIRE(queue.tryPush(cadenza::audio::AudioCommand::play(
      cadenza::audio::SoundCue::Confirm)));

  cadenza::audio::AudioCommand command;
  for (std::size_t index = 0;
       index < cadenza::audio::AudioCommandQueue::kDroppableLimit; ++index) {
    REQUIRE(queue.tryPop(command));
    const auto expected = index % 2 == 0 ? cadenza::audio::SoundCue::Navigate
                                         : cadenza::audio::SoundCue::Boundary;
    CHECK(command.cue == expected);
  }
  REQUIRE(queue.tryPop(command));
  CHECK(command.cue == cadenza::audio::SoundCue::Confirm);
}
