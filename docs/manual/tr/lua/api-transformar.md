# 3D'de taşıma, döndürme ve ölçekleme

| Fonksiyon | Ne yapar |
|---|---|
| `getPosition(obj) → x, y, z` | Nerede olduğu. |
| `setPosition(obj, x, y, z)` | Onu tam bir konuma yerleştirir (fiziği yok sayar). |
| `move(obj, dx, dy, dz)` | Onu bulunduğu yerden kaydırır (`dt` ile kullan). |
| `getRotation(obj) → x, y, z` | Derece cinsinden dönüşü. |
| `setRotation(obj, x, y, z [, order])` | Tam dönüş, Dönüştür kartındakiyle aynı. `order` ile eksenlerin hangi sırayla uygulanacağı seçilir, örneğin `"ZXY"` (önce dikey eksen etrafındaki yön, bir araçtaki gibi). |
| `getRotationX(obj)`, `getRotationY(obj)`, `getRotationZ(obj) → derece` | Tek bir eksenin dönüşü. `getRotationZ`, dikey eksen etrafındaki dönüştür. |
| `setRotationX(obj, g)`, `setRotationY(obj, g)`, `setRotationZ(obj, g)` | Yalnızca bir ekseni değiştirir; diğer ikisi aynı kalır. Örneğin `setRotationZ(obj, getRotationZ(obj) + 90 * dt)` onu dikey eksen etrafında döndürür. |
| `rotate(obj, dx, dy, dz)` | Onu kendi eksenleri etrafında döndürür. |
| `rotateTowards(obj, dx, dy [, factor])` | Onu zemindeki bir yöne (X ve Y) doğru yumuşakça döndürür. |
| `lookAt(obj, x, y, z)` | Onu bir noktaya doğru yöneltir (bir kameranın önü). |
| `getScale(obj) → sx, sy, sz` | Ölçeği. |
| `setScale(obj, s [, sy, sz])` | Tam ölçek; tek bir sayıyla eşit oranda ölçekler. |
| `scale(obj, f)` | Geçerli ölçeği çarpar. |
| `worldPoint(obj, x, y, z) → x, y, z` | Nesnenin bir noktasını dünya koordinatlarına çevirir. |
| `isVisible(obj) → bool`, `setVisible(obj, bool)` | Onu gösterir ya da gizler. Gizli bir nesne artık `update` çağrısı almaz. |
| `setTint(obj, r, g, b [, a])` | Rengini değiştirir (0 ile 1 arası değerler). |
