# Mesajlar ve hata ayıklama

| Fonksiyon | Ne yapar |
|---|---|
| `print(...)` | Ekrana değil, motorun günlüğüne (log) yazar. |
| `info(msg)`, `warning(msg)`, `error(msg)` | Günlüğe bilgi, uyarı ya da hata mesajları yazar. `error` script'i **durdurmaz**. |
| `debugLog(msg)`, `isDebug()` | Yalnızca hata ayıklama sürümlerinde görünen mesajlar. |
| `logCount()`, `logLine(i)` | Günlüğü oyunun içinden okuma (kendi konsolun). |
