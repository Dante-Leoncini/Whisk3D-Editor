# Vértices, UV y materiales

| Función | Qué hace |
|---|---|
| `vertexCount(m)`, `vertexPos(m, i)`, `setVertexPos(m, i, x, y, z)` | Leer y mover vértices uno por uno. |
| `setVertices(m, {i, x, y, z, ...})` | Mover muchos vértices de una vez (mucho más rápido). |
| `vertexGroup(m, "grupo") → {índices}` | Los vértices de un grupo (llamarla una sola vez, en `start`). |
| `setVertexColor`, `setVerticesColor` | Pintar vértices por código. |
| `setVertexUV`, `scrollUV(m, du, dv)`, `copyUV` | Mover las UV (agua, cintas transportadoras…). |
| `setTextureB`, `setTextureMix`, `getTextureMix` | Mezclar dos texturas en un material. |
