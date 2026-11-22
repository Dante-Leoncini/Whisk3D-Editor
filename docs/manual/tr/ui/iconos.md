# Simgeler

## Dosyalar

Her simge, editörün deposunda `res/Skins/Whisk3D/atlas/icons/` içinde **10x10** boyutunda bir PNG'dir. Bugün 106 tane var. Pikseller saydam zemin üzerinde beyazdır: simge, çizildiği rengi alır. Böylece aynı simge gri, beyaz ya da vurgu renginde kullanılabilir.

## Enum ve adlar

`draw/icons.h`, `IconType` enum'unu ve simge sayısını, `ICON_TOTAL` (106) değerini içerir. `draw/icons.cpp` içinde `KNames`, aynı sırayla her birinin adını verir: bu, dosyanın `.png` olmadan adıdır.

```cpp
IconName(IconType::folder);   // "carpeta"
IconIndex("carpeta");           // IconType::carpeta, yoksa -1
```

Neredeyse hepsi enum'dakiyle aynı adı taşır. İstisna `lockOpen`'dur; onun dosyası `candado_abierto.png`'dir.

Birçoğu editöre aittir: modelleme araçları, nesne türleri, animasyon eğrileri. Genel olanlar da vardır: `arrow` ve `arrowRight` (katlama okları), `visible` ve `hidden`, `folder`, `save`, `trash`, `plus` ve `minus`, `list` ve `grid`, `lock`, `notifOk` ve `notifError`, `scissors`, `copy` ve `paste`.

## Koordinatlar

`IconsUV`, her simgenin atlastaki koordinatlarını (`uvs`, 8 sayı) tutan bir `std::vector<IconUV*>`'dır. İki şekilde doldurulur:

- `CreateIcons(width, height)`: her simgenin `font.png` içinde nerede olduğunu gösteren tabloyla (`TableLegacy`, `icons.cpp` içinde). `atlas/icons/` klasörü olmadan kullanılan yol budur.
- `CreateIconsFrom(rects, width, height)`: kendi dikdörtgen listesiyle, yani kurulan atlasınkiyle. `x = -1` olan bir dikdörtgen `font.png`'dekini kullanır: PNG'si olmayan bir simge görünmeye devam eder.

`SetIconScale(scale)`, `IconSizeGS` değerini ve iki mesh'i ayarlar: bir simgenin karesi olan `IconMesh` ve ağaçların çizgileri için bir satır yüksekliğinde bir kare olan `IconLineMesh`.

## Bir simge çizmek

Atlas ayarlıyken, o anki başlangıç noktasında:

```cpp
SetColorID(ColorID::grayUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::folder]->uvs);
```

`Button`, `Tab`, `PopupMenu` ve `GroupPropertie` simgeyi enum'dan bir `int` olarak, hiçbiri için ise `-1` olarak alır.

## Simge eklemek

1. 10x10 boyutundaki PNG, `atlas/icons/` içine.
2. `IconType` enum'unun sonuna bir satır ve `ICON_TOTAL` değerine bir ekleme.
3. Adı, `KNames`'in sonuna.
4. `TableLegacy`'nin sonuna, PNG eksikse onun yerini alacak `font.png` simgesiyle bir satır.

4. satır olmadan, kurulmuş bir atlas olmadığında simgenin koordinatları kalmaz.
