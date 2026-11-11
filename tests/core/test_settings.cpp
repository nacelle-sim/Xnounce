// The shared settings: names, old keys and repairs on loading; and the
// refuelling detector that some of them tune.
#include <doctest/doctest.h>

#include "core/refuel.h"
#include "core/settings.h"

using namespace xn;

TEST_CASE("settings: boarding start names and the 0.1.x key") {
  Settings s;
  CHECK(s.boarding() == BoardingStart::Schedule);
  loadSettings(Ini::parse("[cabin]\nboarding_start = call\n"), s);
  CHECK(s.boarding() == BoardingStart::Call);
  Ini out;
  storeSettings(s, out);
  CHECK(*out.get("cabin", "boarding_start") == "call");

  Settings old1;
  loadSettings(Ini::parse("[cabin]\nboarding_on_call = no\n"), old1);
  CHECK(old1.boarding() == BoardingStart::Doors);
  Settings old2;
  loadSettings(Ini::parse("[cabin]\nboarding_on_call = yes\n"),
               old2);  // 0.1.x default
  CHECK(old2.boarding() == BoardingStart::Schedule);
  Settings both;
  loadSettings(
      Ini::parse("[cabin]\nboarding_on_call = no\nboarding_start = schedule\n"),
      both);
  CHECK(both.boarding() == BoardingStart::Schedule);

  Settings r;
  loadSettings(
      Ini::parse("[timing]\nrefuel_rate_kgs = 40\nrefuel_max_kgs = 10\n"), r);
  CHECK(r.refuelMaxKgs >= r.refuelRateKgs);
}

TEST_CASE("refuel detection ignores instant ISCS loads") {
  Settings set;
  RefuelDetector d;
  SimState s;
  s.onGround = true;
  s.fuelKg = 5000;
  for (int k = 0; k <= 4; ++k) {  // instant 7000 kg
    s.t = k;
    if (k == 2) s.fuelKg = 12000;
    d.update(s, set);
  }
  CHECK(!d.refueling(s.t));
  double detected = -1;
  for (int k = 5; k <= 20; ++k) {  // real refuel 20 kg/s
    s.t = k;
    s.fuelKg += 20;
    const double r = d.update(s, set);
    if (r > 0) detected = r;
  }
  CHECK(detected == doctest::Approx(20).epsilon(0.05));
  CHECK(d.refueling(s.t));
  CHECK(!d.refueling(s.t + 61));
  s.onGround = false;  // airborne never refuelling
  RefuelDetector air;
  for (int k = 0; k <= 5; ++k) {
    s.t = k;
    s.fuelKg += 20;
    air.update(s, set);
  }
  CHECK(!air.refueling(s.t));
}
