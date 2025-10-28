# White Bird Engine

> 一个为节奏游戏开发而构建的免费/自由游戏引擎。
>
> 基于 [Apache License Version 2.0](https://github.com/OppositeNor/white-bird-engine/blob/main/LICENSE) 许可发布。

* [English](./README.md)

## 如何构建

克隆仓库：

```sh
git clone https://github.com/OppositeNor/white-bird-engine.git --recursive
cd white-bird-engine
```

或者：

```sh
git clone https://github.com/OppositeNor/white-bird-engine.git
cd white-bird-engine
git submodule update --init --recursive
```

安装构建依赖：

```sh
sudo pacman -S cmake clang ninja directx-shader-compiler
```

建议使用 Anaconda 来配置构建环境。

你可以在 [这里](https://github.com/conda-forge/miniforge) 安装 Anaconda。

### 配置构建环境

首先创建一个 conda 环境：

```sh
conda env create -n white-bird-engine python=3.13
conda activate white-bird-engine
```

从 `requirements.txt` 安装依赖：

```sh
pip install -r requirements.txt
```

然后运行 Python 脚本来构建项目。

### 构建

```sh
python ./build.py
```

你也可以指定构建目标：

```sh
python ./build.py -t debug          # Debug 构建
python ./build.py -t release        # Release 构建
python ./build.py -t deploy         # 部署构建（默认）
python ./build.py -t debug-gcc      # 使用 GCC 的 Debug 构建
python ./build.py -t release-gcc    # 使用 GCC 的 Release 构建
python ./build.py -t deploy         # 使用 GCC 的部署构建
```
