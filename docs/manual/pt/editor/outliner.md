# Outliner

A lista de tudo o que há na cena, e também a biblioteca de recursos do projeto. No Android não é mostrado (trabalha-se com a Viewport 3D e Propriedades).

## Menu Vista (olho)

| Função | Descrição |
|---|---|
| Cena / Biblioteca / bibliotecas | O que é listado: a árvore da cena, todos os recursos do projeto, ou os de outro .w3d vinculado (só leitura). |
| Construtor / Rotinas | Mostram ou ocultam a rotina Construtor (o que roda uma única vez ao começar) e as demais rotinas. |
| Ícones de visibilidade / Ícones de render | Mostram ou ocultam as colunas do olho e da câmera. |
| Grade | Na Biblioteca: miniaturas em vez de lista. |
| Vincular .w3d… / Desvincular | Usa outro projeto como biblioteca, ou o solta. |

## Vista Cena

| Função | Descrição | Como usar |
|---|---|---|
| Selecionar | Escolhe o objeto. <kbd>Shift</kbd>+clique pega um intervalo; <kbd>Ctrl</kbd>+clique adiciona ou remove. | Clique |
| Expandir / recolher | Mostra ou oculta os filhos. | Seta; <kbd>Esquerda</kbd> / <kbd>Direita</kbd> |
| Visível / Renderizável | O olho oculta o objeto (é animável); a câmera o exclui do render. | Clique no olho / na câmera |
| Reordenar e parentear | Soltar antes, depois ou em cima de outra linha (como filho). | Arrastar a linha |
| Mover (reordenar) | Com o teclado: cima/baixo reordena, esquerda tira do pai, direita coloca sob o anterior, <kbd>Enter</kbd> confirma. | <kbd>G</kbd> |
| Renomear | A linha vira um campo de texto. | Clique duplo ou <kbd>F2</kbd> |
| Duplicar / Duplicar vinculado | Como na Viewport 3D. | <kbd>Shift+D</kbd> / <kbd>Alt+D</kbd> |
| Ocultar / Mostrar | Alterna a visibilidade. | <kbd>H</kbd> |
| Excluir | Com confirmação. | <kbd>X</kbd> ou <kbd>Delete</kbd> |
| Criar / Desempacotar prefab | Pelo menu Objeto. | Clique direito |
| Mostrar o ativo | Rola a lista até o objeto ativo. | <kbd>Num.</kbd> |
| Rolar a lista | Roda, ou arrastar (também com o dedo). | Roda / arrastar |

## Vista Biblioteca

| Função | Descrição |
|---|---|
| Recursos | Malhas, materiais, texturas, animações, prefabs, cenas, sons, scripts, [shaders](shaders.md), fontes e vídeos, em pastas. Um material ou um shader arrastado sobre uma malha do Viewport 3D aparece na parte sob o ponteiro antes de soltá-lo. Visto verde = em uso; X vermelho = órfão; `*` = não salvo. |
| Filtro / Lista ou grade | Mostrar um só tipo; ver miniaturas. |
| Arrastar um recurso | Para uma pasta (move-o), para a Viewport 3D (malha = objeto novo, material ou textura = é atribuído ao que for tocado, prefab = instância) ou para uma lista suspensa de Propriedades. |
| Nova pasta / Mover para pasta | Organizar (as pastas servem só para organizar). |
| Duplicar recurso / Selecionar usuários / Purgar órfãos | Copiar, encontrar quem o usa, excluir o que ninguém usa. |
| Dentro do .w3d / Externo | Em Propriedades (cartão Recurso): move o recurso para dentro ou para fora do projeto. |

## Cenas, jogos e prefabs

O botão com o nome da cena (no Outliner e na Viewport 3D) lista as cenas, jogos e prefabs do projeto. Por ele você cria novos, escolhe a cena inicial e converte uma cena em jogo (ou o contrário).
