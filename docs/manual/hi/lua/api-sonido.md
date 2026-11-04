# साउंड

| फ़ंक्शन | क्या करता है |
|---|---|
| `beep(freq [, ms, vol])` | एक बीप (बिना फ़ाइल के)। |
| `sonido("sonidos/x.wav" [, vol, tono, loop, pan]) → h` | एक साउंड बजाता है; एक पहचानकर्ता लौटाता है। |
| `sonido("sonidos/x.wav", {vol, tono, loop, pan, ataque, decaimiento, sostenido, caida, liberacion, duracion, reverb}) → h` | वही, नामों के साथ, साथ में एनवेलप (अटैक, डिके, 0 से 1 तक सस्टेन, `caida`: सस्टेन इतने सेकंड में 60 dB गिरता है, रिलीज़, और `duracion`: नोट इतने सेकंड बाद अपने आप छूट जाता है) और साउंड का कितना हिस्सा रिवर्ब में जाता है (0 से 1)। |
| `pararSonido(h [, fundido])`, `pararSonidos()`, `ajustarSonido(h, vol [, tono])` | साउंड रोकना या एडजस्ट करना। |
| `soltarSonido(h)` | नोट छोड़ता है: एनवेलप अपने रिलीज़ में चला जाता है। |
| `reverb(sala, amortiguacion, nivel)` | सामान्य रिवर्ब: कमरे का आकार, डैम्पिंग और स्तर (हर एक 0 से 1; स्तर 0 बंद करता है)। |
| `silenciar(bool)`, `estaMudo()` | सारी आवाज़ बंद करना। |
| `tocarSonido(obj [, seg])`, `callarSonido(obj)` | किसी **Sound ऑब्जेक्ट** को फिर से शुरू करें (उसके हिस्से के `seg` सेकंड से) या चुप कराएँ। |
| `sonidoObj(obj) → t`, `setSonidoObj(obj, t)` | Sound ऑब्जेक्ट पढ़ें / बदलें: `archivo` (स्रोत), `activo`, `volumen`, `tono`, `reverb`, `paneo`, `inicio`, `recorteIni`, `recorteFin`, `bucle`, `posicional`, `radio`, `alcance` (पढ़ते समय `duracion`, `sonando` भी)। |
| `crearSonido("sonidos/x.wav" [, padre]) → obj` | नया Sound ऑब्जेक्ट (`destruir(obj)` उसे हटाता है)। प्रीफ़ैब में यह किसी भी ऑब्जेक्ट की तरह बनता है। |
