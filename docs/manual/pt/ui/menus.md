# Menus suspensos

`PopupMenu` (`widgets/PopupMenu.h`) é uma lista flutuante de opções: o menu de uma barra, um menu de contexto, um seletor. Cada opção (`MenuItem`) tem um texto e pode ter:

- um **ícone** à esquerda,
- um **submenu**, com uma seta à direita,
- uma **caixa de seleção**, que mostra e muda um `bool`,
- um **controle deslizante**, que edita um `float` entre dois valores,
- um **atalho**, à direita e em cinza esmaecido ("Shift D"),
- `verde`: é a opção ativa, com o ícone e o texto em `accent`,
- `gris`: aponta para um `bool`; enquanto ele valer `false`, a opção aparece com meia opacidade e não responde,
- `lineaArriba`: uma linha que a separa do grupo de cima.

O menu pode ter um **título** (`titulo`): fica em branco, com uma linha embaixo.

## Montá-lo com uma tabela

A forma recomendada é uma tabela de `MenuDef`: o texto e o que cada opção faz ficam na mesma linha. Não há um número para conectar a um `switch` em outro lugar.

```cpp
static bool grilla = true;
static PopupMenu* recientes = NULL;
static void Nuevo() { /* ... */ }
static void Abrir() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // texto     ação    atalho    ícone                    submenu     caixa
    { "Nuevo",   Nuevo,  "Ctrl N", ICONO(IconType::mas) },
    { "Abrir",   Abrir,  "Ctrl O", ICONO(IconType::carpeta) },
    { "Recientes", NULL, NULL,     ICONO(IconType::lista),  &recientes },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grilla },
};

recientes = new PopupMenu();          // o submenu tem que existir antes de construir
PopupMenu* archivo = new PopupMenu();
archivo->titulo = "Archivo";
archivo->Construir(ARCHIVO, 4);
```

`ICONO(x)` soma 1 ao ícone: numa tabela, o 0 quer dizer "sem ícone". O id de cada opção é a sua linha na tabela.

Dois ganchos opcionais, que o programa instala:

```cpp
W3dMenuTraducir = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuAtajoTexto = MiAtajo;      // std::string MiAtajo(const std::string& atajo)
```

Com `W3dMenuTraducir`, os textos da tabela saem traduzidos ao construir o menu. Com `W3dMenuAtajoTexto`, um atalho pode ser um nome que começa com `@`, e o gancho retorna a tecla correspondente. Sem esse gancho, um atalho com `@` não é mostrado.

## Montá-lo à mão

```cpp
PopupMenu* vista = new PopupMenu();
vista->Agregar("Frente", 0, IconType::camera);
vista->AgregarCheck("Mostrar piso", 1, &verPiso);
vista->AgregarFloat("Tamaño", 2, &tamanio, 0.0f, 4.0f);
MenuItem* it = vista->Agregar("Alambre", 3);
it->verde = esAlambre;
it->lineaArriba = true;
vista->action = AlElegir;              // void AlElegir(int id)
```

`Agregar` retorna o `MenuItem*`, para mudar o resto. Ao escolher uma opção, `Ejecutar(id)` chama a `accion` dela, se tiver uma, e, se não tiver, o `action(id)` do menu. `Limpiar()` apaga todas as opções, para os menus que mudam.

## Abri-lo

```cpp
archivo->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
MenuAbierto = archivo;
```

Ele abre no ponto que você passar: o cursor, ou a borda de um botão. Se não couber, ele se desloca para dentro da tela. `Abrir` não toca em `MenuAbierto`: quem o define é quem abre o menu. `Cerrar()` o volta para `NULL`.

Ao abrir, ele se mede:

- A largura é a da linha mais longa, com seu texto e sua coluna direita: o atalho, a seta, a caixa de seleção ou o controle deslizante. Se uma linha tiver ícone, todas reservam o espaço dele, e assim os textos ficam alinhados.
- Nunca é mais largo que a tela. Se não couber, o texto de cada linha é cortado antes da sua coluna direita: as caixas de seleção continuam visíveis.
- Nunca é mais alto que a tela. Se tiver mais linhas, mostra as que cabem, com uma barra de rolagem à direita. Rola com a roda (`Wheel`).

Um submenu abre ao lado da sua linha: à direita se couber; se não, à esquerda. Se não couber de nenhum lado, ele fica mais estreito e deixa à vista uma coluna do menu de trás, para que se possa passar o mouse para outra opção.

## Mouse e dedo

```cpp
// ao mover
if (MenuAbierto) MenuAbierto->MouseMove(mx, my);

// ao clicar ou tocar
if (MenuAbierto) {
    PopupMenu* m = MenuAbierto;
    int id = m->Click(mx, my);
    if (id >= 0) m->Ejecutar(id);
}

// ao desenhar, com uma projeção de tela inteira
if (MenuAbierto) MenuAbierto->Render();
```

`Click` retorna o id escolhido, `-1` se não escolheu nada ou `-2` se abriu um submenu.

- Passar o mouse destaca a linha e abre o seu submenu, sem clique.
- Se o mouse se afastar mais de `60 * GlobalScale` pixels, o menu se fecha sozinho. Com `cerrarLejos = false`, não: para um menu que não foi aberto no mouse.
- Um clique fora o fecha.
- Uma opção comum fecha o menu. Uma caixa de seleção muda e o deixa aberto, para marcar várias. Um clique num controle deslizante define o valor conforme o ponto onde caiu.
- Manter pressionado sobre um controle deslizante e arrastar muda o valor ao vivo. Quem cuida do mouse passa cada movimento para `MenuSliderDragMover(mx)`, enquanto `MenuSliderDragActivo()` for `true`, e ao soltar chama `MenuSliderDragSoltar()`.
- Numa tela touch, o toque que abre um submenu não escolhe nada dentro dele, mesmo que o submenu tenha ficado sob o dedo. É preciso um segundo toque.
- O que se escolhe num submenu é executado nesse submenu: cada menu tem seus próprios ids.

## Teclado

O foco fica com o submenu aberto mais interno:

```cpp
PopupMenu* m = MenuAbierto;
while (m->submenuAbierto && m->submenuAbierto->abierto) m = m->submenuAbierto;
MenuItem* it = m->ItemActual();      // a opção destacada, ou NULL
```

| Função | O que faz |
|---|---|
| `button_up()`, `button_down()` | Movem o destaque, dando a volta. Não abrem submenus |
| `AbrirSubmenuActual()` | Abre o submenu da opção destacada e destaca a primeira opção dele |
| `AjustarSlider(+1)`, `AjustarSlider(-1)` | Movem o controle deslizante um vigésimo do seu intervalo |
| `Enter()` | Como um clique na opção destacada, e retorna o mesmo. Chamado em `MenuAbierto`, chega sozinho ao submenu mais interno |
| `Cerrar()` | Fecha o menu e seus submenus |

Com o teclado, a opção destacada sempre fica à vista. Qual tecla chama o quê é decidido por quem usa o menu.

`MenuPantallaW` e `MenuPantallaH` precisam ter o tamanho da tela: é com eles que o menu se ajusta.
