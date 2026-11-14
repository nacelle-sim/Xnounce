/// @file
/// Implements `src/audio/hearing.h`; the interface is documented there.
#include "audio/hearing.h"

#include <algorithm>
#include <cmath>

namespace xn {

namespace {

constexpr float kMinInterphone = 0.15f;
/// The ceiling speakers sit this far above a seated head and this far either
/// side of the centre line (VP_SPEC § 4.5).
constexpr double kSpeakerAboveM = 0.9;
constexpr double kSpeakerSideM = 0.9;
constexpr float kMinSpeakerLevel = 0.5f;
constexpr float kMaxSpeakerLevel = 1.2f;

/// The summed sound of every speaker at one place: the square root of the
/// summed intensities, which fall with the distance squared.
double speakerAmplitude(double aft, double right, const CabinGeometry& g,
                        double pitch) {
  const int rows = std::max(
      1, static_cast<int>(
             std::lround(static_cast<double>(g.cabinLengthM) / pitch)));
  double sum = 0;
  for (int k = 0; k < rows; ++k) {
    const double z = g.cabinFrontM + g.cabinLengthM * (k + 0.5) / rows;
    for (double side : {-kSpeakerSideM, kSpeakerSideM}) {
      const double d2 = (right - side) * (right - side) +
                        (aft - z) * (aft - z) + kSpeakerAboveM * kSpeakerAboveM;
      sum += 1.0 / d2;
    }
  }
  return std::sqrt(sum);
}

float c01(float v) {
  return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
}

}  // namespace

float doorOpenFraction(const Settings& set, const SimState& s) {
  if (set.useAircraftDoorBlend && s.doorBlend >= 0) return c01(s.doorBlend);
  return c01(s.cockpitDoor);
}

float aircraftVolume(const Settings& set, const SimState& s) {
  if (!set.followAircraftVolume || s.aircraftVolume < 0) return 1.0f;
  return c01(s.aircraftVolume);
}

float cabinSpeakerLevel(float aftM, float rightM, const CabinGeometry& cabin,
                        float pitchM) {
  if (!(cabin.cabinLengthM > 0) || !(pitchM > 0) || !std::isfinite(aftM) ||
      !std::isfinite(rightM))
    return 1.0f;
  // The cabin's average, over the seats, once per cabin.
  static CabinGeometry cachedCabin;
  static float cachedPitch = -1;
  static double average = 1;
  if (cachedPitch != pitchM || cachedCabin.cabinFrontM != cabin.cabinFrontM ||
      cachedCabin.cabinLengthM != cabin.cabinLengthM) {
    double sum = 0;
    int n = 0;
    for (int j = 0; j <= 100; ++j)
      for (int i = -6; i <= 6; ++i, ++n)
        sum +=
            speakerAmplitude(cabin.cabinFrontM + cabin.cabinLengthM * j / 100.0,
                             i * 0.25, cabin, pitchM);
    average = n > 0 && sum > 0 ? sum / n : 1.0;
    cachedCabin = cabin;
    cachedPitch = pitchM;
  }
  const double level = speakerAmplitude(aftM, rightM, cabin, pitchM) / average;
  return std::clamp(static_cast<float>(level), kMinSpeakerLevel,
                    kMaxSpeakerLevel);
}

Hearing cabinHearing(const Settings& set, const SimState& s) {
  Hearing h;
  if (!set.realisticHearing) {
    h.clear = 1.0f;
    return h;
  }
  if (s.viewExternal) {
    h.clear = c01(set.exteriorGain);
    return h;
  }
  if (s.cameraInCabin) {
    h.clear = s.cameraKnown ? cabinSpeakerLevel(s.cameraAftM, s.cameraRightM,
                                                s.cabin, set.speakerPitchM)
                            : 1.0f;
    return h;
  }
  const float open = doorOpenFraction(set, s);
  const float floor = c01(set.hearingFloor);
  h.clear = c01(set.doorLeak) * open;
  if (s.paListen) h.clear = std::max(h.clear, c01(s.paListenVol));
  if (set.muffle) {
    // The closed door lets a dull, quiet copy through, which fades out as
    // the door opens (§ 4.5, D-14).
    h.muffled = floor * (1.0f - open);
  } else {
    h.clear = std::max(h.clear, floor);
  }
  return h;
}

float soundGain(SoundKind kind, const Settings& set, const SimState& s,
                bool muted) {
  if (muted) return 0.0f;
  const float master =
      std::max(0.0f, set.masterVolume) * aircraftVolume(set, s);
  switch (kind) {
    case SoundKind::Pa:
      return master * set.paVolume * cabinHearing(set, s).clear;
    case SoundKind::Music:
      return master * set.musicVolume * cabinHearing(set, s).clear;
    case SoundKind::Interphone:
      return master * set.interphoneVolume *
             (s.interphoneOn ? std::max(kMinInterphone, s.interphoneVol)
                             : 1.0f);
    case SoundKind::Chime:
      return master * set.chimeVolume;
  }
  return 0.0f;
}

float muffledGain(SoundKind kind, const Settings& set, const SimState& s,
                  bool muted) {
  if (muted || !set.muffle) return 0.0f;
  const float master =
      std::max(0.0f, set.masterVolume) * aircraftVolume(set, s);
  switch (kind) {
    case SoundKind::Pa:
      return master * set.paVolume * cabinHearing(set, s).muffled;
    case SoundKind::Music:
      return master * set.musicVolume * cabinHearing(set, s).muffled;
    default:
      return 0.0f;
  }
}

}  // namespace xn
