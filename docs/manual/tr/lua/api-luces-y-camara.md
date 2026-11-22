# Işıklar ve kamera

| Fonksiyon | Ne yapar |
|---|---|
| `getColor(light) → r, g, b`, `setColor(light, r, g, b)` | Bir ışığın rengi. |
| `getEnergy(light) → e`, `setEnergy(light, e)` | Parlaklığı, 0'dan 1'e (titremesi ya da yavaş yavaş sönmesi için). |
| `setActiveCamera(cam)` | Oyunun görüntülendiği kamerayı değiştirir. |
| `getLens(cam) → fov, near, far`, `setLens(cam [, fov, near, far])` | Görüş açısı ve kırpma mesafeleri. |
| `cameraXZ() → fx, fy, rx, ry` | Kameranın zemin üzerinde, X ve Y'de nereye baktığı: ileri (`fx, fy`) ve sağa (`rx, ry`); karakteri kameraya göre hareket ettirmek için. (Adı, zeminin XZ olduğu zamandan kalmadır.) |
| `screenOf(obj) → x, y, front` | Bir 3D nesnenin ekranda nerede göründüğü (üzerine 2D bir etiket koymak için). |
| `setRail(cam, curve [, offset])`, `setRailNode`, `setRailLook`, `getRail` | Kameranın bir eğriyi (ray) takip etmesini sağlama. |
| `resolution() → width, height` | 3D görüntünün piksel cinsinden gerçek boyutu. |
