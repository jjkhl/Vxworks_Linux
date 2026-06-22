# 第一阶段

- 多通道
- Trigger
- Cursor
- 历史回放



# 一、多通道显示（优先级最高）

这是最值得先做的。

------

## 现在的数据结构

你现在：

```
struct Event
{
    double x;
    double y;
};
```

只能表示：

```
一条曲线
```

------

## 升级后

比如 4 通道：

```
struct Event
{
    double x;
    double ch1;
    double ch2;
    double ch3;
    double ch4;
};
```

或者更灵活：

```
struct Event
{
    double x;
    std::vector<double> ys;
};
```

但实时系统里一般不用 vector（动态分配）。

建议：

```
constexpr int CHANNEL_NUM = 4;

struct Event
{
    double x;
    double y[CHANNEL_NUM];
};
```

更工业。

------

## UI升级

你现在：

```
1条绿色曲线
```

升级成：

```
CH1 绿色
CH2 红色
CH3 黄色
CH4 蓝色
```

------

Qwt：

```
QwtPlotCurve* curves[4];
```

初始化：

```
curves[0] = new QwtPlotCurve("CH1");
curves[1] = new QwtPlotCurve("CH2");
curves[2] = new QwtPlotCurve("CH3");
curves[3] = new QwtPlotCurve("CH4");
```

------

缓存：

```
QVector<double> plotXs;
QVector<double> plotYs[4];
```

------

更新：

```
for (int i = 0; i < 4; i++)
{
    curves[i]->setSamples(plotXs, plotYs[i]);
}
```

------

## 再加显示开关

UI加 Checkbox：

```
☑ CH1
☑ CH2
☐ CH3
☑ CH4
```

控制：

```
curves[i]->setVisible(true/false);
```

------

## 难度

⭐ ⭐

收益巨大。

建议第一个做。

------

# 二、Trigger（示波器核心功能）

这个非常像示波器。

------

## 什么是 Trigger

例如：

信号：

```
0.2 0.4 0.6 1.2 3.5 6.8
```

阈值：

```
5.0
```

当：

```
信号从低于5 → 高于5
```

时：

触发。

------

## 应用

- 抓异常波形
- 找尖峰
- 找突变

工业里很常见。

------

## Trigger类型

### 上升沿

```
last < threshold &&
current >= threshold
```

------

### 下降沿

```
last > threshold &&
current <= threshold
```

------

### 窗口触发

```
min <= value <= max
```

------

## 实现

配置：

```
double triggerThreshold = 5.0;
bool triggerEnabled = true;
bool triggered = false;
```

------

检测：

```
if (!triggered)
{
    if (lastValue < triggerThreshold &&
        currentValue >= triggerThreshold)
    {
        triggered = true;
        paused = true;
    }
}
```

------

效果：

```
检测到波峰
↓
自动暂停
↓
用户分析
```

------

## UI

增加：

- Trigger Enable
- Threshold
- Rising / Falling

------

## 难度

⭐ ⭐ ⭐

价值非常高。

------

# 三、Cursor（分析神器）

这个比普通鼠标坐标高级。

------

你现在是：

```
鼠标位置 -> 当前坐标
```

Cursor 是：

```
Cursor A
Cursor B
```

------

例如：

```
A = (1.25, 4.6)
B = (2.35, 7.9)
```

计算：

```
ΔX = 1.10 s
ΔY = 3.3 V
```

------

## 用途

非常常见：

- 峰峰值
- 上升时间
- 延迟分析

------

## UI

加两个垂直线：

```
|      |
A      B
```

------

Qwt可以用：

```
QwtPlotMarker
```

实现。

------

```
QwtPlotMarker* cursorA;
QwtPlotMarker* cursorB;
```

------

设置：

```
cursorA->setXValue(1.5);
cursorB->setXValue(3.2);
```

------

显示：

```
deltaX = xB - xA;
deltaY = yB - yA;
```

------

## 难度

⭐ ⭐ ⭐

------

# 四、历史回放（非常实用）

这个会让你的软件一下子“专业很多”。

------

## 现在

数据只能实时看。

关闭就没了。

------

## 升级后

支持：

```
导入CSV
```

然后：

- 播放
- 暂停
- 快进
- 回放

------

## UI

加：

- Open CSV
- Play
- Pause
- Speed x1/x2/x4

------

## 实现思路

读 CSV：

```
std::vector<Event> history;
```

------

定时器：

```
playIndex++;
```

------

更新：

```
curve->setSamples(...)
```

------

## 高级功能

拖动时间轴：

```
跳到 02:31
```

类似视频播放器。

------

## 难度

⭐ ⭐ ⭐ ⭐

但很值。


# 第二阶段

- Lock-Free Queue
- 多线程架构
- 性能优化


# 第三阶段

- FFT
- 滤波
- 报警


# 第四阶段

- RPC
- 插件化
- 工程化