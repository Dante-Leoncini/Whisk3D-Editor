# Teclado, controle, toque e mouse

| Função | O que faz |
|---|---|
| `key(name) → bool` | A tecla está pressionada (enquanto você a mantém). |
| `keyPressed(name) → bool` | Foi pressionada exatamente neste quadro (uma única vez por pressionamento). |
| `button(name [, gamepad])`, `buttonPressed(name [, gamepad])` | O mesmo com um botão do controle. Com `gamepad` (1 a 4), o desse controle; sem ele, o de qualquer um. |
| `stick(["left"\|"right"] [, gamepad]) → x, y` | Alavanca do controle, de -1 a 1 (para cima é -1). |
| `trigger(["left"\|"right"] [, gamepad]) → 0..1` | Gatilho analógico. |
| `controls() → n`, `control([n]) → name, tipo` | Quantos controles há e de que tipo (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `tap() → x, y, touching` | O dedo (ou o clique do mouse) na tela. |
| `finger([n]) → x, y, active` | Multitoque: o dedo número n (1 a 4). |
| `touch() → bool` | A tela é de toque: um telefone ou um tablet (Android, o navegador de um iPhone…) ou qualquer aparelho em que a tela já foi tocada. Para mostrar controles na tela só onde eles fazem falta. |
| `tilt() → x, y, z` | O acelerômetro do telefone, em g, nos eixos da tela do jeito que ela é vista (já girada): `x` é o quanto ele está girado para a direita, como um volante (o seno do ângulo); em pé e reto dá `y = 1`, deitado de barriga para cima `z = 1`. `nil` se o aparelho não tiver acelerômetro. |
| `vibrate(ms [, force])` | Vibra por `ms` milissegundos com `force` de 0 a 1: o vibrador do telefone (Android, o navegador) e o motor dos controles (PC). Em um telefone, a força encurta o pulso. Não faz nada onde não há com o que vibrar, nem se o jogador desligou a vibração. |
| `pressed(obj) → bool` | Esse elemento 2D foi tocado ou clicado neste quadro (botões). |
| `mouse() → x, y, dentro` | Onde está o mouse. |
| `captureMouse(bool)`, `mouseRel() → dx, dy`, `mouseCaptured()` | Mouse capturado para olhar com o mouse (jogos em primeira pessoa). No PC e no navegador; no navegador, o mouse é capturado com o primeiro clique ou tecla no jogo. <kbd>Esc</kbd> o solta e pausa o jogo. |
