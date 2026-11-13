/// @file
/// The Virtual Purser: decides when each announcement plays (VP_SPEC § 4).
///
/// The purser reads one `SimState` per tick and tells a `PurserOutput` what to
/// play. It never touches X-Plane or the audio system itself, so the tests and
/// `xn_replay` run whole flights through it with a pretend player. The
/// implementation is split by stage of the flight: boarding.cpp,
/// departure.cpp, signals.cpp, flight.cpp, arrival.cpp, queue.cpp, labels.cpp.
#pragma once

#include <algorithm>
#include <deque>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "core/aircraft_type.h"
#include "core/announcement.h"
#include "core/settings.h"
#include "core/sim_state.h"
#include "core/simbrief.h"

namespace xn {

/// What the purser asks of the audio side. The plugin's `Player` implements it;
/// the tests use a stand-in (`replay::StoryPlayer`).
class PurserOutput {
 public:
  virtual ~PurserOutput() = default;
  /// Starts an announcement. Returns the label of the file that plays, or
  /// nothing when the pack has no eligible file (as in Fenix, the
  /// announcement is then skipped).
  virtual std::optional<std::string> play(AnnType type, CallContext ctx,
                                          bool interphone) = 0;
  virtual bool playing() const = 0;
  virtual void stop() = 0;
  /// Sounds the cockpit call buzzer (VP_SPEC § 4.3).
  virtual void chime() = 0;
  /// Boarding music on or off. The player pauses the music under
  /// announcements by itself.
  virtual void setMusic(bool on) = 0;
  /// The mode of the file that would play, or nothing when no file is
  /// eligible. A file in Interphone mode goes through the cabin call.
  virtual std::optional<Mode> peek(AnnType type, CallContext ctx) const = 0;
};

/// What became of an announcement, as the timeline shows it.
enum class Outcome {
  Queued,
  Playing,
  Played,
  Skipped,
  NoSound,
  Expired,
  Disabled,
  Stopped,
  Unanswered
};
/// The outcome in words: "played", "no sound in pack"...
const char* outcomeName(Outcome o);

/// One announcement on the timeline.
struct TimelineRow {
  double t = 0;
  AnnType type = AnnType::BoardingWelcome;
  CallContext context = CallContext::Any;
  Outcome outcome = Outcome::Queued;
  std::string detail;   ///< The file that played, or why it didn't.
  double zuluSec = -1;  ///< UTC seconds since midnight, or -1 when unknown.
  double localSec = -1;
};

/// An entry of the "coming up" list: an announcement, or a milestone such as
/// "Boarding at 12:39Z".
struct PendingItem {
  std::optional<AnnType> type;
  CallContext context = CallContext::Any;
  std::string what;
  std::string note;
  double inSec = -1;  ///< Seconds until it is due, or -1 when it waits for
                      ///< `condition`.
  std::string condition;
};

/// How far the cabin crew's work has come this flight. The cabin crew's
/// actions in the aircraft follow it (VP_SPEC § 4.7); the purser itself
/// never reads it.
struct CabinStage {
  bool boarding = false;          ///< Boarding has started.
  bool boardingComplete = false;  ///< BoardingComplete was called.
  bool doorsArmed = false;        ///< ArmDoors was called.
  /// The cabin check before takeoff is done: CabinSecured (takeoff) follows.
  bool securedForTakeoff = false;
  /// CrewTakeSeats (takeoff) was called: the line-up signal came.
  bool seatedForTakeoff = false;
  bool takeoff = false;       ///< Takeoff thrust was set.
  bool afterTakeoff = false;  ///< AfterTakeoff was called.
  /// When the cabin service began (CabinService was called), on the purser
  /// clock, or -1.
  double cabinServiceAt = -1;
  bool descentStarted = false;  ///< DescentStarted was called.
  /// The cabin check before landing is done: CabinSecured (landing) follows.
  bool securedForLanding = false;
  /// CrewTakeSeats (landing) was called.
  bool seatedForLanding = false;
  bool touchdown = false;      ///< The aircraft has touched down.
  bool afterLanding = false;   ///< AfterLanding was called.
  bool doorsDisarmed = false;  ///< DisarmDoors was called.
  double disarmedAt = -1;      ///< When, on the purser clock, or -1.
  bool deboarding = false;     ///< DeboardingStarted was called.
  bool deboardingComplete = false;
  bool flightComplete = false;  ///< Nothing more happens on this flight.
};

/// The purser of one aircraft. Main thread only.
class Purser {
 public:
  /// `out` must outlive the purser. `seed` fixes the random timings, for
  /// tests.
  explicit Purser(PurserOutput& out, unsigned seed = std::random_device{}());

  /// The aircraft being flown: its type factor and cockpit wording. Set it
  /// before every tick.
  void setAircraft(const AircraftType& t) { type_ = t; }
  /// The SimBrief boarding schedule (VP_SPEC § 4.1a). An invalid plan means
  /// the doors rule.
  void setBoardingPlan(const BoardingPlan& p) { plan_ = p; }
  /// One tick. `s.t` is the purser clock, which stops while the sim is
  /// paused.
  void tick(const SimState& s, const Settings& set);

  /// Starts over. The next ticks work out how far the flight already is
  /// (a start mid-flight, a teleport; VP_SPEC § 4.4).
  void reset();
  /// Stops the announcement that is playing.
  void skip();
  /// Fires the next pending event now, or stands in for what it waits for.
  void advance();
  /// A cabin call button was pressed. Depending on the stage it starts or
  /// ends boarding, hurries a cabin check, is the line-up signal, or tells
  /// the crew they may leave their seats (VP_SPEC § 4.3a). Applied on the
  /// next tick.
  void cabinCall() { callPressed_ = true; }

  /// The Airline Pack may no longer change: from ArmDoors until the next
  /// flight (VP_SPEC § 1.4).
  bool packLocked() const { return f_.armDoors; }
  /// The aircraft has taken off. From here on the OFP of this flight is kept
  /// (VP_SPEC § 4.2a).
  bool tookOff() const { return f_.takeoff || f_.flown; }
  /// Deboarding is complete and the turnaround is off (VP_SPEC § 4.4).
  bool flightComplete() const { return f_.flightComplete; }
  /// How far the cabin crew's work has come, for the cabin actions.
  CabinStage cabinStage() const;
  /// The flight phase the purser works with.
  Phase phase() const { return phase_; }
  /// The phase is estimated because the FMS gives none (D-20).
  bool phaseEstimated() const { return estimating_; }
  /// The stage in words, for the Status tab: "Boarding", "Cruise"...
  std::string stageLabel() const;
  /// The first entry of the "coming up" list, in Fenix names.
  std::string nextLabel() const;
  /// What plays now, in Fenix names.
  std::string nowLabel() const;
  /// The announcement playing now, if any.
  std::optional<std::pair<AnnType, CallContext>> current() const {
    if (!current_) return std::nullopt;
    return std::make_pair(current_->type, current_->ctx);
  }
  /// The try of the cabin's interphone call, from 1.
  int callAttempt() const { return call_.attempt + 1; }
  /// The announcements of this session, oldest first.
  const std::deque<TimelineRow>& timeline() const { return timeline_; }
  /// The cabin is calling the cockpit and nobody has answered yet.
  bool calling() const { return call_.active && !call_.talking; }
  /// The next queued announcement, so the player can decode it early.
  std::optional<std::pair<AnnType, CallContext>> nextQueued() const {
    if (queue_.empty()) return std::nullopt;
    return std::make_pair(queue_.front().type, queue_.front().ctx);
  }
  /// The "coming up" list: timed entries first, then the ones that wait for
  /// a condition.
  std::vector<PendingItem> pending() const;
  /// The same list as text in Fenix names, for the log and the tests.
  std::vector<std::string> pendingLabels() const;
  /// A warning for the Status tab: boarding was shortened, X-Plane's clock
  /// doesn't match the OFP, or the flight started with engines running.
  const std::string& boardingNote() const { return boardingNote_; }
  /// When the modeled boarding ends and when it started, on the purser clock
  /// (VP_SPEC § 4.1b); -1 when not boarding.
  double boardingEndsAt() const {
    return f_.boarding && !f_.boardingComplete ? f_.boardingEndAt : -1;
  }
  double boardingStartedAt() const {
    return f_.boarding && !f_.boardingComplete ? f_.boardingStartAt : -1;
  }
  /// Receives every new or changed timeline row (for the flight trace).
  void setEventSink(std::function<void(const TimelineRow&)> sink) {
    sink_ = std::move(sink);
  }
  /// Receives the purser's other decisions with their time: boarding
  /// started, which signal meant takeoff thrust, touchdown, the phase
  /// estimate... (for the flight trace).
  void setNoteSink(std::function<void(double, const std::string&)> sink) {
    noteSink_ = std::move(sink);
  }

 private:
  struct Item {
    AnnType type;
    CallContext ctx = CallContext::Any;
    int expire = 0;  // a combination of the Expire... bits below
    double notBefore = 0;
  };
  enum : int {
    ExpireAtTakeoff = 1,
    ExpireLandingWindow = 2,
    ExpireAirborne = 4,
    ExpireOnGround = 8
  };
  // What the SimBrief schedule decided (VP_SPEC § 4.1a): waiting for the
  // boarding time, or the doors rule.
  enum class Sched { Unknown, Waiting, Doors };

  struct Flags {
    bool inferred = false;
    double settleFrom = -1;  // when the state started to settle (§ 4.4)
    // Started parked with engines running: no boarding until they are shut
    // down at the gate.
    bool hotStart = false;
    // From the gate to takeoff.
    double armWaitSince = -1;
    bool boarding = false, boardingComplete = false, armDoors = false,
         armDoorsFinished = false;
    // The modeled boarding (§ 4.1b).
    double boardingNatural = 0, boardingStartAt = -1, boardingEndAt = -1,
           boardingStartZulu = -1;
    int boardingPlanSeqno = -1;
    // The doors rule (§ 4.1a).
    bool doorSeenOpenAtGate = false;
    double readySince = -1;
    double armDoorsAt = -1, nextWelcome = -1;
    bool safetyQueued = false, safetyFinished = false;
    // D-22: the first movement after boarding and the random delay from it
    // to the briefing.
    double movedAt = -1, safetyAfterMove = 30;
    bool taxied = false;
    double takeoffCheckDone = -1;
    // D-23: the cabin secured and the line-up signal.
    bool takeoffSecuredQueued = false, lineup = false;
    bool takeoffCallsQueued = false;
    bool takeoff = false, flown = false;
    double airborneSince = -1;
    // In flight.
    double afterTakeoffDue = -1;
    bool afterTakeoff = false, climbEntered = false;
    double cabinServiceDue = -1;
    bool cruiseEntered = false, cabinService = false;
    // When the cabin service began, or -1 when it was skipped or is to come.
    double cabinServiceAt = -1;
    double descentDue = -1, descentAt = -1;
    bool descentEntered = false, descentStarted = false,
         descentFinished = false;
    double landingCheckDone = -1;
    // D-24: CabinSecured in the approach window; seats and dim on a NO
    // SMOKING flick or the gear down.
    bool landingSecuredQueued = false, seatsLanding = false;
    bool landingCallsQueued = false;
    bool decompression = false;
    // From touchdown on.
    double touchdownAt = -1;
    bool afterLanding = false, disarm = false;
    double disarmAt = -1, deboardAt = -1, deboardDone = -1;
    bool deboarding = false, deboardComplete = false;
    double turnaroundAt = -1;
    bool gapSawClosed = false;  // a door closed: opening it again ends the gap
    // D-26: with the turnaround off, a new OFP starts the next flight.
    bool flightComplete = false;
    int flightSeqno = -1;
    // The seatbelt sign must hold its new state a few seconds.
    bool beltState = false;
    double beltChangedAt = -1;
    bool beltPending = false;
    bool slidesWere = false;
    bool forceBoardingComplete = false, forceLandingCalls = false;
    bool expediteTakeoffCheck = false, expediteLandingCheck = false;
    // The SimBrief plan the schedule last evaluated, and what it decided.
    Sched sched = Sched::Unknown;
    int planSeqno = -1;
    double planAt = -1;
    // The boarding time was further ahead than max_wait_min: X-Plane's clock
    // isn't on the flight's time, so the doors rule applies.
    bool clockMismatch = false;
    // The NO SMOKING flick (§ 4.2) and the phase estimate (D-20).
    int nsLast = -1;
    double nsOffAt = -1;
    bool fmsLeftPreflight = false;
    bool takeoffPowerSeen = false;
    double liftoffAt = -1;
    bool estimateLogged = false;
    double levelSince = -1;
  };

  struct Call {
    bool active = false;
    Item item{};
    int attempt = 0;
    double nextChime = 0;
    bool talking = false;
    bool forceAnswer = false;
  };

  Phase estimatePhase(const SimState& s);
  bool estimateWanted(const SimState& s, const Settings& set) const;
  void scheduleBoarding(const SimState& s, const Settings& set, bool atGate);
  void doorsBoarding(const SimState& s, const Settings& set, bool atGate);
  void infer(const SimState& s);
  void departure(const SimState& s, const Settings& set);
  void flight(const SimState& s, const Settings& set);
  void arrival(const SimState& s, const Settings& set);
  void runQueue(const SimState& s, const Settings& set);
  void runCall(const SimState& s, const Settings& set);
  void enqueue(AnnType t, CallContext c = CallContext::Any, int expire = 0,
               double notBefore = 0);
  void dropWhere(int expireMask, Outcome why);
  bool expired(const Item& it, const SimState& s, const Settings& set) const;
  void log(double t, AnnType type, CallContext c, Outcome o,
           std::string detail = {});
  void finishCurrent(Outcome o, std::string detail = {});
  void onFinished(const Item& it);
  void applyAdvance(const SimState& s, const Settings& set);
  void updateMusic(const SimState& s);
  void startBoarding(const SimState& s, const Settings& set, const char* why);
  double boardingEnd(const SimState& s, const Settings& set);
  // Notices the user setting X-Plane's clock before boarding (§ 4.1a).
  void watchClock(const SimState& s);
  void onCabinCall(const SimState& s, const Settings& set);
  // True on the tick a NO SMOKING flick completes: lit, unlit, lit within
  // ns_flick_max_s.
  bool detectFlick(const SimState& s, const Settings& set);
  // The pilots' signals (D-23, D-24, D-25).
  void signalLineup(const SimState& s, const Settings& set,
                    const std::string& why);
  void signalSeatsLanding(const SimState& s, const Settings& set,
                          const std::string& why);
  void signalAfterTakeoff(const SimState& s, const std::string& why);
  // When the safety briefing is due: after the first movement, and never too
  // soon after ArmDoors (D-22).
  double safetyDue() const {
    return std::max(f_.movedAt + f_.safetyAfterMove,
                    f_.armDoorsAt + preSafetyDelay_);
  }
  // A new flight at the gate: a new OFP with the turnaround off, or the
  // engines shut down after a hot start.
  void freshFlight(const SimState& s);
  double checkDuration(int pax, const Settings& set);
  double randIn(double lo, double hi);
  bool dark(const SimState& s, const Settings& set) const {
    return s.sunPitchDeg < set.darkSunDeg;
  }

  PurserOutput& out_;
  std::mt19937 rng_;
  AircraftType type_;
  const CockpitTerms& terms() const { return type_.terms; }
  Flags f_;
  Phase phase_ = Phase::PreFlight;
  Phase prevPhase_ = Phase::PreFlight;
  std::deque<Item> queue_;
  std::optional<Item> current_;
  double gapUntil_ = 0;
  double now_ = 0;
  Call call_;
  bool skipRequested_ = false, advanceRequested_ = false, callPressed_ = false;
  BoardingStart mode_ = BoardingStart::Schedule;
  BoardingPlan plan_;
  bool estimating_ = false;
  double zulu_ = -1;
  double local_ = -1;
  double lastZulu_ = -1, lastZuluT_ = -1;
  std::string boardingNote_;
  float doorSettle_ = 15;
  std::deque<TimelineRow> timeline_;
  std::function<void(const TimelineRow&)> sink_;
  std::function<void(double, const std::string&)> noteSink_;
  void note(double t, const std::string& text);
  bool music_ = false;
  /// When the AC power went off, or -1 while it is on. The music stops only
  /// after 3 s without power (D-31): a power transfer only drops it out.
  double acLostAt_ = -1;
  float preSafetyDelay_ = 60;
  bool flick_ = false;
  /// The queue is waiting for the aircraft's own announcement (§ 4.7).
  bool aircraftPaHeld_ = false;
};

}  // namespace xn
