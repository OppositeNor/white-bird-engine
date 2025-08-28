# 白鸟引擎

> 为音游打造的开源多平台游戏引擎
>
> Licensed under [Apache License Version 2.0](https://github.com/OppositeNor/white-bird-engine/blob/main/LICENSE).

[English](./README.md)

## 如何构建

将仓库克隆到本地

```sh
git clone https://github.com/OppositeNor/white-bird-engine.git --recursive
cd white-bird-engine
```

或者

```sh
git clone https://github.com/OppositeNor/white-bird-engine.git
cd white-bird-engine
git submodule init --update
```

### 根据平台构建

<details>
<summary> Linux </summary>

安装构建工具：

- Arch

    ```sh
    sudo pacman -S gcc cmake shaderc ninja
    ```

    TODO: install Vulkan SDK

- Debian

    ```sh
    TODO
    ```

构建：

```sh
cmake -B build -G Ninja
cmake --build build
```

</details>

<details>
<summary> MacOS </summary>

使用 Homebrew 安装构建工具

```sh
brew install cmake shaderc ninja
```

构建：

```sh
cmake -B build -G Ninja
cmake --build build
```

</details>

<summary> Windows </summary>
TODO
</details>
