# Vértices, UV e materiais

| Função | O que faz |
|---|---|
| `vertexCount(m)`, `vertexPos(m, i)`, `setVertexPos(m, i, x, y, z)` | Ler e mover vértices um por um. |
| `setVertices(m, {i, x, y, z, ...})` | Mover muitos vértices de uma vez (muito mais rápido). |
| `vertexGroup(m, "grupo") → {índices}` | Os vértices de um grupo (chame-a uma única vez, em `start`). |
| `setVertexColor`, `setVerticesColor` | Pintar vértices por código. |
| `setVertexUV`, `scrollUV(m, du, dv)`, `copyUV` | Mover as UV (água, esteiras transportadoras…). |
| `setTextureB`, `setTextureMix`, `getTextureMix` | Misturar duas texturas em um material. |
