# सीन: खोजना, बनाना और नष्ट करना

| फ़ंक्शन | क्या करता है |
|---|---|
| `buscar(nombre [, raiz]) → obj \| nil` | Outliner में किसी ऑब्जेक्ट को उसके नाम से खोजता है (या सिर्फ़ `raiz` के अंदर)। |
| `nombre(obj) → texto` | ऑब्जेक्ट का नाम। |
| `tipo(obj) → texto` | वह क्या है: `"mesh"`, `"luz"`, `"camara"`, `"vacio"`, `"esqueleto"`, `"texto"`, `"imagen"`, `"boton"`, `"hitbox"`, `"prefab"`, `"rutina"`… (अगर वह मौजूद न हो तो `""`)। |
| `instanciar(prefab [, x, y, z [, rotY [, padre]]]) → obj \| nil` | उस पोज़िशन पर किसी प्रीफ़ैब की कॉपी बनाता है (rotY डिग्री में)। `"Libreria/Escena"` किसी लिंक की हुई लाइब्रेरी के प्रीफ़ैब की कॉपी बनाता है। `padre` देने पर कॉपी उस ऑब्जेक्ट से लटकती है (पोज़िशन और घुमाव उसके सापेक्ष)। |
| `destruir(obj) → bool` | फ़्रेम के अंत में ऑब्जेक्ट को डिलीट करता है। |
| `importarW3D(ruta) → obj \| nil` | गेम चलते समय सीन में कोई दूसरी .w3d फ़ाइल जोड़ता है। |
| `cargar(obj)` / `descargar(obj)` / `cargaAuto(obj)` | लेवल के किसी हिस्से को हाथ से लोड या अनलोड करता है (streaming), या उसे फिर से ऑटोमैटिक पर कर देता है। |
| `cargado(obj) → bool` | क्या वह हिस्सा लोड हो चुका है। |
