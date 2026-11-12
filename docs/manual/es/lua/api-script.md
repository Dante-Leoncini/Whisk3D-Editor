# El script y la comunicación

| Función | Qué hace |
|---|---|
| `yo() → obj` | El objeto dueño de este script. |
| `objeto(prop) → obj \| nil` | El objeto elegido en el campo `prop` de `propiedades`. |
| `opcion(prop) → texto` | La opción elegida en una lista desplegable de `propiedades`; si no se eligió ninguna, la primera de la lista (la de fábrica). |
| `propiedad(prop) → valor` | El número, sí/no o texto propio de este objeto (o el de fábrica declarado). |
| `parametro(prop [, def]) → número` | Como `propiedad` pero siempre número; si no existe devuelve `def` (o 0). |
| `objetivo() → obj \| nil` | El objetivo ("target") del objeto dueño. |
| `compartido(clave) → valor` | Lee el espacio compartido por todos los scripts. |
| `setCompartido(clave, valor)` | Escribe un número, sí/no o texto en el espacio compartido; `nil` lo borra. |
