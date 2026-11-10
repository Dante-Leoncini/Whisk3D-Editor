# Core को सीधे इस्तेमाल करना

उनके लिए जो C या C++ में प्रोग्राम करते हैं और एडिटर के बिना गेम चाहते हैं। इसके लिए चाहिए:

- **Core के सोर्स।** include डायरेक्टरी Core का रूट और साथ में `base/`, `graphics/`, `io/` और `thirdparty/` हैं।
- **`graphics/` का एक बैकएंड**, आपके सिस्टम वाला (जैसे `graphics/opengl/w3dGraphics.cpp`), और `base/w3dlog.cpp`। उसके बाद, वे मॉड्यूल जो आप इस्तेमाल करते हैं (ऑडियो, फ़ाइलें, फ़िज़िक्स...)।
- **कॉन्टेक्स्ट वाली एक विंडो।** उदाहरण SDL2 इस्तेमाल करते हैं। शेडर वाले बैकएंड के साथ कॉन्टेक्स्ट बनाने के बाद एक बार `w3dEngine::GLES2Init(SDL_GL_GetProcAddress)` कॉल करना होता है।

न्यूनतम कोड, `simple_cube` उदाहरण से:

```cpp
gfx::Viewport(0, 0, w, h);
gfx::ClearColor(0.10f, 0.12f, 0.16f, 1.0f);
gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
gfx::MatrixMode(gfx::Projection);  gfx::LoadIdentity();
gfx::Perspective(45.0f, (float)w / (float)h, 0.1f, 100.0f);
gfx::MatrixMode(gfx::ModelView);   gfx::LoadIdentity();
gfx::Translatef(0.0f, 0.0f, -5.0f);
gfx::Rotatef(angulo, 0.0f, 1.0f, 0.0f);
gfx::EnableArray(gfx::VertexArray); gfx::VertexPointer3f(0, cubeVerts);
gfx::EnableArray(gfx::NormalArray); gfx::NormalPointer3b(cubeNormals);
gfx::DrawTriangles(36, cubeIdx);
```

उदाहरण धीरे-धीरे [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) पर अपलोड किए जा रहे हैं: `simple_cube` (एक लाइट वाला क्यूब), `textured_cube` (`LoadTexture` से लोड होने वाले टेक्सचर के साथ) और `normal_map_cube` (DOT3 से नकली उभार)। वहीं `game/w3drun` भी है, वह रनटाइम जो एडिटर में बने गेम (सीन, इंटरफ़ेस और Lua) को एडिटर के बिना चलाता है।

मौजूदा स्थिति, साफ़-साफ़:

- **Core अभी प्रोडक्शन के लिए तैयार नहीं है।** इसे अब भी दोबारा लिखा जा रहा है और यह अक्सर बदलता है।
- **इसके अंदर एडिटर का लॉजिक है** (एडिट मोड, मॉडिफ़ायर, रेंडर मोड) जिसे निकाला जा रहा है। `tools/prueba_independencia.sh` मापता है कि इसे अकेले इस्तेमाल करने लायक बनने में कितना बाकी है।
- **उदाहरण फ़ोल्डर व्यवस्था के एक पुराने वर्ज़न के लिए लिखे गए थे:** उनका CMake `w3dGraphics.cpp` को Core के रूट में बताता है, जबकि आज वह `graphics/opengl/` में है। पाथ ठीक करने पड़ सकते हैं।
- **आगे क्या आ रहा है:** आर्मेचर, मैप के लिए BSP जैसा सिस्टम, वैकल्पिक फ़िज़िक्स, ज़्यादा रेट्रो बैकएंड, ज़्यादा उदाहरण और दस्तावेज़।

सवाल [Telegram](https://t.me/Whisk3D) ग्रुप में पूछें।

अगर C++ तक उतरने की ज़रूरत नहीं है, तो छोटा रास्ता एडिटर है: गेम [Lua](../lua/README.md) में प्रोग्राम होता है और [Properties](../editor/propiedades.md) का **Game** कार्ड उसे हर सिस्टम के लिए कंपाइल करता है। अंदर से यह वही Core इस्तेमाल करता है।
