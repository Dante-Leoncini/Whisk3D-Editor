# Rutinler ve bellekler

Rutinler (Özellikler panelinde kurulan çizim adımı listeleri) Lua'nın koyduğu değerleri okuyabilir. Her bellek, adı olan 256 sayılık bir bloktur ve adımlar onu `@nombre[i]` olarak okur.

| Fonksiyon | Ne yapar |
|---|---|
| `setMemory(name, i, v)`, `getMemory(name, i)`, `setMemories(name, i, v1, v2, ...)` | Belleklere yazma ve onlardan okuma (dizin **0**'dan 255'e gider). |
| `setStep(routine, i, field, ...)`, `getStep(routine, i, field)` | Bir rutinin bir adımını değiştirme ya da okuma (alanlar: `"on"`, `"n"`, `"color"`, `"ref"`, `"modo"`, `"entero"`). |
| `step(name, ...)` | Bir çizim adımı yürütür; yalnızca bir rutinin "Lua çağır" adımının çağırdığı bir fonksiyonun içinde geçerlidir. |

**Tekrarla** değeri 1'den büyük olan bir rutinde `loop` belleği kaçıncı turda olunduğunu söyler: `@loop[0]` geçerli tur (0'dan başlayarak), `@loop[1]` ise toplamdır.
