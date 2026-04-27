# Asset Standard

Assets are described by manifest files in the `res` folder and processed by ACP during build time.

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
