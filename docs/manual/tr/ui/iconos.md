# Simgeler

## Dosyalar

Her simge, editörün deposunda `res/Skins/Whisk3D/atlas/iconos/` içinde **10x10** boyutunda bir PNG'dir. Bugün 106 tane var. Pikseller saydam zemin üzerinde beyazdır: simge, çizildiği rengi alır. Böylece aynı simge gri, beyaz ya da vurgu renginde kullanılabilir.

## Enum ve adlar

`draw/icons.h`, `IconType` enum'unu ve simge sayısını, `ICON_TOTAL` (106) değerini içerir. `draw/icons.cpp` içinde `KNombres`, aynı sırayla her birinin adını verir: bu, dosyanın `.png` olmadan adıdır.

```cpp
IconoNombre(IconType::carpeta);   // "carpeta"
IconoIndice("carpeta");           // IconType::carpeta, yoksa -1
```

Neredeyse hepsi enum'dakiyle aynı adı taşır. İstisna `candadoAbierto`'dur; onun dosyası `candado_abierto.png`'dir.

Birçoğu editöre aittir: modelleme araçları, nesne türleri, animasyon eğrileri. Genel olanlar da vardır: `arrow` ve `arrowRight` (katlama okları), `visible` ve `hidden`, `carpeta`, `guardar`, `borrar`, `mas` ve `menos`, `lista` ve `cuadricula`, `candado`, `notifOk` ve `notifError`, `tijera`, `copiar` ve `pegar`.

## Koordinatlar

`IconsUV`, her simgenin atlastaki koordinatlarını (`uvs`, 8 sayı) tutan bir `std::vector<IconUV*>`'dır. İki şekilde doldurulur:

- `CrearIconos(ancho, alto)`: her simgenin `font.png` içinde nerede olduğunu gösteren tabloyla (`TablaLegacy`, `icons.cpp` içinde). `atlas/iconos/` klasörü olmadan kullanılan yol budur.
- `CrearIconosDesde(rects, ancho, alto)`: kendi dikdörtgen listesiyle, yani kurulan atlasınkiyle. `x = -1` olan bir dikdörtgen `font.png`'dekini kullanır: PNG'si olmayan bir simge görünmeye devam eder.

`SetIconScale(escala)`, `IconSizeGS` değerini ve iki mesh'i ayarlar: bir simgenin karesi olan `IconMesh` ve ağaçların çizgileri için bir satır yüksekliğinde bir kare olan `IconLineMesh`.

## Bir simge çizmek

Atlas ayarlıyken, o anki başlangıç noktasında:

```cpp
SetColorID(ColorID::grisUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::carpeta]->uvs);
```

`Button`, `Tab`, `PopupMenu` ve `GroupPropertie` simgeyi enum'dan bir `int` olarak, hiçbiri için ise `-1` olarak alır.

## Simge eklemek

1. 10x10 boyutundaki PNG, `atlas/iconos/` içine.
2. `IconType` enum'unun sonuna bir satır ve `ICON_TOTAL` değerine bir ekleme.
3. Adı, `KNombres`'in sonuna.
4. `TablaLegacy`'nin sonuna, PNG eksikse onun yerini alacak `font.png` simgesiyle bir satır.

4. satır olmadan, kurulmuş bir atlas olmadığında simgenin koordinatları kalmaz.
