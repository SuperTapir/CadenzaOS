#include "cadenza/apps/apps.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "generated/sight_glyphs_sharp.h"
#include "generated/sight_glyphs_t_embed.h"
#include "generated/sight_cover.h"
#include "generated/sight_t_embed_cover.h"

namespace cadenza {
namespace {

constexpr std::array<int, 7> kNaturalSemitones{{0, 2, 4, 5, 7, 9, 11}};
constexpr std::array<const char*, 7> kStepNames{{"C", "D", "E", "F", "G", "A", "B"}};
constexpr std::array<const char*, 7> kSolfege{{"DO", "RE", "MI", "FA", "SOL", "LA", "TI"}};
constexpr std::array<SpelledPitch, 12> kChordRoots{{
    {SightStep::C, SightAccidental::Natural, 4},
    {SightStep::D, SightAccidental::Flat, 4},
    {SightStep::D, SightAccidental::Natural, 4},
    {SightStep::E, SightAccidental::Flat, 4},
    {SightStep::E, SightAccidental::Natural, 4},
    {SightStep::F, SightAccidental::Natural, 4},
    {SightStep::F, SightAccidental::Sharp, 4},
    {SightStep::G, SightAccidental::Natural, 4},
    {SightStep::A, SightAccidental::Flat, 4},
    {SightStep::A, SightAccidental::Natural, 4},
    {SightStep::B, SightAccidental::Flat, 4},
    {SightStep::B, SightAccidental::Natural, 4},
}};

int wrap(int value, int count) noexcept {
  value %= count;
  return value < 0 ? value + count : value;
}

const char* accidentalText(SightAccidental accidental) noexcept {
  if (accidental == SightAccidental::Sharp) return "#";
  if (accidental == SightAccidental::Flat) return "b";
  return "";
}

void pitchText(const SpelledPitch& pitch, char* output,
               std::size_t capacity, bool withOctave) noexcept {
  std::snprintf(output, capacity, withOctave ? "%s%s%d" : "%s%s",
                kStepNames[static_cast<std::size_t>(pitch.step)],
                accidentalText(pitch.accidental), pitch.octave);
}

SpelledPitch chordTone(SpelledPitch root, int diatonicOffset,
                       int semitoneOffset) noexcept {
  const int rootStep = static_cast<int>(root.step);
  const int absoluteStep = rootStep + diatonicOffset;
  SpelledPitch tone;
  tone.step = static_cast<SightStep>(absoluteStep % 7);
  tone.octave = static_cast<std::int8_t>(root.octave + absoluteStep / 7);
  const int naturalMidi = (tone.octave + 1) * 12 +
                          kNaturalSemitones[static_cast<int>(tone.step)];
  const int target = static_cast<int>(root.midiNote()) + semitoneOffset;
  tone.accidental = static_cast<SightAccidental>(target - naturalMidi);
  return tone;
}

int diatonicIndex(const SpelledPitch& pitch) noexcept {
  return static_cast<int>(pitch.octave) * 7 + static_cast<int>(pitch.step);
}

void drawNotehead(MonoCanvas& canvas, int x, int y, int radius,
                  bool offset) noexcept {
  x += offset ? radius + 2 : 0;
  canvas.fillCircle(x, y, radius, true);
  canvas.line(x + radius, y, x + radius, y - radius * 4, true);
}

struct StaffLayout {
  int space = 0;
  int left = 0;
  int right = 0;
  int trebleTop = 0;
  int bassTop = 0;
  int glyphCell = 0;
  int trebleInkTop = 0;
  int trebleInkBottom = 0;
  int bassInkTop = 0;
  int bassInkBottom = 0;
  int accidentalInkTop = 0;
  int accidentalInkBottom = 0;
};

StaffLayout staffLayout(const MonoCanvas& canvas,
                        float answerProgress) noexcept {
  const float progress = std::max(0.0F, std::min(1.0F, answerProgress));
  if (canvas.width() < 400) {
    const int right = static_cast<int>(300.0F + (172.0F - 300.0F) * progress +
                                       0.5F);
    return {7, 20, right, 32, 88, 24,
            2, 45, 12, 34, 15, 31};
  }
  const int right = static_cast<int>(380.0F + (238.0F - 380.0F) * progress +
                                     0.5F);
  return {10, 20, right, 46, 128, 32,
          2, 60, 16, 46, 20, 42};
}

void drawStaff(MonoCanvas& canvas, const SightCard& card,
               float answerProgress, float noteScale) noexcept {
  const int compact = canvas.width() < 400;
  const StaffLayout layout = staffLayout(canvas, answerProgress);
  const int space = layout.space;
  const int left = layout.left;
  const int right = layout.right;
  const int scoreWidth = right - left;
  const int trebleTop = layout.trebleTop;
  const int bassTop = layout.bassTop;
  for (int line = 0; line < 5; ++line) {
    canvas.line(left, trebleTop + line * space, right,
                trebleTop + line * space, true);
    canvas.line(left, bassTop + line * space, right,
                bassTop + line * space, true);
  }
  canvas.line(left, trebleTop, left, bassTop + 4 * space, true);
  const BitmapView& glyphs = compact ? kSightGlyphsTEmbed : kSightGlyphsSharp;
  const int glyphCell = layout.glyphCell;
  const int glyphHeight = glyphs.height;
  const BitmapBlit inkOnly{BitmapComposition::SetBlack, false, false};
  const int trebleAnchor =
      (layout.trebleInkTop + layout.trebleInkBottom) / 2;
  const int bassAnchor =
      (layout.bassInkTop + layout.bassInkBottom) / 2;
  canvas.drawBitmap(glyphs, {0, 0, glyphCell, glyphHeight}, left + 2,
                    trebleTop + 3 * space - trebleAnchor, inkOnly);
  canvas.drawBitmap(glyphs, {glyphCell, 0, glyphCell, glyphHeight}, left + 2,
                    bassTop + space - bassAnchor, inkOnly);

  if (noteScale <= 0.05F) return;
  const int noteRadius = std::max(
      1, static_cast<int>((compact ? 3.0F : 4.0F) * noteScale + 0.5F));
  const int noteX = left + scoreWidth * 3 / 5;
  std::array<int, 3> noteY{};
  for (std::size_t index = 0; index < card.count; ++index) {
    const int diatonic = diatonicIndex(card.pitches[index]);
    const bool treble = diatonic >= 28;  // C4 and above.
    const int reference = treble ? (4 * 7 + 2) : (3 * 7 + 5); // E4 / A3.
    const int referenceY = treble ? trebleTop + 4 * space : bassTop;
    const int delta = diatonic - reference;
    const int y = referenceY - delta * space / 2;
    noteY[index] = y;
    const int staffTop = treble ? trebleTop : bassTop;
    const int staffBottom = staffTop + 4 * space;
    if (y < staffTop - space / 2) {
      for (int ledger = staffTop - space; ledger >= y; ledger -= space)
        canvas.line(noteX - space, ledger, noteX + space, ledger, true);
    } else if (y > staffBottom + space / 2) {
      for (int ledger = staffBottom + space; ledger <= y; ledger += space)
        canvas.line(noteX - space, ledger, noteX + space, ledger, true);
    }
    const bool adjacent = index > 0 &&
                          std::abs(noteY[index] - noteY[index - 1]) <= space / 2;
    if (card.pitches[index].accidental != SightAccidental::Natural) {
      const int glyphIndex = card.pitches[index].accidental ==
                                     SightAccidental::Sharp
                                 ? 2
                                 : 3;
      const int accidentalAnchor =
          (layout.accidentalInkTop + layout.accidentalInkBottom) / 2;
      canvas.drawBitmap(glyphs,
                        {glyphIndex * glyphCell, 0, glyphCell, glyphHeight},
                        noteX - space * 2 - glyphCell / 2,
                        y - accidentalAnchor, inkOnly);
    }
    drawNotehead(canvas, noteX, y, noteRadius, adjacent);
  }
}

constexpr float clamp01(float value) noexcept {
  return value <= 0.0F ? 0.0F : value >= 1.0F ? 1.0F : value;
}

float levelEase(float progress) noexcept {
  const float remaining = 1.0F - clamp01(progress);
  return 1.0F - remaining * remaining * remaining;
}

float noteEntranceScale(float progress) noexcept {
  constexpr float kOvershoot = 1.25F;
  constexpr float kGrowFraction = 0.65F;
  const float p = clamp01(progress);
  if (p < kGrowFraction) {
    return kOvershoot * levelEase(p / kGrowFraction);
  }
  const float settle = levelEase((p - kGrowFraction) /
                                 (1.0F - kGrowFraction));
  return kOvershoot + (1.0F - kOvershoot) * settle;
}

void drawLevelIdentity(MonoCanvas& canvas, SightLevel level, int xOffset,
                       bool compact) noexcept {
  static constexpr std::array<const char*, 3> primary{{
      "C3 - C5", "C2 - C6", "COMMON CHORDS"}};
  static constexpr std::array<const char*, 3> secondary{{
      "NATURAL NOTES", "NATURAL NOTES", "MAJOR + MINOR"}};
  const int selected = static_cast<int>(level);
  const int width = canvas.width();
  const int contentLeft = compact ? 24 : 34;
  const Rect titleBounds{contentLeft + xOffset, compact ? 43 : 57,
                         width - contentLeft * 2, compact ? 48 : 64};
  canvas.boundedText({primary[selected], titleBounds, 2, 1,
                      TextAlign::MiddleCenter, TextOverflowPolicy::Ellipsis,
                      1, 0.0F, TextRole::Title}, true);
  canvas.text(secondary[selected], width / 2 + xOffset,
              compact ? 101 : 135, 1, true, TextAlign::MiddleCenter,
              TextRole::Compact);
}

void drawLevelRail(MonoCanvas& canvas, SightLevel level,
                   SightLevel previousLevel, float progress,
                   bool transitioning, bool compact) noexcept {
  const int width = canvas.width();
  const int height = canvas.height();
  const int selected = static_cast<int>(level);
  const int previous = static_cast<int>(previousLevel);

  const int railY = height - (compact ? 33 : 43);
  const int segmentWidth = compact ? 25 : 32;
  const int segmentGap = compact ? 7 : 9;
  const int railWidth = segmentWidth * 3 + segmentGap * 2;
  const int railLeft = (width - railWidth) / 2;
  for (int index = 0; index < 3; ++index) {
    const int x = railLeft + index * (segmentWidth + segmentGap);
    canvas.line(x, railY, x + segmentWidth - 1, railY, true);
  }
  if (transitioning && previous != selected) {
    const int oldWidth = static_cast<int>(segmentWidth * (1.0F - progress));
    const int newWidth = static_cast<int>(segmentWidth * progress);
    if (oldWidth > 0) {
      const int x = railLeft + previous * (segmentWidth + segmentGap);
      canvas.fillRect(x + (segmentWidth - oldWidth) / 2, railY - 1,
                      oldWidth, 3, true);
    }
    if (newWidth > 0) {
      const int x = railLeft + selected * (segmentWidth + segmentGap);
      canvas.fillRect(x + (segmentWidth - newWidth) / 2, railY - 1,
                      newWidth, 3, true);
    }
  } else {
    const int x = railLeft + selected * (segmentWidth + segmentGap);
    canvas.fillRect(x, railY - 1, segmentWidth, 3, true);
  }
}

void renderLevelPicker(MonoCanvas& canvas, SightLevel level,
                       SightLevel previousLevel, Seconds elapsed,
                       int direction, MotionProfile motion) noexcept {
  const bool compact = canvas.width() < 400;
  const int width = canvas.width();
  const int height = canvas.height();
  const int contentLeft = compact ? 24 : 34;
  const bool reduceMotion = motion == MotionProfile::Reduced;
  const float rawProgress = reduceMotion
                                ? 1.0F
                                : clamp01(elapsed /
                                          SightApp::kLevelTransitionSeconds);
  const float progress = levelEase(rawProgress);
  const bool transitioning = progress < 1.0F && direction != 0;

  canvas.text("SIGHT", contentLeft, compact ? 12 : 16, 1, true,
              TextAlign::TopLeft, TextRole::Compact);
  char ordinal[16]{};
  std::snprintf(ordinal, sizeof(ordinal), "LEVEL 0%d / 03",
                static_cast<int>(level) + 1);
  canvas.text(ordinal, width - contentLeft, compact ? 12 : 16, 1, true,
              TextAlign::MiddleRight, TextRole::Compact);

  if (!transitioning) {
    drawLevelIdentity(canvas, level, 0, compact);
  } else {
    const int travel = compact ? 18 : 24;
    const int outgoingOffset =
        -direction * static_cast<int>(progress * travel + 0.5F);
    const int incomingOffset = direction * static_cast<int>(
        (1.0F - progress) * travel + 0.5F);
    drawLevelIdentity(canvas, previousLevel, outgoingOffset, compact);

    const int identityTop = compact ? 39 : 52;
    const int identityBottom = compact ? 115 : 151;
    const int identityLeft = contentLeft;
    const int identityWidth = width - contentLeft * 2;
    const int revealWidth =
        static_cast<int>(identityWidth * progress + 0.5F);
    const Rect reveal = direction > 0
                            ? Rect{identityLeft + identityWidth - revealWidth,
                                   identityTop, revealWidth,
                                   identityBottom - identityTop}
                            : Rect{identityLeft, identityTop, revealWidth,
                                   identityBottom - identityTop};
    if (reveal.width > 0) {
      canvas.fillRect(reveal.x, reveal.y, reveal.width, reveal.height, false);
      canvas.setClip(reveal, false);
      drawLevelIdentity(canvas, level, incomingOffset, compact);
      canvas.resetClip();
    }
  }

  drawLevelRail(canvas, level, previousLevel, progress, transitioning,
                compact);
  canvas.text("TURN SELECT", contentLeft, height - (compact ? 12 : 16), 1,
              true, TextAlign::MiddleLeft, TextRole::Compact);
  canvas.text("CLICK START", width - contentLeft,
              height - (compact ? 12 : 16), 1, true,
              TextAlign::MiddleRight, TextRole::Compact);
}

}  // namespace

std::uint8_t SpelledPitch::midiNote() const noexcept {
  const int value = (static_cast<int>(octave) + 1) * 12 +
                    kNaturalSemitones[static_cast<int>(step)] +
                    static_cast<int>(accidental);
  return static_cast<std::uint8_t>(std::max(0, std::min(127, value)));
}

void formatSightChordSymbol(const SightCard& card, char* output,
                            std::size_t capacity) noexcept {
  if (output == nullptr || capacity == 0) return;
  output[0] = '\0';
  if (card.count != 3) return;

  char root[8]{};
  pitchText(card.pitches[0], root, sizeof(root), false);
  std::snprintf(output, capacity, "%s%s", root, card.minor ? "m" : "");
}

std::uint8_t SightApp::candidateCount() const noexcept {
  if (level_ == SightLevel::Level1) return 15;
  if (level_ == SightLevel::AllNotes) return 29;
  return 24;
}

void SightApp::onEnter() noexcept {
  level_ = SightLevel::Level1;
  previousLevel_ = level_;
  screen_ = SightScreen::LevelPicker;
  answerAction_ = SightAnswerAction::Next;
  pageTransition_ = SightPageTransition::None;
  randomState_ = 0x51A17U;
  previousCandidate_ = 0xFF;
  bagSize_ = bagPosition_ = 0;
  levelTransitionElapsed_ = kLevelTransitionSeconds;
  answerRevealElapsed_ = kAnswerRevealSeconds;
  noteEntranceElapsed_ = kNoteEntranceSeconds;
  pageTransitionElapsed_ = kPageTransitionSeconds;
  levelTransitionDirection_ = 0;
}

void SightApp::resetBag() noexcept {
  bagSize_ = candidateCount();
  bagPosition_ = 0;
  for (std::uint8_t index = 0; index < bagSize_; ++index) bag_[index] = index;
  for (std::uint8_t index = bagSize_ - 1; index > 0; --index) {
    randomState_ ^= randomState_ << 13;
    randomState_ ^= randomState_ >> 17;
    randomState_ ^= randomState_ << 5;
    const std::uint8_t other = static_cast<std::uint8_t>(randomState_ % (index + 1));
    std::swap(bag_[index], bag_[other]);
  }
  if (bagSize_ > 1 && bag_[0] == previousCandidate_) {
    std::swap(bag_[0], bag_[1]);
  }
}

void SightApp::nextCard() noexcept {
  if (bagPosition_ >= bagSize_) resetBag();
  const std::uint8_t candidate = bag_[bagPosition_++];
  previousCandidate_ = candidate;
  card_ = {};
  noteEntranceElapsed_ = 0.0F;
  if (level_ != SightLevel::CommonChords) {
    const int startOctave = level_ == SightLevel::Level1 ? 3 : 2;
    const int absoluteStep = startOctave * 7 + candidate;
    card_.pitches[0].step = static_cast<SightStep>(absoluteStep % 7);
    card_.pitches[0].octave = static_cast<std::int8_t>(absoluteStep / 7);
    card_.count = 1;
    return;
  }
  const SpelledPitch root = kChordRoots[candidate / 2];
  const bool minor = (candidate & 1U) != 0;
  card_.pitches[0] = root;
  card_.pitches[1] = chordTone(root, 2, minor ? 3 : 4);
  card_.pitches[2] = chordTone(root, 4, 7);
  card_.count = 3;
  card_.minor = minor;
}

void SightApp::playCurrent(const AppUpdateContext& context) noexcept {
  audio::MusicalNoteSet notes;
  notes.count = card_.count;
  for (std::size_t index = 0; index < card_.count; ++index) {
    notes.notes[index] = card_.pitches[index].midiNote();
  }
  context.commands.submit(SystemCommand::playMusicalNotes(notes));
}

void SightApp::update(const AppUpdateContext& context) noexcept {
  const Seconds dt = std::max(0.0F, context.dt);
  levelTransitionElapsed_ = std::min(
      kLevelTransitionSeconds, levelTransitionElapsed_ + dt);
  noteEntranceElapsed_ =
      std::min(kNoteEntranceSeconds, noteEntranceElapsed_ + dt);
  if (pageTransition_ != SightPageTransition::None) {
    pageTransitionElapsed_ =
        std::min(kPageTransitionSeconds, pageTransitionElapsed_ + dt);
    if (pageTransitionElapsed_ >= kPageTransitionSeconds) {
      pageTransition_ = SightPageTransition::None;
    } else if (context.input.turn == 0 && !context.input.clicked &&
               !context.input.longPressed) {
      return;
    } else {
      pageTransition_ = SightPageTransition::None;
    }
  }
  if (screen_ == SightScreen::AnswerExiting) {
    answerRevealElapsed_ = std::max(0.0F, answerRevealElapsed_ - dt);
    if (answerRevealElapsed_ <= 0.0F) {
      nextCard();
      screen_ = SightScreen::Question;
    }
    return;
  }
  answerRevealElapsed_ = std::min(
      kAnswerRevealSeconds, answerRevealElapsed_ + dt);
  if (context.input.longPressed) return;
  if (screen_ == SightScreen::LevelPicker) {
    if (context.input.turn != 0) {
      previousLevel_ = level_;
      level_ = static_cast<SightLevel>(wrap(static_cast<int>(level_) +
                                                context.input.turn, 3));
      levelTransitionDirection_ = context.input.turn > 0 ? 1 : -1;
      levelTransitionElapsed_ = 0.0F;
      context.commands.submit(
          SystemCommand::playSound(audio::SoundCue::Navigate));
    }
    if (context.input.clicked) {
      context.commands.submit(
          SystemCommand::playSound(audio::SoundCue::Confirm));
      bagSize_ = bagPosition_ = 0;
      previousCandidate_ = 0xFF;
      nextCard();
      screen_ = SightScreen::Question;
      pageTransition_ = context.system.motionProfile == MotionProfile::Reduced
                            ? SightPageTransition::None
                            : SightPageTransition::ToQuestion;
      pageTransitionElapsed_ = 0.0F;
    }
    return;
  }
  if (screen_ == SightScreen::Question) {
    if (context.input.clicked) {
      screen_ = SightScreen::Answer;
      answerAction_ = SightAnswerAction::Next;
      answerRevealElapsed_ = 0.0F;
      playCurrent(context);
    }
    return;
  }
  if (context.input.turn != 0) {
    answerAction_ = static_cast<SightAnswerAction>(
        wrap(static_cast<int>(answerAction_) + context.input.turn, 3));
  }
  if (!context.input.clicked) return;
  if (answerAction_ == SightAnswerAction::Replay) {
    playCurrent(context);
  } else if (answerAction_ == SightAnswerAction::Next) {
    if (context.system.motionProfile == MotionProfile::Reduced) {
      nextCard();
      screen_ = SightScreen::Question;
    } else {
      screen_ = SightScreen::AnswerExiting;
    }
  } else {
    screen_ = SightScreen::LevelPicker;
    pageTransition_ = context.system.motionProfile == MotionProfile::Reduced
                          ? SightPageTransition::None
                          : SightPageTransition::ToLevel;
    pageTransitionElapsed_ = 0.0F;
    context.commands.submit(
        SystemCommand::playSound(audio::SoundCue::Back));
  }
}

void SightApp::render(MonoCanvas& canvas,
                      const AppRenderContext& context) noexcept {
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  if (pageTransition_ == SightPageTransition::ToQuestion) {
    renderLevelPicker(canvas, level_, previousLevel_, levelTransitionElapsed_,
                      levelTransitionDirection_, context.system.motionProfile);
    const float progress =
        levelEase(pageTransitionElapsed_ / kPageTransitionSeconds);
    const int revealWidth = static_cast<int>(width * progress + 0.5F);
    if (revealWidth > 0) {
      const Rect reveal{width - revealWidth, 0, revealWidth, height};
      canvas.setClip(reveal, false);
      canvas.fillRect(reveal.x, reveal.y, reveal.width, reveal.height, false);
      renderStudyPage(canvas, context, SightScreen::Question);
      canvas.resetClip();
    }
    return;
  }
  if (pageTransition_ == SightPageTransition::ToLevel) {
    renderStudyPage(canvas, context, SightScreen::Answer);
    const float progress =
        levelEase(pageTransitionElapsed_ / kPageTransitionSeconds);
    const int revealWidth = static_cast<int>(width * progress + 0.5F);
    if (revealWidth > 0) {
      const Rect reveal{0, 0, revealWidth, height};
      canvas.setClip(reveal, false);
      canvas.fillRect(reveal.x, reveal.y, reveal.width, reveal.height, false);
      renderLevelPicker(canvas, level_, previousLevel_,
                        levelTransitionElapsed_, levelTransitionDirection_,
                        context.system.motionProfile);
      canvas.resetClip();
    }
    return;
  }
  if (screen_ == SightScreen::LevelPicker) {
    renderLevelPicker(canvas, level_, previousLevel_, levelTransitionElapsed_,
                      levelTransitionDirection_, context.system.motionProfile);
    return;
  }
  renderStudyPage(canvas, context, screen_);
}

void SightApp::renderStudyPage(MonoCanvas& canvas,
                               const AppRenderContext& context,
                               SightScreen studyScreen) noexcept {
  const int width = canvas.width();
  const int height = canvas.height();

  canvas.text("SIGHT", 10, 10, 1, true, TextAlign::TopLeft,
              TextRole::Compact);

  const bool compact = width < 400;
  const bool reduceMotion =
      context.system.motionProfile == MotionProfile::Reduced;
  const bool presentingAnswer = studyScreen == SightScreen::Answer ||
                                studyScreen == SightScreen::AnswerExiting;
  const float answerRawProgress =
      presentingAnswer
          ? (reduceMotion
                 ? 1.0F
                 : clamp01(answerRevealElapsed_ / kAnswerRevealSeconds))
          : 0.0F;
  const float answerProgress = levelEase(answerRawProgress);
  float noteScale = 1.0F;
  if (studyScreen == SightScreen::Question && !reduceMotion) {
    noteScale = noteEntranceScale(noteEntranceElapsed_ / kNoteEntranceSeconds);
  } else if (studyScreen == SightScreen::AnswerExiting && !reduceMotion) {
    noteScale = answerRawProgress;
  }
  drawStaff(canvas, card_, answerProgress, noteScale);
  if (studyScreen == SightScreen::Question) {
    canvas.text("CLICK TO REVEAL", width / 2, height - 12, 1, true,
                TextAlign::MiddleCenter, TextRole::Compact);
    return;
  }

  char primaryAnswer[24]{};
  char secondaryAnswer[40]{};
  if (card_.count == 1) {
    char pitch[12]{};
    pitchText(card_.pitches[0], pitch, sizeof(pitch), true);
    std::snprintf(primaryAnswer, sizeof(primaryAnswer), "%s", pitch);
    std::snprintf(secondaryAnswer, sizeof(secondaryAnswer), "%s",
                  kSolfege[static_cast<int>(card_.pitches[0].step)]);
  } else {
    char first[8]{}, second[8]{}, third[8]{};
    pitchText(card_.pitches[0], first, sizeof(first), false);
    pitchText(card_.pitches[1], second, sizeof(second), false);
    pitchText(card_.pitches[2], third, sizeof(third), false);
    formatSightChordSymbol(card_, primaryAnswer, sizeof(primaryAnswer));
    std::snprintf(secondaryAnswer, sizeof(secondaryAnswer), "%s  %s  %s",
                  first, second, third);
  }
  const Rect answerPanel{compact ? 182 : 250,
                         compact ? 30 : 42,
                         compact ? 128 : 140,
                         compact ? 92 : 132};
  const int animatedPanelX = static_cast<int>(
      answerPanel.x + (width - answerPanel.x) * (1.0F - answerProgress) +
      0.5F);
  const Rect animatedPanel{animatedPanelX, answerPanel.y,
                           answerPanel.width, answerPanel.height};
  if (answerProgress > 0.0F) {
    canvas.setClip({answerPanel.x, answerPanel.y, width - answerPanel.x,
                    answerPanel.height}, false);
    canvas.rect(animatedPanel.x, animatedPanel.y, animatedPanel.width,
                animatedPanel.height, true);
    const int dividerY = animatedPanel.y + animatedPanel.height * 3 / 5;
    canvas.line(animatedPanel.x + 8, dividerY,
                animatedPanel.x + animatedPanel.width - 9, dividerY, true);
    canvas.boundedText(
        {primaryAnswer,
         {animatedPanel.x, animatedPanel.y, animatedPanel.width,
          animatedPanel.height * 3 / 5},
         1, 1, TextAlign::MiddleCenter, TextOverflowPolicy::Ellipsis, 1, 0.0F,
         TextRole::Title},
        true);
    canvas.boundedText(
        {secondaryAnswer,
         {animatedPanel.x,
          animatedPanel.y + animatedPanel.height * 3 / 5,
          animatedPanel.width, animatedPanel.height * 2 / 5},
         1, 1, TextAlign::MiddleCenter, TextOverflowPolicy::Ellipsis, 1, 0.0F,
         TextRole::Compact},
        true);
    canvas.resetClip();
  }
  // Preserve the question frame exactly at reveal progress zero; the action
  // strip wipes this prompt as it enters.
  canvas.text("CLICK TO REVEAL", width / 2, height - 12, 1, true,
              TextAlign::MiddleCenter, TextRole::Compact);
  static constexpr std::array<const char*, 3> actions{{"REPLAY", "NEXT", "LEVEL"}};
  const float actionRawProgress = reduceMotion
                                      ? 1.0F
                                      : clamp01((answerRawProgress - 0.15F) /
                                                0.85F);
  const float actionProgress = levelEase(actionRawProgress);
  const int buttonHeight = compact ? 24 : 30;
  const int finalStripY = height - buttonHeight;
  const int stripY = finalStripY + static_cast<int>(
                                         buttonHeight * (1.0F - actionProgress) +
                                         0.5F);
  if (actionProgress > 0.0F) {
    canvas.setClip({0, finalStripY, width, buttonHeight}, false);
    canvas.fillRect(0, stripY, width, buttonHeight, false);
    const int cellInset = compact ? 8 : 12;
    for (int index = 0; index < 3; ++index) {
      const int cellLeft = index * width / 3;
      const int cellRight = (index + 1) * width / 3;
      const bool selected = index == static_cast<int>(answerAction_);
      if (selected) {
        canvas.fillRect(cellLeft + cellInset, stripY,
                        cellRight - cellLeft - cellInset * 2, buttonHeight,
                        true);
      }
      canvas.text(actions[index], (cellLeft + cellRight) / 2,
                  stripY + buttonHeight / 2, 1, !selected,
                  TextAlign::MiddleCenter, TextRole::Compact);
    }
    canvas.resetClip();
  }
}

bool SightApp::renderLauncherCover(MonoCanvas& canvas, Rect bounds) const noexcept {
  if (bounds.width <= 0 || bounds.height <= 0) return false;
  const BitmapView& bitmap = bounds.width <= kSightTEmbedCover.width
                                 ? kSightTEmbedCover
                                 : kSightCover;
  if (!bitmap.valid()) return false;
  canvas.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, false);
  const int copyWidth = std::min(bounds.width, bitmap.width);
  const int copyHeight = std::min(bounds.height, bitmap.height);
  const Rect source{(bitmap.width - copyWidth) / 2,
                    (bitmap.height - copyHeight) / 2,
                    copyWidth, copyHeight};
  canvas.drawBitmap(bitmap, source,
                    bounds.x + (bounds.width - copyWidth) / 2,
                    bounds.y + (bounds.height - copyHeight) / 2);
  return true;
}

}  // namespace cadenza
