/// @file
/// The Fenix vocabulary: announcement types, modes, legacy names, flight
/// phases and times of day (VP_SPEC § 1.3, § 2, § 3.1).
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace xn {

enum class AnnType {
  BoardingMusic,
  BoardingWelcome,
  BoardingComplete,
  ArmDoors,
  DisarmDoors,
  PreSafetyBriefing,
  SafetyBriefing,
  CrewTakeSeats,
  CabinDim,
  CabinSecured,
  AfterTakeoff,
  SeatbeltSignOn,
  SeatbeltSignOff,
  CabinService,
  DescentStarted,
  AfterLanding,
  DeboardingStarted,
  DeboardingComplete,
  Decompression,
  Count
};
constexpr int kAnnTypeCount = static_cast<int>(AnnType::Count);

enum class Mode { Manual, Prerecorded, Interphone };
/// Which of two calls a legacy file name is for (`CrewSeatsTakeoff` versus
/// `CrewSeatsLanding`). Fenix names carry no context: Any.
enum class CallContext { Any, Takeoff, Landing };
enum class Phase {
  PreFlight,
  Takeoff,
  Climb,
  Cruise,
  Descent,
  Approach,
  GA,
  Done
};
enum class TimeOfDay { Morning, Afternoon, Evening, Night };

const char* annTypeName(AnnType t);
const char* modeName(Mode m);
const char* contextName(CallContext c);
const char* phaseName(Phase p);
const char* timeOfDayName(TimeOfDay t);

Mode defaultMode(AnnType t);

/// The name shown in the window, in plain English. The Fenix name is the
/// tooltip and the file name.
const char* annTypeLabel(AnnType t);
std::string annLabel(AnnType t, CallContext c);
std::string annFenixLabel(AnnType t, CallContext c);
/// Where an announcement is heard: cabin PA, interphone or music (VP_SPEC
/// § 2).
const char* heardOn(AnnType t, Mode m);

struct TypeName {
  AnnType type;
  CallContext context = CallContext::Any;
  bool legacy = false;
};
/// The type a file name starts with, without regard to case, legacy aliases
/// included.
std::optional<TypeName> resolveTypeName(std::string_view name);
/// For a name other tools use that isn't a Fenix type, the Fenix type to
/// rename the file to (`BoardingStarted` -> `BoardingWelcome`), else null.
const char* fenixRenameHint(std::string_view name);

std::optional<Phase> phaseFromTag(std::string_view tag);
std::optional<TimeOfDay> timeOfDayFromTag(std::string_view tag);
std::optional<Mode> modeFromTag(std::string_view tag);
/// The time of day at a local time: 06-12 Morning, 12-18 Afternoon, 18-24
/// Evening, else Night.
TimeOfDay timeOfDayAt(double localSecondsSinceMidnight);

}  // namespace xn
