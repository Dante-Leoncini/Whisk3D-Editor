# Rede

O **w3dRed** joga em rede local, wi-fi ou internet, por UDP. Um jogador **hospeda** a partida (porta 47023) e os outros a **procuram** na rede local ou **entram** com um endereço. Tudo passa pelo host, até 8 jogadores. As mensagens podem ser **confiáveis** (chegam todas e em ordem: "pronto", "começar") ou **não confiáveis** (o que é enviado a cada quadro, em que a seguinte substitui a perdida).

```cpp
w3dNet::Host("mi_juego", "La partida de Dante", 2);
// ou, do outro lado:
w3dNet::Find("mi_juego");   // ... w3dNet::Matches() ... w3dNet::Join(direccion);
w3dNet::Send(-1, data, bytes, true);   // -1 = para todos os outros
```

| Sistema | Sockets |
|---|---|
| Windows | Winsock |
| Linux, macOS, Android | POSIX (a lista de interfaces, a partir do Android 7) |
| Symbian | Os sockets do Open C |
| Dreamcast | Os sockets do KallistiOS, pelo adaptador de rede |
| Navegador, PSP, PS Vita, 3DS | Sem rede por enquanto: `Supported()` retorna `false` |

Um jogo não usa a rede se a aplicação não permitir (`SetAllowed`): o editor pergunta ao abrir um projeto que a usa. Em Lua: [Jogar em rede](../lua/api-red.md).
