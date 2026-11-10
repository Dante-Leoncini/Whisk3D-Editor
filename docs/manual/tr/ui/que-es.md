# Whisk3D UI nedir

**Whisk3D UI**, arayüz kurmak için bir C++ kütüphanesidir: düğmeler, sekmeler, açılır menüler, özellik kartları, metin ve simgeler. Kendi deposu vardır: [Whisk3D-UI](https://github.com/Dante-Leoncini/Whisk3D-UI), MIT lisanslı. [Whisk3D Core](../core/README.md) üzerine kurulmuştur ve Whisk3D editörü onunla yapılmıştır.

![Whisk3D mimarisi: isteğe bağlı Editor ve Whisk3D UI, her sistemle kendi arka ucu üzerinden konuşan Whisk3D Core'un üzerinde](img/whisk3d-capas.webp)

## Nerede çalışır

Core ile aynı sistemlerde: **Windows**, **Linux**, **macOS**, **Android**, **tarayıcı**, **Nokia N95** ve **N8**, **Sega Dreamcast**, **Sony PSP**, **PS Vita** ve **Nintendo 3DS**. Editör onu hepsinde derler.

UI'ın sisteme özel kodu yoktur. OpenGL'i çağırmaz: her şeyi `w3dEngine` fonksiyonlarıyla ([Grafik](../core/graficos.md)) çizer ve gerisini her sistemin arka ucu yapar. Değişen az şey derlemeyle ilgilidir:

- Header'lar türler (`GLfloat`, `GLshort`, `GLubyte`) için `GL/gl.h`'yi, Symbian'da ise `GLES/gl.h`'yi içerir.
- Android'de `loadColorsW3d` bayt cinsinden renk tablosunu günceller ama `float` tablosunu güncellemez.
- Kod C++03'tür, çünkü Symbian'ın derleyicisi C++11'i tanımaz: enum'lar bir `struct`'ın içinde durur (`IconType::camera`) ve `override` yerine `W3D_OVERRIDE` kullanılır.

## Neden vektör değil de atlas

Arayüz kütüphanelerinin neredeyse hepsi vektörlerle çizer. Birçok eski cihaz bunu hızlandırmaz: arayüz yazılımla çizilir ve yavaş çalışır. N95 gibi cihazların hızlandırdığı şey ise 3D'dir: dokulu üçgenler.

Bu yüzden Whisk3D UI her şeyi, bir dokudan, yani bir **atlastan** okuyan üçgenlerle çizer. Harfler, kartların yuvarlak kenarları ve simgeler aynı dokudadır: tüm arayüz için tek bir `BindTexture` yeter. Bir metin, harf başına bir çağrıyla değil, atlas başına bir çağrıyla çizilir.

Bedeli: harfler piksellidir ve tam sayı katlarıyla büyür ([Renkler ve ölçek](tema.md)) ve yalnızca atlasta çizili olan harfler vardır ([Metin](texto.md)).

## İçinde neler var

| Klasör | Neyi çözer |
|---|---|
| `core/` | Arayüzün ölçüleri ve ölçeği (`UI.h`): [Renkler ve ölçek](tema.md) |
| `theme/` | Renk paleti (`colores.h`): [Renkler ve ölçek](tema.md) |
| `draw/` | 2D çizim (`glesdraw`), dikdörtgenler, simgeler ve atlas paketleyici: [Çizim ve atlas](dibujo.md), [Simgeler](iconos.md) |
| `text/` | Bitmap yazı tipi, genişletilmiş atlas ve önceden pişirilmiş TTF yazı tipleriyle metin: [Metin](texto.md) |
| `widgets/` | Kenarlıklı kartlar, düğmeler, sekmeler, menüler ve metin kutuları: [Düğmeler, sekmeler ve onay kutuları](botones.md), [Açılır menüler](menus.md), [Girdi ve metin kutuları](entrada.md) |
| `Propieties/` | Özellik kartları ve satırları: [Kartlar ve özellikler](propiedades.md) |
