# Agent Development Guide

A file for guiding coding agents.

## Commands

- Never use git commands except readonly ones like `git status` and `git diff`.
- Run all commands from the workspace root. Create any temp files in `${workspaceRoot}/tmp`.
- `sun fmt` for formatting sun source files
- Format only first-party paths: `sun fmt src/ui.sun src/styles.sun src/markup src/drawing.sun src/plot src/adapters src/tools examples tests`. Do not recursively format `src/`, which includes the independently maintained sun_gobject and sun_video_editor checkouts.
- Core build and tests: `./build.sh`. Optional GTK build and acceptance tests: `./build.sh --gtk` with `SUN_GOBJECT_BUILD` pointing to a compatible dependency build.
- Core build: `sun src/tools/build.sun -- build`.
- Tests: `sun src/tools/build.sun -- test`; selected tests: `sun test tests/main.sun --test-filter <name>`.
- Keep Sun source in `src/`, platform adapters in `src/adapters/`, tests in `tests/`, and generated artifacts in `build/`.
- Keep build tools in `src/tools/`. Declare target source paths, types, and artifact names in `sun-config.json`; build orchestration should select those targets by name.
- Give each example program its own folder directly under `examples/`, with a `main.sun` entry point and its supporting files. The plot viewer shares `examples/plot-export/data.sun`.
- The core must not depend on GTK or sun_gobject. Keep application state explicitly owned and platform handles out of component code.
- sun_gobject is a vcs2l-managed checkout at `src/sun_gobject/`, ignored by this repository. Development and CI use `deps.repos`, which tracks `main`; CI selects only the GTK dependency. Read its own AGENTS.md before making dependency changes. Its default build path is `src/sun_gobject/build`.
- sun_video_editor is an independent application checkout at `src/sun_video_editor/`, registered in `deps.repos` and ignored here. Read its AGENTS.md before editing it. Its build, tests, and packages belong to that repository; the toolkit must not depend on it.
- CI imports only sun_gobject from `deps.repos` through `scripts/import-ci-dependencies.sh`, so toolkit checks do not require access to the private editor repository.
- sun_video_editor is an independent application checkout at `src/sun_video_editor/`, registered in `deps.repos` and ignored here. Read its AGENTS.md before editing it. Its build, tests, and packages belong to that repository; the toolkit must not depend on it.
- CI imports only sun_gobject from `deps.repos` through `scripts/import-ci-dependencies.sh`, so toolkit checks do not require access to the private editor repository.
- Keep sun_plot in `src/plot/`, with tests in `tests/plot/` and examples in `examples/plot-export/` and `examples/plot-viewer/`. Build it as `sun_plot.moon` against sun_ui's public APIs; sun_ui must not depend on sun_plot.
- `./build.sh` also builds and tests sun_plot and generates its SVG example. `./build.sh --plot-gtk` additionally builds and tests the native plot viewer against sun_gobject's checked GTK bindings.
- Keep GTK declarations, native ownership, callbacks, and thread checks in the sun_gobject checkout's sun_gtk bindings. UI adapters should use safe APIs and own application behavior, not native handles.

## Sun Language Conventions

- Implement as much project functionality, tooling, and test logic as possible in Sun rather than other scripts or languages. Use other languages only where Sun cannot reasonably provide the required functionality.

- Sun does not allow implicit copies; make ownership transfers and any required copies explicit.
- Sun does not have macros; use ordinary functions for reusable logic.
- Keep allocations explicit and avoid `unsafe` blocks wherever possible.

## Code Comments (all code, including .sun files)
- Write concise comments in plain English for a general audience of software engineers.
- Every namespace, function, and type definition must have a documentation comment describing what it does or what it is for. This applies to all first-party source code, including C++ and `.sun` files, regardless of visibility.
- Use `/** ... */` blocks in C++, Sun, and JavaScript/TypeScript. Use the language's native equivalent elsewhere, such as Python docstrings or a shell comment block.
- Cover private and internal helpers, methods, constructors, destructors, anonymous namespaces, classes, structs, interfaces, enums, and type aliases, as well as public APIs. Document modules too.
- Put the comment immediately before the declaration or definition it documents. For a function declared in a header and implemented separately, document the declaration; add implementation comments when they explain details beyond that contract.
- Explain the purpose or behavior instead of merely restating the symbol name. Mention ownership, side effects, constraints, or failure behavior when needed to understand correct use.
- Preserve useful existing documentation and keep it accurate when changing code. Do not edit generated code or third-party dependencies to add comments.
- Do not hard-code numeric values that are subject to change in comments.

## Commit messages
- Subject: `<scope>: <description>` (scope = subsystem/package/area; imperative; no `feat`/`fix` types).
- Body: blank line, then one concise bullet per key change if not already captured in the subject.
- Do not add AI attribution to commits or PRs (no Co-Authored-By, Generated-with, tool names, or session links).

## Planning

- Write plans in plain english, avoiding unnecessary jargon and abreviations.
- Use simple, concrete examples in explanations when it makes sense to do so.

## Issues and PR Guidelines

- Never create PRs or issues unless specifically asked by the user.

## Roadmap

- Roadmap items that are entirely completed should be removed from the roadmap doc.
