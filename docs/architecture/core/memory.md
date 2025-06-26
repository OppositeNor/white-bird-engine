# 内存（Memory）

内存安全问题，由于其难定位，难修复，遇到的时候总是一个程序员感到头大。现在有许多方法和工具来避免这类问题。
白鸟引擎使用了**引用计数（Reference Counting, RC）**方法，通过计数有多少个对象在引用一个数据块，然后当引用
归零时，自动将引用的数据进行释放。

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

在部分场景中，我们或许只是想要获取一个数据，而在资源需要被释放的时候，依然能够被释放。例如在观察者模式中，对于
观察者我们希望它在自己该释放的时候释放，并且对于注册的对象来说当观察者被释放的时候，也可以自动注销对应的观察者。

如果我们直接对其引用的话，由于引用的存在，我们无法在不手动注销，或是释放被观察对象的前提下释放对应的资源（因为
被观察对象总是持有一个引用）

```cpp
class Observer {
public:
    Observer() {
        ...
    }
    ~Observer() {
        ...
    }

    void on_event() {
        ...
    }
};

class ObservedObject {
public:
    ObservedObject() {...}
    ~ObservedObject() {...}

    void register_observer(WBE::Ref<Observer> p_observer) {
        observer = p_observer;
    }

    void unregister_observer() {
        observer = nullptr;
    }

    void broadcast() {
        if (observer != nullptr) {
            observer->on_event();
        }
    }

private:
    WBE::Ref<Observer> observer;
};

...

ObservedObject observed_object;

{
    WBE::Ref<Observer> observer = WBE::Ref<Observer>::make_ref(&allocator);
    observed_object.register_observer(observer);
}

// 这里退出上下文之后，虽然 observer 已经被释放，但是 observed_object 依然持有一个 observer 的引用！
// 因此 observer 引用的数据依然没有被释放！

```

这种时候，可以使用**弱引用（Weak reference）**来解决。

```cpp
WBE::Ref<Observer> observer;
WBE::WeakRef<Observer> observer_weak = observer;
```

弱引用可以引用到指向的数据，但是不会参与到引用计数。

弱引用可以获取到当前资源的状态（是否可用？）

```cpp
struct MyData {
    ...
};

WBE::WeakRef<MyData> my_data_weak;
WBE::wbe_console_log()->message(std::format("{}", my_data_weak.is_valid())); // false
{
    WBE::Ref<MyData> my_data;
    my_data_weak = my_data;
    WBE::wbe_console_log()->message(std::format("{}", my_data_weak.is_valid())); // true
}
// my_data 被释放，my_data_weak 资源状态变为 invalid。
WBE::wbe_console_log()->message(std::format("{}", my_data_weak.is_valid())); // false
```

在我们使用数据的时候，我们需要调用 `lock` 函数对数据进行**锁定**，防止我们在使用过程中数据被释放。

`lock` 会返回一个对数据的强引用，从而保证在使用过程中，引用计数不会归零：

```cpp
// 我们可以先判断是否 valid ，然后再进行锁定
if (my_data_weak.is_valid()) {
    WBE::Ref<MyData> data_lock = my_data_weak.lock();
    ...
}

// 也可以先获取，然后判断是否为 nullptr
WBE::Ref<MyData> data_lock = my_data_weak.lock();
if (data_lock != nullptr) {
    ...
}
```

使用弱引用，我们就可以把前面的 `Observer` 改成：

```cpp
class Observer {
public:
    Observer() {
        ...
    }
    ~Observer() {
        ...
    }

    void on_event() {
        ...
    }
};

class ObservedObject {
public:
    ObservedObject() {...}
    ~ObservedObject() {...}

    void register_observer(WBE::Ref<Observer> p_observer) {
        observer = p_observer;
    }

    void unregister_observer() {
        observer = nullptr;
    }

    void broadcast() {
        // 发现观察者无法使用，自动将其注销。
        if (!observer.is_valid()) {
            observer = nullptr;
            return;
        }
        observer.lock()->on_event();
    }

private:
    WBE::WeakRef<Observer> observer; // 把这里替换为 WeakRef
};

...

ObservedObject observed_object;

{
    WBE::Ref<Observer> observer = WBE::Ref<Observer>::make_ref(&allocator);
    observed_object.register_observer(observer);
}

observed_object.broadcast();

// 这里退出上下文之后，observer 被释放，引用归 0，资源得到释放
// 同时 observed_object 发现引用无法使用，观察者就被自动注销。

```

弱引用还可以避免一些内存安全问题，详见下面：

## 需要注意的内存泄漏问题

使用引用并不代表完全的内存安全！例如：

```cpp
struct A {
    Ref<B> ref_b;
};

struct B {
    Ref<A> ref_a;
};

{
    auto a = Ref<A>::make_ref(&allocator);
    auto b = Ref<B>::make_ref(&allocator);
    a.ref_b = b;
    b.ref_a = a;
}
// 注意！这里 a 和 b 的数据都没有被释放！产生了内存泄漏！
```

上述情况中，由于 `a` 对应的数据引用了 `b`，而 `b` 的数据引用了 `a`，即使 `a` 和 `b` 两个引用都被释放，它们的
数据之间还有引用，因此这两个数据依然不会得到释放。这个问题也叫做**循环引用**（Cyclic dependency）。

部分比较高级的垃圾回收（Garbage Collection, GC）系统会自动识别循环引用（例如 Java 等）但是白鸟引擎的引用系统
基于原子计数，并不具备这样的功能。因此上述情况发生的时候，即使 `a` 和 `b` 都被释放，它们分配的资源依然不会被
释放。所以使用的时候需要小心。

解决这个问题的方法就是可以在其中一个数据中使用弱引用：

```cpp
struct A {
    Ref<B> ref_b;
};

struct B {
    WeakRef<A> ref_a;
};

{
    auto a = Ref<A>::make_ref(&allocator);
    auto b = Ref<B>::make_ref(&allocator);
    a.ref_b = b;
    b.ref_a = a;
}
```

这里虽然 `b` 也引用了 `a`，但是由于是弱引用，在 `a` 被释放的时候，`ref_a` 引用的数据也会一起被释放。这样就
可以避免内存泄漏。

