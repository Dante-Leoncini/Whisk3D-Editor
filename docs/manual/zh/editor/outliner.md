# Outliner

场景中所有内容的列表，同时也是项目的资源库。在 Android 上不显示（在那里使用 3D Viewport 和 Properties 工作）。

## View 菜单（眼睛图标）

| 功能 | 说明 |
|---|---|
| Scene / Library / Libraries | 列出什么：场景树、项目的所有资源，或另一个已链接 .w3d 的资源（只读）。 |
| Constructor / Routines | 显示或隐藏 Constructor 例程（开始时只运行一次的内容）以及其他例程。 |
| Visibility icons / Render icons | 显示或隐藏眼睛列和相机列。 |
| Grid | 在 Library 中：用缩略图代替列表。 |
| Link .w3d… / Unlink | 把另一个项目用作库，或解除链接。 |

## Scene 视图

| 功能 | 说明 | 使用方法 |
|---|---|---|
| Select | 选择对象。<kbd>Shift</kbd>+单击选中一个范围；<kbd>Ctrl</kbd>+单击加选或减选。 | 单击 |
| 展开 / 折叠 | 显示或隐藏子级。 | 箭头；<kbd>Left</kbd> / <kbd>Right</kbd> |
| Visible / Renderable | 眼睛图标会隐藏对象（可以做成动画）；相机图标会把它排除在渲染之外。 | 单击眼睛 / 相机图标 |
| 重新排序和设置父级 | 放到另一行的前面、后面或上面（作为子级）。 | 拖动该行 |
| Move (reorder) | 用键盘：上/下重新排序，左移出父级，右放到上一项之下，<kbd>Enter</kbd> 确认。 | <kbd>G</kbd> |
| Rename | 该行会变成文本框。 | 双击或 <kbd>F2</kbd> |
| Duplicate / Duplicate Linked | 与 3D Viewport 中相同。 | <kbd>Shift+D</kbd> / <kbd>Alt+D</kbd> |
| Hide / Show | 切换可见性。 | <kbd>H</kbd> |
| Delete | 需要确认。 | <kbd>X</kbd> 或 <kbd>Delete</kbd> |
| Create Prefab / Unpack Prefab | 在 Object 菜单中。 | 右键单击 |
| Show active | 把列表滚动到活动对象处。 | <kbd>Num.</kbd> |
| 滚动列表 | 用滚轮，或拖动（也可以用手指）。 | 滚轮 / 拖动 |

## Library 视图

| 功能 | 说明 |
|---|---|
| 资源 | 网格、材质、纹理、动画、预制体、场景、声音、脚本、[着色器](shaders.md)、字体和视频，按文件夹存放。拖到 3D Viewport 中网格上的材质或着色器，在松开之前就会显示在指针下方的部分上。绿色对勾 = 正在使用；红色叉号 = 孤立；`*` = 未保存。 |
| Filter / 列表或 Grid | 只显示一种类型；查看缩略图。 |
| 拖动资源 | 拖到文件夹（移动它）、拖到 3D Viewport（网格 = 新对象，材质或纹理 = 指定给碰到的对象，预制体 = 实例），或拖到 Properties 的下拉框中。 |
| New Folder / Move to Folder | 整理（文件夹只用于整理）。 |
| Duplicate Resource / Select Users / Purge Orphans | 复制、找出谁在使用它、删除没人使用的内容。 |
| Inside the .w3d / External | 在 Properties 中（Resource 卡片）：把资源移入或移出项目。 |

## 场景、游戏和预制体

显示场景名称的按钮（在 Outliner 和 3D Viewport 中）会列出项目中的场景、游戏和预制体。在那里你可以新建它们、选择初始场景，以及把场景转换为游戏（或反过来）。
