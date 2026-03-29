# Asset standard

Assets should be stored in the `res` folder, and will be processed by the ASP during compile time.

Types of assets are:

- RUAudio

- RUBinary

- RUShader

- RUFont

- RUImage

- RUText

- RUVideo

- RUMaterial

- RUMesh

Then, index files should be written to index the files by the resource path. The standard of the
resource path is:

The relative path relative to the `res` folder, with `/` separating the directory scope.
