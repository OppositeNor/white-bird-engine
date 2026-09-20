# Asset Standard

Assets are described by manifest files in the `res` folder and processed by ACP during build time.

## Manifest Discovery

ACP only starts from root manifests under `res/res_chunks/`. It does not scan every JSON file under
`res`, because resource formats such as glTF are also JSON and must not be treated as manifests.

Root manifests may reference additional manifests with the `include` array. Include paths are resolved
relative to the resource root. ACP loads only those referenced manifest files from the resource tree,
then recursively follows their includes.

JSON files outside `res/res_chunks/` are processed only when they are reachable through `include` from
a root chunk manifest. Included files must be valid ACP manifests with a `resources` array.

## Resource Scope

The optional `scope` field accepts `global` or `test_only`. The default is `global`.
Global resources are compiled into both deployment and test outputs. Test-only resources are
compiled only when tests are enabled. ACP validates scope values, filters entries before
compilation, and omits `scope` from runtime index files.

Set `scope` on an individual resource entry, or on a whole manifest when all its resources
and includes are test-only. A test-only manifest is skipped for deployment. A global
manifest may include a test-only manifest; ACP removes that include from deployment output.
For a nested resource, keep it in the same scope as its containing resource.

Source manifests live under `res/`. ACP writes deployment resources under
`build/<target>/bin/res` and, for test builds, test resources under `test_env/res`.

## Manifest Entry Rules

- Every resource entry must contain a `type` field.
- Type-specific fields are allowed and preserved.
- For shader resources, use:
    - `src`: source shader file in `res`.
    - `dst`: compiled shader output path in runtime resources.
    - `metadata`: reflection metadata output path.
    - `macros`: optional compile-time Slang macro definitions. Each entry requires a non-empty `name` and may provide a string `value`.
- For direct-copy resources, use:
    - `file`: source file path in `res`.
- Missing files referenced by `file` are treated as build errors.

Shader macros are compile-only and are not emitted into the runtime resource manifest:

```json
{
    "id": "example_shader_variant",
    "source_type": "shader",
    "src": "assets/shaders/example.slang",
    "dst": "assets/shaders/example_variant.spv",
    "metadata": "assets/shaders/example_variant.json",
    "macros": [
        {"name": "WBE_MACRO_1"},
        {"name": "WBE_MACRO_2", "value": "1"}
    ]
}
```

## Material Resources

Material resources associate graphics pipelines with shader-defined texture slots:

```json
{
    "id": "example_material",
    "type": "material",
    "graphics_pipeline_ids": ["forward_cook_torrance_pbr"],
    "textures": [
        {"texture_role": "albedo", "texture_id": "example_albedo"},
        {"texture_role": "normal", "texture_id": "example_normal"},
        {"texture_role": "rma", "texture_id": "example_rma"}
    ]
}
```

Materials do not declare a `material_type`; their graphics pipeline and reflected shader metadata define the rendering contract.

- Each texture slot requires a non-empty `texture_role`. Shaders select roles through reflected `WBE_TEXTURE` attributes.
- The loader retains each exact texture role name together with its runtime hash; consumers use both and never treat a hash alone
  as identity. Resource units do not use closed role enums.
- A texture role may appear at most once in a material.
- Each slot must provide exactly one of `texture` for an inline image or `texture_id` for an existing image resource.
- Regular materials may omit slots; static-set compilation uses the corresponding shader-selected role from the configured default material.
- The default material must provide every texture role selected by each material shader used by the pass.
- `rma` is the canonical texture role and project abbreviation for the roughness, metallic, and ambient-occlusion channel pack.

Material texture descriptor-index fields are selected in `MaterialParams` with `WBE_TEXTURE`. An explicit string selects that
texture role. Omitting the argument selects the annotated field name:

```slang
[WBE_TEXTURE("albedo")]
int albedo_index; // Selects texture_role "albedo".

[WBE_TEXTURE]
int detail_index; // Selects texture_role "detail_index".
```

The static opaque pipeline reflects these attributes independently for each graphics pipeline, resolves each texture to its global
sampled-image descriptor-heap index, and writes that index at the reflected material-field offset. The metadata-only
`material_sampler` and `textures` members and runtime `materials` table remain fixed contracts in `utils/utils.slang`; applications
may configure only the base variable names for `material_textures` and `material_params`, plus the `default_material` resource key. Runtime shaders receive
the material parameter buffer and sampler through `WBE_DESCRIPTOR` push-data fields and access textures through the global heap.

`WBE_TEXTURE` and `WBE_VERTEX_INPUT` share the same open-role annotation resolver: both accept one optional string, default to the
annotated field name, retain exact names for diagnostics, and reject duplicate roles and distinct-name hash collisions. Their
contract policies remain separate: material textures validate the global descriptor/index-table shape, while vertex inputs
validate varying locations, scalar/vector formats, and resource byte layouts.

## Runtime Index Output

ACP generates runtime index files from manifests. Each manifest file produces a corresponding
index file in the output resource directory.

The engine runtime consumes generated index files using resource paths relative to `res`, with
`/` as path separator.
