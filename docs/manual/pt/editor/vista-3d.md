# Viewport 3D

A Viewport 3D é onde você vê e edita a cena. Ela trabalha em diferentes **modos**: o que fazem os cliques, as teclas e os menus depende do modo atual. O modo é escolhido com o seletor da barra superior (só aparece se o objeto ativo for uma malha ou uma armação) ou com <kbd>Tab</kbd>.

| Modo | Para quais objetos | Para que serve |
|---|---|---|
| [Modo Objeto](modo-objeto.md) | Todos | Criar, mover, rotacionar, escalar, duplicar, parentear e excluir objetos inteiros. |
| [Modo Edição](modo-edicion.md) | Malhas e curvas | Editar vértices, arestas e faces (modelar). |
| [Modo Edição de armação](esqueletos.md#modo-edição-de-armação) | Armações | Criar e ajustar os ossos. |
| [Modo Pose](esqueletos.md#modo-pose) | Armações | Posar e animar os ossos. |
| [Pintura de pesos](pintura.md#pintura-de-pesos) | Só malhas 3D | Pintar quanto cada osso move cada vértice. |
| [Pintura de vértices](pintura.md#pintura-de-vértices) | Só malhas 3D | Pintar cores diretamente sobre os vértices. |
| [Pintura de textura](pintura.md#pintura-de-textura) | Só malhas 3D | Pintar sobre a textura, diretamente em cima do objeto. |

## Barra superior da Viewport 3D

| Botão | Descrição |
|---|---|
| Seletor de modo | Muda de modo (Objeto, Edição, Pose, Pintura…). |
| Vértice / Aresta / Face | Só no Modo Edição: o que é selecionado. |
| Ponto pivô | Ao redor de que se rotaciona e escala: Cursor 3D, Origens individuais, Ponto médio ou Elemento ativo. Com a caixa Travar normais. |
| Orientação | Os eixos do transform: Global, Local, Normal (a direção da face) ou Vista. |
| Ímã (Snap) | Opções de ímã. Fica verde se estiver ativo (<kbd>Shift+Tab</kbd>). |
| Edição proporcional | Ao mover algo, o que está perto acompanha suavemente. Fica verde se estiver ativa (<kbd>O</kbd>). |
| Vista | Câmeras, pontos de vista, enquadrar, vista local, perspectiva, travar órbita. |
| Selecionar | Tudo, nenhum, inverter, caixa, vinculado, loops e caminhos. |
| Adicionar (+) | Só no Modo Objeto: criar objetos novos. |
| Malha | Só no Modo Edição: as operações de modelagem. |
| Salvar textura (disquete) | Só em Pintura de textura. Verde se houver pintura não salva. |
| Animação (losango) | Inserir, excluir e limpar keyframes; rastro de movimento. |
| Contexto | O menu do modo: Objeto, Vértice/Aresta/Face, Armação ou Pose. |
| Sobreposições (olho) | Quais ajudas são desenhadas: piso, eixos, gizmo, cursor 3D, normais, estatísticas, raio-X. |
| Render (câmera) | O modo de sombreamento. |
| Cena | Escolher qual cena, jogo ou prefab do projeto é editado, e criar novos. |
| UV | Só no Modo Edição: desdobrar e projetar UVs. |

## Menus comuns a vários modos

### Selecionar

| Função | Descrição | Atalho |
|---|---|---|
| Tudo | Seleciona tudo. | <kbd>A</kbd> |
| Nenhum | Desseleciona tudo. | <kbd>Alt+A</kbd> |
| Inverter | O que está selecionado é desselecionado e vice-versa. | <kbd>Ctrl+I</kbd> |
| Seleção por caixa | Arrastar da esquerda para a direita (verde) pega só o que fica inteiro dentro; da direita para a esquerda (azul) basta encostar. <kbd>Shift</kbd> adiciona. | <kbd>B</kbd> |
| Ver seleção | Mostra ou oculta o contorno do que está selecionado. | Menu |

### Render (modos de sombreamento)

| Função | Descrição |
|---|---|
| Prévia Render | Com as luzes da cena. Se o ray tracing estiver ativo, mostra a imagem traçada aos poucos. |
| Prévia Material | A padrão: uma luz neutra a partir da vista, sem depender das luzes da cena. |
| Prévia Sólido / Aramado / ZBuffer | Cinza sólido, só as arestas, ou a profundidade. |
| Vista Normal / Prévia Alpha | As normais em cor, ou a transparência em preto e branco. |
| Vetores de movimento | Quanto cada pixel se moveu na tela desde o quadro anterior (vermelho = X, verde = Y; lavanda = parado, verde-oliva = o fundo). A mesma imagem da passada `_vectores` do render. |
| Aramado: só o visível | A vista aramada mostra só o que é desenhado de verdade. |

### Sobreposições

| Função | Descrição | Atalho |
|---|---|---|
| Mostrar sobreposições | Liga ou desliga todas as ajudas visuais de uma vez. | <kbd>K</kbd> |
| Piso, Eixo X, Eixo Y, Origens, Cursor 3D, Linhas de relação | Cada ajuda separadamente. | Menu |
| Gizmo | O gizmo de mover/rotacionar/escalar. | Menu |
| Objetos | Mostrar ou ocultar armações, luzes, câmeras, vazios, partículas, curvas, hitboxes. | Menu |
| Normais | Desenha as normais de vértice, personalizadas ou de face, com o seu tamanho. | Menu |
| Estatísticas | FPS, vértices, faces, chamadas de desenho e tempos. | Menu |
| Raio-X | A malha em edição fica semitransparente e dá para escolher os elementos de trás. | Menu |

### Ímã (Snap)

| Função | Descrição | Atalho |
|---|---|---|
| Ativar ímã | O que se move gruda em vértices, arestas, faces ou nos seus centros. | <kbd>Shift+Tab</kbd> |
| Base e destino de snap | Qual ponto do que é movido gruda (mais próximo, centro, mediana, ativo) e em quê. | Menu |
| Projetar na face / Face mais próxima | Para retopologia: os vértices se apoiam sobre outra superfície. | Menu |
| Afetar mover / rotacionar / escalar | Em quais operações o ímã atua. | Menu |
| Menu Snap do cursor | Seleção para a grade, para o cursor ou para o ativo; Cursor para o selecionado, para a origem, para a grade ou para o ativo. | <kbd>Shift+S</kbd> |

### Edição proporcional

| Função | Descrição | Atalho |
|---|---|---|
| Ativar | Ao mover, rotacionar ou escalar, os vizinhos acompanham conforme a distância. | <kbd>O</kbd> |
| Só conectados | Mede a distância pelas arestas: não arrasta peças soltas. | Menu |
| Queda | Forma da influência: suave, esfera, raiz, quadrado inverso, aguda, linear, constante, aleatória. | Menu |
| Tamanho proporcional | O raio de influência. Durante o transform, muda com a roda ou com <kbd>Page Up</kbd> / <kbd>Page Down</kbd>. | Menu / roda |
