# Atalhos de teclado

Todas as funções das tabelas abaixo podem ser reatribuídas, e cada uma pode ter **uma ou várias** combinações (ou nenhuma). Funciona no PC, no Android (com teclado físico) e na Web. No N95 a janela não consegue capturar teclas: lá o arquivo `atajos.ini` é editado à mão.

## Como mudar um atalho

1. Abrir **Propriedades → aba Arquivo/Render → cartão Ajustes → Atalhos de teclado…**
2. Escolher o **Contexto** (Geral, Viewport 3D, Durante uma transformação, Editor UV, Linha do Tempo, Outliner, Editor 2D, Propriedades, Editor de scripts). As setas esquerda/direita também o mudam.
3. Escolher a função na lista e tocar em **Adicionar atalho** (ou tocar de novo na linha escolhida). Aparece "Pressione uma tecla…".
4. Pressionar a combinação nova (por exemplo <kbd>Ctrl+M</kbd>). <kbd>Esc</kbd> cancela sem mudar nada.

| Botão | O que faz |
|---|---|
| Adicionar atalho | Adiciona uma combinação à função. Se outra função do mesmo contexto a tinha, ela é tirada de lá, com um aviso. |
| Limpar | Deixa a função sem atalho. |
| Redefinir | Volta a função ao seu atalho padrão. |
| Restaurar tudo | Todas as funções voltam ao padrão. |

Os menus mostram, ao lado de cada opção, o atalho **configurado**: se você reatribuir uma função, o menu mostra a combinação nova. No N95, os menus mostram a tecla do celular (por exemplo <kbd>1</kbd> para Mover ou <kbd>C</kbd> para Excluir); se houver um teclado Bluetooth conectado, mostram a tecla desse teclado quando a reconhecem.

As combinações alteradas aparecem em verde. Quando um atalho padrão é tirado da sua função, essa tecla fica livre: não faz mais nada nesse contexto.

Os atalhos de cada contexto valem quando o ponteiro está sobre esse painel; os de **Geral** valem em todos. Os de **Durante uma transformação** valem enquanto algo é movido, rotacionado ou escalado, e têm prioridade. No editor de scripts, as letras sempre escrevem: lá só dá para reatribuir combinações com <kbd>Ctrl</kbd>, <kbd>Alt</kbd> ou teclas F.

## O arquivo atajos.ini

As mudanças são salvas sozinhas em `res/atajos.ini`, ao lado de `config.ini`. Só aparecem as funções alteradas, e dá para editá-lo à mão:

```ini
# funcao = combinacao, outra combinacao   (vazio = sem atalho)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

Nomes de teclas: letras e números tal como são, `Enter`, `Esc`, `Tab`, `Space`, `Backspace`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`, `F1`…`F12`, `Num0`…`Num9`, `Num.`, `NumEnter`, `Comma`, `Plus`. Modificadores: `Ctrl+`, `Shift+`, `Alt+`.

## Atalhos padrão

<!-- As tabelas abaixo, até ATAJOS-FIN, são geradas por tools/genmanual_atajos.py a partir do código: não as edite à mão. -->

<!--ATAJOS-INICIO-->

### Geral (qualquer painel)

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em atajos.ini |
|---|---|---|---|
| Salvar projeto | <kbd>Ctrl+S</kbd> | — | `global.guardar` |
| Desfazer | <kbd>Ctrl+Z</kbd> | <kbd>C+Esq</kbd> | `global.deshacer` |
| Refazer | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Dir</kbd> | `global.rehacer` |
| Modo Objeto / Edição | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.modo` |
| Ativar/desativar ímã | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Menu de ímã (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Reproduzir / Pausa | <kbd>Space</kbd> | <kbd>Verde+Soft esq</kbd> | `global.play` |
| Barra de menu | <kbd>M</kbd> | <kbd>Soft esq</kbd> | `global.barraMenu` |
| Pai | <kbd>Ctrl+P</kbd> | — | `global.emparentar` |
| Desparentear | <kbd>Alt+P</kbd> | — | `global.desemparentar` |
| Tela cheia | <kbd>F11</kbd> | — | `global.pantalla` |
| Renderizar Imagem | <kbd>F12</kbd> | — | `global.render` |
| Renderizar Animação | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### Viewport 3D

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em atajos.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | <kbd>Lápis+Cima</kbd> | `3d.seleccionarTodo` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | <kbd>Lápis+Cima</kbd> | `3d.deseleccionar` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Seleção por caixa | <kbd>B</kbd> | — | `3d.caja` |
| Selecionar Vinculado | <kbd>L</kbd> | — | `3d.vinculado` |
| Adicionar | <kbd>Shift+A</kbd> | — | `3d.agregar` |
| Aplicar | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `3d.mover` |
| Rotacionar | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotar` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `3d.escalar` |
| Limpar posição (Pose) | <kbd>Alt+G</kbd> | — | `3d.limpiarPos` |
| Limpar rotação (Pose) | <kbd>Alt+R</kbd> | — | `3d.limpiarRot` |
| Engordar / Limpar escala | <kbd>Alt+S</kbd> | — | `3d.limpiarEsc` |
| Duplicar | <kbd>Shift+D</kbd> | — | `3d.duplicar` |
| Duplicar Vinculado | <kbd>Alt+D</kbd> | — | `3d.instancia` |
| Excluir | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.borrar` |
| Ocultar / Mostrar | <kbd>H</kbd> | — | `3d.ocultar` |
| Juntar | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Inserir Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Inserir keyframe (tudo) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Edição Proporcional | <kbd>O</kbd> | — | `3d.proporcional` |
| Sobreposições | <kbd>K</kbd> | — | `3d.overlays` |
| Vista local | <kbd>/</kbd> | — | `3d.vistaLocal` |
| Enquadrar Seleção | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.encuadrar` |
| Vista frontal | <kbd>Num1</kbd> | <kbd>#+Esq/Dir</kbd> | `3d.vistaFrente` |
| Vista traseira | <kbd>Ctrl+Num1</kbd> | <kbd>#+Esq/Dir</kbd> | `3d.vistaAtras` |
| Vista direita | <kbd>Num3</kbd> | <kbd>#+Esq/Dir</kbd> | `3d.vistaDerecha` |
| Vista esquerda | <kbd>Ctrl+Num3</kbd> | <kbd>#+Esq/Dir</kbd> | `3d.vistaIzquierda` |
| Vista superior | <kbd>Num7</kbd> | <kbd>#+Cima</kbd> | `3d.vistaArriba` |
| Vista inferior | <kbd>Ctrl+Num7</kbd> | <kbd>#+Baixo</kbd> | `3d.vistaAbajo` |
| Perspectiva / Ortográfica | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspectiva` |
| Girar vista à esquerda | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Girar vista à direita | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Vista da câmera | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Definir câmera ativa | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Extrudar | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| Nova face a partir de vértices | <kbd>F</kbd> | — | `3d.cara` |
| Recalcular Normais | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Loop Cut e Deslizar | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Conectar caminho de vértices | <kbd>J</kbd> | — | `3d.conectar` |
| Rasgar | <kbd>V</kbd> | — | `3d.rip` |
| Deslizar vértices | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separar | <kbd>P</kbd> | — | `3d.separar` |
| Mesclar | <kbd>M</kbd> | — | `3d.merge` |
| Menu de contexto | <kbd>W</kbd> | — | `3d.menuContexto` |
| Triangular | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| Menu UV | <kbd>U</kbd> | — | `3d.menuUV` |

### Durante uma transformação (mover / rotacionar / escalar)

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em atajos.ini |
|---|---|---|---|
| Confirmar | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirmar` |
| Cancelar | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancelar` |
| Eixo X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.ejeX` |
| Eixo Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.ejeY` |
| Eixo Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.ejeZ` |
| Excluir X (plano) | <kbd>Shift+X</kbd> | — | `xf.planoX` |
| Excluir Y (plano) | <kbd>Shift+Y</kbd> | — | `xf.planoY` |
| Excluir Z (plano) | <kbd>Shift+Z</kbd> | — | `xf.planoZ` |
| Raio proporcional + | <kbd>PageUp</kbd> | — | `xf.radioMas` |
| Raio proporcional - | <kbd>PageDown</kbd> | — | `xf.radioMenos` |

### Editor UV

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em atajos.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | — | `uv.seleccionarTodo` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | — | `uv.deseleccionar` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `uv.invertir` |
| Selecionar Vinculado | <kbd>L</kbd> | — | `uv.vinculado` |
| Seleção por caixa | <kbd>B</kbd> | — | `uv.caja` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `uv.mover` |
| Rotacionar | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotar` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `uv.escalar` |
| Inserir Keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Inserir keyframe (tudo) | <kbd>Shift+I</kbd> | — | `uv.keyframeTodo` |
| Extrudar | <kbd>E</kbd> | — | `uv.extruir` |
| Duplicar | <kbd>Shift+D</kbd> | — | `uv.duplicar` |
| Excluir | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.borrar` |
| Enquadrar Seleção | <kbd>Num.</kbd> | — | `uv.encuadrar` |

### Linha do Tempo

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em atajos.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Rotacionar alças | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Duplicar | <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Excluir | <kbd>X</kbd> | — | `tl.borrar` |
| Interpolação | <kbd>T</kbd> | — | `tl.interp` |
| Tipo de alça | <kbd>V</kbd> | — | `tl.handle` |
| Enquadrar Seleção | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Quadro anterior | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Próximo quadro | <kbd>Right</kbd> | — | `tl.frameSig` |
| Keyframe anterior | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Próximo keyframe | <kbd>Up</kbd> | — | `tl.keySig` |
| Ir para o início | <kbd>Home</kbd> | — | `tl.inicio` |
| Ir para o fim | <kbd>End</kbd> | — | `tl.fin` |

### Outliner

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em atajos.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | — | `out.seleccionarTodo` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | — | `out.deseleccionar` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `out.invertir` |
| Mover (reordenar) | <kbd>G</kbd> | <kbd>1</kbd> | `out.mover` |
| Renomear | <kbd>F2</kbd> | — | `out.renombrar` |
| Duplicar | <kbd>Shift+D</kbd> | — | `out.duplicar` |
| Duplicar Vinculado | <kbd>Alt+D</kbd> | — | `out.instancia` |
| Ocultar / Mostrar | <kbd>H</kbd> | <kbd>2</kbd> | `out.ocultar` |
| Excluir | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.borrar` |
| Mostrar o ativo | <kbd>Num.</kbd> | — | `out.encuadrar` |

### Editor 2D

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em atajos.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | — | `2d.seleccionarTodo` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | — | `2d.deseleccionar` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `2d.invertir` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `2d.mover` |
| Rotacionar | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotar` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `2d.escalar` |
| Duplicar | <kbd>Shift+D</kbd> | — | `2d.duplicar` |
| Excluir | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.borrar` |
| Enquadrar Seleção | <kbd>Num.</kbd> | — | `2d.encuadrar` |
| Vista de jogo | <kbd>Num0</kbd> | — | `2d.vistaJuego` |

### Propriedades (passos de uma rotina)

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em atajos.ini |
|---|---|---|---|
| Pegar passo | <kbd>G</kbd> | — | `props.agarrar` |
| Apagar passo | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Inserir Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Editor de scripts

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em atajos.ini |
|---|---|---|---|
| Salvar script | <kbd>Ctrl+S</kbd> | — | `ide.guardar` |
| Desfazer | <kbd>Ctrl+Z</kbd> | — | `ide.deshacer` |
| Copiar | <kbd>Ctrl+C</kbd> | — | `ide.copiar` |
| Cortar | <kbd>Ctrl+X</kbd> | — | `ide.cortar` |
| Colar | <kbd>Ctrl+V</kbd> | — | `ide.pegar` |
| Selecionar tudo | <kbd>Ctrl+A</kbd> | — | `ide.todo` |
| Refazer | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.rehacer` |
| Localizar | <kbd>Ctrl+F</kbd> | — | `ide.buscar` |
| Substituir | <kbd>Ctrl+H</kbd> | — | `ide.reemplazar` |

<!--ATAJOS-FIN-->

## Teclas fixas (não reatribuíveis)

Algumas teclas não estão na lista porque dependem do que está sendo feito: as setas ao navegar por menus e painéis, <kbd>Enter</kbd> e <kbd>Esc</kbd> em menus e janelas, as setas do Outliner enquanto uma linha é movida, o teclado do IDE ao digitar, e o teclado do celular no N95 (veja [Navegação](navegacion.md)).
