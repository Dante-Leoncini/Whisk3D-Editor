# Timeline

显示并编辑动画：关键帧表（dope sheet）或曲线编辑器。它显示**所选内容**中带动画的部分。

## 顶部栏

| 功能 | 说明 |
|---|---|
| 播放控制 | 跳到开头、上一个关键帧、上一帧、倒放、播放 / 暂停、下一帧、下一个关键帧、跳到结尾。 |
| Start / End / FPS | 动画的范围和速度（单击即可输入）。 |
| Animation | 编辑哪个动画（场景、片段、骨架、对象、flipbook、Game、Mix），以及新建动画。 |
| Auto Key | 显示为红色时：每次变换都会自动记录关键帧。 |
| Curves / Dope Sheet | 在关键帧表和曲线编辑器之间切换。 |
| Select、Key、View、Pivot | 选择菜单、关键帧操作、取景和侧边面板，以及 <kbd>S</kbd> 以哪里为中心缩放（中心或当前帧）。 |

## 在时间上导航

| 功能 | 说明 | 快捷键 |
|---|---|---|
| 切换帧 | 在帧号上单击或拖动（scrub）。当前帧（绿色）可以直接输入。 | 单击 |
| Previous frame / Next frame | 后退或前进一帧。 | <kbd>Left</kbd> / <kbd>Right</kbd> |
| Previous keyframe / Next keyframe | 跳到最近的关键帧。 | <kbd>Down</kbd> / <kbd>Up</kbd> |
| Go to start / Go to end | 第一帧或最后一帧。 | <kbd>Home</kbd> / <kbd>End</kbd> |
| Play / Pause | 在任意面板中都可以使用。 | <kbd>Space</kbd> |
| 缩放和平移 | 滚轮 = 缩放（在曲线中，<kbd>Ctrl</kbd> = 只缩放时间，<kbd>Shift</kbd> = 只缩放数值）；中键 = 平移。 | 滚轮 / 中键 |
| Frame Selected | 调整视图，让所选内容填满视图。 | <kbd>Num.</kbd> |

## 编辑关键帧（dope sheet 和曲线）

| 功能 | 说明 | 快捷键 |
|---|---|---|
| Select | 单击一个菱形，或单击一行以选中该行的所有关键帧；<kbd>Shift</kbd> 加选。 | 单击；<kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| 移动关键帧 | 在时间轴上挪动关键帧（也可以直接拖动菱形）。 | <kbd>G</kbd> |
| 缩放关键帧 | 以轴心为中心拉伸或压缩。 | <kbd>S</kbd> |
| Rotate handles | 仅在曲线中可用。 | <kbd>R</kbd> |
| Duplicate | 复制关键帧，并保持抓取状态。 | <kbd>Shift+D</kbd> |
| Delete | 删除所选的关键帧（或所选行的曲线）。 | <kbd>X</kbd> |
| Interpolation | Constant、Linear 或 Bezier。 | <kbd>T</kbd> |
| Handle type | Free、Aligned、Vector、Automatic、Auto Clamped（仅限曲线）。 | <kbd>V</kbd> |
| Smart Euler | 去除所选旋转中多余的整圈。 | Key 菜单 |
| 隐藏曲线 | 每一行的眼睛图标可以隐藏该曲线。 | 单击眼睛图标 |
| 精确数值 | 所选关键帧的帧、数值和手柄。 | Properties → Animation → Keyframe |

## 声音

| 功能 | 说明 | 快捷键 |
|---|---|---|
| 声音条 | 每个选中的 Sound 对象都有一条：从哪里开始、持续多久，以**秒**为单位（改变 FPS 不会移动它）。循环时一直重复到结尾。 | — |
| 移动 | 拖动声音条，或在未选关键帧时按 <kbd>G</kbd>（移动所有选中的声音）。<kbd>Ctrl</kbd> 吸附到帧。 | 拖动 / <kbd>G</kbd> |
| 裁剪 | 抓住声音条的一端。 | 拖动一端 |
| Waveform / Volume line | View 菜单：在声音条内显示波形和音量曲线（声音条占三行）。不显示时只是一条。 | View 菜单 |
