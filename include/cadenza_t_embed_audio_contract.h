#pragma once

#include <cstddef>
#include <cstdint>

namespace cadenza::t_embed_audio {

struct RealtimeEndpoint {
  std::uint8_t i2sPort;
  std::uint32_t sampleRateHz;
  std::uint8_t taskCore;
  std::uint8_t taskPriority;
  std::uint32_t taskStackBytes;
  std::uint8_t dmaDescriptors;
  std::uint16_t dmaFrames;
};

inline constexpr RealtimeEndpoint kSpeaker{
    0, 44100, 0, 2, 4096, 4, 128};
inline constexpr RealtimeEndpoint kMicrophone{
    1, 48000, 1, 5, 6144, 4, 240};

inline constexpr int kSpeakerBclkPin = 7;
inline constexpr int kSpeakerWordSelectPin = 5;
inline constexpr int kSpeakerDataOutPin = 6;
inline constexpr int kMicrophoneMclkPin = 48;
inline constexpr int kMicrophoneBclkPin = 47;
inline constexpr int kMicrophoneWordSelectPin = 21;
inline constexpr int kMicrophoneDataInPin = 14;

constexpr bool resourcesAreIndependent() noexcept {
  return kSpeaker.i2sPort != kMicrophone.i2sPort &&
         kSpeaker.taskCore != kMicrophone.taskCore &&
         kSpeakerBclkPin != kMicrophoneBclkPin &&
         kSpeakerWordSelectPin != kMicrophoneWordSelectPin &&
         kSpeakerDataOutPin != kMicrophoneDataInPin;
}

static_assert(resourcesAreIndependent(),
              "T-Embed audio input and output must not share realtime owners");

}  // namespace cadenza::t_embed_audio
