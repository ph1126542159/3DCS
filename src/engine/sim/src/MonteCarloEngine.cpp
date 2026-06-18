// A6 simulation engine (README §7). Monte Carlo loop with a fixed traversal
// order (tolerances -> moves -> measures, child->parent) so that same seed +
// same runs -> bit-for-bit reproducible results.
#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opendva/ISimulationEngine.h"
#include "opendva/Mt19937Rng.h"
#include "opendva/dcs_plugin_api.h"
#include "opendva/domain/Model.h"
#include "opendva/measure/MeasureEvaluator.h"
#include "opendva/move/MoveSolver.h"
#include "opendva/plugin/PluginHost.h"
#include "opendva/tolerance/GeomRule.h"
#include "opendva/tolerance/ToleranceSampler.h"

namespace opendva {
namespace {

// Resolves point positions for the current build. Nominal == base; current ==
// base + accumulated deviation. The reference engine applies tolerance
// deviations directly to point positions along their direction vector.
class BuildPointResolver final : public PointResolver {
public:
    explicit BuildPointResolver(const Model& model) {
        for (const auto& part : model.parts) {
            for (const auto& pt : part.points) {
                nominal_[pt.id] = pt.position;
                current_[pt.id] = pt.position;
                diameters_[pt.id] = pt.diameter;
                directions_[pt.id] = pt.ijk;
            }
        }
        for (const auto& measure : model.measures) {
            measureSpecs_[measure.id] = measure.def.spec;
        }
    }
    void reset() {
        current_ = nominal_;
        measureValues_.clear();
    }
    void addDeviation(PointId id, const Vec3& d) {
        auto it = current_.find(id);
        if (it != current_.end()) { it->second.x += d.x; it->second.y += d.y; it->second.z += d.z; }
    }
    // Overwrites the current position of a point (used by Move relocation).
    void setCurrent(PointId id, const Vec3& p) {
        auto it = current_.find(id);
        if (it != current_.end()) it->second = p;
    }
    Vec3 current(PointId id) const override {
        auto it = current_.find(id);
        return it == current_.end() ? Vec3{} : it->second;
    }
    Vec3 nominal(PointId id) const override {
        auto it = nominal_.find(id);
        return it == nominal_.end() ? Vec3{} : it->second;
    }
    double diameter(PointId id) const override {
        auto it = diameters_.find(id);
        return it == diameters_.end() ? 0.0 : it->second;
    }
    Vec3 direction(PointId id) const override {
        auto it = directions_.find(id);
        return it == directions_.end() ? Vec3{0, 0, 1} : it->second;
    }
    double measureValue(MeasureId id) const override {
        auto it = measureValues_.find(id);
        return it == measureValues_.end() ? 0.0 : it->second;
    }
    SpecLimits measureSpec(MeasureId id) const override {
        auto it = measureSpecs_.find(id);
        return it == measureSpecs_.end() ? SpecLimits{} : it->second;
    }
    double userDllMeasure(const MeasureDef& def) const override {
        if (def.equation.empty()) return 0.0;
        auto* host = plugin::PluginHost::active();
        if (host == nullptr) return 0.0;
        dcsMeasureCalData data{};
        return host->invokeRoutine(def.equation, dcsCalTypeMeas, &data) ? data.value : 0.0;
    }
    void setMeasureValue(MeasureId id, double value) {
        measureValues_[id] = value;
    }

private:
    std::unordered_map<PointId, Vec3> nominal_;
    std::unordered_map<PointId, Vec3> current_;
    std::unordered_map<PointId, double> diameters_;
    std::unordered_map<PointId, Vec3> directions_;
    std::unordered_map<MeasureId, double> measureValues_;
    std::unordered_map<MeasureId, SpecLimits> measureSpecs_;
};

Vec3 normalisedOrFallback(const Vec3& v, const Vec3& fallback) {
    const double len = std::hypot(v.x, v.y, v.z);
    if (len <= 1e-15 || !std::isfinite(len)) return fallback;
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 resolveToleranceDirection(const ToleranceIR& tol,
                               const BuildPointResolver& resolver) {
    const Vec3 fallback = normalisedOrFallback(tol.direction.ijk, {0, 0, 1});
    if (tol.direction.type == DirectionType::TwoPoints &&
        tol.direction.refPoints.size() >= 2) {
        const Vec3 a = resolver.current(tol.direction.refPoints[0]);
        const Vec3 b = resolver.current(tol.direction.refPoints[1]);
        return normalisedOrFallback({b.x - a.x, b.y - a.y, b.z - a.z},
                                    fallback);
    }
    return fallback;
}

double distance(const Vec3& a, const Vec3& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::hypot(dx, dy, dz);
}

// Welford-style accumulation + histogram, finalised to MeasureStats.
struct Accumulator {
    std::vector<double> samples;  // kept for percentiles/histogram (reference impl)
};

// Applies a single tolerance's scalar deviation to all of its feature points,
// along the tolerance direction. Shared by Monte Carlo, GeoFactor and
// Contributor so the geometry rule stays identical across analyses (README §7).
void applyToleranceDeviation(const Model& model, const ToleranceDef& tol,
                             double mag, BuildPointResolver& resolver) {
    const Vec3 dir = resolveToleranceDirection(tol.ir, resolver);
    for (FeatureId fid : tol.features) {
        for (const auto& part : model.parts) {
            for (const auto& f : part.features) {
                if (f.id != fid) continue;
                Vec3 locator{};
                double referenceSpan = 1.0;
                if (!f.definingPoints.empty()) {
                    locator = resolver.current(f.definingPoints.front());
                    referenceSpan = 0.0;
                    for (PointId pid : f.definingPoints) {
                        referenceSpan =
                            std::max(referenceSpan,
                                     distance(resolver.current(pid), locator));
                    }
                    if (referenceSpan <= 1e-15) referenceSpan = 1.0;
                }
                for (PointId pid : f.definingPoints) {
                    tolerance::GeomRuleInput input;
                    input.rule = tol.ir.geomRule;
                    input.magnitude = mag;
                    input.dir = tol.ir.geomRule == GeomRule::NodeNormalOffset
                                    ? resolver.direction(pid)
                                    : dir;
                    input.featureLocatorPoint = locator;
                    input.axis = dir;
                    input.target = resolver.current(pid);
                    input.nominalSize = resolver.diameter(pid);
                    input.referenceSpan = referenceSpan;
                    resolver.addDeviation(pid, tolerance::applyGeomRule(input));
                }
            }
        }
    }
}

// Applies a rigid Mat34 [R|t] to a single point: new = R*old + t.
Vec3 applyMat34(const Mat34& T, const Vec3& p) {
    return {T.m[0][0] * p.x + T.m[0][1] * p.y + T.m[0][2] * p.z + T.m[0][3],
            T.m[1][0] * p.x + T.m[1][1] * p.y + T.m[1][2] * p.z + T.m[1][3],
            T.m[2][0] * p.x + T.m[2][1] * p.y + T.m[2][2] * p.z + T.m[2][3]};
}

// Solves one Move and relocates every point of every part it moves (README §7.2:
// tolerances -> moves -> measures). The transform acts on the CURRENT (already
// toleranced) point positions, so deviation introduced by tolerances rides along
// with the rigid relocation. A failed solve (ok==false) is a no-op (fall-back
// semantics). Uses its own RNG so the main Monte Carlo stream is never disturbed
// — reproducibility of the tolerance draws is preserved bit-for-bit.
void applyMove(const Model& model, const MoveDef& move, BuildPointResolver& resolver,
               IRng& moveRng) {
    auto solver = MoveSolverFactory::create(move.inputs.type);
    if (!solver) return;
    const MoveResult r = solver->solve(move.inputs, moveRng);
    if (!r.ok) return;  // No Solution -> leave the part where it is.
    for (PartId partId : move.moveParts) {
        const Part* part = model.findPart(partId);
        if (!part) continue;
        for (const auto& pt : part->points) {
            const Vec3 moved = applyMat34(r.transform, resolver.current(pt.id));
            resolver.setCurrent(pt.id, moved);
        }
    }
}

// High-Low displacement of a tolerance (README §7.4). The reference sampler
// draws around `offset` with half-width (range * rangeScale)/2, so High/Low
// mirror that span. Returns {low, high} scalar magnitudes.
struct HighLow {
    double low{0};
    double high{0};
};

HighLow toleranceHighLow(const ToleranceDef& tol) {
    if (tol.ir.rands.empty()) return {};
    const RandSpec& r = tol.ir.rands.front();
    const double half = 0.5 * r.range * tol.ir.rangeScale;
    return {r.offset - half, r.offset + half};
}

// Sampling sigma of a tolerance (README §7.5). sigma_t = range / (2 * sigmaNum),
// scaled by rangeScale. With the default Normal sigmaNum=3 this is range/6,
// matching the Monte Carlo population sigma.
const Feature* findFeature(const Model& model, FeatureId id) {
    for (const Part& part : model.parts) {
        for (const Feature& feature : part.features) {
            if (feature.id == id) return &feature;
        }
    }
    return nullptr;
}

MeasureDef materializeFeatureMeasureInputs(const Model& model,
                                           const MeasureDef& def) {
    if (!def.inputPoints.empty() || def.inputFeatures.empty()) return def;
    if (def.type != MeasureType::FeatureMeasure &&
        def.type != MeasureType::FeatureAngle) {
        return def;
    }

    MeasureDef resolved = def;
    if (def.type == MeasureType::FeatureAngle) {
        if (def.inputFeatures.size() < 2) return resolved;
        const Feature* first = findFeature(model, def.inputFeatures[0]);
        const Feature* second = findFeature(model, def.inputFeatures[1]);
        if (first == nullptr || second == nullptr ||
            first->definingPoints.size() < 2 ||
            second->definingPoints.size() < 2) {
            return resolved;
        }
        resolved.inputPoints = {first->definingPoints[0],
                                first->definingPoints[1],
                                second->definingPoints[0],
                                second->definingPoints[1]};
        return resolved;
    }

    for (FeatureId featureId : def.inputFeatures) {
        const Feature* feature = findFeature(model, featureId);
        if (feature == nullptr) continue;
        resolved.inputPoints.insert(resolved.inputPoints.end(),
                                    feature->definingPoints.begin(),
                                    feature->definingPoints.end());
    }
    return resolved;
}

double toleranceSigma(const ToleranceDef& tol) {
    if (tol.ir.rands.empty()) return 0.0;
    const RandSpec& r = tol.ir.rands.front();
    if (r.sigmaNum <= 0.0) return 0.0;
    return (r.range * tol.ir.rangeScale) / (2.0 * r.sigmaNum);
}

MeasureStats finalise(const std::vector<double>& xs, const SpecLimits& spec,
                      double nominalVal) {
    MeasureStats s{};
    s.nominal = nominalVal;
    const std::size_t n = xs.size();
    if (n == 0) return s;
    const double origin = xs[0];
    double scale = 0.0;
    s.minVal = xs[0];
    s.maxVal = xs[0];
    for (double x : xs) {
        scale = std::max(scale, std::fabs(x));
        s.minVal = std::min(s.minVal, x);
        s.maxVal = std::max(s.maxVal, x);
    }
    if (scale <= 0.0 || !std::isfinite(scale)) {
        s.mean = origin;
    } else {
        double meanOffset = 0.0;
        double count = 0.0;
        const double scaledOrigin = origin / scale;
        for (double x : xs) {
            count += 1.0;
            const double offset = x / scale - scaledOrigin;
            meanOffset += (offset - meanOffset) / count;
        }
        s.mean = scale * (scaledOrigin + meanOffset);
    }
    double deviationScale = 0.0;
    for (double x : xs) {
        deviationScale = std::max(deviationScale, std::fabs(x - s.mean));
    }
    double var = 0, m3 = 0, m4 = 0;
    if (deviationScale > 0.0 && std::isfinite(deviationScale)) {
        for (double x : xs) {
            const double d = (x - s.mean) / deviationScale;
            const double d2 = d * d;
            var += d2;
            m3 += d2 * d;
            m4 += d2 * d2;
        }
        var /= static_cast<double>(n);      // population variance (/n, README §7.3)
        m3 /= static_cast<double>(n);
        m4 /= static_cast<double>(n);
        s.sigma = deviationScale * std::sqrt(var);
    }
    s.sixSigma = 6.0 * s.sigma;
    s.range = s.maxVal - s.minVal;
    if (s.sigma > 1e-15) {
        s.skewness = var > 0.0 ? m3 / (var * std::sqrt(var)) : 0.0;
        s.kurtosis = var > 0.0 ? m4 / (var * var) : 0.0;
    }
    // Median.
    std::vector<double> sorted = xs;
    std::sort(sorted.begin(), sorted.end());
    s.median = sorted[n / 2];
    // Capability (README §7.3). Only meaningful with active spec limits.
    if (spec.uslActive && spec.lslActive && s.sigma > 1e-15) {
        s.pp = s.cp = (spec.usl - spec.lsl) / (6.0 * s.sigma);
        s.ppk = s.cpk = std::min((spec.usl - s.mean) / (3.0 * s.sigma),
                                 (s.mean - spec.lsl) / (3.0 * s.sigma));
    }
    // Out-of-spec counts.
    std::size_t lo = 0, hi = 0;
    for (double x : xs) {
        if (spec.lslActive && x < spec.lsl) ++lo;
        if (spec.uslActive && x > spec.usl) ++hi;
    }
    s.lOutPct = 100.0 * static_cast<double>(lo) / static_cast<double>(n);
    s.hOutPct = 100.0 * static_cast<double>(hi) / static_cast<double>(n);
    s.totOutPct = s.lOutPct + s.hOutPct;
    s.dpmo = 1e6 * static_cast<double>(lo + hi) / static_cast<double>(n);
    // Histogram (10 bins across observed range).
    const int bins = 10;
    s.histogram.assign(static_cast<std::size_t>(bins), 0);
    const double span = s.range > 1e-15 ? s.range : 1.0;
    for (double x : xs) {
        int b = static_cast<int>((x - s.minVal) / span * (bins - 1) + 0.5);
        b = std::clamp(b, 0, bins - 1);
        s.histogram[static_cast<std::size_t>(b)]++;
    }
    return s;
}

}  // namespace

class MonteCarloEngine final : public ISimulationEngine {
public:
    std::map<MeasureId, MeasureStats> runMonteCarlo(const Model& model,
                                                    const RunConfig& cfg) override {
        return runMonteCarloDetailed(model, cfg).stats;
    }

    MonteCarloResult runMonteCarloDetailed(const Model& model,
                                           const RunConfig& cfg) override {
        Mt19937Rng rng(cfg.initialSeed);
        // Dedicated stream for Move solving so float/iteration draws inside the
        // solvers never perturb the tolerance draws (reproducibility red line,
        // README §7). Derived deterministically from the run seed.
        Mt19937Rng moveRng(cfg.initialSeed ^ 0x9E3779B97F4A7C15ULL);
        auto sampler = makeReferenceToleranceSampler();
        auto evaluator = makeReferenceMeasureEvaluator();
        BuildPointResolver resolver(model);
        BuildState state{};
        state.impl = &resolver;

        std::unordered_map<MeasureId, std::vector<double>> samples;
        MonteCarloResult result;
        const auto tolerances = model.activeTolerances();   // child->parent order
        const auto moves = model.activeMovesInOrder();        // tree order

        for (int build = 0; build < cfg.totalRuns; ++build) {
            resolver.reset();
            // 1. Tolerances (fixed order; deterministic RNG consumption).
            for (const auto* tol : tolerances) {
                const double mag = sampler->applyDeviation(tol->ir, MeshHandle{}, rng);
                applyToleranceDeviation(model, *tol, mag, resolver);
            }
            // 2. Moves (tree order, README §7.2). Each Move solves a rigid (R,t)
            //    via A3 and relocates its parts on top of the toleranced points.
            for (const auto* move : moves) {
                applyMove(model, *move, resolver, moveRng);
            }
            // 3. Measures.
            SimulationSampleRow sampleRow;
            sampleRow.buildIndex = build;
            for (const auto& mr : model.measures) {
                if (!mr.def.active) continue;
                const MeasureDef measureDef =
                    materializeFeatureMeasureInputs(model, mr.def);
                const double value = evaluator->evaluate(measureDef, state);
                resolver.setMeasureValue(mr.id, value);
                samples[mr.id].push_back(value);
                sampleRow.measureValues[mr.id] = value;
            }
            result.samples.push_back(std::move(sampleRow));
        }

        // Nominal pass: tolerances at nominal (skipped), Moves applied so the
        // nominal value reflects the post-relocation geometry (README §7.2).
        resolver.reset();
        {
            Mt19937Rng nominalMoveRng(cfg.initialSeed ^ 0x9E3779B97F4A7C15ULL);
            for (const auto* move : moves) {
                applyMove(model, *move, resolver, nominalMoveRng);
            }
        }
        for (const auto& mr : model.measures) {
            if (!mr.def.active) continue;
            const MeasureDef measureDef =
                materializeFeatureMeasureInputs(model, mr.def);
            const double nom = evaluator->evaluate(measureDef, state);
            resolver.setMeasureValue(mr.id, nom);
            result.stats[mr.id] = finalise(samples[mr.id], mr.def.spec, nom);
        }
        return result;
    }

    // README §7.4 — High-Low difference quotient. Rows = contributors (active
    // tolerance ids, child->parent order), cols = active output measures.
    SensitivityMatrix runGeoFactor(const Model& model) override {
        SensitivityMatrix sm;
        const auto measures = activeMeasures(model);
        const auto tolerances = model.activeTolerances();
        for (const auto* tol : tolerances) sm.contributors.push_back(tol->id);
        for (const auto* mr : measures) sm.measures.push_back(mr->id);
        sm.geoFactor.assign(tolerances.size(),
                            std::vector<double>(measures.size(), 0.0));
        for (std::size_t i = 0; i < tolerances.size(); ++i) {
            sm.geoFactor[i] = geoFactorRow(model, *tolerances[i], measures);
        }
        return sm;
    }

    // README §7.5 — HLM. For each measure, contribution_i% =
    // (G_i*sigma_i)^2 / Sum_k (G_k*sigma_k)^2 * 100. Six-Sigma_i = sigma_i*G_i*6.
    std::vector<ContributorRow> runContributor(const Model& model) override {
        std::vector<ContributorRow> rows;
        const auto measures = activeMeasures(model);
        const auto tolerances = model.activeTolerances();

        // GeoFactor[i][j] for every (tolerance i, measure j).
        std::vector<std::vector<double>> gf(tolerances.size());
        std::vector<double> sigma(tolerances.size(), 0.0);
        for (std::size_t i = 0; i < tolerances.size(); ++i) {
            gf[i] = geoFactorRow(model, *tolerances[i], measures);
            sigma[i] = toleranceSigma(*tolerances[i]);
        }

        for (std::size_t j = 0; j < measures.size(); ++j) {
            // Total variance for this measure across all contributors. Scale
            // effective sigmas first so finite huge ranges keep their ratios.
            std::vector<double> effSigmaAbs(tolerances.size(), 0.0);
            double maxEffSigma = 0.0;
            for (std::size_t i = 0; i < tolerances.size(); ++i) {
                effSigmaAbs[i] = std::fabs(gf[i][j] * sigma[i]);
                maxEffSigma = std::max(maxEffSigma, effSigmaAbs[i]);
            }
            double sumSq = 0.0;
            if (maxEffSigma > 0.0 && std::isfinite(maxEffSigma)) {
                for (double effSigma : effSigmaAbs) {
                    const double scaled = effSigma / maxEffSigma;
                    sumSq += scaled * scaled;
                }
            }
            for (std::size_t i = 0; i < tolerances.size(); ++i) {
                ContributorRow row{};
                row.measure = measures[j]->id;
                row.contributor = tolerances[i]->id;
                row.geoFactor = gf[i][j];
                row.sixSigma = 6.0 * std::fabs(sigma[i] * gf[i][j]);
                if (sumSq > 1e-300) {
                    const double scaled = effSigmaAbs[i] / maxEffSigma;
                    row.contributionPct = 100.0 * (scaled * scaled) / sumSq;
                }
                rows.push_back(row);
            }
        }
        return rows;
    }

private:
    // Active output measures in model order (shared by GeoFactor/Contributor).
    static std::vector<const MeasureRecord*> activeMeasures(const Model& model) {
        std::vector<const MeasureRecord*> out;
        for (const auto& mr : model.measures) {
            if (mr.def.active && mr.def.asOutput) out.push_back(&mr);
        }
        return out;
    }

    // GeoFactor of one tolerance against every measure (README §7.4). Pushes the
    // tolerance to High (+) and Low (-), rebuilds, and forms the difference
    // quotient GF = (M_high - M_low) / (tol_high - tol_low). All other tolerances
    // stay nominal, so only this contributor moves.
    static std::vector<double> geoFactorRow(
        const Model& model, const ToleranceDef& tol,
        const std::vector<const MeasureRecord*>& measures) {
        std::vector<double> row(measures.size(), 0.0);
        const HighLow hl = toleranceHighLow(tol);
        const double denom = hl.high - hl.low;
        auto evaluator = makeReferenceMeasureEvaluator();
        BuildPointResolver resolver(model);
        BuildState state{};
        state.impl = &resolver;

        // High build.
        resolver.reset();
        applyToleranceDeviation(model, tol, hl.high, resolver);
        std::vector<double> mHigh(measures.size(), 0.0);
        for (std::size_t j = 0; j < measures.size(); ++j) {
            const MeasureDef measureDef =
                materializeFeatureMeasureInputs(model, measures[j]->def);
            mHigh[j] = evaluator->evaluate(measureDef, state);
            resolver.setMeasureValue(measures[j]->id, mHigh[j]);
        }
        // Low build.
        resolver.reset();
        applyToleranceDeviation(model, tol, hl.low, resolver);
        for (std::size_t j = 0; j < measures.size(); ++j) {
            const MeasureDef measureDef =
                materializeFeatureMeasureInputs(model, measures[j]->def);
            const double lowValue = evaluator->evaluate(measureDef, state);
            resolver.setMeasureValue(measures[j]->id, lowValue);
            row[j] =
                std::fabs(denom) > 1e-15 ? (mHigh[j] - lowValue) / denom : 0.0;
        }
        return row;
    }
};

std::unique_ptr<ISimulationEngine> makeMonteCarloEngine() {
    return std::make_unique<MonteCarloEngine>();
}

}  // namespace opendva
