// A2 domain tests (README §2.6/§3.9 serialization, §6.8 Model Variants,
// §3.9.5 three-level part matching). Exercises the round-trip serializer and
// the variant / matching helpers added to the domain model.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "dva_test.h"
#include "opendva/domain/Model.h"
#include "opendva/domain/ModelSerializer.h"

using namespace opendva;

namespace {

std::string withCompleteRootCollections(std::string xml) {
    const std::string closeRoot = "</OpenDVAModel>";
    const auto closePos = xml.rfind(closeRoot);
    if (closePos == std::string::npos) {
        return xml;
    }

    const auto openEnd = xml.find('>');
    if (openEnd == std::string::npos || openEnd >= closePos) {
        return xml;
    }

    auto hasRootChild = [&](const std::string& name) {
        std::size_t depth = 0;
        std::size_t pos = openEnd + 1;
        while (pos < closePos) {
            const auto tagStart = xml.find('<', pos);
            if (tagStart == std::string::npos || tagStart >= closePos) {
                return false;
            }
            const auto tagEnd = xml.find('>', tagStart + 1);
            if (tagEnd == std::string::npos || tagEnd > closePos) {
                return false;
            }
            if (tagStart + 1 < tagEnd && xml[tagStart + 1] == '/') {
                if (depth > 0) {
                    --depth;
                }
                pos = tagEnd + 1;
                continue;
            }
            std::size_t nameStart = tagStart + 1;
            while (nameStart < tagEnd && xml[nameStart] == ' ') {
                ++nameStart;
            }
            std::size_t nameEnd = nameStart;
            while (nameEnd < tagEnd && xml[nameEnd] != ' ' && xml[nameEnd] != '/' && xml[nameEnd] != '>') {
                ++nameEnd;
            }
            const std::string tagName = xml.substr(nameStart, nameEnd - nameStart);
            if (depth == 0 && tagName == name) {
                return true;
            }
            if (tagEnd > tagStart && xml[tagEnd - 1] != '/') {
                ++depth;
            }
            pos = tagEnd + 1;
        }
        return false;
    };

    std::string missing;
    if (!hasRootChild("Parts")) missing += "<Parts/>";
    if (!hasRootChild("Moves")) missing += "<Moves/>";
    if (!hasRootChild("Measures")) missing += "<Measures/>";
    if (!hasRootChild("Variants")) missing += "<Variants/>";
    xml.insert(closePos, missing);
    return xml;
}

// Build a representative model: 2 parts (each with points / a feature /
// a tolerance / a GD&T), 2 ordered moves, 1 measure, 2 variants.
Model makeSampleModel() {
    Model m;
    m.assemblyName = "Bracket<&>Assembly \"v1\"";

    Part p1;
    p1.id = 10;
    p1.cadName = "CAD_BASE";
    p1.dcsName = "Base";
    {
        Point a;
        a.id = 101;
        a.kind = PointKind::Feature;
        a.position = {1.5, -2.25, 3.75};
        a.ijk = {0, 1, 0};
        a.diameter = 8.0;
        a.holeType = HoleType::Hole;
        p1.points.push_back(a);
        Point b;
        b.id = 102;
        b.position = {-4.0, 5.0, 6.0};
        p1.points.push_back(b);
    }
    {
        Feature f;
        f.id = 201;
        f.kind = FeatureKind::Cylinder;
        f.definingPoints = {101, 102};
        f.mesh.feature = 201;
        f.mesh.meshNodeNum = 42;
        f.mesh.version = 3;
        p1.features.push_back(f);
    }
    {
        ToleranceDef t;
        t.id = 301;
        t.name = "Pos_Tol";
        t.active = true;
        t.ir.geomRule = GeomRule::RotateAboutLocatorPoint;
        t.ir.rangeScale = 1.25;
        RandSpec r;
        r.distribution = DistributionType::UserDefined;
        r.range = 0.4;
        r.offset = 0.05;
        r.sigmaNum = 4.0;
        r.userDefinedSamplePath = "samples/bracket offsets.smp";
        t.ir.rands.push_back(r);
        t.ir.truncation = {-0.2, 0.2, true};
        t.ir.direction.type = DirectionType::TwoPoints;
        t.ir.direction.ijk = {0, 0, 1};
        t.ir.direction.refPoints = {101, 102};
        t.features = {201};
        p1.tolerances.push_back(t);
    }
    {
        GdtDef g;
        g.id = 401;
        g.name = "Position_GDT";
        g.type = GdtType::Position;
        g.range = 0.3;
        g.diametrical = true;
        g.drf = {201, kInvalidId, kInvalidId};
        g.features = {201};
        p1.gdts.push_back(g);
    }
    m.parts.push_back(p1);

    Part p2;
    p2.id = 20;
    p2.cadName = "CAD_LID";
    p2.dcsName = "Lid";
    {
        Point c;
        c.id = 111;
        c.position = {9.0, 8.0, 7.0};
        p2.points.push_back(c);
    }
    {
        ToleranceDef t;
        t.id = 302;
        t.name = "Flat_Tol";
        t.ir.geomRule = GeomRule::NodeNormalOffset;
        RandSpec r;
        r.distribution = DistributionType::Normal;
        r.range = 0.1;
        t.ir.rands.push_back(r);
        p2.tolerances.push_back(t);
    }
    {
        GdtDef g;
        g.id = 402;
        g.name = "Flatness_GDT";
        g.type = GdtType::Flatness;
        g.range = 0.15;
        p2.gdts.push_back(g);
    }
    m.parts.push_back(p2);

    // Ordered moves — order is significant (tree order == solve order).
    {
        MoveDef mv;
        mv.id = 501;
        mv.name = "Locate_Base";
        mv.inputs.type = MoveType::SixPlane;
        mv.moveParts = {10};
        MovePair pr;
        pr.objectPoint = {1, 2, 3};
        pr.targetPoint = {1, 2, 4};
        pr.direction.ijk = {0, 0, 1};
        mv.inputs.pairs.push_back(pr);
        m.moves.push_back(mv);
    }
    {
        MoveDef mv;
        mv.id = 502;
        mv.name = "Locate_Lid";
        mv.inputs.type = MoveType::ThreePoint;
        mv.inputs.hole_pin_float.active = true;
        mv.inputs.hole_pin_float.angleRangeDeg = 180.0;
        mv.moveParts = {20, 10};
        m.moves.push_back(mv);
    }

    {
        MeasureRecord rec;
        rec.id = 601;
        rec.name = "Gap_AB";
        rec.def.type = MeasureType::PointPlane;
        rec.def.inputPoints = {101, 111};
        rec.def.spec = {0.5, -0.5, true, true, SpecMode::RelativeToNominal};
        rec.def.scale = 2.0;
        rec.def.values = {2.5, 4.0};
        m.measures.push_back(rec);
    }

    {
        ModelVariant v;
        v.name = "Scenario_A";
        v.moves = {501};
        v.tolerances = {301};
        v.measures = {601};
        m.variants.push_back(v);
    }
    {
        ModelVariant v;
        v.name = "Scenario_B";
        v.moves = {502};
        v.tolerances = {302};
        v.measures = {601};
        m.variants.push_back(v);
    }
    return m;
}

std::string makeLateInvalidVariantXml() {
    Model replacement = makeSampleModel();
    replacement.assemblyName = "Replacement";
    replacement.parts.front().id = 7001;
    replacement.moves.front().id = 8001;
    replacement.measures.front().id = 9001;
    replacement.variants.front().name = "ReplacementVariant";

    std::string badXml = saveModelToString(replacement);
    const std::string requiredName = " name=\"ReplacementVariant\"";
    const auto namePos = badXml.find(requiredName);
    dvatest::check(namePos != std::string::npos, "variant name fixture exists");
    badXml.erase(namePos, requiredName.size());
    return badXml;
}

void checkVec3(const Vec3& a, const Vec3& b, const std::string& what) {
    dvatest::checkNear(a.x, b.x, 1e-12, what + ".x");
    dvatest::checkNear(a.y, b.y, 1e-12, what + ".y");
    dvatest::checkNear(a.z, b.z, 1e-12, what + ".z");
}

void checkModelStillMatches(const Model& actual, const Model& expected, const std::string& context) {
    dvatest::check(actual.assemblyName == expected.assemblyName,
                   context + " preserves assemblyName");
    dvatest::check(actual.parts.size() == expected.parts.size(),
                   context + " preserves part count");
    dvatest::check(actual.parts.front().id == expected.parts.front().id,
                   context + " preserves part identity");
    dvatest::check(actual.moves.size() == expected.moves.size(),
                   context + " preserves move count");
    dvatest::check(actual.moves.front().id == expected.moves.front().id,
                   context + " preserves move identity");
    dvatest::check(actual.measures.size() == expected.measures.size(),
                   context + " preserves measure count");
    dvatest::check(actual.measures.front().id == expected.measures.front().id,
                   context + " preserves measure identity");
    dvatest::check(actual.variants.size() == expected.variants.size(),
                   context + " preserves variant count");
    dvatest::check(actual.variants.front().name == expected.variants.front().name,
                   context + " preserves variant identity");
}

}  // namespace

TEST("domain test helper: complete root collections preserves root-level intent") {
    const std::string completeXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Complete\">"
        "<Parts/><Moves/><Measures/><Variants/>"
        "</OpenDVAModel>";
    dvatest::check(withCompleteRootCollections(completeXml) == completeXml,
                   "helper leaves complete root collections unchanged");

    const std::string nestedOnlyXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Nested\">"
        "<Parts><Part id=\"1\" cadName=\"Cad\" dcsName=\"Dcs\"><Moves/></Part></Parts>"
        "</OpenDVAModel>";
    const std::string completed = withCompleteRootCollections(nestedOnlyXml);
    dvatest::check(completed.find("</Parts><Moves/><Measures/><Variants/></OpenDVAModel>") !=
                       std::string::npos,
                   "helper ignores nested collection-like tags");
}

TEST("domain: serialize round-trip preserves semantics") {
    Model orig = makeSampleModel();
    orig.variants[1].active = true;

    // Round-trip via string (no file system dependency for the core check).
    std::string xml = saveModelToString(orig);
    dvatest::check(xml.find("<OpenDVAModel") != std::string::npos, "root tag present");
    dvatest::check(xml.find("version=\"1\"") != std::string::npos, "root version present");
    dvatest::check(xml.find("<Parts>") != std::string::npos, "root Parts collection present");
    dvatest::check(xml.find("<Moves>") != std::string::npos, "root Moves collection present");
    dvatest::check(xml.find("<Measures>") != std::string::npos, "root Measures collection present");
    dvatest::check(xml.find("<Variants>") != std::string::npos, "root Variants collection present");
    dvatest::check(xml.find("userDefinedSamplePath=\"samples/bracket offsets.smp\"") !=
                       std::string::npos,
                   "user-defined sample path serialized");
    // Escaping of special chars in assemblyName.
    dvatest::check(xml.find("&lt;") != std::string::npos, "escaped '<'");
    dvatest::check(xml.find("&amp;") != std::string::npos, "escaped '&'");
    dvatest::check(xml.find("&quot;") != std::string::npos, "escaped '\"'");

    Model back;
    dvatest::check(loadModelFromString(back, xml), "loadModelFromString ok");

    // Assembly name (round-trips through escaping).
    dvatest::check(back.assemblyName == orig.assemblyName, "assemblyName");

    // Parts.
    dvatest::check(back.parts.size() == 2, "two parts");
    const Part& bp1 = back.parts[0];
    dvatest::check(bp1.id == 10, "part1 id");
    dvatest::check(bp1.cadName == "CAD_BASE", "part1 cadName");
    dvatest::check(bp1.dcsName == "Base", "part1 dcsName");

    // Point coordinates + attributes.
    dvatest::check(bp1.points.size() == 2, "part1 point count");
    checkVec3(bp1.points[0].position, {1.5, -2.25, 3.75}, "p1.pt0.pos");
    checkVec3(bp1.points[0].ijk, {0, 1, 0}, "p1.pt0.ijk");
    dvatest::checkNear(bp1.points[0].diameter, 8.0, 1e-12, "p1.pt0.diameter");
    dvatest::check(bp1.points[0].kind == PointKind::Feature, "p1.pt0.kind");
    dvatest::check(bp1.points[0].holeType == HoleType::Hole, "p1.pt0.holeType");

    // Feature + mesh handle.
    dvatest::check(bp1.features.size() == 1, "part1 feature count");
    dvatest::check(bp1.features[0].kind == FeatureKind::Cylinder, "feat kind");
    dvatest::check(bp1.features[0].definingPoints.size() == 2, "feat defining pts");
    dvatest::check(bp1.features[0].mesh.meshNodeNum == 42, "feat mesh node num");
    dvatest::check(bp1.features[0].mesh.version == 3, "feat mesh version");

    // Tolerance IR (range / distribution / geomRule / truncation / direction).
    dvatest::check(bp1.tolerances.size() == 1, "part1 tol count");
    const ToleranceDef& bt = bp1.tolerances[0];
    dvatest::check(bt.name == "Pos_Tol", "tol name");
    dvatest::check(bt.ir.geomRule == GeomRule::RotateAboutLocatorPoint, "tol geomRule");
    dvatest::checkNear(bt.ir.rangeScale, 1.25, 1e-12, "tol rangeScale");
    dvatest::check(bt.ir.rands.size() == 1, "tol rand count");
    dvatest::check(bt.ir.rands[0].distribution == DistributionType::UserDefined, "tol dist");
    dvatest::checkNear(bt.ir.rands[0].range, 0.4, 1e-12, "tol range");
    dvatest::checkNear(bt.ir.rands[0].offset, 0.05, 1e-12, "tol offset");
    dvatest::checkNear(bt.ir.rands[0].sigmaNum, 4.0, 1e-12, "tol sigmaNum");
    dvatest::check(bt.ir.rands[0].userDefinedSamplePath ==
                       "samples/bracket offsets.smp",
                   "tol user-defined sample path");
    dvatest::check(bt.ir.truncation.active, "tol trunc active");
    dvatest::checkNear(bt.ir.truncation.maxTrunc, 0.2, 1e-12, "tol trunc max");
    dvatest::check(bt.ir.direction.type == DirectionType::TwoPoints, "tol dir type");
    dvatest::check(bt.ir.direction.refPoints.size() == 2, "tol dir refpts");

    // GD&T callouts.
    dvatest::check(bp1.gdts.size() == 1, "part1 gdt count");
    dvatest::check(bp1.gdts[0].type == GdtType::Position, "gdt1 type");
    dvatest::checkNear(bp1.gdts[0].range, 0.3, 1e-12, "gdt1 range");
    dvatest::check(bp1.gdts[0].diametrical, "gdt1 diametrical");
    dvatest::check(bp1.gdts[0].drf.primary == 201, "gdt1 drf primary");
    dvatest::check(back.parts[1].gdts[0].type == GdtType::Flatness, "gdt2 type");

    // Moves: order MUST be preserved (501 then 502).
    dvatest::check(back.moves.size() == 2, "move count");
    dvatest::check(back.moves[0].id == 501, "move[0] id (order)");
    dvatest::check(back.moves[1].id == 502, "move[1] id (order)");
    dvatest::check(back.moves[0].inputs.type == MoveType::SixPlane, "move[0] type");
    dvatest::check(back.moves[1].inputs.type == MoveType::ThreePoint, "move[1] type");
    dvatest::check(back.moves[0].moveParts.size() == 1, "move[0] parts");
    dvatest::check(back.moves[1].moveParts.size() == 2, "move[1] parts");
    dvatest::check(back.moves[1].moveParts[0] == 20, "move[1] object part");
    dvatest::check(back.moves[1].moveParts[1] == 10, "move[1] target part");
    dvatest::check(back.moves[1].inputs.hole_pin_float.active, "move[1] float active");
    dvatest::checkNear(back.moves[1].inputs.hole_pin_float.angleRangeDeg, 180.0, 1e-12,
                       "move[1] float angle");
    checkVec3(back.moves[0].inputs.pairs[0].objectPoint, {1, 2, 3}, "move[0] pair obj");
    checkVec3(back.moves[0].inputs.pairs[0].targetPoint, {1, 2, 4}, "move[0] pair tgt");

    // Measure definition.
    dvatest::check(back.measures.size() == 1, "measure count");
    const MeasureRecord& bm = back.measures[0];
    dvatest::check(bm.name == "Gap_AB", "measure name");
    dvatest::check(bm.def.type == MeasureType::PointPlane, "measure type");
    dvatest::check(bm.def.inputPoints.size() == 2, "measure input pts");
    dvatest::check(bm.def.spec.uslActive && bm.def.spec.lslActive, "measure spec active");
    dvatest::check(bm.def.spec.mode == SpecMode::RelativeToNominal, "measure spec mode");
    dvatest::checkNear(bm.def.spec.usl, 0.5, 1e-12, "measure usl");
    dvatest::checkNear(bm.def.scale, 2.0, 1e-12, "measure scale");
    dvatest::check(bm.def.values.size() == 2, "measure values count");
    dvatest::checkNear(bm.def.values[0], 2.5, 1e-12, "measure value[0]");
    dvatest::checkNear(bm.def.values[1], 4.0, 1e-12, "measure value[1]");

    // Variants.
    dvatest::check(back.variants.size() == 2, "variant count");
    dvatest::check(back.variants[0].name == "Scenario_A", "variant[0] name");
    dvatest::check(!back.variants[0].active, "variant[0] inactive");
    dvatest::check(back.variants[0].moves.size() == 1 &&
                       back.variants[0].moves[0] == 501,
                   "variant[0] moves");
    dvatest::check(back.variants[0].tolerances.size() == 1 &&
                       back.variants[0].tolerances[0] == 301,
                   "variant[0] tolerances");
    dvatest::check(back.variants[0].measures.size() == 1 &&
                       back.variants[0].measures[0] == 601,
                   "variant[0] measures");
    dvatest::check(back.variants[1].name == "Scenario_B", "variant[1] name");
    dvatest::check(back.variants[1].active, "variant[1] active");
    dvatest::check(back.variants[1].moves.size() == 1 &&
                       back.variants[1].moves[0] == 502,
                   "variant[1] moves");
    dvatest::check(back.variants[1].tolerances.size() == 1 &&
                       back.variants[1].tolerances[0] == 302,
                   "variant[1] tolerances");
    dvatest::check(back.variants[1].measures.size() == 1 &&
                       back.variants[1].measures[0] == 601,
                   "variant[1] measures");

    // File-based round trip too (exercises saveModel/loadModel I/O paths).
    const std::string path =
        (std::filesystem::temp_directory_path() / "opendva_domain_roundtrip.xml").string();
    dvatest::check(saveModel(orig, path), "saveModel to file");
    Model fromFile;
    dvatest::check(loadModel(fromFile, path), "loadModel from file");
    dvatest::check(fromFile.moves.size() == 2 && fromFile.moves[0].id == 501,
                   "file round-trip move order");
    dvatest::check(fromFile.variants.size() == 2 &&
                       fromFile.variants[1].active,
                   "file round-trip active variant");
    std::remove(path.c_str());
}

TEST("domain: user-dll move routine name round-trips through XML") {
    Model orig = makeSampleModel();
    orig.moves[0].inputs.type = MoveType::UserDll;
    orig.moves[0].inputs.userDllRoutine = "externalMove";

    const std::string xml = saveModelToString(orig);
    dvatest::check(xml.find("userDllRoutine=\"externalMove\"") != std::string::npos,
                   "user-dll move routine serialized");

    Model back;
    dvatest::check(loadModelFromString(back, xml), "load user-dll move XML");
    dvatest::check(back.moves.size() == orig.moves.size(), "move count");
    dvatest::check(back.moves[0].inputs.type == MoveType::UserDll,
                   "user-dll move type round-trips");
    dvatest::check(back.moves[0].inputs.userDllRoutine == "externalMove",
                   "user-dll move routine round-trips");
}

TEST("domain: empty model serializes required root collections") {
    Model orig;
    orig.assemblyName = "EmptyAssembly";

    const std::string xml = saveModelToString(orig);
    dvatest::check(xml.find("version=\"1\"") != std::string::npos,
                   "empty model root version present");
    dvatest::check(xml.find("<Parts/>") != std::string::npos,
                   "empty model Parts collection present");
    dvatest::check(xml.find("<Moves/>") != std::string::npos,
                   "empty model Moves collection present");
    dvatest::check(xml.find("<Measures/>") != std::string::npos,
                   "empty model Measures collection present");
    dvatest::check(xml.find("<Variants/>") != std::string::npos,
                   "empty model Variants collection present");

    Model loaded;
    dvatest::check(loadModelFromString(loaded, xml),
                   "empty model XML loads");
    dvatest::check(loaded.assemblyName == orig.assemblyName,
                   "empty model assemblyName");
    dvatest::check(loaded.parts.empty(), "empty model parts");
    dvatest::check(loaded.moves.empty(), "empty model moves");
    dvatest::check(loaded.measures.empty(), "empty model measures");
    dvatest::check(loaded.variants.empty(), "empty model variants");
}

TEST("domain: XML prolog comments and processing instructions load") {
    const Model orig = makeSampleModel();
    const std::string savedXml = saveModelToString(orig);
    const auto rootPos = savedXml.find("<OpenDVAModel");
    dvatest::check(rootPos != std::string::npos, "saved XML root fixture exists");
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<?opendva generated-by=\"test\"?>\n"
        "<!-- model prolog comment -->\n" +
        savedXml.substr(rootPos);

    Model loaded;
    dvatest::check(loadModelFromString(loaded, xml),
                   "XML prolog with PI and comment loads");
    checkModelStillMatches(loaded, orig,
                           "prolog load");
}

TEST("domain: UTF-8 BOM prefixed XML model loads") {
    const Model orig = makeSampleModel();
    const std::string xml = std::string("\xEF\xBB\xBF") +
                            saveModelToString(orig);

    Model loaded;
    dvatest::check(loadModelFromString(loaded, xml),
                   "UTF-8 BOM prefixed XML model loads");
    checkModelStillMatches(loaded, orig,
                           "BOM-prefixed XML load");
}

TEST("domain: XML epilog comments and processing instructions load") {
    const Model orig = makeSampleModel();
    const std::string xml =
        saveModelToString(orig) +
        "<!-- model epilog comment -->\n"
        "<?opendva checked-by=\"test\"?>\n";

    Model loaded;
    dvatest::check(loadModelFromString(loaded, xml),
                   "XML epilog with comment and PI loads");
    checkModelStillMatches(loaded, orig,
                           "epilog load");
}

TEST("domain: XML content processing instructions load") {
    const Model orig = makeSampleModel();
    const std::string savedXml = saveModelToString(orig);
    const std::string marker = "<Moves>";
    const auto movesPos = savedXml.find(marker);
    dvatest::check(movesPos != std::string::npos, "saved XML Moves fixture exists");
    const std::string xml =
        savedXml.substr(0, movesPos) +
        "<?opendva section=\"moves\"?>\n" +
        savedXml.substr(movesPos);

    Model loaded;
    dvatest::check(loadModelFromString(loaded, xml),
                   "XML content PI loads");
    checkModelStillMatches(loaded, orig,
                           "content PI load");
}

TEST("domain: invalid XML documents fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badDocuments = {
        {"empty document", ""},
        {"whitespace document", " \t\r\n "},
        {"prolog-only document", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"},
        {"wrong root tag",
         "<NotOpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves/><Measures/><Variants/>"
         "</NotOpenDVAModel>"},
    };

    for (const auto& c : badDocuments) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, c.second),
                       "invalid XML document with " + c.first + " is rejected");
        checkModelStillMatches(loaded, original,
                               "failed invalid XML document with " + c.first + " load");
    }
}

TEST("domain: invalid model files fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;

    const std::string missingPath =
        (std::filesystem::temp_directory_path() / "opendva_missing_model_file.xml").string();
    std::remove(missingPath.c_str());
    dvatest::check(!loadModel(loaded, missingPath),
                   "missing model file is rejected");
    checkModelStillMatches(loaded, original, "failed missing model file load");

    const std::string invalidPath =
        (std::filesystem::temp_directory_path() / "opendva_invalid_model_file.xml").string();
    {
        std::ofstream os(invalidPath, std::ios::binary);
        os << "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>";
        dvatest::check(static_cast<bool>(os), "invalid model file fixture writes");
    }

    loaded = original;
    dvatest::check(!loadModel(loaded, invalidPath),
                   "invalid model file is rejected");
    checkModelStillMatches(loaded, original, "failed invalid model file load");
    std::remove(invalidPath.c_str());
}

TEST("domain: saveModel reports file I/O failures") {
    const Model model = makeSampleModel();
    const std::string directoryPath = std::filesystem::temp_directory_path().string();

    dvatest::check(!saveModel(model, directoryPath),
                   "saveModel rejects a directory path");
}

TEST("domain: late XML schema failures leave the target model unchanged") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml = makeLateInvalidVariantXml();

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "late invalid variant XML is rejected");
    checkModelStillMatches(loaded, original, "late failed load");
}

TEST("domain: late invalid model files leave the target model unchanged") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml = makeLateInvalidVariantXml();

    const std::string invalidPath =
        (std::filesystem::temp_directory_path() / "opendva_late_invalid_model_file.xml").string();
    {
        std::ofstream os(invalidPath, std::ios::binary);
        os << badXml;
        dvatest::check(static_cast<bool>(os), "late invalid model file fixture writes");
    }

    dvatest::check(!loadModel(loaded, invalidPath),
                   "late invalid model file is rejected");
    checkModelStillMatches(loaded, original, "late failed file load");
    std::remove(invalidPath.c_str());
}

TEST("domain: invalid numeric XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts><Part id=\"abc\" cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "<Moves/><Measures/><Variants/>"
        "</OpenDVAModel>";

    bool threw = false;
    bool ok = true;
    try {
        ok = loadModelFromString(loaded, badXml);
    } catch (...) {
        threw = true;
    }

    dvatest::check(!threw, "invalid numeric XML does not throw");
    dvatest::check(!ok, "invalid numeric XML is rejected");
    checkModelStillMatches(loaded, original, "failed invalid numeric XML load");
}

TEST("domain: invalid enum XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badEnums = {
        {"out of range",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points><Point id=\"101\" kind=\"999\" holeType=\"0\"/></Points>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"signed integer",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points><Point id=\"101\" kind=\"+0\" holeType=\"0\" diameter=\"1\">"
         "<Position x=\"0\" y=\"0\" z=\"0\"/>"
         "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
         "</Point></Points>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badEnums) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "invalid enum XML with " + c.first + " is rejected");
        checkModelStillMatches(loaded, original, "failed enum load with " + c.first);
    }
}

TEST("domain: missing required point vector XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badPoints = {
        {"Position",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points>"
         "<Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\">"
         "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
         "</Point>"
         "</Points>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"Ijk",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points>"
         "<Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\">"
         "<Position x=\"0\" y=\"0\" z=\"0\"/>"
         "</Point>"
         "</Points>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badPoints) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required point " + c.first + " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required point " + c.first + " load");
    }
}

TEST("domain: missing required feature child XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badFeatures = {
        {"DefiningPoints",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Features>"
         "<Feature id=\"201\" kind=\"0\">"
         "<Mesh feature=\"201\" meshNodeNum=\"0\" cadPtNum=\"0\" version=\"1\"/>"
         "</Feature>"
         "</Features>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"Mesh",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Features>"
         "<Feature id=\"201\" kind=\"0\">"
         "<DefiningPoints><Ref id=\"101\"/></DefiningPoints>"
         "</Feature>"
         "</Features>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badFeatures) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required feature " + c.first + " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required feature " + c.first + " load");
    }
}

TEST("domain: missing required part collection XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badParts = {
        {"Points",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Features/>"
         "<Tolerances/>"
         "<Gdts/>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"Features",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points/>"
         "<Tolerances/>"
         "<Gdts/>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"Tolerances",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points/>"
         "<Features/>"
         "<Gdts/>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"Gdts",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points/>"
         "<Features/>"
         "<Tolerances/>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badParts) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required part " + c.first + " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required part " + c.first + " load");
    }
}

TEST("domain: missing required move pair child XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badPairs = {
        {"ObjectPoint",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Moves>"
         "<Move id=\"301\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\">"
         "<Pairs><Pair>"
         "<TargetPoint x=\"1\" y=\"0\" z=\"0\"/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "</Pair></Pairs>"
         "<Float active=\"0\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"0\" angleOffsetDeg=\"0\"/>"
         "</Inputs>"
         "<MoveParts/>"
         "</Move>"
         "</Moves>"
         "</OpenDVAModel>"},
        {"TargetPoint",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Moves>"
         "<Move id=\"301\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\">"
         "<Pairs><Pair>"
         "<ObjectPoint x=\"0\" y=\"0\" z=\"0\"/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "</Pair></Pairs>"
         "<Float active=\"0\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"0\" angleOffsetDeg=\"0\"/>"
         "</Inputs>"
         "<MoveParts/>"
         "</Move>"
         "</Moves>"
         "</OpenDVAModel>"},
        {"Direction",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Moves>"
         "<Move id=\"301\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\">"
         "<Pairs><Pair>"
         "<ObjectPoint x=\"0\" y=\"0\" z=\"0\"/>"
         "<TargetPoint x=\"1\" y=\"0\" z=\"0\"/>"
         "</Pair></Pairs>"
         "<Float active=\"0\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"0\" angleOffsetDeg=\"0\"/>"
         "</Inputs>"
         "<MoveParts/>"
         "</Move>"
         "</Moves>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badPairs) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required move pair " + c.first + " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required move pair " + c.first + " load");
    }
}

TEST("domain: missing required direction vector XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts/>"
        "<Moves>"
        "<Move id=\"301\" name=\"BadMove\" active=\"1\">"
        "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\">"
        "<Pairs><Pair>"
        "<ObjectPoint x=\"0\" y=\"0\" z=\"0\"/>"
        "<TargetPoint x=\"1\" y=\"0\" z=\"0\"/>"
        "<Direction type=\"0\"/>"
        "</Pair></Pairs>"
        "<Float active=\"0\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"0\" angleOffsetDeg=\"0\"/>"
        "</Inputs>"
        "<MoveParts/>"
        "</Move>"
        "</Moves>"
        "<Measures/><Variants/>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "missing required direction vector XML is rejected");
    checkModelStillMatches(loaded, original, "failed missing required direction vector load");
}

TEST("domain: missing required tolerance child XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badTolerances = {
        {"IR",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances>"
         "<Tolerance id=\"401\" name=\"BadTolerance\" active=\"1\">"
         "<Features><Ref id=\"201\"/></Features>"
         "</Tolerance>"
         "</Tolerances>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"Features",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances>"
         "<Tolerance id=\"401\" name=\"BadTolerance\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\">"
         "<Rands/>"
         "<Truncation minTrunc=\"-1\" maxTrunc=\"1\" active=\"0\"/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "</IR>"
         "</Tolerance>"
         "</Tolerances>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badTolerances) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required tolerance " + c.first + " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required tolerance " + c.first + " load");
    }
}

TEST("domain: missing required tolerance IR child XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badIrs = {
        {"Rands",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances>"
         "<Tolerance id=\"401\" name=\"BadTolerance\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\">"
         "<Truncation minTrunc=\"-1\" maxTrunc=\"1\" active=\"0\"/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "</IR>"
         "<Features><Ref id=\"201\"/></Features>"
         "</Tolerance>"
         "</Tolerances>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"Truncation",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances>"
         "<Tolerance id=\"401\" name=\"BadTolerance\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\">"
         "<Rands/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "</IR>"
         "<Features><Ref id=\"201\"/></Features>"
         "</Tolerance>"
         "</Tolerances>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"Direction",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances>"
         "<Tolerance id=\"401\" name=\"BadTolerance\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\">"
         "<Rands/>"
         "<Truncation minTrunc=\"-1\" maxTrunc=\"1\" active=\"0\"/>"
         "</IR>"
         "<Features><Ref id=\"201\"/></Features>"
         "</Tolerance>"
         "</Tolerances>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badIrs) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required tolerance IR " + c.first +
                           " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required tolerance IR " + c.first + " load");
    }
}

TEST("domain: missing required GD&T child XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badGdts = {
        {"Drf",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Gdts>"
         "<Gdt id=\"501\" name=\"BadGdt\" active=\"1\" type=\"0\" range=\"1\" diametrical=\"0\">"
         "<Features><Ref id=\"201\"/></Features>"
         "</Gdt>"
         "</Gdts>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
        {"Features",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Gdts>"
         "<Gdt id=\"501\" name=\"BadGdt\" active=\"1\" type=\"0\" range=\"1\" diametrical=\"0\">"
         "<Drf primary=\"201\" secondary=\"0\" tertiary=\"0\"/>"
         "</Gdt>"
         "</Gdts>"
         "</Part>"
         "</Parts>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badGdts) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required GD&T " + c.first + " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required GD&T " + c.first + " load");
    }
}

TEST("domain: missing required move child XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badMoves = {
        {"Inputs",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Moves>"
         "<Move id=\"301\" name=\"BadMove\" active=\"1\">"
         "<MoveParts><Ref id=\"10\"/></MoveParts>"
         "</Move>"
         "</Moves>"
         "</OpenDVAModel>"},
        {"MoveParts",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Moves>"
         "<Move id=\"301\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\">"
         "<Pairs/>"
         "<Float active=\"0\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"0\" angleOffsetDeg=\"0\"/>"
         "</Inputs>"
         "</Move>"
         "</Moves>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badMoves) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required move " + c.first + " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required move " + c.first + " load");
    }
}

TEST("domain: missing required move input child XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badInputs = {
        {"Pairs",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Moves>"
         "<Move id=\"301\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\">"
         "<Float active=\"0\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"0\" angleOffsetDeg=\"0\"/>"
         "</Inputs>"
         "<MoveParts><Ref id=\"10\"/></MoveParts>"
         "</Move>"
         "</Moves>"
         "</OpenDVAModel>"},
        {"Float",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Moves>"
         "<Move id=\"301\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\">"
         "<Pairs/>"
         "</Inputs>"
         "<MoveParts><Ref id=\"10\"/></MoveParts>"
         "</Move>"
         "</Moves>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badInputs) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required move input " + c.first +
                           " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required move input " + c.first + " load");
    }
}

TEST("domain: missing required measure child XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badMeasures = {
        {"Def",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Measures>"
         "<Measure id=\"701\" name=\"BadMeasure\"/>"
         "</Measures>"
         "</OpenDVAModel>"},
        {"InputPoints",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Measures>"
         "<Measure id=\"701\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputFeatures/>"
         "<Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure>"
         "</Measures>"
         "</OpenDVAModel>"},
        {"InputFeatures",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Measures>"
         "<Measure id=\"701\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/>"
         "<Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure>"
         "</Measures>"
         "</OpenDVAModel>"},
        {"Values",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Measures>"
         "<Measure id=\"701\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/>"
         "<InputFeatures/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure>"
         "</Measures>"
         "</OpenDVAModel>"},
        {"Direction",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Measures>"
         "<Measure id=\"701\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/>"
         "<InputFeatures/>"
         "<Values/>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure>"
         "</Measures>"
         "</OpenDVAModel>"},
        {"Spec",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Measures>"
         "<Measure id=\"701\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/>"
         "<InputFeatures/>"
         "<Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "</Def>"
         "</Measure>"
         "</Measures>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badMeasures) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required measure " + c.first +
                           " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required measure " + c.first + " load");
    }
}

TEST("domain: missing required variant collection XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badVariants = {
        {"Moves",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Variants>"
         "<Variant name=\"BadVariant\" active=\"1\">"
         "<Tolerances/>"
         "<Measures/>"
         "</Variant>"
         "</Variants>"
         "</OpenDVAModel>"},
        {"Tolerances",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Variants>"
         "<Variant name=\"BadVariant\" active=\"1\">"
         "<Moves/>"
         "<Measures/>"
         "</Variant>"
         "</Variants>"
         "</OpenDVAModel>"},
        {"Measures",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Variants>"
         "<Variant name=\"BadVariant\" active=\"1\">"
         "<Moves/>"
         "<Tolerances/>"
         "</Variant>"
         "</Variants>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badVariants) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "missing required variant " + c.first + " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required variant " + c.first + " load");
    }
}

TEST("domain: missing required model collection XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badModels = {
        {"Parts",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Moves/>"
         "<Measures/>"
         "<Variants/>"
         "</OpenDVAModel>"},
        {"Moves",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/>"
         "<Measures/>"
         "<Variants/>"
         "</OpenDVAModel>"},
        {"Measures",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/>"
         "<Moves/>"
         "<Variants/>"
         "</OpenDVAModel>"},
        {"Variants",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/>"
         "<Moves/>"
         "<Measures/>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badModels) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, c.second),
                       "missing required model " + c.first + " XML is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required model " + c.first + " load");
    }
}

TEST("domain: non-finite and partial numeric XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string nonFiniteXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"nan\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "<Features/>"
        "<Tolerances/>"
        "<Gdts/>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(nonFiniteXml)),
                   "non-finite numeric XML is rejected");
    checkModelStillMatches(loaded, original, "failed non-finite load");

    const std::string partialNumericXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts><Part id=\"10mm\" cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(partialNumericXml)),
                   "partial numeric XML is rejected");
    checkModelStillMatches(loaded, original, "failed partial numeric load");

    const std::string leadingWhitespaceNumericXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\" 1\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "<Features/>"
        "<Tolerances/>"
        "<Gdts/>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(leadingWhitespaceNumericXml)),
                   "leading whitespace numeric XML is rejected");
    checkModelStillMatches(loaded, original, "failed leading whitespace numeric load");

    const std::string hexNumericXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"0x1p0\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(hexNumericXml)),
                   "hexadecimal numeric XML is rejected");
    checkModelStillMatches(loaded, original, "failed hexadecimal numeric load");

    const std::string plusPrefixedNumericXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"+1\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(plusPrefixedNumericXml)),
                   "plus-prefixed numeric XML is rejected");
    checkModelStillMatches(loaded, original, "failed plus-prefixed numeric load");
}

TEST("domain: invalid boolean XML fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" holeType=\"0\" active=\"maybe\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "invalid boolean XML is rejected");
    checkModelStillMatches(loaded, original, "failed boolean load");
}

TEST("domain: missing required boolean XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badBooleans = {
        {"Point active",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points><Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\">"
         "<Position x=\"0\" y=\"0\" z=\"0\"/>"
         "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
         "</Point></Points>"
         "<Features/><Tolerances/><Gdts/>"
         "</Part></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Tolerance active",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points/><Features/>"
         "<Tolerances><Tolerance id=\"301\" name=\"BadTol\">"
         "<IR rangeScale=\"1\" geomRule=\"0\">"
         "<Rands/><Truncation minTrunc=\"-1\" maxTrunc=\"1\" active=\"0\"/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "</IR><Features/></Tolerance></Tolerances>"
         "<Gdts/></Part></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Truncation active",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points/><Features/>"
         "<Tolerances><Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\">"
         "<Rands/><Truncation minTrunc=\"-1\" maxTrunc=\"1\"/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "</IR><Features/></Tolerance></Tolerances>"
         "<Gdts/></Part></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Gdt active",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points/><Features/><Tolerances/>"
         "<Gdts><Gdt id=\"401\" name=\"BadGdt\" type=\"0\" range=\"1\" diametrical=\"0\">"
         "<Drf primary=\"0\" secondary=\"0\" tertiary=\"0\"/><Features/>"
         "</Gdt></Gdts></Part></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Gdt diametrical",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points/><Features/><Tolerances/>"
         "<Gdts><Gdt id=\"401\" name=\"BadGdt\" active=\"1\" type=\"0\" range=\"1\">"
         "<Drf primary=\"0\" secondary=\"0\" tertiary=\"0\"/><Features/>"
         "</Gdt></Gdts></Part></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Move active",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves><Move id=\"501\" name=\"BadMove\">"
         "<Inputs type=\"0\" searchAccuracy=\"1e-5\" maxIterations=\"500\" isNominalBuild=\"0\">"
         "<Pairs/><Float active=\"1\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"360\" angleOffsetDeg=\"0\"/>"
         "</Inputs><MoveParts/></Move></Moves><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Move Inputs isNominalBuild",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves><Move id=\"501\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"1e-5\" maxIterations=\"500\">"
         "<Pairs/><Float active=\"1\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"360\" angleOffsetDeg=\"0\"/>"
         "</Inputs><MoveParts/></Move></Moves><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Move Float active",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves><Move id=\"501\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"1e-5\" maxIterations=\"500\" isNominalBuild=\"0\">"
         "<Pairs/><Float sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"360\" angleOffsetDeg=\"0\"/>"
         "</Inputs><MoveParts/></Move></Moves><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Measure Def active",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves/><Measures><Measure id=\"601\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def></Measure></Measures><Variants/>"
         "</OpenDVAModel>"},
        {"Measure Def asOutput",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves/><Measures><Measure id=\"601\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def></Measure></Measures><Variants/>"
         "</OpenDVAModel>"},
        {"Spec uslActive",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves/><Measures><Measure id=\"601\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def></Measure></Measures><Variants/>"
         "</OpenDVAModel>"},
        {"Spec lslActive",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves/><Measures><Measure id=\"601\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" mode=\"0\"/>"
         "</Def></Measure></Measures><Variants/>"
         "</OpenDVAModel>"},
        {"Variant active",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves/><Measures/><Variants>"
         "<Variant name=\"BadVariant\"><Moves/><Tolerances/><Measures/></Variant>"
         "</Variants></OpenDVAModel>"},
    };

    for (const auto& c : badBooleans) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, c.second),
                       "missing required boolean " + c.first +
                           " XML attribute is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required boolean " + c.first + " load");
    }
}

TEST("domain: missing required string XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badStrings = {
        {"assemblyName",
         "<OpenDVAModel version=\"1\">"
         "<Parts/><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Part cadName",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" dcsName=\"BadDcs\">"
         "<Points/><Features/><Tolerances/><Gdts/>"
         "</Part></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Part dcsName",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\">"
         "<Points/><Features/><Tolerances/><Gdts/>"
         "</Part></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Tolerance name",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points/><Features/>"
         "<Tolerances><Tolerance id=\"301\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\">"
         "<Rands/><Truncation minTrunc=\"-1\" maxTrunc=\"1\" active=\"0\"/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "</IR><Features/></Tolerance></Tolerances>"
         "<Gdts/></Part></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Gdt name",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points/><Features/><Tolerances/>"
         "<Gdts><Gdt id=\"401\" active=\"1\" type=\"0\" range=\"1\" diametrical=\"0\">"
         "<Drf primary=\"0\" secondary=\"0\" tertiary=\"0\"/><Features/>"
         "</Gdt></Gdts></Part></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Move name",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves><Move id=\"501\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"1e-5\" maxIterations=\"500\" isNominalBuild=\"0\">"
         "<Pairs/><Float active=\"1\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"360\" angleOffsetDeg=\"0\"/>"
         "</Inputs><MoveParts/></Move></Moves><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"Measure name",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves/><Measures><Measure id=\"601\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def></Measure></Measures><Variants/>"
         "</OpenDVAModel>"},
        {"Measure Def equation",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves/><Measures><Measure id=\"601\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def></Measure></Measures><Variants/>"
         "</OpenDVAModel>"},
        {"Variant name",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts/><Moves/><Measures/><Variants>"
         "<Variant active=\"1\"><Moves/><Tolerances/><Measures/></Variant>"
         "</Variants></OpenDVAModel>"},
    };

    for (const auto& c : badStrings) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, c.second),
                       "missing required string " + c.first +
                           " XML attribute is rejected");
        checkModelStillMatches(loaded, original,
                               "failed missing required string " + c.first + " load");
    }
}

TEST("domain: missing required root version XML attribute fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel assemblyName=\"Bad\">"
        "<Parts/><Moves/><Measures/><Variants/>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "missing required root version XML attribute is rejected");
    checkModelStillMatches(loaded, original, "failed missing root version load");
}

TEST("domain: unsupported root version XML attribute fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"2\" assemblyName=\"Bad\">"
        "<Parts/><Moves/><Measures/><Variants/>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "unsupported root version XML attribute is rejected");
    checkModelStillMatches(loaded, original, "failed unsupported root version load");
}

TEST("domain: duplicate XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\" assemblyName=\"Worse\">"
        "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "duplicate XML attribute is rejected");
    checkModelStillMatches(loaded, original, "failed duplicate attribute load");
}

TEST("domain: duplicate XML child elements fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badPointXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Position x=\"1\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badPointXml)),
                   "duplicate XML point child element is rejected");
    checkModelStillMatches(loaded, original, "failed duplicate point child load");

    const std::string badRootXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "<Parts><Part id=\"11\" cadName=\"OtherCad\" dcsName=\"OtherDcs\"/></Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, badRootXml),
                   "duplicate XML root collection child element is rejected");
    checkModelStillMatches(loaded, original, "failed duplicate root child load");
}

TEST("domain: trailing XML content fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "</OpenDVAModel>"
        "<OpenDVAModel version=\"1\" assemblyName=\"Ignored\"/>";

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "trailing XML content is rejected");
    checkModelStillMatches(loaded, original, "failed trailing content load");
}

TEST("domain: unexpected XML collection child tags fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts><NotPart id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "unexpected Parts child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Parts child load");

    const std::string badPointXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<NotPoint id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</NotPoint>"
        "</Points>"
        "<Features/>"
        "<Tolerances/>"
        "<Gdts/>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badPointXml)),
                   "unexpected Points child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Points child load");

    const std::string badFeatureXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Features>"
        "<NotFeature id=\"201\" kind=\"0\"><DefiningPoints/></NotFeature>"
        "</Features>"
        "<Points/>"
        "<Tolerances/>"
        "<Gdts/>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badFeatureXml)),
                   "unexpected Features child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Features child load");

    const std::string badToleranceXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<NotTolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\" geomRule=\"0\"/>"
        "<Features/>"
        "</NotTolerance>"
        "</Tolerances>"
        "<Points/>"
        "<Features/>"
        "<Gdts/>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badToleranceXml)),
                   "unexpected Tolerances child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Tolerances child load");

    const std::string badGdtXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Gdts>"
        "<NotGdt id=\"401\" name=\"BadGdt\" active=\"1\" type=\"1\" range=\"0.2\" "
        "diametrical=\"0\">"
        "<Drf primary=\"0\" secondary=\"0\" tertiary=\"0\"/>"
        "<Features/>"
        "</NotGdt>"
        "</Gdts>"
        "<Points/>"
        "<Features/>"
        "<Tolerances/>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badGdtXml)),
                   "unexpected Gdts child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Gdts child load");

    const std::string badMoveXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Moves><NotMove id=\"501\" name=\"BadMove\" active=\"1\"/></Moves>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badMoveXml)),
                   "unexpected Moves child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Moves child load");

    const std::string badMeasureXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures><NotMeasure id=\"601\" name=\"BadMeasure\"/></Measures>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badMeasureXml)),
                   "unexpected Measures child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Measures child load");

    const std::string badVariantXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Variants><NotVariant name=\"BadVariant\" active=\"1\"/></Variants>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badVariantXml)),
                   "unexpected Variants child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Variants child load");

    const std::string badRandXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"701\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\" geomRule=\"0\">"
        "<Rands><NotRand distribution=\"0\" range=\"1\" offset=\"0\" sigmaNum=\"3\"/>"
        "</Rands>"
        "<Truncation minTrunc=\"-1\" maxTrunc=\"1\" active=\"0\"/>"
        "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "<Points/>"
        "<Features/>"
        "<Gdts/>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badRandXml)),
                   "unexpected Rands child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Rands child load");

    const std::string badPairXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Moves>"
        "<Move id=\"801\" name=\"BadMove\" active=\"1\">"
        "<Inputs type=\"0\" searchAccuracy=\"0.01\" maxIterations=\"10\" isNominalBuild=\"0\">"
        "<Pairs>"
        "<NotPair>"
        "<ObjectPoint x=\"0\" y=\"0\" z=\"0\"/>"
        "<TargetPoint x=\"1\" y=\"0\" z=\"0\"/>"
        "<Direction type=\"0\"><Ijk x=\"1\" y=\"0\" z=\"0\"/></Direction>"
        "</NotPair>"
        "</Pairs>"
        "<Float active=\"0\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"0\" angleOffsetDeg=\"0\"/>"
        "</Inputs>"
        "<MoveParts/>"
        "</Move>"
        "</Moves>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badPairXml)),
                   "unexpected Pairs child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Pairs child load");

    const std::string badRefXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Features>"
        "<Feature id=\"901\" kind=\"0\">"
        "<DefiningPoints><NotRef id=\"101\"/></DefiningPoints>"
        "<Mesh feature=\"901\" meshNodeNum=\"0\" cadPtNum=\"0\" version=\"0\"/>"
        "</Feature>"
        "</Features>"
        "<Points/>"
        "<Tolerances/>"
        "<Gdts/>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badRefXml)),
                   "unexpected Ref-list child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Ref-list child load");

    const std::string badValueXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures>"
        "<Measure id=\"1001\" name=\"BadMeasure\">"
        "<Def type=\"25\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
        "<InputPoints/>"
        "<InputFeatures/>"
        "<Values><NotValue v=\"1\"/></Values>"
        "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
        "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
        "</Def>"
        "</Measure>"
        "</Measures>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badValueXml)),
                   "unexpected Values child XML tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected Values child load");

    const std::string badDirectionRefXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures>"
        "<Measure id=\"1101\" name=\"BadMeasure\">"
        "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
        "<InputPoints/>"
        "<InputFeatures/>"
        "<Values/>"
        "<Direction type=\"4\"><RefPoints><NotRef id=\"101\"/></RefPoints></Direction>"
        "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
        "</Def>"
        "</Measure>"
        "</Measures>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badDirectionRefXml)),
                   "unexpected Direction RefPoints child XML tag is rejected");
    checkModelStillMatches(loaded, original,
                           "failed unexpected Direction RefPoints child load");
}

TEST("domain: unexpected XML object child tags fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badRootChildXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Unknown/>"
        "<Parts/>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, badRootChildXml),
                   "unexpected root XML child tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected root child load");

    const std::string badPointChildXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "<Bogus/>"
        "</Point>"
        "</Points>"
        "<Features/>"
        "<Tolerances/>"
        "<Gdts/>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badPointChildXml)),
                   "unexpected point XML child tag is rejected");
    checkModelStillMatches(loaded, original, "failed unexpected point child load");

    const std::vector<std::pair<std::string, std::string>> badObjectChildren = {
        {"Part",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\"><Bogus/></Part>"
         "</Parts></OpenDVAModel>"},
        {"Feature",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Features><Feature id=\"201\" kind=\"0\"><Bogus/></Feature></Features>"
         "<Points/><Tolerances/><Gdts/>"
         "</Part></Parts></OpenDVAModel>"},
        {"Tolerance",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances><Tolerance id=\"301\" name=\"BadTol\" active=\"1\"><Bogus/></Tolerance></Tolerances>"
         "<Points/><Features/><Gdts/>"
         "</Part></Parts></OpenDVAModel>"},
        {"Gdt",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Gdts><Gdt id=\"401\" name=\"BadGdt\" active=\"1\" type=\"0\" range=\"1\" diametrical=\"0\"><Bogus/></Gdt></Gdts>"
         "<Points/><Features/><Tolerances/>"
         "</Part></Parts></OpenDVAModel>"},
        {"Move",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Moves>"
         "<Move id=\"501\" name=\"BadMove\" active=\"1\"><Bogus/></Move>"
         "</Moves></OpenDVAModel>"},
        {"Move Pair",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Moves>"
         "<Move id=\"501\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\">"
         "<Pairs><Pair><Bogus/></Pair></Pairs>"
         "<Float active=\"0\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"0\" angleOffsetDeg=\"0\"/>"
         "</Inputs>"
         "<MoveParts/>"
         "</Move></Moves></OpenDVAModel>"},
        {"Measure",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures>"
         "<Measure id=\"1101\" name=\"BadMeasure\"><Bogus/></Measure>"
         "</Measures></OpenDVAModel>"},
        {"Variant",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Variants>"
         "<Variant name=\"BadVariant\" active=\"1\"><Bogus/></Variant>"
         "</Variants></OpenDVAModel>"},
    };

    for (const auto& c : badObjectChildren) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "unexpected " + c.first + " XML child tag is rejected");
        checkModelStillMatches(loaded, original,
                               "failed unexpected " + c.first + " child load");
    }
}

TEST("domain: unexpected XML nested child tags fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badNestedChildren = {
        {"Position",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points><Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\">"
         "<Position x=\"0\" y=\"0\" z=\"0\"><Bogus/></Position>"
         "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
         "</Point></Points>"
         "<Features/><Tolerances/><Gdts/>"
         "</Part></Parts></OpenDVAModel>"},
        {"Mesh",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Features><Feature id=\"201\" kind=\"0\"><DefiningPoints/>"
         "<Mesh feature=\"201\" meshNodeNum=\"1\" cadPtNum=\"2\" version=\"3\"><Bogus/></Mesh></Feature></Features>"
         "<Points/><Tolerances/><Gdts/>"
         "</Part></Parts></OpenDVAModel>"},
        {"Tolerance IR",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances><Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\"><Bogus/></IR>"
         "<Features/>"
         "</Tolerance></Tolerances>"
         "<Points/><Features/><Gdts/>"
         "</Part></Parts></OpenDVAModel>"},
        {"Rand",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances><Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\"><Rands>"
         "<Rand distribution=\"0\" range=\"1\" offset=\"0\" sigmaNum=\"6\"><Bogus/></Rand>"
         "</Rands><Truncation minTrunc=\"-1\" maxTrunc=\"1\" active=\"0\"/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction></IR>"
         "<Features/>"
         "</Tolerance></Tolerances>"
         "<Points/><Features/><Gdts/>"
         "</Part></Parts></OpenDVAModel>"},
        {"Truncation",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances><Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\"><Rands/>"
         "<Truncation minTrunc=\"0\" maxTrunc=\"1\" active=\"1\"><Bogus/></Truncation>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction></IR>"
         "<Features/>"
         "</Tolerance></Tolerances>"
         "<Points/><Features/><Gdts/>"
         "</Part></Parts></OpenDVAModel>"},
        {"Direction",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures><Measure id=\"1101\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/><Bogus/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure></Measures></OpenDVAModel>"},
        {"Direction Ref",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures><Measure id=\"1101\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"4\"><RefPoints><Ref id=\"101\"><Bogus/></Ref></RefPoints></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure></Measures></OpenDVAModel>"},
        {"Gdt Drf",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Gdts><Gdt id=\"401\" name=\"BadGdt\" active=\"1\" type=\"0\" range=\"1\" diametrical=\"0\">"
         "<Drf primary=\"201\" secondary=\"0\" tertiary=\"0\"><Bogus/></Drf>"
         "<Features/>"
         "</Gdt></Gdts>"
         "<Points/><Features/><Tolerances/>"
         "</Part></Parts></OpenDVAModel>"},
        {"Move Inputs",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Moves><Move id=\"501\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\"><Bogus/></Inputs>"
         "<MoveParts/>"
         "</Move></Moves></OpenDVAModel>"},
        {"Move Float",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Moves><Move id=\"501\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"10\" isNominalBuild=\"0\">"
         "<Pairs/>"
         "<Float active=\"1\" sigmaNumber=\"6\" rangeScale=\"1\" angleRangeDeg=\"2\" angleOffsetDeg=\"0\"><Bogus/></Float>"
         "</Inputs>"
         "<MoveParts/>"
         "</Move></Moves></OpenDVAModel>"},
        {"Measure Def",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures><Measure id=\"1101\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\"><Bogus/></Def>"
         "</Measure></Measures></OpenDVAModel>"},
        {"Measure Value",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures><Measure id=\"1101\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/>"
         "<Values><Value v=\"1\"><Bogus/></Value></Values>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure></Measures></OpenDVAModel>"},
        {"Measure Spec",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures><Measure id=\"1101\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"><Bogus/></Spec>"
         "</Def>"
         "</Measure></Measures></OpenDVAModel>"},
        {"Variant Ref",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Variants>"
         "<Variant name=\"BadVariant\" active=\"1\"><Moves><Ref id=\"501\"><Bogus/></Ref></Moves><Tolerances/><Measures/></Variant>"
         "</Variants></OpenDVAModel>"},
    };

    for (const auto& c : badNestedChildren) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "unexpected " + c.first + " nested XML child tag is rejected");
        checkModelStillMatches(loaded, original,
                               "failed unexpected " + c.first + " nested child load");
    }
}

TEST("domain: unexpected XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badAttrs = {
        {"root",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\" mystery=\"1\"><Parts/></OpenDVAModel>"},
        {"collection",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts mystery=\"1\"/>"
         "<Moves/><Measures/><Variants/></OpenDVAModel>"},
        {"Point",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points><Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\" active=\"1\" mystery=\"1\">"
         "<Position x=\"0\" y=\"0\" z=\"0\"/><Ijk x=\"0\" y=\"0\" z=\"1\"/>"
         "</Point></Points><Features/><Tolerances/><Gdts/>"
         "</Part></Parts><Moves/><Measures/><Variants/></OpenDVAModel>"},
        {"Vec3",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points><Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\" active=\"1\">"
         "<Position x=\"0\" y=\"0\" z=\"0\" mystery=\"1\"/>"
         "<Ijk x=\"0\" y=\"0\" z=\"1\"/></Point></Points><Features/><Tolerances/><Gdts/>"
         "</Part></Parts><Moves/><Measures/><Variants/></OpenDVAModel>"},
        {"Rand",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Tolerances><Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
         "<IR rangeScale=\"1\" geomRule=\"0\"><Rands>"
         "<Rand distribution=\"0\" range=\"1\" offset=\"0\" sigmaNum=\"6\" mystery=\"1\"/>"
         "</Rands><Truncation minTrunc=\"-1\" maxTrunc=\"1\" active=\"0\"/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction></IR>"
         "<Features/>"
         "</Tolerance></Tolerances>"
         "<Points/><Features/><Gdts/></Part></Parts><Moves/><Measures/><Variants/></OpenDVAModel>"},
        {"Direction",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures><Measure id=\"1101\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\" mystery=\"1\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure></Measures><Parts/><Moves/><Variants/></OpenDVAModel>"},
        {"Ref",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Variants>"
         "<Variant name=\"BadVariant\" active=\"1\"><Moves><Ref id=\"501\" mystery=\"1\"/></Moves>"
         "<Tolerances/><Measures/></Variant>"
         "</Variants><Parts/><Moves/><Measures/></OpenDVAModel>"},
        {"Value",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures><Measure id=\"1101\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/>"
         "<Values><Value v=\"1\" mystery=\"1\"/></Values>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure></Measures><Parts/><Moves/><Variants/></OpenDVAModel>"},
        {"Spec",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures><Measure id=\"1101\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/><Values/>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\" mystery=\"1\"/>"
         "</Def>"
         "</Measure></Measures><Parts/><Moves/><Variants/></OpenDVAModel>"},
    };

    for (const auto& c : badAttrs) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "unexpected " + c.first + " XML attribute is rejected");
        checkModelStillMatches(loaded, original,
                               "failed unexpected " + c.first + " attribute load");
    }
}

TEST("domain: unexpected XML text content fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badText = {
        {"root",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">text<Parts/></OpenDVAModel>"},
        {"collection",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts>text</Parts>"
         "<Moves/><Measures/><Variants/></OpenDVAModel>"},
        {"object",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts>"
         "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">text</Part>"
         "</Parts><Moves/><Measures/><Variants/></OpenDVAModel>"},
        {"leaf",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points><Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"1\" active=\"1\">"
         "<Position x=\"0\" y=\"0\" z=\"0\">text</Position>"
         "<Ijk x=\"0\" y=\"0\" z=\"1\"/></Point></Points><Features/><Tolerances/><Gdts/>"
         "</Part></Parts><Moves/><Measures/><Variants/></OpenDVAModel>"},
        {"id list",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Variants>"
         "<Variant name=\"BadVariant\" active=\"1\"><Moves>text</Moves><Tolerances/><Measures/></Variant>"
         "</Variants><Parts/><Moves/><Measures/></OpenDVAModel>"},
        {"value list",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Measures><Measure id=\"1101\" name=\"BadMeasure\">"
         "<Def type=\"0\" dirMode=\"1\" scale=\"1\" active=\"1\" asOutput=\"1\" equation=\"\">"
         "<InputPoints/><InputFeatures/>"
         "<Values>text</Values>"
         "<Direction type=\"0\"><Ijk x=\"0\" y=\"0\" z=\"1\"/></Direction>"
         "<Spec usl=\"1\" lsl=\"0\" uslActive=\"0\" lslActive=\"0\" mode=\"0\"/>"
         "</Def>"
         "</Measure></Measures><Parts/><Moves/><Variants/></OpenDVAModel>"},
    };

    for (const auto& c : badText) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "unexpected " + c.first + " XML text content is rejected");
        checkModelStillMatches(loaded, original,
                               "failed unexpected " + c.first + " text load");
    }
}

TEST("domain: invalid XML entities fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badEntities = {
        {"unknown attribute entity",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad&bogus;\"><Parts/></OpenDVAModel>"},
        {"unterminated attribute entity",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad & value\"><Parts/></OpenDVAModel>"},
        {"unknown text entity",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">&bogus;<Parts/></OpenDVAModel>"},
        {"unterminated text entity",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">& text<Parts/></OpenDVAModel>"},
    };

    for (const auto& c : badEntities) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, c.second),
                       "invalid " + c.first + " is rejected");
        checkModelStillMatches(loaded, original,
                               "failed invalid " + c.first + " load");
    }
}

TEST("domain: XML numeric character references load in string attributes") {
    Model loaded;
    const std::string xml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Gap &#65;&#x26; Slot\">"
        "<Parts/><Moves/><Measures/><Variants/>"
        "</OpenDVAModel>";

    dvatest::check(loadModelFromString(loaded, xml),
                   "XML numeric character references load");
    dvatest::check(loaded.assemblyName == "Gap A& Slot",
                   "XML numeric character references decode");
}

TEST("domain: raw less-than in XML attributes fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad < raw\"><Parts/></OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "raw less-than in XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed raw less-than attribute load");
}

TEST("domain: raw XML control characters fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    std::string badXml = "<OpenDVAModel version=\"1\" assemblyName=\"Bad";
    badXml.push_back(static_cast<char>(0x01));
    badXml += "Name\"><Parts/></OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "raw XML control character in attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed raw XML control character load");
}

TEST("domain: invalid raw UTF-8 XML bytes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    std::string badXml = "<OpenDVAModel version=\"1\" assemblyName=\"Bad";
    badXml.push_back(static_cast<char>(0xFF));
    badXml += "Name\"><Parts/><Moves/><Measures/><Variants/></OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "invalid raw UTF-8 XML byte is rejected");
    checkModelStillMatches(loaded, original,
                           "failed invalid UTF-8 XML load");
}

TEST("domain: malformed XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts><Part id=\"10\"cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, badXml),
                   "missing whitespace between XML attributes is rejected");
    checkModelStillMatches(loaded, original,
                           "failed malformed XML attribute load");
}

TEST("domain: malformed XML names fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badNames = {
        {"element",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><1Part/></Parts><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"attribute",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\" 1bad=\"value\">"
         "<Parts/><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badNames) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, c.second),
                       "malformed XML " + c.first + " name is rejected");
        checkModelStillMatches(loaded, original,
                               "failed malformed XML " + c.first + " name load");
    }
}

TEST("domain: malformed XML comments fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    std::vector<std::pair<std::string, std::string>> badComments = {
        {"double hyphen",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<!-- bad -- comment -->"
         "<Parts/><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
        {"trailing hyphen",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<!-- bad --->"
         "<Parts/><Moves/><Measures/><Variants/>"
         "</OpenDVAModel>"},
    };
    std::string badCommentControl = "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
                                    "<!-- bad";
    badCommentControl.push_back(static_cast<char>(0x01));
    badCommentControl += "comment -->"
                         "<Parts/><Moves/><Measures/><Variants/>"
                         "</OpenDVAModel>";
    badComments.push_back({"raw control character", badCommentControl});

    for (const auto& c : badComments) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, c.second),
                       "malformed XML comment with " + c.first + " is rejected");
        checkModelStillMatches(loaded, original,
                               "failed malformed XML comment with " + c.first + " load");
    }
}

TEST("domain: malformed XML prolog fails without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    std::vector<std::pair<std::string, std::string>> badPrologs = {
        {"raw less-than",
         "<?xml version=\"1.0<bad\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"empty processing instruction target",
         "<? ?><OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"invalid processing instruction target",
         "<?bad?target?><OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"duplicate XML declaration",
         "<?xml version=\"1.0\"?><?xml version=\"1.0\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"XML declaration after comment",
         "<!-- comment --><?xml version=\"1.0\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"mixed-case XML declaration",
         "<?Xml version=\"1.0\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"missing XML declaration contents",
         "<?xml?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"missing XML declaration version",
         "<?xml encoding=\"UTF-8\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"duplicate XML declaration version",
         "<?xml version=\"1.0\" version=\"1.1\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"unsupported XML declaration version",
         "<?xml version=\"2.0\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"standalone before XML declaration encoding",
         "<?xml version=\"1.0\" standalone=\"yes\" encoding=\"UTF-8\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"invalid XML declaration standalone value",
         "<?xml version=\"1.0\" standalone=\"maybe\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"invalid XML declaration encoding name",
         "<?xml version=\"1.0\" encoding=\"1bad\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"unsupported XML declaration encoding",
         "<?xml version=\"1.0\" encoding=\"UTF-16\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
        {"missing XML declaration pseudo-attribute whitespace",
         "<?xml version=\"1.0\"encoding=\"UTF-8\"?>"
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>"},
    };
    std::string badPiControl = "<?pi bad";
    badPiControl.push_back(static_cast<char>(0x01));
    badPiControl += "?>"
                    "<OpenDVAModel version=\"1\" assemblyName=\"Bad\"><Parts/></OpenDVAModel>";
    badPrologs.push_back({"raw processing instruction control character", badPiControl});

    for (const auto& c : badPrologs) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, c.second),
                       "malformed XML prolog with " + c.first + " is rejected");
        checkModelStillMatches(loaded, original,
                               "failed malformed XML prolog with " + c.first + " load");
    }
}

TEST("domain: empty required numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::vector<std::pair<std::string, std::string>> badValues = {
        {"unsigned id",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"\" cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
         "</OpenDVAModel>"},
        {"enum",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points><Point id=\"101\" kind=\"\" holeType=\"0\" diameter=\"1\"/></Points>"
         "</Part></Parts>"
         "</OpenDVAModel>"},
        {"double",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Parts><Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
         "<Points><Point id=\"101\" kind=\"0\" holeType=\"0\" diameter=\"\"/></Points>"
         "</Part></Parts>"
         "</OpenDVAModel>"},
        {"integer",
         "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
         "<Moves><Move id=\"501\" name=\"BadMove\" active=\"1\">"
         "<Inputs type=\"0\" searchAccuracy=\"0.001\" maxIterations=\"\"/>"
         "</Move></Moves>"
         "</OpenDVAModel>"},
    };

    for (const auto& c : badValues) {
        loaded = original;
        dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(c.second)),
                       "empty required " + c.first + " XML attribute is rejected");
        checkModelStillMatches(loaded, original,
                               "failed empty required " + c.first + " load");
    }
}

TEST("domain: negative unsigned XML ids fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts><Part id=\"-1\" cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "negative unsigned XML id is rejected");
    checkModelStillMatches(loaded, original,
                           "failed negative id load");
}

TEST("domain: signed unsigned XML ids fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts><Part id=\"+1\" cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "signed unsigned XML id is rejected");
    checkModelStillMatches(loaded, original,
                           "failed signed unsigned id load");
}

TEST("domain: missing required XML ids fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts><Part cadName=\"BadCad\" dcsName=\"BadDcs\"/></Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing required XML id is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing id load");
}

TEST("domain: missing Vec3 XML coordinates fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" holeType=\"0\">"
        "<Position x=\"0\" y=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing Vec3 XML coordinate is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing Vec3 coordinate load");
}

TEST("domain: missing double list XML values fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures>"
        "<Measure id=\"601\" name=\"BadMeasure\">"
        "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\">"
        "<Values><Value/></Values>"
        "</Def>"
        "</Measure>"
        "</Measures>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing double list XML value is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing double list value load");
}

TEST("domain: missing point numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" holeType=\"0\" active=\"1\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing point numeric XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing point numeric attribute load");
}

TEST("domain: missing point enum XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string missingKindXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" holeType=\"0\" diameter=\"1\" active=\"1\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingKindXml)),
                   "missing point kind XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing point kind load");

    const std::string missingHoleTypeXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Points>"
        "<Point id=\"101\" kind=\"0\" diameter=\"1\" active=\"1\">"
        "<Position x=\"0\" y=\"0\" z=\"0\"/>"
        "<Ijk x=\"0\" y=\"0\" z=\"1\"/>"
        "</Point>"
        "</Points>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingHoleTypeXml)),
                   "missing point holeType XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing point holeType load");
}

TEST("domain: missing feature enum XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Features>"
        "<Feature id=\"201\">"
        "<DefiningPoints/>"
        "<Mesh feature=\"0\" meshNodeNum=\"0\" cadPtNum=\"0\" version=\"0\"/>"
        "</Feature>"
        "</Features>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing feature kind XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing feature kind load");
}

TEST("domain: missing tolerance random numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string missingRangeXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\" geomRule=\"0\">"
        "<Rands>"
        "<Rand distribution=\"0\" offset=\"0\" sigmaNum=\"3\"/>"
        "</Rands>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingRangeXml)),
                   "missing tolerance random range XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing tolerance random range load");

    const std::string missingOffsetXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\" geomRule=\"0\">"
        "<Rands>"
        "<Rand distribution=\"0\" range=\"0.1\" sigmaNum=\"3\"/>"
        "</Rands>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingOffsetXml)),
                   "missing tolerance random offset XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing tolerance random offset load");

    const std::string missingSigmaXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\" geomRule=\"0\">"
        "<Rands>"
        "<Rand distribution=\"0\" range=\"0.1\" offset=\"0\"/>"
        "</Rands>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingSigmaXml)),
                   "missing tolerance random sigma XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing tolerance random sigma load");
}

TEST("domain: missing tolerance IR numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR geomRule=\"0\">"
        "<Rands>"
        "<Rand distribution=\"0\" range=\"0.1\" offset=\"0\" sigmaNum=\"3\"/>"
        "</Rands>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing tolerance IR rangeScale XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing tolerance IR rangeScale load");
}

TEST("domain: missing tolerance enum XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string missingGeomRuleXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\">"
        "<Rands>"
        "<Rand distribution=\"0\" range=\"0.1\" offset=\"0\" sigmaNum=\"3\"/>"
        "</Rands>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingGeomRuleXml)),
                   "missing tolerance IR geomRule XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing tolerance IR geomRule load");

    const std::string missingDistributionXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\" geomRule=\"0\">"
        "<Rands>"
        "<Rand range=\"0.1\" offset=\"0\" sigmaNum=\"3\"/>"
        "</Rands>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingDistributionXml)),
                   "missing tolerance random distribution XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing tolerance random distribution load");
}

TEST("domain: missing direction enum XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\" geomRule=\"0\">"
        "<Direction><Ijk x=\"1\" y=\"0\" z=\"0\"/></Direction>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing direction type XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing direction type load");
}

TEST("domain: missing tolerance truncation numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string missingMinXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\" geomRule=\"0\">"
        "<Truncation maxTrunc=\"0.2\" active=\"1\"/>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingMinXml)),
                   "missing tolerance truncation min XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing tolerance truncation min load");

    const std::string missingMaxXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Tolerances>"
        "<Tolerance id=\"301\" name=\"BadTol\" active=\"1\">"
        "<IR rangeScale=\"1\" geomRule=\"0\">"
        "<Truncation minTrunc=\"-0.2\" active=\"1\"/>"
        "</IR>"
        "<Features/>"
        "</Tolerance>"
        "</Tolerances>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingMaxXml)),
                   "missing tolerance truncation max XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing tolerance truncation max load");
}

TEST("domain: missing measure spec numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string missingUslXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures>"
        "<Measure id=\"601\" name=\"BadMeasure\">"
        "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\">"
        "<Spec lsl=\"-0.5\" uslActive=\"1\" lslActive=\"1\" mode=\"0\"/>"
        "</Def>"
        "</Measure>"
        "</Measures>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingUslXml)),
                   "missing measure spec usl XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing measure spec usl load");

    const std::string missingLslXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures>"
        "<Measure id=\"601\" name=\"BadMeasure\">"
        "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\">"
        "<Spec usl=\"0.5\" uslActive=\"1\" lslActive=\"1\" mode=\"0\"/>"
        "</Def>"
        "</Measure>"
        "</Measures>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingLslXml)),
                   "missing measure spec lsl XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing measure spec lsl load");
}

TEST("domain: missing measure def numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures>"
        "<Measure id=\"601\" name=\"BadMeasure\">"
        "<Def type=\"0\" dirMode=\"0\" active=\"1\" asOutput=\"1\">"
        "<Spec usl=\"0.5\" lsl=\"-0.5\" uslActive=\"1\" lslActive=\"1\" mode=\"0\"/>"
        "</Def>"
        "</Measure>"
        "</Measures>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing measure def scale XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing measure def scale load");
}

TEST("domain: missing measure enum XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string missingTypeXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures>"
        "<Measure id=\"601\" name=\"BadMeasure\">"
        "<Def dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\">"
        "<Spec usl=\"0.5\" lsl=\"-0.5\" uslActive=\"1\" lslActive=\"1\" mode=\"0\"/>"
        "</Def>"
        "</Measure>"
        "</Measures>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingTypeXml)),
                   "missing measure def type XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing measure def type load");

    const std::string missingDirModeXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures>"
        "<Measure id=\"601\" name=\"BadMeasure\">"
        "<Def type=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\">"
        "<Spec usl=\"0.5\" lsl=\"-0.5\" uslActive=\"1\" lslActive=\"1\" mode=\"0\"/>"
        "</Def>"
        "</Measure>"
        "</Measures>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingDirModeXml)),
                   "missing measure def dirMode XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing measure def dirMode load");

    const std::string missingSpecModeXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Measures>"
        "<Measure id=\"601\" name=\"BadMeasure\">"
        "<Def type=\"0\" dirMode=\"0\" scale=\"1\" active=\"1\" asOutput=\"1\">"
        "<Spec usl=\"0.5\" lsl=\"-0.5\" uslActive=\"1\" lslActive=\"1\"/>"
        "</Def>"
        "</Measure>"
        "</Measures>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingSpecModeXml)),
                   "missing measure spec mode XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing measure spec mode load");
}

TEST("domain: missing move input numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string missingAccuracyXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Moves>"
        "<Move id=\"501\" name=\"BadMove\" active=\"1\">"
        "<Inputs type=\"0\" maxIterations=\"500\" isNominalBuild=\"0\">"
        "<Pairs/>"
        "</Inputs>"
        "<MoveParts/>"
        "</Move>"
        "</Moves>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingAccuracyXml)),
                   "missing move input searchAccuracy XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing move input searchAccuracy load");

    const std::string missingIterationsXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Moves>"
        "<Move id=\"501\" name=\"BadMove\" active=\"1\">"
        "<Inputs type=\"0\" searchAccuracy=\"1e-5\" isNominalBuild=\"0\">"
        "<Pairs/>"
        "</Inputs>"
        "<MoveParts/>"
        "</Move>"
        "</Moves>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingIterationsXml)),
                   "missing move input maxIterations XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing move input maxIterations load");
}

TEST("domain: missing move input enum XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Moves>"
        "<Move id=\"501\" name=\"BadMove\" active=\"1\">"
        "<Inputs searchAccuracy=\"1e-5\" maxIterations=\"500\" isNominalBuild=\"0\">"
        "<Pairs/>"
        "</Inputs>"
        "<MoveParts/>"
        "</Move>"
        "</Moves>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing move input type XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing move input type load");
}

TEST("domain: missing move float numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string missingSigmaXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Moves>"
        "<Move id=\"501\" name=\"BadMove\" active=\"1\">"
        "<Inputs type=\"0\" searchAccuracy=\"1e-5\" maxIterations=\"500\" isNominalBuild=\"0\">"
        "<Pairs/>"
        "<Float active=\"1\" rangeScale=\"1\" angleRangeDeg=\"360\" angleOffsetDeg=\"0\"/>"
        "</Inputs>"
        "<MoveParts/>"
        "</Move>"
        "</Moves>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingSigmaXml)),
                   "missing move float sigmaNumber XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing move float sigmaNumber load");

    const std::string missingRangeScaleXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Moves>"
        "<Move id=\"501\" name=\"BadMove\" active=\"1\">"
        "<Inputs type=\"0\" searchAccuracy=\"1e-5\" maxIterations=\"500\" isNominalBuild=\"0\">"
        "<Pairs/>"
        "<Float active=\"1\" sigmaNumber=\"3\" angleRangeDeg=\"360\" angleOffsetDeg=\"0\"/>"
        "</Inputs>"
        "<MoveParts/>"
        "</Move>"
        "</Moves>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingRangeScaleXml)),
                   "missing move float rangeScale XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing move float rangeScale load");

    const std::string missingAngleRangeXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Moves>"
        "<Move id=\"501\" name=\"BadMove\" active=\"1\">"
        "<Inputs type=\"0\" searchAccuracy=\"1e-5\" maxIterations=\"500\" isNominalBuild=\"0\">"
        "<Pairs/>"
        "<Float active=\"1\" sigmaNumber=\"3\" rangeScale=\"1\" angleOffsetDeg=\"0\"/>"
        "</Inputs>"
        "<MoveParts/>"
        "</Move>"
        "</Moves>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingAngleRangeXml)),
                   "missing move float angleRangeDeg XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing move float angleRangeDeg load");

    const std::string missingAngleOffsetXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Moves>"
        "<Move id=\"501\" name=\"BadMove\" active=\"1\">"
        "<Inputs type=\"0\" searchAccuracy=\"1e-5\" maxIterations=\"500\" isNominalBuild=\"0\">"
        "<Pairs/>"
        "<Float active=\"1\" sigmaNumber=\"3\" rangeScale=\"1\" angleRangeDeg=\"360\"/>"
        "</Inputs>"
        "<MoveParts/>"
        "</Move>"
        "</Moves>"
        "</OpenDVAModel>";

    loaded = original;
    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(missingAngleOffsetXml)),
                   "missing move float angleOffsetDeg XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing move float angleOffsetDeg load");
}

TEST("domain: missing GD&T numeric XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Gdts>"
        "<Gdt id=\"401\" name=\"BadGdt\" active=\"1\" type=\"1\" diametrical=\"0\">"
        "<Drf primary=\"0\" secondary=\"0\" tertiary=\"0\"/>"
        "<Features/>"
        "</Gdt>"
        "</Gdts>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing GD&T numeric XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing GD&T numeric attribute load");
}

TEST("domain: missing GD&T enum XML attributes fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Gdts>"
        "<Gdt id=\"401\" name=\"BadGdt\" active=\"1\" range=\"0.2\" diametrical=\"0\">"
        "<Drf primary=\"0\" secondary=\"0\" tertiary=\"0\"/>"
        "<Features/>"
        "</Gdt>"
        "</Gdts>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing GD&T type XML attribute is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing GD&T type load");
}

TEST("domain: missing GD&T DRF XML ids fail without mutating model") {
    Model original = makeSampleModel();
    Model loaded = original;
    const std::string badXml =
        "<OpenDVAModel version=\"1\" assemblyName=\"Bad\">"
        "<Parts>"
        "<Part id=\"10\" cadName=\"BadCad\" dcsName=\"BadDcs\">"
        "<Gdts>"
        "<Gdt id=\"401\" name=\"BadGdt\" active=\"1\" type=\"1\" range=\"0.2\" diametrical=\"0\">"
        "<Drf secondary=\"0\" tertiary=\"0\"/>"
        "<Features/>"
        "</Gdt>"
        "</Gdts>"
        "</Part>"
        "</Parts>"
        "</OpenDVAModel>";

    dvatest::check(!loadModelFromString(loaded, withCompleteRootCollections(badXml)),
                   "missing GD&T DRF primary XML id is rejected");
    checkModelStillMatches(loaded, original,
                           "failed missing GD&T DRF primary load");
}

TEST("domain: applyVariant activates only the variant's MTM") {
    Model m = makeSampleModel();

    Model a = m.applyVariant("Scenario_A");
    // Scenario_A: move 501, tol 301, measure 601 active; others inactive.
    dvatest::check(a.moves[0].active, "A: move 501 active");
    dvatest::check(!a.moves[1].active, "A: move 502 inactive");
    dvatest::check(a.parts[0].tolerances[0].active, "A: tol 301 active");
    dvatest::check(!a.parts[1].tolerances[0].active, "A: tol 302 inactive");
    dvatest::check(a.measures[0].def.active, "A: measure 601 active");
    // Variant flags reflect selection.
    dvatest::check(a.variants[0].active && !a.variants[1].active, "A: variant flags");
    // activeMovesInOrder reflects the variant.
    dvatest::check(a.activeMovesInOrder().size() == 1, "A: one active move");

    Model b = m.applyVariant("Scenario_B");
    dvatest::check(!b.moves[0].active, "B: move 501 inactive");
    dvatest::check(b.moves[1].active, "B: move 502 active");
    dvatest::check(!b.parts[0].tolerances[0].active, "B: tol 301 inactive");
    dvatest::check(b.parts[1].tolerances[0].active, "B: tol 302 active");
    dvatest::check(b.variants[1].active && !b.variants[0].active, "B: variant flags");

    // Unknown variant: unchanged copy.
    Model u = m.applyVariant("Nope");
    dvatest::check(u.moves.size() == m.moves.size(), "unknown: same moves");
    dvatest::check(u.moves[0].active == m.moves[0].active, "unknown: unchanged active");
}

TEST("domain: active variant produces the analysis model") {
    Model m = makeSampleModel();
    m.variants[1].active = true;
    m.moves[0].active = true;
    m.moves[1].active = false;
    m.parts[0].tolerances[0].active = true;
    m.parts[1].tolerances[0].active = false;

    const std::optional<std::string> active = m.activeVariantName();
    dvatest::check(active.has_value(), "active variant present");
    dvatest::check(active.value() == "Scenario_B", "active variant name");

    const Model analysis = m.activeVariantApplied();
    dvatest::check(!analysis.moves[0].active, "analysis: move 501 inactive");
    dvatest::check(analysis.moves[1].active, "analysis: move 502 active");
    dvatest::check(!analysis.parts[0].tolerances[0].active,
                   "analysis: tol 301 inactive");
    dvatest::check(analysis.parts[1].tolerances[0].active,
                   "analysis: tol 302 active");
    dvatest::check(analysis.variants[1].active, "analysis: variant flag preserved");

    Model noVariant = makeSampleModel();
    noVariant.moves[0].active = false;
    noVariant.moves[1].active = true;
    dvatest::check(!noVariant.activeVariantName().has_value(),
                   "no active variant name");
    const Model unchanged = noVariant.activeVariantApplied();
    dvatest::check(!unchanged.moves[0].active && unchanged.moves[1].active,
                   "no active variant keeps current active flags");
}

TEST("domain: matchPart three-level priority") {
    Model m = makeSampleModel();

    // Level 1: Part ID hit (even with mismatching names).
    const Part* byId = matchPart(m, 20, "WRONG", "WRONG");
    dvatest::check(byId != nullptr && byId->id == 20, "match by id");

    // Level 2: CAD name hit when id absent.
    const Part* byCad = matchPart(m, kInvalidId, "CAD_BASE", "WRONG");
    dvatest::check(byCad != nullptr && byCad->id == 10, "match by cadName");

    // Level 3: DCS name hit when id + cad absent.
    const Part* byDcs = matchPart(m, kInvalidId, "", "Lid");
    dvatest::check(byDcs != nullptr && byDcs->id == 20, "match by dcsName");

    // No match.
    dvatest::check(matchPart(m, 999, "x", "y") == nullptr, "no match");
}
