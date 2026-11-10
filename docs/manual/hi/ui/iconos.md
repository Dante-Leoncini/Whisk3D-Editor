# आइकन

## फ़ाइलें

हर आइकन **10x10** का एक PNG है, एडिटर की रिपॉज़िटरी में `res/Skins/Whisk3D/atlas/iconos/` में। आज ये 106 हैं। पिक्सेल पारदर्शी पर सफ़ेद हैं: आइकन वह रंग ले लेता है जिससे उसे ड्रॉ किया जाता है। इस तरह एक ही आइकन ग्रे, सफ़ेद या एक्सेंट रंग में काम आता है।

## enum और नाम

`draw/icons.h` में `IconType` enum और उनकी संख्या, `ICON_TOTAL` (106), है। `draw/icons.cpp` में `KNombres` हर एक का नाम देता है, उसी क्रम में: यह `.png` के बिना फ़ाइल का नाम है।

```cpp
IconoNombre(IconType::carpeta);   // "carpeta"
IconoIndice("carpeta");           // IconType::carpeta, या -1 अगर मौजूद नहीं है
```

लगभग सभी का नाम वही है जो enum में है। अपवाद `candadoAbierto` है, जो `candado_abierto.png` फ़ाइल है।

बहुत-से एडिटर के हैं: मॉडलिंग के टूल, ऑब्जेक्ट के प्रकार, एनिमेशन के कर्व। सामान्य आइकन भी हैं: `arrow` और `arrowRight` (फ़ोल्ड करने वाले ऐरो), `visible` और `hidden`, `carpeta`, `guardar`, `borrar`, `mas` और `menos`, `lista` और `cuadricula`, `candado`, `notifOk` और `notifError`, `tijera`, `copiar` और `pegar`।

## कोऑर्डिनेट

`IconsUV` एक `std::vector<IconUV*>` है, जिसमें एटलस में हर आइकन के कोऑर्डिनेट (`uvs`, 8 नंबर) हैं। यह दो तरह से भरता है:

- `CrearIconos(ancho, alto)`: उस टेबल से जो बताती है कि हर आइकन `font.png` के अंदर कहाँ है (`TablaLegacy`, `icons.cpp` में)। यह `atlas/iconos/` फ़ोल्डर के बिना वाला तरीका है।
- `CrearIconosDesde(rects, ancho, alto)`: आयतों की अपनी एक सूची से, जो बनाए गए एटलस की है। `x = -1` वाला आयत `font.png` वाला इस्तेमाल करता है: जिस आइकन का PNG नहीं है, वह भी दिखता रहता है।

`SetIconScale(escala)`, `IconSizeGS` और दोनों मेश को एडजस्ट करता है: `IconMesh`, एक आइकन का वर्ग, और `IconLineMesh`, एक पंक्ति की ऊँचाई वाला वर्ग, ट्री की लाइनों के लिए।

## एक आइकन ड्रॉ करना

एटलस सेट होने पर, मौजूदा ओरिजिन पर:

```cpp
SetColorID(ColorID::grisUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::carpeta]->uvs);
```

`Button`, `Tab`, `PopupMenu` और `GroupPropertie` आइकन को enum के एक `int` के रूप में लेते हैं, या बिना आइकन के लिए `-1`।

## एक आइकन जोड़ना

1. 10x10 का PNG, `atlas/iconos/` में।
2. `IconType` enum के आख़िर में एक पंक्ति, और `ICON_TOTAL` में एक जोड़ें।
3. उसका नाम `KNombres` के आख़िर में।
4. `TablaLegacy` के आख़िर में एक पंक्ति, `font.png` के उस आइकन के साथ जो PNG न होने पर उसकी जगह लेता है।

पंक्ति 4 के बिना, जब कोई बना हुआ एटलस नहीं होता तो आइकन बिना कोऑर्डिनेट के रह जाता है।
