# 消息和调试

| 函数 | 作用 |
|---|---|
| `print(...)` | 写入引擎的日志（log），而不是屏幕。 |
| `info(msg)`, `warning(msg)`, `error(msg)` | 在日志中写入信息、警告或错误消息。`error` **不会**中断脚本。 |
| `debugLog(msg)`, `isDebug()` | 只在调试版本中输出的消息。 |
| `logCount()`, `logLine(i)` | 在游戏中读取日志（做一个自己的控制台）。 |
