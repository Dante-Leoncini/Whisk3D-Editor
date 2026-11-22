# Basit fizik (hız ve sekmeler)

| Fonksiyon | Ne yapar |
|---|---|
| `getVelocity(obj, vx [, vy, vz])` / `getVelocity(obj) → vx, vy, vz` | Ona bir hız verirsin, motor da onu kendiliğinden hareket ettirir. |
| `accelerate(obj, factor [, tope])` | Hızını çarparak onu hızlandırır. |
| `box(obj [, x, y, z])` | Çarpışma kutusunun boyutu: 3D'de genişlik (X), derinlik (Y) ve yükseklik (Z); 2D'de piksel cinsinden genişlik ve yükseklik. |
| `bounce(obj, other)`, `bounceOn(obj, area [, sides])`, `bounceInside(obj, area)` | Başka bir nesneden ya da bir alanın kenarlarından sekme (sektiyse `true` döndürürler). |
