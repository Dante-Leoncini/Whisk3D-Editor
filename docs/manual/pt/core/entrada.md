# Entrada

Em quase todos os sistemas, o **SDL2** entrega o teclado, o mouse, os dedos e os controles, e o editor e os jogos são escritos sobre esses eventos. O Core completa o que o SDL não cobre:

| O quê | Como chega |
|---|---|
| Toque | SDL no Android, no navegador, no Vita e no 3DS. No Symbian, o `W3dTactil` faz a mesma conversão que o SDL (o primeiro dedo move um mouse, os outros chegam como dedos): um dedo no S60 5th, multitoque no N8 |
| Inclinação | No N95, a Sensor API da Nokia, carregada ao iniciar (sem ela, o jogo funciona igual). No Android e no PC, chega pelo SDL |
| Controles | SDL no resto. No Symbian, os controles de PS3 e PS4 por Bluetooth (o driver bthid-s60), na mesma ordem de botões que o SDL. Até 4 |
| Vibração | Android, com o vibrador do celular. Navegador, `navigator.vibrate` (no iOS, o "toque" háptico do sistema, com um truque). Desktop: nada. A API promete pouco: "uma vibração curta, se possível" |
| Área de transferência | SDL (também a do navegador); `CClipboard` no Symbian |

Nos consoles sem tela touch (PSP, Dreamcast), o controle move um cursor, como um mouse.

Por que importa: os gestos do touch do Symbian passam pelo mesmo caminho que os do Android, então uma melhoria dos gestos no Android chega sozinha ao celular. Em Lua: [Teclado, controle, toque e mouse](../lua/api-entrada.md).
