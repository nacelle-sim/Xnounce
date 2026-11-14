/// @file
/// Where the camera is in the aircraft (VP_SPEC § 4.2, § 4.5): X-Plane's
/// camera position, taken from world (local OpenGL) coordinates into the
/// aircraft's own axes. Pure arithmetic, so it can be tested.
#pragma once

namespace xn {

/// A point in the aircraft's axes, in metres from its reference point: x to
/// the right, y up, z aft (X-Plane's aircraft coordinates).
struct BodyPoint {
  double x = 0, y = 0, z = 0;
};

/// Takes a world offset from the aircraft's reference point (`local_x/y/z`
/// subtracted) into the aircraft's axes, undoing its heading `psiDeg`, pitch
/// `thetaDeg` and roll `phiDeg` (X-Plane applies roll, then pitch, then
/// heading).
BodyPoint worldToBody(double dx, double dy, double dz, double psiDeg,
                      double thetaDeg, double phiDeg);

}  // namespace xn
