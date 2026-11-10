# Rutinler ve bellekler

Rutinler (Özellikler panelinde kurulan çizim adımı listeleri) Lua'nın koyduğu değerleri okuyabilir. Her bellek, adı olan 256 sayılık bir bloktur ve adımlar onu `@nombre[i]` olarak okur.

| Fonksiyon | Ne yapar |
|---|---|
| `setMemoria(nombre, i, v)`, `memoria(nombre, i)`, `setMemorias(nombre, i, v1, v2, ...)` | Belleklere yazma ve onlardan okuma (dizin **0**'dan 255'e gider). |
| `setPaso(rutina, i, campo, ...)`, `leerPaso(rutina, i, campo)` | Bir rutinin bir adımını değiştirme ya da okuma (alanlar: `"on"`, `"n"`, `"color"`, `"ref"`, `"modo"`, `"entero"`). |
| `paso(nombre, ...)` | Bir çizim adımı yürütür; yalnızca bir rutinin "Lua çağır" adımının çağırdığı bir fonksiyonun içinde geçerlidir. |

**Tekrarla** değeri 1'den büyük olan bir rutinde `bucle` belleği kaçıncı turda olunduğunu söyler: `@bucle[0]` geçerli tur (0'dan başlayarak), `@bucle[1]` ise toplamdır.
