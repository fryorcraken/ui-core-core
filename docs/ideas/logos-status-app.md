# Logos Basecamp Status App

## Problem Statement

How might we validate the full Logos module architecture (QML UI module + C++ Core plugin + storage + delivery, wired through `logos-co/logos-tutorial@tutorial-v2`) end-to-end on one developer's machine in a single sitting, while being honest about what "started", "connected", and "peer count" each mean for the two subordinate modules?

## Recommended Direction

**Three modules, exactly as the architecture demands, ruthlessly minimal in implementation.** The point of the app is not the app — it is exercising the four-layer wiring: QML view → injected `logos` bridge → `app_core` C++ plugin → (`storage_module` + `delivery_module`). Cutting the Core layer would invalidate the experiment; gold-plating the QML would waste the sitting.

The QML module is **pure QML, no C++ at all** — that is the `tutorial-v2` convention for `type: "ui_qml"` modules, and it is the literal embodiment of "the UI is dumb." `Main.qml` calls into `app_core` via the host-injected `logos.callModule("app_core", "method", args)` for status reads, and subscribes to `logos.onModuleEvent("app_core", "statusChanged")` for push updates. The C++ "backend" of the UI is the Core plugin sitting in a different process; QML talks to it through Qt Remote Objects IPC managed by the host.

The UI shows two rows, one per backend module, each with three indicators rendered as plain text: **Started**, **Connected**, and **Peers**. "Started" is the `bool` returned by each module's `start()` call. "Connected" is also the `start()` bool (per project decision — the docs are ambiguous on a separate connected signal and a one-sitting smoke-test does not need to invent one). **"Peers" is a new integer indicator showing the number of currently-connected peers** — this is the actual moving signal that proves p2p is working when alice and bob are launched side by side.

Success: `lgs basecamp launch alice` and `lgs basecamp launch bob` in two terminals; both windows show `Started ✓ / Connected ✓ / Peers: ≥1` for both rows within ~30s of the second launch. Failure modes are tolerated as evidence — a stuck `Peers: 0` is a finding worth recording before deletion, not a bug to fix.

## Key Assumptions to Validate

- [ ] **`lgs basecamp install` resolves the transitive `logos-module-builder` correctly** for both `storage_module` and `delivery_module` once `follows` is wired in `app-core/flake.nix`. **Test:** clean clone, run `lgs basecamp install`, then `grep -c '"original":' app-core/flake.lock` should show one canonical `logos-module-builder` node (additional `_N` aliases must all `follows` it).
- [ ] **Two basecamp instances under the `logos.dev` preset peer to each other on localhost** without external bootstrap. **Test:** launch alice and bob; query `delivery_module.peerCount()` (or equivalent — see open question) from QML on a 1Hz timer and confirm both reach ≥1 within 30s. If not, accept it as a finding and ship.
- [ ] **The injected `logos.callModule(...)` bridge can reach methods that the Core plugin exposes as `Q_INVOKABLE`**, including returning `int` peer counts. **Test:** stub `app_core` with a single `Q_INVOKABLE int peerCount(const QString& moduleName) { return 42; }` first; only proceed to real wiring once QML reads back 42.
- [ ] **Both `storage_module` and `delivery_module` expose a peer-count surface at all.** **Test:** read `tutorial-v2/logos-developer-guide.md` §7 + the storage and delivery module READMEs at their pinned tags before writing any Core code. If neither exposes a count, **the "Peers" indicator becomes an open question, not a feature**.
- [ ] **The `metadata.json` name-matching contract (own name ↔ flake input attr ↔ dependents' `dependencies[]`) is sufficient** for `app-ui` to discover `app_core` via the `logos` bridge at runtime. **Test:** UI module calls `logos.callModule("app_core", "ping", [])` and gets a response without any include-path or registration glue beyond what `mkLogosModule` / `mkLogosQmlModule` generates.

## MVP Scope

**In:**

- `app-core/` — `type: "core"`, `name: "app_core"`. Flake inputs (`tutorial-v2` pin everywhere):

  ```nix
  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder/tutorial-v2";
    storage_module.url       = "github:logos-co/logos-storage-module/<v0.3.2-or-current>";
    delivery_module.url      = "github:logos-co/logos-delivery-module/<v0.1.1-or-current>";
    storage_module.inputs.logos-module-builder.follows  = "logos-module-builder";
    delivery_module.inputs.logos-module-builder.follows = "logos-module-builder";
  };
  ```

  `metadata.json#dependencies: ["storage_module", "delivery_module"]`, `main: "app_core_plugin"`.

  Plugin exposes six `Q_INVOKABLE` methods returning current snapshot: `bool storageStarted()`, `bool storageConnected()`, `int storagePeerCount()`, `bool deliveryStarted()`, `bool deliveryConnected()`, `int deliveryPeerCount()`. The plugin also emits an `eventResponse("statusChanged", QVariantList{})` whenever any value changes, so QML can `logos.onModuleEvent("app_core", "statusChanged")` and refresh, rather than polling for itself.

  On `initLogos`: call `storage_module.init(cfg)` → `start()` (store the bool as `_storageStarted` and `_storageConnected`); call `delivery_module.createNode(cfg)` → `start()` (store the bool as `_deliveryStarted` and `_deliveryConnected`); start one 1Hz `QTimer` that calls `<module>.peerCount()` (or the equivalent surface, TBD) on both modules and emits `statusChanged` if either value changes.

- `app-ui/` — `type: "ui_qml"`, `name: "app_ui"`, **no C++**. Flake input: `app_core` via `github:` URL with `?dir=` (tutorial-v2 convention) for the captured form, plus an `--override-input app_core path:../app-core` for local dev. `metadata.json#view: "Main.qml"`, `dependencies: ["app_core"]`. `Main.qml` is one `Item` with a `Column` of six `Text` elements (`Storage: Started ✓ / Connected ✓ / Peers: 2` × 2 rows), wired through `logos.callModule("app_core", method, [])` and refreshed on `logos.onModuleEvent("app_core", "statusChanged")`. No layouts, no styling, no design-system imports.

- A working `scaffold.toml` with `[modules]` entries for both modules captured via `lgs basecamp modules --flake "./app-core#lgx" --flake "./app-ui#lgx"`.

- CLAUDE.md rewritten to match `tutorial-v2` (replace all `tutorial-v1` references; correct the QML module structure; replace the `Q_PROPERTY` binding pattern with the `logos.callModule` / `logos.onModuleEvent` pattern; correct the storage "connected" indicator to use `start()` bool; add the peer-count indicator).

**Out:**

- C++ in the UI module (would violate `tutorial-v2` `ui_qml` convention).
- `Q_PROPERTY` binding from Core to UI (not how `tutorial-v2` does cross-module QML↔C++ talk; the host injects `logos`, not the Core's `QObject`s).
- Async variants of backend calls. The sync `LogosResult` form is fine.
- Recovery, retry, reconnect logic.
- A `LogosResult` typed wrapper beyond reading `.success` / `.getError()`.
- Per-platform polish (Linux dev path is the only target).
- `Logos.Theme` / `Logos.Controls` design-system imports — they're available, but the sitting doesn't need them.

## Not Doing (and Why)

- **Questioning the three-module architecture** — `app-ui` (`ui_qml` with C++ backend, `#ui-qml-backend` template) → `app-core` (`core`) → `storage_module` + `delivery_module` is the spec. It is the primary directive. Not a design question.
- **Filing logos-docs issues about ambiguous "connected" semantics** — defer until after the sitting reveals whether `start()` bool == connected is actually misleading in practice. May be a perfectly fine answer.
- **Theming, dark mode, window chrome, app icon, About dialog, anything stylistic** — the app gets deleted after the sitting. Cosmetic effort is pure waste.
- **Tests (incl. the `tests/*.mjs` integration framework the tutorial documents)** — the app is itself a test. Writing tests for a one-sitting smoke-test inverts the cost/benefit.
- **Persisting status history, charts, timelines** — current state only; the moment it works, the experiment is done.
- **A README for the app** — CLAUDE.md is the only doc that needs to exist; the app is throwaway and the doc moves with it.

## Open Questions

- **Does the storage_module API expose a peer count, and what's it called?** PR #284 documents `peerId()` and `spr()` but not a count. May need to grep the plugin source at the pinned tag. If absent, the storage "Peers" indicator stays `0` with a TODO + issue link. Resolve at implementation time, do not pre-design.
- **Does the delivery_module API expose a peer count?** The `connectionStateChanged` event payload is `[QString status, QString iso8601Timestamp]` — no count there. Likely a separate method. Same resolution path as storage.
- **If `delivery_module.connectionStateChanged` never fires in a two-instance localhost setup under `logos.dev`**, do we (a) accept "Connected ✗ / Peers: 0" as a documented finding and ship, (b) pivot to a different preset, or (c) escalate? Default: (a).
- **The Core plugin's `statusChanged` event vs. QML polling** — push via `eventResponse` is the tutorial's idiom (calc_module's `versionReady`). Use it unless something breaks. Do not pre-design fallback polling on the QML side.
- **Tag pinning for storage and delivery modules** — `v0.3.2` and `v0.1.1` are the values from the open docs PRs, but `tutorial-v2`-aligned tags may now exist. Verify before pinning; prefer whatever the in-tree `flake.lock` of a known-working `tutorial-v2` consumer pins to.
