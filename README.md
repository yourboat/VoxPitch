# VoxPitch

基于 C++17 / Qt6（Multimedia · Charts · Widgets）开发的桌面端实时音高检测与声乐辅助训练软件，面向声乐学习者与音乐爱好者。目标是把"听感上的跑调"转化为图形化、可量化的反馈。

界面采用类似 Claude 客户端的左侧边栏导航，包含三个并列功能页面。

## 功能

### 实时音高

- 持续采集麦克风信号，用 YIN 算法实时检测基频，绘制成"卷帘"式滚动音高曲线
- 纵轴固定显示约 1 个八度，随演唱音高自动平滑居中，只在自然音（白键）位置标注音名
- 横轴是最近 10 秒的时间窗口；曲线最新点旁浮动显示当前音名，并配合文字标签显示频率（Hz）与音分偏差
- 嵌入完整 88 键虚拟钢琴，支持鼠标点击与电脑键盘弹奏，支持多键同时按住（复音）；麦克风检测到的音高会反过来高亮对应琴键

### 绝对音高训练

- 随机播放一个目标音高（根音权重偏向人声常用的 3/4/5 组）或音程/和弦，用户在 88 键钢琴上作答
- 四种模式：单音 / 双音 / 三音和弦 / 四音和弦
- 多音/和弦模式下钢琴切换为多选模式，可累加试听、提交后报出实际选中的音名，正确时报出和弦/音程名称
- 提供"提示"按钮，逐个补全正确音

### 音频文件分析

- 打开本地音频文件（mp3/wav/m4a/flac/aac/ogg），离线跑一遍 YIN 检测生成全曲参考音高曲线
- "固定中心点、拖拽曲线"的交互方式：屏幕水平中点始终对应一个被选中的时间点，拖动图表或进度条来定位
- 仿真实播放器的功能栏：自绘的前进/后退 10 秒图标按钮、播放/暂停（支持空格快捷键）、可拖动进度条
- 录制跟唱：独立的麦克风采集与检测链路实时录音，原音频静音播放仅用于保持时间轴同步；演唱音高以另一种颜色叠加在参考曲线上，实时给出"准确 / 音高了 / 音低了"反馈（偏差小于 1/4 半音判定准确）
- 录音支持多次覆盖（punch-in），最终可导出为 WAV 文件

## 构建

### 依赖

- CMake ≥ 3.16
- 支持 C++17 的编译器
- Qt6（Multimedia、Charts、Widgets 组件）

### macOS

```bash
brew install qt cmake
cmake -S . -B build -DQt6_DIR=/opt/homebrew/lib/cmake/Qt6
cmake --build build -j$(sysctl -n hw.ncpu)
./build/VoxPitch
```

> Homebrew 安装的 Qt6 默认走 macOS 原生 `darwin`（AVFoundation/CoreAudio）多媒体后端；如果系统里还装有官方安装器版本的 Qt（默认 FFmpeg 后端），两者在实时音频时序上表现不一致。项目在 `main.cpp` 里用 `qputenv("QT_MEDIA_BACKEND", "darwin")` 显式锁定后端，避免该差异。
>
> `CMakeLists.txt` 里还处理了 Homebrew Qt6 的 `FindWrapOpenGL.cmake` 在新版 macOS SDK 上因 `AGL` 框架被移除而导致链接失败的问题。

### Windows

```bash
cmake -S . -B build -DQt6_DIR="<你的Qt安装路径>/lib/cmake/Qt6"
cmake --build build
```

（`CMakeLists.txt` 里有一个仅在未显式指定 `Qt6_DIR` 时生效的开发机默认路径，正常构建请始终显式传入自己的 `Qt6_DIR`。）

## 项目结构

| 文件                               | 说明                                           |
| ---------------------------------- | ---------------------------------------------- |
| `main.cpp`                       | 程序入口                                       |
| `mainwindow.h/.cpp`              | 主窗口：侧边栏导航 + 实时音高页                |
| `pitchdetector.h/.cpp`           | YIN 基音检测算法核心                           |
| `audiocapturer.h/.cpp`           | 麦克风采集封装（`QAudioSource`）             |
| `audioplayer.h/.cpp`             | 合成钢琴音色播放，支持一次性播放与复音实时混音 |
| `pianokeyboard.h/.cpp`           | 自绘 88 键钢琴控件（普通模式 / 多选模式）      |
| `pitchtrainerwindow.h/.cpp`      | 绝对音高训练页                                 |
| `audiofileanalysiswindow.h/.cpp` | 音频文件分析与跟唱评测页                       |
| `pannablechartview.h/.cpp`       | 支持拖拽平移的 `QChartView` 子类             |
| `skipbutton.h/.cpp`              | 自绘的"前进/后退 N 秒"圆形图标按钮             |

详细的模块设计、算法说明与开发过程复盘见实验报告。

## 演示视频网盘链接

https://disk.pku.edu.cn/link/AAEA0C54CE977245088A64D9922C090622

## 已知局限 / 后续规划

当前未实现最初设计中规划的以下内容（详见实验报告第四节的可行性分析）：

- 听音训练模块的旋律生成子模式（单音模仿 / 音程辨识 / 和弦辨识的程序化旋律练习）
- 本地数据持久化（SQLite 练习记录、成长曲线、能力雷达图）
- FFT 频谱可视化
- 多线程音频处理架构（当前为单线程，靠算法层优化保证实时性）
- Qt Quick 界面（当前全部基于 Qt Widgets）
