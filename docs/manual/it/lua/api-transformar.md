# Spostare, ruotare e scalare in 3D

| Funzione | Cosa fa |
|---|---|
| `getPosition(obj) → x, y, z` | Dove si trova. |
| `setPosition(obj, x, y, z)` | Lo mette in un punto esatto (ignora la fisica). |
| `move(obj, dx, dy, dz)` | Lo sposta da dove si trova (usalo con `dt`). |
| `getRotation(obj) → x, y, z` | La sua rotazione in gradi. |
| `setRotation(obj, x, y, z [, order])` | Rotazione esatta, la stessa del riquadro Trasforma. Con `order` si sceglie in che ordine si applicano gli assi, per esempio `"ZXY"` (prima la rotta attorno alla verticale, come un veicolo). |
| `getRotationX(obj)`, `getRotationY(obj)`, `getRotationZ(obj) → gradi` | La rotazione di un solo asse. `getRotationZ` è la rotazione attorno alla verticale. |
| `setRotationX(obj, g)`, `setRotationY(obj, g)`, `setRotationZ(obj, g)` | Cambia un solo asse; gli altri due restano uguali. Per esempio, `setRotationZ(obj, getRotationZ(obj) + 90 * dt)` lo fa girare attorno alla verticale. |
| `rotate(obj, dx, dy, dz)` | Lo ruota sui propri assi. |
| `rotateTowards(obj, dx, dy [, factor])` | Lo fa ruotare gradualmente verso una direzione sul pavimento (X e Y). |
| `lookAt(obj, x, y, z)` | Lo punta verso un punto (il fronte di una camera). |
| `getScale(obj) → sx, sy, sz` | La sua scala. |
| `setScale(obj, s [, sy, sz])` | Scala esatta; con un solo numero scala in modo uniforme. |
| `scale(obj, f)` | Moltiplica la scala attuale. |
| `worldPoint(obj, x, y, z) → x, y, z` | Converte un punto dell'oggetto in coordinate del mondo. |
| `isVisible(obj) → bool`, `setVisible(obj, bool)` | Lo mostra o lo nasconde. Un oggetto nascosto smette di ricevere `update`. |
| `setTint(obj, r, g, b [, a])` | Ne cambia il colore (valori da 0 a 1). |
