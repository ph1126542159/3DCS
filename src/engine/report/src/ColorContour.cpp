#include "opendva/report/ColorContour.h"

#include <algorithm>
#include <cmath>

namespace opendva {

namespace {

// Clamps x into [lo, hi].
double clampd(double x, double lo, double hi) {
    return std::max(lo, std::min(hi, x));
}

// Rounds a 0..1 channel weight to a 0..255 byte.
std::uint8_t toByte(double v) {
    return static_cast<std::uint8_t>(std::lround(clampd(v, 0.0, 1.0) * 255.0));
}

// Maps a normalised t in [0,1] onto a blue->cyan->green->yellow->red ramp.
// Endpoints are pure blue (0,0,255) and pure red (255,0,0); the midpoint
// passes through green, with cyan/yellow as the quarter transitions.
Color rampColor(double t) {
    t = clampd(t, 0.0, 1.0);
    // Four equal segments across the spectrum.
    const double s = t * 4.0;
    double r = 0.0, g = 0.0, b = 0.0;
    if (s < 1.0) {  // blue -> cyan
        b = 1.0;
        g = s;
    } else if (s < 2.0) {  // cyan -> green
        g = 1.0;
        b = 2.0 - s;
    } else if (s < 3.0) {  // green -> yellow
        g = 1.0;
        r = s - 2.0;
    } else {  // yellow -> red
        r = 1.0;
        g = 4.0 - s;
    }
    return Color{toByte(r), toByte(g), toByte(b)};
}

}  // namespace

Color contourColor(double deviation, double minDev, double maxDev) {
    if (!std::isfinite(deviation) || !std::isfinite(minDev) ||
        !std::isfinite(maxDev)) {
        return rampColor(0.5);
    }
    const double span = maxDev - minDev;
    // Degenerate range: collapse to the midpoint colour (green).
    if (span <= 0.0) return rampColor(0.5);
    const double t = clampd((deviation - minDev) / span, 0.0, 1.0);
    return rampColor(t);
}

std::vector<Color> shadeNodes(const std::vector<double>& deviations,
                              double manualMin, double manualMax) {
    std::vector<Color> out;
    out.reserve(deviations.size());
    if (deviations.empty()) return out;

    double lo = manualMin;
    double hi = manualMax;
    // Manual scale only when it forms a valid range; otherwise auto from data.
    if (!std::isfinite(manualMin) || !std::isfinite(manualMax) ||
        !(manualMin <= manualMax)) {
        bool foundFinite = false;
        for (const double deviation : deviations) {
            if (!std::isfinite(deviation)) continue;
            if (!foundFinite) {
                lo = deviation;
                hi = deviation;
                foundFinite = true;
            } else {
                lo = std::min(lo, deviation);
                hi = std::max(hi, deviation);
            }
        }
        if (!foundFinite) {
            lo = 0.0;
            hi = 0.0;
        }
    }
    for (double d : deviations) out.push_back(contourColor(d, lo, hi));
    return out;
}

}  // namespace opendva
