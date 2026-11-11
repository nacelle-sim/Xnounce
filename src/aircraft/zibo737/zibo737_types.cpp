/// @file
/// Implements `src/aircraft/zibo737/zibo737_types.h`; the interface is
/// documented there.
#include "aircraft/zibo737/zibo737_types.h"

#include "core/strings.h"

namespace xn::zibo737 {

Model modelFrom(std::string_view icao, std::string_view acfFile) {
  const std::string i = toUpper(trim(icao));
  if (i == "B736") return Model::B736;
  if (i == "B737") return Model::B737;
  if (i == "B738") return Model::B738;
  if (i == "B739")
    return icontains(acfFile, "9E") ? Model::B739ER : Model::B739;
  return Model::Unknown;
}

const char* modelName(Model m) {
  switch (m) {
    case Model::B736:
      return "737-600";
    case Model::B737:
      return "737-700";
    case Model::B738:
      return "737-800";
    case Model::B739:
      return "737-900";
    case Model::B739ER:
      return "737-900ER";
    case Model::Unknown:
      break;
  }
  return "";
}

const CockpitTerms& terms() {
  static const CockpitTerms t = [] {
    CockpitTerms c;
    c.fms = "FMC";
    c.answerInterphone = "select the cabin interphone on the audio panel";
    c.interphoneAnswered = "interphone answered";
    c.cabinCallButtons = "the overhead ATTEND button";
    c.leversAtTakeoff = "thrust levers at takeoff thrust";
    c.takeoffPowerCue = "TO/GA engaged";
    c.touchdownCue = "the air/ground sensor";
    return c;
  }();
  return t;
}

AircraftType makeType(Model m, const Options& o, int cabinSeats) {
  AircraftType t;
  t.terms = terms();
  t.lead = o.b738;
  switch (m) {
    case Model::B736:
      t.id = "b736";
      t.tags = {"B736"};
      t.typeFactor = 0.7f;
      t.lead = o.b736;
      break;
    case Model::B737:
      t.id = "b737";
      t.tags = {"B737"};
      t.typeFactor = 0.8f;
      t.lead = o.b737;
      break;
    case Model::B738:
    case Model::Unknown:
      t.id = "b738";
      t.tags = {"B738"};
      t.typeFactor = 1.0f;
      break;
    case Model::B739:
    case Model::B739ER:
      t.id = m == Model::B739 ? "b739" : "b739er";
      t.tags = {"B739"};
      t.typeFactor = m == Model::B739 ? 1.1f : 1.15f;
      t.lead = o.b739;
      break;
  }
  t.designators = t.tags;
  t.label = modelName(m == Model::Unknown ? Model::B738 : m);
  t.specific = t.tags.back();
  if (cabinSeats > 0) t.lead.seats = cabinSeats;
  return t;
}

}  // namespace xn::zibo737
