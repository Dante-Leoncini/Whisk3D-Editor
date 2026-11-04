# Script Editor（IDE）

用于编写 Lua 脚本和[手写着色器](shaders.md)（`.glsl`）的文本编辑器，带有行号、语法高亮、缩进参考线，并支持 ñ 和重音符号。它拥有焦点时，按字母键就是输入文字（不会触发编辑器快捷键）。在 Android 上，点击文本会打开键盘。在 N95 上使用 T9 输入。

| 功能 | 说明 | 快捷键 |
|---|---|---|
| Script | 选择要编辑项目中的哪个 .lua，或 Load .lua…（外部文件）。 | Script 菜单 |
| Save | 保存脚本。如果脚本存放在 .w3d 内部，也会同时保存项目。 | <kbd>Ctrl+S</kbd> 或 File → Save |
| Save As… | 换一个名字，保存在项目内部或外部，文件夹可以任选。 | File |
| Save inside the .w3d | 仅当脚本在外部时可用：把它放进项目中。 | File |
| Save outside the .w3d… | 仅当脚本在内部时可用：把它导出成一个文件；各对象会改为使用这个文件。 | File |
| Refresh | 读取磁盘上文件的当前内容（在其他编辑器中修改过的内容）。如果编辑器中有未保存的修改，会在丢弃前询问你（之后还可以用 <kbd>Ctrl+Z</kbd> 找回）。游戏运行时，会实时重新加载使用该脚本的对象。 | 按钮 |
| 着色器（.glsl） | 可从材质（**Shader** 行的小按钮）、Library 或选择器中打开，并按 GLSL 语法高亮。名称旁边会出现 **Compile**：编译你正在编写的内容（如果有材质在用它，会先保存再编译该材质）。保存着色器会在所有使用它的材质中编译它，并提示是否编译成功，或显示错误及其行号。 | <kbd>Ctrl+S</kbd> |
| 保存保护 | 如果外部文件在打开后被别处修改过，Save 会在覆盖前询问你。游戏运行时，Save 也会实时重新加载脚本。 | 自动 |
| 缩进（Tab） | <kbd>Tab</kbd> 补齐到下一个 4 的倍数；选中多行时，为它们增加一级缩进，<kbd>Shift+Tab</kbd> 则减少一级。竖线标出每一级缩进；光标所在代码块的那条线更亮。 | <kbd>Tab</kbd> / <kbd>Shift+Tab</kbd> |
| Edit 菜单 | Undo、Redo、Cut、Copy、Paste、Find… 和 Replace… | Edit 按钮 |
| Select 菜单 | Select All。 | Select 按钮 |
| Undo / Redo | 作用于脚本的文本（而不是场景）。 | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> 或 <kbd>Ctrl+Shift+Z</kbd> |
| Find / Replace | 在右上角打开一个面板：要查找的文本、匹配数量（“3 of 7”）、Match case、Whole word、Previous / Next，在 Replace 中还有新文本以及 Replace 和 Replace all（只占一个撤销步骤）。匹配项会在代码中高亮显示。这个面板不会挡住编辑器：指针在脚本上时可以继续输入。用 Close 或 <kbd>Esc</kbd> 关闭。 | <kbd>Ctrl+F</kbd> / <kbd>Ctrl+H</kbd> |
| Copy / Cut / Paste | 使用系统剪贴板。 | <kbd>Ctrl+C</kbd> / <kbd>Ctrl+X</kbd> / <kbd>Ctrl+V</kbd> |
| Select All / Undo | Undo 只影响文本，不影响场景。 | <kbd>Ctrl+A</kbd> / <kbd>Ctrl+Z</kbd> |
| 用手指选择 | 双击选中单词，三击选中整行。有选区时，两端会出现两个手柄（绿色圆圈）：拖动它们可以调整选区。 | 轻点 |
| 放大镜（触屏） | 手指在代码上停留片刻，手指上方会显示放大的当前行和光标；拖动时光标跟随手指，松开后停在该处。 | 长按 |
| 在 N95 上选择 | 方向键移动光标；按住 Pencil 键时，方向键会从光标所在位置开始选择。按住 Pencil 键期间，下方会出现一个栏：左软键**复制**，右软键**粘贴**。 | Pencil 键 + 方向键 / 软键 |
| 在文本中移动 | 方向键、Home、End、PageUp、PageDown（配合 <kbd>Shift</kbd> 可以选择）；<kbd>Ctrl+Home</kbd> / <kbd>Ctrl+End</kbd> 跳到开头或结尾。 | 键盘 |
