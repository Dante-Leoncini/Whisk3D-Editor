# Decisiones de diseño de Whisk3D

Las reglas que hay que respetar al tocar el editor. Cortas y al grano: si algo necesita
explicarse largo, va en [decisiones-historia.md](decisiones-historia.md), no acá y **nunca**
como un comentario gigante en el código.

Criterio para los comentarios del código: dicen **qué hace** y, si no es obvio, **por qué**.
Dos o tres líneas. El relato de cómo se llegó a esa decisión va en el documento de historia.

---

## Undo: los pasos que guardan un índice

Un paso de undo puede guardar la posición de algo en una lista (la capa 3 de una malla, el
clip 2 de un armature). Si entre que se guarda y que se aplica esa lista **se corre** —porque
se borró o se reordenó un elemento— el Ctrl+Z cae sobre el elemento equivocado. No hay crash
ni aviso: se corrompe en silencio.

**La regla, para cualquier lista nueva:** si una lista tiene destinos de rename por índice
(cualquier `W3dRenameDest` que no sea `Directo`), toda operación que corra sus índices
—borrar, reordenar, insertar en el medio— tiene que hacer una de estas dos cosas:

1. **snapshotear la lista entera** en el mismo paso de undo, o
2. **avisar** con `UndoListaBorrada` / `UndoListaMovida` / `UndoMoverCapaMalla` / `UndoMoverClipArm`.

Y si el paso de undo guarda índices de esa lista, tiene que implementar `RemapLista`.
Al agregar la lista, sumarla también a `W3dNombresJuntarEspacios` (`ObjectMode.cpp`).

**Cómo clasificar cada miembro de un comando de undo:**

| tipo | qué es | seguro? |
|---|---|---|
| (a) | puntero revalidado al aplicar | sí (si murió, no-op) |
| (b) | snapshot completo de la lista | sí, **solo si la lista se guarda** |
| (c) | posición, o vector paralelo posicional | **peligroso**: decir contra qué lista indexa |
| (d) | valor propio | sí |

Tres reglas mecánicas para no clasificar mal:

- **R1.** Un snapshot no es (b) si la lista no se guarda. Si al aplicar la lista se vuelve a
  buscar (por un global, por "el activo", por `FindTarget`), es (c) contra "la lista que esté
  elegida en ese momento", que es el peor caso: cambia sin que se borre ni se reordene nada.
- **R2.** La identidad tiene **dos partes**: el elemento y la lista. La segunda es la que
  siempre se olvida.
- **R3.** En las claves con dueño, el dueño va **antes** del número (`arm:<rig>/k<clip>`): el
  aviso de remapeo corta la clave justo antes del número, así que con el número adelante,
  borrar un clip de un armature corre los índices de todos.

**Un guard del estilo "si los tamaños no coinciden no toco nada" no es una solución**: es la
versión silenciosa del bug (el Ctrl+Z no restaura nada y tampoco avisa).

**Regla de escritura:** un comentario que declara algo inofensivo tiene que venir con el test
que lo demuestra, citado por nombre. Sin eso no es documentación, es una hipótesis sin correr.
