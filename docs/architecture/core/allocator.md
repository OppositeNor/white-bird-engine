# 内存分配器（Allocator）

> [!NOTE] 
>
> 通常内存分配器是在有特殊需求的时候才会使用。如果你只是想简单分配一个对象，更建议使用
> [内存](./memory.md)模块。

白鸟引擎使用自己实现的内存分配器。游戏中的动态内存分配，应当尽量依赖于此系统进行。

引擎在分配内存后，可能会对数据对应的内存位置做出调整，以保证内存连续性或者减少内存碎片。这也
意味着使用分配器的时候，直接使用内存指针可能会导致访问到错误的数据位置。为了避免这种情况发
生，每次在内存分配结束后，分配器会返回内存数据对应在分配器中的 ID。这个 ID 是仅在**这个分配
器**中唯一的。在想要获取数据的时候，可以通过此 ID 以及对应的分配器来找到准确的数据位置。

白鸟引擎提供栈方式分配内存，同时也提供堆方式分配内存。

## 栈分配器

### 分配/释放内存

栈分配器在释放的时候，只能按照分配顺序相反的顺序释放。例如分配的顺序为 `a b c`，那么释放的顺序
就必须为 `c b a`。

```cpp
StackAllocator allocator(1024);
struct MyData {
    short val1;
    int val2;
    float val3;
    double val4;
};
MemID mem_id = allocator.allocate(sizeof(MyData));
allocator.get_obj<MyData>(mem_id)->val1 = 5;
wbe_console_log()->message(std::format("val1: {}", allocator.get_obj<MyData>(mem_id)->val1)); // val1: 5
```

释放内存时，可以直接使用 `pop_stack` 方法，在栈顶的元素会被自动释放：

```cpp
allocator.pop_stack();
```

需要注意的是，`allocate` 方法仅会分配内存，但不会调用对象的构造函数。因此在 `allocate` 之后分
配的值均为未初始化值。同理，`pop_stack` 的作用也仅是释放内存，不会调用其析构函数。

`StackAllocator` 的构造函数可以传入两个参数：

```cpp
// 第一个参数为分配器的总大小（单位：字节）。为可选参数。默认值为 1024。
// 第二个参数为对齐大小（单位：字节）。为可选参数。默认值为 alignof(std::max_align_t)。
StackAllocator allocator(1024, 8);
```

## 堆分配器

### 分配/释放内存

```cpp
HeapAllocatorPool pool_allocator(1024);
struct Data {
    short val1;
    int val2;
    float val3;
    double val4;
};
MemID mem_id = pool_allocator.allocate(sizeof(Data));
pool_allocator.get_obj<Data>(mem_id)->val1 = 5;
wbe_console_log()->message(std::format("val1: {}", pool_allocator.get_obj<Data>(mem_id)->val1)); // val1: 5
```

释放内存时，可以直接调用 `deallocate` 方法：

```cpp
pool_allocator.deallocate(mem_id);
```

如果内存分配器在析构的时候，里面的对象没有被完全释放，内存分配器会打印出一个警告，并释放剩余
内存。通常这种时候都会有潜在的内存泄漏问题发生。

需要注意的是，`allocate` 方法仅会分配内存，但不会调用对象的构造函数。因此在 `allocate` 之后分
配的值均为未初始化值。同理，`deallocate` 的作用也仅是释放内存，不会调用其析构函数。

如果想要在分配/释放的同时调用构造/析构函数，可以使用 `create_obj` 和 `destroy_obj` 来分配/销
毁。

```cpp
class MyClass {
public:
    MyClass(int p_val)
        : val(p_val) {
        wbe_console_log()->message(std::format("Constructed! val: {}", val));
    }
    ~MyClass() {
        wbe_console_log()->message("Destructed! val: {}", val);
    }

    void print_val() {
        wbe_console_log()->message("val: {}", val);
    }

private:
    int val;
};

...

HeapAllocatorPool pool_allocator(1024);
MemID mem_id = create_obj(pool_allocator, 3); // Constructed! val: 3
pool_allocator.get_obj<MyClass>(mem_id)->print_val(); // val: 3
destroy_obj(pool_allocator, mem_id); // Destructed! val: 3
```

`create_obj` 的第一个参数是内存被分配的对应的分配器，后续参数为构造函数的参数。调用完后会返回
创建对象的内存 ID。

### 实现

白鸟引擎提供了多种堆内存分配器的实现，包括：

#### HeapAllocatorRAM

随机分配内存，与直接 `malloc` 相同。没有分配大小限制，但是可能会产生较多的内存碎片。

```cpp
// 无需参数
HeapAllocatorRAM allocator;
```

#### HeapAllocatorPool

将内存分配在一个内存池中。池的总大小需要在构建的时候指定，而且不可以分配超出池总大小的内存。
如果超出将抛出异常。

可以有效减少内存碎片提高性能，但是会限制内存分配大小。

```cpp
// 第一个参数为分配的总大小，第二个参数为内存对齐。
// 第二个参数为可选参数，如果不指定将会默认为 alignof(std::max_align_t)。
// 如果不希望对齐，可以将第二个参数设置为 1 或者 0。
HeapAllocatorPool allocator (1024, 8);
```

`HeapAllocatorPool` 并不会自动整理内存并保证内存连续，如果分配方式不当依然可能产生内存碎片。

#### HeapAllocatorFixedSizePool

只允许分配固定大小对象的内存池。池的总大小需要在构建的时候指定，而且不可以分配超出池总大小
的内存。如果超出将抛出异常。

在分配释放的时候，分配器会自动调整数据位置来保证数据连续性。

分配的时候如果参数不同于指定的对象大小，将会抛出异常。

```cpp
struct MyData {
    int val1;
    double val2;
};

// 模板参数为指定的数据大小。
// 第一个参数为可选参数，指定分配对象的数量。池的总大小将为：对象数量*对象大小。默认为 64
HeapAllocatorFixedSizePool<sizeof(MyData)> allocator(32);
```

## 萃取

`AllocatorTrait` 是对分配器的属性萃取。可以获取到对应分配器的多种属性。包括：

```cpp
// 分配器类型，可以是 AllocatorType::STACK_ALLOCATOR,  AllocatorType::HEAP_ALLOCATOR，代表堆分配器，或者
AllocatorType TYPE;
// 是否是池分配
bool IS_POOL;
// 是否保证连续
bool IS_GURANTEED_CONTINUOUS;
// 是否限制总大小
bool IS_LIMITED_SIZE;
// 分配大小是否固定
bool IS_ALLOC_FIXED_SIZE;
// 分配/释放是否原子化（线程安全）
bool IS_ATOMIC;
```

例如，`AllocatorRAM` 的总大小不限制，但是不保证连续，可以这样进行萃取：

```cpp
AllocatorTrait<HeapAllocatorRAM>::IS_LIMITED_SIZE;          // true
AllocatorTrait<HeapAllocatorRAM>::IS_GURANTEED_CONTINUOUS;  // false
```

