# 3D'de taşıma, döndürme ve ölçekleme

| Fonksiyon | Ne yapar |
|---|---|
| `posicion(obj) → x, y, z` | Nerede olduğu. |
| `setPosicion(obj, x, y, z)` | Onu tam bir konuma yerleştirir (fiziği yok sayar). |
| `mover(obj, dx, dy, dz)` | Onu bulunduğu yerden kaydırır (`dt` ile kullan). |
| `rotacion(obj) → x, y, z` | Derece cinsinden dönüşü. |
| `setRotacion(obj, x, y, z [, orden])` | Tam dönüş, Dönüştür kartındakiyle aynı. `orden` ile eksenlerin hangi sırayla uygulanacağı seçilir, örneğin `"ZXY"` (önce dikey eksen etrafındaki yön, bir araçtaki gibi). |
| `rotacionX(obj)`, `rotacionY(obj)`, `rotacionZ(obj) → derece` | Tek bir eksenin dönüşü. `rotacionZ`, dikey eksen etrafındaki dönüştür. |
| `setRotacionX(obj, g)`, `setRotacionY(obj, g)`, `setRotacionZ(obj, g)` | Yalnızca bir ekseni değiştirir; diğer ikisi aynı kalır. Örneğin `setRotacionZ(obj, rotacionZ(obj) + 90 * dt)` onu dikey eksen etrafında döndürür. |
| `girar(obj, dx, dy, dz)` | Onu kendi eksenleri etrafında döndürür. |
| `girarHacia(obj, dx, dy [, factor])` | Onu zemindeki bir yöne (X ve Y) doğru yumuşakça döndürür. |
| `mirarA(obj, x, y, z)` | Onu bir noktaya doğru yöneltir (bir kameranın önü). |
| `escala(obj) → sx, sy, sz` | Ölçeği. |
| `setEscala(obj, s [, sy, sz])` | Tam ölçek; tek bir sayıyla eşit oranda ölçekler. |
| `escalar(obj, f)` | Geçerli ölçeği çarpar. |
| `puntoMundo(obj, x, y, z) → x, y, z` | Nesnenin bir noktasını dünya koordinatlarına çevirir. |
| `visible(obj) → bool`, `setVisible(obj, bool)` | Onu gösterir ya da gizler. Gizli bir nesne artık `actualizar` çağrısı almaz. |
| `setTinte(obj, r, g, b [, a])` | Rengini değiştirir (0 ile 1 arası değerler). |
