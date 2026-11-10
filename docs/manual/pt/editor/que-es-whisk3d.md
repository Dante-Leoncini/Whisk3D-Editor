# O que é o Whisk3D

O Whisk3D é um **editor 3D e motor de jogos** livre, feito do zero. A ideia é poder criar o que você quiser com o que você tiver: funciona em um PC, em um telefone e também em consoles e aparelhos antigos, sem depender de hardware moderno nem de software proprietário.

## O que dá para fazer

Com o **Whisk3D Editor** você pode:

- **Modelar** em 3D: malhas com vértices, arestas e faces, e [modificadores](objeto-malla.md#a-aba-modificadores) que trabalham sem destruir a malha.
- Fazer o **rig**: montar a [armação](esqueletos.md) de um personagem e pintar o quanto cada osso move.
- **Animar**: keyframes, o editor de curvas e animações por armação ([Linha do Tempo](linea-de-tiempo.md)).
- **Iluminar** a cena com luzes pontuais, de sol e focos.
- **Renderizar**, em tempo real ou com **ray tracing** (sombras suaves, reflexos e transparências), imagens e vídeos.
- **Pintar** as cores dos vértices e as texturas, diretamente sobre o modelo ([Modos de pintura](pintura.md)).
- **Criar jogos**: a lógica é escrita em [Lua](../lua/README.md), com o [editor de scripts](editor-de-scripts.md) integrado; as interfaces (menus, HUD) são montadas no [Editor 2D](editor-2d.md), e o jogo é compilado a partir do próprio editor para cada sistema.
- Importar e exportar modelos em OBJ, FBX, glTF, GLB e STL.

O Whisk3D também tem o seu **próprio formato livre** para salvar os projetos e os jogos: o **`.w3d`**. Um jogo em `.w3d` pode ser aberto diretamente em qualquer sistema onde o Whisk3D funcione, ou **compilado** para um sistema específico, para aproveitar ao máximo o hardware dele. Veja o artigo [W3D: um formato livre para que os videogames não morram](https://leoncini.com.ar/articulo.php?id=w3d-formato-abierto&lang=pt).

Os jogos também podem ser feitos **sem o editor**, programando diretamente sobre o **Whisk3D Core** em C e C++. Esse caminho é só para usuários com experiência, que já sabem programar em C e C++: veja o [Manual do Whisk3D Core](../core/empezar.md).

## Onde funciona

| Sistema | |
|---|---|
| **PC** | Windows, Linux e macOS |
| **Telefones** | Android (a partir do 2.3) e os Nokia N95 e N8 (Symbian) |
| **Navegador** | Qualquer navegador moderno (WebGL) |
| **Consoles** | Sega Dreamcast, Sony PSP, PlayStation Vita e Nintendo 3DS |

## Como ele é montado

O Whisk3D são três projetos, um em cima do outro, como três engrenagens que se movem juntas:

- **Whisk3D Editor** (a engrenagem verde): o editor, com tudo o que foi descrito acima.
- **[Whisk3D UI](https://leoncini.com.ar/proyecto.php?id=whisk3d-ui&lang=pt)** (a amarela): cuida da **interface**: os botões, os menus, os cartões de propriedades, o texto e os ícones. O editor é feito com ela. Veja o [Manual do Whisk3D UI](../ui/README.md).
- **[Whisk3D Core](https://leoncini.com.ar/proyecto.php?id=whisk3d-core&lang=pt)** (a vermelha): a **camada de abstração**. O Whisk3D UI a usa para tudo o que faz.

Um jogo ou um projeto feito no editor vive **dentro** do editor, com a sua lógica escrita em Lua. Mas o Editor e o Whisk3D UI são **opcionais**: dá para usar ferramentas próprias. Um jogo em C ou C++ pode usar o Whisk3D UI para a sua interface, sem o editor, ou falar **diretamente com o Core**, com a sua própria interface e as suas próprias ferramentas. A única coisa que não pode faltar é o Core.

![A arquitetura do Whisk3D em três camadas: em cima o Editor (verde), com o seu jogo ou projeto em Lua dentro; no meio o Whisk3D UI (amarelo); embaixo o Whisk3D Core (vermelho). O Editor e a UI são opcionais: de fora, um jogo em C++ pode usar a UI e outro o Core diretamente. O Core fala com cada sistema pelo seu próprio backend](img/whisk3d-capas.webp)

O Core é o **ingrediente secreto**. Cada sistema desenha, toca sons, lê arquivos e recebe as teclas do seu jeito: um PC usa OpenGL, o Dreamcast tem o seu próprio chip gráfico, o PSP e o 3DS as suas próprias bibliotecas, o navegador WebGL. Nem o editor nem os jogos falam com cada sistema: eles pedem as coisas ao Core, sempre do mesmo jeito ("desenhe esta malha", "toque este som", "abra este arquivo"), e o Core sabe como fazer isso em cada um. Por isso o **mesmo código** funciona em máquinas tão diferentes, e adicionar um sistema novo é ensinar o Core a falar com ele, não reescrever o editor.

Como ele faz isso, com todos os detalhes (os gráficos, o som, os arquivos, a entrada, a rede, os logs): [Manual do Whisk3D Core](../core/README.md).

## O projeto

Todo o trabalho é salvo em um arquivo de projeto `.w3d`. Dentro dele vão a cena e também os recursos: texturas (`texturas/`), scripts (`scripts/`), sons, cenas, prefabs. Um recurso pode ficar **dentro** do .w3d ou **fora**, como um arquivo solto no disco; os menus "Salvar no w3d" e "Salvar fora do w3d" o movem de um lado para o outro. Veja [O projeto](proyecto.md).

> [!NOTE]
> Este manual descreve o que o Whisk3D sabe fazer hoje. Os nomes das funções são os que aparecem no editor em português. As teclas indicadas são as padrão: dá para mudá-las em [Atalhos de teclado](atajos.md).
