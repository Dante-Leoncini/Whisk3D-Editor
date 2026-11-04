# Propriedades

O painel com todos os dados do objeto ativo, organizado em abas (ícones). As abas que aparecem dependem do tipo de objeto. Clicar no título de um cartão o recolhe. Nos campos numéricos: clique para digitar (aceita contas), arraste para deslizar o valor. O losango da direita põe ou tira um keyframe.

| Aba | O que tem |
|---|---|
| Arquivo / Render (disquete) | **Arquivo**: abrir, salvar, salvar como, salvar versão, extrair assets. **Render**: caminho, tamanho, passes extras, fundo, **Ray Tracing** (raios, amostras, passes), Renderizar imagem e Renderizar animação. **Jogo**: plataforma (Windows, Symbian, Linux .deb, AppImage, WebGL, Android), janela, orientação, ícone, física, som e Compilar jogo. **Exportar**: OBJ, glTF, GLB. **Ajustes**: idioma, antialiasing, gráficos, tema, logs, escala, **Atalhos de teclado…** e Salvar alterações. |
| Objeto | Posição, rotação (Euler, quaternion ou ângulo de eixo), escala, nome, visível, renderizável; animações e paleta do objeto. |
| Contextual | Depende do tipo: partes de malha e **material** (textura, iluminação, cor de vértice, transparência, reflexos, normal map, mistura, profundidade, camadas de textura, **Avançado: shader personalizado** — veja [Shaders escritos](shaders.md); Rugosidade, Metálico, Transmissão e IOR com ray tracing), luz, câmera, névoa, partículas, física, hitbox, armação e ossos, elementos 2D, prefabs, rotinas. |
| Malha 3D | A malha usada (e quantos a compartilham), mapas UV, camadas de cor, grupos de vértices, grupos UV. Com um recurso escolhido na Biblioteca: seus dados e sua localização (dentro ou fora do .w3d). |
| Modificadores | Veja [Modificadores](modificadores.md). |
| Transformar | Só no Modo Edição: centro da seleção e de suas UV, editáveis. |
| Armação 2D | Armações e ossos 2D da malha. |
| Restrições | Copiar posição, Copiar rotação, Billboard (olhar para a câmera), Filho De. |
| Scripts | Os scripts Lua do objeto e suas propriedades (veja o [documento de Lua](../lua/README.md)). |
| Animação | A animação ativa, camadas de mistura (Mix), valores exatos do keyframe escolhido e cortes de câmera. |

## Rotinas

Uma rotina é uma lista de passos de desenho (transladar, rotacionar, definir textura, desenhar uma malha, chamar outra rotina, Chamar Lua…) montada em Propriedades. É a forma mais rápida de desenhar no N95.

| Função | Descrição | Atalho |
|---|---|---|
| Adicionar / Remover passo | Adiciona passos agrupados por tema, ou remove o escolhido. | Botões; <kbd>X</kbd> ou <kbd>Delete</kbd> remove |
| Pegar passo | Reordena o passo com as setas; <kbd>Enter</kbd> confirma. | <kbd>G</kbd> |
| Inserir keyframe | Anima os valores do passo no quadro atual. | <kbd>I</kbd> |
| Repetir | Quantas vezes a lista roda seguida (laço de rotina). 1 = rotina normal. | Campo Repetir |
| Modo de render / Usa | Uma lista diferente por modo de sombreamento, ou a mesma para todos. | Listas suspensas |
