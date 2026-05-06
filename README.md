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
git submodule update --init --recursive
```

Install build dependencies:

```sh
sudo pacman -S cmake clang ninja
```

It is suggested to use Anaconda for setting up the build environment.

You can install Anaconda from [here](https://github.com/conda-forge/miniforge).

### Setting up build environment

First create a conda environment.

```sh
conda env create -n white-bird-engine python=3.13
conda activate white-bird-engine
```

Install the dependencies from `requirements.txt`

```sh
pip install -r requirements.txt
```

Run the python script to build the project.

### Build

```sh
python ./build.py
```

You can also indicate the build target.

```sh
python ./build.py -t debug          # Debug target
python ./build.py -t release        # Release target
python ./build.py -t deploy         # Deploy target (default)
python ./build.py -t debug-gcc      # Debug target with GCC compiler
python ./build.py -t release-gcc    # Release target with GCC compiler
python ./build.py -t deploy         # Deploy target with GCC compiler
```

## Contribution

### Use of AI

Use of AI assistants is allowed and encouraged for this project. At the start of every new AI session, please have the
agent read [CONVENTIONS_FOR_AI.md](./CONVENTIONS_FOR_AI.md) first. That file is a compact, high-density reference written
specifically for AI agents and covers the project's layered architecture, build workflow, test layout, memory model, and
naming rules.

After the AI finishes editing, **carefully review** every change and make sure you understand exactly what it did before
committing.
