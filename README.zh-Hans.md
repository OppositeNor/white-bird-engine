# 白鸟引擎

Licensed under [Apache License Version 2.0](https://github.com/OppositeNor/white-bird-engine/blob/main/LICENSE).

[English](./README.md)

一个为音游打造的多平台游戏引擎

## 如何构建

首先将仓库克隆到本地

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

### Linux

安装构建工具：

#### Arch

```sh
sudo pacman -S gcc cmake shaderc ninja
```

构建：

```sh
cmake -B build -G Ninja
cmake --build build
```

### MacOS

使用 Homebrew 安装构建工具

```sh
brew install cmake shaderc ninja
```

构建：

```sh
cmake -B build -G Ninja
cmake --build build
```

### Windows

TODO

