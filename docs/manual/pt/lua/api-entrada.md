# Teclado, controle, toque e mouse

| Função | O que faz |
|---|---|
| `tecla(nombre) → bool` | A tecla está pressionada (enquanto você a mantém). |
| `teclaApretada(nombre) → bool` | Foi pressionada exatamente neste quadro (uma única vez por pressionamento). |
| `boton(nombre [, mando])`, `botonApretado(nombre [, mando])` | O mesmo com um botão do controle. Com `mando` (1 a 4), o desse controle; sem ele, o de qualquer um. |
| `stick(["izq"\|"der"] [, mando]) → x, y` | Alavanca do controle, de -1 a 1 (para cima é -1). |
| `gatillo(["izq"\|"der"] [, mando]) → 0..1` | Gatilho analógico. |
| `controles() → n`, `control([n]) → nombre, tipo` | Quantos controles há e de que tipo (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `toque() → x, y, tocando` | O dedo (ou o clique do mouse) na tela. |
| `dedo([n]) → x, y, activo` | Multitoque: o dedo número n (1 a 4). |
| `tactil() → bool` | A tela é de toque: um telefone ou um tablet (Android, o navegador de um iPhone…) ou qualquer aparelho em que a tela já foi tocada. Para mostrar controles na tela só onde eles fazem falta. |
| `inclinacion() → x, y, z` | O acelerômetro do telefone, em g, nos eixos da tela do jeito que ela é vista (já girada): `x` é o quanto ele está girado para a direita, como um volante (o seno do ângulo); em pé e reto dá `y = 1`, deitado de barriga para cima `z = 1`. `nil` se o aparelho não tiver acelerômetro. |
| `vibrar(ms [, fuerza])` | Vibra por `ms` milissegundos com `fuerza` de 0 a 1: o vibrador do telefone (Android, o navegador) e o motor dos controles (PC). Em um telefone, a força encurta o pulso. Não faz nada onde não há com o que vibrar, nem se o jogador desligou a vibração. |
| `apretado(obj) → bool` | Esse elemento 2D foi tocado ou clicado neste quadro (botões). |
| `raton() → x, y, dentro` | Onde está o mouse. |
| `capturarRaton(bool)`, `ratonRel() → dx, dy`, `ratonCapturado()` | Mouse capturado para olhar com o mouse (jogos em primeira pessoa). No PC e no navegador; no navegador, o mouse é capturado com o primeiro clique ou tecla no jogo. <kbd>Esc</kbd> o solta e pausa o jogo. |
