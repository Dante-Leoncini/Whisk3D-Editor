# Sound

**Sound** 对象是放在场景中的一段音频：一段音乐、一个音效、一台机器的噪音。在 3D Viewport 中它显示为一个扬声器。它可以在任何地方听起来都一样，像音乐那样；也可以从它所在的位置发声，离得越近越响。本文是其属性的参考；如何添加和移动对象，请参阅 [Object Mode](../tutoriales/modo-objeto.md) 教程。

新建的 Sound 对象一开始没有音频：**None**。音频文件在 Sound 卡片中选择。

![完整的 Sound 选项卡，显示一个刚添加的声音：文件为 None](../tutoriales/img/objeto-sonido-propiedades.webp)

## 直接带着声音创建

- ![PC](../tutoriales/img/icono-pc.svg) 把 [Library](outliner.md#library-视图) 中的一个声音拖到 3D Viewport 中，会在放下的位置创建一个使用该声音的 Sound 对象。
- ![PC](../tutoriales/img/icono-pc.svg) 把一个声音文件（例如 `.wav`）拖放到 3D Viewport 上，会把它加入项目，并在那里创建对象。

## 选项卡

| 选项卡 | 内容 |
|---|---|
| Object | 位置、名称和可见性，和所有对象一样：参见 [3D Mesh](objeto-malla.md#object-选项卡)。隐藏的声音不会发声，隐藏一个集合会让其中的声音全部静音。 |
| Sound | 即下面的卡片。 |
| Constraints | 参见 [Constraints](restricciones.md)。 |
| Scripts | 对象的 Lua 脚本，可以在游戏中让它发声或静音。参见 [Whisk3D 中的 Lua](../lua/README.md)。 |

## Sound 卡片

| 属性 | 作用 |
|---|---|
| Sound | 播放的文件及其时长。菜单中列出项目中的声音、**None**，以及 **Load sound from the file browser...**（从磁盘把一个声音载入项目）。 |
| Active | 取消勾选后不会发声。可以用来关掉它而不删除。 |
| Volume ◆ | 声音有多响：1 是文件本身的音量，0 是静音，2 是两倍。 |
| Pitch ◆ | 更尖或更低沉。1 是原始音高；2 高一个八度、速度快一倍，0.5 低一个八度、速度慢一半。因此改变音高也会改变时长。 |
| Reverb ◆ | 这个声音有多少送入混响的回声：0 完全没有，1 全部送入。能营造出洞穴、教堂或走廊的感觉。 |
| Pan | 从哪一侧发声：-1 在左侧，0 在中间，1 在右侧。只在不使用 Positional (3D) 时有效：使用时，由它所在的位置决定从哪一侧发声。 |
| Start time | 在场景的第几秒开始播放。 |
| Trim start | 从文件开头剪掉多少秒。 |
| Trim end | 从文件结尾剪掉多少秒。 |
| Loop forever | 两次剪切之间剩下的部分会不停地无缝循环。适用于背景音乐和持续的噪音。 |
| Positional (3D) | 从对象所在的位置发声。见下文。 |
| Full volume radius | 仅在使用 Positional (3D) 时：在这个距离（米）以内，以完整音量播放。 |
| Audible range | 仅在使用 Positional (3D) 时：超过这个距离（米）就听不到了。 |

## 时间以秒为单位

声音的时间以**秒**为单位，而不是帧。这样，如果场景从每秒 30 帧改为 60 帧，声音仍然对齐在同一个时刻，不需要重新调整任何东西。

在 [Timeline](linea-de-tiempo.md#声音) 中，每个选中的声音都有自己的**条带**：显示它从哪里开始、持续多久。拖动条带会改变 **Start time**，抓住条带的某一端则会改变剪切。用 Timeline 的 Play 播放时它会发声，停止时静音。

在游戏中，声音在出现时开始播放（例如作为预制体一部分的爆炸声），再加上 **Start time** 指定的时间。

## Positional (3D)

不勾选时，无论声音在哪里，听起来都一样，像音乐那样。勾选后，声音会相对于听者，**从它所在的位置**传来：

- 越近越响：在 **Full volume radius** 以内以完整音量播放，从那里开始逐渐变小，直到在 **Audible range** 处消失。
- 如果它在右边，声音就从右边传来；如果在左边，就从左边传来。
- 如果它在后面，声音会更小，也更靠中间。

听者在游戏中是活动相机，在编辑器中是活动的 3D Viewport（如果正通过相机观看，则是相机）。选中声音时，3D Viewport 会绘制两个球体：一个表示完整音量半径，另一个表示可听范围。

## 示例

| 声音 | 如何搭建 |
|---|---|
| 背景音乐 | 不使用 Positional (3D)，开启 Loop forever。 |
| 瀑布 | Positional (3D) 和 Loop forever，Full volume radius 设得小，Audible range 设得大：从远处就开始听到，越靠近越响。 |
| 动画第 4 秒的一声撞击 | Start time 设为 4，或者在 Timeline 中把条带移到那里。在 30 FPS 下是第 121 帧：第 1 帧是第 0 秒。 |
| 在结尾淡出的音乐 | 在 Volume 上设置关键帧（◆）：结尾前一点设为 1，结尾处设为 0。 |
