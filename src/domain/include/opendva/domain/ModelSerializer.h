// A2 model serialization (README §2.6 / §3.9). Round-trips a Model to a
// self-described XML document (root <OpenDVAModel>) with no external XML
// dependency: a minimal hand-written writer/reader handle just the subset of
// XML this schema emits.
//
// Round-trip guarantee: loadModel(saveModel(m)) is semantically equal to m for
// point coordinates, tolerance IR, move type/order, measure definitions, GD&T
// callouts and model variants.
#pragma once
#include <string>

#include "opendva/domain/Model.h"

namespace opendva {

// Serialize `model` to the file at `path`. Returns false on I/O failure.
bool saveModel(const Model& model, const std::string& path);

// Parse the file at `path` into `model`. Returns false on I/O failure or if the
// document is not a well-formed OpenDVAModel; on failure, `model` is unchanged.
bool loadModel(Model& model, const std::string& path);

// Same as above but operating on in-memory XML strings (used by tests and for
// embedding). `saveModelToString` never fails; `loadModelFromString` returns
// false on a parse error and leaves `model` unchanged.
std::string saveModelToString(const Model& model);
bool loadModelFromString(Model& model, const std::string& xml);

}  // namespace opendva
