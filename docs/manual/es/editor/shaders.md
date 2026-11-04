# Shaders escritos

Un **shader escrito** es un archivo de texto (`.glsl`) que decide cómo se dibuja un material, píxel por píxel. Sirve para efectos que el material común no tiene: vidrio esmerilado que deja ver borroso lo de atrás, agua que se mueve, distorsión de calor, dibujo animado, un brillo que late…

Escribís solamente lo tuyo: una función `Fragmento()` (el color de cada píxel) y, si querés, una `Vertice()` (mover los vértices). Todo lo demás lo pone Whisk3D.

## Dónde anda

El mismo archivo anda igual en todos los backends gráficos:

| Backend | `@vertice` | `@fragmento` |
|---|---|---|
| OpenGL ES 2.0: Android, navegador (WebGL), Nokia N8, PC con **Gráficos → OpenGL ES 2.0** | En la GPU | En la GPU |
| Vulkan (PC) | En la GPU | En la GPU |
| OpenGL de la PC (el de fábrica), OpenGL ES 1.1 (Nokia N95), Nintendo 3DS, Dreamcast | **En la CPU** | No: el material se dibuja como siempre |

En un backend sin shaders no se rompe nada: el **`@vertice` corre en la CPU** (el agua con olas se mueve igual en el N95) y el color sale del material normal. La tarjeta Material lo avisa: *"Este backend gráfico no tiene shaders: la sección @vertice corre en la CPU y el resto del material se dibuja como siempre"*.

### El `@vertice` en la CPU

Donde no hay shaders, Whisk3D trae un intérprete que lee el **mismo texto** de `@vertice` y lo corre vértice por vértice antes de dibujar. No hay que escribir nada distinto. Entiende lo que se usa para mover vértices:

- `float`, `int`, `bool`, `vec2`, `vec3`, `vec4`; constantes (`const float VELOCIDAD = 2.0;`) y funciones propias (con `in`, `out`, `inout`);
- `if` / `else`, `for` (con `break` y `continue`), `return`, la ternaria `? :`;
- los operadores, los componentes (`pos.y`, `n.xz`, también para escribir) y los constructores (`vec3(...)`);
- las funciones de números de GLSL: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `pow`, `exp`, `log`, `sqrt`, `abs`, `sign`, `floor`, `ceil`, `fract`, `mod`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `length`, `distance`, `dot`, `cross`, `normalize`, `reflect`, `refract`;
- `w3d_tiempo`, `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`.

No entiende matrices, arreglos, `while` ni `#define`: si los usás, el error lo dice con su renglón (y en la GPU andaría igual). Corre una vez por cuadro sobre todos los vértices de la malla: en el N95 conviene usarlo en mallas chicas (unos cientos de vértices, como el agua de ejemplo). Funciona si **todas** las partes de la malla usan el mismo shader; si no, la malla se dibuja sin mover.

> **Por qué el mismo archivo anda en todos lados:** el archivo no tiene nada propio de un backend. Cada backend le agrega adelante lo suyo (las entradas `w3d_*`, la precisión, el `main`). OpenGL ES 2.0 lo compila directo; Vulkan lo pasa a SPIR-V por dentro. Si mañana Whisk3D suma otro backend, tu shader sigue sirviendo.

## Usar un shader en un material

1. Elegí el objeto y abrí la pestaña **Material** de Propiedades.
2. Abajo de todo, tildá **Avanzado: shader personalizado**. Si el material todavía no tiene archivo, se crea uno nuevo (`shader.glsl`) con una plantilla, se usa en el acto y se abre en el IDE.
3. En **Shader** elegís el archivo:
   - los `.glsl` que ya están en el proyecto;
   - **Shader nuevo**: otro archivo con la plantilla;
   - **Cargar del disco...**: un `.glsl` de tu computadora. Se copia adentro del proyecto, como las texturas y los scripts.
4. El botoncito a la derecha de **Shader** abre el archivo en el **IDE** (hace falta un IDE en el layout).
5. Escribís, y **Ctrl+S** guarda y compila. También podés tocar **Compilar** en el material o en la barra del IDE (aparece al lado del nombre del archivo cuando es un `.glsl`).
6. Debajo de **Compilar** queda el resultado: **Compilado**, o el error en rojo con su renglón.

No hace falta guardar el proyecto para empezar. Si todavía no lo guardaste, el shader se escribe en una carpeta de trabajo del editor; al guardar el proyecto entra adentro del `.w3d`, en la carpeta `shaders/`, y el material y el IDE pasan a usar esa copia. Desde ahí viaja con el proyecto (en el celular, en el juego compilado, en otra computadora). Sacás el tilde y el material vuelve a dibujarse como siempre; el archivo queda elegido.

Si un shader no compila, el objeto se dibuja con su material normal hasta que lo arregles. El editor no se cae.

### Desde la Biblioteca: arrastrar un shader a una malla

Los shaders del proyecto aparecen en la **Biblioteca** (el Outliner en modo Biblioteca), en **Shaders**, con cuántos objetos los usan. Desde ahí:

- **Arrastralo sobre una malla** en la Vista 3D: mientras lo movés, la **parte de la malla** que queda bajo el puntero se dibuja con ese shader, así ves a qué parte le vas a pegar. Si te movés a otra parte o salís de la malla, vuelve a su material. Al soltar, el material de esa parte pasa a usar el shader (con el material por defecto, la parte recibe un material nuevo). Se deshace con **Ctrl+Z**.
- Lo mismo con un **material**: mientras lo arrastrás, la parte se ve con él.
- **Abrir en el IDE**, renombrarlo, moverlo de carpeta o sacarlo afuera del `.w3d`, como un script.
- Un `.glsl` arrastrado **desde el explorador de tu computadora** a la ventana entra al proyecto; si lo soltás sobre una malla, además se le pone a esa parte.

## Cómo es el archivo

```glsl
// los comentarios van con // o /* */

@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    // opcional: mover los vértices
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
```

- **`@fragmento`** marca dónde empieza el código de los píxeles y **`@vertice`** el de los vértices. Las dos son opcionales: un archivo sin marcas es todo fragmento.
- **`vec4 Fragmento()`** es obligatoria: devuelve el color del píxel (rojo, verde, azul, alfa; de 0 a 1).
- **`void Vertice(inout vec3 pos, inout vec3 normal)`** es opcional. Recibe la posición y la normal del vértice en el espacio del **objeto** (Y para arriba, en metros) y las podés cambiar.
- Podés escribir tus propias funciones y constantes (`const float VELOCIDAD = 2.0;`), arriba de donde las usás.
- El lenguaje es **GLSL ES 1.00**, el de WebGL 1. Tenés `float`, `vec2/3/4`, `mat3/4`, `sin`, `cos`, `mix`, `clamp`, `pow`, `dot`, `normalize`, `reflect`, `refract`, `fract`, `smoothstep` y el resto de GLSL.
- **No** escribas `main`, ni declares `uniform`, `attribute`, `varying` o `precision`: eso lo pone Whisk3D.
- Los errores dicen el renglón de **tu archivo** (por ejemplo `0(4)` o `w3d.frag:4` es el renglón 4).

## Lo que tenés para usar

### En `Fragmento()`

| Nombre | Tipo | Qué es |
|---|---|---|
| `w3d_uv` | `vec2` | Las coordenadas UV del punto (de la primera capa UV). |
| `w3d_Normal()` | `vec3` | La normal del punto, ya normalizada, en el espacio de la cámara. Si se ve la cara de atrás, apunta para el otro lado. |
| `w3d_normal` | `vec3` | La normal sin normalizar ni dar vuelta. |
| `w3d_posVista` | `vec3` | La posición del punto en el espacio de la cámara (la cámara está en 0,0,0 mirando hacia −Z). |
| `w3d_HaciaCamara()` | `vec3` | La dirección del punto hacia la cámara. |
| `w3d_HaciaLuz()` | `vec3` | La dirección del punto hacia la primera luz. |
| `w3d_Luz(n)` | `vec3` | La luz que le llega con la normal `n`: el ambiente más la primera luz (difusa). |
| `w3d_luzColor` | `vec4` | El color de la primera luz (con su intensidad). |
| `w3d_luzPos` | `vec4` | La posición de la primera luz en el espacio de la cámara (`w` = 0: es una dirección, el sol). |
| `w3d_ambiente` | `vec4` | La luz ambiente. |
| `w3d_color` | `vec4` | El **Color** del material (con su alfa). |
| `w3d_emision` | `vec4` | La **Emisión** del material. |
| `w3d_Textura(uv)` | `vec4` | La textura del material en esas UV (blanco si no tiene). La `v` arranca **arriba** de la imagen (0 = la fila de arriba, 1 = la de abajo). |
| `w3d_colorVertice` | `vec4` | El color de vértice (si la malla tiene y el material usa **Color de vértice**). |
| `w3d_rugosidad`, `w3d_metalico` | `float` | La **Rugosidad** y el **Metálico** del material. |
| `w3d_ior`, `w3d_transmision` | `float` | El **IOR** y la **Transmisión** del material (los del trazado de rayos). |
| `w3d_tiempo` | `float` | Los segundos, para animar. En el editor es el tiempo de la **línea de tiempo** (el cuadro actual ÷ los FPS): con pausa queda quieto, con **Play** avanza y arrastrando la línea de tiempo ves cualquier momento. En el juego exportado es el tiempo desde que arrancó. |
| `w3d_PuntoPantalla()` | `vec2` | Dónde está el píxel en la vista: (0,0) abajo a la izquierda, (1,1) arriba a la derecha. |
| `w3d_Pixel()` | `vec2` | Lo que mide un píxel en esas mismas unidades. |
| `w3d_Pantalla(p)` | `vec4` | **Lo que ya está dibujado** en el punto `p` de la vista. Ver abajo. |
| `w3d_AMundo(v)` | `vec3` | Pasa una dirección del espacio de la cámara al del **mundo** (Y para arriba). Sirve para leer un mapa de entorno que no gire con la cámara (ver *Reflejo de entorno*). |

### En `Vertice()`

`pos` y `normal` (del objeto), y `w3d_tiempo`.

## Ver lo que hay detrás: `w3d_Pantalla`

Un material que lee la pantalla con `w3d_Pantalla` puede mostrar lo que hay detrás del objeto, y moverlo, borronearlo o teñirlo. Así se hacen el vidrio, el agua, el aire caliente o un campo de fuerza.

- Las mallas con un shader que lee la pantalla se dibujan **al final**, después del resto de la escena, para que lo de atrás ya esté dibujado. No importa dónde estén en el Outliner.
- Antes de cada una se copia la vista a una textura (una copia por malla y por parte de malla). En una PC o un teléfono moderno es barato. En un teléfono viejo, mejor pocos objetos así.
- Las **partículas** se dibujan después, así que no se ven a través del vidrio.
- Conviene que el material **no** sea **Transparente**: el shader ya mezcla lo de atrás y devuelve alfa 1.

## Ejemplos

Todos estos están como archivos en **`examples/shaders/`** del repositorio, con sus texturas en `examples/shaders/texturas/`, y puestos en objetos en el proyecto **`examples/shaders/Ejemplos_Shaders.w3d`**: abrilo y probalos. También podés arrastrar los `.glsl` a tu proyecto.

| Archivo | Qué hace |
|---|---|
| `vidrio_esmerilado.glsl` | Vidrio para cualquier forma: lo de atrás borroso y doblado. |
| `vidrio_plano.glsl` | Vidrio esmerilado para un **plano** (una ventana, una mampara); la textura dice dónde está esmerilado. |
| `reflejo_entorno.glsl` | Metal o plástico que refleja una foto panorámica del entorno (la textura). |
| `reflejo_matcap.glsl` | El reflejo más barato: una textura con una esfera pintada. |
| `latido.glsl` | Un brillo que late (`w3d_tiempo`). |
| `dibujo_animado.glsl` | La luz en tres tonos. |
| `agua.glsl` | Olas con `@vertice` y lo de abajo corrido. |
| `bandera.glsl` | Una bandera que flamea (solo `@vertice`: anda también en el N95). |
| `respirar.glsl` | Se infla y desinfla sobre sus normales (solo `@vertice`). |
| `torcer.glsl` | Se retuerce alrededor del eje vertical (solo `@vertice`). |
| `saltar.glsl` | Rebota y se aplasta al tocar el piso (solo `@vertice`). |

Los de vértices están puestos en mallas chicas en **`examples/shaders/Ejemplos_Vertex_N95.w3d`**, pensado para el N95 (ahí el `@vertice` corre en la CPU). Se mueven con **Play** de la línea de tiempo.

### Un color que late

```glsl
vec4 Fragmento() {
    float latido = 0.5 + 0.5 * sin(w3d_tiempo * 4.0);
    vec3 luz = w3d_Luz(w3d_Normal());
    return vec4(w3d_color.rgb * luz + vec3(1.0, 0.2, 0.1) * latido * 0.5, 1.0);
}
```

Un shader que usa `w3d_tiempo` se mueve con **Play** de la línea de tiempo (como una animación); en pausa queda quieto y el editor no redibuja de más. En el juego exportado se mueve solo.

### Dibujo animado (tres tonos)

```glsl
vec4 Fragmento() {
    float l = dot(w3d_Normal(), w3d_HaciaLuz());
    float tono = l > 0.6 ? 1.0 : (l > 0.1 ? 0.65 : 0.35);
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * tono, base.a);
}
```

### Agua que se mueve (`@vertice`)

```glsl
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    float ola = sin(pos.x * 2.0 + w3d_tiempo * 2.0) * 0.1 + sin(pos.z * 3.0 + w3d_tiempo * 1.5) * 0.05;
    pos.y += ola;
}

@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 p = w3d_PuntoPantalla() + n.xy * 0.03;          // lo de abajo, un poco corrido
    vec3 fondo = w3d_Pantalla(p).rgb;
    float borde = pow(1.0 - max(dot(n, w3d_HaciaCamara()), 0.0), 3.0);
    return vec4(mix(fondo * w3d_color.rgb, vec3(0.8, 0.9, 1.0), borde * 0.6), 1.0);
}
```

Para que la ola se note, el plano necesita vértices: subdividilo en Modo Edición.

### Vidrio esmerilado

Es `examples/shaders/vidrio_esmerilado.glsl` (la esfera de vidrio del proyecto de ejemplos). Lo de atrás se ve borroso y un poco doblado:

```glsl
@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // la refracción: se lee la pantalla corrida según la normal
    vec2 p = w3d_PuntoPantalla() + n.xy * (w3d_ior - 1.0) * 0.15;

    // lo borroso: el promedio de 13 lecturas alrededor; la rugosidad abre el círculo
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

    vec3 atras = c * mix(vec3(1.0), w3d_color.rgb, 0.35);     // teñido por el color del vidrio
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 col = mix(superficie, atras, w3d_transmision);

    // el brillo de la luz y el borde más claro
    vec3 medio = normalize(w3d_HaciaLuz() + ojo);
    float brillo = pow(max(dot(n, medio), 0.0), 80.0) * (1.0 - w3d_rugosidad);
    float borde = pow(1.0 - max(dot(n, ojo), 0.0), 3.0);
    col += w3d_luzColor.rgb * brillo + vec3(0.2) * borde;
    return vec4(col, 1.0);
}
```

Se maneja desde el material: **Transmisión** (0 = vidrio opaco del color del material, 1 = se ve todo lo de atrás), **IOR** (cuánto se dobla lo de atrás) y **Rugosidad** (cuánto se borronea). Son los mismos valores que usa el **trazado de rayos**, así que en la Vista Render el mismo objeto es vidrio de verdad.

### Vidrio plano esmerilado

Para un **plano**: una ventana, una mampara, una pantalla de vidrio. Es `examples/shaders/vidrio_plano.glsl`. Promedia 17 lecturas de lo de atrás en dos anillos; la **Rugosidad** abre los anillos. La **textura** del material dice *dónde* está esmerilado: blanco = borroso, negro = liso. Con `texturas/escarcha.png` quedan manchas, una ventanita limpia y gotas que bajan; sin textura, todo el vidrio es esmerilado. Al plano conviene sacarle el **Culling** para verlo de los dos lados.

```glsl
float esmerilado = w3d_Textura(w3d_uv).r * w3d_rugosidad;   // cuanto se borronea ESTE punto
vec2 r = w3d_Pixel() * (1.0 + 26.0 * esmerilado);           // el radio de las lecturas
```

### Reflejo de entorno (cómo se usa una textura para reflejar)

Es `examples/shaders/reflejo_entorno.glsl`. La **textura del material** es una foto **panorámica** de lo que rodea al objeto: una imagen *equirectangular*, el doble de ancha que de alta, que cubre 360° de costado y 180° de arriba a abajo (como `texturas/entorno.png`: cielo, sol, edificios y un piso a cuadros). Para cada píxel:

1. `reflect(-ojo, n)` dice hacia dónde **rebota la mirada**, en el espacio de la cámara.
2. `w3d_AMundo(...)` pasa esa dirección al **mundo**, así el reflejo queda quieto aunque gire la cámara.
3. `Panorama(...)` convierte la dirección en un punto de la foto: el ángulo alrededor (`atan`) es la `u` y el ángulo de arriba a abajo (`acos`) es la `v`.
4. Se mezcla con la superficie según **Fresnel** (de costado se refleja más) y el **Metálico** (un metal refleja todo, teñido de su **Color**).

```glsl
@fragmento
const float PI = 3.14159265;

vec2 Panorama(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;      // la vuelta alrededor (0..1)
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;       // de arriba (0) a abajo (1)
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

Cualquier foto panorámica 2:1 sirve (las de 360° de un celular, o las HDRI pasadas a PNG).

### Reflejo matcap

Es `examples/shaders/reflejo_matcap.glsl`: una sola lectura de textura, anda en cualquier GPU. La textura es una **esfera pintada** (`texturas/matcap.png`); cada píxel toma el color del punto de la esfera que mira para el mismo lado que su normal. El reflejo sigue a la cámara (no al mundo).

```glsl
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 uv = vec2(n.x, -n.y) * 0.49 + 0.5;   // (la v de la textura arranca arriba: y al revés)
    return vec4(w3d_Textura(uv).rgb * w3d_color.rgb, 1.0);
}
```

## En una rutina: el paso **Shader**

Un shader escrito es como un **estado de GL**: se pone y lo que se dibuja después lo usa. En una **rutina** (Agregar → Draw → **Shader**) el paso tiene el **archivo** `.glsl`, el tilde **On** y cuatro números: **IOR**, **Transmission**, **Roughness** y **Metallic** (llegan como `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`; se pueden animar o leer de una memoria).

```
Shader          vidrio_plano.glsl  On   (IOR 1.5, Roughness 0.6)
Vertex pointer  Ventana
Draw elements   Ventana
Shader          Off
Draw elements   Marco           ← ya sin shader
```

Con **On**, los "Draw elements" que siguen usan el shader; con el tilde apagado se saca. Al terminar la rutina se saca solo. Anda en **OpenGL ES 2.0 y Vulkan**; en OpenGL ES 1.1 y el GL fijo el paso no hace nada (los dibujos de una rutina apuntan directo a los datos de la malla: el `@vertice` en la CPU es para los materiales). Un shader que lee la pantalla (`w3d_Pantalla`) ve lo que la rutina y la escena dibujaron **antes** de ese paso: el orden lo decidís vos.

## Con el trazado de rayos

El trazado de rayos no ejecuta tu shader. Usa los valores del material: **Color**, **Rugosidad**, **Metálico**, **Transmisión**, **IOR** y la textura. Si en el shader usás esos mismos valores (como el vidrio esmerilado), la Vista Material y la Vista Render dicen lo mismo.

**Transmisión** (de 0 a 1, en la tarjeta Material con el trazado de rayos prendido) dice cuánto de la superficie es vidrio: la luz pasa doblada por el **IOR** y, con **Rugosidad**, sale esmerilada. El **alfa** es otra cosa: lo transparente por el alfa (o por su mapa) no está, y por ahí se ve lo de atrás derecho, sin doblar. Con alfa 50 % y transmisión 1, la mitad pasa derecho y la otra mitad refractada. Con transmisión 0, el IOR dobla lo que se ve a través del alfa, como en las versiones anteriores.

## Lo que no hace (todavía)

- Usa una sola luz: la primera de la escena (o la del preview).
- No lleva la **niebla** ni los **planos de recorte** de los espejos.
- En **Sólido** y en las vistas de depuración (normales, alfa, vectores) se ve el material normal.
- No tiene parámetros propios: usa los del material (color, rugosidad, metálico, transmisión, IOR, emisión).

## Errores comunes

| Mensaje | Qué pasa |
|---|---|
| `undefined variable` / `undeclared identifier` | Un nombre mal escrito o que no existe (¿`w3d_Normal()` con paréntesis?). |
| `cannot convert` / `wrong operand types` | Mezcla de tipos: `1` es `int`, escribí `1.0`. Un `vec3` no se suma con un `vec4`. |
| `Fragmento: no matching overloaded function` o no compila sin `Fragmento` | Falta la función `vec4 Fragmento()` o devuelve otro tipo. |
| `redefinition of main` | No escribas `main`: lo pone Whisk3D. |
| No se ve lo de atrás | El shader usa `w3d_Pantalla` pero el material está en **Transparente** con alfa bajo, o lo de atrás se dibuja después (partículas). |
