# Script ve iletişim

| Fonksiyon | Ne yapar |
|---|---|
| `self() → obj` | Bu script'in sahibi olan nesne. |
| `object(prop) → obj \| nil` | `properties` içindeki `prop` alanında seçilen nesne. |
| `option(prop) → text` | `properties` içindeki bir açılır listede seçilen seçenek; hiçbiri seçilmediyse listenin ilki (varsayılan). |
| `property(prop) → value` | Bu nesneye özel sayı, evet/hayır ya da metin (veya tanımlanan varsayılan değer). |
| `param(prop [, def]) → número` | `property` gibi ama her zaman sayı döndürür; yoksa `def`'i (ya da 0) döndürür. |
| `target() → obj \| nil` | Sahip nesnenin hedefi ("target"). |
| `getShared(clave) → value` | Tüm script'lerin paylaştığı ortak alandan okur. |
| `setShared(clave, value)` | Ortak alana bir sayı, evet/hayır ya da metin yazar; `nil` onu siler. |
