# लाइट और कैमरा

| फ़ंक्शन | क्या करता है |
|---|---|
| `getColor(light) → r, g, b`, `setColor(light, r, g, b)` | किसी लाइट का रंग। |
| `getEnergy(light) → e`, `setEnergy(light, e)` | उसकी चमक, 0 से 1 तक (टिमटिमाने या धीरे-धीरे बुझने के लिए)। |
| `setActiveCamera(cam)` | वह कैमरा बदलता है जिससे गेम दिखता है। |
| `getLens(cam) → fov, near, far`, `setLens(cam [, fov, near, far])` | देखने का कोण और क्लिपिंग की दूरियाँ। |
| `cameraXZ() → fx, fy, rx, ry` | कैमरा फ़र्श पर, X और Y में, किस दिशा में देख रहा है: आगे (`fx, fy`) और दाईं ओर (`rx, ry`), कैरेक्टर को कैमरे के हिसाब से चलाने के लिए। (यह नाम उस समय का है जब फ़र्श XZ था।) |
| `screenOf(obj) → x, y, front` | कोई 3D ऑब्जेक्ट स्क्रीन पर कहाँ दिखता है (उसके ऊपर कोई 2D लेबल रखने के लिए)। |
| `setRail(cam, curve [, offset])`, `setRailNode`, `setRailLook`, `getRail` | कैमरे को किसी कर्व (रेल) के साथ-साथ चलाना। |
| `resolution() → width, height` | 3D इमेज का असली साइज़, पिक्सेल में। |
