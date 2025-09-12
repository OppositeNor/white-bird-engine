# 白鸟引擎

> 为音游开发而构建的免费游戏引擎。  
>
> 许可证：[Apache License Version 2.0](https://github.com/OppositeNor/white-bird-engine/blob/main/LICENSE)。

- [English](./README.md)

## 如何构建

克隆仓库：

```sh
git clone https://github.com/OppositeNor/white-bird-engine.git --recursive
cd white-bird-engine
````

或者：

```sh
git clone https://github.com/OppositeNor/white-bird-engine.git
cd white-bird-engine
git submodule init --update
```

建议使用 Anaconda 来搭建构建环境。

你可以从 [这里](https://github.com/conda-forge/miniforge) 安装 Anaconda。

### 设置构建环境

首先创建一个 conda 环境。

```sh
conda env create -n white-bird-engine python=3.13
conda activate white-bird-engine
```

从 `requirements.txt` 安装依赖：

```sh
pip install -r requirements.txt
```

运行 Python 脚本来构建项目。

### 构建

```sh
python ./build.py
```

你也可以指定构建目标：

```sh
python ./build.py -t debug          # 调试目标
python ./build.py -t release        # 发布目标
python ./build.py -t deploy         # 部署目标（默认）
python ./build.py -t debug-gcc      # 使用 GCC 编译器的调试目标
python ./build.py -t release-gcc    # 使用 GCC 编译器的发布目标
python ./build.py -t deploy         # 使用 GCC 编译器的部署目标
```
