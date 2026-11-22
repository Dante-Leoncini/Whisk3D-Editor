# Sommets, UV et matériaux

| Fonction | Ce qu'elle fait |
|---|---|
| `vertexCount(m)`, `vertexPos(m, i)`, `setVertexPos(m, i, x, y, z)` | Lire et déplacer les sommets un par un. |
| `setVertices(m, {i, x, y, z, ...})` | Déplacer de nombreux sommets d'un coup (beaucoup plus rapide). |
| `vertexGroup(m, "grupo") → {índices}` | Les sommets d'un groupe (à appeler une seule fois, dans `start`). |
| `setVertexColor`, `setVerticesColor` | Peindre des sommets par code. |
| `setVertexUV`, `scrollUV(m, du, dv)`, `copyUV` | Déplacer les UV (eau, tapis roulants…). |
| `setTextureB`, `setTextureMix`, `getTextureMix` | Mélanger deux textures dans un matériau. |
