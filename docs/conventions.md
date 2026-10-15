- The project is layered (upper to lower):

    - global

    - function

    - resource

    - core

    - platform

- Only upper layer depending lower layer is allowed. Lower layer depending on upper layer is
prohibited.

- Template use in the exposed API of the global layer is prohibited.

- `rma` is an accepted abbreviation for the roughness, metallic, and ambient-occlusion texture channel pack. Use `rma` for the
project-owned identifier and material texture role.

- Unleass you are doing testing, do not try-catch any exceptions. All exceptions in the project
are thrown as `std::runtime_exception`, and should cause a crash on the program.
