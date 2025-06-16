# 白鸟引擎

一个专门为音游打造的多平台游戏引擎

## 如何构建

### Linux

安装构建工具：

#### Arch

```bash
sudo pacman -S gcc cmake shaderc ninja
```

安装 Vulkan 驱动

NVIDIA:

```bash
sudo pacman -S nvidia-utils
```

AMD:

```bash
# TODO
```

---

构建：

```bash
cmake -B build -G Ninja
cmake --build build
```
