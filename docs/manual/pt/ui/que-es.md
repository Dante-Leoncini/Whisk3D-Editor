# O que é o Whisk3D UI

O **Whisk3D UI** é uma biblioteca C++ para montar interfaces: botões, abas, menus suspensos, cartões de propriedades, texto e ícones. Tem seu próprio repositório, [Whisk3D-UI](https://github.com/Dante-Leoncini/Whisk3D-UI), com licença MIT. É construída sobre o [Whisk3D Core](../core/README.md), e o editor do Whisk3D é feito com ela.

![A arquitetura do Whisk3D: o Editor e o Whisk3D UI, opcionais, sobre o Whisk3D Core, que fala com cada sistema pelo seu próprio backend](img/whisk3d-capas.webp)

## Onde roda

Nos mesmos sistemas que o Core: **Windows**, **Linux**, **macOS**, **Android**, o **navegador**, os **Nokia N95** e **N8**, o **Sega Dreamcast**, o **Sony PSP**, o **PS Vita** e o **Nintendo 3DS**. O editor a compila em todos.

A UI não tem código próprio por sistema. Ela não chama o OpenGL: desenha tudo com as funções de `w3dEngine` ([Gráficos](../core/graficos.md)), e o backend de cada sistema faz o resto. O pouco que muda é de compilação:

- Os headers incluem `GL/gl.h`, ou `GLES/gl.h` no Symbian, por causa dos tipos (`GLfloat`, `GLshort`, `GLubyte`).
- No Android, `loadColorsW3d` atualiza a tabela de cores em bytes, mas não a de `float`.
- O código é C++03, porque o compilador do Symbian não conhece C++11: os enums ficam dentro de um `struct` (`IconType::camera`) e se usa `W3D_OVERRIDE` em vez de `override`.

## Por que atlas e não vetores

Quase todas as bibliotecas de interface desenham com vetores. Muitos aparelhos antigos não aceleram isso: a interface é desenhada por software e fica lenta. O que aparelhos como o N95 aceleram é o 3D: triângulos com textura.

Por isso, o Whisk3D UI desenha tudo com triângulos que leem de uma textura, um **atlas**. As letras, as bordas arredondadas dos cartões e os ícones estão na mesma textura: um único `BindTexture` basta para toda a interface. Um texto é desenhado com uma chamada por atlas, não com uma por letra.

O preço: as letras são de pixels e crescem em múltiplos inteiros ([Cores e escala](tema.md)), e só existem as letras que estão desenhadas no atlas ([Texto](texto.md)).

## O que há dentro

| Pasta | O que resolve |
|---|---|
| `core/` | As medidas da interface e sua escala (`UI.h`): [Cores e escala](tema.md) |
| `theme/` | A paleta de cores (`colores.h`): [Cores e escala](tema.md) |
| `draw/` | O desenho 2D (`glesdraw`), os retângulos, os ícones e o empacotador de atlas: [Desenho e atlas](dibujo.md), [Ícones](iconos.md) |
| `text/` | A fonte bitmap, o atlas estendido e o texto com fontes TTF bakeadas: [Texto](texto.md) |
| `widgets/` | Os cartões com borda, os botões, as abas, os menus e as caixas de texto: [Botões, abas e caixas de seleção](botones.md), [Menus suspensos](menus.md), [Entrada e caixas de texto](entrada.md) |
| `Propieties/` | Os cartões de propriedades e suas linhas: [Cartões e propriedades](propiedades.md) |
