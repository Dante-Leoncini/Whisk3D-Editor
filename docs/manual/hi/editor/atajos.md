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

## shortcuts.ini फ़ाइल

बदलाव अपने-आप `res/shortcuts.ini` में सेव होते हैं, `config.ini` के बगल में। इसमें सिर्फ़ बदले गए फ़ंक्शन होते हैं, और इसे हाथ से एडिट किया जा सकता है:

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

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | shortcuts.ini में नाम |
|---|---|---|---|
| Save project | <kbd>Ctrl+S</kbd> | — | `global.save` |
| Undo | <kbd>Ctrl+Z</kbd> | <kbd>C+Left</kbd> | `global.undo` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Right</kbd> | `global.redo` |
| Object / Edit Mode | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.mode` |
| Toggle Snap | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Snap menu | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Play / Pause | <kbd>Space</kbd> | <kbd>Green+Left soft</kbd> | `global.play` |
| Menu bar | <kbd>M</kbd> | <kbd>Left soft</kbd> | `global.menuBar` |
| Parent | <kbd>Ctrl+P</kbd> | — | `global.parent` |
| Clear Parent | <kbd>Alt+P</kbd> | — | `global.clearParent` |
| Fullscreen | <kbd>F11</kbd> | — | `global.screen` |
| Render Image | <kbd>F12</kbd> | — | `global.render` |
| Render Animation | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### 3D Viewport

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | shortcuts.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | <kbd>Pencil+Up</kbd> | `3d.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | <kbd>Pencil+Up</kbd> | `3d.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `3d.invert` |
| Box Select | <kbd>B</kbd> | — | `3d.box` |
| Select Linked | <kbd>L</kbd> | — | `3d.linked` |
| Add | <kbd>Shift+A</kbd> | — | `3d.add` |
| Apply | <kbd>Ctrl+A</kbd> | — | `3d.apply` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `3d.move` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotate` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `3d.scale` |
| Clear Location (Pose) | <kbd>Alt+G</kbd> | — | `3d.clearPos` |
| Clear Rotation (Pose) | <kbd>Alt+R</kbd> | — | `3d.clearRot` |
| Shrink/Fatten / Clear Scale | <kbd>Alt+S</kbd> | — | `3d.clearScale` |
| Duplicate | <kbd>Shift+D</kbd> | — | `3d.duplicate` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `3d.instance` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.delete` |
| Hide / Show | <kbd>H</kbd> | — | `3d.hide` |
| Join | <kbd>Ctrl+J</kbd> | — | `3d.join` |
| Insert Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `3d.keyframeAll` |
| Proportional Editing | <kbd>O</kbd> | — | `3d.proportional` |
| Overlays | <kbd>K</kbd> | — | `3d.overlays` |
| Local View | <kbd>/</kbd> | — | `3d.viewLocal` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.focus` |
| Front View | <kbd>Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.viewFront` |
| Back View | <kbd>Ctrl+Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.viewBack` |
| Right View | <kbd>Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.viewRight` |
| Left View | <kbd>Ctrl+Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.viewLeft` |
| Top View | <kbd>Num7</kbd> | <kbd>#+Up</kbd> | `3d.viewTop` |
| Bottom View | <kbd>Ctrl+Num7</kbd> | <kbd>#+Down</kbd> | `3d.viewBottom` |
| Perspective / Orthographic | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspective` |
| Roll Left | <kbd>Num4</kbd> | — | `3d.rollLeft` |
| Roll Right | <kbd>Num6</kbd> | — | `3d.rollRight` |
| Camera View | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.viewCamera` |
| Set Active Camera | <kbd>Ctrl+Num0</kbd> | — | `3d.activeCamera` |
| Extrude | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extrude` |
| New Face from Vertices | <kbd>F</kbd> | — | `3d.face` |
| Recalculate Normals | <kbd>Shift+N</kbd> | — | `3d.normals` |
| Loop Cut and Slide | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Connect Vertex Path | <kbd>J</kbd> | — | `3d.connect` |
| Rip | <kbd>V</kbd> | — | `3d.rip` |
| Vertex Slide | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separate | <kbd>P</kbd> | — | `3d.separate` |
| Merge | <kbd>M</kbd> | — | `3d.merge` |
| Context menu | <kbd>W</kbd> | — | `3d.contextMenu` |
| Triangulate | <kbd>Ctrl+T</kbd> | — | `3d.triangulate` |
| UV menu | <kbd>U</kbd> | — | `3d.menuUV` |

### During a transform (Move / Rotate / Scale)

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | shortcuts.ini में नाम |
|---|---|---|---|
| Confirm | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirm` |
| Cancel | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancel` |
| X axis | <kbd>X</kbd> | <kbd>1</kbd> | `xf.axisX` |
| Y axis | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.axisY` |
| Z axis | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.axisZ` |
| Exclude X (plane) | <kbd>Shift+X</kbd> | — | `xf.planeX` |
| Exclude Y (plane) | <kbd>Shift+Y</kbd> | — | `xf.planeY` |
| Exclude Z (plane) | <kbd>Shift+Z</kbd> | — | `xf.planeZ` |
| Proportional radius + | <kbd>PageUp</kbd> | — | `xf.radiusPlus` |
| Proportional radius - | <kbd>PageDown</kbd> | — | `xf.radiusMinus` |

### UV Editor

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | shortcuts.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `uv.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | — | `uv.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `uv.invert` |
| Select Linked | <kbd>L</kbd> | — | `uv.linked` |
| Box Select | <kbd>B</kbd> | — | `uv.box` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `uv.move` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotate` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `uv.scale` |
| Insert Keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `uv.keyframeAll` |
| Extrude | <kbd>E</kbd> | — | `uv.extrude` |
| Duplicate | <kbd>Shift+D</kbd> | — | `uv.duplicate` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.delete` |
| Frame Selected | <kbd>Num.</kbd> | — | `uv.focus` |

### Timeline

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | shortcuts.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `tl.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | — | `tl.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `tl.invert` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `tl.move` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `tl.scale` |
| Rotate handles | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotate` |
| Duplicate | <kbd>Shift+D</kbd> | — | `tl.duplicate` |
| Delete | <kbd>X</kbd> | — | `tl.delete` |
| Interpolation | <kbd>T</kbd> | — | `tl.interp` |
| Handle type | <kbd>V</kbd> | — | `tl.handle` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.focus` |
| Previous frame | <kbd>Left</kbd> | — | `tl.prevFrame` |
| Next frame | <kbd>Right</kbd> | — | `tl.nextFrame` |
| Previous keyframe | <kbd>Down</kbd> | — | `tl.prevKey` |
| Next keyframe | <kbd>Up</kbd> | — | `tl.nextKey` |
| Go to start | <kbd>Home</kbd> | — | `tl.start` |
| Go to end | <kbd>End</kbd> | — | `tl.end` |

### Outliner

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | shortcuts.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `out.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | — | `out.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `out.invert` |
| Move (reorder) | <kbd>G</kbd> | <kbd>1</kbd> | `out.move` |
| Rename | <kbd>F2</kbd> | — | `out.rename` |
| Duplicate | <kbd>Shift+D</kbd> | — | `out.duplicate` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `out.instance` |
| Hide / Show | <kbd>H</kbd> | <kbd>2</kbd> | `out.hide` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.delete` |
| Show active | <kbd>Num.</kbd> | — | `out.focus` |

### 2D Editor

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | shortcuts.ini में नाम |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `2d.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | — | `2d.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `2d.invert` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `2d.move` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotate` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `2d.scale` |
| Duplicate | <kbd>Shift+D</kbd> | — | `2d.duplicate` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.delete` |
| Frame Selected | <kbd>Num.</kbd> | — | `2d.focus` |
| Game View | <kbd>Num0</kbd> | — | `2d.gameView` |

### Properties (रूटीन के कदम)

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | shortcuts.ini में नाम |
|---|---|---|---|
| Grab step | <kbd>G</kbd> | — | `props.grab` |
| Delete step | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.delete` |
| Insert Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Script Editor

| फ़ंक्शन | डिफ़ॉल्ट शॉर्टकट | Nokia N95 (फ़ोन का कीपैड) | shortcuts.ini में नाम |
|---|---|---|---|
| Save script | <kbd>Ctrl+S</kbd> | — | `ide.save` |
| Undo | <kbd>Ctrl+Z</kbd> | — | `ide.undo` |
| Copy | <kbd>Ctrl+C</kbd> | — | `ide.copy` |
| Cut | <kbd>Ctrl+X</kbd> | — | `ide.cut` |
| Paste | <kbd>Ctrl+V</kbd> | — | `ide.paste` |
| Select All | <kbd>Ctrl+A</kbd> | — | `ide.all` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.redo` |
| Find | <kbd>Ctrl+F</kbd> | — | `ide.find` |
| Replace | <kbd>Ctrl+H</kbd> | — | `ide.replace` |

<!--ATAJOS-FIN-->

## तय कुंजियाँ (बदली नहीं जा सकतीं)

कुछ कुंजियाँ सूची में नहीं हैं, क्योंकि वे इस पर निर्भर करती हैं कि आप क्या कर रहे हैं: मेनू और पैनल में घूमते समय ऐरो कुंजियाँ, मेनू और विंडो में <kbd>Enter</kbd> और <kbd>Esc</kbd>, Outliner में पंक्ति मूव करते समय ऐरो कुंजियाँ, लिखते समय IDE का कीबोर्ड, और N95 पर फ़ोन का कीपैड ([नेविगेशन](navegacion.md) देखें)।
