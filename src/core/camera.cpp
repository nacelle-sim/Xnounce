/// @file
/// Implements `src/core/camera.h`; the interface is documented there.
#include "core/camera.h"

#include <cmath>

namespace xn {

BodyPoint worldToBody(double dx, double dy, double dz, double psiDeg,
                      double thetaDeg, double phiDeg) {
  constexpr double kRad = 3.14159265358979323846 / 180.0;
  const double cp = std::cos(psiDeg * kRad), sp = std::sin(psiDeg * kRad);
  const double ct = std::cos(thetaDeg * kRad), st = std::sin(thetaDeg * kRad);
  const double cr = std::cos(phiDeg * kRad), sr = std::sin(phiDeg * kRad);
  // Undo the heading (about y).
  const double x1 = dx * cp + dz * sp;
  const double y1 = dy;
  const double z1 = dz * cp - dx * sp;
  // Undo the pitch (about x).
  const double y2 = y1 * ct + z1 * st;
  const double z2 = z1 * ct - y1 * st;
  // Undo the roll (about z).
  BodyPoint b;
  b.x = x1 * cr - y2 * sr;
  b.y = y2 * cr + x1 * sr;
  b.z = z2;
  return b;
}

}  // namespace xn
