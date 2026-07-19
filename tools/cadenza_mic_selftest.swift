import AVFoundation
import CoreAudio
import Foundation

func findCadenzaDeviceID() -> AudioDeviceID? {
  var size: UInt32 = 0
  var address = AudioObjectPropertyAddress(
    mSelector: kAudioHardwarePropertyDevices,
    mScope: kAudioObjectPropertyScopeGlobal,
    mElement: kAudioObjectPropertyElementMain)
  guard AudioObjectGetPropertyDataSize(
    AudioObjectID(kAudioObjectSystemObject), &address, 0, nil, &size) == noErr
  else { return nil }
  let count = Int(size) / MemoryLayout<AudioDeviceID>.size
  var ids = [AudioDeviceID](repeating: 0, count: count)
  guard AudioObjectGetPropertyData(
    AudioObjectID(kAudioObjectSystemObject), &address, 0, nil, &size, &ids) == noErr
  else { return nil }
  for id in ids {
    var nameAddr = AudioObjectPropertyAddress(
      mSelector: kAudioObjectPropertyName,
      mScope: kAudioObjectPropertyScopeGlobal,
      mElement: kAudioObjectPropertyElementMain)
    var nameRef: Unmanaged<CFString>?
    var nameSize = UInt32(MemoryLayout<Unmanaged<CFString>?>.size)
    guard AudioObjectGetPropertyData(id, &nameAddr, 0, nil, &nameSize, &nameRef) == noErr
    else { continue }
    let name = nameRef?.takeUnretainedValue() as String? ?? ""
    var streamAddr = AudioObjectPropertyAddress(
      mSelector: kAudioDevicePropertyStreams,
      mScope: kAudioDevicePropertyScopeInput,
      mElement: kAudioObjectPropertyElementMain)
    var streamSize: UInt32 = 0
    if AudioObjectGetPropertyDataSize(id, &streamAddr, 0, nil, &streamSize) != noErr
      || streamSize == 0 { continue }
    if name.localizedCaseInsensitiveContains("Cadenza")
      || name.localizedCaseInsensitiveContains("Microphone streaming")
    {
      fputs("device \(id) \(name)\n", stderr)
      return id
    }
  }
  return nil
}

func recordOnce(seconds: Double) -> (code: Int32, line: String) {
  guard let device = findCadenzaDeviceID() else {
    return (2, "RESULT=NO_CADENZA")
  }
  var defaultAddr = AudioObjectPropertyAddress(
    mSelector: kAudioHardwarePropertyDefaultInputDevice,
    mScope: kAudioObjectPropertyScopeGlobal,
    mElement: kAudioObjectPropertyElementMain)
  var deviceId = device
  _ = AudioObjectSetPropertyData(
    AudioObjectID(kAudioObjectSystemObject), &defaultAddr, 0, nil,
    UInt32(MemoryLayout<AudioDeviceID>.size), &deviceId)

  let t0 = CFAbsoluteTimeGetCurrent()
  let engine = AVAudioEngine()
  let input = engine.inputNode
  let format = input.inputFormat(forBus: 0)
  var samples = [Float]()
  let lock = NSLock()
  var firstTapAt: CFAbsoluteTime?
  input.installTap(onBus: 0, bufferSize: 1024, format: format) { buffer, _ in
    if firstTapAt == nil { firstTapAt = CFAbsoluteTimeGetCurrent() }
    guard let channel = buffer.floatChannelData?[0] else { return }
    let count = Int(buffer.frameLength)
    lock.lock()
    samples.append(contentsOf: UnsafeBufferPointer(start: channel, count: count))
    lock.unlock()
  }
  do {
    try engine.start()
  } catch {
    return (5, "RESULT=START_FAIL open_ms=\(Int((CFAbsoluteTimeGetCurrent() - t0) * 1000))")
  }
  Thread.sleep(forTimeInterval: seconds)
  engine.stop()
  input.removeTap(onBus: 0)
  let wallMs = (CFAbsoluteTimeGetCurrent() - t0) * 1000
  let firstMs = firstTapAt.map { ($0 - t0) * 1000 } ?? -1
  lock.lock()
  let x = samples
  lock.unlock()
  guard !x.isEmpty else {
    return (3, String(format: "open_wall=%.0fms first_tap=%.0fms RESULT=EMPTY", wallMs, firstMs))
  }

  var peak: Float = 0, sum: Float = 0, dc: Float = 0, clip = 0, flips = 0, hf: Float = 0
  for i in 0..<x.count {
    let v = x[i]
    dc += v
    sum += v * v
    let a = abs(v)
    if a > peak { peak = a }
    if a >= 0.999 { clip += 1 }
    if i > 0 {
      if x[i] * x[i - 1] < 0 { flips += 1 }
      let d = x[i] - x[i - 1]
      hf += d * d
    }
  }
  dc /= Float(x.count)
  let rms = sqrt(sum / Float(x.count))
  hf = sqrt(hf / Float(max(1, x.count - 1)))
  let silent = rms < 0.0002 && peak < 0.001
  // Sticky 炸麦 signature: elevated sample-to-sample energy while still audible.
  let harsh = !silent && hf > max(0.015 as Float, rms * 2.5) && peak > 0.01
  // macOS "blinking mic / long loading" proxy: first audio arrives very late.
  let slowOpen = firstMs > 1500 || wallMs > seconds * 1000 + 1500
  let status: String
  let code: Int32
  if silent {
    status = "SILENT"; code = 6
  } else if harsh {
    status = "HARSH"; code = 4
  } else if slowOpen {
    status = "SLOW_OPEN"; code = 7
  } else {
    status = "OK"; code = 0
  }
  let line = String(
    format: "open_wall=%.0fms first_tap=%.0fms n=%d peak=%.4f rms=%.4f clip=%d dc=%.5f hf_diff=%.4f zcr=%.0f/s RESULT=%@",
    wallMs, firstMs, x.count, peak, rms, clip, dc, hf, Double(flips) / seconds, status)
  return (code, line)
}

let seconds = CommandLine.arguments.count > 1 ? (Double(CommandLine.arguments[1]) ?? 2.0) : 2.0
let cycles = CommandLine.arguments.count > 2 ? (Int(CommandLine.arguments[2]) ?? 1) : 1
var worst: Int32 = 0
for i in 1...max(1, cycles) {
  let result = recordOnce(seconds: seconds)
  print("cycle\(i): \(result.line)")
  if result.code != 0 { worst = result.code }
  if i < cycles { Thread.sleep(forTimeInterval: 0.6) }
}
exit(worst)
