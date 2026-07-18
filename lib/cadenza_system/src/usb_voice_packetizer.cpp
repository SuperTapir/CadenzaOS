#include "cadenza/voice/usb_voice_packetizer.h"

#include <algorithm>

namespace cadenza::voice {

void UsbVoicePacketizer::setConnected(bool connected) noexcept {
  connected_ = connected;
  reconcile();
}

void UsbVoicePacketizer::setAltSettingActive(bool active) noexcept {
  altSettingActive_ = active;
  reconcile();
}

void UsbVoicePacketizer::reconcile() noexcept {
  const bool requested =
      capture_ != nullptr && connected_ && altSettingActive_;
  if (requested == streaming_) return;
  resetCurrentBlock();
  if (!requested) {
    if (capture_) capture_->setIntent(VoiceConsumer::Usb, false);
    streaming_ = false;
    ++diagnostics_.deactivations;
    return;
  }
  streaming_ = capture_->setIntent(VoiceConsumer::Usb, true);
  if (streaming_) ++diagnostics_.activations;
}

bool UsbVoicePacketizer::nextPacket(UsbVoicePacket& packet) noexcept {
  if (!streaming_ || !capture_) return false;
  if (currentOffset_ >= kVoiceSamplesPerBlock) {
    if (!capture_->tryPop(VoiceConsumer::Usb, currentBlock_)) {
      packet.samples.fill(0);
      ++diagnostics_.packets;
      ++diagnostics_.underflows;
      return true;
    }
    currentOffset_ = 0;
  }
  std::copy_n(currentBlock_.samples.begin() + currentOffset_,
              kUsbVoiceSamplesPerPacket, packet.samples.begin());
  currentOffset_ += kUsbVoiceSamplesPerPacket;
  ++diagnostics_.packets;
  return true;
}

void UsbVoicePacketizer::shutdown() noexcept {
  if (capture_ && streaming_) {
    capture_->setIntent(VoiceConsumer::Usb, false);
    ++diagnostics_.deactivations;
  }
  streaming_ = false;
  connected_ = false;
  altSettingActive_ = false;
  resetCurrentBlock();
  capture_ = nullptr;
}

void UsbVoicePacketizer::resetCurrentBlock() noexcept {
  currentBlock_ = {};
  currentOffset_ = kVoiceSamplesPerBlock;
}

}  // namespace cadenza::voice
