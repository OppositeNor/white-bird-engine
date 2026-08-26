---
name: white-bird-cpp-reflection
description: Use and extend White Bird Engine's build-time C++ reflection and code-generation system. Apply when annotating C++ types or fields, adding serialization, generated field access, factories, aliases, required fields, labels, type UUIDs, or new reflection consumers. Do not use for Slang shader reflection.
---

# White Bird C++ Reflection

This system is annotation-driven code generation, not runtime language reflection. The reflection build parses `WBE_META(...)` annotations with Clang and generates target-specific C++ APIs. During normal compilation, `WBE_META(...)` expands to nothing.

## Choose the type category

- Use `WBE_META(WBE_SERIALIZABLE)` when the type needs generated static serialization/access plus virtual JSON serialization and name-based `SerializableFactory` construction. Derive from `Serializable` and add `WBE_DECL_SERIALIZABLE(Type)`.
- Use `WBE_META(WBE_SERIALIZABLE_STATIC)` when the type needs generated `SerializableSD<Type>` and `SerializableAccess<Type>` but must not derive from `Serializable` or enter the dynamic factory.
- Existing categories such as `WBE_COMPONENT` and `WBE_CONFIG_OPTION` participate according to `build_config.py`; preserve their established pattern instead of adding a second annotation.
- Use bare `WBE_META(...)` on a class only when a specific generated consumer, such as `TypeUUID`, needs the class metadata and no serialization category applies.

## Declare a serializable type

```cpp
#include "core/reflection/reflection_defs.hh"
#include "core/reflection/serializable.hh"

namespace WhiteBirdEngine {

struct WBE_META(WBE_SERIALIZABLE, WBE_ALIASES = [ LegacySettings ]) Settings final : public Serializable {
    WBE_DECL_SERIALIZABLE(Settings)

    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string name;

    WBE_META(WBE_REFLECT, WBE_ALIASES = [ old_quality ])
    int32_t quality = 0;
};

} // namespace WhiteBirdEngine
```

Only fields carrying `WBE_META(WBE_REFLECT)` enter generated serializers and accessors. `WBE_REFLECT_REQUIRED` makes deserialization throw when neither the canonical key nor an alias is present. Keep alias syntax exactly `WBE_ALIASES = [ NameA, NameB ]`.

Before selecting a field type, verify that `SerializableSD<T>` or the chosen parser supports it. Optional and vector deserialization default-construct their element type.

## Use the generated APIs

After rebuilding, include the specific generated header needed by the caller:

```cpp
#include "generated/serializable_factory.gen.hh"
#include "generated/serializables_access.gen.hh"
#include "generated/serializables_sd.gen.hh"
```

Static serialization works with JSON or YAML parser data:

```cpp
Settings settings;
SerializableSD<Settings>::deserialize(parser.get_data(), settings);

JSONData output;
SerializableSD<Settings>::serialize(output, settings);
```

For `WBE_SERIALIZABLE` types, `settings.serialize(output)` and `settings.deserialize(data)` use generated virtual definitions. `SerializableFactory` creates such types by canonical class name or class alias. `SerializableAccess<Settings>` lists reflected fields and performs exact typed name lookup; unknown names and mismatched template types throw.

## Regenerate and validate

Never edit `*.gen.*`. Rebuild through the repository wrapper so the metaparser and generators run:

```sh
python build.py -t debug
```

For changes to annotations, serialization, accessors, or factories, validate with:

```sh
python validation.py -t debug -- --gtest_filter='WBEDeserializer*:*WBESerializableFactoryTest*'
```

The wrapper runs the full unit suite after the focused tests pass. Do not run lint automatically.

## Advanced work

Read [references/system-and-extension.md](references/system-and-extension.md) before changing parser extraction, annotation categories, supported field/container types, inheritance behavior, templates, `generate.json`, labels, type UUIDs, factories, or other generated consumers. Keep changes layer-correct and update tests at the parser, generated API, and parser-format boundaries that actually change.
