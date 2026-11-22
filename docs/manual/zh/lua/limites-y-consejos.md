# 限制和建议

## Lua 中不可用的部分

- 可以使用 `string`、`table` 和 `math`，以及基础函数（`print`、`pairs`、`ipairs`、`type`、`tostring`、`tonumber`、`pcall`、`assert`……）。
- **没有** `io` 和 `os`（游戏不会访问磁盘上的文件；所以也没有 `os.time`），也没有 `require`、`coroutine`、`utf8` 和 `debug`。要保存数据，请使用 `getConfig` / `saveConfig`。
- 每个脚本都是独立的：它看不到其他脚本的变量。要相互通信，请使用 `setShared` / `getShared`。

## 错误

- 如果某个函数出错，错误会记录到日志中（`archivo.lua:LÍNEA: mensaje`），这次调用会被中断，但下一帧还会再次调用它：同样的错误会在每一帧重复出现，直到你修复为止。
- 如果文件无法读取或有语法错误，该脚本会一直处于关闭状态，直到下一次 Play。
- 在 Whisk3D 中，`error("...")` 只会记录消息，脚本会继续运行。如果你想中断执行，请使用 `assert(condicion, "mensaje")`。

## 注意事项

- `update` 只会在活动场景中**可见**的对象上运行：隐藏一个对象，就会关闭它的逻辑。
- `destroy`、`changeScene` 和 `playScene` 在帧结束时才生效，而不是立即生效。
- 游戏文本使用的字体无法绘制重音符号和 ñ：请写 `"MENU"`，而不是 `"MENÚ"`。
- `safeArea`、`uiBox` 和 `resolution` 要在绘制完第一帧之后才有真实的值：不要在 `start` 中使用它们。
- 在 Web 和 Android 上，只有**第一个**灯光会产生照明。
- Nokia N95 上没有手柄和鼠标：请用 `key` 配合手机键盘的按键名称。整数是 32 位的。

## 让游戏跑得更快（尤其是在手机上）

- 只在 `start` 中查找一次对象，并把它们保存在 `local` 变量中。
- 使用“批量”版本（`setVertices`、`setVerticesColor`），而不是把单个版本调用上千次。
- 优先使用引擎的函数（`scrollUV`、`linePoint`、带旋转顺序的 `setRotation`），而不是在 Lua 中自己做计算。
- 先用 `luaProfile` 和 `clockMs` 测量，再进行优化，不要盲目优化。
