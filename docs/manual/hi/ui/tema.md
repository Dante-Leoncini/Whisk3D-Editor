# रंग और स्केल

## पैलेट

UI के रंग भूमिकाएँ हैं, तय रंग नहीं: `ColorID` enum (`theme/colores.h`)।

| भूमिका | किस काम आता है |
|---|---|
| `background` | बैकग्राउंड। बंद चेकबॉक्स का बॉक्स और सामान्य अवस्था में बटन की बॉर्डर |
| `blanco` | टेक्स्ट और उस चीज़ की बॉर्डर जो माउस के नीचे है या चुनी हुई है |
| `accent` | फ़ोकस: मेनू की बॉर्डर, जो एडिट हो रहा है, सक्रिय विकल्प, स्लाइडर का भराव |
| `accentDark` | चुने हुए टेक्स्ट का बैकग्राउंड और नंबर वाले बटन की पट्टी |
| `negro` | `PropSeparator` की लाइन |
| `gris` | कार्ड, बटन और मेनू का मुख्य हिस्सा |
| `headerColor` | मेनू की हाइलाइट की गई पंक्ति और स्लाइडर का ट्रैक |
| `negroTransparente` | 25 % पर काला। UI इसे इस्तेमाल नहीं करती |
| `grisUI` | सामान्य टेक्स्ट और चालू चेकबॉक्स का बॉक्स |
| `grisLinea` | मेनू में समूहों को अलग करने वाली लाइनें |

`ListaColores` में ये `float` में हैं और `ListaColoresUbyte` में बाइट में। `SetColorID` इनमें से एक को ड्रॉइंग का रंग बनाता है, वैकल्पिक अल्फ़ा के साथ:

```cpp
SetColorID(ColorID::accent);         // अपारदर्शी
SetColorID(ColorID::grisUI, 0.5f);   // आधी अपारदर्शिता पर
```

`loadColorsW3d(ruta)` इन्हें एक टेक्स्ट फ़ाइल से पढ़ता है, हर लाइन में एक रंग: नाम और 0 से 1 तक के चार कंपोनेंट, जैसे `accent 0.0 1.0 0.0 1.0`। जो नाम यह नहीं जानता उन्हें अनदेखा करता है, इसलिए वही `skin.ini` किसी दूसरे प्रोग्राम के रंग भी रख सकती है। अगर फ़ाइल नहीं खुल पाई तो यह `false` लौटाता है। फ़ाइल के बिना कोड वाले रंग (`theme/colores.cpp`) रहते हैं; Whisk3D की स्किन इन्हें बदलती है, उदाहरण के लिए `accent` हरा हो जाता है।

कुछ रंग कोड में लिखे हुए हैं और पैलेट से नहीं आते: बिना सेव किए बदलाव वाले डायमंड का हरा, एरर वाले `PropLabel` का लाल, अधूरे `PropText` की लाल बॉर्डर और बंद `PropButton` का ग्रे।

## स्केल

मूल माप `core/UI.h` में हैं, पिक्सेल में:

| माप | मान |
|---|---|
| `margin`, `padding` | 10 |
| `border` | 4 |
| `gap` | 5 |
| `RenglonHeight` | 12 |
| `LetterWidth`, `CharacterWidth` | 6 |
| `LetterHeight` | 11 |

`GlobalScale` इन्हें गुणा करता है, हमेशा एक पूर्णांक से: इस तरह पिक्सेल वाले अक्षर साफ़ बने रहते हैं। जैसे, N95 की 240 पिक्सेल वाली स्क्रीन पर 1 और PC पर 3।

हर माप का पहले से गुणा किया हुआ वर्ज़न है, आख़िर में `GS` के साथ: `RenglonHeightGS`, `gapGS`, `borderGS`, `bordersGS` (बॉर्डर का दोगुना), `CharacterWidthGS`, `LetterHeightGS`... विजेट यही इस्तेमाल करते हैं।

UI में वह फ़ंक्शन नहीं है जो इन्हें दोबारा गणना करता है: एडिटर वाला `SetGlobalScale` है, `main/ui/ViewPorts/ViewPorts.cpp` में। अकेले, इसे ऐसे किया जाता है:

```cpp
void Escala(int s) {
    GlobalScale = s;
    marginGS = margin * s;  paddingGS = padding * s;  gapGS = gap * s;
    borderGS = border * s;  bordersGS = borderGS * 2;
    RenglonHeightGS = RenglonHeight * s;
    LetterWidthGS = LetterWidth * s;  LetterHeightGS = LetterHeight * s;
    CharacterWidthGS = CharacterWidth * s;
    SetIconScale(s);
    if (WhiskFont) WhiskFont->SetScale(s);
}
```

हर विजेट के आकार उसके `Resize` में गणना होते हैं: स्केल बदलने के बाद, हर कार्ड, बटन और मेनू में इसे फिर से कॉल करना होता है।

## कॉम्पैक्ट या टच

`UIBotonAltura()` बटन की ऊँचाई देता है और `UIBarPadding()` किसी बार में बटन के ऊपर और नीचे की ख़ाली जगह। दोनों `UITactil` पर निर्भर करते हैं:

- `false`, कॉम्पैक्ट मोड, N95 के लिए सोचा गया: बटन एक पंक्ति और बॉर्डर के बराबर होता है, `2 * GlobalScale` कम, और बार ऊपर और नीचे `GlobalScale` छोड़ती है।
- `true`: बटन और बार में एक `gap` जुड़ जाता है, ताकि उंगली समा सके। बॉर्डर मोटी नहीं होतीं।

`UITactil` `false` से शुरू होता है और आज इसे कुछ भी चालू नहीं करता।
