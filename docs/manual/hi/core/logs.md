# लॉग

printf शैली के तीन स्तर: `w3dLogf` (जानकारी), `w3dLogfW` (चेतावनी) और `w3dLogfE` (एरर)। `g_w3dLogNivel` रनटाइम पर फ़िल्टर करता है (3 = सब कुछ, 0 = कुछ नहीं) और `W3D_DEV_LOG=0` के साथ कंपाइल करने पर ये कॉल एक्ज़ीक्यूटेबल से हट जाते हैं। आख़िरी लाइनें मेमोरी में भी रहती हैं: एडिटर का [Console](../editor/consola.md) यही दिखाता है।

| सिस्टम | कहाँ जाता है |
|---|---|
| PC | `whisk3d.log`, हर लाइन मिलीसेकंड वाले टाइमस्टैम्प और `[INFO]`, `[WARN]` या `[ERROR]` के साथ |
| Android | logcat, `Whisk3D` टैग के साथ (`adb logcat -s Whisk3D`) |
| ब्राउज़र | कंसोल: स्तर के हिसाब से `console.log`, `console.warn` और `console.error` |
| Symbian | `E:\whisk3d.log` |
| Dreamcast | सीरियल पोर्ट (dc-tool या Flycast का कंसोल) |
| PSP | स्टैंडर्ड आउटपुट (PPSSPP, psplink) और `EBOOT.PBP` के बगल में `whisk3d.log` |
| PS Vita | `sceClibPrintf` और `ux0:data/Whisk3D/whisk3d.log` |
| 3DS | `svcOutputDebugString` और `sdmc:/3ds/Whisk3D/whisk3d.log` |

यह क्यों ज़रूरी है: असली कंसोल पर कोई टर्मिनल नहीं होता, और किसी क्रैश के बाद कार्ड पर पड़ा लॉग ही अकेली चीज़ बचती है। इसीलिए 3DS, Vita और PSP पर हर लाइन उसी पल लिखी जाती है। N95 पर इसका उल्टा होता था: हर लाइन उसी पल लिखने में दसियों मिलीसेकंड लगते थे और गेम अटक जाता था। वहाँ जानकारी इकट्ठा होती है और हर 3 KB पर डिस्क पर जाती है, चेतावनियाँ तुरंत जाती हैं और एरर ज़बरदस्ती डिस्क पर लिखवाए जाते हैं। Lua से: [मैसेज और डीबगिंग](../lua/api-mensajes-y-depuracion.md)।
