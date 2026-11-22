# 2D arayüz ve ekran

| Fonksiyon | Ne yapar |
|---|---|
| `screen() → width, height` | Tuvalin boyutu. |
| `screenMargin() → mx, my` | Sabit boyutlu bir tuvalin etrafında, her yanda gerçek kenarlara kadar ne kadar ekran kaldığı, tuval birimleriyle (kadrajın şeritleri de ekrandır). Dikeyde 4:3 bir tuvalle: bir göstergeyi `my` kadar yukarı kaydırmak onu üst kenara yapıştırır. |
| `screenFrame() → x, y, width, height` | 3D görüntünün çerçevesi (kameranın kadrajı, şeritler olmadan), tuval birimleriyle, merkezinden itibaren. |
| `captureScreen(name [, withHud]) → path` | Bu karenin 3D görüntüsünü (sahne bittiğinde; `withHud` ile HUD'dan sonra) bir dokuya kopyalar. Yol (`"pantalla:<name>"`), `setTexture` için, bir materyal için ya da bir rutinin "Bind texture" adımı için kullanılır. |
| `captureUV(name) → u0, v0, u1, v1` | Yakalanan dokunun, görüntüyü doğru yönde veren kısmı, `setUVRect` için (kopya henüz yapılmadıysa `nil`: kare çizilirken yapılır). |
| `safeArea() → x, y, w, h` | Ekranın çentiksiz kısmı (`start` içinde kullanma). |
| `getScale() → f` | Argümansız: boyutları ekrana uyarlamak için çarpan (kısa kenar / 480). |
| `getPosPx(obj) → x, y`, `setPosPx(obj, x, y)` | Bir 2D öğenin konumu (`nil` o ekseni olduğu gibi bırakır). |
| `getSizePx(obj) → w, h`, `setSizePx(obj, w, h)` | Bir 2D öğenin boyutu. |
| `uiBox(obj) → x, y, w, h` | Gerçekte kapladığı dikdörtgen (x, y = merkez). |
| `setText(obj, value)`, `setSize(text, px)` | Bir metnin ya da düğmenin yazısını ve yazı boyutunu değiştirir. |
| `setTexture(img, "ruta.png")`, `setUVRect`, `setFlipbook` | Görüntüyü değiştirir, bir kısmını kırpar (u1 < u0 ya da v1 < v0 ile aynalanmış) ya da bir kare sayfasını canlandırır. |
| `setOpacity(obj, 0..1)`, `fade(obj, objetivo, paso)` | Saydamlık ve yumuşak geçişler. |
| `setRot2D(obj, degrees)` | Bir 2D öğeyi döndürür. |
| `collide(a, b) → bool` | İki 2D öğenin ekranda üst üste binip binmediği. |
| `limit(obj, area [, axis])`, `inside(area, x [, y])` | Bir öğeyi başka bir öğenin içinde tutma ya da bir noktanın içeride olup olmadığını sorma. |
| `quit()` | Oyunu kapatır (editörde hiçbir şey yapmaz). |
