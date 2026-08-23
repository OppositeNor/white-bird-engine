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

## Manifest Entry Rules

- Every resource entry must contain a `type` field.
- Type-specific fields are allowed and preserved.
- For shader resources, use:
    - `src`: source shader file in `res`.
    - `dst`: compiled shader output path in runtime resources.
    - `metadata`: reflection metadata output path.
- For direct-copy resources, use:
    - `file`: source file path in `res`.
- Missing files referenced by `file` are treated as build errors.

## Material Resources

Material resources identify their shading model independently from their texture slots. The currently supported material contract is:

```json
{
    "id": "example_material",
    "type": "material",
    "material_type": "cook_torrance",
    "graphics_pipeline_ids": ["forward_cook_torrance_pbr"],
    "textures": [
        {"texture_type": "albedo", "texture_id": "example_albedo"},
        {"texture_type": "normal", "texture_id": "example_normal"},
        {"texture_type": "rma", "texture_id": "example_rma"}
    ]
}
```

- `material_type` is required and currently accepts only `cook_torrance`.
- Each texture slot requires a `texture_type`: `albedo`, `normal`, or `rma`.
- The loader decodes both type names to runtime hash codes; resource units do not retain the strings or use closed enums.
- A texture type may appear at most once in a material.
- Each slot must provide exactly one of `texture` for an inline image or `texture_id` for an existing image resource.
- Regular materials may omit slots; bindless compilation uses the corresponding slot from the configured default material.
- The default material must provide all three Cook–Torrance texture types.

## Runtime Index Output

ACP generates runtime index files from manifests. Each manifest file produces a corresponding
index file in the output resource directory.

The engine runtime consumes generated index files using resource paths relative to `res`, with
`/` as path separator.
