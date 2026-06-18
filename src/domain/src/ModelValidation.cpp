#include "opendva/domain/ModelValidation.h"

#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "opendva/dcs_plugin_api.h"
#include "opendva/plugin/PluginHost.h"

namespace opendva {
namespace {

void addIssue(std::vector<ModelIssue>& issues, ModelIssueSeverity severity,
              ModelIssueCategory category, std::string code, std::string message) {
    issues.push_back({severity, category, std::move(code), std::move(message)});
}

void addError(std::vector<ModelIssue>& issues, ModelIssueCategory category,
              std::string code, std::string message) {
    addIssue(issues, ModelIssueSeverity::Error, category, std::move(code),
             std::move(message));
}

void addWarning(std::vector<ModelIssue>& issues, ModelIssueCategory category,
                std::string code, std::string message) {
    addIssue(issues, ModelIssueSeverity::Warning, category, std::move(code),
             std::move(message));
}

std::unordered_set<PointId> collectPointIds(const Model& model) {
    std::unordered_set<PointId> ids;
    for (const Part& part : model.parts) {
        for (const Point& point : part.points) ids.insert(point.id);
    }
    return ids;
}

std::unordered_set<PointId> collectActivePointIds(const Model& model) {
    std::unordered_set<PointId> ids;
    for (const Part& part : model.parts) {
        for (const Point& point : part.points) {
            if (point.active) ids.insert(point.id);
        }
    }
    return ids;
}

std::unordered_set<PartId> collectPartIds(const Model& model) {
    std::unordered_set<PartId> ids;
    for (const Part& part : model.parts) ids.insert(part.id);
    return ids;
}

std::unordered_set<FeatureId> collectFeatureIds(const Model& model) {
    std::unordered_set<FeatureId> ids;
    for (const Part& part : model.parts) {
        for (const Feature& feature : part.features) ids.insert(feature.id);
    }
    return ids;
}

const Feature* findFeature(const Model& model, FeatureId id) {
    for (const Part& part : model.parts) {
        for (const Feature& feature : part.features) {
            if (feature.id == id) return &feature;
        }
    }
    return nullptr;
}

bool featureAngleHasUsableFeatureInputs(const Model& model,
                                        const MeasureDef& def) {
    if (def.type != MeasureType::FeatureAngle ||
        def.inputFeatures.size() < 2) {
        return false;
    }
    const Feature* first = findFeature(model, def.inputFeatures[0]);
    const Feature* second = findFeature(model, def.inputFeatures[1]);
    return first != nullptr && second != nullptr &&
           first->definingPoints.size() >= 2 &&
           second->definingPoints.size() >= 2;
}

std::unordered_set<ToleranceId> collectToleranceIds(const Model& model) {
    std::unordered_set<ToleranceId> ids;
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) ids.insert(tolerance.id);
    }
    return ids;
}

std::unordered_set<MoveId> collectMoveIds(const Model& model) {
    std::unordered_set<MoveId> ids;
    for (const MoveDef& move : model.moves) ids.insert(move.id);
    return ids;
}

std::unordered_set<MeasureId> collectMeasureIds(const Model& model) {
    std::unordered_set<MeasureId> ids;
    for (const MeasureRecord& measure : model.measures) ids.insert(measure.id);
    return ids;
}

std::unordered_set<MeasureId> collectActiveMeasureIds(const Model& model) {
    std::unordered_set<MeasureId> ids;
    for (const MeasureRecord& measure : model.measures) {
        if (measure.def.active) ids.insert(measure.id);
    }
    return ids;
}

std::unordered_map<MeasureId, std::size_t> collectMeasureOrder(const Model& model) {
    std::unordered_map<MeasureId, std::size_t> order;
    for (std::size_t i = 0; i < model.measures.size(); ++i) {
        order[model.measures[i].id] = i;
    }
    return order;
}

bool measureInputFeaturesAreMeasureRefs(MeasureType type) {
    return type == MeasureType::Combination || type == MeasureType::Equation;
}

std::size_t requiredMeasurePointCount(MeasureType type) {
    switch (type) {
        case MeasureType::NominalPoint:
        case MeasureType::CircleDiameter:
        case MeasureType::GdtPosition:
        case MeasureType::GdtSurfaceProfile:
        case MeasureType::GdtConcentricity:
            return 1;
        case MeasureType::PointPoint:
        case MeasureType::LineNominal:
        case MeasureType::TwoPointList:
        case MeasureType::DimensionalDistance:
        case MeasureType::CircleInterference:
        case MeasureType::VirtualClearance:
        case MeasureType::GdtPerpendicularity:
        case MeasureType::GdtAngularity:
        case MeasureType::GdtParallelism:
            return 2;
        case MeasureType::PointLine:
        case MeasureType::PlaneNominal:
            return 3;
        case MeasureType::PointPlane:
        case MeasureType::LineLine:
        case MeasureType::Circularity:
        case MeasureType::FeatureAngle:
            return 4;
        case MeasureType::LinePlane:
            return 5;
        case MeasureType::PlanePlane:
            return 6;
        case MeasureType::FeatureMeasure:
        case MeasureType::Combination:
        case MeasureType::Equation:
        case MeasureType::UserDll:
            return 0;
    }
    return 0;
}

bool equationHasOverlongLine(const std::string& equation) {
    constexpr std::size_t kMaxEquationLineLength = 400;
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        if (end - start > kMaxEquationLineLength) return true;
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

std::string trimCopy(const std::string& s) {
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

bool equationHasInvalidSetCfg(const std::string& equation) {
    constexpr const char* kPrefix = "SETCFG=";
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        bool hasSetCfgPrefix = line.size() >= 7;
        for (std::size_t i = 0; hasSetCfgPrefix && i < 7; ++i) {
            hasSetCfgPrefix =
                std::tolower(static_cast<unsigned char>(line[i])) ==
                std::tolower(static_cast<unsigned char>(kPrefix[i]));
        }
        if (hasSetCfgPrefix) {
            const std::string value = line.substr(7);
            if (line.rfind(kPrefix, 0) != 0 ||
                (value != "NUMBERDATA" && value != "LENGTHDATA" &&
                value != "ANGLEDATA" && value != "AREADATA" &&
                value != "VOLUMEDATA" && value != "FORCEDATA")) {
                return true;
            }
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool isEquationRemLine(const std::string& line) {
    return line.size() >= 3 &&
           (line[0] == 'r' || line[0] == 'R') &&
           (line[1] == 'e' || line[1] == 'E') &&
           (line[2] == 'm' || line[2] == 'M');
}

bool equationEndsWithRemLine(const std::string& equation) {
    std::string lastContentLine;
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        if (!line.empty()) {
            lastContentLine = line;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return isEquationRemLine(lastContentLine);
}

bool parsePositiveIndex(const std::string& text, std::size_t& out) {
    if (text.empty()) return false;
    std::size_t value = 0;
    for (const unsigned char c : text) {
        if (!std::isdigit(c)) return false;
        const std::size_t digit = static_cast<std::size_t>(c - '0');
        if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) return false;
        value = value * 10 + digit;
    }
    if (value == 0) return false;
    out = value;
    return true;
}

std::size_t equationMaxIndexedVariable(const std::string& equation,
                                        const std::string& variableKey) {
    std::size_t maxIndex = 0;
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return maxIndex;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key = colon == std::string::npos ? token : token.substr(0, colon);
        if (key == variableKey) {
            std::size_t index = 0;
            if (colon == std::string::npos || !parsePositiveIndex(token.substr(colon + 1), index)) {
                return std::numeric_limits<std::size_t>::max();
            }
            maxIndex = std::max(maxIndex, index);
        }
        pos = close + 1;
    }
    return maxIndex;
}

std::size_t equationRequiredPointVariableCount(const std::string& equation) {
    std::size_t requiredCount = 0;
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return requiredCount;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key = colon == std::string::npos ? token : token.substr(0, colon);
        if (key.size() >= 3 && key[0] == 'P') {
            const char axis = key.back();
            if (axis == 'X' || axis == 'Y' || axis == 'Z' ||
                axis == 'C' || axis == 'I' || axis == 'J' || axis == 'K') {
                const std::string group = key.substr(1, key.size() - 2);
                if (group == "1" || group == "2") {
                    std::size_t requiredIndex = group == "1" ? 1u : 2u;
                    if (colon != std::string::npos) {
                        std::size_t index = 0;
                        if (!parsePositiveIndex(token.substr(colon + 1), index)) {
                            return std::numeric_limits<std::size_t>::max();
                        }
                        requiredIndex = group == "1" ? index : index + 1;
                    }
                    requiredCount = std::max(requiredCount, requiredIndex);
                }
            }
        }
        pos = close + 1;
    }
    return requiredCount;
}

bool equationHasInvalidPointVariable(const std::string& equation) {
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return false;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key = colon == std::string::npos ? token : token.substr(0, colon);
        if (!key.empty() && key[0] == 'P') {
            if (key.size() < 3) return true;
            const char axis = key.back();
            const bool knownAxis = axis == 'X' || axis == 'Y' || axis == 'Z' ||
                                   axis == 'C' || axis == 'I' || axis == 'J' ||
                                   axis == 'K';
            const std::string group = key.substr(1, key.size() - 2);
            if ((group != "1" && group != "2") || !knownAxis) return true;
            if (colon != std::string::npos) {
                std::size_t index = 0;
                if (!parsePositiveIndex(token.substr(colon + 1), index)) return true;
            }
        }
        pos = close + 1;
    }
    return false;
}

bool equationHasInvalidDirectionVariable(const std::string& equation) {
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return false;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key = colon == std::string::npos ? token : token.substr(0, colon);
        if (key.size() == 3 && key[0] == 'D' && key[1] == 'R' &&
            (key[2] == 'I' || key[2] == 'J' || key[2] == 'K')) {
            std::size_t index = 0;
            if (colon == std::string::npos ||
                !parsePositiveIndex(token.substr(colon + 1), index) ||
                index != 1) {
                return true;
            }
        }
        pos = close + 1;
    }
    return false;
}

bool equationHasUnknownVariable(const std::string& equation) {
    std::size_t pos = 0;
    while ((pos = equation.find('[', pos)) != std::string::npos) {
        const std::size_t close = equation.find(']', pos + 1);
        if (close == std::string::npos) return false;
        const std::string token = equation.substr(pos + 1, close - pos - 1);
        const std::size_t colon = token.find(':');
        const std::string key = colon == std::string::npos ? token : token.substr(0, colon);
        const bool knownScalarList = key == "VAL" || key == "MS" || key == "STR";
        const bool pointVariable = !key.empty() && key[0] == 'P';
        const bool directionVariable = key.size() == 3 && key[0] == 'D' &&
                                       key[1] == 'R' &&
                                       (key[2] == 'I' || key[2] == 'J' ||
                                        key[2] == 'K');
        if (!knownScalarList && !pointVariable && !directionVariable) return true;
        pos = close + 1;
    }
    return false;
}

bool equationHasMalformedVariable(const std::string& equation) {
    std::size_t openCount = 0;
    for (const char c : equation) {
        if (c == '[') {
            ++openCount;
        } else if (c == ']') {
            if (openCount == 0) return true;
            --openCount;
        }
    }
    return openCount != 0;
}

bool startsWithEquationConditional(const std::string& line) {
    std::size_t pos = 0;
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
        ++pos;
    }
    std::string word;
    while (pos < line.size() &&
           (std::isalnum(static_cast<unsigned char>(line[pos])) ||
            line[pos] == '_')) {
        word.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(line[pos]))));
        ++pos;
    }
    return word == "if" || word == "if_then_else";
}

bool lineHasTopLevelComparison(const std::string& line) {
    int parenDepth = 0;
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
        } else if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
        } else if (bracketDepth == 0 && c == '(') {
            ++parenDepth;
        } else if (bracketDepth == 0 && c == ')' && parenDepth > 0) {
            --parenDepth;
        } else if (bracketDepth == 0 && parenDepth == 0) {
            if (i + 1 < line.size() &&
                ((c == '=' && line[i + 1] == '=') ||
                 (c == '<' && line[i + 1] == '=') ||
                 (c == '>' && line[i + 1] == '='))) {
                return true;
            }
            if (c == '=') return true;
            if (c == '<' || c == '>') return true;
        }
    }
    return false;
}

bool equationHasTopLevelComparison(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            !startsWithEquationConditional(line) &&
            lineHasTopLevelComparison(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasTrailingEquationOperator(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = line.size(); i > 0; --i) {
        const char c = line[i - 1];
        if (c == ']') {
            return false;
        }
        if (c == '[' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        return c == '+' || c == '-' || c == '*' || c == '/' ||
               c == '^' || c == ',';
    }
    return false;
}

bool equationHasTrailingOperator(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasTrailingEquationOperator(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasBareNegativeConstant(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
        } else if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
        }
        if (c != '-' || bracketDepth > 0 ||
            i + 1 >= line.size() ||
            (!std::isdigit(static_cast<unsigned char>(line[i + 1])) &&
             line[i + 1] != '.')) {
            continue;
        }
        std::size_t prev = i;
        while (prev > 0 &&
               std::isspace(static_cast<unsigned char>(line[prev - 1]))) {
            --prev;
        }
        if (prev == 0) return true;
        const char before = line[prev - 1];
        if (before == '+' || before == '-' || before == '*' ||
            before == '/' || before == '^' || before == ',') {
            return true;
        }
    }
    return false;
}

bool equationHasBareNegativeConstant(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasBareNegativeConstant(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasNonFiniteNumericLiteral(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0) continue;
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            while (i + 1 < line.size() &&
                   (std::isalnum(static_cast<unsigned char>(line[i + 1])) ||
                    line[i + 1] == '_')) {
                ++i;
            }
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(c)) &&
            !(c == '.' && i + 1 < line.size() &&
              std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            continue;
        }

        const std::size_t start = i;
        while (i < line.size() &&
               std::isdigit(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (i < line.size() && line[i] == '.') {
            ++i;
            while (i < line.size() &&
                   std::isdigit(static_cast<unsigned char>(line[i]))) {
                ++i;
            }
        }
        if (i < line.size() && (line[i] == 'e' || line[i] == 'E')) {
            const std::size_t exponentStart = i;
            ++i;
            if (i < line.size() && (line[i] == '+' || line[i] == '-')) ++i;
            const std::size_t digitStart = i;
            while (i < line.size() &&
                   std::isdigit(static_cast<unsigned char>(line[i]))) {
                ++i;
            }
            if (digitStart == i) i = exponentStart;
        }
        const std::string token = line.substr(start, i - start);
        try {
            std::size_t parsed = 0;
            const double value = std::stod(token, &parsed);
            if (parsed == token.size() && !std::isfinite(value)) return true;
        } catch (...) {
            return true;
        }
        if (i == 0) break;
        --i;
    }
    return false;
}

bool equationHasNonFiniteNumericLiteral(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNonFiniteNumericLiteral(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool parseEquationNumberAt(const std::string& line, std::size_t pos,
                           double& value, std::size_t& end) {
    if (pos >= line.size() ||
        (!std::isdigit(static_cast<unsigned char>(line[pos])) &&
         !(line[pos] == '.' && pos + 1 < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[pos + 1]))))) {
        return false;
    }
    std::size_t i = pos;
    while (i < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[i]))) {
        ++i;
    }
    if (i < line.size() && line[i] == '.') {
        ++i;
        while (i < line.size() &&
               std::isdigit(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
    }
    if (i < line.size() && (line[i] == 'e' || line[i] == 'E')) {
        const std::size_t exponentStart = i;
        ++i;
        if (i < line.size() && (line[i] == '+' || line[i] == '-')) ++i;
        const std::size_t digitStart = i;
        while (i < line.size() &&
               std::isdigit(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (digitStart == i) i = exponentStart;
    }
    const std::string token = line.substr(pos, i - pos);
    try {
        std::size_t parsed = 0;
        value = std::stod(token, &parsed);
        if (parsed != token.size()) return false;
    } catch (...) {
        value = std::numeric_limits<double>::infinity();
    }
    end = i;
    return true;
}

bool parseEquationLiteralAt(const std::string& line, std::size_t pos,
                            double& value, std::size_t& end) {
    if (parseEquationNumberAt(line, pos, value, end)) return true;
    if (pos >= line.size() || line[pos] != '(') return false;
    std::size_t inner = pos + 1;
    while (inner < line.size() &&
           std::isspace(static_cast<unsigned char>(line[inner]))) {
        ++inner;
    }
    double sign = 1.0;
    if (inner < line.size() && (line[inner] == '+' || line[inner] == '-')) {
        sign = line[inner] == '-' ? -1.0 : 1.0;
        ++inner;
        while (inner < line.size() &&
               std::isspace(static_cast<unsigned char>(line[inner]))) {
            ++inner;
        }
    }
    std::size_t valueEnd = 0;
    if (!parseEquationNumberAt(line, inner, value, valueEnd)) return false;
    std::size_t close = valueEnd;
    while (close < line.size() &&
           std::isspace(static_cast<unsigned char>(line[close]))) {
        ++close;
    }
    if (close >= line.size() || line[close] != ')') return false;
    value *= sign;
    end = close + 1;
    return true;
}

bool lineHasNonFiniteNumericArithmetic(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0) continue;
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            while (i + 1 < line.size() &&
                   (std::isalnum(static_cast<unsigned char>(line[i + 1])) ||
                    line[i + 1] == '_')) {
                ++i;
            }
            continue;
        }
        double left = 0.0;
        std::size_t leftEnd = 0;
        if (!parseEquationLiteralAt(line, i, left, leftEnd)) continue;
        std::size_t opPos = leftEnd;
        while (opPos < line.size() &&
               std::isspace(static_cast<unsigned char>(line[opPos]))) {
            ++opPos;
        }
        if (opPos >= line.size() ||
            (line[opPos] != '*' && line[opPos] != '/' &&
             line[opPos] != '+' && line[opPos] != '-')) {
            i = leftEnd == 0 ? i : leftEnd - 1;
            continue;
        }
        std::size_t rightStart = opPos + 1;
        while (rightStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[rightStart]))) {
            ++rightStart;
        }
        double right = 0.0;
        std::size_t rightEnd = 0;
        if (!parseEquationLiteralAt(line, rightStart, right, rightEnd)) {
            i = leftEnd == 0 ? i : leftEnd - 1;
            continue;
        }
        double result = 0.0;
        if (line[opPos] == '*') {
            result = left * right;
        } else if (line[opPos] == '/') {
            result = left / right;
        } else if (line[opPos] == '+') {
            result = left + right;
        } else {
            result = left - right;
        }
        if (!std::isfinite(result)) return true;
        i = rightEnd == 0 ? i : rightEnd - 1;
    }
    return false;
}

bool equationHasNonFiniteNumericArithmetic(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNonFiniteNumericArithmetic(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasNonFiniteNumericFunctionResult(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (lowerName != "pow" && lowerName != "exp" &&
            lowerName != "inch2mm" && lowerName != "rad2deg") {
            if (i == 0) break;
            --i;
            continue;
        }
        std::size_t argStart = i;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        if (argStart >= line.size() || line[argStart] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        ++argStart;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        double base = 0.0;
        std::size_t baseEnd = 0;
        if (!parseEquationLiteralAt(line, argStart, base, baseEnd)) {
            if (i == 0) break;
            --i;
            continue;
        }
        while (baseEnd < line.size() &&
               std::isspace(static_cast<unsigned char>(line[baseEnd]))) {
            ++baseEnd;
        }
        if (lowerName == "exp") {
            if (baseEnd >= line.size() || line[baseEnd] != ')') {
                if (i == 0) break;
                --i;
                continue;
            }
            if (!std::isfinite(std::exp(base))) return true;
            i = baseEnd;
            continue;
        }
        if (lowerName == "inch2mm" || lowerName == "rad2deg") {
            if (baseEnd >= line.size() || line[baseEnd] != ')') {
                if (i == 0) break;
                --i;
                continue;
            }
            const double result = lowerName == "inch2mm"
                                      ? base * 25.4
                                      : base / 3.14159265358979323846 * 180.0;
            if (!std::isfinite(result)) return true;
            i = baseEnd;
            continue;
        }
        if (baseEnd >= line.size() || line[baseEnd] != ',') {
            if (i == 0) break;
            --i;
            continue;
        }
        std::size_t exponentStart = baseEnd + 1;
        while (exponentStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[exponentStart]))) {
            ++exponentStart;
        }
        double exponent = 0.0;
        std::size_t exponentEnd = 0;
        if (!parseEquationLiteralAt(line, exponentStart, exponent, exponentEnd)) {
            if (i == 0) break;
            --i;
            continue;
        }
        while (exponentEnd < line.size() &&
               std::isspace(static_cast<unsigned char>(line[exponentEnd]))) {
            ++exponentEnd;
        }
        if (exponentEnd >= line.size() || line[exponentEnd] != ')') {
            if (i == 0) break;
            --i;
            continue;
        }
        if (!std::isfinite(std::pow(base, exponent))) return true;
        i = exponentEnd;
    }
    return false;
}

bool equationHasNonFiniteNumericFunctionResult(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNonFiniteNumericFunctionResult(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasOutOfRangeTrigLiteral(const std::string& line) {
    constexpr double kHalfPi = 1.5707963267948966;
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool isInverseTrig = lowerName == "asin" ||
                                   lowerName == "arcsin" ||
                                   lowerName == "arcsine" ||
                                   lowerName == "acos" ||
                                   lowerName == "arccos" ||
                                   lowerName == "arccosine";
        if (lowerName != "sin" && lowerName != "tan" &&
            lowerName != "cos" && !isInverseTrig) {
            if (i == 0) break;
            --i;
            continue;
        }
        std::size_t argStart = i;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        if (argStart >= line.size() || line[argStart] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        ++argStart;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        double value = 0.0;
        std::size_t argEnd = 0;
        if (!parseEquationLiteralAt(line, argStart, value, argEnd)) {
            if (i == 0) break;
            --i;
            continue;
        }
        while (argEnd < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argEnd]))) {
            ++argEnd;
        }
        if (argEnd >= line.size() || line[argEnd] != ')') {
            if (i == 0) break;
            --i;
            continue;
        }
        const double magnitude = std::fabs(value);
        if (isInverseTrig) {
            if (magnitude > 1.0) return true;
        } else if (lowerName == "cos" || lowerName == "tan") {
            if (magnitude >= kHalfPi) return true;
        } else if (magnitude > kHalfPi) {
            return true;
        }
        i = argEnd;
    }
    return false;
}

bool equationHasOutOfRangeTrigLiteral(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasOutOfRangeTrigLiteral(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasInvalidDomainFunctionLiteral(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (lowerName != "sqrt" && lowerName != "log" &&
            lowerName != "mod") {
            if (i == 0) break;
            --i;
            continue;
        }
        std::size_t argStart = i;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        if (argStart >= line.size() || line[argStart] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        ++argStart;
        while (argStart < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argStart]))) {
            ++argStart;
        }
        double value = 0.0;
        std::size_t argEnd = 0;
        if (!parseEquationLiteralAt(line, argStart, value, argEnd)) {
            if (i == 0) break;
            --i;
            continue;
        }
        while (argEnd < line.size() &&
               std::isspace(static_cast<unsigned char>(line[argEnd]))) {
            ++argEnd;
        }
        if (lowerName == "mod") {
            if (argEnd >= line.size() || line[argEnd] != ',') {
                if (i == 0) break;
                --i;
                continue;
            }
            std::size_t divisorStart = argEnd + 1;
            while (divisorStart < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[divisorStart]))) {
                ++divisorStart;
            }
            double divisor = 0.0;
            std::size_t divisorEnd = 0;
            if (!parseEquationLiteralAt(line, divisorStart, divisor, divisorEnd)) {
                if (i == 0) break;
                --i;
                continue;
            }
            while (divisorEnd < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[divisorEnd]))) {
                ++divisorEnd;
            }
            if (divisorEnd >= line.size() || line[divisorEnd] != ')') {
                if (i == 0) break;
                --i;
                continue;
            }
            if (std::fabs(divisor) < 1e-15) return true;
            i = divisorEnd;
            continue;
        }
        if (argEnd >= line.size() || line[argEnd] != ')') {
            if (i == 0) break;
            --i;
            continue;
        }
        if (lowerName == "sqrt") {
            if (value < 0.0) return true;
        } else if (value <= 0.0) {
            return true;
        }
        i = argEnd;
    }
    return false;
}

bool equationHasInvalidDomainFunctionLiteral(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasInvalidDomainFunctionLiteral(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

std::size_t requiredEquationFunctionArity(const std::string& lowerName) {
    if (lowerName == "pow" || lowerName == "mod") return 2;
    if (lowerName == "if_then_else") return 3;
    if (lowerName == "min" || lowerName == "max" ||
        lowerName == "if") {
        return 0;
    }
    if (lowerName == "sqrt" || lowerName == "sin" ||
        lowerName == "cos" || lowerName == "tan" ||
        lowerName == "arcsin" || lowerName == "arcsine" ||
        lowerName == "asin" || lowerName == "arccos" ||
        lowerName == "arccosine" || lowerName == "acos" ||
        lowerName == "arctan" || lowerName == "arctangent" ||
        lowerName == "atan" || lowerName == "abs" ||
        lowerName == "log" || lowerName == "exp" ||
        lowerName == "roundup" || lowerName == "rounddown" ||
        lowerName == "round" || lowerName == "deg2rad" ||
        lowerName == "rad2deg" || lowerName == "ang2pos" ||
        lowerName == "ang2neg" || lowerName == "mm2inch" ||
        lowerName == "inch2mm") {
        return 1;
    }
    return 0;
}

bool lineHasInvalidFunctionArity(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        const std::size_t nameStart = i;
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t open = i;
        while (open < line.size() &&
               std::isspace(static_cast<unsigned char>(line[open]))) {
            ++open;
        }
        if (open >= line.size() || line[open] != '(') {
            i = nameStart;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool isVariadicMinMax =
            lowerName == "min" || lowerName == "max";
        const std::size_t requiredArity =
            requiredEquationFunctionArity(lowerName);
        if (requiredArity == 0 && !isVariadicMinMax) {
            i = open;
            continue;
        }
        int parenDepth = 1;
        int variableDepth = 0;
        std::size_t commaCount = 0;
        bool hasArgumentToken = false;
        bool currentArgumentHasToken = false;
        bool hasEmptyArgumentSlot = false;
        std::size_t j = open + 1;
        for (; j < line.size(); ++j) {
            const char ch = line[j];
            if (ch == '[') {
                ++variableDepth;
                hasArgumentToken = true;
                currentArgumentHasToken = true;
                continue;
            }
            if (ch == ']' && variableDepth > 0) {
                --variableDepth;
                continue;
            }
            if (variableDepth > 0) continue;
            if (ch == '(') {
                ++parenDepth;
                hasArgumentToken = true;
                currentArgumentHasToken = true;
            } else if (ch == ')') {
                --parenDepth;
                if (parenDepth == 0) break;
            } else if (ch == ',' && parenDepth == 1) {
                if (!currentArgumentHasToken) hasEmptyArgumentSlot = true;
                ++commaCount;
                currentArgumentHasToken = false;
            } else if (!std::isspace(static_cast<unsigned char>(ch))) {
                hasArgumentToken = true;
                currentArgumentHasToken = true;
            }
        }
        if (j >= line.size() || parenDepth != 0) {
            i = open;
            continue;
        }
        if (hasArgumentToken && !currentArgumentHasToken) {
            hasEmptyArgumentSlot = true;
        }
        if (hasEmptyArgumentSlot) return true;
        const std::size_t actualArity = hasArgumentToken ? commaCount + 1 : 0;
        if (isVariadicMinMax) {
            if (actualArity == 0) return true;
            i = j;
            continue;
        }
        if (actualArity != requiredArity) return true;
        i = j;
    }
    return false;
}

bool equationHasInvalidFunctionArity(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasInvalidFunctionArity(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasInvalidMathOperatorCase(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
        } else if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        const std::size_t start = i;
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t afterName = i;
        while (afterName < line.size() &&
               std::isspace(static_cast<unsigned char>(line[afterName]))) {
            ++afterName;
        }
        if (afterName >= line.size() || line[afterName] != '(') {
            i = start;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool conditionalName =
            lowerName == "if" || lowerName == "if_then_else";
        if (rawName != lowerName && !conditionalName &&
            rawName != "MIN" && rawName != "MAX") {
            return true;
        }
        if (i == 0) break;
        --i;
    }
    return false;
}

bool isSupportedEquationFunction(const std::string& lowerName) {
    return lowerName == "if" || lowerName == "if_then_else" ||
           lowerName == "min" || lowerName == "max" ||
           lowerName == "pow" || lowerName == "mod" ||
           lowerName == "sqrt" || lowerName == "sin" ||
           lowerName == "cos" || lowerName == "tan" ||
           lowerName == "arcsin" || lowerName == "arcsine" ||
           lowerName == "asin" || lowerName == "arccos" ||
           lowerName == "arccosine" || lowerName == "acos" ||
           lowerName == "arctan" || lowerName == "arctangent" ||
           lowerName == "atan" || lowerName == "abs" ||
           lowerName == "log" || lowerName == "exp" ||
           lowerName == "roundup" || lowerName == "rounddown" ||
           lowerName == "round" || lowerName == "deg2rad" ||
           lowerName == "rad2deg" || lowerName == "ang2pos" ||
           lowerName == "ang2neg" || lowerName == "mm2inch" ||
           lowerName == "inch2mm";
}

bool lineHasUnsupportedMathOperator(const std::string& line) {
    int bracketDepth = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
        } else if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
        }
        if (bracketDepth > 0 ||
            !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            continue;
        }
        const std::size_t start = i;
        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t afterName = i;
        while (afterName < line.size() &&
               std::isspace(static_cast<unsigned char>(line[afterName]))) {
            ++afterName;
        }
        if (afterName >= line.size() || line[afterName] != '(') {
            i = start;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (!isSupportedEquationFunction(lowerName)) return true;
        if (i == 0) break;
        --i;
    }
    return false;
}

bool equationHasInvalidMathOperatorCase(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasInvalidMathOperatorCase(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool equationHasUnsupportedMathOperator(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasUnsupportedMathOperator(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasNestedMultiEntryOperator(const std::string& line) {
    int bracketDepth = 0;
    int multiEntryDepth = 0;
    std::vector<bool> parenMultiEntry;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0) continue;
        if (c == ')') {
            if (!parenMultiEntry.empty()) {
                if (parenMultiEntry.back() && multiEntryDepth > 0) {
                    --multiEntryDepth;
                }
                parenMultiEntry.pop_back();
            }
            continue;
        }
        if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            if (c == '(') parenMultiEntry.push_back(false);
            continue;
        }

        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t afterName = i;
        while (afterName < line.size() &&
               std::isspace(static_cast<unsigned char>(line[afterName]))) {
            ++afterName;
        }
        if (afterName >= line.size() || line[afterName] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool isMultiEntry = lowerName == "min" || lowerName == "max";
        if (isMultiEntry && multiEntryDepth > 0) return true;
        parenMultiEntry.push_back(isMultiEntry);
        if (isMultiEntry) ++multiEntryDepth;
        i = afterName;
    }
    return false;
}

bool equationHasNestedMultiEntryOperator(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNestedMultiEntryOperator(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool lineHasNestedConditionalOperator(const std::string& line) {
    int bracketDepth = 0;
    int conditionalDepth = 0;
    std::vector<bool> parenConditional;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '[') {
            ++bracketDepth;
            continue;
        }
        if (c == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (bracketDepth > 0) continue;
        if (c == ')') {
            if (!parenConditional.empty()) {
                if (parenConditional.back() && conditionalDepth > 0) {
                    --conditionalDepth;
                }
                parenConditional.pop_back();
            }
            continue;
        }
        if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
            if (c == '(') parenConditional.push_back(false);
            continue;
        }

        std::string rawName;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_')) {
            rawName.push_back(line[i]);
            ++i;
        }
        std::size_t afterName = i;
        while (afterName < line.size() &&
               std::isspace(static_cast<unsigned char>(line[afterName]))) {
            ++afterName;
        }
        if (afterName >= line.size() || line[afterName] != '(') {
            if (i == 0) break;
            --i;
            continue;
        }
        std::string lowerName = rawName;
        for (char& ch : lowerName) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        const bool isConditional =
            lowerName == "if" || lowerName == "if_then_else";
        if (isConditional && conditionalDepth > 0) return true;
        parenConditional.push_back(isConditional);
        if (isConditional) ++conditionalDepth;
        i = afterName;
    }
    return false;
}

bool equationHasNestedConditionalOperator(const std::string& equation) {
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine &&
            lineHasNestedConditionalOperator(line)) {
            return true;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

bool equationHasMissingStringReference(const std::string& equation) {
    std::size_t stringValueCount = 0;
    std::size_t start = 0;
    while (start <= equation.size()) {
        std::size_t end = equation.find('\n', start);
        if (end == std::string::npos) end = equation.size();
        const std::string line = trimCopy(equation.substr(start, end - start));
        const bool isConfigLine = line.rfind("SETCFG=", 0) == 0;
        const bool isCommentLine = !line.empty() && !isConfigLine &&
                                   isEquationRemLine(line);
        if (!line.empty() && !isConfigLine && !isCommentLine) {
            std::size_t pos = 0;
            while ((pos = line.find('[', pos)) != std::string::npos) {
                const std::size_t close = line.find(']', pos + 1);
                if (close == std::string::npos) break;
                const std::string token = line.substr(pos + 1, close - pos - 1);
                const std::size_t colon = token.find(':');
                const std::string key =
                    colon == std::string::npos ? token : token.substr(0, colon);
                if (key == "STR") {
                    std::size_t index = 0;
                    if (colon == std::string::npos ||
                        !parsePositiveIndex(token.substr(colon + 1), index) ||
                        index > stringValueCount) {
                        return true;
                    }
                }
                pos = close + 1;
            }
            ++stringValueCount;
        }
        if (end == equation.size()) break;
        start = end + 1;
    }
    return false;
}

std::unordered_set<FeatureId> collectReferencedFeatureIds(const Model& model) {
    std::unordered_set<FeatureId> ids;
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            for (const FeatureId featureId : tolerance.features) ids.insert(featureId);
        }
        for (const GdtDef& gdt : part.gdts) {
            for (const FeatureId featureId : gdt.features) ids.insert(featureId);
            if (gdt.drf.primary != kInvalidId) ids.insert(gdt.drf.primary);
            if (gdt.drf.secondary != kInvalidId) ids.insert(gdt.drf.secondary);
            if (gdt.drf.tertiary != kInvalidId) ids.insert(gdt.drf.tertiary);
        }
    }
    for (const MeasureRecord& measure : model.measures) {
        if (measureInputFeaturesAreMeasureRefs(measure.def.type)) continue;
        for (const FeatureId featureId : measure.def.inputFeatures) ids.insert(featureId);
    }
    return ids;
}

bool datumExists(const std::unordered_set<FeatureId>& featureIds, FeatureId id) {
    return id == kInvalidId || featureIds.find(id) != featureIds.end();
}

template <typename IdT>
bool hasDuplicateIds(const std::vector<IdT>& ids) {
    std::unordered_set<IdT> seen;
    for (const IdT id : ids) {
        if (!seen.insert(id).second) return true;
    }
    return false;
}

bool datumReferenceFrameHasDuplicateRefs(const DatumReferenceFrame& drf) {
    std::vector<FeatureId> datums;
    const FeatureId ids[] = {drf.primary, drf.secondary, drf.tertiary};
    for (const FeatureId id : ids) {
        if (id != kInvalidId) datums.push_back(id);
    }
    return hasDuplicateIds(datums);
}

bool datumReferenceFrameHasGap(const DatumReferenceFrame& drf) {
    return (drf.primary == kInvalidId &&
            (drf.secondary != kInvalidId || drf.tertiary != kInvalidId)) ||
           (drf.secondary == kInvalidId && drf.tertiary != kInvalidId);
}

bool gdtTypeRequiresDatum(GdtType type) {
    switch (type) {
        case GdtType::Position:
        case GdtType::Perpendicularity:
        case GdtType::Angularity:
        case GdtType::Parallelism:
        case GdtType::TotalRunout:
        case GdtType::CircularRunout:
        case GdtType::Concentricity:
        case GdtType::Symmetry:
            return true;
        case GdtType::Size:
        case GdtType::SurfaceProfile:
        case GdtType::Flatness:
        case GdtType::Straightness:
        case GdtType::Circularity:
        case GdtType::Cylindricity:
        case GdtType::LineProfile:
        case GdtType::DimensioningLocation:
        case GdtType::AngleSize:
        case GdtType::TorusMinorDiameterSize:
        case GdtType::SetFeatureAverage:
            return false;
    }
    return false;
}

bool directionHasMissingRefPoint(const Direction& direction,
                                 const std::unordered_set<PointId>& pointIds) {
    for (const PointId pointId : direction.refPoints) {
        if (pointIds.find(pointId) == pointIds.end()) return true;
    }
    return false;
}

bool directionHasWrongRefPointCount(const Direction& direction) {
    switch (direction.type) {
        case DirectionType::TwoPoints:
            return direction.refPoints.size() != 2;
        case DirectionType::Normal:
        case DirectionType::PickPtDir:
            return direction.refPoints.size() != 1;
        case DirectionType::TypeIn:
        case DirectionType::AssocDir:
        case DirectionType::Auto:
            return false;
    }
    return false;
}

bool twoPointDirectionHasDuplicateRefs(const Direction& direction) {
    return direction.type == DirectionType::TwoPoints &&
           direction.refPoints.size() == 2 &&
           direction.refPoints[0] == direction.refPoints[1];
}

bool directionHasInactiveRefPoint(const Direction& direction,
                                  const std::unordered_set<PointId>& activePointIds) {
    for (const PointId pointId : direction.refPoints) {
        if (activePointIds.find(pointId) == activePointIds.end()) return true;
    }
    return false;
}

bool featureHasInactiveDefiningPoint(const Model& model, FeatureId featureId,
                                     const std::unordered_set<PointId>& pointIds,
                                     const std::unordered_set<PointId>& activePointIds) {
    for (const Part& part : model.parts) {
        for (const Feature& feature : part.features) {
            if (feature.id != featureId) continue;
            for (const PointId pointId : feature.definingPoints) {
                if (pointIds.find(pointId) != pointIds.end() &&
                    activePointIds.find(pointId) == activePointIds.end()) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool datumHasInactiveDefiningPoint(const Model& model,
                                   const DatumReferenceFrame& drf,
                                   const std::unordered_set<PointId>& pointIds,
                                   const std::unordered_set<PointId>& activePointIds) {
    const FeatureId datums[] = {drf.primary, drf.secondary, drf.tertiary};
    for (const FeatureId datum : datums) {
        if (datum == kInvalidId) continue;
        if (featureHasInactiveDefiningPoint(model, datum, pointIds, activePointIds)) {
            return true;
        }
    }
    return false;
}

bool isFiniteVec3(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool isZeroVec3(const Vec3& v) {
    constexpr double kEps = 1e-15;
    return std::fabs(v.x) <= kEps &&
           std::fabs(v.y) <= kEps &&
           std::fabs(v.z) <= kEps;
}

bool hasBoundUserDllMeasureRoutine(const MeasureDef& def) {
    if (def.equation.empty()) return false;
    const auto* host = plugin::PluginHost::active();
    if (host == nullptr) return false;
    for (const auto& routine : host->routines()) {
        if (routine.name == def.equation && routine.type == dcsCalTypeMeas) {
            return true;
        }
    }
    return false;
}

bool hasBoundUserDllMoveRoutine(const MoveInputs& inputs) {
    if (inputs.userDllRoutine.empty()) return false;
    const auto* host = plugin::PluginHost::active();
    if (host == nullptr) return false;
    for (const auto& routine : host->routines()) {
        if (routine.name == inputs.userDllRoutine &&
            routine.type == dcsCalTypeMove) {
            return true;
        }
    }
    return false;
}

bool requiresUnsupportedMoveInfrastructure(const MoveInputs& inputs) {
    return (inputs.type == MoveType::UserDll &&
            !hasBoundUserDllMoveRoutine(inputs)) ||
           inputs.type == MoveType::AutoBend ||
           (inputs.type == MoveType::Iteration && !inputs.pairs.empty());
}

std::string unsupportedMoveInfrastructureMessage(MoveType type) {
    switch (type) {
        case MoveType::UserDll:
            return "Active UserDll move requires a bound plugin host.";
        case MoveType::Iteration:
            return "Active Iteration move is missing nested move-sequence infrastructure.";
        case MoveType::AutoBend:
            return "Active AutoBend move is missing bend/FEA infrastructure.";
        default:
            return "Active move type requires infrastructure that is not available in this reference build.";
    }
}

std::size_t requiredMovePairCount(MoveType type) {
    switch (type) {
        case MoveType::Transform:
        case MoveType::RTouch:
        case MoveType::Gravity:
            return 1;
        case MoveType::TwoPoint:
        case MoveType::CrossProduct:
        case MoveType::LeastSquaresAxis:
            return 2;
        case MoveType::StepPlane:
        case MoveType::SixPlane:
        case MoveType::ThreePoint:
        case MoveType::BestFit:
        case MoveType::FeatureMove:
        case MoveType::PatternRigid:
        case MoveType::PatternFit:
        case MoveType::RotateLine:
        case MoveType::LinePlane:
            return 3;
        case MoveType::Match:
            return 5;
        case MoveType::ThermalScaling:
        case MoveType::Iteration:
        case MoveType::UserDll:
        case MoveType::AutoBend:
            return 0;
    }
    return 0;
}

template <typename IdT>
void addDuplicateIdIssue(std::vector<ModelIssue>& issues, IdT id,
                         std::unordered_set<IdT>& seen, const std::string& code,
                         const std::string& message) {
    if (id == kInvalidId) return;
    if (!seen.insert(id).second) {
        addError(issues, ModelIssueCategory::Model, code, message);
    }
}

void addDuplicateIdIssues(const Model& model, std::vector<ModelIssue>& issues) {
    std::unordered_set<PartId> partIds;
    std::unordered_set<PointId> pointIds;
    std::unordered_set<FeatureId> featureIds;
    std::unordered_set<ToleranceId> toleranceIds;
    std::unordered_set<GdtId> gdtIds;
    std::unordered_set<MoveId> moveIds;
    std::unordered_set<MeasureId> measureIds;

    for (const Part& part : model.parts) {
        addDuplicateIdIssue(issues, part.id, partIds, "model.duplicate_part_id",
                            "Model contains duplicate part ids.");
        for (const Point& point : part.points) {
            addDuplicateIdIssue(issues, point.id, pointIds,
                                "model.duplicate_point_id",
                                "Model contains duplicate point ids.");
        }
        for (const Feature& feature : part.features) {
            addDuplicateIdIssue(issues, feature.id, featureIds,
                                "model.duplicate_feature_id",
                                "Model contains duplicate feature ids.");
        }
        for (const ToleranceDef& tolerance : part.tolerances) {
            addDuplicateIdIssue(issues, tolerance.id, toleranceIds,
                                "model.duplicate_tolerance_id",
                                "Model contains duplicate tolerance ids.");
        }
        for (const GdtDef& gdt : part.gdts) {
            addDuplicateIdIssue(issues, gdt.id, gdtIds, "model.duplicate_gdt_id",
                                "Model contains duplicate GD&T ids.");
        }
    }
    for (const MoveDef& move : model.moves) {
        addDuplicateIdIssue(issues, move.id, moveIds, "model.duplicate_move_id",
                            "Model contains duplicate move ids.");
    }
    for (const MeasureRecord& measure : model.measures) {
        addDuplicateIdIssue(issues, measure.id, measureIds,
                            "model.duplicate_measure_id",
                            "Model contains duplicate measure ids.");
    }
}

bool hasExactlyOneActiveVariant(const Model& model) {
    int activeCount = 0;
    for (const ModelVariant& variant : model.variants) {
        if (variant.active) ++activeCount;
    }
    return activeCount == 1;
}

}  // namespace

namespace {

std::vector<ModelIssue> validateModelForSimulationState(const Model& model) {
    std::vector<ModelIssue> issues;
    if (model.parts.empty()) {
        addError(issues, ModelIssueCategory::Model, "model.no_parts",
                 "Model has no parts.");
    }

    bool hasActiveMeasure = false;
    for (const MeasureRecord& measure : model.measures) {
        if (measure.def.active && measure.def.asOutput) {
            hasActiveMeasure = true;
            break;
        }
    }
    if (!hasActiveMeasure) {
        addError(issues, ModelIssueCategory::Model, "model.no_active_measures",
                 "Model has no active output measures.");
    }

    const std::unordered_set<PartId> partIds = collectPartIds(model);
    const std::unordered_set<PointId> pointIds = collectPointIds(model);
    const std::unordered_set<PointId> activePointIds = collectActivePointIds(model);
    const std::unordered_set<FeatureId> featureIds = collectFeatureIds(model);
    const std::unordered_set<ToleranceId> toleranceIds = collectToleranceIds(model);
    const std::unordered_set<MoveId> moveIds = collectMoveIds(model);
    const std::unordered_set<MeasureId> measureIds = collectMeasureIds(model);
    const std::unordered_set<MeasureId> activeMeasureIds =
        collectActiveMeasureIds(model);
    const std::unordered_map<MeasureId, std::size_t> measureOrder =
        collectMeasureOrder(model);
    const std::unordered_set<FeatureId> referencedFeatureIds =
        collectReferencedFeatureIds(model);

    addDuplicateIdIssues(model, issues);

    for (const Part& part : model.parts) {
        if (part.points.empty()) {
            addWarning(issues, ModelIssueCategory::Feature, "part.no_points",
                       "Part has no points.");
        }
        if (part.features.empty()) {
            addWarning(issues, ModelIssueCategory::Feature, "part.no_features",
                       "Part has no features.");
        }
        for (const Point& point : part.points) {
            if (!isFiniteVec3(point.position)) {
                addError(issues, ModelIssueCategory::Feature,
                         "point.position_non_finite",
                         "Point position contains a non-finite coordinate.");
            }
            if (!isFiniteVec3(point.ijk)) {
                addError(issues, ModelIssueCategory::Feature,
                         "point.direction_non_finite",
                         "Point direction contains a non-finite component.");
            } else if (isZeroVec3(point.ijk)) {
                addError(issues, ModelIssueCategory::Feature,
                         "point.direction_zero",
                         "Point direction vector cannot be zero.");
            }
            if (!std::isfinite(point.diameter)) {
                addError(issues, ModelIssueCategory::Feature,
                         "point.diameter_non_finite",
                         "Point diameter is not finite.");
            } else if (point.diameter < 0.0) {
                addError(issues, ModelIssueCategory::Feature,
                         "point.negative_diameter",
                         "Point diameter cannot be negative.");
            }
        }
        for (const Feature& feature : part.features) {
            if (feature.definingPoints.empty()) {
                addWarning(issues, ModelIssueCategory::Feature, "feature.no_points",
                           "Feature has no defining points.");
            }
            if (hasDuplicateIds(feature.definingPoints)) {
                addError(issues, ModelIssueCategory::Feature,
                         "feature.points_not_distinct",
                         "Feature defining points must be distinct.");
            }
            for (PointId pointId : feature.definingPoints) {
                if (pointIds.find(pointId) == pointIds.end()) {
                    addError(issues, ModelIssueCategory::Feature,
                             "feature.point_missing",
                             "Feature references a point that is not in the model.");
                }
            }
            if (referencedFeatureIds.find(feature.id) == referencedFeatureIds.end()) {
                addWarning(issues, ModelIssueCategory::Feature,
                           "feature.unreferenced",
                           "Feature is not referenced by any tolerance, GD&T, or measure.");
            }
        }
    }

    for (const MoveDef& move : model.moves) {
        if (!move.active) continue;
        if (requiresUnsupportedMoveInfrastructure(move.inputs)) {
            addError(issues, ModelIssueCategory::Move, "move.unsupported_type",
                     unsupportedMoveInfrastructureMessage(move.inputs.type));
        }
        if (!std::isfinite(move.inputs.searchAccuracy) ||
            move.inputs.searchAccuracy <= 0.0) {
            addError(issues, ModelIssueCategory::Move,
                     "move.invalid_search_accuracy",
                     "Active move search accuracy must be finite and positive.");
        }
        if (move.inputs.maxIterations <= 0) {
            addError(issues, ModelIssueCategory::Move,
                     "move.invalid_max_iterations",
                     "Active move max iterations must be positive.");
        }
        if (move.inputs.hole_pin_float.active) {
            if (move.inputs.hole_pin_float.sigmaNumber < 1 ||
                move.inputs.hole_pin_float.sigmaNumber > 8) {
                addError(issues, ModelIssueCategory::Move,
                         "move.invalid_float_sigma_number",
                         "Active move float sigma number must be in the range 1..8.");
            }
            if (!std::isfinite(move.inputs.hole_pin_float.rangeScale)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.float_range_scale_non_finite",
                         "Active move float range scale must be finite.");
            } else if (move.inputs.hole_pin_float.rangeScale <= 0.0) {
                addError(issues, ModelIssueCategory::Move,
                         "move.invalid_float_range_scale",
                         "Active move float range scale must be positive.");
            }
            if (!std::isfinite(move.inputs.hole_pin_float.angleRangeDeg)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.float_angle_range_non_finite",
                         "Active move float angle range must be finite.");
            }
            if (!std::isfinite(move.inputs.hole_pin_float.angleOffsetDeg)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.float_angle_offset_non_finite",
                         "Active move float angle offset must be finite.");
            }
        }
        const std::size_t requiredPairs =
            requiredMovePairCount(move.inputs.type);
        if (move.inputs.pairs.size() < requiredPairs) {
            addError(issues, ModelIssueCategory::Move, "move.not_enough_pairs",
                     "Active move has fewer point pairs than its solver requires.");
        }
        if (move.moveParts.size() < 2) {
            addError(issues, ModelIssueCategory::Move, "move.not_enough_parts",
                     "Active move requires object and target parts.");
            continue;
        }
        if (hasDuplicateIds(move.moveParts)) {
            addError(issues, ModelIssueCategory::Move, "move.parts_not_distinct",
                     "Active move parts must be distinct.");
        }
        if (move.moveParts[0] == move.moveParts[1]) {
            addError(issues, ModelIssueCategory::Move, "move.same_object_target",
                     "Active move object and target parts must be different.");
        }
        for (PartId partId : move.moveParts) {
            if (partIds.find(partId) == partIds.end()) {
                addError(issues, ModelIssueCategory::Move, "move.part_missing",
                         "Active move references a part that is not in the model.");
            }
        }
        for (const MovePair& pair : move.inputs.pairs) {
            if (!isFiniteVec3(pair.objectPoint)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.object_point_non_finite",
                         "Move object point contains a non-finite coordinate.");
            }
            if (!isFiniteVec3(pair.targetPoint)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.target_point_non_finite",
                         "Move target point contains a non-finite coordinate.");
            }
            if (!isFiniteVec3(pair.direction.ijk)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.direction_non_finite",
                         "Move direction contains a non-finite component.");
            } else if (isZeroVec3(pair.direction.ijk)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.direction_zero",
                         "Move direction vector cannot be zero.");
            }
            if (directionHasWrongRefPointCount(pair.direction)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.direction_point_count",
                         "Two-point move direction requires exactly two reference points.");
                break;
            } else if (twoPointDirectionHasDuplicateRefs(pair.direction)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.direction_points_not_distinct",
                         "Two-point move direction requires distinct reference points.");
                break;
            } else if (directionHasMissingRefPoint(pair.direction, pointIds)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.direction_point_missing",
                         "Move direction references a point that is not in the model.");
                break;
            } else if (directionHasInactiveRefPoint(pair.direction, activePointIds)) {
                addError(issues, ModelIssueCategory::Move,
                         "move.direction_point_inactive",
                         "Move direction references a point that is inactive.");
                break;
            }
        }
    }

    for (const MeasureRecord& measure : model.measures) {
        if (!measure.def.active) continue;
        const auto currentMeasureOrder = measureOrder.find(measure.id);
        const std::size_t currentIndex =
            currentMeasureOrder == measureOrder.end() ? model.measures.size()
                                                      : currentMeasureOrder->second;
        if (measure.def.type == MeasureType::UserDll &&
            !hasBoundUserDllMeasureRoutine(measure.def)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.userdll_unbound",
                     "Active UserDll measure requires a bound plugin host.");
        }
        if (measure.def.type == MeasureType::Equation &&
            equationHasOverlongLine(measure.def.equation)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.equation_line_too_long",
                     "Equation measure contains a line longer than 400 characters.");
        }
        if (measure.def.type == MeasureType::Equation &&
            equationHasInvalidSetCfg(measure.def.equation)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.equation_setcfg_invalid",
                     "Equation measure contains an unsupported SETCFG directive.");
        }
        if (measure.def.type == MeasureType::Equation &&
            equationEndsWithRemLine(measure.def.equation)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.equation_trailing_rem",
                     "Equation measure cannot end with a REM comment line.");
        }
        if (measure.def.type == MeasureType::Equation) {
            const std::size_t maxMeasureIndex =
                equationMaxIndexedVariable(measure.def.equation, "MS");
            if (maxMeasureIndex > 0 && measure.def.inputFeatures.empty()) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.no_input_measures",
                         "Equation measure references measure values but has no input measures.");
            } else if (maxMeasureIndex > measure.def.inputFeatures.size()) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.input_measure_missing",
                         "Equation measure references a measure input index that is not configured.");
            }
            const std::size_t maxValueIndex =
                equationMaxIndexedVariable(measure.def.equation, "VAL");
            if (maxValueIndex > measure.def.values.size()) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.value_missing",
                         "Equation measure references a value-list index that is not configured.");
            }
            if (equationHasMissingStringReference(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.string_value_missing",
                         "Equation measure references a string-list index that is not available.");
            }
            if (equationHasMalformedVariable(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.variable_malformed",
                         "Equation measure contains an unterminated variable.");
            }
            if (equationHasTopLevelComparison(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_top_level_equality",
                         "Equation measure cannot use a top-level comparison operator.");
            }
            if (equationHasTrailingOperator(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_malformed",
                         "Equation measure cannot end with an operator.");
            }
            if (equationHasBareNegativeConstant(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_bare_negative",
                         "Equation measure negative constants must be parenthesized.");
            }
            if (equationHasNonFiniteNumericLiteral(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_non_finite_numeric",
                         "Equation measure numeric literals must be finite.");
            }
            if (equationHasNonFiniteNumericArithmetic(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_non_finite_numeric",
                         "Equation measure numeric arithmetic must remain finite.");
            }
            if (equationHasNonFiniteNumericFunctionResult(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_non_finite_numeric",
                         "Equation measure numeric function results must remain finite.");
            }
            if (equationHasOutOfRangeTrigLiteral(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_trig_range",
                         "Equation measure trig literal inputs are out of range.");
            }
            if (equationHasInvalidDomainFunctionLiteral(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_function_domain",
                         "Equation measure function literal input is outside the supported domain.");
            }
            if (equationHasInvalidFunctionArity(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_function_arity",
                         "Equation measure function arguments do not match the supported arity.");
            }
            if (equationHasInvalidMathOperatorCase(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_math_operator_case",
                         "Equation measure math operators must use supported casing.");
            }
            if (equationHasUnsupportedMathOperator(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_function_unknown",
                         "Equation measure contains an unsupported function.");
            }
            if (equationHasNestedMultiEntryOperator(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_multi_entry_nested",
                         "Equation measure MIN/MAX operators cannot be nested.");
            }
            if (equationHasNestedConditionalOperator(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.equation_conditional_nested",
                         "Equation measure conditional operators cannot be nested.");
            }
            const std::size_t requiredEquationPointCount =
                equationRequiredPointVariableCount(measure.def.equation);
            if (requiredEquationPointCount > measure.def.inputPoints.size()) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.input_point_missing",
                         "Equation measure references an input point index that is not configured.");
            }
            if (equationHasInvalidPointVariable(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.point_variable_invalid",
                         "Equation measure contains an unsupported point variable.");
            }
            if (equationHasInvalidDirectionVariable(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.direction_variable_invalid",
                         "Equation measure direction variables must use index 1.");
            }
            if (equationHasUnknownVariable(measure.def.equation)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.variable_unknown",
                         "Equation measure contains an unsupported variable.");
            }
        }
        const std::size_t requiredMeasurePoints =
            requiredMeasurePointCount(measure.def.type);
        const bool pointCountSatisfiedByFeatures =
            featureAngleHasUsableFeatureInputs(model, measure.def);
        if (measure.def.inputPoints.size() < requiredMeasurePoints &&
            !pointCountSatisfiedByFeatures) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.not_enough_input_points",
                     "Measure has fewer input points than its type requires.");
        }
        if (measure.def.type == MeasureType::FeatureMeasure &&
            measure.def.inputFeatures.empty()) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.no_input_features",
                     "Feature measure requires at least one input feature.");
        }
        if (measure.def.type == MeasureType::Combination &&
            measure.def.equation == "in_spec" && measure.def.inputFeatures.empty()) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.no_input_measures",
                     "Combination in-spec measure requires at least one input measure.");
        }
        if (hasDuplicateIds(measure.def.inputPoints)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.input_points_not_distinct",
                     "Measure input points must be distinct.");
        }
        for (PointId pointId : measure.def.inputPoints) {
            if (pointIds.find(pointId) == pointIds.end()) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.input_point_missing",
                         "Measure references a point that is not in the model.");
            } else if (activePointIds.find(pointId) == activePointIds.end()) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.input_point_inactive",
                         "Measure references a point that is inactive.");
            }
        }
        if (!measureInputFeaturesAreMeasureRefs(measure.def.type) &&
            hasDuplicateIds(measure.def.inputFeatures)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.input_features_not_distinct",
                     "Measure input features must be distinct.");
        }
        if (measureInputFeaturesAreMeasureRefs(measure.def.type)) {
            for (FeatureId measureId : measure.def.inputFeatures) {
                const auto referencedOrder =
                    measureOrder.find(static_cast<MeasureId>(measureId));
                if (referencedOrder == measureOrder.end()) {
                    addError(issues, ModelIssueCategory::Measure,
                             "measure.input_measure_missing",
                             "Measure references another measure that is not in the model.");
                } else if (referencedOrder->second >= currentIndex) {
                    addError(issues, ModelIssueCategory::Measure,
                             "measure.input_measure_not_previous",
                             "Measure references another measure that is not earlier in model order.");
                } else if (activeMeasureIds.find(static_cast<MeasureId>(measureId)) ==
                           activeMeasureIds.end()) {
                    addError(issues, ModelIssueCategory::Measure,
                             "measure.input_measure_inactive",
                             "Measure references another measure that is inactive.");
                }
            }
        } else {
            for (FeatureId featureId : measure.def.inputFeatures) {
                if (featureIds.find(featureId) == featureIds.end()) {
                    addError(issues, ModelIssueCategory::Measure,
                             "measure.input_feature_missing",
                             "Measure references a feature that is not in the model.");
                } else if (featureHasInactiveDefiningPoint(model, featureId,
                                                           pointIds,
                                                           activePointIds)) {
                    addError(issues, ModelIssueCategory::Measure,
                             "measure.input_feature_point_inactive",
                             "Measure references a feature with an inactive defining point.");
                }
            }
        }
        if (directionHasWrongRefPointCount(measure.def.direction)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.direction_point_count",
                     "Two-point measure direction requires exactly two reference points.");
        } else if (twoPointDirectionHasDuplicateRefs(measure.def.direction)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.direction_points_not_distinct",
                     "Two-point measure direction requires distinct reference points.");
        } else if (directionHasMissingRefPoint(measure.def.direction, pointIds)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.direction_point_missing",
                     "Measure direction references a point that is not in the model.");
        } else if (directionHasInactiveRefPoint(measure.def.direction, activePointIds)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.direction_point_inactive",
                     "Measure direction references a point that is inactive.");
        }
        if (!isFiniteVec3(measure.def.direction.ijk)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.direction_non_finite",
                     "Measure direction contains a non-finite component.");
        } else if (isZeroVec3(measure.def.direction.ijk)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.direction_zero",
                     "Measure direction vector cannot be zero.");
        }
        if (!std::isfinite(measure.def.scale)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.scale_non_finite",
                     "Measure scale is not finite.");
        } else if (measure.def.scale == 0.0) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.invalid_scale",
                     "Measure scale cannot be zero.");
        }
        for (double value : measure.def.values) {
            if (!std::isfinite(value)) {
                addError(issues, ModelIssueCategory::Measure,
                         "measure.value_non_finite",
                         "Measure value list contains a non-finite value.");
                break;
            }
        }
        if (!std::isfinite(measure.def.spec.lsl) ||
            !std::isfinite(measure.def.spec.usl)) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.spec_non_finite",
                     "Measure spec limits must be finite.");
        } else if (measure.def.spec.lslActive && measure.def.spec.uslActive &&
                   measure.def.spec.lsl > measure.def.spec.usl) {
            addError(issues, ModelIssueCategory::Measure,
                     "measure.spec_limits_misordered",
                     "Measure lower spec limit cannot be greater than the upper spec limit.");
        }
    }

    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            if (!tolerance.active) continue;
            if (tolerance.features.empty()) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.no_features",
                         "Active tolerance has no controlled features.");
            }
            if (hasDuplicateIds(tolerance.features)) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.features_not_distinct",
                         "Active tolerance controlled features must be distinct.");
            }
            if (tolerance.ir.rands.empty()) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.no_random_variables",
                         "Active tolerance has no random variables.");
            }
            if (!std::isfinite(tolerance.ir.rangeScale)) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.range_scale_non_finite",
                         "Tolerance range scale is not finite.");
            } else if (tolerance.ir.rangeScale <= 0.0) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.invalid_range_scale",
                         "Tolerance range scale must be positive.");
            }
            if (tolerance.ir.truncation.active) {
                if (!std::isfinite(tolerance.ir.truncation.minTrunc) ||
                    !std::isfinite(tolerance.ir.truncation.maxTrunc)) {
                    addError(issues, ModelIssueCategory::Tolerance,
                             "tolerance.truncation_non_finite",
                             "Active tolerance truncation limits must be finite.");
                } else if (tolerance.ir.truncation.minTrunc >
                           tolerance.ir.truncation.maxTrunc) {
                    addError(issues, ModelIssueCategory::Tolerance,
                             "tolerance.truncation_misordered",
                             "Active tolerance truncation minimum cannot exceed the maximum.");
                }
            }
            for (const RandSpec& rand : tolerance.ir.rands) {
                if (!std::isfinite(rand.range)) {
                    addError(issues, ModelIssueCategory::Tolerance,
                             "tolerance.range_non_finite",
                             "Tolerance random range is not finite.");
                } else if (rand.range < 0.0) {
                    addError(issues, ModelIssueCategory::Tolerance,
                             "tolerance.negative_range",
                             "Tolerance random range cannot be negative.");
                }
                if (!std::isfinite(rand.offset)) {
                    addError(issues, ModelIssueCategory::Tolerance,
                             "tolerance.offset_non_finite",
                             "Tolerance random offset is not finite.");
                }
                if (!std::isfinite(rand.sigmaNum) || rand.sigmaNum <= 0.0) {
                    addError(issues, ModelIssueCategory::Tolerance,
                             "tolerance.invalid_sigma_number",
                             "Tolerance sigma number must be finite and positive.");
                }
                if (rand.distribution == DistributionType::UserDefined &&
                    rand.userDefinedSamplePath.empty()) {
                    addWarning(issues, ModelIssueCategory::Tolerance,
                               "tolerance.userdefined_sample_path_missing",
                               "UserDefined distribution has no .SMP sample path and will use the default fallback.");
                } else if (rand.distribution != DistributionType::UserDefined &&
                           !rand.userDefinedSamplePath.empty()) {
                    addWarning(issues, ModelIssueCategory::Tolerance,
                               "tolerance.userdefined_sample_path_ignored",
                               "UserDefined sample path is ignored unless the distribution is UserDefined.");
                }
            }
            for (FeatureId featureId : tolerance.features) {
                if (featureIds.find(featureId) == featureIds.end()) {
                    addError(issues, ModelIssueCategory::Tolerance,
                             "tolerance.feature_missing",
                             "Tolerance references a feature that is not in the model.");
                } else if (featureHasInactiveDefiningPoint(model, featureId,
                                                           pointIds,
                                                           activePointIds)) {
                    addError(issues, ModelIssueCategory::Tolerance,
                             "tolerance.feature_point_inactive",
                             "Tolerance references a feature with an inactive defining point.");
                }
            }
            if (directionHasWrongRefPointCount(tolerance.ir.direction)) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.direction_point_count",
                         "Two-point tolerance direction requires exactly two reference points.");
            } else if (twoPointDirectionHasDuplicateRefs(tolerance.ir.direction)) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.direction_points_not_distinct",
                         "Two-point tolerance direction requires distinct reference points.");
            } else if (directionHasMissingRefPoint(tolerance.ir.direction, pointIds)) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.direction_point_missing",
                         "Tolerance direction references a point that is not in the model.");
            } else if (directionHasInactiveRefPoint(tolerance.ir.direction,
                                                    activePointIds)) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.direction_point_inactive",
                         "Tolerance direction references a point that is inactive.");
            }
            if (!isFiniteVec3(tolerance.ir.direction.ijk)) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.direction_non_finite",
                         "Tolerance direction contains a non-finite component.");
            } else if (isZeroVec3(tolerance.ir.direction.ijk)) {
                addError(issues, ModelIssueCategory::Tolerance,
                         "tolerance.direction_zero",
                         "Tolerance direction vector cannot be zero.");
            }
        }

        for (const GdtDef& gdt : part.gdts) {
            if (!gdt.active) continue;
            if (gdt.features.empty()) {
                addError(issues, ModelIssueCategory::Gdt, "gdt.no_features",
                         "Active GD&T callout has no controlled features.");
            }
            if (hasDuplicateIds(gdt.features)) {
                addError(issues, ModelIssueCategory::Gdt,
                         "gdt.features_not_distinct",
                         "Active GD&T callout controlled features must be distinct.");
            }
            if (!std::isfinite(gdt.range)) {
                addError(issues, ModelIssueCategory::Gdt, "gdt.range_non_finite",
                         "GD&T tolerance zone range is not finite.");
            } else if (gdt.range < 0.0) {
                addError(issues, ModelIssueCategory::Gdt, "gdt.negative_range",
                         "GD&T tolerance zone range cannot be negative.");
            } else if (gdt.range == 0.0) {
                addWarning(issues, ModelIssueCategory::Gdt, "gdt.zero_range",
                           "GD&T tolerance zone range is zero.");
            }
            for (FeatureId featureId : gdt.features) {
                if (featureIds.find(featureId) == featureIds.end()) {
                    addError(issues, ModelIssueCategory::Gdt, "gdt.feature_missing",
                             "GD&T callout references a feature that is not in the model.");
                } else if (featureHasInactiveDefiningPoint(model, featureId,
                                                           pointIds,
                                                           activePointIds)) {
                    addError(issues, ModelIssueCategory::Gdt,
                             "gdt.feature_point_inactive",
                             "GD&T callout references a feature with an inactive defining point.");
                }
            }
            if (datumReferenceFrameHasGap(gdt.drf)) {
                addError(issues, ModelIssueCategory::Gdt,
                         "gdt.datum_order_invalid",
                         "GD&T datum reference frame slots must be filled in priority order.");
            } else if (gdtTypeRequiresDatum(gdt.type) &&
                       gdt.drf.primary == kInvalidId) {
                addError(issues, ModelIssueCategory::Gdt,
                         "gdt.datum_required",
                         "GD&T type requires a primary datum reference.");
            } else if (datumReferenceFrameHasDuplicateRefs(gdt.drf)) {
                addError(issues, ModelIssueCategory::Gdt,
                         "gdt.datums_not_distinct",
                         "GD&T datum reference frame requires distinct datum features.");
            } else if (!datumExists(featureIds, gdt.drf.primary) ||
                !datumExists(featureIds, gdt.drf.secondary) ||
                !datumExists(featureIds, gdt.drf.tertiary)) {
                addError(issues, ModelIssueCategory::Gdt, "gdt.datum_missing",
                         "GD&T datum reference frame references a feature that is not in the model.");
            } else if (datumHasInactiveDefiningPoint(model, gdt.drf, pointIds,
                                                     activePointIds)) {
                addError(issues, ModelIssueCategory::Gdt,
                         "gdt.datum_point_inactive",
                         "GD&T datum reference frame references a feature with an inactive defining point.");
            }
        }
    }

    std::unordered_set<std::string> variantNames;
    int activeVariantCount = 0;
    for (const ModelVariant& variant : model.variants) {
        if (variant.name.empty()) {
            addError(issues, ModelIssueCategory::Model, "variant.name_empty",
                     "Model variant name cannot be empty.");
        }
        if (!variantNames.insert(variant.name).second) {
            addError(issues, ModelIssueCategory::Model,
                     "variant.duplicate_name",
                     "Model variants must have unique names.");
        }
        if (variant.active) {
            ++activeVariantCount;
            if (activeVariantCount > 1) {
                addError(issues, ModelIssueCategory::Model,
                         "variant.multiple_active",
                         "Only one model variant can be active at a time.");
            }
        }
        if (hasDuplicateIds(variant.moves)) {
            addError(issues, ModelIssueCategory::Model,
                     "variant.moves_not_distinct",
                     "Model variant move references must be distinct.");
        }
        if (hasDuplicateIds(variant.tolerances)) {
            addError(issues, ModelIssueCategory::Model,
                     "variant.tolerances_not_distinct",
                     "Model variant tolerance references must be distinct.");
        }
        if (hasDuplicateIds(variant.measures)) {
            addError(issues, ModelIssueCategory::Model,
                     "variant.measures_not_distinct",
                     "Model variant measure references must be distinct.");
        }
        for (const MoveId moveId : variant.moves) {
            if (moveIds.find(moveId) == moveIds.end()) {
                addError(issues, ModelIssueCategory::Model, "variant.move_missing",
                         "Model variant references a move that is not in the model.");
            }
        }
        for (const ToleranceId toleranceId : variant.tolerances) {
            if (toleranceIds.find(toleranceId) == toleranceIds.end()) {
                addError(issues, ModelIssueCategory::Model,
                         "variant.tolerance_missing",
                         "Model variant references a tolerance that is not in the model.");
            }
        }
        for (const MeasureId measureId : variant.measures) {
            if (measureIds.find(measureId) == measureIds.end()) {
                addError(issues, ModelIssueCategory::Model,
                         "variant.measure_missing",
                         "Model variant references a measure that is not in the model.");
            }
        }
    }

    return issues;
}

}  // namespace

std::vector<ModelIssue> validateModelForSimulation(const Model& model) {
    if (hasExactlyOneActiveVariant(model)) {
        return validateModelForSimulationState(model.activeVariantApplied());
    }
    return validateModelForSimulationState(model);
}

bool hasBlockingIssues(const std::vector<ModelIssue>& issues) {
    for (const ModelIssue& issue : issues) {
        if (issue.severity == ModelIssueSeverity::Error) return true;
    }
    return false;
}

ValidationSummary summarizeIssues(const std::vector<ModelIssue>& issues) {
    ValidationSummary summary;
    summary.categoryCounts[ModelIssueCategory::Model] = 0;
    summary.categoryCounts[ModelIssueCategory::Move] = 0;
    summary.categoryCounts[ModelIssueCategory::Tolerance] = 0;
    summary.categoryCounts[ModelIssueCategory::Measure] = 0;
    summary.categoryCounts[ModelIssueCategory::Gdt] = 0;
    summary.categoryCounts[ModelIssueCategory::Feature] = 0;

    for (const ModelIssue& issue : issues) {
        switch (issue.severity) {
            case ModelIssueSeverity::Info:
                ++summary.infoCount;
                break;
            case ModelIssueSeverity::Warning:
                ++summary.warningCount;
                break;
            case ModelIssueSeverity::Error:
                ++summary.errorCount;
                break;
        }
        ++summary.categoryCounts[issue.category];
    }
    return summary;
}

const char* issueSeverityName(ModelIssueSeverity severity) {
    switch (severity) {
        case ModelIssueSeverity::Info: return "Info";
        case ModelIssueSeverity::Warning: return "Warning";
        case ModelIssueSeverity::Error: return "Error";
    }
    return "Error";
}

const char* issueCategoryName(ModelIssueCategory category) {
    switch (category) {
        case ModelIssueCategory::Model: return "Model";
        case ModelIssueCategory::Move: return "Move";
        case ModelIssueCategory::Tolerance: return "Tolerance";
        case ModelIssueCategory::Measure: return "Measure";
        case ModelIssueCategory::Gdt: return "GD&T";
        case ModelIssueCategory::Feature: return "Feature";
    }
    return "Model";
}

}  // namespace opendva
