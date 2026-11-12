# Işıklar ve kamera

| Fonksiyon | Ne yapar |
|---|---|
| `color(luz) → r, g, b`, `setColor(luz, r, g, b)` | Bir ışığın rengi. |
| `energia(luz) → e`, `setEnergia(luz, e)` | Parlaklığı, 0'dan 1'e (titremesi ya da yavaş yavaş sönmesi için). |
| `setCamaraActiva(cam)` | Oyunun görüntülendiği kamerayı değiştirir. |
| `lenteDe(cam) → fov, cerca, lejos`, `setLente(cam [, fov, cerca, lejos])` | Görüş açısı ve kırpma mesafeleri. |
| `camaraXZ() → fx, fy, rx, ry` | Kameranın zemin üzerinde, X ve Y'de nereye baktığı: ileri (`fx, fy`) ve sağa (`rx, ry`); karakteri kameraya göre hareket ettirmek için. (Adı, zeminin XZ olduğu zamandan kalmadır.) |
| `pantallaDe(obj) → x, y, delante` | Bir 3D nesnenin ekranda nerede göründüğü (üzerine 2D bir etiket koymak için). |
| `setRiel(cam, curva [, offset])`, `setRielNodo`, `setMiradaRiel`, `rielDe` | Kameranın bir eğriyi (ray) takip etmesini sağlama. |
| `resolucion() → ancho, alto` | 3D görüntünün piksel cinsinden gerçek boyutu. |
