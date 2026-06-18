# 3DCS Forge Digital Twin Product Design Prototypes

This folder contains high-fidelity PNG prototypes for the selected Product Design visual direction: Forge Digital Twin.

Design target: Qt6 QWidget / Qt3D-ready C++17 desktop software, industrial high-tech visual language, polished dark forged-metal workspace, ribbon/menu tooling, 3D digital-twin viewport, dockable inspectors, telemetry HUD, simulation/optimization/report modules.

Generated pages:
- 00_main_workbench.png - Main Workbench
- 01_modeling.png - Modeling And Features
- 02_moves.png - Moves And Assembly Constraints
- 03_tolerances.png - Tolerances And GD&T
- 04_measures.png - Measurement And Inspection
- 05_simulation.png - Simulation Compute
- 06_visualization.png - Visualization Analysis
- 07_aao.png - AAO Optimization
- 08_mechanical.png - Mechanical Motion
- 09_fea_compliant.png - FEA Compliant Assembly
- 10_reports.png - Reports And Publishing
- 11_cad_integration.png - CAD Integration
- 12_user_dll.png - User DLL Extensions
- 13_help_tutorials.png - Help And Tutorials
- 14_system.png - System Settings
- 99_contact_sheet.png - overview contact sheet

Implementation notes for later Qt QWidget work:
- Use QMainWindow with QMenuBar, QToolBar/QTabBar-style ribbon, QDockWidget side panels, QTreeView/QTableView inspectors, and a central Qt3D/QOpenGLWidget viewport.
- Keep the visual system consistent: forged dark panels, fine separator lines, restrained glow accents per module, dense engineering data, and large clear 3D model canvas.
