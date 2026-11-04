# 声音

| 函数 | 作用 |
|---|---|
| `beep(freq [, ms, vol])` | 一声蜂鸣（不需要文件）。 |
| `sonido("sonidos/x.wav" [, vol, tono, loop, pan]) → h` | 播放一个声音；返回一个标识符。 |
| `sonido("sonidos/x.wav", {vol, tono, loop, pan, ataque, decaimiento, sostenido, caida, liberacion, duracion, reverb}) → h` | 同样的函数，使用命名参数，并可设置包络（起音、衰减、0 到 1 的持续电平、`caida`：持续电平在这么多秒内下降 60 dB、释音，以及 `duracion`：音符在这么多秒后自动释放）和送入混响的比例（0 到 1）。 |
| `pararSonido(h [, fundido])`, `pararSonidos()`, `ajustarSonido(h, vol [, tono])` | 停止或调整声音。 |
| `soltarSonido(h)` | 释放音符：包络进入释音阶段。 |
| `reverb(sala, amortiguacion, nivel)` | 全局混响：房间大小、阻尼和电平（各 0 到 1；电平 0 关闭）。 |
| `silenciar(bool)`, `estaMudo()` | 全局静音。 |
| `tocarSonido(obj [, seg])`、`callarSonido(obj)` | 重新播放一个 **Sound 对象**（从其片段的第 `seg` 秒开始）或让它静音。 |
| `sonidoObj(obj) → t`、`setSonidoObj(obj, t)` | 读取 / 修改 Sound 对象：`archivo`（音源）、`activo`、`volumen`、`tono`、`reverb`、`paneo`、`inicio`、`recorteIni`、`recorteFin`、`bucle`、`posicional`、`radio`、`alcance`（读取时还有 `duracion`、`sonando`）。 |
| `crearSonido("sonidos/x.wav" [, padre]) → obj` | 新建一个 Sound 对象（`destruir(obj)` 删除它）。在预制体中会像其他对象一样被实例化。 |
