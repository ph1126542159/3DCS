// A7 Color Contour shading (README §8.3). Maps signed deviations onto a
// blue->red spectrum for the variation cloud. Pure data, no GUI.
#pragma once

#include <cstdint>
#include <vector>

namespace opendva {

// RGB triple, 0-255 per channel.
struct Color {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
};

// Maps a deviation onto the contour spectrum. The deviation is normalised to
// t = clamp((dev - minDev) / (maxDev - minDev), 0, 1); t=0 -> blue (0,0,255)
// for the most negative deviation, t=1 -> red (255,0,0) for the most positive,
// transitioning through cyan/green/yellow in between. A degenerate range
// (maxDev <= minDev) maps everything to the midpoint colour.
Color contourColor(double deviation, double minDev, double maxDev);

// Shades a set of nodes. If manualMin <= manualMax the manual scale is used
// (README Manual Scale, default 0-100); otherwise the data range [min,max] of
// `deviations` is taken automatically. An empty input yields an empty result.
std::vector<Color> shadeNodes(const std::vector<double>& deviations,
                              double manualMin, double manualMax);

}  // namespace opendva
