# Usar a UI diretamente

Para quem programa em C ou C++ e quer a interface sem o editor. É preciso:

- **O Core, com seu modelo de cena.** A UI desenha com `w3dEngine` e algumas linhas leem objetos da cena (`objects/`, `animation/`, `script/`). Veja [Usar o Core diretamente](../core/empezar.md).
- **O código-fonte da UI.** Todos os `.cpp` de `libs/WhiskUI/`. Os diretórios de includes são `libs/`, para `"WhiskUI/draw/icons.h"`, e as subpastas `widgets/`, `text/`, `draw/`, `theme/` e `core/`, porque lá dentro os arquivos se incluem entre si sem a pasta (`"icons.h"`, `"card.h"`).
- **Os arquivos do skin.** `font.png` (as letras e as bordas dos cartões), `font-ext.png` (o atlas estendido), `skin.ini` (as cores) e, para os ícones novos, a pasta `atlas/iconos/`. Eles não vêm com a biblioteca: estão no repositório do editor, em `res/Skins/Whisk3D/`.

## Inicializar

O mesmo que o editor faz ao iniciar, sem o que é dele:

```cpp
#include "w3dTexture.h"
#include "WhiskUI/core/UI.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/font.h"
#include "WhiskUI/text/W3dFont.h"

loadColorsW3d("Skins/Whisk3D/skin.ini");    // sem o arquivo, ficam as cores do código

unsigned int atlas = 0; int w = 0, h = 0;
w3dEngine::LoadTexture("Skins/Whisk3D/font.png", atlas, &w, &h);

CalcCardUV(w, h);          // as bordas arredondadas
CrearIconos(w, h);         // os ícones que estão dentro de font.png
W3dFontSetTexSize(w, h);
SetIconScale(GlobalScale);

WhiskFont = new Font(w, h, atlas);
WhiskFont->SetScale(GlobalScale);
WhiskFont->SetAtlasExtendido("Skins/Whisk3D/font-ext.png");   // só é carregado quando for necessário
```

`GlobalScale` começa em 1. Para outra escala, veja [Cores e escala](tema.md). Para os ícones avulsos de `atlas/iconos/`, veja [Desenho e atlas](dibujo.md).

Cada vez que o tamanho da janela muda:

```cpp
W3dPantallaAlto = alto;    // para converter o Y da interface para o do Scissor e do Viewport
MenuPantallaW = ancho;     // para que os menus não saiam da tela
MenuPantallaH = alto;
```

## O estado 2D

A UI desenha em pixels, com o Y para baixo e a origem no canto superior esquerdo. Cada widget é desenhado na origem da matriz atual: ele é posicionado com `Translatef`. Antes de desenhar:

```cpp
namespace gfx = w3dEngine;

gfx::Viewport(0, 0, ancho, alto);
gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
gfx::Ortho(0, ancho, alto, 0, -1, 1);       // Y para baixo
gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();
gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
gfx::Disable(gfx::Lighting);  gfx::Disable(gfx::Fog);
gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
gfx::DisableArray(gfx::NormalArray);
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(atlas);
gfx::TexFilter(false);                       // pixels nítidos
gfx::Enable(gfx::Blend); gfx::BlendAlpha();
```

## Um cartão

Um cartão com um número, uma caixa de seleção e um botão. Cada linha aponta para uma variável sua:

```cpp
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"

static float volumen = 0.8f;
static bool  pantallaCompleta = false;
static void Salir() { /* ... */ }

GroupPropertie* opciones = new GroupPropertie("Opciones");

PropFloat* vol = new PropFloat("Volumen");
vol->value = &volumen;
vol->SetRango(0.0f, 1.0f);
opciones->properties.push_back(vol);

PropBool* pc = new PropBool("Pantalla completa");
pc->value = &pantallaCompleta;
opciones->properties.push_back(pc);

PropButton* salir = new PropButton("Salir", IconType::borrar);
salir->action = Salir;
opciones->properties.push_back(salir);

opciones->Resize(200 * GlobalScale, 0);    // a altura é calculada pelo cartão

// a cada quadro, com o estado 2D definido
gfx::PushMatrix();
gfx::Translatef(10, 10, 0);
opciones->Render();
gfx::PopMatrix();
```

A UI não lê o teclado nem o mouse: é você quem os passa para ela. Com o teclado:

```cpp
// seta para baixo
if (opciones->editando) opciones->button_down();   // muda o valor
else opciones->NextSelect();                        // a linha de baixo
// OK ou Enter: no título, recolhe o cartão; numa linha, a edita ou a pressiona
opciones->EnterPropertieSelect();
// Cancelar: deixa a linha como estava
opciones->Cancel();
```

Com o mouse, a linha sob o ponteiro é você quem calcula: veja [Cartões e propriedades](propiedades.md). Os menus, sim, recebem coordenadas: veja [Menus suspensos](menus.md).

## O estado atual, sem rodeios

- **Não está pronta para produção.** O `README.md` do repositório diz isso, e é verdade: ela tem código do editor dentro, que está sendo separado.
- **Não há um exemplo que a use sozinha.** Os exemplos de `ui/` de [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) são interfaces `.w3dui` com Lua que o runtime do jogo roda: isso é do editor. O código desta página foi montado com as mesmas chamadas que o editor usa, não vem de um exemplo testado.
- **Hoje ela não linka sozinha.** `PropList.cpp` chama `RebindMaterialMeshPart()`, que está no painel de propriedades do editor. As funções de foco de `TextField.h` (`TextFieldEnfocar`, `TextFieldCancelar`, `TextFieldInputChar`, `TextFieldFinDeCuadro`) estão definidas no editor, em `main/ui/ViewPorts/NumInput.cpp`.
- **Depende do modelo de cena do Core.** `PropertieBase.h` inclui `objects/Objects.h`, e a lista de propriedades lê malhas, esqueletos, animações e scripts.
- **A escala é recalculada pelo editor.** `SetGlobalScale` está em `main/ui/ViewPorts/ViewPorts.cpp`. Com a UI sozinha, é preciso recalcular as medidas à mão ([Cores e escala](tema.md)).
- **Tudo é global.** Há uma fonte (`WhiskFont`), uma tabela de ícones (`IconsUV`), um menu aberto (`MenuAbierto`) e uma caixa de texto em foco (`g_textFieldActivo`) por programa.
- **Os headers mostram tipos do OpenGL** (`GLfloat`, `GLshort`, `GLuint`), embora já não chamem o OpenGL.
- **O que vem por aí,** segundo o `README.md`: limpar o código que é do editor e fazer exemplos de uso.

O que é da UI e o que é do editor, item por item: [O que é da UI e o que é do editor](ui-y-editor.md). As dúvidas, no grupo do [Telegram](https://t.me/Whisk3D).
