#include "opendva/domain/Model.h"

namespace opendva {

const Part* Model::findPart(PartId id) const {
    for (const auto& p : parts) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

std::vector<const MoveDef*> Model::activeMovesInOrder() const {
    std::vector<const MoveDef*> out;
    out.reserve(moves.size());
    for (const auto& m : moves) {
        if (m.active) out.push_back(&m);  // preserve tree order
    }
    return out;
}

std::vector<const ToleranceDef*> Model::activeTolerances() const {
    // Child->parent order: parts are stored leaf-first by convention (A2),
    // so a simple forward sweep already yields the required traversal order.
    std::vector<const ToleranceDef*> out;
    for (const auto& part : parts) {
        for (const auto& tol : part.tolerances) {
            if (tol.active) out.push_back(&tol);
        }
    }
    return out;
}

namespace {

template <typename T>
bool contains(const std::vector<T>& v, const T& value) {
    for (const auto& e : v) {
        if (e == value) return true;
    }
    return false;
}

}  // namespace

Model Model::applyVariant(const std::string& variantName) const {
    Model out = *this;

    const ModelVariant* variant = nullptr;
    for (const auto& v : out.variants) {
        if (v.name == variantName) {
            variant = &v;
            break;
        }
    }
    if (variant == nullptr) return out;  // unknown variant: unchanged copy

    // For each component class the variant constrains, keep only the listed
    // components active; deactivate the rest of that class.
    for (auto& m : out.moves) {
        m.active = contains(variant->moves, m.id);
    }
    for (auto& part : out.parts) {
        for (auto& tol : part.tolerances) {
            tol.active = contains(variant->tolerances, tol.id);
        }
    }
    for (auto& rec : out.measures) {
        rec.def.active = contains(variant->measures, rec.id);
    }

    // Reflect the selection in the variant table.
    for (auto& v : out.variants) {
        v.active = (v.name == variantName);
    }
    return out;
}

std::optional<std::string> Model::activeVariantName() const {
    for (const ModelVariant& variant : variants) {
        if (variant.active) return variant.name;
    }
    return std::nullopt;
}

Model Model::activeVariantApplied() const {
    const std::optional<std::string> active = activeVariantName();
    if (!active.has_value()) return *this;
    return applyVariant(active.value());
}

const Part* matchPart(const Model& model, PartId id, const std::string& cadName,
                      const std::string& dcsName) {
    // Priority 1: Part ID.
    if (id != kInvalidId) {
        for (const auto& p : model.parts) {
            if (p.id == id) return &p;
        }
    }
    // Priority 2: CAD Part Name.
    if (!cadName.empty()) {
        for (const auto& p : model.parts) {
            if (p.cadName == cadName) return &p;
        }
    }
    // Priority 3: 3DCS (DCS) Part Name.
    if (!dcsName.empty()) {
        for (const auto& p : model.parts) {
            if (p.dcsName == dcsName) return &p;
        }
    }
    return nullptr;
}

}  // namespace opendva
