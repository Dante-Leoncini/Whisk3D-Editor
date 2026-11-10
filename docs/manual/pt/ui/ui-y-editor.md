# O que é da UI e o que é do editor

O editor é feito com o Whisk3D UI, mas muito do que se vê nele é dele mesmo. O que é da UI está em `libs/WhiskUI/`; o que é do editor, em `main/` do repositório do Whisk3D.

## Da UI

- O desenho 2D, os cartões com borda e os retângulos.
- A tabela de ícones e o empacotador de atlas.
- A fonte bitmap, o atlas estendido e o texto com fontes TTF bakeadas.
- A paleta de cores e as medidas, com sua escala.
- Os botões, as abas, as caixas de seleção e os menus suspensos.
- Os cartões de propriedades e suas linhas, com o losango de animação.
- O estado das caixas de texto e o seu desenho.

## Do editor

| O quê | Onde |
|---|---|
| As viewports: dividir a tela, suas bordas, suas barras de menus e de ferramentas | `main/ui/ViewPorts/` (`ViewPorts.cpp`, `WithBorder.cpp`, `ToolbarBase.cpp`) |
| O painel de propriedades: quais cartões existem, suas abas, o clique e o arraste sobre as linhas | `main/ui/ViewPorts/Properties.cpp` |
| As janelas modais: o seletor de cor, o explorador de arquivos, as confirmações, a barra de progresso, os teclados na tela | `main/ui/ViewPorts/PopUp/` |
| As notificações | `main/ui/ViewPorts/Notificaciones.cpp` |
| As barras de rolagem dos painéis | `main/ui/ViewPorts/ScrollBar.cpp` |
| As dicas de ajuda. O texto fica guardado em `Button::ayuda` | `W3dAyudaRender`, em `main/ui/ViewPorts/LayoutInput.cpp` |
| Que tecla, clique ou toque vai para cada widget, e as teclas do N95 | `main/ui/ViewPorts/LayoutInput.cpp`, `main/app/controles.cpp`, `platform/symbian/src/` |
| As funções de foco das caixas de texto | `main/ui/ViewPorts/NumInput.cpp` |
| As traduções, `T()`. A UI só tem o gancho `W3dMenuTraducir` | `main/config/W3dLang.cpp` |
| A inicialização da UI | `main/app/W3dInitUI.cpp` |
| Montar o atlas do skin com os ícones avulsos | `main/io/SkinAtlas.cpp` |
| Recalcular a escala | `SetGlobalScale`, em `main/ui/ViewPorts/ViewPorts.cpp` |
| Ler o `skin.ini` no PC e no Android | `loadColors`, em `main/app/main.cpp` |
| As cores próprias do editor: o chão, os eixos, as normais | `main/ui/W3dColors.cpp` |
| Os arquivos do skin: `font.png`, `font-ext.png`, `atlas/iconos/`, `skin.ini` | `res/Skins/Whisk3D/` |
| O gerador do atlas estendido | `tools/fuente_ext.py` |
| O [Editor 2D](../editor/editor-2d.md) e as interfaces `.w3dui` dos jogos: textos, imagens, retângulos, contêineres, slice 9, botões e vídeos | `main/objects/` (`UI.h`, `Texto2D.h`, `Boton2D.h`...), `main/render/UIOverlay.cpp`, `main/io/UI2DFormato.cpp`, `main/io/Fuente2D.cpp`, `main/ui/ViewPorts/Editor2D.cpp` |

O Editor 2D desenha o texto com `W3dTextAtlas`, que é da UI. Bakear uma TTF em tempo de execução, com stb_truetype, é do editor (`main/io/Fuente2D.cpp`).

## Na UI, mas do editor

Coisas que estão em `libs/WhiskUI/` e ainda são do editor:

- `PropListMeshParts` lê malhas, esqueletos, animações, restrições e scripts, e chama `RebindMaterialMeshPart()` do painel de propriedades.
- `PropColor` guarda a paleta e o objeto de onde vem a cor (`palRef`, `palObj`), para o seletor de cor.
- `GroupProperties` é a lista global de cartões do painel de propriedades.
- `core/UI.h` tem estado do editor: o modo de visualização touch do Modo Edição (`g_viewEditMode`), o movimento do mouse (`dx`, `dy`), as teclas Alt, Shift e Ctrl (`LAltPressed`, `LShiftPressed`, `LCtrlPressed`) e as medidas das viewports (`paddingViewport`, `MinViewportHeight`, `MinViewportWidth`).
- A maioria dos ícones são ferramentas do editor.

O que falta separar, e o estado da biblioteca: o final de [Usar a UI diretamente](empezar.md).
