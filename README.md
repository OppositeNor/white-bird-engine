# White Bird Engine

> A free game engine built for rhythm game development.
>
> Licensed under [Apache License Version 2.0](https://github.com/OppositeNor/white-bird-engine/blob/main/LICENSE).

- [简体中文](./README.zh-Hans.md)

## How to build

Clone the repository:

```sh
git clone https://github.com/OppositeNor/white-bird-engine.git --recursive
cd white-bird-engine
```

Or:

```sh
git clone https://github.com/OppositeNor/white-bird-engine.git
cd white-bird-engine
git submodule init --update
```

### Build according to your system

<details>
<summary> Linux </summary>

Install build dependencies:

- Arch

    ```sh
    sudo pacman -S gcc cmake shaderc ninja
    ```

    TODO: install Vulkan SDK

- Debian

    TODO

Build:

```sh
cmake -B build -G Ninja
cmake --build build
```

</details>

<details>
<summary> MacOS </summary>

Install build dependencies with homebrew:

```sh
brew install cmake shaderc ninja
```

Build:

```sh
cmake -B build -G Ninja
cmake --build build
```

</details>

<details>

<summary> Windows </summary>
TODO
</details>

