# Menus suspensos

`PopupMenu` (`widgets/PopupMenu.h`) é uma lista flutuante de opções: o menu de uma barra, um menu de contexto, um seletor. Cada opção (`MenuItem`) tem um texto e pode ter:

- um **ícone** à esquerda,
- um **submenu**, com uma seta à direita,
- uma **caixa de seleção**, que mostra e muda um `bool`,
- um **controle deslizante**, que edita um `float` entre dois valores,
- um **atalho**, à direita e em cinza esmaecido ("Shift D"),
- `green`: é a opção ativa, com o ícone e o texto em `accent`,
- `gray`: aponta para um `bool`; enquanto ele valer `false`, a opção aparece com meia opacidade e não responde,
- `lineUp`: uma linha que a separa do grupo de cima.

O menu pode ter um **título** (`title`): fica em branco, com uma linha embaixo.

## Montá-lo com uma tabela

A forma recomendada é uma tabela de `MenuDef`: o texto e o que cada opção faz ficam na mesma linha. Não há um número para conectar a um `switch` em outro lugar.

```cpp
static bool grid = true;
static PopupMenu* recent = NULL;
static void New() { /* ... */ }
static void Open() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // texto     ação    atalho    ícone                    submenu     caixa
    { "Nuevo",   New,  "Ctrl N", ICON(IconType::more) },
    { "Abrir",   Open,  "Ctrl O", ICON(IconType::folder) },
    { "Recientes", NULL, NULL,     ICON(IconType::list),  &recent },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grid },
};

recent = new PopupMenu();          // o submenu tem que existir antes de construir
PopupMenu* archivo = new PopupMenu();
archivo->title = "Archivo";
archivo->Build(ARCHIVO, 4);
```

`ICON(x)` soma 1 ao ícone: numa tabela, o 0 quer dizer "sem ícone". O id de cada opção é a sua linha na tabela.

Dois ganchos opcionais, que o programa instala:

```cpp
W3dMenuTranslate = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuShortcutText = MiShortcut;      // std::string MiAtajo(const std::string& atajo)
```

Com `W3dMenuTranslate`, os textos da tabela saem traduzidos ao construir o menu. Com `W3dMenuShortcutText`, um atalho pode ser um nome que começa com `@`, e o gancho retorna a tecla correspondente. Sem esse gancho, um atalho com `@` não é mostrado.

## Montá-lo à mão

```cpp
PopupMenu* view = new PopupMenu();
view->Add("Frente", 0, IconType::camera);
view->AddCheck("Mostrar piso", 1, &showFloor);
view->AddFloat("Tamaño", 2, &size, 0.0f, 4.0f);
MenuItem* it = view->Add("Alambre", 3);
it->green = isWire;
it->lineUp = true;
view->action = OnPick;              // void AlElegir(int id)
```

`Add` retorna o `MenuItem*`, para mudar o resto. Ao escolher uma opção, `Run(id)` chama a `action` dela, se tiver uma, e, se não tiver, o `action(id)` do menu. `Clear()` apaga todas as opções, para os menus que mudam.

## Abri-lo

```cpp
archivo->Open(mx, my, MenuScreenW, MenuScreenH);
OpenMenu = archivo;
```

Ele abre no ponto que você passar: o cursor, ou a borda de um botão. Se não couber, ele se desloca para dentro da tela. `Open` não toca em `OpenMenu`: quem o define é quem abre o menu. `Close()` o volta para `NULL`.

Ao abrir, ele se mede:

- A largura é a da linha mais longa, com seu texto e sua coluna direita: o atalho, a seta, a caixa de seleção ou o controle deslizante. Se uma linha tiver ícone, todas reservam o espaço dele, e assim os textos ficam alinhados.
- Nunca é mais largo que a tela. Se não couber, o texto de cada linha é cortado antes da sua coluna direita: as caixas de seleção continuam visíveis.
- Nunca é mais alto que a tela. Se tiver mais linhas, mostra as que cabem, com uma barra de rolagem à direita. Rola com a roda (`Wheel`).

Um submenu abre ao lado da sua linha: à direita se couber; se não, à esquerda. Se não couber de nenhum lado, ele fica mais estreito e deixa à vista uma coluna do menu de trás, para que se possa passar o mouse para outra opção.

## Mouse e dedo

```cpp
// ao mover
if (OpenMenu) OpenMenu->MouseMove(mx, my);

// ao clicar ou tocar
if (OpenMenu) {
    PopupMenu* m = OpenMenu;
    int id = m->Click(mx, my);
    if (id >= 0) m->Run(id);
}

// ao desenhar, com uma projeção de tela inteira
if (OpenMenu) OpenMenu->Render();
```

`Click` retorna o id escolhido, `-1` se não escolheu nada ou `-2` se abriu um submenu.

- Passar o mouse destaca a linha e abre o seu submenu, sem clique.
- Se o mouse se afastar mais de `60 * GlobalScale` pixels, o menu se fecha sozinho. Com `closeFar = false`, não: para um menu que não foi aberto no mouse.
- Um clique fora o fecha.
- Uma opção comum fecha o menu. Uma caixa de seleção muda e o deixa aberto, para marcar várias. Um clique num controle deslizante define o valor conforme o ponto onde caiu.
- Manter pressionado sobre um controle deslizante e arrastar muda o valor ao vivo. Quem cuida do mouse passa cada movimento para `MenuSliderDragMove(mx)`, enquanto `MenuSliderDragActive()` for `true`, e ao soltar chama `MenuSliderDragRelease()`.
- Numa tela touch, o toque que abre um submenu não escolhe nada dentro dele, mesmo que o submenu tenha ficado sob o dedo. É preciso um segundo toque.
- O que se escolhe num submenu é executado nesse submenu: cada menu tem seus próprios ids.

## Teclado

O foco fica com o submenu aberto mais interno:

```cpp
PopupMenu* m = OpenMenu;
while (m->openSubmenu && m->openSubmenu->open) m = m->openSubmenu;
MenuItem* it = m->CurrentItem();      // a opção destacada, ou NULL
```

| Função | O que faz |
|---|---|
| `button_up()`, `button_down()` | Movem o destaque, dando a volta. Não abrem submenus |
| `OpenSubmenuCurrent()` | Abre o submenu da opção destacada e destaca a primeira opção dele |
| `FitSlider(+1)`, `FitSlider(-1)` | Movem o controle deslizante um vigésimo do seu intervalo |
| `Enter()` | Como um clique na opção destacada, e retorna o mesmo. Chamado em `OpenMenu`, chega sozinho ao submenu mais interno |
| `Close()` | Fecha o menu e seus submenus |

Com o teclado, a opção destacada sempre fica à vista. Qual tecla chama o quê é decidido por quem usa o menu.

`MenuScreenW` e `MenuScreenH` precisam ter o tamanho da tela: é com eles que o menu se ajusta.
