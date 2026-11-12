# Script ve iletişim

| Fonksiyon | Ne yapar |
|---|---|
| `yo() → obj` | Bu script'in sahibi olan nesne. |
| `objeto(prop) → obj \| nil` | `propiedades` içindeki `prop` alanında seçilen nesne. |
| `opcion(prop) → texto` | `propiedades` içindeki bir açılır listede seçilen seçenek; hiçbiri seçilmediyse listenin ilki (varsayılan). |
| `propiedad(prop) → valor` | Bu nesneye özel sayı, evet/hayır ya da metin (veya tanımlanan varsayılan değer). |
| `parametro(prop [, def]) → número` | `propiedad` gibi ama her zaman sayı döndürür; yoksa `def`'i (ya da 0) döndürür. |
| `objetivo() → obj \| nil` | Sahip nesnenin hedefi ("target"). |
| `compartido(clave) → valor` | Tüm script'lerin paylaştığı ortak alandan okur. |
| `setCompartido(clave, valor)` | Ortak alana bir sayı, evet/hayır ya da metin yazar; `nil` onu siler. |
