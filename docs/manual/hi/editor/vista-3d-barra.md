# 3D Viewport की मेनू बार

![Object Mode में 3D Viewport की मेनू बार, एक क्यूब चुना हुआ: Object Mode, पिवट, Global, मैग्नेट, प्रोपोर्शनल एडिटिंग, View, Select, Add, Animation, Object, Overlays, Render और सीन](../tutoriales/img/vista-3d-barra-menus.webp)

अगर मेनू बार पूरी नहीं समाती, तो उसे स्क्रॉल किया जा सकता है: [जब मेनू बार के सारे विकल्प न दिखें](interfaz.md#जब-मेनू-बार-के-सारे-विकल्प-न-दिखें) देखें।

## मोड

![मोड का बटन](../tutoriales/img/boton-vista3d-modo.webp) यह दिखाता है कि आप किस मोड में काम कर रहे हैं, और इस पर टैप करके दूसरा मोड चुना जाता है। कौन-से मोड दिखेंगे, यह सक्रिय ऑब्जेक्ट पर निर्भर करता है: मेश में Edit Mode और तीनों पेंट मोड होते हैं; आर्मेचर में Edit Mode और Pose Mode। किसी दूसरे प्रकार के ऑब्जेक्ट (लाइट, कैमरा) के साथ बटन नहीं दिखता, क्योंकि उसका इकलौता मोड Object Mode है। हर मोड [मोड](vista-3d.md#मोड) में समझाया गया है।

![क्यूब चुना होने पर मोड का मेनू: Object Mode, Edit Mode, Vertex Paint, Weight Paint और Texture Paint](../tutoriales/img/vista-3d-menu-modo.webp)

![PC](../tutoriales/img/icono-pc.svg) <kbd>Tab</kbd> Object Mode से Edit Mode में ले जाता है और वापस लाता है। ![Nokia](../tutoriales/img/icono-nokia.svg) N95 पर, <kbd>OK</kbd>।

मेश के **Edit Mode** में बगल में **Vertex / Edge / Face** बटन दिखता है: एक क्लिक से क्या चुना जाएगा ([Edit Mode](modo-edicion.md) देखें)।

## पिवट

![पिवट का बटन](../tutoriales/img/boton-vista3d-pivote.webp) **पिवट पॉइंट** वह जगह है जहाँ से ट्रांसफ़ॉर्म होता है: चुनी हुई चीज़ें किसके चारों ओर घूमती हैं और कहाँ से स्केल होती हैं। बटन का आइकन चुने हुए पिवट के हिसाब से बदलता है, ताकि एक नज़र में दिख जाए कि कौन-सा लगा है।

![Transform Pivot Point मेनू: 3D Cursor, Individual Origins, Median Point (चुना हुआ, हरे रंग में) और Active Element, हर एक अपने आइकन के साथ](../tutoriales/img/vista-3d-menu-pivote.webp)

| पिवट | कहाँ से घूमता और स्केल होता है |
|---|---|
| ![3D Cursor](../tutoriales/img/boton-pivote-cursor.webp) **3D Cursor** | 3D कर्सर से। यह चीज़ों को किसी भी पॉइंट के चारों ओर घुमाने के काम आता है: कर्सर को वहाँ रखें ([Snap मेनू](#snap) से) और रोटेट करें। |
| ![Individual Origins](../tutoriales/img/boton-pivote-individual.webp) **Individual Origins** | हर ऑब्जेक्ट अपने ओरिजिन से: सब एक साथ घूमते हैं, हर एक अपनी जगह पर। |
| ![Median Point](../tutoriales/img/boton-pivote-medio.webp) **Median Point** | सारी चुनी हुई चीज़ों के केंद्र से। यही डिफ़ॉल्ट है। |
| ![Active Element](../tutoriales/img/boton-pivote-activo.webp) **Active Element** | सक्रिय ऑब्जेक्ट (आख़िरी चुना गया) के ओरिजिन से: बाकी उसके चारों ओर घूमते हैं। |

हर एक का एक उदाहरण, वीडियो में, [Object Mode](../tutoriales/modo-objeto.md#पिवट-पॉइंट) ट्यूटोरियल में है।

## ओरिएंटेशन

![ओरिएंटेशन का बटन](../tutoriales/img/boton-vista3d-orientacion.webp) किसी एक अक्ष पर मूव, रोटेट या स्केल करते समय (<kbd>X</kbd>, <kbd>Y</kbd>, <kbd>Z</kbd> से, गिज़्मो से या N95 की कुंजियों से) इस्तेमाल होने वाले अक्ष। बटन चुना हुआ ओरिएंटेशन दिखाता है।

![Transform Orientations मेनू: Global, Local, Normal और View](../tutoriales/img/vista-3d-menu-orientacion.webp)

| ओरिएंटेशन | अक्ष |
|---|---|
| **Global** | दुनिया के अक्ष: हमेशा एक जैसे, Z ऊपर की ओर। यही डिफ़ॉल्ट है। |
| **Local** | ऑब्जेक्ट के अपने अक्ष: ये उसके साथ घूमते हैं। घुमाई गई कार अपने Local अक्ष पर आगे बढ़ती है। |
| **Normal** | Edit Mode में: चुना हुआ फ़ेस जिस दिशा में देखता है (Extrude यही इस्तेमाल करता है)। |
| **View** | स्क्रीन के अक्ष: X बगल की ओर, Y ऊपर की ओर। |

![PC](../tutoriales/img/icono-pc.svg) ट्रांसफ़ॉर्म के दौरान अक्ष का अक्षर दूसरी बार दबाने से Global से Local हो जाता है: <kbd>G</kbd> <kbd>Y</kbd> <kbd>Y</kbd>।

## Snap

![Snap (मैग्नेट) का बटन](../tutoriales/img/boton-vista3d-iman.webp) **Snap** चालू होने पर, जो चीज़ हिल रही है वह किसी चीज़ से चिपक जाती है: दूसरे ऑब्जेक्ट के वर्टेक्स, एज या फ़ेस से, या उनके केंद्रों से। चालू होने पर बटन हरा हो जाता है।

![Snap मेनू: Enable, Snap Base, Snap Target और Affect Move, Rotate, Scale तथा Include Active, Edited और Non-Edited चेकबॉक्स](../tutoriales/img/vista-3d-menu-iman.webp)

| विकल्प | क्या करता है | शॉर्टकट |
|---|---|---|
| Enable | Snap को चालू या बंद करता है। | <kbd>Shift+Tab</kbd> |
| Snap Base | हिलने वाली चीज़ का कौन-सा पॉइंट चिपकेगा: सबसे पास वाला, केंद्र, मीडियन या सक्रिय। | मेनू |
| Snap Target | किससे चिपकेगा: वर्टेक्स, एज, फ़ेस, उनके केंद्र। Face Project / Face Nearest से वर्टेक्स किसी दूसरी सतह पर टिक जाते हैं (रीटोपोलॉजी के लिए)। | मेनू |
| Affect Move / Rotate / Scale | किन ऑपरेशन में काम करेगा। | मेनू |
| Include Active / Edited / Non-Edited | किन ऑब्जेक्ट से चिपक सकता है। | मेनू |

**3D कर्सर** एक दूसरे मेनू से हिलता है, कर्सर वाले Snap मेनू से: <kbd>Shift</kbd> + <kbd>S</kbd> या Object मेनू → Snap (Cursor to Selected, Selection to Cursor...)। यह [Object Mode](../tutoriales/modo-objeto.md#3d-कर्सर) में समझाया गया है।

## Proportional Editing

![प्रोपोर्शनल एडिटिंग का बटन](../tutoriales/img/boton-vista3d-proporcional.webp) **प्रोपोर्शनल एडिटिंग** से, किसी चीज़ को मूव, रोटेट या स्केल करने पर उसके पास की चीज़ें नरमी से साथ चलती हैं: जितनी पास, उतना ज़्यादा। यह एक-एक वर्टेक्स हिलाए बिना प्राकृतिक आकार (ज़मीन, चेहरा) मॉडल करने के काम आता है। चालू होने पर यह हरा हो जाता है।

![Proportional Editing मेनू: Enable, Connected Only, falloff के आकार (Smooth, Sphere, Root, Inverse Square, Sharp, Linear, Constant, Random) और Proportional Size](../tutoriales/img/vista-3d-menu-proporcional.webp)

| विकल्प | क्या करता है | शॉर्टकट |
|---|---|---|
| Enable | प्रोपोर्शनल एडिटिंग को चालू या बंद करता है। | <kbd>O</kbd> |
| Connected Only | दूरी को एज के रास्ते मापता है: पास पड़े अलग टुकड़ों को साथ नहीं खींचता। | मेनू |
| Falloff | दूरी के साथ असर कैसे बँटता है: Smooth, Sphere, Root, Inverse Square, Sharp, Linear, Constant या Random। | मेनू |
| Proportional Size | असर का दायरा। ट्रांसफ़ॉर्म के दौरान इसे व्हील से या <kbd>PageUp</kbd> / <kbd>PageDown</kbd> से बदला जाता है। | मेनू / व्हील |

## View

![View बटन](../tutoriales/img/boton-vista3d-vista.webp) **नेविगेट करने** से जुड़ा सब कुछ: सीन को कहाँ से देखा जाए। हर विकल्प, अपने वीडियो के साथ, [3D Viewport में नेविगेट करना](../tutoriales/navegacion.md) ट्यूटोरियल में है।

![View मेनू: Cameras, Viewpoint, Frame Selected, Local View, Perspective/Ortho, Lock Orbit और Lock the camera to the view](../tutoriales/img/vista-3d-menu-vista.webp)

| विकल्प | क्या करता है | ![PC](../tutoriales/img/icono-pc.svg) शॉर्टकट |
|---|---|---|
| Cameras | सक्रिय कैमरे से देखना, या चुने हुए ऑब्जेक्ट को सक्रिय कैमरा बनाना। | <kbd>Num 0</kbd> / <kbd>Ctrl+Num 0</kbd> |
| Viewpoint | Front, Back, Right, Left, Top, Bottom। | <kbd>Num 1</kbd>, <kbd>Num 3</kbd>, <kbd>Num 7</kbd> (<kbd>Ctrl</kbd> के साथ, उलटी दिशा वाला) |
| Frame Selected | व्यू को चुनी हुई चीज़ पर केंद्रित करता है और उसे इतना पास लाता है कि वह ठीक से समा जाए। | <kbd>Num .</kbd> |
| Local View | व्यू में सिर्फ़ चुनी हुई चीज़ छोड़ता है, और वापस लाता है। | <kbd>/</kbd> |
| Perspective/Ortho | पर्सपेक्टिव से ऑर्थोग्राफ़िक में ले जाता है और वापस लाता है। | <kbd>Num 5</kbd> |
| Lock Orbit | जो पहले ऑर्बिट करता था, वह अब व्यू को खिसकाता है। | — |
| Lock the camera to the view | कैमरे से देखते समय: नेविगेट करने से कैमरा हिलता है। | — |

## Select

![Select बटन](../tutoriales/img/boton-vista3d-seleccionar.webp) एक बार में सब कुछ चुनना, कुछ नहीं, जो चुना है उसका उलटा, या जो किसी बॉक्स के अंदर आए।

![Select मेनू: All, None, Invert और Box Select](../tutoriales/img/vista-3d-menu-seleccionar.webp)

| विकल्प | क्या करता है | शॉर्टकट |
|---|---|---|
| All | सब कुछ चुनता है। | <kbd>A</kbd> |
| None | कुछ भी चुना हुआ नहीं छोड़ता। | <kbd>Alt+A</kbd> |
| Invert | जो चुना हुआ है वह अचयनित हो जाता है, और उलटा भी। | <kbd>Ctrl+I</kbd> |
| Box Select | ड्रैग किया जाने वाला एक बॉक्स: बाएँ से दाएँ (हरा) सिर्फ़ वही चुनता है जो पूरी तरह अंदर हो; दाएँ से बाएँ (नीला) बस छू जाना काफ़ी है। <kbd>Shift</kbd> जोड़ता है। | <kbd>B</kbd>, या बैकग्राउंड पर ड्रैग करें |

Edit Mode में इसमें Select Linked, लूप और पाथ भी जुड़ जाते हैं ([Edit Mode](modo-edicion.md) देखें)।

## Add

![Add बटन](../tutoriales/img/boton-vista3d-anadir.webp) नए ऑब्जेक्ट बनाता है, जो 3D कर्सर पर दिखते हैं और चुने हुए रहते हैं। यह सिर्फ़ Object Mode में होता है।

![खुला हुआ Add मेनू, सभी प्रकार के ऑब्जेक्ट के साथ](../tutoriales/img/vista-3d-menu-anadir.webp)

हर प्रकार [Object Mode → Add मेनू](modo-objeto.md#add-मेनू) में है। ![PC](../tutoriales/img/icono-pc.svg) <kbd>Shift</kbd> + <kbd>A</kbd> इसे वहाँ खोलता है जहाँ माउस है।

Edit Mode में इसकी जगह **Mesh** (मॉडलिंग के ऑपरेशन, [Edit Mode](modo-edicion.md) देखें) और **UV** (UV को अनरैप और प्रोजेक्ट करना, [UV Editor](editor-uv.md) देखें) दिखते हैं। Texture Paint में एक **डिस्केट** दिखता है, जो पेंट किया गया टेक्सचर सेव करता है; बिना सेव की गई पेंटिंग होने पर यह हरा हो जाता है।

## Animation

![Animation बटन](../tutoriales/img/boton-vista3d-animacion.webp) चुनी हुई चीज़ों के **कीफ़्रेम**: मौजूदा फ़्रेम में पोज़िशन, रोटेशन या स्केल की वैल्यू सेव करना, ताकि एनिमेशन एक कीफ़्रेम से अगले तक चले। यह कुछ चुना होने पर, Object Mode और Pose Mode में दिखता है, और इसके विकल्प इस पर निर्भर करते हैं कि क्या चुना है।

![क्यूब चुना होने पर Animation मेनू: Insert Keyframe, Insert Mesh Keyframe, Delete Keyframe, Clear Keyframe, Delete Mesh Layer और Motion Trail](../tutoriales/img/vista-3d-menu-animacion.webp)

| विकल्प | क्या करता है | शॉर्टकट |
|---|---|---|
| Insert Keyframe | यह चुनने के लिए एक मेनू कि क्या सेव हो: पोज़िशन, रोटेशन, स्केल या सब कुछ। | <kbd>I</kbd> (<kbd>Shift+I</kbd>: बिना पूछे सब कुछ) |
| Insert Mesh Keyframe | सिर्फ़ 3D मेश के साथ: चुनता है कि मेश के एनिमेशन से क्या सेव हो, वर्टेक्स, नॉर्मल या UV। | मेनू |
| Delete Keyframe | मौजूदा फ़्रेम का कीफ़्रेम डिलीट करता है। | मेनू |
| Clear Keyframe | चुनी हुई चीज़ों का पूरा एनिमेशन डिलीट करता है। | मेनू |
| Delete Mesh Layer | सिर्फ़ 3D मेश के साथ: मेश के एनिमेशन की एक पूरी लेयर डिलीट करता है, **वर्टेक्स** वाली, **नॉर्मल** वाली (जैसे, बिना लाइटिंग वाले मॉडल में) या **UV** वाली। कम से कम एक कीफ़्रेम हमेशा बचा रहता है। | मेनू |
| Motion Trail | एनिमेट किए गए ऑब्जेक्ट जिस रास्ते पर चलते हैं, उसे व्यू में ड्रॉ करता है। | मेनू |

### Pose Mode में

आर्मेचर को पोज़ देते समय, मेनू चुनी हुई बोन (और सक्रिय बोन) के कीफ़्रेम के साथ काम करता है:

![Pose Mode में Animation मेनू, Leon के आर्मेचर के साथ: Insert Keyframe, Delete Keyframe, Clear Keyframe, Reset Pose और Motion Trail](../tutoriales/img/vista-3d-menu-animacion-pose.webp)

| विकल्प | क्या करता है | शॉर्टकट |
|---|---|---|
| Insert Keyframe | चुनी हुई बोन की पोज़ मौजूदा फ़्रेम में सेव करता है: पोज़िशन, रोटेशन, स्केल या सब कुछ। | <kbd>I</kbd> |
| Delete Keyframe | उन बोन का मौजूदा फ़्रेम वाला कीफ़्रेम डिलीट करता है। <kbd>Ctrl+Z</kbd> से अनडू होता है। | मेनू |
| Clear Keyframe | सक्रिय एनिमेशन में उन बोन के सारे कीफ़्रेम डिलीट करता है। <kbd>Ctrl+Z</kbd> से अनडू होता है। | मेनू |
| Reset Pose | चुनी हुई बोन (या कोई न चुनी हो तो सारी) उस पोज़िशन, रोटेशन और स्केल पर लौट आती हैं जो Edit Mode में उनकी है। <kbd>Ctrl+Z</kbd> से अनडू होता है। | मेनू |
| Motion Trail | एनिमेट किए गए ऑब्जेक्ट जिस रास्ते पर चलते हैं, उसे व्यू में ड्रॉ करता है। | मेनू |

एनिमेशन का अपना ट्यूटोरियल और अपना अध्याय है, [Timeline](linea-de-tiempo.md)।

## Object

![Object बटन](../tutoriales/img/boton-vista3d-objeto.webp) चुनी हुई चीज़ों के साथ क्या किया जा सकता है। यह वही मेनू है जो व्यू में राइट क्लिक से (या, टचस्क्रीन पर, एक उंगली स्थिर रखकर) खुलता है। यह कुछ चुना होने पर दिखता है।

![Object मेनू: Transform, Set Origin, Apply, Snap, Cut, Copy, Paste, Duplicate Objects, Duplicate Linked, Join, Set Parent, Clear Parent, Delete और प्रीफ़ैब](../tutoriales/img/vista-3d-menu-objeto.webp)

हर विकल्प [Object Mode](modo-objeto.md) में है। Edit Mode में यह बटन उस चीज़ का मेनू बन जाता है जो चुनी जाती है (Vertex, Edge या Face); Pose Mode में Pose का; आर्मेचर एडिट करते समय Armature का।

## Overlays

![Overlays बटन](../tutoriales/img/boton-vista3d-superposiciones.webp) **ओवरले** वे सहायक चीज़ें हैं जो काम करने के लिए सीन के ऊपर ड्रॉ होती हैं और रेंडर में नहीं आतीं: फ़र्श, अक्ष, गिज़्मो, 3D कर्सर। इन्हें यहीं से चालू और बंद किया जाता है, सब एक साथ या एक-एक करके।

![Overlays मेनू: Show Overlays, Floor, X Axis, Y Axis, Origins, Gizmo, Objects, 3D Cursor, Relationship Lines, Normals, Statistics, X-Ray और Black Letterbox](../tutoriales/img/vista-3d-menu-superposiciones.webp)

| विकल्प | क्या करता है | शॉर्टकट |
|---|---|---|
| Show Overlays | सारी सहायक चीज़ें एक साथ चालू या बंद करता है। | <kbd>K</kbd> |
| Floor, X Axis, Y Axis, Origins, 3D Cursor, Relationship Lines | हर सहायक चीज़ अलग से। Relationship lines हर चाइल्ड को उसके पैरेंट से जोड़ती हैं। | मेनू |
| Gizmo | मूव, रोटेट और स्केल का गिज़्मो। Nokia N95 पर यह डिफ़ॉल्ट रूप से बंद होता है। | मेनू |
| Objects | किन प्रकारों के ऑब्जेक्ट ड्रॉ होंगे: आर्मेचर, लाइट, कैमरे, एम्प्टी, पार्टिकल, कर्व, हिटबॉक्स। | मेनू |
| Normals | वर्टेक्स या फ़ेस के नॉर्मल उनके साइज़ के साथ ड्रॉ करता है। | मेनू |
| Statistics | फ़्रेम प्रति सेकंड, वर्टेक्स, फ़ेस, ड्रॉ कॉल और समय। | मेनू |
| X-Ray | Edit Mode में मेश अर्ध-पारदर्शी दिखती है और पीछे वाले एलिमेंट भी चुने जा सकते हैं। | मेनू |
| Black Letterbox | जब स्क्रीन से अलग अनुपात वाले कैमरे से देखा जा रहा हो, तो शॉट से बाहर का हिस्सा ढक देता है। | मेनू |

## Render

![Render बटन](../tutoriales/img/boton-vista3d-render.webp) **शेडिंग मोड**: व्यू में सीन कैसे ड्रॉ होता है। इससे प्रोजेक्ट नहीं बदलता, बस यह बदलता है कि काम करते समय वह कैसा दिखता है।

![Render मेनू: Render Preview, Material Preview, Solid Preview, Wireframe Preview, ZBuffer Preview, Normal View, Alpha Preview, Motion vectors और Wireframe: Only Visible](../tutoriales/img/vista-3d-menu-render.webp)

![Leon एक के बाद एक शेडिंग मोड से गुज़रते हुए](../tutoriales/img/vista-3d-sombreado.webp)

| मोड | कैसा दिखता है |
|---|---|
| Render Preview | सीन की लाइटों के साथ। रे ट्रेसिंग चालू होने पर, ट्रेस की गई इमेज धीरे-धीरे दिखाता है। |
| Material Preview | डिफ़ॉल्ट: मटीरियल और टेक्सचर, व्यू की तरफ़ से एक न्यूट्रल लाइट के साथ, सीन की लाइटों पर निर्भर हुए बिना। |
| Solid Preview | सब कुछ स्लेटी, आकार देखने के लिए। |
| Wireframe Preview | सिर्फ़ एज। **Wireframe: Only Visible** के साथ, सिर्फ़ उन चीज़ों के जो सामने से दिखती हैं। |
| ZBuffer Preview | गहराई: पास उजला, दूर गहरा। |
| Normal View | नॉर्मल, रंगों में। |
| Alpha Preview | पारदर्शिता, काले-सफ़ेद में। |
| Motion vectors | पिछले फ़्रेम से हर पिक्सेल कितना हिला (लाल = X, हरा = Y)। यह रेंडर के `_vectores` पास वाली ही इमेज है। |

![PC](../tutoriales/img/icono-pc.svg) <kbd>Z</kbd> अगले मोड पर ले जाता है। ![Nokia](../tutoriales/img/icono-nokia.svg) N95 पर, <kbd>6</kbd>।

## सीन

![सीन का बटन](../tutoriales/img/boton-vista3d-escena.webp) जो एडिट हो रहा है उसका नाम: प्रोजेक्ट का कोई **सीन**, **गेम** या **प्रीफ़ैब**। इस पर टैप करके दूसरा चुना जाता है, नया बनाया जाता है या सीन को गेम में बदला जाता है।

![सीन का मेनू: मौजूदा सीन, New Scene, New Game, New Prefab और Convert to Game](../tutoriales/img/vista-3d-menu-escena.webp)

सीन, गेम और प्रीफ़ैब [प्रोजेक्ट (.w3d) और फ़ाइलें](proyecto.md) में हैं।
