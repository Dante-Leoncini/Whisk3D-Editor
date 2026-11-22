# Köşeler, UV ve materyaller

| Fonksiyon | Ne yapar |
|---|---|
| `vertexCount(m)`, `vertexPos(m, i)`, `setVertexPos(m, i, x, y, z)` | Köşeleri tek tek okuma ve taşıma. |
| `setVertices(m, {i, x, y, z, ...})` | Birçok köşeyi tek seferde taşıma (çok daha hızlı). |
| `vertexGroup(m, "grupo") → {índices}` | Bir grubun köşeleri (yalnızca bir kez, `start` içinde çağır). |
| `setVertexColor`, `setVerticesColor` | Köşeleri kodla boyama. |
| `setVertexUV`, `scrollUV(m, du, dv)`, `copyUV` | UV'leri kaydırma (su, taşıma bantları…). |
| `setTextureB`, `setTextureMix`, `getTextureMix` | Bir materyalde iki dokuyu karıştırma. |
