# UI को सीधे इस्तेमाल करना

उनके लिए जो C या C++ में प्रोग्राम करते हैं और एडिटर के बिना इंटरफ़ेस चाहते हैं। इसके लिए चाहिए:

- **Core, अपने सीन मॉडल के साथ।** UI `w3dEngine` से ड्रॉ करती है और कुछ पंक्तियाँ सीन के ऑब्जेक्ट पढ़ती हैं (`objects/`, `animation/`, `script/`)। [Core को सीधे इस्तेमाल करना](../core/empezar.md) देखें।
- **UI के सोर्स।** `libs/WhiskUI/` की सारी `.cpp` फ़ाइलें। include डायरेक्टरी हैं `libs/`, `"WhiskUI/draw/icons.h"` के लिए, और सबफ़ोल्डर `widgets/`, `text/`, `draw/`, `theme/` और `core/`, क्योंकि अंदर वे एक-दूसरे को बिना फ़ोल्डर के include करते हैं (`"icons.h"`, `"card.h"`)।
- **स्किन की फ़ाइलें।** `font.png` (अक्षर और कार्ड के किनारे), `font-ext.png` (एक्सटेंडेड एटलस), `skin.ini` (रंग) और, नए आइकन के लिए, `atlas/iconos/` फ़ोल्डर। ये लाइब्रेरी के साथ नहीं आतीं: ये एडिटर की रिपॉज़िटरी में, `res/Skins/Whisk3D/` में हैं।

## शुरू करना

वही जो एडिटर शुरू होते समय करता है, उसकी अपनी चीज़ों के बिना:

```cpp
#include "w3dTexture.h"
#include "WhiskUI/core/UI.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/font.h"
#include "WhiskUI/text/W3dFont.h"

loadColorsW3d("Skins/Whisk3D/skin.ini");    // फ़ाइल के बिना कोड वाले रंग रहते हैं

unsigned int atlas = 0; int w = 0, h = 0;
w3dEngine::LoadTexture("Skins/Whisk3D/font.png", atlas, &w, &h);

CalcCardUV(w, h);          // गोल किनारे
CrearIconos(w, h);         // वे आइकन जो font.png के अंदर हैं
W3dFontSetTexSize(w, h);
SetIconScale(GlobalScale);

WhiskFont = new Font(w, h, atlas);
WhiskFont->SetScale(GlobalScale);
WhiskFont->SetAtlasExtendido("Skins/Whisk3D/font-ext.png");   // यह तभी लोड होता है जब ज़रूरत हो
```

`GlobalScale` 1 से शुरू होता है। दूसरे स्केल के लिए [रंग और स्केल](tema.md) देखें। `atlas/iconos/` के अलग-अलग आइकन के लिए [ड्रॉइंग और एटलस](dibujo.md) देखें।

हर बार जब विंडो का आकार बदलता है:

```cpp
W3dPantallaAlto = alto;    // इंटरफ़ेस की Y को Scissor और Viewport की Y में बदलने के लिए
MenuPantallaW = ancho;     // ताकि मेनू स्क्रीन से बाहर न जाएँ
MenuPantallaH = alto;
```

## 2D स्टेट

UI पिक्सेल में ड्रॉ करती है, Y नीचे की ओर और ओरिजिन ऊपर बाईं ओर। हर विजेट मौजूदा मैट्रिक्स के ओरिजिन पर ड्रॉ होता है: उसे `Translatef` से जगह पर रखा जाता है। ड्रॉ करने से पहले:

```cpp
namespace gfx = w3dEngine;

gfx::Viewport(0, 0, ancho, alto);
gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
gfx::Ortho(0, ancho, alto, 0, -1, 1);       // Y नीचे की ओर
gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();
gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
gfx::Disable(gfx::Lighting);  gfx::Disable(gfx::Fog);
gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
gfx::DisableArray(gfx::NormalArray);
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(atlas);
gfx::TexFilter(false);                       // साफ़ पिक्सेल
gfx::Enable(gfx::Blend); gfx::BlendAlpha();
```

## एक कार्ड

एक नंबर, एक चेकबॉक्स और एक बटन वाला कार्ड। हर पंक्ति आपके एक वेरिएबल की ओर इशारा करती है:

```cpp
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"

static float volumen = 0.8f;
static bool  pantallaCompleta = false;
static void Salir() { /* ... */ }

GroupPropertie* opciones = new GroupPropertie("Opciones");

PropFloat* vol = new PropFloat("Volumen");
vol->value = &volumen;
vol->SetRango(0.0f, 1.0f);
opciones->properties.push_back(vol);

PropBool* pc = new PropBool("Pantalla completa");
pc->value = &pantallaCompleta;
opciones->properties.push_back(pc);

PropButton* salir = new PropButton("Salir", IconType::borrar);
salir->action = Salir;
opciones->properties.push_back(salir);

opciones->Resize(200 * GlobalScale, 0);    // ऊँचाई कार्ड ख़ुद गणना करता है

// हर फ़्रेम में, 2D स्टेट सेट करके
gfx::PushMatrix();
gfx::Translatef(10, 10, 0);
opciones->Render();
gfx::PopMatrix();
```

UI कीबोर्ड या माउस नहीं पढ़ती: ये आप उसे देते हैं। कीबोर्ड से:

```cpp
// नीचे वाला ऐरो
if (opciones->editando) opciones->button_down();   // मान बदलता है
else opciones->NextSelect();                        // नीचे वाली पंक्ति
// OK या Enter: शीर्षक पर कार्ड को फ़ोल्ड करता है; किसी पंक्ति पर, उसे एडिट करता है या दबाता है
opciones->EnterPropertieSelect();
// Cancel: पंक्ति को वैसा ही छोड़ देता है जैसी वह थी
opciones->Cancel();
```

माउस के साथ, पॉइंटर के नीचे वाली पंक्ति आप ख़ुद गणना करते हैं: [कार्ड और प्रॉपर्टी](propiedades.md) देखें। मेनू ज़रूर कोऑर्डिनेट लेते हैं: [ड्रॉपडाउन मेनू](menus.md) देखें।

## मौजूदा स्थिति, साफ़-साफ़

- **यह प्रोडक्शन के लिए तैयार नहीं है।** रिपॉज़िटरी का `README.md` यही कहता है, और यह सच है: इसके अंदर एडिटर का कोड है, जिसे अलग किया जा रहा है।
- **कोई ऐसा उदाहरण नहीं है जो इसे अकेले इस्तेमाल करे।** [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) के `ui/` वाले उदाहरण Lua के साथ `.w3dui` इंटरफ़ेस हैं, जिन्हें गेम का रनटाइम चलाता है: वह एडिटर का है। इस पेज का कोड उन्हीं कॉल से बना है जो एडिटर इस्तेमाल करता है, किसी परखे हुए उदाहरण से नहीं लिया गया।
- **आज यह अकेले लिंक नहीं होती।** `PropList.cpp`, `RebindMaterialMeshPart()` को कॉल करता है, जो एडिटर के प्रॉपर्टी पैनल में है। `TextField.h` के फ़ोकस वाले फ़ंक्शन (`TextFieldEnfocar`, `TextFieldCancelar`, `TextFieldInputChar`, `TextFieldFinDeCuadro`) एडिटर में, `main/ui/ViewPorts/NumInput.cpp` में परिभाषित हैं।
- **यह Core के सीन मॉडल पर निर्भर है।** `PropertieBase.h`, `objects/Objects.h` को include करता है, और प्रॉपर्टी की सूची मेश, आर्मेचर, एनिमेशन और स्क्रिप्ट पढ़ती है।
- **स्केल एडिटर दोबारा गणना करता है।** `SetGlobalScale`, `main/ui/ViewPorts/ViewPorts.cpp` में है। अकेले इस्तेमाल करने पर, माप हाथ से दोबारा गणना करने पड़ते हैं ([रंग और स्केल](tema.md))।
- **सब कुछ ग्लोबल है।** हर प्रोग्राम में एक फ़ॉन्ट (`WhiskFont`), आइकन की एक टेबल (`IconsUV`), एक खुला मेनू (`MenuAbierto`) और एक फ़ोकस वाला टेक्स्ट बॉक्स (`g_textFieldActivo`) होता है।
- **हेडर OpenGL के टाइप दिखाते हैं** (`GLfloat`, `GLshort`, `GLuint`), हालाँकि वे अब OpenGL को कॉल नहीं करते।
- **आगे क्या आ रहा है,** `README.md` के अनुसार: एडिटर वाला कोड साफ़ करना और इस्तेमाल के उदाहरण बनाना।

क्या UI का है और क्या एडिटर का, एक-एक करके: [क्या UI का है और क्या एडिटर का](ui-y-editor.md)। सवाल [Telegram](https://t.me/Whisk3D) ग्रुप में पूछें।
