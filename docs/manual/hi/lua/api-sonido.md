# साउंड

| फ़ंक्शन | क्या करता है |
|---|---|
| `beep(freq [, ms, vol])` | एक बीप (बिना फ़ाइल के)। |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | एक साउंड बजाता है; एक पहचानकर्ता लौटाता है। |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | वही, नामों के साथ, साथ में एनवेलप (अटैक, डिके, 0 से 1 तक सस्टेन, `fall`: सस्टेन इतने सेकंड में 60 dB गिरता है, रिलीज़, और `duration`: नोट इतने सेकंड बाद अपने आप छूट जाता है) और साउंड का कितना हिस्सा रिवर्ब में जाता है (0 से 1)। |
| `stopSound(h [, fundido])`, `stopSounds()`, `adjustSound(h, vol [, pitch])` | साउंड रोकना या एडजस्ट करना। |
| `releaseSound(h)` | नोट छोड़ता है: एनवेलप अपने रिलीज़ में चला जाता है। |
| `reverb(room, damping, level)` | सामान्य रिवर्ब: कमरे का आकार, डैम्पिंग और स्तर (हर एक 0 से 1; स्तर 0 बंद करता है)। |
| `mute(bool)`, `isMuted()` | सारी आवाज़ बंद करना। |
| `playSound(obj [, seg])`, `muteSound(obj)` | किसी **Sound ऑब्जेक्ट** को फिर से शुरू करें (उसके हिस्से के `seg` सेकंड से) या चुप कराएँ। |
| `soundObject(obj) → t`, `setSoundObject(obj, t)` | Sound ऑब्जेक्ट पढ़ें / बदलें: `file` (स्रोत), `active`, `volume`, `pitch`, `reverb`, `pan`, `start`, `clipStart`, `clipEnd`, `loop`, `positional`, `radius`, `range` (पढ़ते समय `duration`, `playing` भी)। |
| `createSound("sounds/x.wav" [, parent]) → obj` | नया Sound ऑब्जेक्ट (`destroy(obj)` उसे हटाता है)। प्रीफ़ैब में यह किसी भी ऑब्जेक्ट की तरह बनता है। |
