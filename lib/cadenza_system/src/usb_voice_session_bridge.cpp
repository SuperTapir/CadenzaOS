#include "cadenza/voice/usb_voice_session_bridge.h"

namespace cadenza::voice {

void UsbVoiceSessionBridge::changed() noexcept {
  lifecycleChanges_.fetch_add(1, std::memory_order_relaxed);
  generation_.fetch_add(1, std::memory_order_release);
}

void UsbVoiceSessionBridge::deactivateCapture() noexcept {
  if (capture_) capture_->setIntent(VoiceConsumer::Usb, false);
}

void UsbVoiceSessionBridge::onConnectedFromCallback(bool connected) noexcept {
  const bool previous = connected_.exchange(connected, std::memory_order_acq_rel);
  if (previous == connected && (connected || !altSettingActive())) return;
  if (!connected) {
    altSettingActive_.store(false, std::memory_order_release);
    deactivateCapture();
  }
  changed();
}

void UsbVoiceSessionBridge::onAltSettingFromCallback(bool active) noexcept {
  if (active && !connected()) active = false;
  const bool previous =
      altSettingActive_.exchange(active, std::memory_order_acq_rel);
  if (previous == active) return;
  if (!active) deactivateCapture();
  changed();
}

bool UsbVoiceSessionBridge::synchronize(
    UsbVoicePacketizer& packetizer) noexcept {
  const std::uint32_t generation =
      generation_.load(std::memory_order_acquire);
  if (generation == appliedGeneration_) return packetizer.streaming();

  // Always cross the inactive state. This flushes an in-flight partial block
  // after a close+open pair that occurred entirely between two data callbacks.
  packetizer.setAltSettingActive(false);
  packetizer.setConnected(false);
  const bool connectedNow = connected();
  const bool activeNow = connectedNow && altSettingActive();
  packetizer.setConnected(connectedNow);
  packetizer.setAltSettingActive(activeNow);
  appliedGeneration_ = generation;
  synchronizations_.fetch_add(1, std::memory_order_relaxed);
  return packetizer.streaming();
}

}  // namespace cadenza::voice
