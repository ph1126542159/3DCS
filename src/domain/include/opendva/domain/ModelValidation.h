// Domain validation for deciding whether a model is ready to simulate.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "opendva/domain/Model.h"

namespace opendva {

enum class ModelIssueSeverity { Info, Warning, Error };
enum class ModelIssueCategory { Model, Move, Tolerance, Measure, Gdt, Feature };

struct ModelIssue {
    ModelIssueSeverity severity{ModelIssueSeverity::Error};
    ModelIssueCategory category{ModelIssueCategory::Model};
    std::string code;
    std::string message;
};

struct ValidationSummary {
    int errorCount{0};
    int warningCount{0};
    int infoCount{0};
    std::map<ModelIssueCategory, int> categoryCounts;
};

std::vector<ModelIssue> validateModelForSimulation(const Model& model);
bool hasBlockingIssues(const std::vector<ModelIssue>& issues);
ValidationSummary summarizeIssues(const std::vector<ModelIssue>& issues);
const char* issueSeverityName(ModelIssueSeverity severity);
const char* issueCategoryName(ModelIssueCategory category);

}  // namespace opendva
