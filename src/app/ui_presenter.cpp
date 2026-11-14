/// @file
/// Implements `src/app/ui_presenter.h`; the interface is documented there.
#include "app/ui_presenter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

#include "audio/hearing.h"
#include "core/strings.h"

namespace xn {

namespace {

const char* onOff(bool b) { return b ? "on" : "off"; }

std::string fmt(const char* f, double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), f, v);
  return buf;
}

/// Adds the aircraft's rows to the Live inputs table, each below the row it
/// names, or at the end when that row isn't there.
void insertAircraftRows(std::vector<std::pair<std::string, std::string>>& rows,
                        const std::vector<LiveRow>& extra) {
  for (const LiveRow& r : extra) {
    auto at = std::find_if(rows.begin(), rows.end(), [&](const auto& row) {
      return row.first == r.after;
    });
    if (at != rows.end()) ++at;
    rows.insert(at, {r.label, r.value});
  }
}

}  // namespace

void fillFlight(ui::UiModel::Flight& f, const FlightPlanService& plans,
                const AirlineTable& airlines, const SimState& sim,
                const Settings& settings, const std::string& boardingNote) {
  f = {};
  if (!plans.installed()) {
    f.reason =
        "simbrief_hub is not installed: boarding starts when a door has been "
        "open at the gate.";
    return;
  }
  const std::optional<Ofp>& ofp = plans.ofp();
  if (!ofp) {
    f.reason = plans.noPlanReason().empty()
                   ? std::string("No OFP in simbrief_hub yet.")
                   : plans.noPlanReason();
    return;
  }
  f.callsign = ofp->callsign;
  f.origin = ofp->origin;
  f.destination = ofp->destination;
  f.etd = zuluLabel(secondOfDay(static_cast<double>(ofp->estOut)));
  // The airline name comes from the callsign. The route bar uses the
  // positions of this plan's airports, so it is shown only while the plan
  // passes its check or is the plan being flown.
  if (f.callsign.size() >= 3 &&
      std::isalpha(static_cast<unsigned char>(f.callsign[0])) &&
      std::isalpha(static_cast<unsigned char>(f.callsign[1])) &&
      std::isalpha(static_cast<unsigned char>(f.callsign[2])))
    f.airline = airlines.nameFor(f.callsign.substr(0, 3));
  const OfpTracker& tracker = plans.tracker();
  const OfpPlace& origin = plans.origin();
  const OfpPlace& dest = plans.destination();
  if ((plans.accepted() || tracker.currentIsLatched()) &&
      plans.placesCurrent() && origin.found && dest.found) {
    const double total = distanceNm(origin.lat, origin.lon, dest.lat, dest.lon);
    const double toGo = distanceNm(sim.lat, sim.lon, dest.lat, dest.lon);
    if (total > 1.0) {
      f.toGoNm = toGo;
      f.routeFrac =
          static_cast<float>(std::clamp(1.0 - toGo / total, 0.0, 1.0));
    }
  }
  if (tracker.latched() && !plans.accepted()) {
    f.state = 1;
    f.line = tracker.status();
    return;
  }
  if (!plans.accepted()) {
    f.state = 2;
    f.reason = "Not used here: " +
               (plans.rejection().empty() ? std::string("not checked yet")
                                          : plans.rejection()) +
               ". Boarding starts when a door has been open at the gate.";
    return;
  }
  f.state = 1;
  const BoardingPlan& plan = plans.plan();
  if (plan.valid) {
    f.etd = zuluLabel(plan.etdSec);
    f.boarding = zuluLabel(plan.boardingAtSec);
  }
  if (settings.boarding() != BoardingStart::Schedule) {
    f.line = settings.boarding() == BoardingStart::Doors
                 ? "Not used for boarding (Boarding starts: Doors open)"
                 : "Not used for boarding (Boarding starts: Cabin call only)";
  } else if (plan.valid && sim.zuluSec >= 0 && boardingNote.empty()) {
    const double until = secondsUntil(plan.boardingAtSec, sim.zuluSec);
    if (until > 0) {
      char buf[48];
      std::snprintf(buf, sizeof(buf), "Boarding in %d:%02d",
                    static_cast<int>(until) / 60, static_cast<int>(until) % 60);
      f.line = buf;
    }
  }
}

void presentActive(ui::UiModel& m, const ActiveView& v) {
  const SimState& s = v.sim;
  const Settings& set = v.settings;
  const Purser* purser = v.purser;
  const CockpitTerms& terms = v.type.terms;

  m.loadedAircraft.clear();
  m.aircraft = v.aircraftName + " " + v.type.label;
  m.livery = v.liveryName.empty() ? "(default livery)" : v.liveryName;
  m.pack = v.packs.pack() ? v.packs.pack()->name
                          : (v.packs.library() ? "none" : "scanning...");
  m.packMissing = v.packs.pack() ? missingList(*v.packs.pack()) : std::string();
  const std::vector<std::string> customTags =
      v.packs.customTags(s.satcom, set.satcomTags);
  m.customTags = join(customTags, ", ");
  // "pull CAB on the ACP" -> "Pull CAB on the ACP to answer"
  m.answerHint = terms.answerInterphone + " to answer";
  if (!m.answerHint.empty())
    m.answerHint[0] = static_cast<char>(
        std::toupper(static_cast<unsigned char>(m.answerHint[0])));
  fillFlight(m.flight, v.plans, v.packs.airlines(), s, set,
             purser ? purser->boardingNote() : std::string());

  const bool estimated = purser && purser->phaseEstimated();
  const Hearing heard = cabinHearing(set, s);
  m.liveInputs = {
      {terms.fms + " phase",
       s.fmsPhase ? std::string(phaseName(*s.fmsPhase)) + " (" +
                        std::to_string(s.fmsPhaseRaw) + ")"
                  : "unknown (" + std::to_string(s.fmsPhaseRaw) + ")"},
      {"Purser phase", purser ? std::string(phaseName(purser->phase())) +
                                    (estimated ? " (estimated: " + terms.fms +
                                                     " not initialized)"
                                               : std::string())
                              : "-"},
      {"Flight number", s.flightNumber.empty() ? "-" : s.flightNumber},
      {"On ground", s.onGround ? "yes" : "no"},
      {"Speed / height",
       fmt("%.0f kt GS", s.gsKt) + ", " + fmt("%.0f ft AGL", s.aglFt)},
      {"Beacon / seatbelts",
       std::string(onOff(s.beacon)) + " / " + onOff(s.seatbelt)},
      {"Engines running", std::to_string(s.enginesRunning) + " of " +
                              std::to_string(s.engineCount)},
      {"Passenger doors", fmt("most open %.0f%%", s.doorMax * 100) +
                              fmt(", L1 %.0f%%", s.doorFwd * 100)},
      {"Slides",
       s.slidesKnown ? (s.slidesArmed ? "armed" : "disarmed") : "unknown"},
      {"Passengers", std::to_string(s.pax)},
      {"AC power", s.acPower ? "yes" : "no"},
      {"Cockpit door", fmt("%.0f%% open", s.cockpitDoor * 100) +
                           (s.doorBlend >= 0 ? " (" + v.aircraftName +
                                                   fmt(" sound blend %.0f%%)",
                                                       s.doorBlend * 100)
                                             : std::string())},
      {"Cabin PA heard",
       fmt("clear %.0f%%", heard.clear * 100) +
           (set.muffle && set.realisticHearing
                ? fmt(", through the door %.0f%%", heard.muffled * 100)
                : std::string())},
      {"Zulu time", s.zuluSec >= 0 ? zuluLabel(s.zuluSec) : std::string("-")},
      {"Camera",
       (s.viewExternal ? std::string("external")
                       : (s.cameraInCabin ? "in the cabin" : "cockpit")) +
           (s.cameraKnown && !s.viewExternal
                ? fmt(", %.1f m aft of the eye", s.cameraAftM)
                : std::string())},
      {"Local time",
       fmt("%02.0f:", std::floor(s.localTimeSec / 3600)) +
           fmt("%02.0f", std::fmod(std::floor(s.localTimeSec / 60), 60)) +
           std::string(" (") + timeOfDayName(timeOfDayAt(s.localTimeSec)) +
           ")"},
      {"Sun elevation", fmt("%.1f deg", s.sunPitchDeg)},
      {"Recorder", v.recorderPath.empty() ? "off" : v.recorderPath},
  };
  insertAircraftRows(m.liveInputs, v.aircraftRows);
  m.crewActions.clear();
  for (const ActionStatus& a : v.actions)
    m.crewActions.push_back({a.name, a.on, a.available, a.note});
  m.crewNote = v.announcementNote;
  m.checks.clear();
  for (const CheckRow& c : v.checks)
    m.checks.push_back({c.label, c.reading, c.result, c.available});
  if (!purser) return;

  m.liveInputs.insert(
      m.liveInputs.begin(),
      {{"Variant", std::to_string(v.packs.variant())},
       {"Time-of-day tag", timeOfDayName(timeOfDayAt(s.localTimeSec))},
       {"Refuelling", v.refueling ? "yes" : "no"},
       {"Overwater", v.overwater ? "yes" : "no"}});
  m.stage = purser->stageLabel();
  m.calling = purser->calling();
  m.callAttempt = purser->callAttempt();
  m.boardingNote = purser->boardingNote();
  m.boardingFrac = -1;
  m.boardingLeftSec = -1;
  const double from = purser->boardingStartedAt(),
               until = purser->boardingEndsAt();
  const BoardingPlan& plan = v.plans.plan();
  if (from >= 0 && until > from && until < 1e11) {
    m.boardingFrac =
        static_cast<float>(std::clamp((s.t - from) / (until - from), 0.0, 1.0));
    m.boardingLeftSec = std::max(0.0, until - s.t);
    m.boardingPax = plan.valid && plan.pax > 0 ? plan.pax : s.pax;
  } else if (from >= 0 && plan.valid && plan.pax > 0) {
    // The aircraft's turnaround service boards (VP_SPEC § 4.1c): progress by
    // the passengers on board, no time left to tell.
    m.boardingFrac = static_cast<float>(
        std::clamp(static_cast<double>(s.pax) / plan.pax, 0.0, 1.0));
    m.boardingPax = plan.pax;
  }
  if (const auto cur = purser->current()) {
    m.playing = annLabel(cur->first, cur->second);
    m.playingFenix = annFenixLabel(cur->first, cur->second);
  } else {
    m.playing.clear();
    m.playingFenix.clear();
  }
  m.pending.clear();
  for (const PendingItem& p : purser->pending()) {
    ui::UiModel::Pending row;
    row.name = p.type ? annLabel(*p.type, p.context) : p.what;
    row.note = p.note;
    row.fenix = p.type ? annFenixLabel(*p.type, p.context) : std::string();
    row.inSec = p.inSec;
    row.condition = p.condition;
    m.pending.push_back(std::move(row));
  }
  m.now.file = v.now.file;
  m.now.pos = v.now.pos;
  m.now.dur = v.now.dur;
  m.now.decoding = v.now.decoding;
  m.now.paused = v.now.paused;
  m.now.interphone = v.now.interphone;
  m.music = v.now.music;
  m.events.clear();
  const auto& tl = purser->timeline();
  for (auto r = tl.rbegin(); r != tl.rend() && m.events.size() < 200; ++r) {
    ui::UiModel::EventRow e;
    e.zuluSec = r->zuluSec;
    e.localSec = r->localSec;
    e.name = annLabel(r->type, r->context);
    e.fenix = annFenixLabel(r->type, r->context);
    e.outcome = outcomeName(r->outcome);
    e.detail = r->detail;
    // 0 played, 1 a problem, 2 neutral.
    e.tone =
        (r->outcome == Outcome::Played || r->outcome == Outcome::Playing) ? 0
        : (r->outcome == Outcome::NoSound ||
           r->outcome == Outcome::Unanswered || r->outcome == Outcome::Expired)
            ? 1
            : 2;
    m.events.push_back(std::move(e));
  }
}

void presentDormant(ui::UiModel& m, const LoadedAircraft& d,
                    const std::string& cardNote) {
  std::string name = !d.uiName.empty() ? d.uiName : d.acfIcao;
  if (!d.acfFile.empty())
    name += name.empty() ? d.acfFile : " (" + d.acfFile + ")";
  name += cardNote;
  m.loadedAircraft = name;
  m.aircraft.clear();
  m.livery.clear();
  m.pack.clear();
  m.packMissing.clear();
  m.customTags.clear();
  m.flight = {};
  m.liveInputs.clear();
  m.crewActions.clear();
  m.crewNote.clear();
  m.checks.clear();
  m.stage.clear();
  m.calling = false;
  m.boardingNote.clear();
  m.boardingFrac = -1;
  m.playing.clear();
  m.playingFenix.clear();
  m.pending.clear();
  m.now = {};
  m.music.clear();
  m.events.clear();
}

std::vector<std::string> explainSelection(const Pack* pack, int annType,
                                          int callContext, SelectContext c) {
  std::vector<std::string> lines;
  if (!pack) return {"No active pack."};
  if (annType < 0 || annType >= kAnnTypeCount) return lines;
  c.context = static_cast<CallContext>(std::clamp(callContext, 0, 2));
  Rng rng(1);
  const Selection sel =
      selectSound(*pack, static_cast<AnnType>(annType), c, rng);
  lines.push_back(
      "Pack " + pack->name + ", " + c.aircraft.label + " [" +
      join(c.aircraft.tags, ",") + "], " + timeOfDayName(c.time) + ", phase " +
      phaseName(c.phase) + ", variant " + std::to_string(c.variant) +
      (c.refueling ? ", refuelling" : "") + (c.overwater ? ", overwater" : "") +
      (c.customTags.empty() ? "" : ", tags " + join(c.customTags, ",")));
  if (sel.file)
    lines.push_back(
        "-> plays " + sel.file->relPath + fmt(" (score %.1f)", sel.score) +
        (sel.variantFallback ? ", variant " + std::to_string(sel.variant) +
                                   " (nothing fits in variant " +
                                   std::to_string(c.variant) + ")"
                             : std::string()) +
        (sel.timeFallback
             ? std::string(", nearest time of day ") + timeOfDayName(sel.time) +
                   " (nothing for " + timeOfDayName(c.time) + ")"
             : std::string()));
  else
    lines.push_back("-> nothing eligible: the announcement is skipped");
  for (const Candidate& cd : sel.candidates)
    lines.push_back(std::string(cd.eligible ? "  ok   " : "  no   ") +
                    cd.file->relPath + ": " + cd.why);
  if (sel.candidates.empty())
    lines.push_back("  (the pack has no file of this type)");
  return lines;
}

}  // namespace xn
