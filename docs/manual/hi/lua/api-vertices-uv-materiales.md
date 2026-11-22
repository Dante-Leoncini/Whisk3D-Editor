# वर्टेक्स, UV और मटीरियल

| फ़ंक्शन | क्या करता है |
|---|---|
| `vertexCount(m)`, `vertexPos(m, i)`, `setVertexPos(m, i, x, y, z)` | वर्टेक्स को एक-एक करके पढ़ना और मूव करना। |
| `setVertices(m, {i, x, y, z, ...})` | एक साथ कई वर्टेक्स मूव करना (बहुत ज़्यादा तेज़)। |
| `vertexGroup(m, "grupo") → {índices}` | किसी ग्रुप के वर्टेक्स (इसे सिर्फ़ एक बार, `start` में कॉल करें)। |
| `setVertexColor`, `setVerticesColor` | कोड से वर्टेक्स पेंट करना। |
| `setVertexUV`, `scrollUV(m, du, dv)`, `copyUV` | UV को मूव करना (पानी, कन्वेयर बेल्ट…)। |
| `setTextureB`, `setTextureMix`, `getTextureMix` | किसी मटीरियल में दो टेक्सचर मिलाना। |
