# एनिमेशन

| फ़ंक्शन | क्या करता है |
|---|---|
| `animScene(name [, loop])`, `stopScene()`, `animSceneFrame(f)` | पूरे सीन के एनिमेशन। |
| `animObject(root, "clip" [, loop])`, `animObjectStop`, `animObjectFinished`, `animObjectSpeed` | किसी ऑब्जेक्ट और उसके चाइल्ड के एनिमेशन क्लिप। |
| `animClip(armature, clip [, loop])`, `animFinished`, `animSpeed`, `animTransition` | किसी आर्मेचर के एनिमेशन (चलना, कूदना…) और उनके बीच का स्मूद ट्रांज़िशन। |
| `animLayer(esq, n, clip [, weight, mode])` और उससे जुड़े फ़ंक्शन | कई एनिमेशन को एक साथ लेयर (1 से 16) में मिलाना, जैसे दौड़ना + गोली चलाना। |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | कोई बोन कहाँ है (हाथ में हथियार रखने के लिए)। |
| `animate(mesh, n [, frame])` | किसी मेश का वर्टेक्स एनिमेशन बदलता है। |
