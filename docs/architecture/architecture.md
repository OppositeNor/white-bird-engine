# Engine Architecture

White Bird Engine uses a layered architecture, where each layer contains multiple modules, and the modules within
each layer can have dependencies on each other. The higher layer modules are allowed to depend on low layer
modules; however, low layer modules depending on high layer modules is strictly prohibited.

White Bird Engine consists the following layers, ordered from low to high:

- [Platform Layer](./platform.md)

Platform layer acts as an interface between the engine and the platform. This handles and provides abstractions
for OS specific features.

- [Core Layer](./core.md)


