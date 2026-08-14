# Historia de las decisiones

Por qué las reglas de [decisiones.md](decisiones.md) son como son. Esto **no va en el código**:
es contexto para cuando alguien quiera cambiar una regla y necesite saber qué se rompió antes.

---

## Undo: ocho rondas del mismo bug

El bug siempre fue el mismo con distinta ropa: **un paso de undo guarda una posición, la lista
se corre, y el Ctrl+Z aplica sobre el elemento de al lado**. Apareció ocho veces en comandos
distintos porque se lo buscaba de a uno, por intuición, en vez de clasificar.

**Rondas 1 a 5.** Se descubrió primero en los pasos de *rename*, y el criterio quedó escrito
sólo para ellos. Pero los pasos que guardan posición no son sólo esos: un paso de *reordenar*
("intercambiar i y j") también se rompe si entre medio se borra un elemento con un borrado no
deshacible — no corrompe nombres, pero deja un orden que nunca existió. De ahí salió que esos
comandos también implementaran `RemapLista`, siguiendo sus dos puntas y matándose (no-op) si
les borraron una.

**Ronda 6.** Se probó una defensa por construcción: hacer `RemapLista` virtual pura, para que
el compilador obligue a implementarla. Eso cierra un agujero ("me olvidé de escribirla") pero
**no** el otro: "clasifiqué mal un miembro". `KeyframesUndo` ya implementaba `RemapLista` y ya
estaba en la tabla, y aun así su `snap` figuraba como (b) cuando era (c): un vector paralelo
posicional contra una lista que ni siquiera se guardaba, porque se re-resolvía desde los
globales al aplicar. El compilador no puede atrapar eso. De ahí salieron las reglas mecánicas
R1/R2 de `decisiones.md`.

**Rondas 7 y 8.** El orden de los segmentos de una clave resultó no ser cosmético. Con el
número adelante (`arm:k<clip>/<rig>`, como quedó en la ronda 7), el prefijo del aviso de
remapeo no puede llevar el rig, y borrar o reordenar un clip de **un** armature corre los
índices de clip de **todos**. Lo fija el test `dopeclip3d`.

**Lo que dejó la ronda 8, y es la lección más útil:** los tres fallos estaban *justificados
como cubiertos* en un comentario del propio arreglo ("es inofensivo porque el resolver chequea
el nombre del rig", "(a) puntero revalidado contra `SceneAnimations`"). O sea que el comentario
no sólo estaba equivocado: **apagó la revisión siguiente**, porque quien pasó después lo leyó y
siguió de largo. Por eso la regla de escritura: si un comentario declara algo inofensivo, tiene
que citar el test que lo demuestra.

**El otro síntoma de la misma familia:** un guard "si los tamaños no coinciden no toco nada"
(era el caso de `BonesUndo` / `Bones2DUndo`). Parece defensivo y es el bug en silencio.

Tests que fijan todo esto: `reparentundo`, `dopeclip3d`, `modtargetuaf`, `capaorden`,
`capaindices`, `cliporden`, `nombresundo`.
