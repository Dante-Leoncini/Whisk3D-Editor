# El script y la comunicación

| Función | Qué hace |
|---|---|
| `self() → obj` | El objeto dueño de este script. |
| `object(prop) → obj \| nil` | El objeto elegido en el campo `prop` de `properties`. |
| `option(prop) → text` | La opción elegida en una lista desplegable de `properties`; si no se eligió ninguna, la primera de la lista (la de fábrica). |
| `property(prop) → value` | El número, sí/no o texto propio de este objeto (o el de fábrica declarado). |
| `param(prop [, def]) → número` | Como `property` pero siempre número; si no existe devuelve `def` (o 0). |
| `target() → obj \| nil` | El objetivo ("target") del objeto dueño. |
| `getShared(clave) → value` | Lee el espacio compartido por todos los scripts. |
| `setShared(clave, value)` | Escribe un número, sí/no o texto en el espacio compartido; `nil` lo borra. |
