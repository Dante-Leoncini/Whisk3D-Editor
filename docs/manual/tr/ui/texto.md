# Metin

## Bitmap yazı tipi

Arayüzün yazısı `font.png` içinde elle çizilmiştir. Her karakter **5x11** piksellik bir hücredir ve **6** ilerler (`CharacterWidth`). Bir satır **12** ölçer (`RenglonHeight`). Eş aralıklıdır (monospace): tüm karakterler aynı yeri kaplar. Ölçekle birlikte her şey `GlobalScale` ile çarpılır.

`Font` (`text/font.h`) her karakterin koordinatlarını saklar. Tek bir tane vardır, globaldir: `WhiskFont`. Atlasın boyutu ve dokusuyla oluşturulur ([UI'ı doğrudan kullanma](empezar.md)).

`font.png` harfleri, rakamları ve ASCII'nin neredeyse tüm işaretlerini içerir; ayrıca `ñ`, `Ñ`, `¡`, `°` ve onay işareti `✔`. İkisi, ters çevrilmiş başka bir karakterden çıkar: `\`, aynalanmış `/`'dir ve `¿`, döndürülmüş ve biraz daha aşağıya konmuş `?`'dir.

## Aksanlar

Aksanlı harfler çizilmiş değildir: temel harf ve üstündeki işaretle kurulur.

- **Akut** ve **tilde**, `font.png`'den gelir: akut virgülden kesilmiş bir parçadır, tilde ise `ñ`'ninkidir. 2 piksellik bir şerittirler; büyük harfin üstünde daha yukarıda dururlar.
- Diğer işaretler (grav, şapka, iki nokta, nokta, breve, çengel ve ogonek) genişletilmiş atlastadır.

## Genişletilmiş atlas

128x64 boyutundaki `font-ext.png`, `font.png`'ye sığmayanları içerir:

- aksan işaretleri,
- `ß`, `ı`, `ł`, `Ł`, `œ`, `Œ`, `æ` ve `Æ`,
- `;`, `^`, `~`, `` ` ``, `±`, `«`, `»`, `–`, `—` ve `…`,
- **Kiril** alfabesi. Latin bir harfle aynı olan harfler (`А В Е К М Н О Р С Т Х` ve `а е о р с у х`) `font.png`'dekini kullanır.

Ancak bir metin ona ilk kez ihtiyaç duyduğunda yüklenir: `SetAtlasExtendido` yalnızca yolu kaydeder. İngilizcede ya da İspanyolcada hiç yüklenmeyebilir ve N95'te bellek değerlidir. Dosya yoksa, bileşik bir harf işaretsiz çıkar ve genişletilmiş atlastaki bir karakter `?` olarak çıkar.

Her karakterin hangi hücrede olduğunu ve her harfin nasıl kurulduğunu gösteren tablolar `text/FuenteExt.h` içindedir. Elle düzenlenmezler: onları, PNG'yi de çizen ve editörün deposunda bulunan `tools/fuente_ext.py` üretir.

İki dokuyla İspanyolca, Portekizce, Fransızca, Almanca, İtalyanca, Lehçe, Türkçe ve Rusça sığar. Japonca, Çince ve Hintçe sığmaz: hiçbir atlasta olmayan karakter `?` olarak çıkar. `WhiskFont->Soporta(cp)` bir karakterin çizilip çizilemeyeceğini söyler.

Metin UTF-8'dir. `UTF8_Char` 1 ile 3 bayt arasındaki dizileri okur. Emoji gibi 4 baytlık bir dizi okunmaz: soru işaretleri olarak çıkar.

## Bir metin çizmek

```cpp
SetColorID(ColorID::blanco);
RenderBitmapText("Hola", textAlign::left, 200);   // 200: piksel cinsinden azami genişlik
```

O anki matrisin başlangıç noktasına, o anki renkle çizilir. Her metin, atlas başına bir çizim çağrısıdır: bir, ya da genişletilmiş atlası kullanıyorsa iki. Bitince `font.png` dokusunu yeniden bağlı bırakır, böylece ardından gelen simgeler onu bulur.

## Ölçmek ve kesmek

Bir metnin genişliği, harf sayısı çarpı `CharacterWidthGS`'dir. `UTF8_Largo(texto)` baytları değil harfleri sayar: bir `ñ` iki bayt kaplar ve bir sayılır.

Azami genişliğe sığmazsa `RenderBitmapText` onu keser:

| Hizalama | Nerede durur | Ne kesilir |
|---|---|---|
| `textAlign::left` | Başlangıç noktasında başlar | Sonu |
| `textAlign::right` | Başlangıç noktasında biter | Başı: bir alanda en son yazılan görünür |
| `textAlign::center` | Azami genişlikte ortalanmış | Sonu |

`RenderBitmapEtiqueta(texto, ancho)` bir satırın etiketi içindir: sağa hizalanır, ama sığmazsa sonunu keser ve `..` ekler ("Vectores de movimi..").

`RenderBitmapFloat(valor, alineación, ancho, unidad)` bir sayıyı dört ondalık basamakla ve bitişik birimiyle çizer: `90.0000°`. `%f` kullanmaz, çünkü Symbian'ın `printf`'i `float` biçimlendirmez.

## TTF yazı tipleriyle metin

Serbest boyutlu harfler için `w3dui::W3dTextAtlas` (`text/W3dTextAtlas.h`) vardır. Yalnızca bir header'dır. Bir atlasa önceden pişirilmiş (bake edilmiş) bir TTF yazı tipini çizer ve her harf kendi başına ölçeklenir ve yerleştirilir: başlıklar, puanlar ve hareketli harfler için işe yarar. TTF'yi çalışırken rasterleştirmez.

Atlas PC'de, Pillow gerektiren `text/bake_atlas.py` ile pişirilir: `bake_atlas.py <fuente.ttf> <lado del atlas> <alto de letra> <salida>`. Atlasın kenarı 128, 256, 512 ya da 1024'tür. ASCII'yi ve `áéíóúÁÉÍÓÚñÑüÜ¿¡` karakterlerini pişirir. Sonuçta kaplamanın gri tonlarla ve alfasız olduğu `<salida>.png` ve her harfin ölçülerini içeren `<salida>.w3dfnt` çıkar.

```cpp
#include "WhiskUI/text/W3dTextAtlas.h"
#include "io/w3dFilesystem.h"

std::vector<unsigned char> fnt, png;
w3dFileSystem::ReadFileBytes("fuentes/titulo.w3dfnt", fnt);
w3dFileSystem::ReadFileBytes("fuentes/titulo.png", png);

w3dui::W3dTextAtlas titulo;
titulo.Load(&fnt[0], fnt.size(), &png[0], png.size());

titulo.Begin();                                                // atlas, karıştırma ve durum
titulo.DrawText("Puntaje: 120", 10, 10, 24, 1, 1, 1, 1);      // x, y, yükseklik, r, g, b, a
float ancho = titulo.TextWidth("Puntaje: 120", 24);
```

`(x, y)` satırın üst kısmıdır; Y aşağı doğrudur. `DrawText` tüm harfleri tek bir çizimde toplar. `DrawGlyph` tek bir harf çizer; farklı boyut ya da renkteki harfler için. `LineHeight(alto)` bir satırın yüksekliğini verir. Varsayılan olarak önceden çarpılmış alfayla (`MezclaPremult`) karıştırır: metin herhangi bir renkte olabilir.

## GPU'suz tablo

`text/W3dFont.h`, bitmap yazı tipinin aynı tablosunu GPU'ya dokunmadan içerir: her harfin `font.png` içinde nerede olduğu (`W3dFontGetGlyphPx`) ve koordinatları (`W3dFontGetGlyph`). Aynı yazıyla başka bir atlas kurmak için işe yarar.
