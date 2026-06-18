#include "dva_test.h"

#include "FeatureCatalogModel.h"

TEST("feature catalog imports complete 3DCS help tree") {
    opendva::ui::FeatureCatalogModel catalog;

    dvatest::check(catalog.rowCount() >= 946,
                   "catalog includes every parsed help TOC node");
    dvatest::check(catalog.findById("1") != nullptr,
                   "catalog includes Welcome root node");
    dvatest::check(catalog.findById("4.1") != nullptr,
                   "catalog includes Model Navigator node");
    dvatest::check(catalog.findById("4.2.1.1") != nullptr,
                   "catalog includes Tree Link Wizard node");
    dvatest::check(catalog.findById("18.3.38") != nullptr,
                   "catalog includes final Visualization Export node");
}

TEST("feature catalog maps help nodes to gui surfaces") {
    opendva::ui::FeatureCatalogModel catalog;

    const auto* modelNavigator = catalog.findById("4.1");
    dvatest::check(modelNavigator != nullptr, "Model Navigator node exists");
    dvatest::check(modelNavigator != nullptr &&
                       modelNavigator->uiSurface == "Navigator Dock",
                   "Model Navigator maps to navigator dock");

    const auto* treeLink = catalog.findById("4.2.1.1");
    dvatest::check(treeLink != nullptr, "Tree Link Wizard node exists");
    dvatest::check(treeLink != nullptr &&
                       treeLink->uiSurface == "Wizard",
                   "Tree Link Wizard maps to wizard surface");

    const auto* animation = catalog.findByTitle("Animation Accelerator");
    dvatest::check(animation != nullptr, "Animation Accelerator node exists");
    dvatest::check(animation != nullptr &&
                       animation->uiSurface == "Analysis Window",
                   "Animation Accelerator maps to analysis window");
}

TEST("feature catalog maps official help nodes to workbench commands") {
    opendva::ui::FeatureCatalogModel catalog;

    const auto* treeLink = catalog.findByTitle("Tree Link Wizard");
    dvatest::check(treeLink != nullptr, "Tree Link Wizard node exists");
    dvatest::check(treeLink != nullptr &&
                       treeLink->workspaceId == QStringLiteral("modeling"),
                   "Tree Link Wizard routes to Modeling workspace");
    dvatest::check(treeLink != nullptr &&
                       treeLink->command == QStringLiteral("Tree Link"),
                   "Tree Link Wizard routes to Tree Link command");

    const auto* stiffGen = catalog.findByTitle("StiffGen");
    dvatest::check(stiffGen != nullptr, "StiffGen node exists");
    dvatest::check(stiffGen != nullptr &&
                       stiffGen->workspaceId == QStringLiteral("fea"),
                   "StiffGen routes to FEA workspace");
    dvatest::check(stiffGen != nullptr &&
                       stiffGen->command == QStringLiteral("StiffGen"),
                   "StiffGen routes to StiffGen command");

    const auto* toleranceOptimizer = catalog.findByTitle("Tolerance Optimizer");
    dvatest::check(toleranceOptimizer != nullptr,
                   "Tolerance Optimizer node exists");
    dvatest::check(toleranceOptimizer != nullptr &&
                       toleranceOptimizer->workspaceId == QStringLiteral("aao"),
                   "Tolerance Optimizer routes to AAO workspace");
    dvatest::check(toleranceOptimizer != nullptr &&
                       toleranceOptimizer->command ==
                           QStringLiteral("Tolerance Optimizer"),
                   "Tolerance Optimizer routes to optimizer command");

    const QVariantMap node = catalog.get(catalog.firstIndexForGroup("Moves"));
    dvatest::check(node.value("workspaceId").toString() == QStringLiteral("moves"),
                   "catalog get exposes the workbench route role");
    dvatest::check(!node.value("command").toString().isEmpty(),
                   "catalog get exposes the command route role");
}
