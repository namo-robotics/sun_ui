# Declarative XML views

XML is an optional build-time frontend to `ui.View`. Generated code uses the same
owned descriptions, keyed reconciliation, state sessions, and semantic events as
handwritten Sun. Applications do not parse XML at runtime.

## Build

Use the current published Sun development compiler, which includes XML reader and
attribute source spans. The build checks both APIs and explains how to update an
older compiler. No separate stdlib bundle is needed.

```sh
./scripts/bootstrap-sun.sh --update  # if the installed compiler is older
./build.sh                         # core, plotting, XML generation and tests
./build.sh --gtk                   # also handwritten and XML GTK acceptance
./build/forms-xml
```

For focused work, `sun src/tools/build.sun -- markup` builds the compiler, runs compiler
and generated-description tests, and generates examples without GTK or sun_gobject.
`sun src/tools/build.sun -- markup-gtk` additionally compiles and runs the generated
counter, task list, and form acceptance scenarios after the GTK adapter is built.
Both are included in the corresponding normal build and CI workflows.

To generate another view:

```sh
build/sun-viewc examples/forms/forms.view.xml build/generated/examples/forms/forms.view.sun
```

Include the generated `.sun` file in the application's `source_files` manifest.
Keep application state and handlers in handwritten files. Outputs and `.map`
files live under `build/`; do not edit them. Every invocation reads the complete
import graph, so changes to imports regenerate consumers. Failed generation
returns nonzero and stops the build before compiling stale output.

## Views and expressions

```xml
<Views module="counter">
  <View name="view" state="i64">
    <Button key="{1}" text="{`Count: ${state}`}" action="1" />
  </View>
</Views>
```

A `View` generates `function view(state: const ref State) ui.View`. The generated
function provides allocator `a` and owned description `result`. It never retains
state or borrowed strings. Whole-attribute `{...}` values are ordinary Sun
expressions, checked by Sun. Write `text="{state.name.clone(a)}"` when a copy is
needed. Use Sun formatting expressions for dynamic text; there is no second
expression interpreter. XML escaping still applies inside expressions.

String properties without braces are literals. Numeric and boolean properties
use Sun literals. Literal keys are strings; `{1}` is an integer key, distinct from
`"1"`. Use `{String("...")}` with XML-escaped quotes for a string whose contents
look like an expression.

`If test="{condition}"` supports a final `Else` child. `For each="id"
in="{items}" key="{id}"` evaluates the collection once and creates a keyed scope
for each item. Collection values must expose `size()` and indexed access. Repeated
items require explicit stable keys; static children without keys receive a key
based on their declaration position. Use explicit keys when identity must survive
source edits.

Elements and attributes use unqualified names. Formatting whitespace and comments
are ignored; text content belongs in explicit properties. Unknown declarations
and attributes are errors. Parser diagnostics point into the original file;
generated source maps retain original filenames and byte offsets. The example
build wrapper adds original XML locations to Sun compilation diagnostics.

## Named actions and automatic keys

Static control keys are optional. Omit `key` to receive a deterministic declaration
key. Keep an explicit key when it is used for focus/lookup or must survive source
reorganization. A `For` still requires a stable item key; automatic indexes would
break identity when items are reordered or removed.

```xml
<View name="view" state="State" fallback="handle_numeric">
  <Button text="Add task" action="add_task" />
  <Button text="Reverse tasks" action="state.reverse_tasks" />
  <TextInput value="{state.draft.clone(a)}" changeAction="edit_draft" />
  <For each="id" in="{visible_ids(state)}" key="{id}">
    <Task id="{id}" selected="{state.selected == id}" />
  </For>
</View>
```

A bare action name refers to a Sun function, including qualified names such as
`handlers.add_task`. The function takes `(state: ref State, event: const ref
ui.Event, queue: ref ui.EventQueue)` and returns `SystemResult<void>`. A name
starting with `state.` calls a method on the owned application state; the method
takes only `(event: const ref ui.Event, queue: ref ui.EventQueue)` and returns the
same result type. Sun checks handler names, accessibility, and signatures.

For a view named `view`, the generated dispatcher is `view_handle`. Pass it to
`ui.Session<State>(state, view, view_handle)`. Handlers run during normal queued
dispatch, never while constructing the view or collecting native signals. Payloads
and captured edit values arrive unchanged, and handler errors follow the normal
session cleanup policy. No function closures or state references are retained in
the description.

Numeric literals and numeric expressions such as `action="{actions.select}"`
remain supported. Optional `fallback="handle_numeric"` receives those events;
without a fallback the generated dispatcher ignores numeric events. Generated IDs
are internal and carry a separate captured `action_scope`, so a numeric ID cannot
accidentally call a named handler. Do not persist generated IDs or key spellings.

Named actions are supported on controls declared inside a `View`, including its
`If` and `For` bodies. Reusable component definitions have no application state
type; they continue to accept explicit numeric action parameters and route through
the view's fallback. Specify either `action` or `changeAction` on a control.

## Controls and layout

| Tag | Main properties |
| --- | --- |
| `Row`, `Column` | `spacing` |
| `Grid` | `spacing`, `Grid.Rows`, `Grid.Columns` |
| `Scroll` | At most one physical child, including through component scopes |
| `Label`, `Button` | `text`; buttons also have `action` and `payload` |
| `TextInput` | `value`, `changeAction`, `payload` |
| `Checkbox` | `text`, `checked`, `changeAction`, `payload` |
| `Select` | `selected` index, `changeAction`, `payload`, `Select.Options` |

Common properties are `enabled`, `visible`, uniform `margin`, `minWidth`,
`minHeight`, requested `width` and `height`, `horizontal` and `vertical` alignment
(`Fill`, `Start`, `Center`, `End`), and `expandX`/`expandY`. Parent layout may allocate
more than requested dimensions. Hidden controls keep identity but occupy no
space; omitting a node with `If` unmounts it.

Grid children use zero-based `row`, `column`, `rowSpan`, and `columnSpan`. Tracks
are explicit property elements; absent rows or columns mean one automatic track.

```xml
<Grid key="fields" spacing="8">
  <Grid.Rows><Track kind="Auto" /></Grid.Rows>
  <Grid.Columns>
    <Track kind="Fixed" value="140" />
    <Track kind="Weight" value="1" />
    <Track kind="Weight" value="2" />
  </Grid.Columns>
  <Label key="caption" text="Name" />
  <TextInput key="name" column="1" columnSpan="2"
             value="{state.name.clone(a)}" changeAction="{actions.edit_name}" />
</Grid>
```

Fixed sizes and weights must be positive. Automatic tracks use content requests;
weighted tracks share surplus space after content requests and fixed tracks.
Spans must stay within declared tracks. These checks run before renderer mutation.

Select options are `<Select.Options><Option key="normal" text="Normal" /> ...
</Select.Options>`. The selected property is an index; the emitted value contains
the option's stable application key captured at signal time.

## Components, templates, and resources

A `Component` or `Template` declares `Param` children with explicit Sun types and
generates a function taking `result: ref ui.View`, `parent: i64`, then its inputs.
Calling a component creates an independent keyed scope. Scope nodes do not add
native layout containers. Changing the template/function identity remounts its
subtree even when the instance key stays the same.

```xml
<Template name="Footer">
  <Param name="content" type="function (ref ui.View, i64) void" />
  <Column key="footer"><Slot name="content" /></Column>
</Template>
```

Supply slots as named, typed Sun functions, for example
`<Footer key="footer" content="{render_footer}" />`. Slots run only during view
construction and append owned nodes; they are not retained event callbacks.
Function-valued slots remain supported alongside named XML content slots.

For XML content, declare named `Content` inputs and insert each with `Slot`:

```xml
<Component name="Panel">
  <Content name="header" />
  <Content name="body" />
  <Column styleResource="panel">
    <Slot name="header" />
    <Slot name="body" />
  </Column>
</Component>
```

Call it with named property elements:

```xml
<Panel key="preview">
  <Panel.header><Label text="Preview" /></Panel.header>
  <Panel.body><Label text="{state.name.clone(a)}" /></Panel.body>
</Panel>
```

Every content input is required; an empty property element supplies an empty body.
Each declaration needs exactly one unconditional insertion point, outside `If`,
`For`, and other component content bodies. Supplied bodies can contain conditions,
loops, nested components, and named actions. Expressions, resources, and actions
belong to the caller; the wrapper's own elements use its local resources.

Content slots generate `ref ui.ContentSlot` parameters recording nonvisual keyed
insertion groups. The component builds its structure first; the caller then builds
content beneath those groups. No closures or state references survive view
construction. Logical sibling order determines native layout, independently of
when slot descendants are appended. Imported signatures declare the same
`Content` inputs. For template-resource calls, use `<Use.header>` and `<Use.body>`.

Register application components once instead of repeating their signatures in each
view. Put `components.registry.xml` beside the application's root view:

```xml
<ComponentRegistry>
  <Source path="components.view.xml" />
</ComponentRegistry>
```

The generator discovers every top-level `Component` and `Template` in those XML
files and their dependencies. Their declared names, `Param` types, and `Content`
slots become available before any view is generated. Views use the names directly:

```xml
<Views module="video_app">
  <View name="view" state="State">
    <LabeledField text="Save as">
      <LabeledField.body>
        <TextInput value="{state.output.clone(a)}" changeAction="2" />
      </LabeledField.body>
    </LabeledField>
  </View>
</Views>
```

`LabeledField` is defined once in a registered source, including its `text`
parameter and `body` slot. No `<Import>` declaration is needed in the view.
Registered components can use other registered components regardless of source
listing order. Registration is local to one generation invocation, not a process
or runtime global.

`sun-viewc` automatically reads an adjacent `components.registry.xml`. For views
in another directory, select the application registry explicitly:

```sh
build/sun-viewc examples/app/views/main.view.xml build/generated/app/main.view.sun \
  --registry examples/app/components.registry.xml
```

Source paths are relative to the registry file, must end in `.view.xml`, and
cannot contain parent traversal. Repeated source paths are deduplicated. Duplicate
component names across files, names that shadow built-in controls, and conflicts
with local component/import declarations are errors. A component's own definition
is retained when compiling its registered source. Missing registrations and
incorrect inputs fail during generation; Sun still checks the generated function
calls and ownership. All registered files are regenerated and checked, including
unused components. Source maps retain their original XML locations, and failed
generation does not publish partial source output.

For explicit function bindings without an application registry, `Import` declares
a Sun component function and its named inputs. An optional `source` imports
another `.view.xml` file:

```xml
<Import name="Footer" function="form_parts.Footer" source="parts/footer.view.xml">
  <Param name="content" type="function (ref ui.View, i64) void" />
</Import>
```

Import paths are relative, with no parent traversal. Generation rejects cycles,
deduplicates shared dependencies, preserves their relative artifact paths, and
combines their code into the consumer's generated compilation unit.

Use `<Include source="theme.view.xml" />` to include shared resource definitions
without declaring a component import. The same path, cycle, regeneration, and
source-map rules apply. Generated resource constructors are public; call them
through their module, for example `resources="{studio.resources()}"`. An include
makes definitions available but does not automatically merge resource dictionaries.

A root `Resources name="resources"` generates a fallible dictionary constructor.
Its `Color`, `Font`, `Spacing`, `Style`, `Theme`, and `Template` children have names and
explicit types. `scope="theme"` selects the theme dictionary; the default scope
is application. `ref` creates an alias and `basedOn` extends a style. The entire
dictionary is validated, including unused aliases. Colors are packed RGB integers.

```xml
<Resources name="resources">
  <Style name="base" scope="theme">
    <Normal font="Sans" fontSize="14" />
  </Style>
  <Style name="primary" basedOn="base">
    <Normal foreground="0x202735" background="0xf4f6fa" />
    <Focus background="0xe0eaff" />
  </Style>
  <Template name="footerTemplate" value="Footer" />
</Resources>
```

Styles support `Normal`, `Hover`, `Focus`, and `Disabled` appearances. Select a
resolved theme with `View theme="{theme(state)}"`; select a named resolved style
with a control's `style` expression. Direct `foreground`, `background`, `font`,
and `fontSize` properties override the normal appearance. Precedence is native
defaults, view theme, control style, then direct overrides. Theme changes update
properties without replacing unchanged controls.

Style appearances can reference palette entries using `foregroundResource`,
`backgroundResource`, and `fontResource`. `fontSizeResource`, `borderWidthResource`,
`radiusResource`, and `paddingResource` require `Spacing` entries. A property cannot
have both a literal and a resource reference in the same appearance. References
resolve in the style's defining scope, including forward references; a derived
style can override the resolved base properties. Numeric values must fit their
appearance fields, and font sizes must be positive.

A theme resource combines a base style with control-kind defaults:

```xml
<Resources name="resources">
  <Color name="accent" value="0x73e2cb" />
  <Style name="base"><Normal font="Sans" fontSize="14" /></Style>
  <Style name="field"><Normal foregroundResource="accent" padding="8" /></Style>
  <Theme name="studio" basedOn="base">
    <Default kind="TextInput" styleResource="field" />
    <Default kind="Select" styleResource="field" />
  </Theme>
</Resources>
<View name="view" state="State" resources="{resources()}" themeResource="studio">
  <TextInput value="{state.name.clone(a)}" changeAction="1" />
</View>
```

`Theme.basedOn` names a base **style**, not another theme. Defaults name styles
and native control kinds; duplicate kinds are errors. `themeResource` and the
existing `theme` expression are mutually exclusive. Precedence is native
defaults, theme base, control-kind default, explicit control style, then direct
overrides. All interaction states participate in this order. All resources,
including unused themes and styles, are validated before native mutation.

Handwritten views can call `Resources.theme`, then `View.set_theme`, or use
`View.theme_resource` to resolve a name in the current scope. `ui.Theme` owns its
base `Style` and `Vec<ThemeDefault>`. The existing `View.theme: Style` remains
available. Theme changes preserve unchanged control identities.

Use `<Use template="footerTemplate" ... />` for a template resource. Its value must
name a declared component, template, or import. The generator emits a function
accessor with the target's full parameter signature; Sun checks that signature
even when the resource is unused. `Use` invokes the typed function value. Aliases
resolve lexically, starting in the component/view dictionary and falling back to
application and theme declarations. Runtime template selection uses ordinary Sun
conditions and typed component functions; resources do not erase parameter types.

A `Resources` child of a `View`, `Component`, or `Template` defines local resources.
Definitions are constructed before the content, regardless of declaration order.
Each component instance gets an independent scope nested under its caller's scope;
leaving the component restores the caller's scope. Local definitions can depend on
the component's typed inputs. They cannot specify application/theme `scope`.

```xml
<View name="view" state="State" resources="{resources()}">
  <Resources>
    <Style name="heading" basedOn="base"><Normal fontSize="20" /></Style>
  </Resources>
  <Label text="Tasks" styleResource="heading" />
</View>
```

The view's `resources` expression returns `SystemResult<ui.Resources>` and supplies
application/theme definitions. Lookup searches the current component, enclosing
scopes, application, then theme. Use `styleResource`, `foregroundResource`,
`backgroundResource`, `fontResource`, `spacingResource`, and `marginResource` for
typed named lookups; whole-attribute expressions can compute resource names.
Ordinary direct appearance properties override resolved styles.

Handwritten views use the same `View.resources`, `push_resources`, `define`, and
typed lookup methods; restore `resource_scope` to the returned enclosing scope
after constructing a component. Lookup failures accumulate in
`View.construction_error`. Tree preflight rejects these and validates every
dictionary before changing mounted controls. Missing resources, type mismatches,
duplicate entries, and alias/inheritance cycles fail atomically.

Template names are resolved at generation time, so `Use` takes a literal name.
There is no reflection-based property system or erased runtime template lookup.

## Events, identity, and migration

`Session<State>` still owns application state. Events carry the action number,
integer application context (`payload`), mounted identity, and an owned tagged
`EventValue` (`None`, `Text`, `Boolean`, or `Selection`). Handle edits explicitly;
there is no automatic two-way binding. Programmatic updates suppress user-edit
notifications, and unchanged text is not written back to GTK. A rejected edit is
restored on the next render even if the desired application value did not change.

Sibling keys must be unique within a parent scope. `Tree.identity(i64)` remains a
root convenience; `identity_path(Vec<Key>)` addresses nested controls and
`View.focus_path` requests nested focus. Reordering preserves identity and focus;
changing kind/template or moving between scopes remounts. Unmounted identities
cannot dispatch stale events. Renderer failures clear resources and stop the tree.

For handwritten code, construct `ui.Element(ui.Key(...), ui.Kind...)`, set its
owned properties, and append it with `view.append(parent_index, element)`.
Parents precede children; `-1` denotes a root. `view.button(...)` remains available.
Renderer implementations now receive typed elements and parent-aware placement,
then `finish()` completes deferred placement before focus effects.

This is a bounded Sun-native toolkit, not a XAML compatibility implementation.
Native control-template replacement, editable multiline text, arbitrary selectors,
animations, runtime XML interpretation, and a scripting engine are not included.

## Media preview and flat panels

`Video` displays a local file with native playback and a draggable trim timeline.
Its `text` property is the local filename; `duration`, `trimStart`, `trimEnd`, and
`position` use integer microseconds. `changeAction` receives a `ui.ValueKind.Range`
event with `range_start`, `range_end`, and `position`. Keep the range in owned
application state and pass it back to the next view. An empty preview uses zero
for all times. Loaded previews require a positive, ordered range within duration.

```xml
<Video key="preview" text="{state.path.clone(a)}"
       duration="{state.duration}" trimStart="{state.start}" trimEnd="{state.end}"
       position="{state.position}" changeAction="15" expandX="true" />
```

Appearance resources and inline controls accept `borderWidth`, `radius`, and
`padding` in pixels. Values inherit when omitted; `borderWidth="0"` explicitly
removes the native border, shadow, and transition delay. Use background fills to
separate panels and retain a deliberate keyboard-focus appearance for controls.
See [sun_video_editor/src/sun_video_editor.view.xml](https://github.com/namo-robotics/sun_video_editor/blob/main/src/sun_video_editor.view.xml) for the complete example.

Image previews use `<Image text="{state.path.clone(a)}" height="360" expandX="true" />`.
The GTK adapter fits the local image without distorting its aspect ratio. Empty
paths clear the preview; unreadable images show an inline fallback message.

Set `trimControls="false"` on `Video` to use standard video playback controls,
without the custom trim timeline or its transport row. Trim controls default to enabled.
