# C++ Reflection System and Extension Guide

## System boundary and flow

White Bird Engine's C++ reflection is a build-time pipeline:

1. `build_config.py` discovers project headers and sources while excluding dependencies, generated output, caches, Git metadata, and build directories.
2. `build_script/on_build.py` constructs `WBEMetaparser` and `WBEReflector`.
3. `build_script/reflection/metaparser.py` runs Clang with detailed processing records and reads annotation records produced by `WBE_META(...)`.
4. It records classes/structs, base specifiers, annotated fields, aliases, labels, type spellings, source headers, and annotation strings in the models from `metadata_types.py`.
5. `WBEReflector` sorts/checks metadata and renders entries from repository `generate.json` files through Jinja templates.
6. Generated headers live in `include/per_target/<target>/generated/`; generated sources live in `src/per_target/<target>/generated/` and are compiled by the normal build.

`include/core/reflection/reflection_defs.hh` defines the crucial boundary: under `WBE_REFLECTION_PARSER`, `WBE_META` becomes a Clang `annotate` attribute; during normal compilation it is empty. Do not write runtime logic that depends on inspecting `WBE_META` itself.

The reflection metadata cache is content-hash validated. The generator writes an output only when its rendered content changes.

## Annotation contracts

### Classes and structs

- `WBE_SERIALIZABLE`: generated static serializer/accessor, generated virtual JSON methods, and dynamic factory registration. The type derives from `Serializable` and contains `WBE_DECL_SERIALIZABLE(Type)`.
- `WBE_SERIALIZABLE_STATIC`: generated static serializer/accessor without virtual methods or dynamic factory registration.
- `WBE_COMPONENT`: included in the static-serializable category.
- `WBE_CONFIG_OPTION`: included in the serializable category.
- `WBE_CLA_CONFIGURATION`: feeds command-line configuration generation.
- Other annotated classes are still available to consumers that iterate all class metadata, notably `TypeUUID` generation.

The category lists are defined in `build_config.py`, not in the metaparser. Before adding a new category, decide which existing generator sets it should join.

### Fields

Only `FIELD_DECL`s containing `WBE_REFLECT` are recorded as reflected fields:

```cpp
WBE_META(WBE_REFLECT)
std::string name;
```

Add `WBE_REFLECT_REQUIRED` when absence must fail deserialization. Optional reflected fields preserve their current/default value when a key is absent, except `std::optional<T>` fields are reset.

### Aliases

Use the exact supported parameter form:

```cpp
WBE_META(WBE_SERIALIZABLE, WBE_ALIASES = [ OldTypeName, OlderTypeName ])
WBE_META(WBE_REFLECT, WBE_ALIASES = [ old_field_name ])
```

Field deserialization checks the canonical key before aliases. Class aliases enter the serializable factory and type-name registry where applicable. Factory class names and aliases must be globally unique.

The parser accepts a narrow comma-separated bracket list. Do not put quoted strings, expressions, or commas inside an alias token unless parser support is deliberately extended and tested.

### Labels

`WBE_LABEL` produces a compile-time hash and, under the reflection parser, an annotated variable declaration. `build_config.py` assigns annotation categories to static and/or dynamic label registries. `WBEReflector.checks()` rejects hash collisions between distinct label names.

## Generated outputs and runtime APIs

### `serializables_sd.gen.hh`

Generates `SerializableSD<Type>` specializations for every static-serializable category and a recursive `SerializableSD<std::vector<T>>` specialization.

- Serialization visits recorded base classes, then reflected fields.
- Deserialization visits recorded base classes, resolves canonical/alias keys, enforces required fields, and deserializes present values.
- `std::optional<T>` is emitted only when engaged; absence resets it during deserialization.

The generic `SerializableSD<T>` delegates to parser-data `set/get`. `SerializableSD<std::optional<T>>` lives in `reflection_defs.hh`.

### `serializable_impl.gen.cpp`

For dynamic `WBE_SERIALIZABLE` categories, defines the virtual JSON `serialize` and `deserialize` methods declared by `WBE_DECL_SERIALIZABLE`. Each definition delegates to `SerializableSD<Type>`.

### `serializables_access.gen.hh`

Generates `SerializableAccess<Type>` for static-serializable categories:

- `list_fields()` returns directly reflected field names;
- `get_field<T>(object, name)` returns a const or mutable reference;
- `get_pointer_to_member<T>(name)` returns an exact typed member pointer.

Lookup is name-based and the requested `T` must exactly match Clang's recorded field type. Unknown names and type mismatches throw `std::runtime_error`. The accessor lists the type's direct reflected fields; do not assume it flattens inherited fields.

### `serializable_factory.gen.hh`

Registers dynamic serializable class names and aliases. It can construct:

- `Unique<Serializable>` using the global allocator;
- `Ref<Serializable>` using the global allocator;
- an in-place `Serializable*` in caller-provided storage.

The generated branch requires the concrete type to convert to `Serializable*` and its `SerializableSD` to accept the parser-data type. The factory singleton must be initialized. In-place callers own storage, alignment, and destruction.

### `type_uuid.gen.hh`

Every parsed annotated class receives `TypeUUID<Type>::UUID`, computed from its canonical class name. `TypeUUIDManager` maps canonical names and aliases to IDs and IDs back to canonical names. Unknown values throw.

### Other consumers

- `label_manager.gen.hh`: static/dynamic label registration.
- `cla_configuration.gen.hh`: command-line configuration adapters.
- `serializable_factory.gen.hh`: dynamic serializable creation.

Generation entries are declared in `include/per_target/generate.json` and `src/generated/generate.json`. A generation entry supplies `out_dir`, `output_name`, `template`, and `data`. `${...}` expands build parameters; dot-prefixed data paths resolve reflection/config metadata.

## Currently exercised serialization types

The generic parser plus generated recursion currently has coverage for:

- signed and unsigned 32-bit and 64-bit integers;
- `float`, `double`, `bool`, and `std::string`;
- `std::optional<std::string>` and the generic optional mechanism;
- `glm::vec2`, `glm::vec3`, `glm::vec4`, and `glm::quat`;
- engine `Buffer<N>` values;
- nested reflected types;
- `std::vector<int>`, `std::vector<std::string>`, and vectors of reflected types;
- single, multiple, multilevel, and virtual-diamond inheritance;
- required fields and aliases.

Support depends on both `SerializableSD<T>` and the selected parser data implementation. JSON and YAML have parallel but separate `set/get` branches. Do not claim a type is generally supported merely because one parser accepts it.

A nested type is not serializable merely because one of its fields uses `WBE_REFLECT`. The nested type must itself belong to a generated serializable category or provide a visible hand-written `SerializableSD<NestedType>` specialization; the same requirement applies to `std::vector<NestedType>` elements.

Vectors and optionals construct temporary elements with `T{}` during deserialization, so element types must be default-constructible. Vector serialization translates each element through `SerializableSD<T>` rather than relying on native parser support for arbitrary `std::vector<T>`.

## Adding a reflected or serializable type

1. Include `core/reflection/reflection_defs.hh`; also include `core/reflection/serializable.hh` for dynamic serializable types.
2. Select the narrowest existing class category that provides the required generated consumers.
3. For `WBE_SERIALIZABLE`, derive from `Serializable` and add `WBE_DECL_SERIALIZABLE(Type)` inside the class.
4. Annotate only fields intended for generated APIs with `WBE_META(WBE_REFLECT)`.
5. Add required/alias annotations only when the data contract needs them.
6. Verify every reflected field and base type can be named from generated-header include context and can be serialized by each required parser format.
7. Build with `python build.py -t debug`; do not invoke CMake directly and do not edit generated files.
8. Add JSON/YAML tests for data-contract behavior, accessor tests for name/type access, and factory tests only when dynamic construction is required.

Use `tests/unit/core/reflection_test/reflection_test_data.hh` as the canonical declaration examples. Relevant consumers are tested in `accessor_test.hh`, `factory_test.hh`, `serializer_test.hh`, `deserializer_test_json.hh`, and `deserializer_test_yaml.hh`.

## Extending supported field or container types

Choose one of two extension levels:

- Add a hand-written `SerializableSD<NewType>` specialization when the type can be translated through the existing parser-data API. Place it in a non-generated header that is visible before generated specializations use it.
- Extend `ParserJSON` and/or `ParserYAML` `set/get` branches when the parser-data representation itself must understand the type.

For a reusable container, follow the generated `std::vector<T>` pattern only if recursive element conversion and default construction match the desired contract. Avoid teaching templates about a one-off application type.

Test round trips, missing values, malformed/type-mismatched data, nested usage, and both parser formats that the feature claims to support.

## Extending extraction or generation

### New metadata or language construct

`build_script/reflection/metadata_types.py` already models methods, but the current metaparser extracts base classes and annotated fields only. Method reflection therefore requires AST traversal and metadata population before any Jinja template can use it.

When adding metadata:

1. extend the metadata model;
2. populate it in `metaparser.py` from verified Clang cursor shapes;
3. preserve cache serialization/deserialization;
4. add parser tests or a representative annotated fixture;
5. update only consumers that require the new metadata.

### New annotation category

Annotation strings are stored generically, while category membership is configured in `build_config.py`. Add the category to the appropriate generator set and update template conditions. Extend metaparser logic only when the annotation requires structured parameters or different AST extraction.

### New generated consumer

Add a focused Jinja template and a `generate.json` entry. Reuse the existing metadata rather than reparsing source. Keep generated includes sufficient for every emitted type spelling, use target-specific output paths, and let `code_gen.py` manage changed-file writes.

## Pitfalls and invariants

- Do not confuse this system with Slang shader reflection.
- Do not edit generated `*.gen.*` artifacts.
- Unannotated fields are intentionally invisible.
- `WBE_META` carries no runtime information in normal compilation.
- Clang's recorded `field.type.spelling` is emitted into accessor templates; it must compile in generated-header context.
- Required-field errors occur during deserialization and are `std::runtime_error`.
- The engine does not catch reflection/parser exceptions outside tests.
- Inheritance generation invokes `SerializableSD<Base>` for every recorded base; each base must have a compatible specialization.
- Alias parsing is intentionally narrow.
- Factory aliases and canonical names must be unique.
- Label hashes must not collide.
- Keep all engine and test-support types in `WhiteBirdEngine`; do not introduce helper namespaces.