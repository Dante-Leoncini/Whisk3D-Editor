# Gráficos

## A API

Tudo o que é desenhado passa pelas funções de `w3dEngine` (`graphics/w3dGraphics.h`): ligar e desligar capacidades (`Enable`, `Disable`), matrizes (`MatrixMode`, `PushMatrix`, `Translatef`, `Perspective`), arrays de vértices (`VertexPointer3f`, `NormalPointer3b`, `TexCoordPointer2f`), buffers na GPU (`GenBuffer`, `ArrayBufferData`), o desenho (`DrawTriangles`, `DrawLines`, `DrawPoints`), materiais (`Material`), luzes (`LightNfv`, `LightNSpot`), névoa, mesclagem, recorte por retângulo, estêncil e planos de recorte.

Ela se parece com o OpenGL clássico de propósito: é a forma de desenhar que mais sistemas entendem, do N95 ao Dreamcast.

## Pipeline fixo e shaders

Há duas famílias de GPU:

- **Pipeline fixo.** A GPU traz as etapas prontas de fábrica: transformar os vértices, calcular a luz por vértice, combinar a textura, aplicar a névoa. O programa só liga interruptores e passa valores. É assim que funcionam o OpenGL ES 1.1 (o N95), o OpenGL clássico do PC, o GE do PSP e a PVR do Dreamcast.
- **Shaders.** A GPU não traz nada pronto: cada etapa é um pequeno programa que roda nela. É assim que funcionam o OpenGL ES 2.0, o WebGL e o Vulkan.

O Core fala a língua do pipeline fixo. O backend de shaders **emula** esse pipeline com um "uber-shader" que faz as mesmas contas que o OpenGL ES 1.1 (luz por vértice com até 8 luzes, atenuação, cone, névoa, a mistura de duas texturas). Por isso, o navegador e o Android ficam iguais ao PC e ao N95.

## Os backends

| Backend | O que é | Onde é usado |
|---|---|---|
| `opengl/` | Pipeline fixo: OpenGL de desktop, OpenGL ES 1.1, GLdc, vitaGL | Linux, Windows, macOS, N95 e S60, Dreamcast, PS Vita |
| `gles2/` | Shaders: OpenGL ES 2.0 e WebGL | Android, o navegador, o Nokia N8 (Symbian^3), o PC (opcional) |
| `vulkan/` | O mesmo de shaders, compilado sobre um OpenGL ES 2.0 feito com Vulkan | O PC (opcional) |
| `gu/` | O GE do PSP | PSP |
| `citro3d/` | A GPU PICA200 do 3DS | Nintendo 3DS |
| `pvr/` | Desenho direto na PVR do Dreamcast | Módulo opcional para jogos nativos |
| `video/` | Vídeo para textura | Opcional: FFmpeg no desktop, `<video>` no navegador |

## Como é escolhido

Quase sempre, **ao compilar**: cada sistema compila um único backend e as chamadas vão direto, sem nenhum custo extra.

A exceção é o PC (Windows e Linux). Ali o executável traz OpenGL, OpenGL ES 2.0 e Vulkan, e a escolha é feita **ao iniciar**, em **Propriedades → Ajustes → Gráficos** (vale ao reiniciar; veja [Outras funções](../editor/otras-funciones.md)). Cada backend é compilado no seu próprio namespace (`w3dEngine::gl`, `w3dEngine::es2`, `w3dEngine::vk`) e preenche uma **tabela de despacho** (`w3dBackendTabla.h`) com suas funções. Cada função de `w3dEngine` chama a do backend ativo:

```cpp
// graphics/w3dDespacho.inc, gerado por tools/generar_despacho.py
void Enable(Cap c) { W3D_TABLA()->Enable(c); }
void DrawTriangles(int count, const MeshIndex* indices) { W3D_TABLA()->DrawTriangles(count, indices); }
```

Se o escolhido não funcionar nessa máquina, fica o OpenGL. O Vulkan precisa do driver, da GPU e do compilador de shaders `shaderc_shared` ao lado do executável, e vem desligado nos builds de 32 bits. No macOS, o executável traz só OpenGL.

## A viagem de um `DrawTriangles`

O mesmo pedido termina em lugares muito diferentes:

![Um mesmo gfx::DrawTriangles passa pelo Whisk3D Core e desce até cada backend: opengl (PC, macOS, N95, Dreamcast, PS Vita), gles2 (Android, navegador, N8, PC), vulkan (PC), gu (PSP) e citro3d (3DS)](img/whisk3d-core-despacho.webp)

- **OpenGL fixo (PC, N95):** quase direto para `glDrawElements`. A matriz só é enviada logo antes do desenho, em um único `glLoadMatrixf`.
- **OpenGL ES 2.0 e WebGL:** a pilha de matrizes vive na CPU e viaja como uniforms até o uber-shader. O WebGL não aceita arrays na RAM, então o backend os passa para buffers. Na GPU VideoCore IV do Android 2.3, ele monta um pequeno programa por combinação de estado e junta os desenhos da interface: de 179 por quadro, sobram 16.
- **PSP (GE):** a CPU passa cada vértice para o espaço de recorte e recorta ali, porque o GE não recorta contra as laterais. Depois os escreve intercalados em um anel de memória que o GE lê. A luz (até quatro) e a névoa vão por hardware.
- **3DS (PICA200):** os vértices são copiados para a memória linear e transformados por um vertex shader próprio (`w3d.v.pica`). Os desenhos pequenos seguidos com o mesmo estado saem juntos: de 191 por quadro, sobram 18. As linhas e os pontos são montados na CPU como triângulos.
- **Dreamcast:** o GLdc faz o papel de OpenGL 1.x sobre a PVR e usa o mesmo backend fixo que o PC e o N95, com patches próprios. Para um jogo que precisa de cada ciclo, existe o `pvr/`: o GLdc gasta uns 400 ciclos do SH-4 por vértice, e o desenho direto transforma cada um com uma única instrução (`FTRV`). Os dois não convivem no mesmo programa.
- **PS Vita:** o vitaGL emula o pipeline fixo sobre a GPU do console e compila seus shaders em tempo de execução (por isso precisa do `libshacccg.suprx`).

## Perguntar ao backend o que ele sabe fazer

Nem todas as GPUs conseguem fazer o mesmo, e o Core não esconde isso: cada backend responde o que tem, e quem desenha escolhe outro caminho se faltar algo.

| Pergunta | O que acontece se a resposta for não |
|---|---|
| `VBOSoportado()` | As malhas vão como arrays na RAM (Dreamcast, Vita) |
| `EstencilBits()` | O reflexo da água passa a usar o recorte por planos |
| `PlanosRecorteMax()` | O reflexo se vira sem planos (a PVR não os tem) |
| `MezclaTexturasSoportada()` | As camadas de um material vão em várias passadas |
| `TieneTexGen()` | O reflexo esférico é calculado por software ou com a matriz de textura (`TexMatrixMatcap`, que funciona no N95) |
| `SombreadoPorPixelSoportado()` | A luz fica por vértice e o editor nem oferece a opção |
| `ShadersEscritos()` | O material com shader próprio é desenhado como sempre (veja [Shaders escritos](../editor/shaders.md)) |

## O cache de estado

Cada mudança de estado custa algo ao driver. O Core lembra o que está definido: ligar algo que já está ligado, ou vincular a textura que já está vinculada, não gera nenhuma chamada. `IsEnabled` e `GetMatrix` respondem a partir dessa memória e não perguntam à GPU, porque no N95 uma pergunta assim obriga a esperar que ela termine de desenhar.

## Texturas

`LoadTexture` tem duas etapas. O **decode** (do arquivo para pixels RGBA) depende do sistema: stb em quase todos, o ICL no Symbian. O **upload** (dos pixels para a GPU) é comum, e cada backend converte para o formato do seu chip:

- **PSP:** potência de dois até 512, "swizzled", em 565 ou 8888.
- **3DS:** blocos de 8x8 em ordem Morton, de 8 a 1024 por lado.
- **Dreamcast:** potência de dois de 8 a 1024, em 16 bits (565 ou 4444), sem mipmaps.
- **N95:** só potência de dois; as outras são reamostradas antes do envio.

Um projeto pode pedir texturas de **16 bits** (metade da memória) e, só no N95, enviá-las **com metade do tamanho** a partir de certo tamanho (`texturasMitadDesde`). O formato `.w3dt` guarda uma textura já pronta para a GPU, com seus mipmaps: carregá-la é ler e enviar.

Um detalhe honesto: o envio de texturas e as luzes ainda chamam duas ou três funções do OpenGL diretamente. Nos consoles sem OpenGL, o backend traz uma pequena camada que as implementa (`gu/w3dGLgu.cpp`, `citro3d/w3dGLc3d.cpp`).

## A tela e a área segura

`PantallaSetTam` e `PantallaEsVertical` guardam, em um único lugar, se a tela está em pé ou deitada (horizontal = mais larga que alta), para que qualquer parte do motor possa perguntar. A **área segura** (`SafeAreaGet`) diz quantos pixels de cada borda não servem para a interface: o notch, a barra de gestos, as barras do navegador do celular. O fundo é desenhado na janela inteira, e a interface, dentro da área segura. No PC, no Symbian e nos consoles, as bordas valem 0; no navegador, são medidas.

## Os limites do menor

O que se escreve para o Core tem que caber na máquina mais limitada. Estes são os limites que se notam:

| Limite | Onde | Como o Core lida com ele |
|---|---|---|
| Índices de 16 bits (até 65.535 vértices por malha) | Symbian | `MeshIndex` é de 16 bits ali e de 32 no resto; o importador rejeita o que não cabe |
| 8 luzes ao mesmo tempo | Todos (`LucesMax`) | O backend de shaders as emula com a mesma conta; o PSP faz 4 por hardware e o Android 2.3 usa até 4 |
| Sem shaders | Os backends fixos | As funções de shaders respondem que não e o material é desenhado como sempre |
| Z de 16 bits | PSP | O plano próximo se ajusta à distância de órbita, como no N95 |
| 16 MB para tudo | Dreamcast | Compila-se com `-Os` e o linker descarta o que não é usado; as texturas vão em 16 bits |
| C++03 | Symbian | Nada de `nullptr`, `auto` nem `override` nos headers (`W3D_OVERRIDE`); `tools/prueba_independencia.sh` compila cada header sozinho, em C++03 |
