# 2D arayüz ve ekran

| Fonksiyon | Ne yapar |
|---|---|
| `pantalla() → ancho, alto` | Tuvalin boyutu. |
| `margenPantalla() → mx, my` | Sabit boyutlu bir tuvalin etrafında, her yanda gerçek kenarlara kadar ne kadar ekran kaldığı, tuval birimleriyle (kadrajın şeritleri de ekrandır). Dikeyde 4:3 bir tuvalle: bir göstergeyi `my` kadar yukarı kaydırmak onu üst kenara yapıştırır. |
| `marcoPantalla() → x, y, ancho, alto` | 3D görüntünün çerçevesi (kameranın kadrajı, şeritler olmadan), tuval birimleriyle, merkezinden itibaren. |
| `capturarPantalla(nombre [, conHud]) → ruta` | Bu karenin 3D görüntüsünü (sahne bittiğinde; `conHud` ile HUD'dan sonra) bir dokuya kopyalar. Yol (`"pantalla:<nombre>"`), `setTextura` için, bir materyal için ya da bir rutinin "Bind texture" adımı için kullanılır. |
| `capturaUV(nombre) → u0, v0, u1, v1` | Yakalanan dokunun, görüntüyü doğru yönde veren kısmı, `setUVRect` için (kopya henüz yapılmadıysa `nil`: kare çizilirken yapılır). |
| `areaSegura() → x, y, w, h` | Ekranın çentiksiz kısmı (`inicio` içinde kullanma). |
| `escala() → f` | Argümansız: boyutları ekrana uyarlamak için çarpan (kısa kenar / 480). |
| `posPx(obj) → x, y`, `setPosPx(obj, x, y)` | Bir 2D öğenin konumu (`nil` o ekseni olduğu gibi bırakır). |
| `tamPx(obj) → w, h`, `setTamPx(obj, w, h)` | Bir 2D öğenin boyutu. |
| `cajaUI(obj) → x, y, w, h` | Gerçekte kapladığı dikdörtgen (x, y = merkez). |
| `setTexto(obj, valor)`, `setTam(texto, px)` | Bir metnin ya da düğmenin yazısını ve yazı boyutunu değiştirir. |
| `setTextura(img, "ruta.png")`, `setUVRect`, `setFlipbook` | Görüntüyü değiştirir, bir kısmını kırpar (u1 < u0 ya da v1 < v0 ile aynalanmış) ya da bir kare sayfasını canlandırır. |
| `setOpacidad(obj, 0..1)`, `fundir(obj, objetivo, paso)` | Saydamlık ve yumuşak geçişler. |
| `setRot2D(obj, grados)` | Bir 2D öğeyi döndürür. |
| `chocan(a, b) → bool` | İki 2D öğenin ekranda üst üste binip binmediği. |
| `limitar(obj, area [, eje])`, `dentro(area, x [, y])` | Bir öğeyi başka bir öğenin içinde tutma ya da bir noktanın içeride olup olmadığını sorma. |
| `salir()` | Oyunu kapatır (editörde hiçbir şey yapmaz). |
