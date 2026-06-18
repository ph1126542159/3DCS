// A5 measure evaluator. Reference build supports the distance family with the
// three direction modes (README §6.1.3) plus the point-to-line/plane distance,
// line/plane angle, two-point-list, circularity, dimensional-distance,
// circle-interference, virtual-clearance, feature, combination, and equation
// measures (README §6.2-6.6), User-DLL measure dispatch through the resolver
// hook, plus the six GD&T measures degraded to Point Distance sets (README
// §6.7).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <limits>
#include <string>
#include <vector>

#include "opendva/measure/MeasureEvaluator.h"

namespace opendva {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-12;
constexpr std::size_t kMaxEquationStringLength = 400;

Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
double len(const Vec3& v) { return std::hypot(v.x, v.y, v.z); }
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 scale(const Vec3& v, double s) { return {v.x * s, v.y * s, v.z * s}; }
Vec3 midpoint(const Vec3& a, const Vec3& b) {
    return {a.x * 0.5 + b.x * 0.5, a.y * 0.5 + b.y * 0.5, a.z * 0.5 + b.z * 0.5};
}
Vec3 normaliseWithFallback(Vec3 v, const Vec3& fallback) {
    const double scaleValue =
        std::max(std::fabs(v.x), std::max(std::fabs(v.y), std::fabs(v.z)));
    if (scaleValue < kEps || !std::isfinite(scaleValue)) return fallback;
    const double sx = v.x / scaleValue;
    const double sy = v.y / scaleValue;
    const double sz = v.z / scaleValue;
    const double l = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (l < kEps || !std::isfinite(l)) return fallback;
    return {sx / l, sy / l, sz / l};
}
Vec3 normalise(Vec3 v) {
    return normaliseWithFallback(v, {0, 0, 1});
}
Vec3 unitOrZero(Vec3 v) {
    return normaliseWithFallback(v, {0, 0, 0});
}
double componentScale(const Vec3& v) {
    return std::max(std::fabs(v.x), std::max(std::fabs(v.y), std::fabs(v.z)));
}
double scaledDotForDirection(const Vec3& v, const Vec3& dir) {
    const double s = componentScale(v);
    if (s <= 0.0 || !std::isfinite(s)) return 0.0;
    return (v.x / s) * dir.x + (v.y / s) * dir.y + (v.z / s) * dir.z;
}
double stableDotForDirection(const Vec3& v, const Vec3& dir) {
    const auto closeAbs = [](double a, double b) {
        const double ma = std::fabs(a);
        const double mb = std::fabs(b);
        return std::fabs(ma - mb) <= std::max(ma, mb) * 1e-12;
    };
    const auto paired = [](double va, double da, double vb, double db) {
        if ((da < 0.0) == (db < 0.0)) {
            return 0.5 * (da + db) * (va + vb);
        }
        return 0.5 * (da - db) * (va - vb);
    };
    if (closeAbs(dir.x, dir.y)) {
        return paired(v.x, dir.x, v.y, dir.y) + v.z * dir.z;
    }
    if (closeAbs(dir.x, dir.z)) {
        return paired(v.x, dir.x, v.z, dir.z) + v.y * dir.y;
    }
    if (closeAbs(dir.y, dir.z)) {
        return paired(v.y, dir.y, v.z, dir.z) + v.x * dir.x;
    }
    return dot(v, dir);
}

double rad2deg(double r) { return (r / kPi) * 180.0; }
double angleToPositiveDegrees(double degrees) {
    double result = std::fmod(degrees, 360.0);
    if (result < 0.0) result += 360.0;
    return result;
}

double angleToSignedDegrees(double degrees) {
    double result = angleToPositiveDegrees(degrees);
    if (result > 180.0) result -= 360.0;
    return result;
}

bool parsePositiveIndex(const std::string& text, long& out) {
    if (text.empty()) return false;
    long value = 0;
    for (const unsigned char c : text) {
        if (!std::isdigit(c)) return false;
        const int digit = static_cast<int>(c - '0');
        if (value > (std::numeric_limits<long>::max() - digit) / 10) return false;
        value = value * 10 + digit;
    }
    out = value;
    return true;
}

// True Angle between two vectors, in degrees, range [0, 180].
double vectorAngleDeg(const Vec3& a, const Vec3& b) {
    const Vec3 au = unitOrZero(a);
    const Vec3 bu = unitOrZero(b);
    if (len(au) < kEps || len(bu) < kEps) return 0.0;
    double c = dot(au, bu);
    c = std::max(-1.0, std::min(1.0, c));  // clamp against round-off
    return rad2deg(std::acos(c));
}

double planeAngleDeg(const Vec3& a, const Vec3& b) {
    const double angle = vectorAngleDeg(a, b);
    return angle > 90.0 ? 180.0 - angle : angle;
}

double signedProjectedAngleDeg(const Vec3& a, const Vec3& b, const Vec3& viewDir) {
    const Vec3 n = unitOrZero(viewDir);
    if (len(n) < kEps) return 0.0;
    const Vec3 seed = std::fabs(n.z) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    const Vec3 u = unitOrZero(cross(seed, n));
    const Vec3 v = cross(n, u);
    const double au0 = scaledDotForDirection(a, u);
    const double av0 = scaledDotForDirection(a, v);
    const double bu0 = scaledDotForDirection(b, u);
    const double bv0 = scaledDotForDirection(b, v);
    const double na = std::hypot(au0, av0);
    const double nb = std::hypot(bu0, bv0);
    if (componentScale(a) * na < kEps || componentScale(b) * nb < kEps) return 0.0;
    const double au = au0 / na;
    const double av = av0 / na;
    const double bu = bu0 / nb;
    const double bv = bv0 / nb;
    const double c = std::max(-1.0, std::min(1.0, au * bu + av * bv));
    const double s = au * bv - av * bu;
    return rad2deg(std::atan2(s, c));
}

double applyMode(const Vec3& delta, const Vec3& dir, DirectionMode mode) {
    switch (mode) {
        case DirectionMode::TrueDistance:
            return len(delta);  // non-negative
        case DirectionMode::ProjectedOnVector:
            return stableDotForDirection(delta, unitOrZero(dir));  // signed
        case DirectionMode::ProjectedOnPlane: {
            const double dirScale = componentScale(dir);
            if (dirScale <= 0.0 || !std::isfinite(dirScale)) return 0.0;
            const Vec3 n = scale(dir, 1.0 / dirScale);
            const double nLen = len(n);
            if (nLen < kEps) return 0.0;
            return len(cross(delta, n)) / nLen;  // non-negative
        }
    }
    return len(delta);
}

// Perpendicular distance from point p to the infinite line through (a, b).
// |(p - a) - ((p - a) . u) u| with u the unit line direction (README §6.2c).
double pointLineDistance(const Vec3& p, const Vec3& a, const Vec3& b) {
    const Vec3 line = sub(b, a);
    const double lineScale = componentScale(line);
    if (lineScale <= 0.0 || !std::isfinite(lineScale)) return 0.0;
    const Vec3 lineScaled = scale(line, 1.0 / lineScale);
    const double lineLen = len(lineScaled);
    if (lineLen < kEps) return 0.0;
    const Vec3 ap = sub(p, a);
    const Vec3 area = cross(ap, lineScaled);
    const double crossDistance = len(area) / lineLen;
    if (std::isfinite(crossDistance)) return crossDistance;

    const Vec3 u = unitOrZero(line);
    if (len(u) < kEps) return 0.0;
    const Vec3 seed = std::fabs(u.z) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    const Vec3 v0 = unitOrZero(cross(seed, u));
    const Vec3 v1 = cross(u, v0);
    const double s = componentScale(ap);
    if (s <= 0.0 || !std::isfinite(s)) return 0.0;
    const double d0 = dot(ap, v0);
    const double d1 = dot(ap, v1);
    if (std::isfinite(d0) && std::isfinite(d1)) return std::hypot(d0, d1);
    const double sd0 = scaledDotForDirection(ap, v0);
    const double sd1 = scaledDotForDirection(ap, v1);
    return s * std::hypot(sd0, sd1);
}

double projectedPointLineDistance(const Vec3& p, const Vec3& a, const Vec3& b,
                                  const Vec3& planeNormal) {
    const Vec3 n = unitOrZero(planeNormal);
    if (len(n) < kEps) return 0.0;
    const Vec3 line = sub(b, a);
    const double lineInPlane = applyMode(line, n, DirectionMode::ProjectedOnPlane);
    if (lineInPlane < kEps) return 0.0;
    const Vec3 ap = sub(p, a);
    const double area = stableDotForDirection(cross(ap, line), n);
    return std::fabs(area) / lineInPlane;
}

// Unit normal of the plane fitted to three points (README §6.2d).
Vec3 planeNormal(const Vec3& q0, const Vec3& q1, const Vec3& q2) {
    const Vec3 u = unitOrZero(sub(q1, q0));
    const Vec3 v = unitOrZero(sub(q2, q0));
    return unitOrZero(cross(u, v));
}

// Project p onto the plane through `origin` with unit normal n.
Vec3 projectOntoPlane(const Vec3& p, const Vec3& origin, const Vec3& n) {
    const Vec3 d = sub(p, origin);
    return sub(p, scale(n, dot(d, n)));
}

// Centroid of a point list (assumes non-empty).
Vec3 centroidOf(const std::vector<Vec3>& pts) {
    const Vec3 origin = pts.front();
    double scaleValue = 0.0;
    for (const Vec3& p : pts) {
        scaleValue = std::max(scaleValue, std::fabs(origin.x));
        scaleValue = std::max(scaleValue, std::fabs(origin.y));
        scaleValue = std::max(scaleValue, std::fabs(origin.z));
        scaleValue = std::max(scaleValue, std::fabs(p.x));
        scaleValue = std::max(scaleValue, std::fabs(p.y));
        scaleValue = std::max(scaleValue, std::fabs(p.z));
    }
    if (scaleValue <= 0.0 || !std::isfinite(scaleValue)) return origin;

    Vec3 meanOffset{};
    double n = 0.0;
    for (const Vec3& p : pts) {
        n += 1.0;
        const Vec3 offset{p.x / scaleValue - origin.x / scaleValue,
                          p.y / scaleValue - origin.y / scaleValue,
                          p.z / scaleValue - origin.z / scaleValue};
        meanOffset.x += (offset.x - meanOffset.x) / n;
        meanOffset.y += (offset.y - meanOffset.y) / n;
        meanOffset.z += (offset.z - meanOffset.z) / n;
    }
    return {scaleValue * (origin.x / scaleValue + meanOffset.x),
            scaleValue * (origin.y / scaleValue + meanOffset.y),
            scaleValue * (origin.z / scaleValue + meanOffset.z)};
}

// Drop the component of p along the unit normal n (project to the plane
// through the origin perpendicular to n).
Vec3 dropNormal(const Vec3& p, const Vec3& n) {
    return sub(p, scale(n, dot(p, n)));
}

// Build an orthonormal in-plane basis (u, v) for the plane whose unit normal is
// n. u is an arbitrary direction perpendicular to n; v = n x u completes the
// right-handed frame. Used to sample 0/45/90/135 degree directions in-plane.
void planeBasis(const Vec3& n, Vec3& u, Vec3& v) {
    // Pick a seed axis least aligned with n to avoid a degenerate cross product.
    const Vec3 seed = std::fabs(n.z) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    u = normalise(cross(seed, n));
    v = normalise(cross(n, u));
}

// Shared GD&T core (README §6.7): project a current-vs-nominal deviation vector
// into the plane perpendicular to dirNormal, then for a diametrical zone sample
// the in-plane deviation along 0/45/90/135 degree directions and report the
// largest absolute projection scaled by 2 (a diametrical tolerance band is the
// diameter, hence the factor of two). For a non-diametrical zone the band is the
// signed deviation along dirNormal itself (a single Point Distance). Results may
// be negative to keep the response linear (no MMC/LMC/datum-shift handling).
double gdtPositionValue(const Vec3& deviation, const Vec3& dirNormal,
                        bool diametrical) {
    const Vec3 n = unitOrZero(dirNormal);
    if (len(n) < kEps) {
        return 0.0;
    }
    if (!diametrical) {
        // Non-diametrical: single Point Distance projected on the reference axis.
        return stableDotForDirection(deviation, n);
    }
    Vec3 u, v;
    planeBasis(n, u, v);
    const Vec3 inPlane = dropNormal(deviation, n);
    // Sample 0/45/90/135 degree directions; opposite directions only flip the
    // sign, so the largest absolute projection covers the full 0-180 sweep.
    double best = 0.0;
    for (int k = 0; k < 4; ++k) {
        const double a = static_cast<double>(k) * (kPi / 4.0);  // 0,45,90,135 deg
        const Vec3 axis{u.x * std::cos(a) + v.x * std::sin(a),
                        u.y * std::cos(a) + v.y * std::sin(a),
                        u.z * std::cos(a) + v.z * std::sin(a)};
        best = std::max(best, std::fabs(dot(inPlane, axis)));
    }
    return 2.0 * best;  // diametrical band = diameter = 2 * max radial offset
}

// ----- Minimal expression evaluator for MeasureType::Equation (README §6.6) --
// Supported subset: numeric literals, the [Keyword:Index] variable syntax for
// point coordinates/radius/direction (P1X/P1Y/P1Z/P1C/P1I/P1J/P1K ..
// resolved from inputPoints), measure direction (DRI/DRJ/DRK),
// measure references (MS resolved from inputFeatures), earlier string-list
// values (STR), indexed VAL constants (def.values), and legacy [VAL]
// compatibility (def.scale), the binary operators
// + - * / ^, unary minus, parentheses, and scalar functions including
// sqrt/sin/cos/tan/abs/log/round, inverse trig, unit conversion, and MIN/MAX.
// Trig inputs use radians; use deg2rad for degree-valued inputs.
// Unsupported Equation keywords are TODOs where the reference model does not
// carry source data for them.
class ExprParser {
public:
    ExprParser(const std::string& src, const std::vector<Vec3>& points,
               const std::vector<double>& pointDiameters,
               const std::vector<Vec3>& pointDirections,
               const Vec3& measureDirection,
               const std::vector<double>& measureValues,
               const std::vector<double>& stringValues,
               const std::vector<double>& valueList, double val)
        : src_(src),
          points_(points),
          pointDiameters_(pointDiameters),
          pointDirections_(pointDirections),
          measureDirection_(measureDirection),
          measureValues_(measureValues),
          stringValues_(stringValues),
          valueList_(valueList),
          val_(val) {}

    // Returns the evaluated value; `ok` is set false on any parse/lookup error.
    double parse(bool& ok) {
        ok_ = true;
        const double v = parseExpr();
        skipSpace();
        if (pos_ != src_.size()) ok_ = false;  // trailing garbage
        if (!std::isfinite(v)) ok_ = false;
        ok = ok_;
        return v;
    }

private:
    void skipSpace() {
        while (pos_ < src_.size() &&
               std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
    }
    char peek() {
        skipSpace();
        return pos_ < src_.size() ? src_[pos_] : '\0';
    }
    bool consumeWord(const char* word) {
        skipSpace();
        const std::size_t start = pos_;
        for (std::size_t i = 0; word[i] != '\0'; ++i) {
            if (pos_ + i >= src_.size()) return false;
            const char actual =
                static_cast<char>(std::tolower(static_cast<unsigned char>(src_[pos_ + i])));
            if (actual != word[i]) return false;
        }
        pos_ += std::char_traits<char>::length(word);
        if (pos_ < src_.size() &&
            (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
            pos_ = start;
            return false;
        }
        return true;
    }
    bool previousNonSpaceIsOpenParen() const {
        std::size_t i = pos_;
        while (i > 0) {
            --i;
            if (!std::isspace(static_cast<unsigned char>(src_[i]))) {
                return src_[i] == '(';
            }
        }
        return false;
    }
    bool readComparisonOperator(std::string& op) {
        skipSpace();
        if (pos_ + 1 < src_.size() && src_[pos_] == '=' && src_[pos_ + 1] == '=') {
            op = "==";
            pos_ += 2;
            return true;
        }
        if (pos_ + 1 < src_.size() && src_[pos_] == '<' && src_[pos_ + 1] == '=') {
            op = "<=";
            pos_ += 2;
            return true;
        }
        if (pos_ + 1 < src_.size() && src_[pos_] == '>' && src_[pos_ + 1] == '=') {
            op = ">=";
            pos_ += 2;
            return true;
        }
        if (pos_ < src_.size() && (src_[pos_] == '<' || src_[pos_] == '>')) {
            op.assign(1, src_[pos_++]);
            return true;
        }
        return false;
    }
    static bool compareValues(double lhs, double rhs, const std::string& op) {
        if (op == "==") return std::fabs(lhs - rhs) <= kEps;
        if (op == "<=") return lhs <= rhs + kEps;
        if (op == ">=") return lhs + kEps >= rhs;
        if (op == "<") return lhs < rhs - kEps;
        if (op == ">") return lhs > rhs + kEps;
        return false;
    }
    static bool inTwoPiRange(double value) {
        return value >= -2.0 * kPi - kEps && value <= 2.0 * kPi + kEps;
    }
    static bool inHalfPiRange(double value) {
        return value > -0.5 * kPi && value < 0.5 * kPi;
    }

    // comparison := expr (('=='|'<='|'>='|'<'|'>') expr)?
    double parseComparison() {
        const double lhs = parseExpr();
        if (!ok_) return 0.0;
        std::string op;
        if (!readComparisonOperator(op)) return lhs;

        const double rhs = parseExpr();
        if (!ok_) return 0.0;
        return compareValues(lhs, rhs, op) ? 1.0 : 0.0;
    }

    // expr := term (('+'|'-') term)*
    double parseExpr() {
        double v = parseTerm();
        while (ok_) {
            const char c = peek();
            if (c == '+') { ++pos_; v += parseTerm(); }
            else if (c == '-') { ++pos_; v -= parseTerm(); }
            else break;
        }
        return v;
    }
    // term := power (('*'|'/') power)*
    double parseTerm() {
        double v = parsePower();
        while (ok_) {
            const char c = peek();
            if (c == '*') { ++pos_; v *= parsePower(); }
            else if (c == '/') {
                ++pos_;
                const double rhs = parsePower();
                if (std::fabs(rhs) < kEps) { ok_ = false; return 0.0; }
                v /= rhs;
            } else {
                break;
            }
        }
        return v;
    }
    // power := unary ('^' power)?  (right-associative)
    double parsePower() {
        const double base = parseUnary();
        if (peek() == '^') {
            ++pos_;
            return std::pow(base, parsePower());
        }
        return base;
    }
    // unary := '-' unary | primary
    double parseUnary() {
        if (peek() == '-') {
            if (!previousNonSpaceIsOpenParen()) {
                ok_ = false;
                return 0.0;
            }
            ++pos_;
            return -parseUnary();
        }
        if (peek() == '+') { ++pos_; return parseUnary(); }
        return parsePrimary();
    }
    // primary := number | '(' expr ')' | '[' var ']' | func '(' expr ')'
    double parsePrimary() {
        const char c = peek();
        if (c == '(') {
            ++pos_;
            const double v = parseComparison();
            if (peek() != ')') { ok_ = false; return 0.0; }
            ++pos_;
            return v;
        }
        if (c == '[') return parseVariable();
        if (std::isalpha(static_cast<unsigned char>(c))) return parseFunc();
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            return parseNumber();
        }
        ok_ = false;
        return 0.0;
    }
    double parseNumber() {
        skipSpace();
        const std::size_t start = pos_;
        bool sawDigit = false;
        while (pos_ < src_.size() &&
               std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
            sawDigit = true;
        }
        if (pos_ < src_.size() && src_[pos_] == '.') {
            ++pos_;
            while (pos_ < src_.size() &&
                   std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                ++pos_;
                sawDigit = true;
            }
        }
        if (!sawDigit) { ok_ = false; return 0.0; }
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            const std::size_t exponentStart = pos_;
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
            const std::size_t exponentDigitsStart = pos_;
            while (pos_ < src_.size() &&
                   std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                ++pos_;
            }
            if (pos_ == exponentDigitsStart) {
                pos_ = exponentStart;
            }
        }
        double value = 0.0;
        try {
            value = std::stod(src_.substr(start, pos_ - start));
        } catch (const std::exception&) {
            ok_ = false;
            return 0.0;
        }
        if (!std::isfinite(value)) {
            ok_ = false;
            return 0.0;
        }
        return value;
    }
    double parseParenthesizedValue() {
        if (peek() != '(') { ok_ = false; return 0.0; }
        ++pos_;
        const double v = parseComparison();
        if (peek() != ')') { ok_ = false; return 0.0; }
        ++pos_;
        return v;
    }
    bool skipExpressionTo(char delimiter) {
        skipSpace();
        const std::size_t start = pos_;
        int parenDepth = 0;
        int bracketDepth = 0;
        while (pos_ < src_.size()) {
            const char c = src_[pos_];
            if (parenDepth == 0 && bracketDepth == 0 && c == delimiter) {
                std::size_t end = pos_;
                while (end > start &&
                       std::isspace(static_cast<unsigned char>(src_[end - 1]))) {
                    --end;
                }
                return end > start;
            }
            if (c == '(') {
                ++parenDepth;
            } else if (c == ')') {
                if (parenDepth == 0) return false;
                --parenDepth;
            } else if (c == '[') {
                ++bracketDepth;
            } else if (c == ']') {
                if (bracketDepth == 0) return false;
                --bracketDepth;
            }
            ++pos_;
        }
        return false;
    }
    bool skipParenthesizedValue() {
        if (peek() != '(') return false;
        ++pos_;
        if (!skipExpressionTo(')')) return false;
        ++pos_;
        return true;
    }
    double parseIfStatement() {
        const double lhs = parseParenthesizedValue();
        std::string op;
        if (!ok_ || !readComparisonOperator(op)) { ok_ = false; return 0.0; }
        const double rhs = parseParenthesizedValue();
        if (!ok_) return 0.0;
        const bool condition = compareValues(lhs, rhs, op);
        consumeWord("then");
        if (condition) {
            const double trueValue = parseParenthesizedValue();
            if (!ok_) return 0.0;
            consumeWord("else");
            if (!skipParenthesizedValue()) {
                ok_ = false;
                return 0.0;
            }
            return trueValue;
        }

        if (!skipParenthesizedValue()) {
            ok_ = false;
            return 0.0;
        }
        consumeWord("else");
        const double falseValue = parseParenthesizedValue();
        if (!ok_) return 0.0;
        return falseValue;
    }
    double parseIfThenElseFunction() {
        const double condition = parseComparison();
        if (!ok_ || peek() != ',') { ok_ = false; return 0.0; }
        ++pos_;

        if (std::fabs(condition) > kEps) {
            const double trueValue = parseComparison();
            if (!ok_ || peek() != ',') { ok_ = false; return 0.0; }
            ++pos_;
            if (!skipExpressionTo(')')) { ok_ = false; return 0.0; }
            ++pos_;
            return trueValue;
        }

        if (!skipExpressionTo(',')) { ok_ = false; return 0.0; }
        ++pos_;
        const double falseValue = parseComparison();
        if (!ok_ || peek() != ')') { ok_ = false; return 0.0; }
        ++pos_;
        return falseValue;
    }
    // func := identifier '(' expr ')'
    double parseFunc() {
        skipSpace();
        std::size_t start = pos_;
        while (pos_ < src_.size() &&
               (std::isalnum(static_cast<unsigned char>(src_[pos_])) ||
                src_[pos_] == '_')) {
            ++pos_;
        }
        const std::string rawName = src_.substr(start, pos_ - start);
        std::string name = rawName;
        for (char& ch : name) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool isConditionalName = name == "if" || name == "if_then_else";
        if (rawName != name && !isConditionalName && rawName != "MIN" && rawName != "MAX") {
            ok_ = false;
            return 0.0;
        }
        if (name == "if") {
            if (conditionalDepth_ > 0) {
                ok_ = false;
                return 0.0;
            }
            ++conditionalDepth_;
            struct ConditionalGuard {
                int& depth;
                ~ConditionalGuard() { --depth; }
            } conditionalGuard{conditionalDepth_};
            return parseIfStatement();
        }
        if (peek() != '(') { ok_ = false; return 0.0; }
        ++pos_;
        const bool isMultiEntryOperator = name == "min" || name == "max";
        if (isMultiEntryOperator && multiEntryDepth_ > 0) {
            ok_ = false;
            return 0.0;
        }
        const bool isConditionalOperator = name == "if_then_else";
        if (isConditionalOperator && conditionalDepth_ > 0) {
            ok_ = false;
            return 0.0;
        }
        struct MultiEntryGuard {
            int& depth;
            bool active;
            ~MultiEntryGuard() {
                if (active) --depth;
            }
        };
        struct ConditionalGuard {
            int& depth;
            bool active;
            ~ConditionalGuard() {
                if (active) --depth;
            }
        };
        if (isMultiEntryOperator) ++multiEntryDepth_;
        if (isConditionalOperator) ++conditionalDepth_;
        MultiEntryGuard multiEntryGuard{multiEntryDepth_, isMultiEntryOperator};
        ConditionalGuard conditionalGuard{conditionalDepth_, isConditionalOperator};
        if (isConditionalOperator) {
            return parseIfThenElseFunction();
        }
        std::vector<double> args;
        args.push_back(parseComparison());
        while (ok_ && peek() == ',') {
            ++pos_;
            args.push_back(parseComparison());
        }
        if (peek() != ')') { ok_ = false; return 0.0; }
        ++pos_;
        if (args.empty()) { ok_ = false; return 0.0; }
        const double arg = args[0];
        auto requireUnary = [&]() -> bool {
            if (args.size() != 1) { ok_ = false; return false; }
            return true;
        };
        if (name == "min" || name == "max") {
            double best = args[0];
            for (std::size_t i = 1; i < args.size(); ++i) {
                best = name == "min" ? std::min(best, args[i]) : std::max(best, args[i]);
            }
            return best;
        }
        if (name == "pow") {
            if (args.size() != 2) {
                ok_ = false;
                return 0.0;
            }
            return std::pow(args[0], args[1]);
        }
        if (name == "mod") {
            if (args.size() != 2 || std::fabs(args[1]) < kEps) {
                ok_ = false;
                return 0.0;
            }
            return std::fmod(args[0], args[1]);
        }
        if (!requireUnary()) return 0.0;
        if (name == "sqrt") return arg < 0.0 ? (ok_ = false, 0.0) : std::sqrt(arg);
        if (name == "sin") return inTwoPiRange(arg) ? std::sin(arg) : (ok_ = false, 0.0);
        if (name == "cos") return inHalfPiRange(arg) ? std::cos(arg) : (ok_ = false, 0.0);
        if (name == "tan") return inHalfPiRange(arg) ? std::tan(arg) : (ok_ = false, 0.0);
        if (name == "arcsin" || name == "arcsine" || name == "asin") {
            return (arg < -1.0 || arg > 1.0) ? (ok_ = false, 0.0) : std::asin(arg);
        }
        if (name == "arccos" || name == "arccosine" || name == "acos") {
            return (arg < -1.0 || arg > 1.0) ? (ok_ = false, 0.0) : std::acos(arg);
        }
        if (name == "arctan" || name == "arctangent" || name == "atan") return std::atan(arg);
        if (name == "abs") return std::fabs(arg);
        if (name == "log") return arg <= 0.0 ? (ok_ = false, 0.0) : std::log(arg);
        if (name == "exp") return std::exp(arg);
        if (name == "roundup") return std::ceil(arg);
        if (name == "rounddown") return std::floor(arg);
        if (name == "round") return std::round(arg);
        if (name == "deg2rad") return (arg / 180.0) * kPi;
        if (name == "rad2deg") return rad2deg(arg);
        if (name == "ang2pos") return angleToPositiveDegrees(arg);
        if (name == "ang2neg") return angleToSignedDegrees(arg);
        if (name == "mm2inch") return arg / 25.4;
        if (name == "inch2mm") return arg * 25.4;
        ok_ = false;  // unsupported function
        return 0.0;
    }
    // variable := '[' KEYWORD (':' index)? ']' — supports PnX/PnY/PnZ and VAL.
    double parseVariable() {
        ++pos_;  // consume '['
        std::size_t start = pos_;
        while (pos_ < src_.size() && src_[pos_] != ']') ++pos_;
        if (pos_ == src_.size()) { ok_ = false; return 0.0; }
        std::string token = src_.substr(start, pos_ - start);
        ++pos_;  // consume ']'

        std::string key = token;
        long index = 0;
        const std::size_t colon = token.find(':');
        if (colon != std::string::npos) {
            key = token.substr(0, colon);
            if (!parsePositiveIndex(token.substr(colon + 1), index)) {
                ok_ = false;
                return 0.0;
            }
        }
        if (key == "VAL") {
            if (colon == std::string::npos) return val_;  // legacy constant operand (def.scale)
            if (index <= 0) { ok_ = false; return 0.0; }
            const std::size_t valueIndex = static_cast<std::size_t>(index - 1);
            if (valueIndex < valueList_.size()) return valueList_[valueIndex];
            ok_ = false;
            return 0.0;
        }
        if (key == "MS") {
            if (index <= 0) { ok_ = false; return 0.0; }
            const std::size_t measureIndex = static_cast<std::size_t>(index - 1);
            if (measureIndex < measureValues_.size()) return measureValues_[measureIndex];
            ok_ = false;
            return 0.0;
        }
        if (key == "STR") {
            if (index <= 0) { ok_ = false; return 0.0; }
            const std::size_t stringIndex = static_cast<std::size_t>(index - 1);
            if (stringIndex < stringValues_.size()) return stringValues_[stringIndex];
            ok_ = false;
            return 0.0;
        }
        if (key.size() == 3 && key[0] == 'D' && key[1] == 'R') {
            if (colon == std::string::npos || index != 1) { ok_ = false; return 0.0; }
            const char axis = key[2];
            if (axis == 'I') return measureDirection_.x;
            if (axis == 'J') return measureDirection_.y;
            if (axis == 'K') return measureDirection_.z;
        }
        // Point variable: P1/P2 plus axis in {X,Y,Z,C,I,J,K}; ':index'
        // uses the documented 1-based index within each group. In this compact
        // model inputPoints stores Group1 then Group2, so P2:1 maps to slot 1.
        if (key.size() >= 3 && key[0] == 'P') {
            const char axis = key.back();
            const std::string group = key.substr(1, key.size() - 2);
            if (group != "1" && group != "2") {
                ok_ = false;
                return 0.0;
            }
            std::size_t pointIndex = static_cast<std::size_t>(index);
            if (colon == std::string::npos) {
                pointIndex = group == "1" ? 0u : 1u;
            } else {
                if (index <= 0) {
                    ok_ = false;
                    return 0.0;
                }
                if (group == "1") {
                    pointIndex = static_cast<std::size_t>(index - 1);
                } else if (group == "2") {
                    pointIndex = static_cast<std::size_t>(index);
                }
            }
            if (pointIndex < points_.size()) {
                const Vec3& p = points_[pointIndex];
                if (axis == 'X') return p.x;
                if (axis == 'Y') return p.y;
                if (axis == 'Z') return p.z;
                if (axis == 'C') {
                    if (pointIndex < pointDiameters_.size()) return 0.5 * pointDiameters_[pointIndex];
                    ok_ = false;
                    return 0.0;
                }
                if (axis == 'I' || axis == 'J' || axis == 'K') {
                    if (pointIndex >= pointDirections_.size()) {
                        ok_ = false;
                        return 0.0;
                    }
                    const Vec3& dir = pointDirections_[pointIndex];
                    if (axis == 'I') return dir.x;
                    if (axis == 'J') return dir.y;
                    return dir.z;
                }
            }
        }
        ok_ = false;  // unknown / out-of-range keyword
        return 0.0;
    }

    const std::string& src_;
    const std::vector<Vec3>& points_;
    const std::vector<double>& pointDiameters_;
    const std::vector<Vec3>& pointDirections_;
    const Vec3& measureDirection_;
    const std::vector<double>& measureValues_;
    const std::vector<double>& stringValues_;
    const std::vector<double> valueList_;
    double val_{0.0};
    std::size_t pos_{0};
    bool ok_{true};
    int multiEntryDepth_{0};
    int conditionalDepth_{0};
};

}  // namespace

class ReferenceMeasureEvaluator final : public IMeasureEvaluator {
public:
    double evaluate(const MeasureDef& def, const BuildState& state) override {
        const auto* resolver = static_cast<const PointResolver*>(state.impl);
        if (!resolver) return 0.0;
        if (!std::isfinite(def.scale)) return 0.0;
        double value = 0.0;
        switch (def.type) {
            case MeasureType::NominalPoint: {
                if (def.inputPoints.size() < 1) break;
                const Vec3 cur = resolver->current(def.inputPoints[0]);
                const Vec3 nom = resolver->nominal(def.inputPoints[0]);
                value = applyMode(sub(cur, nom), def.direction.ijk, def.dirMode);
                break;
            }
            case MeasureType::PointPoint: {
                if (def.inputPoints.size() < 2) break;
                const Vec3 p1 = resolver->current(def.inputPoints[0]);
                const Vec3 p2 = resolver->current(def.inputPoints[1]);
                value = applyMode(sub(p2, p1), def.direction.ijk, def.dirMode);
                break;
            }
            case MeasureType::PointLine:
                value = evalPointLine(def, *resolver);
                break;
            case MeasureType::PointPlane:
                value = evalPointPlane(def, *resolver);
                break;
            case MeasureType::LineNominal:
            case MeasureType::LineLine:
            case MeasureType::LinePlane:
                value = evalLineAngle(def, *resolver);
                break;
            case MeasureType::PlaneNominal:
            case MeasureType::PlanePlane:
                value = evalPlaneAngle(def, *resolver);
                break;
            case MeasureType::TwoPointList:
                value = evalTwoPointList(def, *resolver);
                break;
            case MeasureType::Circularity:
                value = evalCircularity(def, *resolver);
                break;
            case MeasureType::DimensionalDistance:
                value = evalDimensionalDistance(def, *resolver);
                break;
            case MeasureType::CircleInterference:
                value = evalCircleInterference(def, *resolver);
                break;
            case MeasureType::VirtualClearance:
                value = evalVirtualClearance(def, *resolver);
                break;
            case MeasureType::FeatureMeasure:
                value = evalFeatureMeasure(def, *resolver);
                break;
            case MeasureType::FeatureAngle:
                value = evalFeatureAngle(def, *resolver);
                break;
            case MeasureType::Combination:
                value = evalCombination(def, *resolver);
                break;
            case MeasureType::Equation:
                // The equation evaluator already folds in def.scale through the
                // [VAL] keyword, so return the raw result without re-scaling.
                return evalEquation(def, *resolver);
            case MeasureType::CircleDiameter:
                value = evalCircleDiameter(def, *resolver);
                break;
            case MeasureType::GdtPosition:
                value = evalGdtPosition(def, *resolver);
                break;
            case MeasureType::GdtSurfaceProfile:
                value = evalGdtSurfaceProfile(def, *resolver);
                break;
            case MeasureType::GdtPerpendicularity:
            case MeasureType::GdtAngularity:
            case MeasureType::GdtParallelism:
                value = evalGdtOrientation(def, *resolver);
                break;
            case MeasureType::GdtConcentricity:
                value = evalGdtConcentricity(def, *resolver);
                break;
            case MeasureType::UserDll:
                value = resolver->userDllMeasure(def);
                break;
            default:
                value = 0.0;
                break;
        }
        if (!std::isfinite(value)) return 0.0;
        const double scaled = value * def.scale;
        return std::isfinite(scaled) ? scaled : 0.0;
    }

private:
    // Point-Line (3 points = 1 measured + 2 line). True = perpendicular distance.
    // Projected-on-Plane projects the point and the line into the plane normal
    // to def.direction.ijk, then measures the in-plane perpendicular distance.
    static double evalPointLine(const MeasureDef& def, const PointResolver& r) {
        if (def.inputPoints.size() < 3) return 0.0;
        const Vec3 p = r.current(def.inputPoints[0]);
        const Vec3 a = r.current(def.inputPoints[1]);
        const Vec3 b = r.current(def.inputPoints[2]);
        if (def.dirMode == DirectionMode::ProjectedOnPlane) {
            return projectedPointLineDistance(p, a, b, def.direction.ijk);
        }
        return pointLineDistance(p, a, b);  // True Distance (non-negative)
    }

    // Point-Plane (4 points = 1 measured + 3 plane). True = |(P - Q) . n|.
    // Projected-on-Vector = (P - Q) . d (signed); reverse of the point-to-plane
    // vector relative to d gives a negative value (README §6.2d).
    static double evalPointPlane(const MeasureDef& def, const PointResolver& r) {
        if (def.inputPoints.size() < 4) return 0.0;
        const Vec3 p = r.current(def.inputPoints[0]);
        const Vec3 q0 = r.current(def.inputPoints[1]);
        const Vec3 q1 = r.current(def.inputPoints[2]);
        const Vec3 q2 = r.current(def.inputPoints[3]);
        const Vec3 n = planeNormal(q0, q1, q2);
        const Vec3 delta = sub(p, q0);
        if (def.dirMode == DirectionMode::ProjectedOnVector) {
            return stableDotForDirection(delta, unitOrZero(def.direction.ijk));  // signed along d
        }
        return std::fabs(dot(delta, n));  // True perpendicular distance
    }

    // Line angle (degrees). LineNominal: 2 points define the line, compared to
    // def.direction.ijk. LineLine: 4 points (two lines). LinePlane: 5 points
    // (2 line + 3 plane); returns the minimum angle between line and plane.
    static double evalLineAngle(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        if (def.type == MeasureType::LineNominal) {
            if (pts.size() < 2) return 0.0;
            const Vec3 line = sub(r.current(pts[1]), r.current(pts[0]));
            return vectorAngleDeg(line, def.direction.ijk);
        }
        if (def.type == MeasureType::LineLine) {
            if (pts.size() < 4) return 0.0;
            const Vec3 l1 = sub(r.current(pts[1]), r.current(pts[0]));
            const Vec3 l2 = sub(r.current(pts[3]), r.current(pts[2]));
            if (def.dirMode == DirectionMode::ProjectedOnPlane) {
                return signedProjectedAngleDeg(l1, l2, def.direction.ijk);
            }
            return vectorAngleDeg(l1, l2);
        }
        // LinePlane: minimum angle between line and plane = 90 - angle(line, n).
        if (pts.size() < 5) return 0.0;
        const Vec3 line = sub(r.current(pts[1]), r.current(pts[0]));
        const Vec3 n = planeNormal(r.current(pts[2]), r.current(pts[3]), r.current(pts[4]));
        if (len(line) < kEps || len(n) < kEps) return 0.0;
        const double toNormal = vectorAngleDeg(line, n);
        // Fold to [0, 90] so the line-plane angle stays in [0, 90].
        const double folded = toNormal > 90.0 ? 180.0 - toNormal : toNormal;
        return 90.0 - folded;
    }

    // Plane angle (degrees) = angle between plane normals. PlaneNominal: 1 plane
    // (3 points) vs def.direction.ijk (treated as the reference normal).
    // PlanePlane: 2 planes (6 points).
    static double evalPlaneAngle(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        if (def.type == MeasureType::PlaneNominal) {
            if (pts.size() < 3) return 0.0;
            const Vec3 n = planeNormal(r.current(pts[0]), r.current(pts[1]), r.current(pts[2]));
            return planeAngleDeg(n, def.direction.ijk);
        }
        // PlanePlane.
        if (pts.size() < 6) return 0.0;
        const Vec3 n1 = planeNormal(r.current(pts[0]), r.current(pts[1]), r.current(pts[2]));
        const Vec3 n2 = planeNormal(r.current(pts[3]), r.current(pts[4]), r.current(pts[5]));
        if (def.dirMode == DirectionMode::ProjectedOnPlane) {
            return signedProjectedAngleDeg(n1, n2, def.direction.ijk);
        }
        return planeAngleDeg(n1, n2);
    }

    // Two Point List (README §6.6). inputPoints splits in half: the first half is
    // group1, the second half is group2. The aggregate sub-type is selected via
    // def.equation (otherwise unused for this type): "all_min", "all_max",
    // "pair_min", "pair_max", "pair_max_min", or "center_deviate". Empty /
    // unknown defaults to "pair_max".
    // Each pair distance honours def.dirMode (True or Projected on Vector).
    static double evalTwoPointList(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        const std::size_t n = pts.size() / 2;
        if (n == 0 || pts.size() % 2 != 0) return 0.0;

        auto pairValue = [&](const Vec3& a, const Vec3& b) -> double {
            return applyMode(sub(b, a), def.direction.ijk, def.dirMode);
        };

        const std::string& mode = def.equation;
        if (mode == "pair_max_min") {
            double minValue = std::numeric_limits<double>::max();
            double maxValue = std::numeric_limits<double>::lowest();
            for (std::size_t i = 0; i < n; ++i) {
                const double v = pairValue(r.current(pts[i]), r.current(pts[n + i]));
                minValue = std::min(minValue, v);
                maxValue = std::max(maxValue, v);
            }
            return maxValue - minValue;
        }

        if (mode == "center_deviate") {
            const Vec3 dir = unitOrZero(def.direction.ijk);
            if (len(dir) < kEps) return 0.0;
            double total = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                const Vec3 curMid = midpoint(r.current(pts[i]), r.current(pts[n + i]));
                const Vec3 nomMid = midpoint(r.nominal(pts[i]), r.nominal(pts[n + i]));
                total += dot(sub(curMid, nomMid), dir);
            }
            return total / static_cast<double>(n);
        }

        const bool wantAll = mode.rfind("all", 0) == 0;
        const bool wantMin = mode.find("min") != std::string::npos;

        double best = wantMin ? std::numeric_limits<double>::max()
                              : std::numeric_limits<double>::lowest();
        auto fold = [&](double v) { best = wantMin ? std::min(best, v) : std::max(best, v); };

        if (wantAll) {
            // All Points Min/Max: every cross-pair between the two groups.
            const std::size_t g2 = pts.size() - n;  // size of group2
            for (std::size_t i = 0; i < n; ++i) {
                const Vec3 a = r.current(pts[i]);
                for (std::size_t j = 0; j < g2; ++j) {
                    fold(pairValue(a, r.current(pts[n + j])));
                }
            }
        } else {
            // Pair Points Min/Max: same-index pairs group1[i] <-> group2[i].
            for (std::size_t i = 0; i < n; ++i) {
                fold(pairValue(r.current(pts[i]), r.current(pts[n + i])));
            }
        }
        return best;
    }

    // Circularity (README §6.3): project the points onto the plane normal to
    // def.direction.ijk, fit the circle centre (centroid of projected points),
    // then circularity = max radius - min radius (outer minus inner radius).
    static double evalCircularity(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        if (pts.size() < 4) return 0.0;
        const Vec3 n = unitOrZero(def.direction.ijk);
        if (len(n) < kEps) return 0.0;

        // Project all points into the plane and accumulate a centroid centre.
        std::vector<Vec3> proj;
        proj.reserve(pts.size());
        for (PointId id : pts) {
            const Vec3 p = r.current(id);
            const Vec3 pp = sub(p, scale(n, dot(p, n)));  // drop the normal component
            proj.push_back(pp);
        }
        const Vec3 centroid = centroidOf(proj);

        double rMin = std::numeric_limits<double>::max();
        double rMax = std::numeric_limits<double>::lowest();
        for (const Vec3& pp : proj) {
            const double rad = len(sub(pp, centroid));
            rMin = std::min(rMin, rad);
            rMax = std::max(rMax, rad);
        }
        return rMax - rMin;  // outer radius - inner radius
    }

    // Circle Diameter (README §6.3): report the feature-of-size diameter carried
    // by the measured circle point. Missing size data is represented as 0 by the
    // resolver, preserving the reference build's fallback behaviour.
    static double evalCircleDiameter(const MeasureDef& def, const PointResolver& r) {
        if (def.inputPoints.empty()) return 0.0;
        return r.diameter(def.inputPoints[0]);
    }

    // Dimensional Distance (README §6.3). PointResolver only exposes point
    // positions (no CAD surfaces), so the two features are approximated by two
    // point groups: the first half of inputPoints is group1, the second half is
    // group2. The sub-mode is selected via def.equation ("center"/"min"/"max",
    // default "center"):
    //   center  -> distance between the two group centroids,
    //   min/max -> extreme cross-pair distance between the groups.
    // The result honours def.dirMode (True / Projected on Vector / on Plane).
    static double evalDimensionalDistance(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        const std::size_t n = pts.size() / 2;
        if (n == 0 || pts.size() % 2 != 0) return 0.0;
        const std::size_t g2 = pts.size() - n;

        std::vector<Vec3> a, b;
        a.reserve(n);
        b.reserve(g2);
        for (std::size_t i = 0; i < n; ++i) a.push_back(r.current(pts[i]));
        for (std::size_t j = 0; j < g2; ++j) b.push_back(r.current(pts[n + j]));

        const std::string& mode = def.equation;
        if (mode == "min" || mode == "max") {
            const bool wantMin = mode == "min";
            double best = wantMin ? std::numeric_limits<double>::max()
                                  : std::numeric_limits<double>::lowest();
            for (const Vec3& pa : a) {
                for (const Vec3& pb : b) {
                    const double d = applyMode(sub(pb, pa), def.direction.ijk, def.dirMode);
                    best = wantMin ? std::min(best, d) : std::max(best, d);
                }
            }
            return best;
        }
        // Default: centre distance between the two group centroids.
        return applyMode(sub(centroidOf(b), centroidOf(a)), def.direction.ijk, def.dirMode);
    }

    // Circle Interference (README §6.3): assembly clearance of hole/pin pairs.
    // inputPoints are taken pairwise (Objects/Targets): each consecutive pair is
    // one assembly, clearance = centre distance - radius sum. Sub-mode in
    // def.equation: default/min -> minimum clearance, max -> maximum clearance,
    // interference -> count assemblies whose clearance is negative.
    static double evalCircleInterference(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        const std::size_t pairs = pts.size() / 2;
        if (pairs == 0 || pts.size() % 2 != 0) return 0.0;
        double minClearance = std::numeric_limits<double>::max();
        double maxClearance = std::numeric_limits<double>::lowest();
        int interferenceCount = 0;
        for (std::size_t i = 0; i < pairs; ++i) {
            const PointId objId = pts[2 * i];
            const PointId tgtId = pts[2 * i + 1];
            const Vec3 obj = r.current(objId);
            const Vec3 tgt = r.current(tgtId);
            const double centreDist = len(sub(tgt, obj));
            const double radiusSum = 0.5 * r.diameter(objId) + 0.5 * r.diameter(tgtId);
            const double clearance = centreDist - radiusSum;
            minClearance = std::min(minClearance, clearance);
            maxClearance = std::max(maxClearance, clearance);
            if (clearance < 0.0) ++interferenceCount;
        }
        if (def.equation == "max") return maxClearance;
        if (def.equation == "interference") return static_cast<double>(interferenceCount);
        return minClearance;
    }

    // Virtual Clearance (README §6.3): largest bolt through a hole group / the
    // smallest tube around a pin group. PointResolver gives no size data, so the
    // FoS is approximated by the bounding geometry of the centre points projected
    // onto the plane perpendicular to def.direction.ijk (the required axis):
    //   Inner Diameter (default) -> max inscribed circle diameter that fits among
    //     the points = 2 * min radius from the projected centroid,
    //   Outer Diameter (def.equation == "outer") -> min circumscribed circle
    //     diameter that encloses the points = 2 * max radius from the centroid.
    //
    // If active diameters are available, the projected centre offset is combined
    // with each feature radius. Without size data, preserve the centre-cloud
    // fallback used by the reference model.
    static double evalVirtualClearance(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        if (pts.size() < 2) return 0.0;
        const Vec3 n = unitOrZero(def.direction.ijk);
        if (len(n) < kEps) return 0.0;

        std::vector<Vec3> proj;
        proj.reserve(pts.size());
        for (PointId id : pts) proj.push_back(dropNormal(r.current(id), n));
        const Vec3 centroid = centroidOf(proj);

        const bool outer = def.equation == "outer";
        bool hasSize = false;
        double sizedBest = outer ? std::numeric_limits<double>::lowest()
                                 : std::numeric_limits<double>::max();
        double rMin = std::numeric_limits<double>::max();
        double rMax = std::numeric_limits<double>::lowest();
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const double rad = len(sub(proj[i], centroid));
            rMin = std::min(rMin, rad);
            rMax = std::max(rMax, rad);
            const double pointRadius = 0.5 * r.diameter(pts[i]);
            if (pointRadius > 0.0) {
                hasSize = true;
                const double candidate = outer ? pointRadius + rad : pointRadius - rad;
                sizedBest = outer ? std::max(sizedBest, candidate)
                                  : std::min(sizedBest, candidate);
            }
        }
        if (hasSize) return 2.0 * sizedBest;
        return outer ? 2.0 * rMax : 2.0 * rMin;
    }

    // Feature Measure (README §6.4), Feature-Feature mode. Each feature is a set
    // of mesh nodes approximated by points: the first half of inputPoints is
    // group1, the second half is group2. Reports the minimum point-to-point
    // distance across the groups (smallest gap; would be negative for an exact
    // interference model, which needs surface orientation not available here).
    static double evalFeatureMeasure(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        const std::size_t n = pts.size() / 2;
        if (n == 0 || pts.size() % 2 != 0) return 0.0;
        const std::size_t g2 = pts.size() - n;

        double best = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < n; ++i) {
            const Vec3 a = r.current(pts[i]);
            for (std::size_t j = 0; j < g2; ++j) {
                best = std::min(best, len(sub(r.current(pts[n + j]), a)));
            }
        }
        return best;
    }

    // Feature Angle (3DCS Feature Angle measure): approximate the two picked
    // feature vectors from point pairs and report their included angle in
    // degrees. Full CAD-surface vectors are resolved upstream into these point
    // inputs in the reference build.
    static double evalFeatureAngle(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        if (pts.size() < 4) return 0.0;
        const Vec3 v1 = sub(r.current(pts[1]), r.current(pts[0]));
        const Vec3 v2 = sub(r.current(pts[3]), r.current(pts[2]));
        const double angle = vectorAngleDeg(v1, v2);
        const Vec3 dir = unitOrZero(def.direction.ijk);
        if (len(dir) < kEps) return 0.0;
        const double handedness = dot(cross(v1, v2), dir);
        return handedness < -kEps ? -angle : angle;
    }

    // Combination (README §6.6): Sum / Subtract / Max / Min over other active
    // measures. When inputFeatures is populated, this reference build treats
    // those ids as MeasureIds and reads their already-evaluated values from the
    // resolver. Older tests/models that only provide inputPoints keep the
    // degraded point-X fallback for compatibility.
    static double evalCombination(const MeasureDef& def, const PointResolver& r) {
        const std::string& op = def.equation;

        if (!def.inputFeatures.empty()) {
            auto valueAt = [&](std::size_t i) {
                return r.measureValue(static_cast<MeasureId>(def.inputFeatures[i]));
            };
            if (op == "in_spec") {
                double passed = 0.0;
                for (FeatureId id : def.inputFeatures) {
                    const auto measureId = static_cast<MeasureId>(id);
                    const double value = r.measureValue(measureId);
                    const SpecLimits spec = r.measureSpec(measureId);
                    const bool aboveLsl = !spec.lslActive || value >= spec.lsl;
                    const bool belowUsl = !spec.uslActive || value <= spec.usl;
                    if (aboveLsl && belowUsl) passed += 1.0;
                }
                return passed / static_cast<double>(def.inputFeatures.size());
            }
            if (op == "max" || op == "min") {
                const bool wantMin = op == "min";
                double best = valueAt(0);
                for (std::size_t i = 1; i < def.inputFeatures.size(); ++i) {
                    const double v = valueAt(i);
                    best = wantMin ? std::min(best, v) : std::max(best, v);
                }
                return best;
            }
            const bool subtract = op == "subtract";
            double acc = valueAt(0);
            for (std::size_t i = 1; i < def.inputFeatures.size(); ++i) {
                const double v = valueAt(i);
                acc += subtract ? -v : v;
            }
            return acc;
        }

        const auto& pts = def.inputPoints;
        if (op == "in_spec") return 0.0;
        if (pts.empty()) return 0.0;
        if (op == "max" || op == "min") {
            const bool wantMin = op == "min";
            double best = wantMin ? std::numeric_limits<double>::max()
                                  : std::numeric_limits<double>::lowest();
            for (PointId id : pts) {
                const double v = r.current(id).x;
                best = wantMin ? std::min(best, v) : std::max(best, v);
            }
            return best;
        }
        // Sum (default) and Subtract both accumulate; subtract negates operands
        // after the first.
        const bool subtract = op == "subtract";
        double acc = r.current(pts[0]).x;
        for (std::size_t i = 1; i < pts.size(); ++i) {
            const double v = r.current(pts[i]).x;
            acc += subtract ? -v : v;
        }
        return acc;
    }

    // GD&T zone selector: a diametrical (cylindrical) tolerance zone is the
    // default for axis/round features; a non-diametrical (planar) zone is
    // requested through def.equation == "non_diametrical".
    static bool isDiametrical(const MeasureDef& def) {
        return def.equation != "non_diametrical";
    }

    // GD&T Position (README §6.7). The measured feature is inputPoints[0]; its
    // current-vs-nominal deviation drives the result. def.direction.ijk is the
    // reference axis / zone normal (the DRF-derived direction). Diametrical:
    // sample the in-plane deviation along 0/45/90/135 degrees and report the
    // largest projection x2 (the diameter band). Non-diametrical: a single
    // Point Distance along def.direction.ijk. The optional remaining points are
    // the DRF reference; when none are given the point's own nominal is the
    // reference, which already yields the position deviation.
    static double evalGdtPosition(const MeasureDef& def, const PointResolver& r) {
        if (def.inputPoints.empty()) return 0.0;
        const PointId fp = def.inputPoints[0];
        const Vec3 cur = r.current(fp);
        const Vec3 nom = r.nominal(fp);
        return gdtPositionValue(sub(cur, nom), def.direction.ijk, isDiametrical(def));
    }

    // GD&T Surface Profile (README §6.7). Per measured point, the Point Distance
    // between the reference point (nominal) and the measured point (current),
    // measured along def.direction.ijk (the surface normal). Multiple points are
    // folded by reporting the largest absolute deviation (Recommended GD&T Value
    // is the worst point). The profile zone is non-diametrical (a signed band).
    static double evalGdtSurfaceProfile(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        if (pts.empty()) return 0.0;
        const Vec3 dir = unitOrZero(def.direction.ijk);
        if (len(dir) < kEps) return 0.0;
        double best = 0.0;
        bool first = true;
        for (PointId id : pts) {
            const Vec3 dev = sub(r.current(id), r.nominal(id));
            const double v = dot(dev, dir);  // signed
            if (first || std::fabs(v) > std::fabs(best)) {
                best = v;
                first = false;
            }
        }
        return best;
    }

    // GD&T orientation tolerances — Perpendicularity / Angularity / Parallelism
    // (README §6.7, shared routine; the three differ only in DRF/nominal
    // orientation semantics, which the caller bakes into the geometry, so the
    // numeric evaluation is identical here).
    //
    // Feature Locator Point (FLP) convention: the FLP is the nominal geometric
    // centre of the measured feature, taken as the nominal position of
    // inputPoints[0]. Orientation rotates the feature about the FLP, so the
    // locator deviation is zero and the feature ends/edges deviate the most.
    // The result is the largest orientation-induced offset of the remaining
    // feature points (axis ends / surface extremes) relative to the FLP.
    //
    // Diametrical: each end point contributes 4 in-plane directions
    // (0/45/90/135 degrees); two axis ends => 8 Point Distances. Non-diametrical:
    // the planar extreme points fold back to the FLP (a signed band). All extra
    // points after inputPoints[0] are treated as the feature ends / extremes.
    static double evalGdtOrientation(const MeasureDef& def, const PointResolver& r) {
        const auto& pts = def.inputPoints;
        if (pts.size() < 2) return 0.0;
        const Vec3 flp = r.nominal(pts[0]);  // Feature Locator Point (nominal centre).
        const bool diametrical = isDiametrical(def);

        double best = 0.0;
        for (std::size_t i = 1; i < pts.size(); ++i) {
            // Orientation offset = how far the end moved off its nominal arm,
            // i.e. the change of the (end - FLP) arm vector from nominal to
            // current. Pure translation of the whole feature cancels in this
            // difference, leaving the orientation (rotation about FLP) component.
            const Vec3 armCur = sub(r.current(pts[i]), flp);
            const Vec3 armNom = sub(r.nominal(pts[i]), flp);
            const Vec3 dev = sub(armCur, armNom);
            const double v = gdtPositionValue(dev, def.direction.ijk, diametrical);
            if (std::fabs(v) > std::fabs(best)) best = v;
        }
        return best;
    }

    // GD&T Concentricity (README §6.7). The measured feature centre
    // (inputPoints[0]) deviation from the coaxial DRF, sampled along 4
    // directions (0/45/90/135 degrees) in the plane perpendicular to the DRF
    // axis def.direction.ijk; report the largest projection x2 (diametrical
    // band). The reference is the point's nominal centre (coaxial DRF).
    static double evalGdtConcentricity(const MeasureDef& def, const PointResolver& r) {
        if (def.inputPoints.empty()) return 0.0;
        const PointId fp = def.inputPoints[0];
        const Vec3 dev = sub(r.current(fp), r.nominal(fp));
        // Concentricity is always a diametrical (cylindrical/coaxial) zone.
        return gdtPositionValue(dev, def.direction.ijk, /*diametrical=*/true);
    }

    static std::string trimCopy(const std::string& s) {
        std::size_t first = 0;
        while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) {
            ++first;
        }
        std::size_t last = s.size();
        while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) {
            --last;
        }
        return s.substr(first, last - first);
    }

    // Equation (README §6.6): evaluate def.equation with the minimal recursive
    // descent parser. Point variables [PnX:i]/[PnC:i] read inputPoints,
    // [MS:i] reads inputFeatures as a measure list, [STR:i] reads an earlier
    // newline-delimited string result, [VAL:i] reads the value list, and [VAL]
    // keeps the legacy scale constant compatibility path. On any
    // parse error the result is 0 (matching the 3DCS "invalid measure returns
    // 0" convention, README §3.6.3).
    static bool isEquationCommentLine(const std::string& line) {
        return line.size() >= 3 &&
               std::tolower(static_cast<unsigned char>(line[0])) == 'r' &&
               std::tolower(static_cast<unsigned char>(line[1])) == 'e' &&
               std::tolower(static_cast<unsigned char>(line[2])) == 'm';
    }

    static bool isEquationConfigLine(const std::string& line) {
        constexpr const char* kPrefix = "SETCFG=";
        return line.rfind(kPrefix, 0) == 0;
    }

    static bool isValidEquationConfigLine(const std::string& line) {
        if (!isEquationConfigLine(line)) return false;
        const std::string value = trimCopy(line.substr(7));
        return value == "NUMBERDATA" || value == "LENGTHDATA" || value == "ANGLEDATA" ||
               value == "AREADATA" || value == "VOLUMEDATA" || value == "FORCEDATA";
    }

    static double evalEquation(const MeasureDef& def, const PointResolver& r) {
        std::vector<Vec3> coords;
        coords.reserve(def.inputPoints.size());
        std::vector<double> pointDiameters;
        pointDiameters.reserve(def.inputPoints.size());
        std::vector<Vec3> pointDirections;
        pointDirections.reserve(def.inputPoints.size());
        for (PointId id : def.inputPoints) {
            coords.push_back(r.current(id));
            pointDiameters.push_back(r.diameter(id));
            pointDirections.push_back(r.direction(id));
        }
        std::vector<double> measureValues;
        measureValues.reserve(def.inputFeatures.size());
        for (FeatureId id : def.inputFeatures) {
            measureValues.push_back(r.measureValue(static_cast<MeasureId>(id)));
        }

        std::vector<double> stringValues;
        double lastValue = 0.0;
        bool evaluatedAny = false;
        bool lastStringWasComment = false;
        std::size_t start = 0;
        while (start <= def.equation.size()) {
            std::size_t end = def.equation.find('\n', start);
            if (end == std::string::npos) end = def.equation.size();
            std::string line = trimCopy(def.equation.substr(start, end - start));
            const bool isConfigLine = isEquationConfigLine(line);
            if (isConfigLine && !isValidEquationConfigLine(line)) return 0.0;
            if (isConfigLine) lastStringWasComment = false;
            if (!line.empty() && !isConfigLine) {
                lastStringWasComment = isEquationCommentLine(line);
            }
            if (line.size() > kMaxEquationStringLength) return 0.0;
            if (!line.empty() && !lastStringWasComment && !isConfigLine) {
                ExprParser parser(line, coords, pointDiameters, pointDirections, def.direction.ijk,
                                  measureValues, stringValues, def.values, def.scale);
                bool ok = false;
                lastValue = parser.parse(ok);
                if (!ok) return 0.0;
                stringValues.push_back(lastValue);
                evaluatedAny = true;
            }
            if (end == def.equation.size()) break;
            start = end + 1;
        }
        if (lastStringWasComment) return 0.0;
        return evaluatedAny ? lastValue : 0.0;
    }
};

std::unique_ptr<IMeasureEvaluator> makeReferenceMeasureEvaluator() {
    return std::make_unique<ReferenceMeasureEvaluator>();
}

}  // namespace opendva
