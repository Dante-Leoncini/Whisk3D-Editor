# रिजिड बॉडी, टक्कर और हिटबॉक्स

| फ़ंक्शन | क्या करता है |
|---|---|
| `physicsVel`, `physicsAngVel`, `physicsImpulse`, `physicsForce` | किसी रिजिड बॉडी का वेग, घुमाव, धक्के और बल। |
| `physicsGravity([g])`, `physicsActive(obj, bool)`, `physicsAwake(obj)` | गुरुत्वाकर्षण (डिफ़ॉल्ट 9.81), किसी ऑब्जेक्ट की फ़िज़िक्स चालू/बंद करना। |
| `physicsRay(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | एक किरण छोड़ता है और बताता है कि वह किससे टकराती है। |
| `contacts() → list` | इस फ़्रेम की सारी टक्करें। |
| `colGround(mesh, x, y, z)`, `colWall(...)`, `colRay(...)` | किसी मेश के त्रिकोणों से टक्कर (फ़र्श पर खड़े होना, दीवारों के आर-पार न जाना)। |
| `hitboxActive`, `setHitboxActive`, `setHitboxSize`, `hitboxInside`, `hitboxTouching` | हिटबॉक्स संभालना और पूछना कि उनके अंदर क्या है। |
| `showHitboxes(bool)` | डीबग करने के लिए हिटबॉक्स ड्रॉ करता है। |
