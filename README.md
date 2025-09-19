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
git submodule init --update --recursive
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
