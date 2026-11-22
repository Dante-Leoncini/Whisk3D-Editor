# Sahneler ve ara sahneler

| Fonksiyon | Ne yapar |
|---|---|
| `changeScene(name [, restart])` | Karenin sonunda başka bir sahneye (menü, 2. seviye…) geçer. |
| `playScene(name [, "funcion"])` | Oyun duraklatılmışken bir 3D sahneyi ara sahne olarak oynatır; bitince o fonksiyonu çağırır. |
| `stopScene3D()`, `scenePlaying()` | Ara sahneyi keser / hangisinin oynatıldığını söyler. |
| `draw3D([on])` | Tüm 3D'yi kapatır ya da açar (örneğin opak bir menünün arkasında). |
