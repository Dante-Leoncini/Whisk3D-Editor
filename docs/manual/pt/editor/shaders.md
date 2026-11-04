# Shaders escritos

Um **shader escrito** é um arquivo de texto (`.glsl`) que decide como um material é desenhado, pixel por pixel. Serve para efeitos que o material comum não tem: vidro fosco que deixa ver borrado o que está atrás, água que se mexe, distorção de calor, desenho animado, um brilho que pulsa…

Você escreve só a sua parte: uma função `Fragmento()` (a cor de cada pixel) e, se quiser, uma `Vertice()` (mover os vértices). Todo o resto o Whisk3D coloca.

## Onde funciona

O mesmo arquivo funciona igual em todos os backends gráficos:

| Backend | `@vertice` | `@fragmento` |
|---|---|---|
| OpenGL ES 2.0: Android, navegador (WebGL), Nokia N8, PC com **Gráficos → OpenGL ES 2.0** | Na GPU | Na GPU |
| Vulkan (PC) | Na GPU | Na GPU |
| OpenGL do PC (o de fábrica), OpenGL ES 1.1 (Nokia N95), Nintendo 3DS, Dreamcast | **Na CPU** | Não: o material é desenhado como sempre |

Num backend sem shaders nada quebra: o **`@vertice` roda na CPU** (a água com ondas se mexe igual no N95) e a cor vem do material normal. O cartão Material avisa: *"Este backend gráfico não tem shaders: a seção @vertice roda na CPU e o resto do material é desenhado como sempre"*.

### O `@vertice` na CPU

Onde não há shaders, o Whisk3D traz um interpretador que lê o **mesmo texto** do `@vertice` e o executa vértice por vértice antes de desenhar. Não é preciso escrever nada diferente. Ele entende o que se usa para mover vértices:

- `float`, `int`, `bool`, `vec2`, `vec3`, `vec4`; constantes (`const float VELOCIDAD = 2.0;`) e funções próprias (com `in`, `out`, `inout`);
- `if` / `else`, `for` (com `break` e `continue`), `return`, o ternário `? :`;
- os operadores, os componentes (`pos.y`, `n.xz`, também para escrever) e os construtores (`vec3(...)`);
- as funções numéricas do GLSL: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `pow`, `exp`, `log`, `sqrt`, `abs`, `sign`, `floor`, `ceil`, `fract`, `mod`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `length`, `distance`, `dot`, `cross`, `normalize`, `reflect`, `refract`;
- `w3d_tiempo`, `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`.

Não entende matrizes, arrays, `while` nem `#define`: se você os usar, o erro diz isso com a sua linha (e na GPU funcionaria). Roda uma vez por quadro sobre todos os vértices da malha: no N95 convém usá-lo em malhas pequenas (algumas centenas de vértices, como a água de exemplo). Funciona se **todas** as partes da malha usam o mesmo shader; se não, a malha é desenhada sem se mover.

> **Por que o mesmo arquivo funciona em todo lugar:** o arquivo não tem nada próprio de um backend. Cada backend acrescenta na frente o que é seu (as entradas `w3d_*`, a precisão, o `main`). O OpenGL ES 2.0 o compila direto; o Vulkan o converte para SPIR-V por dentro. Se amanhã o Whisk3D ganhar outro backend, seu shader continua servindo.

## Usar um shader num material

1. Selecione o objeto e abra a aba **Material** de Propriedades.
2. Lá embaixo de tudo, marque **Avançado: shader personalizado**. Se o material ainda não tem arquivo, um novo (`shader.glsl`) é criado com um modelo, passa a ser usado na hora e abre no IDE.
3. Em **Shader** você escolhe o arquivo:
   - os `.glsl` que já estão no projeto;
   - **Novo shader**: outro arquivo com o modelo;
   - **Carregar do disco...**: um `.glsl` do seu computador. Ele é copiado para dentro do projeto, como as texturas e os scripts.
4. O botãozinho à direita de **Shader** abre o arquivo no **IDE** (é preciso ter um IDE no layout).
5. Você escreve, e **Ctrl+S** salva e compila. Também dá para tocar em **Compilar** no material ou na barra do IDE (aparece ao lado do nome do arquivo quando é um `.glsl`).
6. Embaixo de **Compilar** fica o resultado: **Compilado**, ou o erro em vermelho com a sua linha.

Não é preciso salvar o projeto para começar. Se você ainda não o salvou, o shader é gravado numa pasta de trabalho do editor; ao salvar o projeto ele entra dentro do `.w3d`, na pasta `shaders/`, e o material e o IDE passam a usar essa cópia. A partir daí viaja com o projeto (para o celular, para o jogo compilado, para outro computador). Desmarcando, o material volta a ser desenhado como sempre; o arquivo continua escolhido.

Se um shader não compila, o objeto é desenhado com seu material normal até você consertá-lo. O editor não cai.

### Pela Biblioteca: arrastar um shader para uma malha

Os shaders do projeto aparecem na **Biblioteca** (o Outliner no modo Biblioteca), em **Shaders**, com quantos objetos os usam. A partir dali:

- **Arraste-o sobre uma malha** no Viewport 3D: enquanto você o move, a **parte da malha** que fica sob o ponteiro é desenhada com esse shader, assim você vê em que parte vai aplicá-lo. Se você passar para outra parte ou sair da malha, ela volta ao seu material. Ao soltar, o material dessa parte passa a usar o shader (com o material padrão, a parte recebe um material novo). Desfaz-se com **Ctrl+Z**.
- O mesmo com um **material**: enquanto você o arrasta, a parte aparece com ele.
- **Abrir no IDE**, renomeá-lo, movê-lo de pasta ou tirá-lo de dentro do `.w3d`, como um script.
- Um `.glsl` arrastado **do explorador de arquivos do seu computador** para a janela entra no projeto; se você o soltar sobre uma malha, além disso é aplicado a essa parte.

## Como é o arquivo

```glsl
// os comentários vão com // ou /* */

@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    // opcional: mover os vértices
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
```

- **`@fragmento`** marca onde começa o código dos pixels e **`@vertice`** o dos vértices. As duas são opcionais: um arquivo sem marcas é todo fragmento.
- **`vec4 Fragmento()`** é obrigatória: devolve a cor do pixel (vermelho, verde, azul, alfa; de 0 a 1).
- **`void Vertice(inout vec3 pos, inout vec3 normal)`** é opcional. Recebe a posição e a normal do vértice no espaço do **objeto** (Y para cima, em metros) e você pode mudá-las.
- Você pode escrever suas próprias funções e constantes (`const float VELOCIDAD = 2.0;`), acima de onde as usa.
- A linguagem é **GLSL ES 1.00**, a do WebGL 1. Você tem `float`, `vec2/3/4`, `mat3/4`, `sin`, `cos`, `mix`, `clamp`, `pow`, `dot`, `normalize`, `reflect`, `refract`, `fract`, `smoothstep` e o resto do GLSL.
- **Não** escreva `main`, nem declare `uniform`, `attribute`, `varying` ou `precision`: isso o Whisk3D coloca.
- Os erros dizem a linha do **seu arquivo** (por exemplo `0(4)` ou `w3d.frag:4` é a linha 4).

## O que você tem para usar

### Em `Fragmento()`

| Nome | Tipo | O que é |
|---|---|---|
| `w3d_uv` | `vec2` | As coordenadas UV do ponto (da primeira camada UV). |
| `w3d_Normal()` | `vec3` | A normal do ponto, já normalizada, no espaço da câmera. Se a face de trás está visível, aponta para o outro lado. |
| `w3d_normal` | `vec3` | A normal sem normalizar nem inverter. |
| `w3d_posVista` | `vec3` | A posição do ponto no espaço da câmera (a câmera está em 0,0,0 olhando para −Z). |
| `w3d_HaciaCamara()` | `vec3` | A direção do ponto para a câmera. |
| `w3d_HaciaLuz()` | `vec3` | A direção do ponto para a primeira luz. |
| `w3d_Luz(n)` | `vec3` | A luz que chega com a normal `n`: o ambiente mais a primeira luz (difusa). |
| `w3d_luzColor` | `vec4` | A cor da primeira luz (com sua intensidade). |
| `w3d_luzPos` | `vec4` | A posição da primeira luz no espaço da câmera (`w` = 0: é uma direção, o sol). |
| `w3d_ambiente` | `vec4` | A luz ambiente. |
| `w3d_color` | `vec4` | A **Cor** do material (com seu alfa). |
| `w3d_emision` | `vec4` | A **Emissão** do material. |
| `w3d_Textura(uv)` | `vec4` | A textura do material nessas UV (branco se não tiver). A `v` começa **em cima** da imagem (0 = a linha de cima, 1 = a de baixo). |
| `w3d_colorVertice` | `vec4` | A cor de vértice (se a malha tiver e o material usar **Cor de vértice**). |
| `w3d_rugosidad`, `w3d_metalico` | `float` | A **Rugosidade** e o **Metálico** do material. |
| `w3d_ior`, `w3d_transmision` | `float` | O **IOR** e a **Transmissão** do material (os do ray tracing). |
| `w3d_tiempo` | `float` | Os segundos, para animar. No editor é o tempo da **linha do tempo** (o quadro atual ÷ os FPS): pausado fica parado, com **Play** avança e arrastando a linha do tempo você vê qualquer momento. No jogo exportado é o tempo desde que ele começou. |
| `w3d_PuntoPantalla()` | `vec2` | Onde o pixel está na vista: (0,0) embaixo à esquerda, (1,1) em cima à direita. |
| `w3d_Pixel()` | `vec2` | O tamanho de um pixel nessas mesmas unidades. |
| `w3d_Pantalla(p)` | `vec4` | **O que já está desenhado** no ponto `p` da vista. Veja abaixo. |
| `w3d_AMundo(v)` | `vec3` | Passa uma direção do espaço da câmera para o do **mundo** (Y para cima). Serve para ler um mapa de ambiente que não gire com a câmera (veja *Reflexo de ambiente*). |

### Em `Vertice()`

`pos` e `normal` (do objeto), e `w3d_tiempo`.

## Ver o que está atrás: `w3d_Pantalla`

Um material que lê a tela com `w3d_Pantalla` pode mostrar o que está atrás do objeto, e deslocá-lo, borrá-lo ou tingi-lo. É assim que se fazem o vidro, a água, o ar quente ou um campo de força.

- As malhas com um shader que lê a tela são desenhadas **por último**, depois do resto da cena, para que o que está atrás já esteja desenhado. Não importa onde estejam no Outliner.
- Antes de cada uma, a vista é copiada para uma textura (uma cópia por malha e por parte de malha). Num PC ou num celular moderno é barato. Num celular antigo, melhor poucos objetos assim.
- As **partículas** são desenhadas depois, então não aparecem através do vidro.
- Convém que o material **não** seja **Transparente**: o shader já mistura o que está atrás e devolve alfa 1.

## Exemplos

Todos estes estão como arquivos em **`examples/shaders/`** do repositório, com suas texturas em `examples/shaders/texturas/`, e aplicados a objetos no projeto **`examples/shaders/Ejemplos_Shaders.w3d`**: abra-o e experimente. Você também pode arrastar os `.glsl` para o seu projeto.

| Arquivo | O que faz |
|---|---|
| `vidrio_esmerilado.glsl` | Vidro para qualquer forma: o que está atrás borrado e dobrado. |
| `vidrio_plano.glsl` | Vidro fosco para um **plano** (uma janela, um box de vidro); a textura diz onde ele é fosco. |
| `reflejo_entorno.glsl` | Metal ou plástico que reflete uma foto panorâmica do ambiente (a textura). |
| `reflejo_matcap.glsl` | O reflexo mais barato: uma textura com uma esfera pintada. |
| `latido.glsl` | Um brilho que pulsa (`w3d_tiempo`). |
| `dibujo_animado.glsl` | A luz em três tons. |
| `agua.glsl` | Ondas com `@vertice` e o que está embaixo deslocado. |
| `bandera.glsl` | Uma bandeira tremulando (só `@vertice`: funciona também no N95). |
| `respirar.glsl` | Infla e desinfla ao longo das suas normais (só `@vertice`). |
| `torcer.glsl` | Torce-se em volta do eixo vertical (só `@vertice`). |
| `saltar.glsl` | Quica e se achata ao tocar o chão (só `@vertice`). |

Os de vértices estão aplicados a malhas pequenas em **`examples/shaders/Ejemplos_Vertex_N95.w3d`**, pensado para o N95 (lá o `@vertice` roda na CPU). Eles se movem com o **Play** da linha do tempo.

### Uma cor que pulsa

```glsl
vec4 Fragmento() {
    float latido = 0.5 + 0.5 * sin(w3d_tiempo * 4.0);
    vec3 luz = w3d_Luz(w3d_Normal());
    return vec4(w3d_color.rgb * luz + vec3(1.0, 0.2, 0.1) * latido * 0.5, 1.0);
}
```

Um shader que usa `w3d_tiempo` se move com o **Play** da linha do tempo (como uma animação); em pausa fica parado e o editor não redesenha à toa. No jogo exportado ele se move sozinho.

### Desenho animado (três tons)

```glsl
vec4 Fragmento() {
    float l = dot(w3d_Normal(), w3d_HaciaLuz());
    float tono = l > 0.6 ? 1.0 : (l > 0.1 ? 0.65 : 0.35);
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * tono, base.a);
}
```

### Água que se mexe (`@vertice`)

```glsl
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    float ola = sin(pos.x * 2.0 + w3d_tiempo * 2.0) * 0.1 + sin(pos.z * 3.0 + w3d_tiempo * 1.5) * 0.05;
    pos.y += ola;
}

@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 p = w3d_PuntoPantalla() + n.xy * 0.03;          // o que está embaixo, um pouco deslocado
    vec3 fondo = w3d_Pantalla(p).rgb;
    float borde = pow(1.0 - max(dot(n, w3d_HaciaCamara()), 0.0), 3.0);
    return vec4(mix(fondo * w3d_color.rgb, vec3(0.8, 0.9, 1.0), borde * 0.6), 1.0);
}
```

Para a onda aparecer, o plano precisa de vértices: subdivida-o no Modo Edição.

### Vidro fosco

É `examples/shaders/vidrio_esmerilado.glsl` (a esfera de vidro do projeto de exemplos). O que está atrás aparece borrado e um pouco dobrado:

```glsl
@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // a refração: lê-se a tela deslocada conforme a normal
    vec2 p = w3d_PuntoPantalla() + n.xy * (w3d_ior - 1.0) * 0.15;

    // o borrado: a média de 13 leituras ao redor; a rugosidade abre o círculo
    vec2 r = w3d_Pixel() * (1.0 + 18.0 * w3d_rugosidad);
    vec3 c = w3d_Pantalla(p).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 2.5,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  2.5)).rgb;
    c += w3d_Pantalla(p + r * vec2(-2.5, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0, -2.5)).rgb;
    c /= 13.0;

    vec3 atras = c * mix(vec3(1.0), w3d_color.rgb, 0.35);     // tingido pela cor do vidro
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 col = mix(superficie, atras, w3d_transmision);

    // o brilho da luz e a borda mais clara
    vec3 medio = normalize(w3d_HaciaLuz() + ojo);
    float brillo = pow(max(dot(n, medio), 0.0), 80.0) * (1.0 - w3d_rugosidad);
    float borde = pow(1.0 - max(dot(n, ojo), 0.0), 3.0);
    col += w3d_luzColor.rgb * brillo + vec3(0.2) * borde;
    return vec4(col, 1.0);
}
```

É controlado pelo material: **Transmissão** (0 = vidro opaco da cor do material, 1 = vê-se tudo o que está atrás), **IOR** (quanto o que está atrás se dobra) e **Rugosidade** (quanto borra). São os mesmos valores que o **ray tracing** usa, então na Prévia Render o mesmo objeto é vidro de verdade.

### Vidro plano fosco

Para um **plano**: uma janela, um box de vidro, uma tela de vidro. É `examples/shaders/vidrio_plano.glsl`. Faz a média de 17 leituras do que está atrás em dois anéis; a **Rugosidade** abre os anéis. A **textura** do material diz *onde* ele é fosco: branco = borrado, preto = liso. Com `texturas/escarcha.png` ficam manchas, uma janelinha limpa e gotas escorrendo; sem textura, o vidro inteiro é fosco. Convém tirar o **Culling** do plano para vê-lo dos dois lados.

```glsl
float esmerilado = w3d_Textura(w3d_uv).r * w3d_rugosidad;   // quanto ESTE ponto borra
vec2 r = w3d_Pixel() * (1.0 + 26.0 * esmerilado);           // o raio das leituras
```

### Reflexo de ambiente (como se usa uma textura para refletir)

É `examples/shaders/reflejo_entorno.glsl`. A **textura do material** é uma foto **panorâmica** do que rodeia o objeto: uma imagem *equirretangular*, duas vezes mais larga que alta, que cobre 360° em volta e 180° de cima a baixo (como `texturas/entorno.png`: céu, sol, prédios e um piso xadrez). Para cada pixel:

1. `reflect(-ojo, n)` diz para onde **o olhar ricocheteia**, no espaço da câmera.
2. `w3d_AMundo(...)` passa essa direção para o **mundo**, assim o reflexo fica parado mesmo que a câmera gire.
3. `Panorama(...)` converte a direção em um ponto da foto: o ângulo em volta (`atan`) é a `u` e o ângulo de cima a baixo (`acos`) é a `v`.
4. Mistura-se com a superfície segundo **Fresnel** (de lado reflete mais) e o **Metálico** (um metal reflete tudo, tingido da sua **Cor**).

```glsl
@fragmento
const float PI = 3.14159265;

vec2 Panorama(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;      // a volta em torno (0..1)
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;       // de cima (0) a baixo (1)
    return vec2(u, v);
}

vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();
    vec3 r = reflect(-ojo, n);
    vec3 entorno = w3d_Textura(Panorama(normalize(w3d_AMundo(r)))).rgb;
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(n, ojo), 0.0), 5.0);
    float cuanto = mix(fresnel, 1.0, w3d_metalico);
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 reflejo = entorno * mix(vec3(1.0), w3d_color.rgb, w3d_metalico);
    return vec4(mix(superficie, reflejo, cuanto), 1.0);
}
```

Qualquer foto panorâmica 2:1 serve (as de 360° de um celular, ou HDRIs convertidas para PNG).

### Reflexo matcap

É `examples/shaders/reflejo_matcap.glsl`: uma única leitura de textura, roda em qualquer GPU. A textura é uma **esfera pintada** (`texturas/matcap.png`); cada pixel pega a cor do ponto da esfera que olha para o mesmo lado que a sua normal. O reflexo segue a câmera (não o mundo).

```glsl
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 uv = vec2(n.x, -n.y) * 0.49 + 0.5;   // (a v da textura começa em cima: y invertido)
    return vec4(w3d_Textura(uv).rgb * w3d_color.rgb, 1.0);
}
```

## Numa rotina: o passo **Shader**

Um shader escrito é como um **estado do GL**: ele é ativado e o que se desenha depois o usa. Numa **rotina** (Adicionar → Draw → **Shader**) o passo tem o **arquivo** `.glsl`, a caixa **On** e quatro números: **IOR**, **Transmission**, **Roughness** e **Metallic** (chegam como `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`; podem ser animados ou lidos de uma memória).

```
Shader          vidrio_plano.glsl  On   (IOR 1.5, Roughness 0.6)
Vertex pointer  Ventana
Draw elements   Ventana
Shader          Off
Draw elements   Marco           ← já sem shader
```

Com **On**, os "Draw elements" seguintes usam o shader; com a caixa desmarcada ele é retirado. Ao terminar a rotina, ele sai sozinho. Funciona em **OpenGL ES 2.0 e Vulkan**; em OpenGL ES 1.1 e no GL fixo o passo não faz nada (os desenhos de uma rotina apontam direto para os dados da malha: o `@vertice` na CPU é para os materiais). Um shader que lê a tela (`w3d_Pantalla`) vê o que a rotina e a cena desenharam **antes** desse passo: a ordem é você quem decide.

## Com o ray tracing

O ray tracing não executa o seu shader. Ele usa os valores do material: **Cor**, **Rugosidade**, **Metálico**, **Transmissão**, **IOR** e a textura. Se no shader você usar esses mesmos valores (como o vidro fosco), a Prévia Material e a Prévia Render dizem a mesma coisa.

**Transmissão** (de 0 a 1, no cartão Material com o ray tracing ligado) diz quanto da superfície é vidro: a luz passa dobrada pelo **IOR** e, com **Rugosidade**, sai fosca. O **alfa** é outra coisa: o que é transparente pelo alfa (ou pelo seu mapa) não está lá, e por ali se vê o que está atrás reto, sem dobrar. Com alfa 50 % e transmissão 1, metade passa reto e a outra metade refratada. Com transmissão 0, o IOR dobra o que se vê através do alfa, como nas versões anteriores.

## O que não faz (ainda)

- Usa uma luz só: a primeira da cena (ou a da prévia).
- Não leva a **névoa** nem os **planos de corte** dos espelhos.
- Em **Sólido** e nas vistas de depuração (normais, alfa, vetores) aparece o material normal.
- Não tem parâmetros próprios: usa os do material (cor, rugosidade, metálico, transmissão, IOR, emissão).

## Erros comuns

| Mensagem | O que acontece |
|---|---|
| `undefined variable` / `undeclared identifier` | Um nome escrito errado ou que não existe (`w3d_Normal()` com parênteses?). |
| `cannot convert` / `wrong operand types` | Mistura de tipos: `1` é `int`, escreva `1.0`. Um `vec3` não se soma com um `vec4`. |
| `Fragmento: no matching overloaded function` ou não compila sem `Fragmento` | Falta a função `vec4 Fragmento()` ou ela devolve outro tipo. |
| `redefinition of main` | Não escreva `main`: o Whisk3D o coloca. |
| Não se vê o que está atrás | O shader usa `w3d_Pantalla` mas o material está em **Transparente** com alfa baixo, ou o que está atrás é desenhado depois (partículas). |
