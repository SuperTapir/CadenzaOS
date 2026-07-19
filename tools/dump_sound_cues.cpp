#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "cadenza/audio/interaction_sound.h"
#include "cadenza/audio/sound_cue_library.h"

namespace {
constexpr std::size_t kTailSamples = 1024;
constexpr std::size_t kDemoGapSamples = 6615;  // 150 ms at 44.1 kHz.

void write16(std::ostream& stream, std::uint16_t value) {
  stream.put(static_cast<char>(value & 0xFFU));
  stream.put(static_cast<char>(value >> 8U));
}

void write32(std::ostream& stream, std::uint32_t value) {
  write16(stream, static_cast<std::uint16_t>(value & 0xFFFFU));
  write16(stream, static_cast<std::uint16_t>(value >> 16U));
}

bool writeWav(const std::filesystem::path& path,
              const std::vector<std::int16_t>& samples) {
  std::ofstream output(path, std::ios::binary);
  if (!output) return false;
  const auto dataBytes = static_cast<std::uint32_t>(samples.size() * 2U);
  output.write("RIFF", 4);
  write32(output, 36U + dataBytes);
  output.write("WAVEfmt ", 8);
  write32(output, 16);
  write16(output, 1);  // PCM
  write16(output, 1);  // mono
  write32(output, 44100);
  write32(output, 44100U * 2U);
  write16(output, 2);
  write16(output, 16);
  output.write("data", 4);
  write32(output, dataBytes);
  for (const auto sample : samples) {
    write16(output, static_cast<std::uint16_t>(sample));
  }
  return output.good();
}

std::vector<std::int16_t> renderCueSamples(cadenza::audio::SoundCue cue) {
  cadenza::audio::InteractionSoundService service;
  if (!service.play(cue)) return {};
  const auto profile = cadenza::audio::InteractionSoundService::profile(cue);
  const std::size_t sampleCount = static_cast<std::size_t>(
                                      std::ceil(profile.durationSeconds *
                                                cadenza::audio::kSampleRate)) +
                                  kTailSamples;
  std::vector<std::int16_t> samples(sampleCount);
  service.render(samples.data(), samples.size());
  return samples;
}

bool renderCue(cadenza::audio::SoundCue cue,
               const std::filesystem::path& path) {
  const auto samples = renderCueSamples(cue);
  if (samples.empty()) return false;
  const auto peakSample = *std::max_element(
      samples.begin(), samples.end(), [](std::int16_t left, std::int16_t right) {
        return std::abs(static_cast<int>(left)) <
               std::abs(static_cast<int>(right));
      });
  if (!writeWav(path, samples)) return false;
  std::cout << cadenza::audio::soundCueName(cue) << ", peak="
            << std::abs(static_cast<int>(peakSample)) << ", " << path << '\n';
  return true;
}

bool renderDemo(const std::filesystem::path& path,
                const std::vector<cadenza::audio::SoundCue>& cues) {
  std::vector<std::int16_t> output(kDemoGapSamples / 2U, 0);
  for (const auto cue : cues) {
    const auto samples = renderCueSamples(cue);
    if (samples.empty()) return false;
    output.insert(output.end(), samples.begin(), samples.end());
    output.insert(output.end(), kDemoGapSamples, 0);
  }
  return writeWav(path, output);
}

bool renderDemos(const std::filesystem::path& directory) {
  using cadenza::audio::SoundCue;
  return renderDemo(directory / "demo-input.wav",
                    {SoundCue::Navigate, SoundCue::Navigate,
                     SoundCue::Navigate, SoundCue::Navigate,
                     SoundCue::Boundary}) &&
         renderDemo(directory / "demo-action.wav",
                    {SoundCue::Confirm, SoundCue::Back, SoundCue::ToggleOn,
                     SoundCue::ToggleOff, SoundCue::Reject}) &&
         renderDemo(directory / "demo-outcome.wav",
                    {SoundCue::Complete, SoundCue::Warning,
                     SoundCue::Failure}) &&
         renderDemo(directory / "demo-system.wav",
                    {SoundCue::Notification, SoundCue::Connect,
                     SoundCue::Disconnect, SoundCue::PowerOn,
                     SoundCue::PowerOff, SoundCue::TimerComplete});
}

bool cueFromName(const std::string& name, cadenza::audio::SoundCue& cue) {
  for (std::size_t index = 0;
       index < static_cast<std::size_t>(cadenza::audio::SoundCue::Count);
       ++index) {
    const auto candidate = static_cast<cadenza::audio::SoundCue>(index);
    if (name == cadenza::audio::soundCueName(candidate)) {
      cue = candidate;
      return true;
    }
  }
  return false;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--all") {
    const std::filesystem::path directory = argv[2];
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
      std::cerr << "cannot create output directory: " << error.message()
                << '\n';
      return 1;
    }
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(cadenza::audio::SoundCue::Count);
         ++index) {
      const auto cue = static_cast<cadenza::audio::SoundCue>(index);
      if (!renderCue(cue, directory /
                              (std::string(cadenza::audio::soundCueName(cue)) +
                               ".wav"))) {
        return 1;
      }
    }
    return renderDemos(directory) ? 0 : 1;
  }

  if (argc == 3) {
    cadenza::audio::SoundCue cue;
    if (!cueFromName(argv[1], cue)) {
      std::cerr << "unknown cue: " << argv[1] << '\n';
      return 2;
    }
    return renderCue(cue, argv[2]) ? 0 : 1;
  }

  std::cerr << "usage:\n  " << argv[0]
            << " --all OUTPUT_DIR\n  " << argv[0]
            << " CUE OUTPUT.wav\n";
  return 2;
}
