# Lo script e la comunicazione

| Funzione | Cosa fa |
|---|---|
| `self() → obj` | L'oggetto proprietario di questo script. |
| `object(prop) → obj \| nil` | L'oggetto scelto nel campo `prop` di `properties`. |
| `option(prop) → text` | L'opzione scelta in un menu a tendina di `properties`; se non ne è stata scelta nessuna, la prima della lista (quella predefinita). |
| `property(prop) → value` | Il numero, sì/no o testo proprio di questo oggetto (o quello predefinito dichiarato). |
| `param(prop [, def]) → número` | Come `property` ma sempre un numero; se non esiste restituisce `def` (o 0). |
| `target() → obj \| nil` | L'obiettivo ("target") dell'oggetto proprietario. |
| `getShared(clave) → value` | Legge lo spazio condiviso da tutti gli script. |
| `setShared(clave, value)` | Scrive un numero, sì/no o testo nello spazio condiviso; `nil` lo cancella. |
