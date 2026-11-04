# Editor de scripts (IDE)

Editor de texto para os scripts Lua e os [shaders escritos](shaders.md) (`.glsl`), com números de linha, cores de sintaxe, guias de indentação e suporte ao ñ e aos acentos. Enquanto ele tem o foco, as letras escrevem (não disparam atalhos do editor). No Android, tocar no texto abre o teclado. No N95, escreve-se com T9.

| Função | Descrição | Atalho |
|---|---|---|
| Script | Escolher qual .lua do projeto é editado, ou Carregar .lua… (um arquivo externo). | Menu Script |
| Salvar | Salva o script. Se ele estiver dentro do .w3d, salva também o projeto. | <kbd>Ctrl+S</kbd> ou Arquivo → Salvar |
| Salvar como… | Outro nome, dentro ou fora do projeto, em uma pasta à sua escolha. | Arquivo |
| Salvar no w3d | Só se o script for externo: coloca-o no projeto. | Arquivo |
| Salvar fora do w3d… | Só se o script for interno: tira-o para um arquivo; os objetos passam a usar esse arquivo. | Arquivo |
| Atualizar | Traz o arquivo tal como está no disco (o que foi editado com outro editor). Se houver mudanças não salvas no editor, pergunta antes de descartá-las (e dá para recuperá-las com <kbd>Ctrl+Z</kbd>). Com o jogo rodando, recarrega ao vivo os objetos que usam o script. | Botão |
| Shaders (.glsl) | Abrem-se a partir do material (o botãozinho da linha **Shader**), da Biblioteca ou do seletor, e são coloridos como GLSL. Ao lado do nome aparece **Compilar**: compila o que você está escrevendo (se um material o usa, salva-o e compila esse material). Salvar um shader o compila em todos os materiais que o usam e avisa se compilou, ou o erro com a sua linha. | <kbd>Ctrl+S</kbd> |
| Proteção ao salvar | Se o arquivo externo mudou por fora desde que foi aberto, Salvar pergunta antes de sobrescrevê-lo. Salvar também recarrega o script ao vivo se o jogo estiver rodando. | Automático |
| Indentação (Tab) | <kbd>Tab</kbd> completa até o próximo múltiplo de 4; com várias linhas selecionadas, adiciona um nível a elas, e <kbd>Shift+Tab</kbd> o remove. As linhas verticais marcam cada nível; a do bloco do cursor aparece mais clara. | <kbd>Tab</kbd> / <kbd>Shift+Tab</kbd> |
| Menu Editar | Desfazer, Refazer, Cortar, Copiar, Colar, Localizar… e Substituir… | Botão Editar |
| Menu Selecionar | Selecionar tudo. | Botão Selecionar |
| Desfazer / Refazer | Sobre o texto do script (não sobre a cena). | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> ou <kbd>Ctrl+Shift+Z</kbd> |
| Localizar / Substituir | Abre um painel no canto superior direito: o texto a buscar, quantas ocorrências há ("3 de 7"), diferenciar maiúsculas, palavra inteira, Anterior / Próximo e, em Substituir, o texto novo com Substituir e Substituir tudo (um único passo de desfazer). As ocorrências são destacadas no código. O painel não bloqueia o editor: com o ponteiro sobre o script, você continua digitando. Fecha-se com Fechar ou <kbd>Esc</kbd>. | <kbd>Ctrl+F</kbd> / <kbd>Ctrl+H</kbd> |
| Copiar / Cortar / Colar | Com a área de transferência do sistema. | <kbd>Ctrl+C</kbd> / <kbd>Ctrl+X</kbd> / <kbd>Ctrl+V</kbd> |
| Selecionar tudo / Desfazer | Desfazer afeta o texto, não a cena. | <kbd>Ctrl+A</kbd> / <kbd>Ctrl+Z</kbd> |
| Selecionar com o dedo | Toque duplo seleciona a palavra, toque triplo a linha inteira. Com uma seleção, aparecem duas alças (círculos verdes) nas suas pontas: arraste-as para ajustá-la. | Toques |
| Lupa (touch) | Deixar o dedo parado um momento sobre o código mostra, acima do dedo, a linha ampliada com o cursor; arrastando, o cursor segue o dedo e fica onde você soltar. | Toque longo |
| Selecionar no N95 | As setas movem o cursor; mantendo o lápis pressionado, as setas selecionam a partir de onde está o cursor. Enquanto o lápis é mantido, aparece uma barra embaixo: a tecla de seleção esquerda **copia** e a direita **cola**. | Lápis + setas / softkeys |
| Mover-se pelo texto | Setas, Home, End, Page Up, Page Down (com <kbd>Shift</kbd>, selecionam); <kbd>Ctrl+Home</kbd> / <kbd>Ctrl+End</kbd> vão para o começo ou para o fim. | Teclado |
