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

Expected first paint: `Storage: started=? connected=?  Delivery: started=? connected=?` then transitions to `Storage: yes/yes  Delivery: no/no`. Delivery stays at `no/no` because of the upstream bugs documented under "What we observe at runtime" below — the node is actually peering up, but the SDK never tells the UI.

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

## What we observe at runtime

After running the app end-to-end (see trace log + delivery DEBUG logs):

- **Storage:** `started=yes`, `connected=yes`. `storage_module.init("{}")` and `storage_module.start()` both return `true` in ~20ms. Cross-module IPC `app_core → app_ui` works: `app_ui` calls `storageStartedAsync` and gets the real value back, which paints to the QML UI.
- **Delivery:** `started=no`, `connected=no` — **even though the delivery node is fully operational** (host stdout shows it sending and receiving relay messages with multiple peers within seconds of launch). Two upstream bugs explain why the UI doesn't reflect that:
  1. `delivery_module.start()` returns `success=false` after exactly 60s on a working node (matches our `Timeout(60000)`). Returns false in 20s if we leave the SDK default timeout.
  2. `connectionStateChanged` — documented in `logos-docs` PR #226 §3.1 as the authoritative liveness signal — **is never emitted** by `logos-delivery-module@v0.1.x`. We subscribed before `createNode`, waited 90+ seconds with the node actively relaying traffic, got no events.

The cross-module event lane works (proven by storage). The signals the delivery module is supposed to send don't fire. That's the bug, not anything in this app.

## Deviations from the tutorial-v2 docs

Everything below diverges from the documented patterns. Some are workarounds for upstream issues; some are intentional simplifications. Listed for honesty, and so future maintainers (or upstream PR reviewers) can decide whether to fold the fixes back into the docs or back out our workarounds.

### 1. Assign the inherited `PluginInterface::logosAPI` field in `initLogos`

**Doc says** (Part 1 §troubleshooting): *"check that `initLogos` assigns to the **global** `logosAPI` variable (defined in the Logos SDK / liblogos), not to a class member like `m_logosAPI`."*

**Reality**: there is no global `logosAPI` symbol in any SDK header. The "global" the doc means is actually a public field on `PluginInterface` itself:

```cpp
// liblogos-headers/include/interface.h
class PluginInterface {
public:
    // ...
    LogosAPI* logosAPI = nullptr;   // ← this is what Part 1 calls "the global"
};
```

Every plugin that inherits `PluginInterface` (transitively, via its module-specific interface header) has this field. The SDK uses it for event routing and inter-module calls.

**We do**: assign it in `initLogos` for both `app_core` and `app_ui`:

```cpp
void AppCorePlugin::initLogos(LogosAPI* logosAPIInstance) {
    logosAPI = logosAPIInstance;   // inherited from PluginInterface
    m_logosAPI = logosAPIInstance; // local convenience copy
    m_logos = new LogosModules(logosAPIInstance);
    // ...
}
```

**Symptom of getting this wrong**: `app_core::initLogos` runs and emits `statusChanged`, but `app_ui`'s subscription to `app_core.on("statusChanged", ...)` never fires — so the UI shows `?` forever even though storage is fine. Setting the inherited field fixed the cross-module event flow for us.

**Upstream**: the Part 1 troubleshooting text is misleading. There is no "global variable in liblogos"; it's a base-class field. Part 3's Step 5 example also doesn't show it being assigned and doesn't warn about it. Worth a tutorial PR.

### 2. Synchronous `init()` + `start()` for `storage_module` (`app_core/src/app_core_plugin.cpp`)

**Doc says** (developer guide §8.2):

> Prefer async wrappers. Use `doSomethingAsync(...)` instead of `doSomething(...)` to avoid blocking the caller's thread. Synchronous calls can cause hangs if the target module is slow to respond.

**We do**: call `storage_module.init(cfg)` and `storage_module.start()` synchronously inside `app_core::initLogos`.

**Why**: storage's calls return in ~20ms in practice. Sync is fine and keeps the code simple. The standalone-app's ui-host ready handshake has a 10s ceiling, so anything in `initLogos` that runs sub-second is safe.

### 3. Asynchronous `createNodeAsync()` + `startAsync()` for `delivery_module` with `Timeout(60000)`

**Doc shows** (PR #226 §3.6): *"All lifecycle calls (`createNode`, `start`, `stop`, `subscribe`, `unsubscribe`, `send`) are synchronous and return `LogosResult`."*

**We do**: async variants with `Timeout(60000)` instead of the documented sync path.

**Why** (verified by running the documented sync path once for comparison): `delivery_module.start()` blocks for exactly 20s in sync mode, then returns `success=false`. That's longer than the standalone-app's 10s ui-host ready timeout — so `app_ui` fails to load (the user sees "Failed to load UI plugin" before they ever see the status screen). The async variant returns `initLogos` immediately and lets `app_ui`'s ui-host complete its handshake, so the UI loads even though delivery is still spinning up.

**Note on the `Timeout(60000)`**: the SDK accepts the longer timeout and honors it (we see the `startAsync` callback fire at exactly 60s instead of 20s), but the outcome is the same — `success=false` regardless of timeout. The `Timeout` parameter is not in the developer-guide docs but is present in the generated `delivery_module_api.h`. The longer timeout is mostly cosmetic; deviation #4 below is what actually drives the visible status.

### 4. Delivery "started"/"connected" intended to come from `connectionStateChanged` event — currently produces `no` because the event never fires

**Doc shows** (PR #226 §3.1): subscribe to `connectionStateChanged` and treat the first non-empty status string as "node is up". *"Wire your handlers **before** `start()` so you don't miss the first `connectionStateChanged` event."*

**We do**: subscribe before `createNodeAsync` (line 49 vs 61 in `app_core_plugin.cpp`). On `connectionStateChanged` with `status == "Connected"`, set `m_deliveryStarted = true`.

**Observed**: the handler **never fires**, even over 90+ seconds with the delivery node actively relaying messages to four+ peers. We see `delivery.createNode cb success=1`, the delivery DEBUG log shows peers being added, relay messages flowing — and our `connectionStateChanged` handler logs nothing. `m_deliveryStarted` stays `false`, so the UI shows `no/no`.

**Why we still keep this code**: when the upstream bug is fixed, this is the right path. Falling back to `start()`'s success bool (#3) would also report `false` (also wrong), so there's no better local workaround.

**Upstream**: the `connectionStateChanged` event documented in PR #226 is not emitted by `logos-delivery-module@v0.1.x` under default config. Either the event is implemented under a different name in the shipped binary, or the implementation was never wired up. Either way, the doc and the binary disagree. **File against `logos-co/logos-delivery-module`.**

### 5. Storage "connected" indicator reuses the `start()` bool

**Doc says**: nothing — storage's "connected" semantics aren't defined in any merged doc.

**We do**: `app_core::storageConnected()` returns the same value as `storageStarted()`.

**Why**: spec called for a separate "connected" indicator per module. Delivery's planned `connectionStateChanged` doesn't have a storage equivalent (per `logos-co/logos-storage-module@v0.3.2`'s `storage_module_plugin.h`, only `storageStart`/`storageStop`/upload/download events exist — no peer-connected event). Treating `start() bool` as "connected" is the simplest defensible choice for an MVP.

**Upstream**: worth filing a question/PR on whether storage should expose a peer-count or peer-connected event.

### 6. Peer-count indicator removed from spec

**Original spec**: three indicators per module — started, connected, peers.

**We do**: dropped peers.

**Why**: neither `storage_module` (v0.3.2) nor `delivery_module` (v0.1.2) exposes a peer-count `Q_INVOKABLE` or event-payload field. `delivery_module` has `getNodeInfo()` which *might* contain a peer count under one of its identifiers — undocumented. Rather than probe, we cut the indicator.

### 7. Storage `init("{}")` — empty config, not the canonical one

**Doc/canonical config** at `logos-co/node-configs/storage_config.json` includes `listen-addrs`, `data-dir`, `disc-port`, etc. PR #284's revised tutorial passes a populated config.

**We do**: pass `"{}"` and let storage use defaults.

**Why**: the canonical config triggered `Unexpected field 'listen-addrs' while deserializing StorageConf*` at the underlying Nim deserializer in earlier testing. The schema in the shipped binary didn't match the documented JSON. Empty config bypasses the mismatch.

**Upstream**: the canonical config file and the deserializer schema may still be out of sync — re-test against the current storage_module pin before filing. (We didn't re-test in this round; the empty config keeps working so we left it.)

### 8. `app_ui` C++ backend uses `app_core::statusChanged` event to refresh PROPs

**Doc shows**: the Part 3 calc tutorial has the backend update PROPs internally (e.g., `setStatus("Ready")` in `initLogos`). It doesn't show a multi-module pattern where one Core's state needs to flow into another module's PROPs.

**We do**: `app_ui` C++ backend subscribes to `m_logos->app_core.on("statusChanged", ...)` and, on each event, calls `app_core.storageStartedAsync(cb)` etc., then `setStorageStarted(v)` in the callback. The setters auto-sync the PROPs over QtRO to the QML replica.

**Why**: it's the natural pattern given the Core+UI separation, but the docs don't show this specific shape. The generated `LogosModules` SDK exposes `module.on(eventName, cb)` for every module — verified in `app_core/result/include/app_core_api.h` and `delivery_module_api.h`. The dev guide §8 doesn't mention this `.on(...)` accessor; it should.

**Upstream**: worth a docs PR to surface the `.on(eventName, cb)` accessor as the cross-module event-subscription pattern.

## Project finding: `lgs` docs vs `tutorial-v2`

See `docs/findings/lgs-vs-tutorial-v2.md`. The `lgs` CLI's `docs/basecamp-module-requirements.md` requires a `follows` declaration on transitive `logos-module-builder` inputs. Following it caused our build to fail; **dropping the `follows` and using the tutorial-v2 unpinned-dep pattern works**. The `lgs` rule is probably scoped to `lgs basecamp install`'s flow but presented as a general module-authoring requirement.

## Project finding (one-pager)

The design rationale and Phase-3 one-pager from the planning session is at `docs/ideas/logos-status-app.md`.
