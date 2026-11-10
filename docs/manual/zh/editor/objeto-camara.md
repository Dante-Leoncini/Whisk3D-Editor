# Camera

**相机**是放在场景中的一个视点。**活动相机**用于游戏和渲染：它看到的画面，就是玩游戏时看到的画面，也是图像或视频中输出的画面。本文是其属性的参考；如何添加和移动对象，请参阅 [Object Mode](../tutoriales/modo-objeto.md) 教程。

![完整的 Camera 选项卡](../tutoriales/img/objeto-camara-propiedades.webp)

## 选项卡

| 选项卡 | 内容 |
|---|---|
| Object | 位置、旋转、名称和可见性，和所有对象一样：参见 [3D Mesh](objeto-malla.md#object-选项卡)。 |
| Camera | 相机的镜头：即下面的卡片。 |
| Constraints | 参见 [Constraints](restricciones.md)。 |
| Scripts | 对象的 Lua 脚本。参见 [Whisk3D 中的 Lua](../lua/README.md)。 |

## Camera 卡片

| 属性 | 作用 |
|---|---|
| Orthographic | 没有透视：远处的东西不会变小，平行线也不会汇聚。参见[透视和正交](../tutoriales/navegacion.md#透视和正交)。 |
| FOV ◆ | 视角，单位为度（从上到下）。值越小越像长焦镜头：拍进的范围更小，看起来更近。值越大就是广角：拍进的范围更大，透视感也更夸张。初始值为 45。 |
| Near ◆ | 最小绘制距离：离相机比这更近的东西不会绘制。初始值为 0.01。 |
| Far ◆ | 最大绘制距离：比这更远的东西不会绘制。初始值为 1000。 |
| Depth of field | 开启景深模糊：位于对焦距离上的东西清晰，其余的模糊。由光线追踪绘制；Post Processing 也可以使用同一个焦点。 |
| F-stop | 模糊的程度，和真实的相机一样：数值越小越模糊。 |
| Focus distance ◆ | 对焦在多少米处。相机上绘制的线和十字标出对焦的位置。 |
| Focus object | 焦点会自动跟随的对象，无论它在哪里。选定对象后，Focus distance 会消失。选择 **None** 则恢复手动设置距离。 |
| Target | 活动相机始终注视的对象：如果该对象移动，相机会转动来跟随它。选择 **None** 时，相机朝向你旋转它的方向。 |

勾选 Depth of field 后，才会出现 F-stop、Focus distance 和 Focus object。

### 何时调整 Near 和 Far

如果离相机很近的墙或角色被切掉了，就调小 **Near**。如果大型场景的远处背景消失了，就调大 **Far**。范围最好不要拉得比需要的更大：Near 和 Far 相距太远时，靠得很近的面可能会闪烁，在 Nokia N95 上尤其如此。

Near 和 Far 可以做成动画：例如，当相机升高、显示更多场景时，可以用它把绘制距离的上限推远。

### 跟随角色的相机

根据镜头的需要，有两种方式：

- **相机待在原地并转动：**把角色放到 **Target** 中。即使角色在跑，活动相机也会让它保持在画面中央，就像有人站在一旁看着它经过。
- **相机跟着角色移动：**把 Target 保持为 None，并为相机添加一个 **Child Of** 约束，以角色为来源，并且只勾选 **Location**。相机会以相同的距离和角度跟着角色移动，角色转身时相机不会跟着转（参见 [Child Of](restricciones.md#child-of)）。

## 选择活动相机

场景中可以有多台相机，但只有一台是活动相机。要更换活动相机：

| | 如何选择 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 选中相机，然后按 <kbd>Ctrl</kbd> + <kbd>Num 0</kbd> |
| 菜单 | View → Cameras → Set Active Object as Camera |
| Properties | Animation 选项卡 → **Active camera** 卡片 |

在 Active camera 卡片中，菱形图标用来设置**镜头切换**：在某一帧从一台相机切换到另一台。参见 [Timeline](linea-de-tiempo.md)。

要通过活动相机观看并通过导航来调整镜头，请参阅导航教程中的[活动相机](../tutoriales/navegacion.md#活动相机)。
