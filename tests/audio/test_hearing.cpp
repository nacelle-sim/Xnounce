// Where the camera is and how loud the cabin PA is there (VP_SPEC § 4.2,
// § 4.5): the camera in the aircraft's axes, the cabin speaker level, and the
// clear voice in the cabin.
#include <doctest/doctest.h>

#include "aircraft/toliss/toliss_types.h"
#include "aircraft/zibo737/zibo737_types.h"
#include "audio/hearing.h"
#include "core/camera.h"

using namespace xn;

TEST_CASE("camera: world offsets into the aircraft's axes") {
  // Heading north: aft is south, +z in X-Plane's world.
  BodyPoint b = worldToBody(0, 0, 10, 0, 0, 0);
  CHECK(b.z == doctest::Approx(10));
  CHECK(b.x == doctest::Approx(0).epsilon(1e-9));
  // Heading east: aft points west (-x), the right wing south (+z).
  b = worldToBody(-10, 0, 0, 90, 0, 0);
  CHECK(b.z == doctest::Approx(10));
  b = worldToBody(0, 0, 3, 90, 0, 0);
  CHECK(b.x == doctest::Approx(3));
  // Nose up 10 degrees: a point 20 m aft along the fuselage sits lower.
  b = worldToBody(0, -20 * std::sin(10 * 3.14159265358979 / 180),
                  20 * std::cos(10 * 3.14159265358979 / 180), 0, 10, 0);
  CHECK(b.z == doctest::Approx(20));
  CHECK(b.y == doctest::Approx(0).epsilon(1e-9));
  // Rolled 30 degrees right: the right side is lower.
  b = worldToBody(std::cos(30 * 3.14159265358979 / 180),
                  -std::sin(30 * 3.14159265358979 / 180), 0, 0, 0, 30);
  CHECK(b.x == doctest::Approx(1));
  CHECK(b.y == doctest::Approx(0).epsilon(1e-9));
}

TEST_CASE("hearing: the X-Camera views of Run1(XN) are in the cabin") {
  // Owner's X-Camera file for the LevelUp 737-800: the pilot at Z -15.65,
  // the wing seats from -6.97, the cabin camera at +10.56 (metres from the
  // aircraft's reference point); the eye is at -15.63.
  const CabinGeometry g =
      zibo737::cabinOf(zibo737::Model::B738, zibo737::Options{});
  CHECK(g.cockpitDepthM == doctest::Approx(1.2f));
  const float eye = -15.63f;
  CHECK(-15.65f - eye < g.cockpitDepthM);  // the pilot view: cockpit
  CHECK(-6.97f - eye > g.cockpitDepthM);   // the wing seats: cabin
  CHECK(10.56f - eye > g.cockpitDepthM);   // the cabin camera: cabin
  CHECK(10.56f - eye < g.cabinFrontM + g.cabinLengthM);
}

TEST_CASE("hearing: the ceiling speakers along the cabin") {
  const CabinGeometry g =
      zibo737::cabinOf(zibo737::Model::B738, zibo737::Options{});
  // Along the rows: close to 1, never far from it.
  double sum = 0;
  int n = 0;
  float lo = 10, hi = 0;
  for (float z = g.cabinFrontM + 2; z < g.cabinFrontM + g.cabinLengthM - 2;
       z += 0.1f, ++n) {
    const float l = cabinSpeakerLevel(z, 0.5f, g, 2.0f);
    sum += l;
    lo = std::min(lo, l);
    hi = std::max(hi, l);
  }
  CHECK(sum / n == doctest::Approx(1.0).epsilon(0.06));
  CHECK(lo > 0.85f);
  CHECK(hi < 1.15f);
  CHECK(hi - lo > 0.02f);  // a row is louder than between rows
  // Towards the galleys it falls off, but never below the floor.
  CHECK(cabinSpeakerLevel(g.cabinFrontM + g.cabinLengthM + 1, 0, g, 2.0f) <
        0.8f);
  CHECK(cabinSpeakerLevel(200, 0, g, 2.0f) == doctest::Approx(0.5f));
  // An unknown cabin is 1 everywhere.
  CHECK(cabinSpeakerLevel(10, 0, CabinGeometry{}, 2.0f) == 1.0f);
  // The ToLiss A321's longer cabin.
  const CabinGeometry a321 =
      toliss::cabinOf(toliss::Family::A321, toliss::Options{});
  CHECK(a321.cabinLengthM > g.cabinLengthM);
}

TEST_CASE("hearing: in the cabin the clear voice follows the speakers") {
  Settings set;
  SimState s;
  s.cabin = zibo737::cabinOf(zibo737::Model::B738, zibo737::Options{});
  s.cameraInCabin = true;
  s.cameraKnown = true;
  s.cameraAftM = 15;
  const Hearing mid = cabinHearing(set, s);
  CHECK(mid.clear == doctest::Approx(1.0f).epsilon(0.12));
  CHECK(mid.muffled == 0.0f);
  s.cameraAftM = s.cabin.cabinFrontM + s.cabin.cabinLengthM + 1.5f;  // galley
  CHECK(cabinHearing(set, s).clear < mid.clear);
  s.cameraKnown = false;  // the old head rule: full level
  CHECK(cabinHearing(set, s).clear == 1.0f);
  // The cockpit with the door shut: the muffled voice, as before.
  s.cameraInCabin = false;
  s.cockpitDoor = 0;
  const Hearing cockpit = cabinHearing(set, s);
  CHECK(cockpit.clear == 0.0f);
  CHECK(cockpit.muffled == doctest::Approx(set.hearingFloor));
  // The PA microphone selected on the 737: clear at the PA volume.
  s.paListen = true;
  s.paListenVol = 1;
  CHECK(cabinHearing(set, s).clear == 1.0f);
}
