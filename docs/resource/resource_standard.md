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

## Runtime Index Output

ACP generates runtime index files from manifests. Each manifest file produces a corresponding
index file in the output resource directory.

The engine runtime consumes generated index files using resource paths relative to `res`, with
`/` as path separator.
