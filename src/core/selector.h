/// @file
/// Choosing the file that plays, by the Fenix tag rules (VP_SPEC § 3.2–3.4).
#pragma once

#include <random>
#include <string>
#include <vector>

#include "core/aircraft_type.h"
#include "core/pack_library.h"

namespace xn {

using Rng = std::mt19937;

struct SelectContext {
  AircraftType aircraft;
  bool refueling = false;
  TimeOfDay time = TimeOfDay::Morning;
  double localHour = -1;  ///< 0–24, for nearness (D-28); below 0: the middle
                          ///< of `time`.
  Phase phase = Phase::PreFlight;
  bool overwater = false;
  std::vector<std::string> customTags;  ///< The livery's and the antenna's.
  int variant = 0;
  CallContext context = CallContext::Any;
};

struct Candidate {
  const SoundFile* file = nullptr;
  bool eligible = false;
  bool variantPool = false;  ///< In the variant used for this call.
  float score = 0;
  std::string why;  ///< For the explainer, as "A320 ok, Evening ok".
};

struct Selection {
  const SoundFile* file = nullptr;  ///< Null: nothing fits; skip it.
  float score = 0;
  int variant = 0;               ///< The variant the file came from.
  bool variantFallback = false;  ///< The session variant had nothing that
                                 ///< fits.
  TimeOfDay time = TimeOfDay::Morning;  ///< The time of day the choice used.
  bool timeFallback = false;  ///< Nothing for the time of day; the nearest
                              ///< was used (D-28).
  std::vector<Candidate> candidates;  ///< Every file of the type, for the
                                      ///< explainer.
};

/// The other times of day, nearest first (D-28).
std::vector<TimeOfDay> nearestTimesOfDay(TimeOfDay now, double localHour);

/// Chooses the file for an announcement. On a tie the file played last
/// (`lastPath`) is avoided.
Selection selectSound(const Pack& pack, AnnType type, const SelectContext& ctx,
                      Rng& rng, const std::string& lastPath = {});

/// Picks the session variant at random from the variants the pack has
/// (VP_SPEC § 3.4, D-27).
int pickVariant(const Pack& pack, Rng& rng);

}  // namespace xn
