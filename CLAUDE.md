# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project goal

A minimal **Logos Basecamp app** with three modules, validating the `logos-co/logos-tutorial@tutorial-v2` architecture end-to-end on one developer's machine:

- **`app-ui`** — `type: "ui_qml"` with a **C++ backend** (`tutorial-v2` Part 3, template `#ui-qml-backend`). QML view + `.rep`-defined typed interface + C++ plugin running in a separate `ui-host` process. Shows the status of the storage and delivery nodes: **Started**, **Connected**, **Peers** — three indicators per module.
- **`app-core`** — `type: "core"`. C++ plugin. Owns the lifecycle of `storage_module` and `delivery_module`; exposes status getters as `Q_INVOKABLE` methods (no QML, no view).
- Dependencies: `app-core` depends on `storage_module` and `delivery_module`; `app-ui` depends only on `app-core`.

**Data flow:**

```
Main.qml (in host process)
  ──── logos.module("app_ui").storagePeerCount() ────►  app-ui C++ backend (ui-host process)
                                                                      │
                                                                      │  m_logos->app_core.storagePeerCount()
                                                                      ▼
                                                              app-core plugin (loaded in host)
                                                                      │
                                                                      │  m_logos->storage_module.peerId()  /  m_logos->delivery_module....
                                                                      ▼
                                                              storage_module / delivery_module
```

The QML view calls into its **own** C++ backend through a typed replica (via the `logos.module(...)` bridge and `.rep`-generated stubs). That backend calls `app-core` via the typed `LogosModules` SDK in C++. The Core in turn calls storage and delivery the same way. Three named modules; one Qt Remote Objects hop (QML ↔ ui-backend); the rest is in-process typed C++ calls.

## Scaffolding with `logos-scaffold`

The workspace is scaffolded and orchestrated by **`logos-co/logos-scaffold`** (redirects to `logos-co/scaffold`; binaries are `logos-scaffold` and the short alias `lgs`). The QML/core templates themselves come from **`logos-co/logos-module-builder@tutorial-v2`**.

### Prerequisites

- Nix with flakes enabled (`experimental-features = nix-command flakes`)
- `cargo` to install `lgs`
- Unix (the CLI is Unix-only; relies on `lsof`, `ps`, `kill`)
- `git add -A` before every `nix build` — Nix sandboxes to git-tracked files

### Bootstrap sequence

```bash
# 1. Install the orchestrator once
cargo install --git https://github.com/logos-co/scaffold     # → `logos-scaffold` + `lgs`

# 2. Make this directory a scaffold project (writes scaffold.toml + .scaffold/)
cd /home/fryorcraken/src/fryorcraken/ui-core-core
git init
lgs init

# 3. Scaffold the two modules from logos-module-builder@tutorial-v2 templates
mkdir app-core && cd app-core
nix flake init -t github:logos-co/logos-module-builder/tutorial-v2          # default = core
git add -A && cd ..

mkdir app-ui && cd app-ui
nix flake init -t github:logos-co/logos-module-builder/tutorial-v2#ui-qml-backend   # QML view + C++ backend
git add -A && cd ..

# 4. Wire basecamp + lgpm and seed alice/bob profiles
lgs basecamp setup

# 5. Capture the module set into scaffold.toml.
lgs basecamp modules --flake "./app-core#lgx" --flake "./app-ui#lgx"
lgs basecamp modules --show

# 6. Build all captured modules and install into alice + bob via lgpm
lgs basecamp install

# 7. Launch (two terminals to exercise p2p)
lgs basecamp launch alice    # terminal 1
lgs basecamp launch bob      # terminal 2
```

### What `lgs` writes where (project-local, never under $HOME)

- `scaffold.toml` — project root. `[repos.*]` pins and `[modules.<name>]` entries.
- `.scaffold/basecamp/profiles/{alice,bob}/` — per-profile XDG dirs. **`launch` scrubs these every invocation**; don't store anything there.
- `.scaffold/logs/` — install/setup logs.

### Important behaviors

- **`basecamp modules` is the sole automated writer of `[modules]`.** Hand-edits to `scaffold.toml` survive re-runs; existing keys are never overwritten.
- **Module-name resolution for `github:` refs is a guess.** When scaffold sees `github:logos-co/logos-storage-module/...#lgx`, it derives `module_name = storage_module` (strip `logos-` prefix, `-` → `_`) and prints an assumption note. Edit `scaffold.toml` if wrong.
- **Empty `[modules]` causes `launch` to bail** before scrubbing.

## Module layout (after `nix flake init`)

### Core module (`app-core/`, `type: "core"`)

```
app-core/
├── flake.nix          # mkLogosModule
├── metadata.json      # name, type: "core", main: "app_core_plugin", dependencies
├── CMakeLists.txt     # logos_module(NAME app_core SOURCES src/...)
└── src/
    ├── app_core_interface.h
    ├── app_core_plugin.h
    └── app_core_plugin.cpp
```

### UI module (`app-ui/`, `type: "ui_qml"` + C++ backend, Part 3 pattern)

```
app-ui/
├── flake.nix          # mkLogosQmlModule — compiles C++ backend AND bundles QML view
├── metadata.json      # type: "ui_qml", main: "app_ui_plugin", view: "qml/Main.qml"
├── icons/             # at least one PNG referenced by metadata.json#icon
├── CMakeLists.txt     # logos_module(NAME app_ui REP_FILE src/app_ui.rep SOURCES ...)
├── src/
│   ├── app_ui.rep                # source of truth for the QML↔C++ interface
│   ├── app_ui_interface.h
│   ├── app_ui_plugin.h
│   ├── app_ui_plugin.cpp
│   └── qml/
│       └── Main.qml
```

Note: `metadata.json` for the UI module sets **both** `main` (C++ plugin) **and** `view` (QML entry). That's how the builder knows this is the C++-backend variant.

### Wiring invariants

- **Three names must agree** for any dependency: dep's `metadata.json#name` ↔ the consumer's flake-input attr ↔ the consumer's `metadata.json#dependencies[]` entry.
- **Cross-flake URL form**: for remote-pinned deps, use `github:owner/repo/<ref>?dir=<subdir>`. For local dev, use `path:../sibling` on a single line (`lgs` parses `path:../<sibling>` line-by-line; multi-line attrset forms are not detected).
- **The `.rep` file is the source of truth for the UI module's QML↔C++ interface.** `repc` generates `AppUiSimpleSource` (backend base class), `AppUiReplica` (QML-side typed proxy), and `AppUiViewPluginBase` (remoting glue). The plugin class inherits all three.
- **Inter-module calls in C++ go through the typed SDK**: `m_logos->app_core.storagePeerCount()`. The header `logos_sdk.h` is generated by the builder from the dependency's compiled plugin metadata.

### `app-ui/src/app_ui.rep`

```rep
class AppUi
{
    PROP(int storagePeerCount READWRITE)
    PROP(int deliveryPeerCount READWRITE)
    PROP(bool storageStarted READWRITE)
    PROP(bool storageConnected READWRITE)
    PROP(bool deliveryStarted READWRITE)
    PROP(bool deliveryConnected READWRITE)
}
```

All six are `PROP` (no `SLOT`s) because the UI never needs to ask the backend to *do* anything — it only reads status. PROPs auto-sync from backend to QML replica over Qt Remote Objects; the QML side reads `backend.storagePeerCount` and gets live updates with no boilerplate.

### `app-ui/src/qml/Main.qml`

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    readonly property var backend: logos.module("app_ui")

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 8
        Text { text: "Storage:  started="  + (backend ? backend.storageStarted  : "?")
                   + "  connected=" + (backend ? backend.storageConnected : "?")
                   + "  peers="     + (backend ? backend.storagePeerCount : "?") }
        Text { text: "Delivery: started=" + (backend ? backend.deliveryStarted  : "?")
                   + "  connected=" + (backend ? backend.deliveryConnected : "?")
                   + "  peers="     + (backend ? backend.deliveryPeerCount : "?") }
    }
}
```

`backend.<prop>` updates automatically — no `Connections` block needed for PROP values. If a `SLOT` is added later (e.g., a refresh button), call sites use `logos.watch(backend.someSlot(args), okCb, errCb)`.

### `app-ui/src/app_ui_plugin.{h,cpp}`

The plugin inherits `AppUiSimpleSource`, `AppUiInterface`, and `AppUiViewPluginBase`. On `initLogos(LogosAPI*)` it constructs `m_logos = new LogosModules(api)`, calls `setBackend(this)` to register with the remoting host, and starts a 1Hz `QTimer` that polls Core for status and calls the generated `setStorageStarted(...)` / `setStoragePeerCount(...)` / etc. setters — those setters trigger the auto-sync to QML. There are no `Q_INVOKABLE` methods on the UI plugin itself; the QML calls `logos.module("app_ui")` and reads PROPs.

### `app-core/src/app_core_plugin.{h,cpp}`

Plain `Q_INVOKABLE` methods (no `.rep`, because Core is consumed from C++, not QML):

```cpp
Q_INVOKABLE bool storageStarted()   const { return m_storageStarted; }
Q_INVOKABLE bool storageConnected() const { return m_storageStarted; }   // same bool, by project decision
Q_INVOKABLE int  storagePeerCount() const;
Q_INVOKABLE bool deliveryStarted()  const { return m_deliveryStarted; }
Q_INVOKABLE bool deliveryConnected()const { return m_deliveryStarted; }  // same bool
Q_INVOKABLE int  deliveryPeerCount() const;

Q_INVOKABLE void initLogos(LogosAPI* api);
```

On `initLogos`: build `LogosModules*`, call `storage_module.init(cfg)` → `start()` (store the bool), call `delivery_module.createNode(cfg)` → `start()` (read `LogosResult.success`). The peer-count getters poll the underlying module APIs on demand. No internal QTimer — the UI's backend is the one polling at 1Hz.

### `app-core/flake.nix` inputs

```nix
inputs = {
  logos-module-builder.url = "github:logos-co/logos-module-builder/tutorial-v2";

  storage_module.url       = "github:logos-co/logos-storage-module/<pinned-ref>";
  delivery_module.url      = "github:logos-co/logos-delivery-module/<pinned-ref>";

  # Force transitive logos-module-builder onto our pin. Without this, the
  # storage / delivery modules pull in their own master-branch module-builder
  # and the extra flake.lock entry silently wins when scaffold injects
  # --override-input at install time. Symptom: `no 'main' field in metadata.json`
  # from a build that works fine with a direct `nix build .#lgx`.
  storage_module.inputs.logos-module-builder.follows  = "logos-module-builder";
  delivery_module.inputs.logos-module-builder.follows = "logos-module-builder";
};
```

Verify the actual tagged refs at implementation time — `v0.3.2` (storage) and `v0.1.1` (delivery) are values from open docs PRs and may not be `tutorial-v2`-aligned.

### `app-ui/flake.nix` inputs

```nix
inputs = {
  logos-module-builder.url = "github:logos-co/logos-module-builder/tutorial-v2";

  # Option A: remote (captured form for CI / scaffold)
  app_core.url        = "github:<your-owner>/<your-repo>/<ref>?dir=app-core";

  # Option B: local dev — comment out Option A and uncomment this
  # app_core.url      = "path:../app-core";
};
```

`metadata.json#dependencies: ["app_core"]`. Transitive `storage_module` / `delivery_module` resolve through Core. Override locally without editing `flake.nix` via `nix run . --override-input app_core path:../app-core`.

## Status surface

Six values, four sources:

| Indicator | UI PROP | UI backend gets it from | Origin |
|---|---|---|---|
| Storage Started | `storageStarted` | `m_logos->app_core.storageStarted()` | `storage_module.start()` returned `true` |
| Storage Connected | `storageConnected` | `m_logos->app_core.storageConnected()` | **same bool as Started** (project decision) |
| Storage Peers | `storagePeerCount` | `m_logos->app_core.storagePeerCount()` | TBD — peer-count method on storage_module; if absent, return `-1` |
| Delivery Started | `deliveryStarted` | `m_logos->app_core.deliveryStarted()` | `LogosResult.success` from `delivery_module.start()` |
| Delivery Connected | `deliveryConnected` | `m_logos->app_core.deliveryConnected()` | **same bool as Started** (project decision) |
| Delivery Peers | `deliveryPeerCount` | `m_logos->app_core.deliveryPeerCount()` | TBD — peer-count method on delivery_module; if absent, return `-1` |

The "Connected" indicator reusing the `start()` bool is intentional — the open Storage/Delivery docs don't define a clean "connected" event for both, and inventing one for a one-sitting smoke-test is yak-shaving. **The peer-count indicator is the actually-moving signal** that proves p2p works when alice and bob run side by side.

## Common commands

```bash
# scaffold-driven workflow (preferred)
lgs basecamp setup                                # one-time
lgs basecamp modules                              # auto-discover, or --flake/--path
lgs basecamp modules --show                       # print captured set, no mutation
lgs basecamp install                              # build all + lgpm install to alice/bob
lgs basecamp install --print-output               # stream nix output for CI/debug
lgs basecamp doctor                               # health: pins, profiles, drift
lgs basecamp launch alice                         # scrub-and-launch
lgs basecamp build-portable                       # .lgx-portable for AppImage hand-loading
lgs basecamp docs                                 # print the module contract

# per-module raw nix (run inside the module dir)
nix build                                         # full build (lib + generated SDK headers)
nix build .#lgx                                   # dev .lgx package
nix build .#lgx-portable                          # portable .lgx package

# Standalone UI run (UI module only) — no basecamp needed
nix run .                                         # opens the QML window with bundled deps
DEV_QML_PATH=$PWD/src/qml nix run .               # live-reload QML from source tree
                                                  # (basename of metadata.json#view must exist
                                                  # under that directory)

# Inspect a built plugin
nix build 'github:logos-co/logos-module/tutorial-v2#lm' --out-link ./lm
./lm/bin/lm ./result/lib/app_core_plugin.so
./lm/bin/lm methods ./result/lib/app_core_plugin.so --json

# Stale QML cache after rebuild → disable disk cache
QML_DISABLE_DISK_CACHE=1 ./basecamp-result/bin/logos-basecamp
```

## Things to verify before relying on them

- **Storage and Delivery API surfaces are still in open docs PRs.** Re-fetch `logos-co/logos-docs` PRs #166, #226, #284 when starting work. In particular, **the existence and naming of a peer-count method on each module is unverified** — read the plugin source at the pinned ref. If absent, return `-1` from the Core getter and the UI will render `-1` as `?`.
- **Tag pins for storage and delivery may have moved past `v0.3.2` / `v0.1.1`.** Pick whatever a known-good `tutorial-v2` consumer's `flake.lock` already builds against.
- **Two-instance peer-up under `logos.dev` preset is unverified** on localhost. If alice and bob never see each other (peers stuck at `0`), accept it as a documented finding rather than chasing a fix.
