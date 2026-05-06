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

## 贡献

### AI 的使用

本项目允许并鼓励使用 AI 助手参与开发。每次开启新的 AI 会话时，请先让 AI 通读
[CONVENTIONS_FOR_AI.md](./CONVENTIONS_FOR_AI.md)。该文件是专门面向 AI 代理编写的精简高密度参考资料，
涵盖了本项目的分层架构、构建流程、测试目录结构、内存模型以及命名规范等关键约定。

AI 完成编辑后，请**务必仔细审阅**每一处改动，确认你完全理解它所做的事情，然后再提交。
