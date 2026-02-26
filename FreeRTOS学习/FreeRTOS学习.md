# FreeRTOS与VxWorks区别

## 总览

| 维度       | VxWorks                | FreeRTOS                |
| ---------- | ---------------------- | ----------------------- |
| 定位       | **完整 RTOS 操作系统** | **RTOS 内核（Kernel）** |
| 设计哲学   | OS 平台                | 内核 + 组件             |
| 工程复杂度 | 高                     | 低（但自由度高）        |
| 商业属性   | 商业闭源               | 开源（MIT）             |
| 学习曲线   | 陡峭但系统             | 平缓但需要自建体系      |

## 任务模型

二者任务模型高度相似，但约束不同。

```cpp
// VxWorks
taskSpawn("tA", 100, 0, 8192, func, 0,0,0,0,0,0,0,0,0,0);

// FreeRTOS
xTaskCreate(func, "tA", 1024, NULL, 3, NULL);
```

| 项目     | VxWorks  | FreeRTOS               |
| -------- | -------- | ---------------------- |
| 栈大小   | 字节     | **Stack word 数**（⚠️） |
| 优先级   | 0~255    | 0~configMAX_PRIORITIES |
| 创建失败 | 少见     | **很常见（堆不够）**   |
| 动态创建 | 默认支持 | 可禁用                 |

**关键差异：**

* **FreeRTOS的栈单位是word，而不是字节**

  > FreeRTOS设计之初就考虑到要运行在各种不同架构的处理器上（8位、16位、32位、64位）。不同架构的"字"大小不同：
  >
  > 8位MCU：1字 = 1字节
  >
  > 16位MCU：1字 = 2字节
  >
  > 32位MCU：1字 = 4字节
  >
  > 64位处理器：1字 = 8字节

* **FreeRTOS强依赖heap_x.c**

使用字的好处：

* 大多数处理器架构要求栈指针必须按字对齐，否则可能导致性能下降或硬件异常，使用字为单位天然保证了栈的正确对齐
* 简化内存管理逻辑、避免额外的对齐计算开销、减小内核代码体积

二者任务模型几乎一致：

| 状态 | VxWorks | FreeRTOS          |
| ---- | ------- | ----------------- |
| 运行 | RUNNING | Running           |
| 就绪 | READY   | Ready             |
| 阻塞 | PEND    | Blocked           |
| 挂起 | SUSPEND | Suspended         |
| 延时 | DELAY   | Blocked (timeout) |

## 调度器

二者理念一致，实现取舍不同。

调度策略

| 项目       | VxWorks | FreeRTOS |
| ---------- | ------- | -------- |
| 抢占       | 是      | 可配置   |
| 时间片     | 是      | 可配置   |
| 优先级调度 | 是      | 是       |
| Tick 驱动  | 是      | 是       |

FreeRTOS在编译期决定行为，一切透明；VxWorks在运行时配置，OS隐藏细节

```cpp
// FreeRTOS
#define configUSE_PREEMPTION     1
#define configUSE_TIME_SLICING   1
#define configTICK_RATE_HZ       1000
```

## 通信机制

```cpp
// VxWorks
// 方式1：VxWorks原生消息队列
MSG_Q_ID msgQId = msgQCreate(10, sizeof(int), MSG_Q_FIFO);

// 发送
int data = 100;
msgQSend(msgQId, (char*)&data, sizeof(int), WAIT_FOREVER, MSG_PRI_NORMAL);

// 接收
int received;
msgQReceive(msgQId, (char*)&received, sizeof(int), WAIT_FOREVER);

// 方式2：POSIX消息队列
mqd_t mq;
struct mq_attr attr = {0, 10, sizeof(int), 0};
mq = mq_open("/myqueue", O_CREAT | O_RDWR, 0644, &attr);

int data = 100;
mq_send(mq, (char*)&data, sizeof(int), 0);

// 特点：
// - 支持优先级（256级）
// - 支持超时、紧急消息
// - POSIX兼容性
// - 支持广播

// FreeRTOS
// 创建队列
QueueHandle_t xQueue = xQueueCreate(10, sizeof(int));

// 发送（阻塞）
int data = 100;
xQueueSend(xQueue, &data, portMAX_DELAY);

// 接收（阻塞）
int received;
xQueueReceive(xQueue, &received, portMAX_DELAY);

// 从ISR发送
xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);

// 特点：
// - 支持FIFO、优先级队列
// - 支持覆盖模式（队列集）
// - 固定大小，创建时确定
```

核心点：

* FIFO队列
* 支持阻塞
* 支持超时

核心差异：

| 项目     | VxWorks  | FreeRTOS           |
| -------- | -------- | ------------------ |
| 传递方式 | 通常指针 | **值拷贝（默认）** |
| ISR 使用 | 直接     | `FromISR` 专用 API |
| 内存管理 | OS 管理  | 用户决定           |

## 信号量/互斥

| 用途 | VxWorks      | FreeRTOS           |
| ---- | ------------ | ------------------ |
| 同步 | SEM_BINARY   | Binary Semaphore   |
| 互斥 | SEM_MUTEX    | Mutex              |
| 计数 | SEM_COUNTING | Counting Semaphore |

FreeRTOS有4种类型，API统一简单；VxWorks选项更多，支持读写锁、更精细的优先级反转控制

```cpp
// VxWorks
// 方式1：VxWorks原生信号量
// 二值信号量
SEM_ID semBinary = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
semGive(semBinary);
semTake(semBinary, WAIT_FOREVER);

// 计数信号量
SEM_ID semCount = semCCreate(SEM_Q_FIFO, 0);

// 互斥量（多种选项）
SEM_ID semMutex = semMCreate(
    SEM_Q_PRIORITY |           // 按优先级排队
    SEM_INVERSION_SAFE |       // 优先级继承
    SEM_DELETE_SAFE            // 删除安全
);

// 方式2：POSIX信号量
sem_t sem;
sem_init(&sem, 0, 1);
sem_wait(&sem);
sem_post(&sem);

// 特点：
// - 支持优先级继承、天花板协议
// - 支持删除安全
// - 支持超时（semTake with timeout）
// - 读写信号量（semRWCreate）

// FreeRTOS
// 二值信号量
SemaphoreHandle_t xBinarySem = xSemaphoreCreateBinary();
xSemaphoreGive(xBinarySem);
xSemaphoreTake(xBinarySem, portMAX_DELAY);

// 计数信号量
SemaphoreHandle_t xCountingSem = xSemaphoreCreateCounting(10, 0);

// 互斥量（带优先级继承）
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();
xSemaphoreTake(xMutex, portMAX_DELAY);
// critical section
xSemaphoreGive(xMutex);

// 递归互斥量
SemaphoreHandle_t xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY); // 可以多次获取
xSemaphoreGiveRecursive(xRecursiveMutex);
xSemaphoreGiveRecursive(xRecursiveMutex);
```

## 中断模型

| 对比维度       | VxWorks                        | FreeRTOS                                   |
| -------------- | ------------------------------ | ------------------------------------------ |
| 中断处理机制   | 两级中断处理：ISR+中断级任务   | 单极ISR，强调快速退出                      |
| ISR中可调用API | 可调用较多系统服务(有限制)     | 仅能调用FromISR后缀的专用API               |
| 中断嵌套       | 原生支持，完善的优先级管理     | 依赖硬件和移植，需配置优先级阈值           |
| 中断连接方式   | intConnect()连接中断向量       | 直接在启动代码或HAL层注册                  |
| 延迟处理机制   | 支持中断级任务(Interrupt Task) | 通过信号量/队列/任务通知唤醒任务           |
| 上下文切换触发 | 支持中断级任务切换             | 通过xHigherPriorityTaskWoken标志延迟切换   |
| 中断优先级配置 | 灵活的中断锁定级别配置         | configMAX_SYSCALL_INTERRUPT_PRIORITY限制   |
| 中断响应延迟   | 通常更低，硬实时性能更高       | 轻量级但需精心设计保证实时性               |
| 中断栈管理     | 独立的中断栈，自动管理         | 通常使用任务栈或独立中断栈(取决于移植)     |
| 可抢占性       | ISR可被更高优先级中断抢占      | 受configMAX_SYSCALL_INTERRUPT_PRIORITY控制 |
| 复杂度         | 功能全面，学习曲线较陡         | 简单轻量，易于理解和使用                   |
| 使用场景       | 复杂的硬实时系统               | 资源受限的嵌入式xi't                       |

