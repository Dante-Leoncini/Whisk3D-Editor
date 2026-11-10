# Entrada e caixas de texto

## A UI não lê eventos

O Whisk3D UI não abre janelas nem escuta o mouse, o teclado ou a tela touch. Quem recebe os eventos é o programa, em quase todos os sistemas pelo SDL2 ([Entrada](../core/entrada.md) do Core), e ele passa a cada widget o que lhe corresponde:

| Evento | Para quem vai |
|---|---|
| O mouse ou o dedo se move | `PopupMenu::MouseMove(x, y)`; o `hover` de botões e abas; `PropHoverGroup` e `PropHoverFila` nos cartões |
| Clique ou toque | `PopupMenu::Click(x, y)`; `Button::Contains` e `Tab::Contains`; num cartão, a linha que o programa calcular |
| Roda | `PopupMenu::Wheel(dir)` |
| Setas | `button_up`, `button_down`, `button_left` e `button_right` de menus, cartões e linhas |
| OK ou Enter | `PopupMenu::Enter()`, `GroupPropertie::EnterPropertieSelect()` |
| Cancelar ou Esc | `PopupMenu::Cerrar()`, `GroupPropertie::Cancel()` |
| Uma letra | A caixa de texto em foco |

As coordenadas são pixels de tela, com o Y para baixo.

## Sem mouse

Tudo pode ser usado com quatro setas, OK e Cancelar: é assim que se usa no N95. O que a UI oferece para isso:

- Os cartões são percorridos linha por linha e pulam o que não pode ser escolhido.
- Os menus são percorridos com as setas, e a opção destacada nunca sai da vista.
- Os números são alterados com as setas, sem digitar.
- O foco do teclado é visível: `focoMenu` nos botões e `foco` nas abas, em `accent`, e a borda branca de `g_kfFocoProp` sobre um losango.

Qual tecla do celular é cada uma, as soft keys ou o OK, não é da UI: quem traduz isso é o editor e cada sistema.

## Tela touch

- Um toque é um clique.
- O toque que abre um submenu não escolhe nada dentro dele ([Menus suspensos](menus.md)).
- `UITactil` aumenta os botões e as barras para o dedo ([Cores e escala](tema.md)). Hoje nada a ativa.

Arrastar para rolar, manter pressionado ou tocar duas vezes fica por conta de quem usa a UI.

## Caixas de texto

`TextField` (`widgets/TextField.h`) é o estado de uma caixa de texto: o texto (`text`), a posição do cursor (`caret`) e se está tudo selecionado (`selectAll`). Não desenha nem lê teclas: é só o dado.

| Função | O que faz |
|---|---|
| `SetText(t)` | Define o texto, com o cursor no final |
| `SelectAll()` | Seleciona tudo: a primeira tecla o substitui |
| `InsertChar(c)` | Escreve um byte onde está o cursor |
| `Backspace()`, `DelForward()` | Apagam para trás ou para a frente |
| `CaretIzq()`, `CaretDer()` | Movem o cursor. Com tudo selecionado, tiram a seleção |

Apagar e mover o cursor tratam uma letra de vários bytes, como um `ñ` ou uma letra cirílica, como uma só. Para escrever uma delas, passa-se cada byte para `InsertChar`.

Só há uma caixa em foco por vez: `g_textFieldActivo`, ou `NULL` se não houver nenhuma. As teclas vão para ela.

```cpp
TextField nombre;
nombre.SetText("Jugador 1");
nombre.SelectAll();
g_textFieldActivo = &nombre;

// a cada letra
if (g_textFieldActivo) g_textFieldActivo->InsertChar(c);

// ao desenhar, com a cor do texto definida
RenderTextField(nombre, g_textFieldActivo == &nombre, textAlign::left, 200);
```

`RenderTextField(caja, foco, alineación, ancho)` a desenha na origem atual:

- Sem foco, é o texto.
- Com foco e tudo selecionado, o texto fica em branco sobre `accentDark`.
- Com foco e sem seleção, aparece o cursor: um tracinho que pisca a cada 530 ms.

`TextFieldParpadeoTick()` retorna `true` quando o tracinho muda: é o momento de redesenhar.

`TextField.h` também declara `TextFieldEnfocar`, `TextFieldCancelar` (Esc: volta ao texto de antes do foco), `TextFieldInputChar` e `TextFieldFinDeCuadro`. Hoje elas estão definidas no editor, em `main/ui/ViewPorts/NumInput.cpp`: a UI sozinha não as traz.
