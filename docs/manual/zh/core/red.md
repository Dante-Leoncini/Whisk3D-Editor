# 网络

**w3dRed** 通过 UDP 在局域网、Wi-Fi 或互联网上联机游戏。一个人**主持**对局（端口 47023），其他人在局域网中**搜索**它，或者通过地址**加入**。所有数据都经过主机，最多 8 名玩家。消息可以是**可靠的**（全部送达且有序：“准备好了”“开始”），也可以是**不可靠的**（每帧发送的内容，下一条会取代丢失的那条）。

```cpp
w3dRed::Hostear("mi_juego", "La partida de Dante", 2);
// 或者，在另一端：
w3dRed::Buscar("mi_juego");   // ... w3dRed::Partidas() ... w3dRed::Unirse(direccion);
w3dRed::Enviar(-1, datos, bytes, true);   // -1 = 发给其他所有人
```

| 系统 | 套接字 |
|---|---|
| Windows | Winsock |
| Linux、macOS、Android | POSIX（网络接口列表从 Android 7 起可用） |
| Symbian | Open C 的套接字 |
| Dreamcast | KallistiOS 的套接字，通过网络适配器 |
| 浏览器、PSP、PS Vita、3DS | 暂时没有网络：`Soportada()` 返回 `false` |

如果应用程序不允许，游戏就不会使用网络（`SetPermitida`）：打开一个使用网络的项目时，编辑器会询问。在 Lua 中：[联网游戏](../lua/api-red.md)。
