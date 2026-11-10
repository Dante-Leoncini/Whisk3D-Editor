# क्या UI का है और क्या एडिटर का

एडिटर Whisk3D UI से बना है, लेकिन उसमें जो दिखता है उसका बहुत-सा हिस्सा उसका अपना है। UI वाला हिस्सा `libs/WhiskUI/` में है; एडिटर वाला, Whisk3D रिपॉज़िटरी के `main/` में।

## UI का

- 2D ड्रॉइंग, बॉर्डर वाले कार्ड और आयत।
- आइकन की टेबल और एटलस पैकर।
- बिटमैप फ़ॉन्ट, एक्सटेंडेड एटलस और बेक किए गए TTF फ़ॉन्ट वाला टेक्स्ट।
- रंगों का पैलेट और माप, उनके स्केल के साथ।
- बटन, टैब, चेकबॉक्स और ड्रॉपडाउन मेनू।
- प्रॉपर्टी कार्ड और उनकी पंक्तियाँ, एनिमेशन वाले डायमंड के साथ।
- टेक्स्ट बॉक्स की स्टेट और उनकी ड्रॉइंग।

## एडिटर का

| क्या | कहाँ |
|---|---|
| व्यूपोर्ट: स्क्रीन को बाँटना, उनकी बॉर्डर, उनकी मेनू बार और टूलबार | `main/ui/ViewPorts/` (`ViewPorts.cpp`, `WithBorder.cpp`, `ToolbarBase.cpp`) |
| प्रॉपर्टी पैनल: कौन-से कार्ड हैं, उनके टैब, पंक्तियों पर क्लिक और ड्रैग | `main/ui/ViewPorts/Properties.cpp` |
| मोडल विंडो: कलर पिकर, फ़ाइल एक्सप्लोरर, पुष्टि वाले डायलॉग, प्रोग्रेस बार, ऑन-स्क्रीन कीबोर्ड | `main/ui/ViewPorts/PopUp/` |
| नोटिफ़िकेशन | `main/ui/ViewPorts/Notificaciones.cpp` |
| पैनलों की स्क्रॉल बार | `main/ui/ViewPorts/ScrollBar.cpp` |
| मदद वाले छोटे टूलटिप। टेक्स्ट `Button::ayuda` में रहता है | `W3dAyudaRender`, `main/ui/ViewPorts/LayoutInput.cpp` में |
| कौन-सी कुंजी, क्लिक या टच किस विजेट को जाता है, और N95 की कुंजियाँ | `main/ui/ViewPorts/LayoutInput.cpp`, `main/app/controles.cpp`, `platform/symbian/src/` |
| टेक्स्ट बॉक्स के फ़ोकस वाले फ़ंक्शन | `main/ui/ViewPorts/NumInput.cpp` |
| अनुवाद, `T()`। UI में सिर्फ़ `W3dMenuTraducir` हुक है | `main/config/W3dLang.cpp` |
| UI का स्टार्टअप | `main/app/W3dInitUI.cpp` |
| अलग-अलग आइकन से स्किन का एटलस बनाना | `main/io/SkinAtlas.cpp` |
| स्केल दोबारा गणना करना | `SetGlobalScale`, `main/ui/ViewPorts/ViewPorts.cpp` में |
| PC और Android पर `skin.ini` पढ़ना | `loadColors`, `main/app/main.cpp` में |
| एडिटर के अपने रंग: फ़र्श, अक्ष, नॉर्मल | `main/ui/W3dColors.cpp` |
| स्किन की फ़ाइलें: `font.png`, `font-ext.png`, `atlas/iconos/`, `skin.ini` | `res/Skins/Whisk3D/` |
| एक्सटेंडेड एटलस का जनरेटर | `tools/fuente_ext.py` |
| [2D Editor](../editor/editor-2d.md) और गेम के `.w3dui` इंटरफ़ेस: टेक्स्ट, इमेज, आयत, कंटेनर, slice 9, बटन और वीडियो | `main/objects/` (`UI.h`, `Texto2D.h`, `Boton2D.h`...), `main/render/UIOverlay.cpp`, `main/io/UI2DFormato.cpp`, `main/io/Fuente2D.cpp`, `main/ui/ViewPorts/Editor2D.cpp` |

2D Editor टेक्स्ट को `W3dTextAtlas` से ड्रॉ करता है, जो UI का है। चलते समय stb_truetype से किसी TTF को बेक करना एडिटर का है (`main/io/Fuente2D.cpp`)।

## UI में, लेकिन एडिटर का

ऐसी चीज़ें जो `libs/WhiskUI/` में हैं और अभी भी एडिटर की हैं:

- `PropListMeshParts` मेश, आर्मेचर, एनिमेशन, कंस्ट्रेंट और स्क्रिप्ट पढ़ता है, और प्रॉपर्टी पैनल का `RebindMaterialMeshPart()` कॉल करता है।
- `PropColor` कलर पिकर के लिए पैलेट और वह ऑब्जेक्ट सेव करता है जिससे रंग आता है (`palRef`, `palObj`)।
- `GroupProperties` प्रॉपर्टी पैनल के कार्ड की ग्लोबल सूची है।
- `core/UI.h` में एडिटर की स्टेट है: Edit Mode का टच व्यू मोड (`g_viewEditMode`), माउस की हलचल (`dx`, `dy`), Alt, Shift और Ctrl कुंजियाँ (`LAltPressed`, `LShiftPressed`, `LCtrlPressed`) और व्यूपोर्ट के माप (`paddingViewport`, `MinViewportHeight`, `MinViewportWidth`)।
- ज़्यादातर आइकन एडिटर के टूल हैं।

क्या अलग करना बाकी है, और लाइब्रेरी की स्थिति: [UI को सीधे इस्तेमाल करना](empezar.md) का आख़िरी हिस्सा।
