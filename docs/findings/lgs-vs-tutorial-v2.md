# Finding: `lgs` (logos-scaffold) docs diverge from `logos-tutorial@tutorial-v2`

## Status
Open — to file upstream once the app is working.

## Summary

`logos-co/scaffold` (the `lgs` / `logos-scaffold` CLI) ships `docs/basecamp-module-requirements.md` with author-side rules that contradict the **authoritative** `logos-co/logos-tutorial@tutorial-v2` developer guide and tutorials. Following the `lgs` rules causes builds to fail in ways the tutorial does not warn about and the tutorial's working pattern does not require.

## Authoritative source

- `logos-co/logos-tutorial` at tag `tutorial-v2`
  - `logos-developer-guide.md` — §1, §8, §9.2, and the troubleshooting section "UI module `nix run` fails to load dependencies"
  - `tutorial-wrapping-c-library.md`, `tutorial-qml-ui-app.md`, `tutorial-cpp-ui-app.md`

The tutorial pattern for declaring a dependency on another Logos module is, verbatim from the developer guide §9.2:

> Each entry in `dependencies` must match the `name` field in that module's own `metadata.json`. When adding a dependency as a flake input, the **input attribute name** must also match the dependency name — e.g., `waku_module.url = "github:logos-co/logos-waku-module"`. The URL can point to any repo, but the attribute name is how the builder resolves dependencies.

And from troubleshooting:

```nix
inputs = {
  logos-module-builder.url = "github:logos-co/logos-module-builder/tutorial-v2";
  calc_module.url   = "github:logos-co/logos-tutorial/tutorial-v2?dir=logos-calc-module";
  storage_module.url = "github:logos-co/logos-storage-module";
};
```

No version pins on third-party modules, no `follows` overrides, no special transitive-input rules.

## What `lgs` docs claim instead

`logos-co/scaffold/docs/basecamp-module-requirements.md` — §"Transitive inputs must `follows` the top-level `logos-module-builder`":

> Multi-sub-flake projects that pull in modules which themselves depend on `logos-module-builder` (e.g. `delivery_module` → `logos-module-builder`) **must** unify that transitive reference onto the project's top-level pin using a `follows` entry.
>
> ```nix
> delivery_module.inputs.logos-module-builder.follows = "logos-module-builder";
> ```
>
> Symptoms when this is missing:
> - `lgs basecamp install` fails inside `nix build` with errors from a newer `logos-module-builder` (e.g. `no 'main' field in metadata.json`).

## Observed contradiction

Applying the `lgs` `follows` advice to `app-core/flake.nix` while building with plain `nix build`:

```nix
inputs.delivery_module.inputs.logos-module-builder.follows = "logos-module-builder";
```

→ Causes the `delivery_module` build to fail with:

```
src/delivery_module_plugin.h:9:10: fatal error: logos_module_context.h: No such file or directory
    9 | #include <logos_module_context.h>
```

The `follows` forces `delivery_module` to be evaluated against `tutorial-v2`'s `logos-module-builder`, which does not provide `logos_module_context.h` — that header comes from a newer `logos-module-builder` revision the upstream `delivery_module` was authored against.

Removing the `follows` (i.e. following the tutorial-v2 pattern verbatim) → **build succeeds**, both modules load at runtime via `nix run .`, and the full chain `capability_module → delivery_module → storage_module → app_core → app_ui` initializes correctly.

## Root cause hypothesis

The `lgs basecamp install` workflow probably needs `follows` because it executes `nix build` with `--override-input` flags that collapse different module-builder revisions across sibling sub-flakes, exposing the mismatch. Plain `nix build` (and `nix run` for UI modules) — which is what the tutorial documents — lets each module use its own pinned `logos-module-builder` transitively, and the bundling pipeline picks up the prebuilt `.lgx` artifacts rather than rebuilding from collapsed sources.

If that hypothesis holds, the `lgs` doc is correct **for the `lgs basecamp install` flow specifically** but presents the rule as a general module-authoring requirement. That's the friction.

## Recommendation to upstream

Either:

1. Scope the `lgs` `follows` requirement to projects that go through `lgs basecamp install`, and explicitly note that plain `nix build` / `nix run` per the tutorial does **not** need it.
2. Fix the `lgs basecamp install` pipeline so it doesn't need `follows` either — by not collapsing sibling sub-flake module-builder pins via `--override-input`, or by only collapsing them when they match.

In either case, the `lgs` doc should reference the tutorial-v2 developer guide as the authoritative source for module authoring, and confine itself to documenting the `lgs` workflow on top.

## To-do

- [ ] Confirm hypothesis by reading `lgs basecamp install` source (`src/basecamp/*.rs`) for the `--override-input` logic.
- [ ] Open issue / PR on `logos-co/scaffold` once the app is fully working and this finding has been reproduced cleanly.
