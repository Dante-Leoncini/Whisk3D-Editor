# Streaming y costo de render en hardware chico

Notas de diseño del motor para escenarios grandes en GPU modestas (PS1/PS2/Dreamcast/móvil
tipo GLES 1.1). Todo lo de acá es mecánica del motor; los números entre paréntesis vienen de un
proyecto real que sirvió de banco de pruebas (un nivel de plataformas de ~315 objetos, 28 trozos de
escenario y 43 cajas destructibles), y están para dar escala, no porque el motor sepa nada de ese
juego.

## 1. Una textura por RUTA, no por material

`Whisk3DCore/objects/TextureCache.cpp` es la única puerta de creación y destrucción de una `Texture`
de contenido. Indexa por la ruta con la que se pidió y lleva **refcount**.

Sin ese caché, cada camino que ve un `map_Kd` hace `new Texture()` + `LoadTexture`: el importador
de OBJ diferido, los sprites animados y el diálogo "Load Texture", todos por separado. Un escenario
partido en N trozos que comparten el mismo atlas sube el atlas **N veces** a la GPU (medido: 28
trozos con un atlas de 512×512 = 28 MB; 43 cajas con 2 atlas = 11 MB). Es el gasto de memoria más
caro que puede tener una escena, y no se nota hasta que la GPU es chica.

Corolarios que importan:

- **Refcount, no solo mapa.** Sin contar referencias no se puede liberar, y liberar es la mitad del
  trabajo: sin esto, abrir un proyecto tras otro fuga todas las texturas del anterior (GPU incluida)
  para el resto de la vida del proceso.
- **Memoria de fallos.** Las rutas que ya fallaron se recuerdan; si no, un `.mtl` con una textura
  inexistente hace que cada material que la pida vuelva a golpear el disco (con cientos de objetos,
  son cientos de `fopen` fallidos por importación).
- **La muleta del "ancla" ya no hace falta.** Antes del caché, un proyecto podía esquivar el
  problema declarando el `map_Kd` en UN solo `.mtl` y compartiendo el *material* por nombre en los
  otros. Con caché por ruta eso es innecesario: declarar la misma textura en los N `.mtl` cuesta un
  solo objeto de GPU.

El A/B se puede medir con el mismo binario y la misma escena: comando `texcache off|on` del harness
(ver `tools/pruebas/prueba_texcache.w3s`) y `texinfo` para las cuentas.

## 2. Un draw call por atlas, y ordenar por textura

Si cada grupo de objetos comparte un atlas, el orden por textura sale casi gratis y el peor caso
baja a un puñado de binds por frame (medido: escenario → cajas → coleccionables → enemigos →
personaje = **5 binds**).

Lo que el motor todavía NO hace: el bucle de render recorre el árbol de la escena en orden de
jerarquía, no en orden de material. Con 5 texturas distintas da igual; con 50 hay que ordenar por
material antes de emitir.

## 3. La unidad de streaming es el trozo visible

El motor no impone una política de carga: la unidad natural es el objeto/zona que el `Culling` o el
PVS ya usa como sector. Lo que el motor aporta para eso:

- `Culling` con recorte contra el frustum de la cámara activa (`soloCamaraActiva`), para que lo que
  se previsualiza sea lo que se dibuja.
- `LOD` que mide distancia **al AABB en mundo** con histéresis, y mide desde la cámara del juego
  mientras se juega (cobertura: `tools/pruebas/prueba_lod_aabb.w3s`).
- PVS por triángulo por sector (`formato/pvs-json.md`), para decidir visibilidad sin recorrer la
  geometría.

## 4. Texturas en 16 bits

Subir los atlas en 16 bits (RGBA4444 / RGB565) en una GPU sin compresión no pierde nada apreciable
cuando el arte de origen ya es de paleta, y divide por dos el pico de memoria. Es una decisión del
proyecto, no del motor: el motor sube lo que le den.

## 5. Lo que no dibuja nada no apila su matriz

El recorrido de la escena (`Object::Render`) saltea un subarbol entero, sin `glPushMatrix` / `glMultMatrixf` /
`glPopMatrix`, cuando ni el objeto ni ninguno de sus hijos visibles dibuja algo (`Object::DibujaAlgo`). En el juego
(overlays apagados) no dibujan nada: un emisor de particulas sin particulas vivas (las vivas van en el pase diferido;
la flecha del emisor es un overlay), una hitbox, una camara (su gizmo es un overlay), un armature (sus huesos los dibuja
el editor en una pasada aparte) y un vacio. Antes cada uno costaba sus 3 llamadas por cuadro aunque no se viera nada: en
un nivel con ~50 emisores de efectos esperando (sangre, polvo, chispas) eran ~150 llamadas por cuadro.
Prueba: `tools/pruebas/prueba_no_apila.w3s` (comando `noapila`: el render con el objeto visible y oculto hace las mismas
llamadas GL).

## 6. La matriz y los punteros llegan a GL solo cuando se usan

En el N95 cada llamada GL cuesta ~15 us, y la mayoria de las de un cuadro no dibujaban: eran matrices, punteros y binds.
Dos caches de `gfx` (pipeline fijo, PC y GL ES 1.1; GLES2/WebGL ya lo hacia por uniforms) las sacan sin cambiar la imagen:

- **Pila perezosa de la modelview** (`gfx::g_matricesPerezosas`). Push/Pop/Mult/Load/Trasladar/Rotar/Escalar solo cambian
  la sombra en la CPU (la que ya leia `GetMatrix`); cada nivel lleva una serie y GL la de lo que tiene cargado. Antes de lo
  que usa la matriz (los `Draw*`, `Light0fv`/`LightNfv` de la posicion, `PlanosRecorte`) va UN `glLoadMatrixf` si cambio.
  Un objeto que el autocull descarta cuesta 0 llamadas (antes 3); uno que dibuja, 1. Cambiar de matriz (el orto del HUD) no
  obliga a mandarla: si un draw la necesita con otra activa, pasa a la modelview y vuelve. Sin la matriz en la sombra (pila
  de mas de 32) se apila de verdad, como antes. El GL crudo que use la modelview (las luces de `Light.cpp`, el cursor de
  Symbian) llama `gfx::MatricesSincronizar()` antes. La sombra rota y multiplica con las mismas cuentas que el `glRotatef`
  y el `glMultMatrixf` de Mesa: los bits son los mismos y la imagen tambien.
- **Cache de punteros y binds** (`gfx::g_punterosCache`). Un `glXxxPointer` igual al puesto no se repite, ni su bind (GL
  guarda con cada puntero el buffer de ese momento); `BindIndexVBO` del IBO ya bindeado tampoco; `UnbindVBOs` no hace
  nada: el 0 lo bindean el proximo puntero de RAM y el proximo draw con indices de RAM. `Mesh::RenderObject` ya no pone
  los punteros de RAM que el camino del VBO pisaba (si al final no va por VBO, un esqueleto animando, los pone despues).
  Una malla con VBO pasa de 21 llamadas (push, mult, 4 punteros de RAM, 4 binds+punteros, IBO, UV otra vez, draw, 2
  binds en 0, pop) a 11 (4 binds+punteros, IBO, una carga de matriz, draw). `Invalidate` y `DeleteBuffer` olvidan lo
  que corresponde; todo puntero y todo bind de buffer tiene que pasar por `gfx`.

Medido en un nivel de 320x240 en modo juego: 469 -> 327 llamadas por cuadro, y 874 -> 529 en una escena con varios
personajes skinneados; la misma imagen. Las dos opciones son para el A/B (harness `perezosas`, `punteroscache`,
`menosllamadas`). Prueba: `tools/pruebas/prueba_menos_llamadas.w3s` (con las opciones prendidas y apagadas: la misma
imagen, menos llamadas y `glaudit` sin desincronizaciones, que ademas compara la modelview, los binds y los punteros).
