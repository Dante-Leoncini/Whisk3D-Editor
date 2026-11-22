# 声音

| 函数 | 作用 |
|---|---|
| `beep(freq [, ms, vol])` | 一声蜂鸣（不需要文件）。 |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | 播放一个声音；返回一个标识符。 |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | 同样的函数，使用命名参数，并可设置包络（起音、衰减、0 到 1 的持续电平、`fall`：持续电平在这么多秒内下降 60 dB、释音，以及 `duration`：音符在这么多秒后自动释放）和送入混响的比例（0 到 1）。 |
| `stopSound(h [, fundido])`, `stopSounds()`, `adjustSound(h, vol [, pitch])` | 停止或调整声音。 |
| `releaseSound(h)` | 释放音符：包络进入释音阶段。 |
| `reverb(room, damping, level)` | 全局混响：房间大小、阻尼和电平（各 0 到 1；电平 0 关闭）。 |
| `mute(bool)`, `isMuted()` | 全局静音。 |
| `playSound(obj [, seg])`、`muteSound(obj)` | 重新播放一个 **Sound 对象**（从其片段的第 `seg` 秒开始）或让它静音。 |
| `soundObject(obj) → t`、`setSoundObject(obj, t)` | 读取 / 修改 Sound 对象：`file`（音源）、`active`、`volume`、`pitch`、`reverb`、`pan`、`start`、`clipStart`、`clipEnd`、`loop`、`positional`、`radius`、`range`（读取时还有 `duration`、`playing`）。 |
| `createSound("sounds/x.wav" [, parent]) → obj` | 新建一个 Sound 对象（`destroy(obj)` 删除它）。在预制体中会像其他对象一样被实例化。 |
