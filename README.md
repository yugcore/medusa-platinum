# Medusa Platinum

Standalone, real-time 3D DCC, scene composition, and cinematic animation application powered by ZeGFX.

---

## Directory Structure

```text
MedusaPlatinum/
├── docs/                 # Architecture, engineering status, comparisons, and integration reports
├── include/medusa/       # Modular C++ public and subsystem headers
│   ├── app/              # Application lifecycle (medusa_app.hpp)
│   ├── core/             # Base types, screen metrics, theme tokens (medusa_types, medusa_screen, medusa_theme)
│   ├── platform/         # Win32 windowing, event handling (medusa_window.hpp)
│   ├── graphics/         # Direct2D UI, Direct3D 12 & ZeGFX pipeline interop (medusa_graphics.hpp)
│   ├── project/          # Project format v4, scene graph, undo/redo, recovery (medusa_project, medusa_production_scene)
│   ├── ui/               # Widget library, SVG vector icons (medusa_widgets, medusa_icons)
│   └── editor/           # Editor orchestration, viewports, and workspace panels (medusa_editor.hpp)
├── src/                  # C++ implementation files mirroring include/
├── resources/            # Authored SVG icons, branding logos, and assets
├── scripts/              # Tooling and generator scripts (e.g. generate_icons.py)
├── tests/                # Unit, integration, and visual regression tests
├── artifacts/            # [Ignored] Verification bitmaps and rendering captures
├── logs/                 # [Ignored] Build logs and runtime diagnostic outputs
├── bin/                  # [Ignored] Compiled binaries
└── build/                # [Ignored] Object files and intermediate build output
```

## Building

From the `MedusaPlatinum` directory:

```powershell
make production
```

Or run via:

```powershell
make run
```
