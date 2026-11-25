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

## O arquivo shortcuts.ini

As mudanças são salvas sozinhas em `res/shortcuts.ini`, ao lado de `config.ini`. Só aparecem as funções alteradas, e dá para editá-lo à mão:

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

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em shortcuts.ini |
|---|---|---|---|
| Salvar projeto | <kbd>Ctrl+S</kbd> | — | `global.save` |
| Desfazer | <kbd>Ctrl+Z</kbd> | <kbd>C+Esq</kbd> | `global.undo` |
| Refazer | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Dir</kbd> | `global.redo` |
| Modo Objeto / Edição | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.mode` |
| Ativar/desativar ímã | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Menu de ímã (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Reproduzir / Pausa | <kbd>Space</kbd> | <kbd>Verde+Soft esq</kbd> | `global.play` |
| Barra de menu | <kbd>M</kbd> | <kbd>Soft esq</kbd> | `global.menuBar` |
| Pai | <kbd>Ctrl+P</kbd> | — | `global.parent` |
| Desparentear | <kbd>Alt+P</kbd> | — | `global.clearParent` |
| Tela cheia | <kbd>F11</kbd> | — | `global.screen` |
| Renderizar Imagem | <kbd>F12</kbd> | — | `global.render` |
| Renderizar Animação | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### Viewport 3D

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em shortcuts.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | <kbd>Lápis+Cima</kbd> | `3d.selectAll` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | <kbd>Lápis+Cima</kbd> | `3d.deselect` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `3d.invert` |
| Seleção por caixa | <kbd>B</kbd> | — | `3d.box` |
| Selecionar Vinculado | <kbd>L</kbd> | — | `3d.linked` |
| Adicionar | <kbd>Shift+A</kbd> | — | `3d.add` |
| Aplicar | <kbd>Ctrl+A</kbd> | — | `3d.apply` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `3d.move` |
| Rotacionar | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotate` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `3d.scale` |
| Limpar posição (Pose) | <kbd>Alt+G</kbd> | — | `3d.clearPos` |
| Limpar rotação (Pose) | <kbd>Alt+R</kbd> | — | `3d.clearRot` |
| Engordar / Limpar escala | <kbd>Alt+S</kbd> | — | `3d.clearScale` |
| Duplicar | <kbd>Shift+D</kbd> | — | `3d.duplicate` |
| Duplicar Vinculado | <kbd>Alt+D</kbd> | — | `3d.instance` |
| Excluir | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.delete` |
| Ocultar / Mostrar | <kbd>H</kbd> | — | `3d.hide` |
| Juntar | <kbd>Ctrl+J</kbd> | — | `3d.join` |
| Inserir Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Inserir keyframe (tudo) | <kbd>Shift+I</kbd> | — | `3d.keyframeAll` |
| Edição Proporcional | <kbd>O</kbd> | — | `3d.proportional` |
| Sobreposições | <kbd>K</kbd> | — | `3d.overlays` |
| Vista local | <kbd>/</kbd> | — | `3d.viewLocal` |
| Enquadrar Seleção | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.focus` |
| Vista frontal | <kbd>Num1</kbd> | <kbd>#+Esq/Dir</kbd> | `3d.viewFront` |
| Vista traseira | <kbd>Ctrl+Num1</kbd> | <kbd>#+Esq/Dir</kbd> | `3d.viewBack` |
| Vista direita | <kbd>Num3</kbd> | <kbd>#+Esq/Dir</kbd> | `3d.viewRight` |
| Vista esquerda | <kbd>Ctrl+Num3</kbd> | <kbd>#+Esq/Dir</kbd> | `3d.viewLeft` |
| Vista superior | <kbd>Num7</kbd> | <kbd>#+Cima</kbd> | `3d.viewTop` |
| Vista inferior | <kbd>Ctrl+Num7</kbd> | <kbd>#+Baixo</kbd> | `3d.viewBottom` |
| Perspectiva / Ortográfica | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspective` |
| Girar vista à esquerda | <kbd>Num4</kbd> | — | `3d.rollLeft` |
| Girar vista à direita | <kbd>Num6</kbd> | — | `3d.rollRight` |
| Vista da câmera | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.viewCamera` |
| Definir câmera ativa | <kbd>Ctrl+Num0</kbd> | — | `3d.activeCamera` |
| Extrudar | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extrude` |
| Nova face a partir de vértices | <kbd>F</kbd> | — | `3d.face` |
| Recalcular Normais | <kbd>Shift+N</kbd> | — | `3d.normals` |
| Loop Cut e Deslizar | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Conectar caminho de vértices | <kbd>J</kbd> | — | `3d.connect` |
| Rasgar | <kbd>V</kbd> | — | `3d.rip` |
| Deslizar vértices | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separar | <kbd>P</kbd> | — | `3d.separate` |
| Mesclar | <kbd>M</kbd> | — | `3d.merge` |
| Menu de contexto | <kbd>W</kbd> | — | `3d.contextMenu` |
| Triangular | <kbd>Ctrl+T</kbd> | — | `3d.triangulate` |
| Menu UV | <kbd>U</kbd> | — | `3d.menuUV` |

### Durante uma transformação (mover / rotacionar / escalar)

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em shortcuts.ini |
|---|---|---|---|
| Confirmar | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirm` |
| Cancelar | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancel` |
| Eixo X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.axisX` |
| Eixo Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.axisY` |
| Eixo Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.axisZ` |
| Excluir X (plano) | <kbd>Shift+X</kbd> | — | `xf.planeX` |
| Excluir Y (plano) | <kbd>Shift+Y</kbd> | — | `xf.planeY` |
| Excluir Z (plano) | <kbd>Shift+Z</kbd> | — | `xf.planeZ` |
| Raio proporcional + | <kbd>PageUp</kbd> | — | `xf.radiusPlus` |
| Raio proporcional - | <kbd>PageDown</kbd> | — | `xf.radiusMinus` |

### Editor UV

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em shortcuts.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | — | `uv.selectAll` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | — | `uv.deselect` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `uv.invert` |
| Selecionar Vinculado | <kbd>L</kbd> | — | `uv.linked` |
| Seleção por caixa | <kbd>B</kbd> | — | `uv.box` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `uv.move` |
| Rotacionar | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotate` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `uv.scale` |
| Inserir Keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Inserir keyframe (tudo) | <kbd>Shift+I</kbd> | — | `uv.keyframeAll` |
| Extrudar | <kbd>E</kbd> | — | `uv.extrude` |
| Duplicar | <kbd>Shift+D</kbd> | — | `uv.duplicate` |
| Excluir | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.delete` |
| Enquadrar Seleção | <kbd>Num.</kbd> | — | `uv.focus` |

### Linha do Tempo

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em shortcuts.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | — | `tl.selectAll` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | — | `tl.deselect` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `tl.invert` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `tl.move` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `tl.scale` |
| Rotacionar alças | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotate` |
| Duplicar | <kbd>Shift+D</kbd> | — | `tl.duplicate` |
| Excluir | <kbd>X</kbd> | — | `tl.delete` |
| Interpolação | <kbd>T</kbd> | — | `tl.interp` |
| Tipo de alça | <kbd>V</kbd> | — | `tl.handle` |
| Enquadrar Seleção | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.focus` |
| Quadro anterior | <kbd>Left</kbd> | — | `tl.prevFrame` |
| Próximo quadro | <kbd>Right</kbd> | — | `tl.nextFrame` |
| Keyframe anterior | <kbd>Down</kbd> | — | `tl.prevKey` |
| Próximo keyframe | <kbd>Up</kbd> | — | `tl.nextKey` |
| Ir para o início | <kbd>Home</kbd> | — | `tl.start` |
| Ir para o fim | <kbd>End</kbd> | — | `tl.end` |

### Outliner

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em shortcuts.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | — | `out.selectAll` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | — | `out.deselect` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `out.invert` |
| Mover (reordenar) | <kbd>G</kbd> | <kbd>1</kbd> | `out.move` |
| Renomear | <kbd>F2</kbd> | — | `out.rename` |
| Duplicar | <kbd>Shift+D</kbd> | — | `out.duplicate` |
| Duplicar Vinculado | <kbd>Alt+D</kbd> | — | `out.instance` |
| Ocultar / Mostrar | <kbd>H</kbd> | <kbd>2</kbd> | `out.hide` |
| Excluir | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.delete` |
| Mostrar o ativo | <kbd>Num.</kbd> | — | `out.focus` |

### Editor 2D

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em shortcuts.ini |
|---|---|---|---|
| Selecionar tudo | <kbd>A</kbd> | — | `2d.selectAll` |
| Desselecionar tudo | <kbd>Alt+A</kbd> | — | `2d.deselect` |
| Inverter seleção | <kbd>Ctrl+I</kbd> | — | `2d.invert` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `2d.move` |
| Rotacionar | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotate` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `2d.scale` |
| Duplicar | <kbd>Shift+D</kbd> | — | `2d.duplicate` |
| Excluir | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.delete` |
| Enquadrar Seleção | <kbd>Num.</kbd> | — | `2d.focus` |
| Vista de jogo | <kbd>Num0</kbd> | — | `2d.gameView` |

### Propriedades (passos de uma rotina)

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em shortcuts.ini |
|---|---|---|---|
| Pegar passo | <kbd>G</kbd> | — | `props.grab` |
| Apagar passo | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.delete` |
| Inserir Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Editor de scripts

| Função | Atalho padrão | Nokia N95 (teclado do celular) | Nome em shortcuts.ini |
|---|---|---|---|
| Salvar script | <kbd>Ctrl+S</kbd> | — | `ide.save` |
| Desfazer | <kbd>Ctrl+Z</kbd> | — | `ide.undo` |
| Copiar | <kbd>Ctrl+C</kbd> | — | `ide.copy` |
| Cortar | <kbd>Ctrl+X</kbd> | — | `ide.cut` |
| Colar | <kbd>Ctrl+V</kbd> | — | `ide.paste` |
| Selecionar tudo | <kbd>Ctrl+A</kbd> | — | `ide.all` |
| Refazer | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.redo` |
| Localizar | <kbd>Ctrl+F</kbd> | — | `ide.find` |
| Substituir | <kbd>Ctrl+H</kbd> | — | `ide.replace` |

<!--ATAJOS-FIN-->

## Teclas fixas (não reatribuíveis)

Algumas teclas não estão na lista porque dependem do que está sendo feito: as setas ao navegar por menus e painéis, <kbd>Enter</kbd> e <kbd>Esc</kbd> em menus e janelas, as setas do Outliner enquanto uma linha é movida, o teclado do IDE ao digitar, e o teclado do celular no N95 (veja [Navegação](navegacion.md)).
