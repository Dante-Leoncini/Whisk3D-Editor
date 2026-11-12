# लाइट और कैमरा

| फ़ंक्शन | क्या करता है |
|---|---|
| `color(luz) → r, g, b`, `setColor(luz, r, g, b)` | किसी लाइट का रंग। |
| `energia(luz) → e`, `setEnergia(luz, e)` | उसकी चमक, 0 से 1 तक (टिमटिमाने या धीरे-धीरे बुझने के लिए)। |
| `setCamaraActiva(cam)` | वह कैमरा बदलता है जिससे गेम दिखता है। |
| `lenteDe(cam) → fov, cerca, lejos`, `setLente(cam [, fov, cerca, lejos])` | देखने का कोण और क्लिपिंग की दूरियाँ। |
| `camaraXZ() → fx, fy, rx, ry` | कैमरा फ़र्श पर, X और Y में, किस दिशा में देख रहा है: आगे (`fx, fy`) और दाईं ओर (`rx, ry`), कैरेक्टर को कैमरे के हिसाब से चलाने के लिए। (यह नाम उस समय का है जब फ़र्श XZ था।) |
| `pantallaDe(obj) → x, y, delante` | कोई 3D ऑब्जेक्ट स्क्रीन पर कहाँ दिखता है (उसके ऊपर कोई 2D लेबल रखने के लिए)। |
| `setRiel(cam, curva [, offset])`, `setRielNodo`, `setMiradaRiel`, `rielDe` | कैमरे को किसी कर्व (रेल) के साथ-साथ चलाना। |
| `resolucion() → ancho, alto` | 3D इमेज का असली साइज़, पिक्सेल में। |
