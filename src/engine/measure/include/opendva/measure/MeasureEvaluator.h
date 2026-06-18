// A5 reference measure evaluator + the PointResolver the engine implements.
#pragma once
#include <memory>

#include "opendva/IMeasureEvaluator.h"
#include "opendva/Types.h"

namespace opendva {

// The simulation engine implements this and points BuildState::impl at it.
struct PointResolver {
    virtual ~PointResolver() = default;
    virtual Vec3 current(PointId) const = 0;
    virtual Vec3 nominal(PointId) const = 0;
    // Per-point feature-of-size diameter for the current build (0 if the point
    // carries no size). Enables CircleDiameter / CircleInterference / Virtual
    // Clearance to use real radii instead of a centre-distance approximation.
    virtual double diameter(PointId) const { return 0.0; }
    // Per-point direction vector for Equation P1I/P1J/P1K style variables.
    virtual Vec3 direction(PointId) const { return {0, 0, 1}; }
    // Value of another already-evaluated measure in this same build, for the
    // Combination measure (returns 0 if not available). Measures are evaluated
    // in list order, so a Combination must reference earlier measures.
    virtual double measureValue(MeasureId) const { return 0.0; }
    // Specification limits for another measure. Used by Combination In-Spec;
    // default empty limits preserve lightweight test and compatibility resolvers.
    virtual SpecLimits measureSpec(MeasureId) const { return {}; }
    // User-DLL measure dispatch hook. The simulation layer can override this to
    // route a UserDll measure to the plugin host; the default preserves the
    // current unbound-plugin behaviour.
    virtual double userDllMeasure(const MeasureDef&) const { return 0.0; }
};

std::unique_ptr<IMeasureEvaluator> makeReferenceMeasureEvaluator();

}  // namespace opendva
