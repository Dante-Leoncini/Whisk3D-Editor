# Modo Objeto

| Função | Descrição | Como usar |
|---|---|---|
| Selecionar | Escolhe um objeto. Clique no vazio desseleciona. | Clique; <kbd>Shift</kbd>+clique adiciona ou remove |
| Mover / Rotacionar / Escalar | Veja [Mover, rotacionar e escalar](transformar.md). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>, gizmo |
| Adicionar | Cria um objeto novo no cursor 3D (veja o [menu Adicionar](#menu-adicionar)). | <kbd>Shift+A</kbd> ou botão + |
| Aplicar | Incorpora na malha a posição, a rotação, a escala ou tudo (o objeto fica em 0, 0, 0, sem rotação e com escala 1). | <kbd>Ctrl+A</kbd> |
| Definir origem | Geometria para Origem, Origem para Geometria, Origem para Cursor 3D. | Menu Objeto |
| Ímã | Leva o cursor 3D para o selecionado, para o ativo, para a origem do mundo ou para a grade, e o selecionado para o cursor, para o ativo ou para a grade. | <kbd>Shift+S</kbd> ou menu Objeto → Ímã |
| Duplicar objetos | Cópia independente; fica presa ao ponteiro para você movê-la. | <kbd>Shift+D</kbd> |
| Duplicar vinculado | Cópia que compartilha a malha com o original (editar uma edita as duas). | <kbd>Alt+D</kbd> |
| Juntar | Junta as malhas selecionadas no objeto ativo. | <kbd>Ctrl+J</kbd> |
| Parentear | O que está selecionado passa a ser filho do ativo (mantendo ou não o transform). | <kbd>Ctrl+P</kbd> |
| Desparentear | Remove o pai (mantendo ou não a posição). | <kbd>Alt+P</kbd> |
| Excluir | Exclui os objetos selecionados (pede confirmação). | <kbd>X</kbd> ou <kbd>Delete</kbd> |
| Ocultar / Mostrar | Alterna a visibilidade do objeto ativo. | <kbd>H</kbd> |
| Criar prefab / Desempacotar prefab | Converte a seleção em um prefab reutilizável, ou volta a separar uma instância. | Menu Objeto |
| Inserir keyframe | Salva a posição, a rotação e/ou a escala no quadro atual (menu de canais). <kbd>Shift+I</kbd> salva tudo sem perguntar. | <kbd>I</kbd> |
| Excluir / Limpar keyframes | Exclui o keyframe do quadro atual ou toda a animação. | Menu Animação |
| Rastro de movimento | Desenha o caminho que os objetos animados percorrem. | Menu Animação |
| Mudar de modo | Malha: Objeto ↔ Edição. Armação: Objeto ↔ Pose. Curva: edição de nós. | <kbd>Tab</kbd> |

## Menu Adicionar

Só no Modo Objeto. O que é novo aparece no cursor 3D e fica selecionado. As primitivas abrem um painel para ajustar suas medidas.

| Função | Descrição |
|---|---|
| Malha | Plano, Cubo, Círculo, Esfera UV, Cone, Cilindro (com tamanho, raio, segmentos, sombreamento suave). Também Vértice (um ponto solto para começar a modelar) e Referência (um plano com uma imagem, para decalcar). |
| Vazio | Um objeto sem malha, para agrupar ou como ponto de referência. |
| Câmera / Luz / Névoa | Câmera, luz e névoa da cena. A luz tem **Tipo**: Ponto, Sol (direcional) ou Foco (um cone, desenhado na vista); Adicionar também oferece Sol e Foco. **Intensidade** a deixa mais forte (animável); **Distância** (m) é até onde chega uma luz pontual ou foco, e o comprimento do seu cone (0 = sem limite; o ray tracing a apaga suavemente ali). Com a luz selecionada, um foco mostra seu cone e uma pontual uma esfera dessa distância; um sol mostra sempre uma linha até o chão, para onde ilumina. |
| Som | Um som colocado na cena (ícone de alto-falante). Toca com a linha do tempo em **segundos**, não em frames; com **Posicional (3D)** é ouvido de onde está (mais alto quanto mais perto da câmera, do lado em que está); sem isso é como uma música. Soltar um .wav na vista 3D cria um ali. |
| Armação | Uma armação com um osso, para animar personagens. |
| Caminho | Uma curva de dois nós (é editada como malha: <kbd>E</kbd> adiciona nós). |
| Coleção | Uma pasta para organizar objetos. |
| UI | Uma interface 2D para jogos (é editada no Editor 2D). |
| Hitbox | Uma caixa de detecção de colisões; se houver um objeto ativo, nasce como filho dele, do seu tamanho. |
| Física | Não cria objeto: adiciona um corpo rígido ao objeto ativo. |
| Partículas | Um emissor de partículas. |
| LOD / Culling / Grid Cull | Objetos de otimização: níveis de detalhe e ocultação do que não se vê. |
| Limpar Z / Recorte | Ferramentas de desenho: limpar a profundidade ou recortar uma área da tela. |
| Prefab / Cena / Proxy W3D | Inserir um prefab do projeto, outra cena, ou um prefab de uma biblioteca externa. |
| Importar | OBJ, FBX, glTF, GLB, STL ou uma interface Whisk3D UI. O STL (impressão 3D, CAD) entra com Z para cima, girado para ficar em pé, com os vértices iguais soldados. |
| Núcleo (avançado) | Rotinas: listas de passos de desenho montadas à mão (transformar, desenhar, texturas, luzes, névoa, mistura, profundidade, controle, Chamar Lua). Inclui o **Laço de rotina**: uma rotina que repete seus passos várias vezes (por exemplo "transladar, desenhar" 20 vezes), como um array, mas mais rápido. |
