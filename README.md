# ui-core-core

A minimal Logos Basecamp app that validates the **UI(QML+C++ backend) → Core → storage + delivery** module architecture per `logos-co/logos-tutorial@tutorial-v2`.

Three modules:

- **`app-ui/`** (`type: "ui_qml"` with C++ backend, scaffolded with `#ui-qml-backend` template) — QML view + C++ backend plugin running in an isolated `ui-host` process. Displays the started/connected status of storage and delivery.
- **`app-core/`** (`type: "core"`) — depends on `storage_module` and `delivery_module`. Owns their lifecycle, exposes the four status booleans via `Q_INVOKABLE`, emits `statusChanged` events when values change.
- Upstream deps: `logos-co/logos-storage-module` and `logos-co/logos-delivery-module` (both unpinned per dev-guide convention).

## Authoritative documentation

The **authoritative source** for module authoring is `logos-co/logos-tutorial` at tag `tutorial-v2`:

- [Developer guide](https://github.com/logos-co/logos-tutorial/blob/tutorial-v2/logos-developer-guide.md) — module structure (Part 1), metadata fields (§1.3), module dependencies (§9.2), inter-module communication (§8)
- [Part 1 — Wrapping a C library](https://github.com/logos-co/logos-tutorial/blob/tutorial-v2/tutorial-wrapping-c-library.md) — core modules
- [Part 3 — C++ UI module](https://github.com/logos-co/logos-tutorial/blob/tutorial-v2/tutorial-cpp-ui-app.md) — **this project's UI pattern**: QML view + C++ backend, scaffolded from the `#ui-qml-backend` template, running in an isolated `ui-host` process. **Not** the QML-only pattern from Part 2.

The **authoritative source** for the storage and delivery module APIs is `logos-co/logos-docs` — currently in open PRs (not yet merged to main):

- [PR #166 — feat: initial documentation for Logos Storage](https://github.com/logos-co/logos-docs/pull/166) — the storage module's `Q_INVOKABLE` surface and lifecycle (`init` → `start` → events).
- [PR #226 — feat: use logos-delivery-module journey](https://github.com/logos-co/logos-docs/pull/226) — the delivery module's API surface, `createNode` config schema, and event payloads (including `connectionStateChanged`, which this project subscribes to).
- [PR #284 — storage docs follow-up](https://github.com/logos-co/logos-docs/pull/284) — supersedes parts of #166; check before relying on #166 alone.

Because these are open PRs, the documented APIs may shift before merge. Re-fetch them before extending the storage or delivery integration.

`logos-co/scaffold` (the `lgs` CLI) is **not** authoritative for module authoring; see `docs/findings/lgs-vs-tutorial-v2.md` for an instance where its rules diverge.

## Prerequisites

- **Nix** with flakes enabled. Either set globally:
  ```ini
  # ~/.config/nix/nix.conf
  experimental-features = nix-command flakes
  ```
  or pass `--extra-experimental-features 'nix-command flakes'` to each `nix` command.
- **Git** — Nix builds only see git-tracked files; uncommitted/untracked files are invisible to the build.
- **Linux or macOS** with a graphical session (for running the UI).
- **Internet** on first build — Nix pulls `logos-module-builder`, `logos-cpp-sdk`, `storage_module`, `delivery_module`, Qt 6, etc.

## Build

The UI module's flake transitively pulls everything else, so you only ever need to build from `app-ui/`:

```bash
git clone https://github.com/fryorcraken/ui-core-core.git
cd ui-core-core/app-ui
git add -A                        # Nix needs files tracked; only matters after edits
nix build                         # → result/, contains app_ui_plugin.so, app_ui_replica_factory.so, qml/Main.qml, metadata.json
```

This single `nix build` produces:

- `app_ui` (the QML+C++ backend module)
- `app_core` (the core module) — built transitively as a dep
- `capability_module`, `storage_module`, `delivery_module` LGX packages — bundled by `mkLogosQmlModule`
- A `run-logos-standalone-ui` wrapper that knows where all the deps live

To build just the core module on its own:

```bash
cd ../app-core
nix build                         # → result/lib/app_core_plugin.so, result/include/app_core_api.{h,cpp}
```

## Run

From `app-ui/`:

```bash
nix run .                         # opens logos-standalone-app with the QML view
```

What happens on launch:

1. `logos-standalone-app` starts, sets up Qt + the QML engine.
2. Reads `metadata.json#dependencies` from `app_ui` → loads `app_core`, which in turn pulls `storage_module` + `delivery_module` + `capability_module` (each in its own `logos_host_qt` subprocess).
3. Spawns a `ui-host` subprocess to load `app_ui_plugin.so` (the C++ backend).
4. The standalone app's `MainWindow` creates a `QQuickWidget`, points it at `qml/Main.qml`, injects the `logos` bridge.
5. QML calls `logos.module("app_ui")` to get the typed replica; the four `Q_PROPERTY` bools auto-sync over Qt Remote Objects.
6. `app_core::initLogos` starts storage (sync) and delivery (async, with a 60-second timeout); on each state change it emits `statusChanged`; `app_ui`'s backend subscribes to that event and refreshes the PROPs.

Expected first paint: `Storage: started=? connected=?  Delivery: started=? connected=?` then transitions to `yes`/`no` per actual state.

### Live-editing QML

Set `DEV_QML_PATH` to your source `qml/` directory; the standalone app loads QML from there instead of the nix-store copy:

```bash
DEV_QML_PATH=$PWD/src/qml nix run .
```

You still need a rebuild for C++ / `.rep` / metadata changes — only `.qml` files are picked up from `DEV_QML_PATH` without recompilation.

### Inspecting the built plugin

```bash
nix build 'github:logos-co/logos-module/tutorial-v2#lm' --out-link ./lm
./lm/bin/lm methods ./result/lib/app_core_plugin.so        # for the core module
./lm/bin/lm methods app-ui/result/lib/app_ui_plugin.so     # for the UI backend
```

### Cleaning up

`nix run` runs in the foreground; Ctrl-C stops it. If a previous instance left orphan `logos_host_qt` / `ui-host` processes (Qt Remote Objects sockets in `/tmp/logos_*`), kill them:

```bash
pkill -9 -f 'logos-standalone-app|logos_host_qt|ui-host|run-logos'
rm -f /tmp/logos_*
```

## Project layout

```
ui-core-core/
├── README.md                 # this file
├── CLAUDE.md                 # context for AI assistants working on this repo
├── scaffold.toml             # written by `lgs init`; not strictly needed for build/run
├── app-core/                 # type: "core" — wraps storage + delivery, exposes status
│   ├── flake.nix
│   ├── metadata.json
│   ├── CMakeLists.txt
│   └── src/{app_core_interface.h, app_core_plugin.{h,cpp}}
├── app-ui/                   # type: "ui_qml" with C++ backend (Part 3 pattern)
│   ├── flake.nix
│   ├── metadata.json         # main: "app_ui_plugin", view: "qml/Main.qml"
│   ├── CMakeLists.txt
│   └── src/
│       ├── app_ui.rep        # QtRO interface; PROP(bool storageStarted), etc.
│       ├── app_ui_interface.h
│       ├── app_ui_plugin.{h,cpp}
│       └── qml/Main.qml
└── docs/
    ├── findings/lgs-vs-tutorial-v2.md
    └── ideas/logos-status-app.md
```

## Deviations from the tutorial-v2 docs

Everything below diverges from the documented patterns. Some are workarounds for upstream issues; some are intentional simplifications. Listed for honesty, and so future maintainers (or upstream PR reviewers) can decide whether to fold the fixes back into the docs or back out our workarounds.

### 1. Synchronous `init()` + `start()` for `storage_module` (`app_core/src/app_core_plugin.cpp`)

**Doc says** (developer guide §8.2):

> Prefer async wrappers. Use `doSomethingAsync(...)` instead of `doSomething(...)` to avoid blocking the caller's thread. Synchronous calls can cause hangs if the target module is slow to respond.

**We do**: call `storage_module.init(cfg)` and `storage_module.start()` synchronously inside `app_core::initLogos`.

**Why**: storage's calls return in ~20ms in practice, well below the default 20s IPC timeout. The simpler sync code is fine here.

**Risk**: if storage ever gets slower (a real connect-to-network step would push it past 20s), this will silently fail with `start() → false`. Migrate to async if that happens.

### 2. Asynchronous `createNode()` + `start()` for `delivery_module` with extended `Timeout(60000)`

**Doc shows**: sync `delivery_module.createNode(cfg)` and `delivery_module.start()` returning `LogosResult`.

**We do**: async variants with an explicit 60-second IPC timeout instead of the default 20s.

**Why**: `delivery_module.start()` does a full nwaku/libp2p node startup which takes ~20s. The default 20s IPC timeout in the SDK fires before delivery returns, and we get a misleading `success=false` from a node that actually started fine. Custom `Timeout(60000)` gives nwaku enough headroom.

**Upstream**: the dev guide doesn't mention this timing characteristic of delivery, and doesn't document when to override the default timeout. Worth a PR to either lengthen the SDK default or document the override pattern.

### 3. Delivery "started"/"connected" sourced from `connectionStateChanged` event, not `start()` return value

**Doc shows**: `delivery_module.start()` returns `LogosResult{success: bool}`; the bool indicates whether the node started.

**We do**: subscribe to `delivery_module`'s `connectionStateChanged` event and treat `status == "Connected"` as truthy for both `deliveryStarted` and `deliveryConnected`. The `start()` callback fires but its `success` value is ignored.

**Why**: see (2) — `start()` returns false on timeout even when the node is up. The `connectionStateChanged` event fires when delivery actually peers up to the network (`logos.dev` cluster) and is the authoritative liveness signal. This event is documented in `logos-docs` PR #226 (delivery API journey).

**Risk**: `connectionStateChanged` is in an open docs PR, not in merged docs. The event name/payload could change.

### 4. Storage "connected" indicator reuses the `start()` bool

**Doc says**: nothing — storage's "connected" semantics aren't defined in any merged doc.

**We do**: `app_core::storageConnected()` returns the same value as `storageStarted()`.

**Why**: spec called for a separate "connected" indicator per module. Delivery has `connectionStateChanged`; storage does not (per `logos-co/logos-storage-module@v0.3.2`'s `storage_module_plugin.h`, only `storageStart`/`storageStop`/upload/download events exist — no peer-connected event). Treating `start() bool` as "connected" is the simplest defensible choice for an MVP. The doc/upstream has no preferred answer here.

**Upstream**: worth filing a question/PR on whether storage should expose a peer-count or peer-connected event.

### 5. Peer-count indicator removed from spec

**Original spec**: three indicators per module — started, connected, peers.

**We do**: dropped peers.

**Why**: neither `storage_module` (v0.3.2) nor `delivery_module` (v0.1.2) exposes a peer-count `Q_INVOKABLE` or event-payload field. `delivery_module` has `getNodeInfo()` which *might* contain a peer count under one of its identifiers — undocumented. Rather than probe, we cut the indicator.

**Risk**: trivial. The architecture supports adding it later if `getNodeInfo` or a future upstream API exposes it.

### 6. Storage `init("{}")` — empty config, not the canonical one

**Doc/canonical config** at `logos-co/node-configs/storage_config.json` includes `listen-addrs`, `data-dir`, `disc-port`, etc.

**We do**: pass `"{}"` and let storage use defaults.

**Why**: the canonical config triggers `Unexpected field 'listen-addrs' while deserializing StorageConf*` at the underlying Nim deserializer. The schema in the shipped binary doesn't match the documented JSON. Empty config bypasses the mismatch.

**Upstream**: real bug — the canonical config file and the deserializer schema are out of sync. The shipped binary on `main` rejects the field that the publish-side docs say is required. File on `logos-co/logos-storage-module` or `logos-co/node-configs`.

### 7. Defer `initLogos` setup to `QTimer::singleShot(0, ...)`

**Doc shows** (`tutorial-cpp-ui-app.md` Step 5): `initLogos()` is synchronous — store the `LogosAPI*`, construct `LogosModules`, call `setBackend(this)`, done.

**We do**: in **early iterations** of `app_ui_plugin`, the initial `refresh()` call ran inside `initLogos` and made a synchronous remote call to `app_core`, which blocked the ui-host's ready handshake. The standalone-app then timed out waiting for ui-host ready (10s) and showed "Failed to load UI plugin". Wrapping the first refresh in `QTimer::singleShot(0, this, [this]{ refresh(); })` let `initLogos` return immediately and the handshake completed.

**Status**: the current `app_ui_plugin.cpp` no longer needs `singleShot` because we now use `app_core.on("statusChanged", ...)` + async getters — neither blocks. **This deviation has been removed.** Documenting it because it tripped us up and others may hit it: **do not make blocking sync calls inside `initLogos`**, even if the docs suggest sync is OK.

### 8. `app_ui` C++ backend uses `app_core::statusChanged` event to refresh PROPs

**Doc shows**: the Part 3 calc tutorial has the backend update PROPs internally (e.g., `setStatus("Ready")` in `initLogos`). It doesn't show a multi-module pattern where one Core's state needs to flow into another module's PROPs.

**We do**: `app_ui` C++ backend subscribes to `app_core.on("statusChanged", ...)` and, on each event, calls `app_core.storageStartedAsync(cb)` etc., then `setStorageStarted(v)` in the callback. The setters auto-sync the PROPs over QtRO to the QML replica.

**Why**: it's the natural pattern given the Core+UI separation, but the docs don't show this specific shape. Earlier I had a 1Hz `QTimer` polling instead, which was redundant — PROPs already auto-sync, and we have an event to drive the refresh. The polling was a wrong-pattern artifact of not thinking through the event flow.

### 9. Storage's `start()` returns `false` even when the threadpool/discovery starts — and we accept it

**Observed**: in some runs (timing-dependent), `m_logos->storage_module.start()` returns `false` even though storage's logs show the threadpool starts, discovery initializes, UPnP succeeds. We treat that as "not started" in the UI.

**Why we accept it**: same root cause as (2) — the IPC return value isn't always meaningful for nodes that do real network work. Documenting but not fixing because storage doesn't expose a clean "ready" event we could subscribe to (see (4)).

**Upstream**: combine with (2)+(4) into one PR — IPC timeout / liveness-signal documentation.

## Project finding: `lgs` docs vs `tutorial-v2`

See `docs/findings/lgs-vs-tutorial-v2.md`. The `lgs` CLI's `docs/basecamp-module-requirements.md` requires a `follows` declaration on transitive `logos-module-builder` inputs. Following it caused our build to fail; **dropping the `follows` and using the tutorial-v2 unpinned-dep pattern works**. The `lgs` rule is probably scoped to `lgs basecamp install`'s flow but presented as a general module-authoring requirement.

## Project finding (one-pager)

The design rationale and Phase-3 one-pager from the planning session is at `docs/ideas/logos-status-app.md`.
