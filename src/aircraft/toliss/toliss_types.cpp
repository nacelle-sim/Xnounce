#include "aircraft/toliss/toliss_types.h"

#include "core/strings.h"

namespace xn::toliss {

const char* familyName(Family f) {
  switch (f) {
    case Family::A319:
      return "A319";
    case Family::A320:
      return "A320";
    case Family::A321:
      return "A321";
    case Family::A330:
      return "A330";
    case Family::A340:
      return "A340";
    default:
      return "unknown";
  }
}

Family familyFrom(std::string_view icao, std::string_view acfFile) {
  const std::string i = toUpper(trim(icao));
  if (i == "A319" || i == "A19N") return Family::A319;
  if (i == "A320" || i == "A20N") return Family::A320;
  if (i == "A321" || i == "A21N") return Family::A321;
  if (i.rfind("A33", 0) == 0) return Family::A330;
  if (i.rfind("A34", 0) == 0) return Family::A340;
  const std::string f = toLower(acfFile);
  if (f.rfind("a319", 0) == 0) return Family::A319;
  if (f.rfind("a320", 0) == 0) return Family::A320;
  if (f.rfind("a321", 0) == 0) return Family::A321;
  if (f.rfind("a330", 0) == 0 || f.rfind("a339", 0) == 0) return Family::A330;
  if (f.rfind("a340", 0) == 0 || f.rfind("a346", 0) == 0) return Family::A340;
  return Family::Unknown;
}

int engineIsNeo(std::string_view code) {
  const std::string c = toUpper(trim(code));
  if (c.empty()) return -1;
  if (c.rfind("PW", 0) == 0 || c.rfind("LEA", 0) == 0) return 1;
  if (c.rfind("CFM", 0) == 0 || c.rfind("IAE", 0) == 0) return 0;
  return -1;
}

const CockpitTerms& terms() {
  static const CockpitTerms t = [] {
    CockpitTerms c;
    c.fms = "FMGS";
    c.answerInterphone = "pull CAB on the ACP";
    c.interphoneAnswered = "CAB answered";
    c.cabinCallButtons = "overhead CALLS: FWD, AFT, ALL...";
    c.leversAtTakeoff = "thrust levers at FLX/TOGA";
    c.takeoffPowerCue = "ECAM flight phase T.O. power";
    c.touchdownCue = "ECAM flight phase";
    return c;
  }();
  return t;
}

AircraftType makeType(Family f, bool neo, const Options& o) {
  AircraftType t;
  t.terms = terms();
  // A type without its own boarding rows uses the A320's.
  t.lead = o.a320;
  switch (f) {
    case Family::A319:
      t.id = "a319";
      t.tags = {"A319"};
      t.designators = {"A319", "A19N"};
      t.typeFactor = 0.9f;
      t.label = "A319";
      t.lead = o.a319;
      break;
    case Family::A320:
      t.id = "a320";
      t.tags = neo ? std::vector<std::string>{"A320", "A20N"}
                   : std::vector<std::string>{"A320"};
      t.designators = {"A320", "A20N"};
      t.typeFactor = 1.0f;
      t.label = neo ? "A320neo" : "A320";
      break;
    case Family::A321:
      t.id = "a321";
      t.tags = neo ? std::vector<std::string>{"A321", "A21N"}
                   : std::vector<std::string>{"A321"};
      t.designators = {"A321", "A21N"};
      t.typeFactor = 1.25f;
      t.label = neo ? "A321neo" : "A321";
      t.lead = o.a321;
      break;
    case Family::A330:
      t.id = "a330";
      t.tags = {"A330", "A339"};
      t.designators = {"A339"};
      t.typeFactor = 1.6f;
      t.label = "A330-900";
      break;
    case Family::A340:
      t.id = "a340";
      t.tags = {"A340", "A346"};
      t.designators = {"A346"};
      t.typeFactor = 1.8f;
      t.label = "A340-600";
      break;
    default:
      t.id = "unknown";
      t.label = "unknown";
      break;
  }
  t.specific = t.tags.empty() ? std::string() : t.tags.back();
  return t;
}

AircraftType currentType(const TypeInputs& in, const Options& o) {
  const std::string icao =
      toUpper(in.liveIcao.empty() ? in.loadedIcao : in.liveIcao);
  int neo = -1;
  if (icao == "A20N" || icao == "A21N" || icao == "A19N")
    neo = 1;
  else if (icao == "A320" || icao == "A321" || icao == "A319")
    neo = 0;
  if (neo < 0 && !in.tlscfgEngine.empty()) neo = engineIsNeo(in.tlscfgEngine);
  if (neo < 0) neo = engineIsNeo(leadingBracketCode(in.liveryName));
  Family fam = familyFrom(icao, in.acfFile);
  if (fam == Family::Unknown) fam = familyFrom(in.loadedIcao, in.acfFile);
  return makeType(fam, neo == 1, o);
}

Recognition classify(const LoadedAircraft& ac, bool markerPresent) {
  Recognition r;
  // The marker alone isn't enough: other Airbus add-ons use the same dataref
  // prefix. ToLiss aircraft are by "Kiwi" and sit in a folder named ToLiss.
  const bool looksToliss =
      icontains(ac.author, "kiwi") || icontains(ac.acfPath, "toliss");
  const Family family = familyFrom(ac.acfIcao, ac.acfFile);
  if (!markerPresent || !looksToliss || family == Family::Unknown) return r;
  r.ours = true;
  r.model = familyName(family);
  r.supported = family == Family::A319 || family == Family::A320 ||
                family == Family::A321;
  if (!r.supported) {
    r.dormantReason = "not a supported ToLiss model";
    r.logLine = "dormant: ToLiss " + ac.acfFile + " (" + r.model +
                ") is not supported yet; this version runs on the A319, A320 "
                "and A321";
    r.cardNote = ": a ToLiss model this version doesn't support yet";
  }
  return r;
}

}  // namespace xn::toliss
