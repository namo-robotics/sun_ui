# sun_ui

An experimental React-like UI core written in Sun, with an optional GTK adapter.
This is the first prototype for [sun_gobject issue #1](https://github.com/namo-robotics/sun_gobject/issues/1).
Application state produces a description; queued messages update state; reconciliation
updates existing controls while preserving their keyed identity.

The first view element means a **semantic native button**, including its label,
keyboard activation, focus, and accessible button role. Views currently compose a
single flat sibling list in a vertical layout. This deliberately small vocabulary
supports a counter and a keyed task list; it is not yet a general widget toolkit.

## Development setup

The compiler bootstrap, Ubuntu development container, VS Code tasks, Sun test
runner, formatting, and CI follow `sun_gobject`. The core needs only Sun and its
standard library. There is no core dependency on GTK, `sun_gobject`, or a platform
event loop, and no native build step in this repository.

```sh
./build.sh                   # core bundle and independent tests
sun fmt --check src adapters tools examples tests
sun tools/build.sun -- build
sun tools/build.sun -- test
```

Use the published Sun development Debian package, not an older system compiler.
`./scripts/bootstrap-sun.sh --update` installs the current development package;
it requires network access and package-install privileges. The normal bootstrap
reuses an installed compiler. `SUN_EXECUTABLE=/absolute/path/to/sun ./build.sh`
selects an existing compiler explicitly. The version used is recorded in
`build/sun-version.txt`. The prototype was verified with `cc7686189ee2`.

Open in the included devcontainer, or build the same image directly:

```sh
docker build -t sun-ui-dev .
docker run --rm --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspaces/sun_ui" sun-ui-dev ./build.sh
```

### Optional GTK prototypes

GTK needs a compatible `sun_gobject` build produced with the same Sun compiler,
GTK 4 development packages (`pkg-config --libs gtk4`), and, for headless testing,
Xvfb, xauth, and dbus-run-session. The development image includes these system
packages. CI tests against `sun_gobject` revision
`8a6529dd7dc314da82ce935a66bd6359275bb2dc`.

Build `sun_gobject` using its own `./build.sh`, then point this package at its
build directory. The default is `../sun_gobject/build`:

```sh
SUN_GOBJECT_BUILD=../sun_gobject/build ./build.sh --gtk
./build/counter
./build/tasks
```

Inside a devcontainer, place the dependency checkout under `tmp/sun_gobject` and
set `SUN_GOBJECT_BUILD=tmp/sun_gobject/build`, or explicitly mount a sibling checkout.
The dependency directory must contain `sun_gobject.moon`, `sun_gtk.moon`, and
`native/libsun_gobject_native.a`. The build never modifies that dependency.

The counter updates one button's label per dispatch. In the task list, click a
task to select it, then use Add, Reverse, and Remove selected. Task IDs remain
stable across ordering changes. Both programs accept `--self-test`; `./build.sh
--gtk` runs these scenarios under a virtual display with fatal GTK warnings and
a timeout. CI separately builds the core without a dependency checkout.

## Package layout

| Path | Responsibility |
| --- | --- |
| `src/ui.sun` | Descriptions, mounted identity, reconciliation, events, state sessions |
| `adapters/gtk/renderer.sun` | Optional GTK window, button ownership, ordering, focus, event polling |
| `examples/` | Counter and composed keyed task list |
| `tests/` | Independent recording-renderer tests |
| `tools/build.sun` | Build and test orchestration in Sun |
| `scripts/bootstrap-sun.sh` | Published compiler installation |
| `build/` | Ignored bundles, programs, version evidence, and test logs |

Consumers import `sun_ui.moon`. GTK consumers additionally import
`sun_ui_gtk.moon`, `sun_gobject.moon`, and `sun_gtk.moon`, and link GTK and the
`sun_gobject` native archive. Component functions never receive platform handles.
No C/C++ code or changes to `sun_gobject` were required; the small unsafe GTK box
and focus boundary is confined to the optional adapter.

## Ownership and update contract

- `Session<State>` owns application state. A named view function receives a
  `const ref State`; a named event handler receives a temporary mutable borrow.
  Ordinary functions compose components by appending owned elements to a view.
- `View` owns its vector and label strings. Passing it to `Tree.render` consumes
  it. The mounted tree retains the last description; GTK copies label bytes.
- An element has a sibling key, label, action number, and integer payload. The
  action and payload are captured into an owned `Event` when queued. Task IDs
  therefore need no borrowed closure captures. Arbitrary typed payloads and
  component-local state are future work.
- Keys identify siblings. Reordering preserves the same mounted identity and GTK
  button. Removing and reinserting a key creates a fresh identity. Duplicate keys
  are rejected before any platform mutation. A `Tree` and its renderer form one
  lifetime pair; numeric mounted identities are local to that pair.
- `Tree.enqueue` only queues; it never invokes handlers. `Session.dispatch`
  processes the queue length observed at entry, then renders once if any live
  handler ran. Handler-posted events wait for the next dispatch. Events from
  unmounted identities are skipped. State changes within a batch are not commits:
  unmount takes effect when the batch's render finishes.
- The platform loop owns scheduling: poll native events, capture messages, call
  `dispatch`, repeat. GTK is given a turn before each batch, drains at most a
  bounded number of clicks per control, and blocks only when no queued work
  remains. FIFO ordering holds within the core queue; GTK's per-button polling
  queues do not preserve a global chronological order across different buttons.

## Effects, cleanup, and failure

Views only describe controls. Native click subscriptions belong to mounted
controls and are disconnected when those controls are removed. Reordering does
not reconnect them. `View.focus_key` requests a focus effect applied **after** a
successful commit; ordinary updates do not force focus. This prototype does not
provide a general user-effect or hook API.

Call `Tree.shutdown(renderer)` when a session ends. It idempotently clears all
mounted resources. The GTK renderer also releases its controls, subscriptions,
window, and application on destruction, including error exits.

Duplicate-key errors leave the prior interface intact and allow a corrected
render. A renderer error can occur after partial native changes: the core clears
all controls and permanently stops that tree. A handler error clears controls
and queued messages and stops dispatch; already-applied state mutations are not
rolled back. Recreate the session/tree to recover from these terminal errors.
Focus is best-effort, as GTK may decline to focus a control in an inactive window.
Tests check the window's logical focus widget, independent of window-manager
activation. Allocation failure follows the Sun standard library's behavior; it
is not presented as a recoverable render transaction.

## Costs and boundaries

Each view allocates a vector and owned label strings; each commit allocates a
replacement mounted vector. Event queues own their storage and grow as needed.
Label updates currently clone the string at the safe `sun_gobject` boundary.
The initial vectors have nonzero capacity, as required by the tested Sun release.
No reusable arena or allocation-free render guarantee is provided.

Key lookup and duplicate validation are linear scans, giving quadratic work for
a full list; front-removal from vectors also moves remaining entries. This is
appropriate for the small prototypes, not a large-list performance claim. GTK
ordering checks avoid unnecessary native reorders, and unchanged labels avoid
native text updates.

GTK handles layout, text rendering, keyboard activation, focus navigation, and
native button accessibility. The core does not implement layout, text editing,
input methods, or accessibility trees. The headless tests validate framework
semantics; the GTK tests validate actual clicked signals, native sibling order,
logical focus preservation, targeted updates, and removal. Screen-reader behavior
and interactive window-manager behavior have not been manually validated.

Remaining exploration includes nested layouts and parent-scoped keys, typed
message payloads, general effect dependencies, editable controls, reusable render
storage, and additional adapters. Hooks and captured closures remain out of scope
until their ownership and lifetime contracts are established.
