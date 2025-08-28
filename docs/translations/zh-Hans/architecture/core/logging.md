# 日志 （Logging）

日志系统在白鸟中，主要承担着 Debug 测试，处理输出等任务。日志由多个日志对象来管理输入输出，并由日志
管理器（Logging Manager）对日志对象进行管理。

## 日志对象（Log）

日志可以被输出到各种不同的频道。频道可以使用默认提供的，也可以使用白鸟系统的频道系统来自定义频道（详
见[这里](./channel.md)）

需要注意的是，如果要使用日志系统，全局（Global）必须被初始化。

```cpp
namespace WBE = WhiteBirdEngine;

...

std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv);

...

WBE::LogStream log_stream(WBE::WBE_CHANNEL_USER, std::cout);
log_stream.message("Hello World!");
```

这里展示了一个较为常用的 `LogStream`：

```cpp
// 第一个而参数是这个日志对象所属的频道。
// 第二个对象是一个 std::ostream& 对象，作为日志输出的流。
WBE::LogStream log_stream(WBE::WBE_CHANNEL_USER, std::cout);
```

## 日志管理器（Logging Manager）

日志管理器用于管理不同频道中的不同日志对象。可以在程序尝试获取日志对象的时候自动对

TODO

## 简单接口

白鸟引擎也实现了一些简单的接口，来简单快速的进行日志输出：

```cpp
WBE::wbe_console_log()->message("...");                         // 简单快速在控制台中进行输出
WBE::wbe_console_log(WBE::WBE_CHANNEL_USER)->warning("...");    // 也可以在参数部分写频道 ID
```

