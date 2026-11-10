# Whisk3D Core क्या है

**Whisk3D Core** वह लेयर है जो Whisk3D को हर मशीन से अलग रखती है। यही असली राज़ है: इसकी वजह से एक ही कोड **Windows**, **Linux**, **macOS**, **Android**, **ब्राउज़र** (WebGL), **Nokia N95** और **N8** (Symbian), **Sega Dreamcast**, **Sony PSP**, **PS Vita** और **Nintendo 3DS** पर चलता है।

Core एक C++ लाइब्रेरी है जिसकी अपनी रिपॉज़िटरी है, [Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core), MIT लाइसेंस के तहत। इसका डिज़ाइन **RenderWare** जैसे क्लासिक इंजनों से प्रेरित है: ऊपर एक छोटी और स्थिर API, और नीचे हर सिस्टम के लिए अलग इम्प्लीमेंटेशन।

![Whisk3D की तीन लेयर वाली आर्किटेक्चर: सबसे ऊपर Editor (हरा), जिसके अंदर आपका गेम या Lua प्रोजेक्ट है; बीच में Whisk3D UI (पीला); सबसे नीचे Whisk3D Core (लाल)। Editor और UI वैकल्पिक हैं: बाहर से, C++ का कोई गेम UI इस्तेमाल कर सकता है और कोई दूसरा सीधे Core। Core हर सिस्टम से उसके अपने बैकएंड के ज़रिए बात करता है](img/whisk3d-capas.webp)

## विचार: माँगना, करना नहीं

आधुनिक ग्राफ़िक्स कार्ड वाला PC, 2007 का एक फ़ोन और एक Dreamcast ऐसे तरीकों से ड्रॉ करते हैं जिनमें आपस में कोई समानता नहीं है। PC OpenGL या Vulkan बोलता है; PSP की अपनी चिप (GE) और एक लाइब्रेरी (sceGu) है; 3DS citro3d इस्तेमाल करता है; N95, OpenGL ES 1.1। अगर गेम इनमें से हर एक को कॉल करता, तो उसे ग्यारह बार लिखना पड़ता।

एक **एब्स्ट्रैक्शन लेयर** यह समस्या हल करती है। गेम ड्रॉ नहीं करता: वह **माँगता** है। वह कहता है "ये त्रिकोण इस टेक्सचर और इस लाइट के साथ ड्रॉ करो", और जो हिस्सा उस सिस्टम को जानता है, यानी **बैकएंड**, उसे पता होता है कि यह कैसे करना है। साउंड, फ़ाइलों, लॉग और नेटवर्क के साथ भी यही होता है: माँगने का एक ही तरीका और हर सिस्टम के लिए एक इम्प्लीमेंटेशन।

घर के नियम:

- **ऊपर का कोड कभी सिस्टम को कॉल नहीं करता।** न OpenGL को, न sceGu को, न Symbian को। सब कुछ Core के फ़ंक्शन से होकर जाता है।
- **हेडर सिस्टम के टाइप नहीं दिखाते।** एक टेक्सचर एक `unsigned int` है, और क्षमताएँ Core के अपने enum हैं (`w3dEngine::DepthTest`, `w3dEngine::Blend`), OpenGL वाले नहीं। इस तरह Core इस्तेमाल करने वाला किसी ग्राफ़िक्स हेडर पर निर्भर नहीं रहता।
- **हर सिस्टम कम से कम योगदान देता है।** अपना स्टार्टअप (विंडो, GPU का कॉन्टेक्स्ट) और वह सब जिसकी साझा कोड में जगह नहीं है। एडिटर की रिपॉज़िटरी में यह `platform/<sistema>/` में और `#ifdef W3D_PSP`, `#ifdef W3D_3DS` आदि ब्लॉक में रहता है।

Whisk3D Core इस्तेमाल करने के लिए लिखे गए एक गेम का छोटा उदाहरण, जो सभी सिस्टम पर चलता है:

```cpp
#include "graphics/w3dGraphics.h"   // ड्रॉ करना
#include "graphics/w3dTexture.h"    // टेक्सचर
#include "io/w3dFilesystem.h"       // फ़ाइलें
#include "base/w3dlog.h"            // लॉग
#include "audio/W3dAudio.h"         // साउंड

namespace gfx = w3dEngine;

// एक फ़ाइल: यह डिस्क पर, .w3d के अंदर, APK के अंदर या गेम में पैक की हुई हो सकती है
std::vector<unsigned char> nivel;
if (!w3dFileSystem::ReadFileBytes("niveles/uno.txt", nivel))
    w3dLogfE("no encontre %s", "niveles/uno.txt");

// एक टेक्सचर: stb (या Symbian का ICL) इसे डिकोड करता है और बैकएंड इसे अपलोड करता है
unsigned int pasto = 0;
gfx::LoadTexture("texturas/pasto.png", pasto);

// एक साउंड: Core इसे मिक्स करता है; आउटपुट हर सिस्टम का अपना है
w3dEngine::W3dSound* salto = w3dEngine::W3dSoundLoad("sonidos/salto.wav");
w3dEngine::W3dSoundPlay(salto, 1.0f, false);

// ड्रॉ करना: OpenGL, sceGu या citro3d को एक भी कॉल नहीं
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(pasto);
gfx::EnableArray(gfx::VertexArray);   gfx::VertexPointer3f(0, posiciones);
gfx::EnableArray(gfx::TexCoordArray); gfx::TexCoordPointer2f(0, uvs);
gfx::DrawTriangles(cantidadDeIndices, indices);
```

## अंदर क्या है

Core का हर फ़ोल्डर एक मॉड्यूल है:

| फ़ोल्डर | क्या हल करता है |
|---|---|
| `graphics/` | ड्रॉइंग API (`w3dEngine`), टेक्सचर, स्क्रीन और बैकएंड: [ग्राफ़िक्स](graficos.md) |
| `audio/` | इफ़ेक्ट, संगीत और वॉल्यूम: [साउंड और संगीत](sonido.md) |
| `io/` | फ़ाइलें, कंटेनर (.w3d, zip, pak), मेश और रिसोर्स: [फ़ाइलें](archivos.md) |
| `base/` | लॉग, सेव की गई सेटिंग और सबके लिए साझा चीज़ें: [लॉग](logs.md) |
| `input/` | टच, कंट्रोलर, झुकाव, वाइब्रेशन और क्लिपबोर्ड: [इनपुट](entrada.md) |
| `net/` | नेटवर्क पर खेलना: [नेटवर्क](red.md) |
| `physics/` | फ़िज़िक्स, टक्कर, हिटबॉक्स, रैगडॉल और नेविगेशन: [फ़िज़िक्स](fisica.md) |
| `script/` | Lua: [Lua स्क्रिप्ट](lua.md) |
| `math/` | वेक्टर, मैट्रिक्स और क्वाटर्नियन: [गणित, सीन और फ़ॉर्मैट](formatos.md) |
| `objects/`, `animation/` | सीन: ऑब्जेक्ट, मेश, मटीरियल, लाइट, कैमरा और एनिमेशन: [गणित, सीन और फ़ॉर्मैट](formatos.md) |
| `tools/` | डिस्पैच जनरेटर, स्वतंत्रता की जाँच और पैकेजर |
