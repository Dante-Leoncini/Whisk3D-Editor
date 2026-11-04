# कीबोर्ड शॉर्टकट

नीचे की टेबल के सभी फ़ंक्शन को दूसरी कुंजियाँ दी जा सकती हैं, और हर फ़ंक्शन के **एक या कई** कॉम्बिनेशन हो सकते हैं (या एक भी नहीं)। यह PC, Android (फ़िज़िकल कीबोर्ड के साथ) और Web पर काम करता है। N95 पर विंडो कुंजियाँ कैप्चर नहीं कर सकती: वहाँ `atajos.ini` फ़ाइल हाथ से एडिट की जाती है।

## शॉर्टकट कैसे बदलें

1. **Properties → File/Render टैब → Settings कार्ड → Keyboard Shortcuts…** खोलें।
2. **Context** चुनें (General, 3D Viewport, During a transform, UV Editor, Timeline, Outliner, 2D Editor, Properties, Script Editor)। बाईं/दाईं ऐरो कुंजियाँ भी इसे बदलती हैं।
3. सूची में फ़ंक्शन चुनें और **Add shortcut** पर टैप करें (या चुनी हुई पंक्ति पर फिर से टैप करें)। "Press a key…" दिखता है।
4. नया कॉम्बिनेशन दबाएँ (जैसे <kbd>Ctrl+M</kbd>)। <kbd>Esc</kbd> बिना कुछ बदले रद्द कर देता है।

| बटन | क्या करता है |
|---|---|
| Add shortcut | फ़ंक्शन में एक कॉम्बिनेशन जोड़ता है। अगर उसी कॉन्टेक्स्ट का कोई दूसरा फ़ंक्शन उसे इस्तेमाल कर रहा था, तो उससे हटा देता है और आपको बताता है। |
| Clear | फ़ंक्शन को बिना शॉर्टकट के छोड़ देता है। |
| Reset | फ़ंक्शन को उसके डिफ़ॉल्ट शॉर्टकट पर लौटा देता है। |
| Reset all | सभी फ़ंक्शन डिफ़ॉल्ट पर लौट जाते हैं। |

मेनू हर विकल्प के बगल में **सेट किया गया** शॉर्टकट दिखाते हैं: अगर आप किसी फ़ंक्शन की कुंजी बदलते हैं, तो मेनू नया कॉम्बिनेशन दिखाता है। N95 पर मेनू फ़ोन की कुंजी दिखाते हैं (जैसे Move के लिए <kbd>1</kbd> या Delete के लिए <kbd>C</kbd>); अगर कोई Bluetooth कीबोर्ड जुड़ा हो, तो वे उस कीबोर्ड की कुंजी दिखाते हैं, जब वे उसे समझ पाते हैं।

बदले गए कॉम्बिनेशन हरे रंग में दिखते हैं। जब किसी फ़ंक्शन से उसका डिफ़ॉल्ट शॉर्टकट हटा दिया जाता है, तो वह कुंजी ख़ाली हो जाती है: उस कॉन्टेक्स्ट में वह अब कुछ नहीं करती।

हर कॉन्टेक्स्ट के शॉर्टकट तब काम करते हैं जब पॉइंटर उस पैनल पर हो; **General** वाले हर जगह काम करते हैं। **During a transform** वाले तब काम करते हैं जब कुछ मूव, रोटेट या स्केल हो रहा हो, और उन्हें प्राथमिकता मिलती है। Script Editor में अक्षर हमेशा टाइप होते हैं: वहाँ सिर्फ़ <kbd>Ctrl</kbd>, <kbd>Alt</kbd> या F कुंजियों वाले कॉम्बिनेशन ही बदले जा सकते हैं।

## atajos.ini फ़ाइल

बदलाव अपने-आप `res/atajos.ini` में सेव होते हैं, `config.ini` के बगल में। इसमें सिर्फ़ बदले गए फ़ंक्शन होते हैं, और इसे हाथ से एडिट किया जा सकता है:

```ini
# फ़ंक्शन = कॉम्बिनेशन, दूसरा कॉम्बिनेशन   (ख़ाली = कोई शॉर्टकट नहीं)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

कुंजियों के नाम: अक्षर और अंक जैसे हैं वैसे ही, `Enter`, `Esc`, `Tab`, `Space`, `Backspace`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`, `F1`…`F12`, `Num0`…`Num9`, `Num.`, `NumEnter`, `Comma`, `Plus`। मॉडिफ़ायर: `Ctrl+`, `Shift+`, `Alt+`।

## डिफ़ॉल्ट शॉर्टकट

<!-- नीचे की टेबल, ATAJOS-FIN तक, tools/genmanual_atajos.py कोड से बनाता है: इन्हें हाथ से एडिट न करें। -->

<!--ATAJOS-INICIO-->

### General (कोई भी पैनल)

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | atajos.ini में नाम |
|---|---|---|---|
| Save project | <kbd>Ctrl+S</kbd> | — | `global.guardar` |
| Undo | <kbd>Ctrl+Z</kbd> | <kbd>C+Left</kbd> | `global.deshacer` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Right</kbd> | `global.rehacer` |
| Object / Edit Mode | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.modo` |
| Toggle Snap | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Snap menu | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Play / Pause | <kbd>Space</kbd> | <kbd>Green+Left soft</kbd> | `global.play` |
| Menu bar | <kbd>M</kbd> | <kbd>Left soft</kbd> | `global.barraMenu` |
| Parent | <kbd>Ctrl+P</kbd> | — | `global.emparentar` |
| Clear Parent | <kbd>Alt+P</kbd> | — | `global.desemparentar` |
| Fullscreen | <kbd>F11</kbd> | — | `global.pantalla` |
| Render Image | <kbd>F12</kbd> | — | `global.render` |
| Render Animation | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### 3D Viewport

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | atajos.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | <kbd>Pencil+Up</kbd> | `3d.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | <kbd>Pencil+Up</kbd> | `3d.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Box Select | <kbd>B</kbd> | — | `3d.caja` |
| Select Linked | <kbd>L</kbd> | — | `3d.vinculado` |
| Add | <kbd>Shift+A</kbd> | — | `3d.agregar` |
| Apply | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `3d.mover` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotar` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `3d.escalar` |
| Clear Location (Pose) | <kbd>Alt+G</kbd> | — | `3d.limpiarPos` |
| Clear Rotation (Pose) | <kbd>Alt+R</kbd> | — | `3d.limpiarRot` |
| Shrink/Fatten / Clear Scale | <kbd>Alt+S</kbd> | — | `3d.limpiarEsc` |
| Duplicate | <kbd>Shift+D</kbd> | — | `3d.duplicar` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `3d.instancia` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.borrar` |
| Hide / Show | <kbd>H</kbd> | — | `3d.ocultar` |
| Join | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Insert Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Proportional Editing | <kbd>O</kbd> | — | `3d.proporcional` |
| Overlays | <kbd>K</kbd> | — | `3d.overlays` |
| Local View | <kbd>/</kbd> | — | `3d.vistaLocal` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.encuadrar` |
| Front View | <kbd>Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaFrente` |
| Back View | <kbd>Ctrl+Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaAtras` |
| Right View | <kbd>Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaDerecha` |
| Left View | <kbd>Ctrl+Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaIzquierda` |
| Top View | <kbd>Num7</kbd> | <kbd>#+Up</kbd> | `3d.vistaArriba` |
| Bottom View | <kbd>Ctrl+Num7</kbd> | <kbd>#+Down</kbd> | `3d.vistaAbajo` |
| Perspective / Orthographic | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspectiva` |
| Roll Left | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Roll Right | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Camera View | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Set Active Camera | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Extrude | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| New Face from Vertices | <kbd>F</kbd> | — | `3d.cara` |
| Recalculate Normals | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Loop Cut and Slide | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Connect Vertex Path | <kbd>J</kbd> | — | `3d.conectar` |
| Rip | <kbd>V</kbd> | — | `3d.rip` |
| Vertex Slide | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separate | <kbd>P</kbd> | — | `3d.separar` |
| Merge | <kbd>M</kbd> | — | `3d.merge` |
| Context menu | <kbd>W</kbd> | — | `3d.menuContexto` |
| Triangulate | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| UV menu | <kbd>U</kbd> | — | `3d.menuUV` |

### During a transform (Move / Rotate / Scale)

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | atajos.ini में नाम |
|---|---|---|---|
| Confirm | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirmar` |
| Cancel | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancelar` |
| X axis | <kbd>X</kbd> | <kbd>1</kbd> | `xf.ejeX` |
| Y axis | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.ejeY` |
| Z axis | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.ejeZ` |
| Exclude X (plane) | <kbd>Shift+X</kbd> | — | `xf.planoX` |
| Exclude Y (plane) | <kbd>Shift+Y</kbd> | — | `xf.planoY` |
| Exclude Z (plane) | <kbd>Shift+Z</kbd> | — | `xf.planoZ` |
| Proportional radius + | <kbd>PageUp</kbd> | — | `xf.radioMas` |
| Proportional radius - | <kbd>PageDown</kbd> | — | `xf.radioMenos` |

### UV Editor

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | atajos.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `uv.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `uv.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `uv.invertir` |
| Select Linked | <kbd>L</kbd> | — | `uv.vinculado` |
| Box Select | <kbd>B</kbd> | — | `uv.caja` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `uv.mover` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotar` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `uv.escalar` |
| Insert Keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `uv.keyframeTodo` |
| Extrude | <kbd>E</kbd> | — | `uv.extruir` |
| Duplicate | <kbd>Shift+D</kbd> | — | `uv.duplicar` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.borrar` |
| Frame Selected | <kbd>Num.</kbd> | — | `uv.encuadrar` |

### Timeline

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | atajos.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Rotate handles | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Duplicate | <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Delete | <kbd>X</kbd> | — | `tl.borrar` |
| Interpolation | <kbd>T</kbd> | — | `tl.interp` |
| Handle type | <kbd>V</kbd> | — | `tl.handle` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Previous frame | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Next frame | <kbd>Right</kbd> | — | `tl.frameSig` |
| Previous keyframe | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Next keyframe | <kbd>Up</kbd> | — | `tl.keySig` |
| Go to start | <kbd>Home</kbd> | — | `tl.inicio` |
| Go to end | <kbd>End</kbd> | — | `tl.fin` |

### Outliner

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | atajos.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `out.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `out.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `out.invertir` |
| Move (reorder) | <kbd>G</kbd> | <kbd>1</kbd> | `out.mover` |
| Rename | <kbd>F2</kbd> | — | `out.renombrar` |
| Duplicate | <kbd>Shift+D</kbd> | — | `out.duplicar` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `out.instancia` |
| Hide / Show | <kbd>H</kbd> | <kbd>2</kbd> | `out.ocultar` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.borrar` |
| Show active | <kbd>Num.</kbd> | — | `out.encuadrar` |

### 2D Editor

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | atajos.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `2d.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `2d.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `2d.invertir` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `2d.mover` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotar` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `2d.escalar` |
| Duplicate | <kbd>Shift+D</kbd> | — | `2d.duplicar` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.borrar` |
| Frame Selected | <kbd>Num.</kbd> | — | `2d.encuadrar` |
| Game View | <kbd>Num0</kbd> | — | `2d.vistaJuego` |

### Properties (रूटीन के कदम)

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | atajos.ini में नाम |
|---|---|---|---|
| Grab step | <kbd>G</kbd> | — | `props.agarrar` |
| Delete step | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Insert Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Script Editor

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | atajos.ini में नाम |
|---|---|---|---|
| Save script | <kbd>Ctrl+S</kbd> | — | `ide.guardar` |
| Undo | <kbd>Ctrl+Z</kbd> | — | `ide.deshacer` |
| Copy | <kbd>Ctrl+C</kbd> | — | `ide.copiar` |
| Cut | <kbd>Ctrl+X</kbd> | — | `ide.cortar` |
| Paste | <kbd>Ctrl+V</kbd> | — | `ide.pegar` |
| Select All | <kbd>Ctrl+A</kbd> | — | `ide.todo` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.rehacer` |
| Find | <kbd>Ctrl+F</kbd> | — | `ide.buscar` |
| Replace | <kbd>Ctrl+H</kbd> | — | `ide.reemplazar` |

<!--ATAJOS-FIN-->

## तय कुंजियाँ (बदली नहीं जा सकतीं)

कुछ कुंजियाँ सूची में नहीं हैं, क्योंकि वे इस पर निर्भर करती हैं कि आप क्या कर रहे हैं: मेनू और पैनल में घूमते समय ऐरो कुंजियाँ, मेनू और विंडो में <kbd>Enter</kbd> और <kbd>Esc</kbd>, Outliner में पंक्ति मूव करते समय ऐरो कुंजियाँ, लिखते समय IDE का कीबोर्ड, और N95 पर फ़ोन का कीपैड ([नेविगेशन](navegacion.md) देखें)।
