# 内存（Memory）

提供一些智能引用类型。每个引用类型中存在一个原子计数器。一个资源在所有引用都被释放的时候，对应分配的资源也会
被释放。

## 引用

TODO

## 弱引用

TODO

## 注意

使用引用并不代表完全的内存安全！在出现**循环引用**（Cyclic dependency）的时候，依然会出现内存泄漏问题。例如：

```cpp
struct A {
    Ref<B> ref_b;
};

struct B {
    Ref<A> ref_a;
};

auto a = Ref<A>::make_ref(&allocator);
auto b = Ref<B>::make_ref(&allocator);
a.ref_b = b;
b.ref_a = a;
```

部分 GC 系统会自动识别循环引用（例如 Java 等）但是白鸟引擎的引用系统基于原子计数，并不具备这样的功能。因此上
述情况发生的时候，即使 `a` 和 `b` 都被释放，它们分配的资源依然不会被释放。所以使用的时候需要格外小心。
