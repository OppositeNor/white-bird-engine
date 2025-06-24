# 内存（Memory）

提供一些智能引用类型。每个引用类型中存在一个原子计数器。一个资源在所有引用都被释放的时候，对应分配的资源也会
被释放。

## 引用

白鸟引擎中的引用与 C++ STL 中的 `shared_ptr` 类似，可以对一个分配的资源进行引用，当没有对象再引用这个资源的
时候，资源会被自动释放。

```cpp
namespace WBE = WhiteBirdEngine;

...

WBE::HeapAllocatorPool allocator(1024);

struct A {
    short val1;
    int val2;
    A(short p_val1, int p_val2)
        : val1(p_val1), val2(p_val2) {}
};
struct B {
    float val3;
    double val4;
    A(float p_val3, double p_val4)
        : val3(p_val3), val4(p_val4) {}
};

// 可以先创建一个不引用任何资源的引用对象
WBE::Ref<A> ref_a_alt(&allocator);

{
    // 创建了一个 A 资源，并创建一个对象 ref_a 来引用这个资源。
    WBE::Ref<A> ref_a = WBE::Ref<A>::make_ref(&allocator, 42, -10);
    // 创建了一个 B 资源，并创建一个对象 ref_b 来引用这个资源。
    WBE::Ref<B> ref_b = WBE::Ref<B>::make_ref(&allocator, 3.14f, 1.5);

    // 可以获取引用的值
    WBE::wbe_console_log()->message(std::format("ref_a: {} {}", ref_a->val1, (*ref_a).val2)); // ref_a: 42 -10
    WBE::wbe_console_log()->message(std::format("ref_b: {} {}", ref_b->val3, (*ref_b).val4)); // ref_b: 3.14 1.5

    // 可以对资源的值进行更改
    ref_a->val1 = 5;
    (*ref_b).val4 = 2.414;
    WBE::wbe_console_log()->message(std::format("ref_a: {} {}", ref_a->val1, (*ref_a).val2)); // ref_a: 5 -10
    WBE::wbe_console_log()->message(std::format("ref_b: {} {}", ref_b->val3, (*ref_b).val4)); // ref_b: 3.14 2.414

    // 让另一个引用对象来引用同一个资源
    ref_a_alt = ref_a;

    // 任何一个引用对象都可以对资源进行更改
    ref_a_alt->val1 = 10;
    WBE::wbe_console_log()->message(std::format("ref_a: {} {}", ref_a->val1, (*ref_a).val2)); // ref_a: 10 -10
}

// 退出上下文，ref_a 和 ref_b 被析构，其中 ref_b 引用的资源没有任何对象再对它进行引用，ref_b 引用的资源被释放
// 而 ref_a 引用的资源由于还有 ref_a_alt 还在引用，所以这个资源没有被释放。
WBE::wbe_console_log()->message(std::format("ref_a: {} {}", ref_a_alt->val1, (*ref_a_alt).val2)); // ref_a: 10 -10

```

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
