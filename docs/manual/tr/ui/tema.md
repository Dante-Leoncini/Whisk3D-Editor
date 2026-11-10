# Renkler ve ölçek

## Palet

UI'ın renkleri sabit renkler değil, rollerdir: `ColorID` enum'u (`theme/colores.h`).

| Rol | Ne için kullanılır |
|---|---|
| `background` | Arka plan. Kapalı bir onay kutusunun kutusu ve bekleyen bir düğmenin kenarlığı |
| `blanco` | Metin ve farenin altında olanın ya da seçilenin kenarlığı |
| `accent` | Odak: menülerin kenarlığı, düzenlenmekte olan, etkin seçenek, bir kaydırıcının dolgusu |
| `accentDark` | Seçili metnin arka planı ve sayısal bir düğmenin çubuğu |
| `negro` | `PropSeparator` çizgisi |
| `gris` | Kartların, düğmelerin ve menülerin gövdesi |
| `headerColor` | Bir menünün vurgulanan satırı ve bir kaydırıcının yolu |
| `negroTransparente` | %25 siyah. UI onu kullanmaz |
| `grisUI` | Normal metin ve açık bir onay kutusunun kutusu |
| `grisLinea` | Menülerde grupları ayıran çizgiler |

`ListaColores` onları `float` olarak, `ListaColoresUbyte` ise bayt olarak tutar. `SetColorID` birini isteğe bağlı bir alfayla çizim rengi olarak ayarlar:

```cpp
SetColorID(ColorID::accent);         // opak
SetColorID(ColorID::grisUI, 0.5f);   // yarı opaklıkta
```

`loadColorsW3d(ruta)` onları bir metin dosyasından, satır başına bir renk olarak okur: ad ve 0 ile 1 arasında dört bileşen, örneğin `accent 0.0 1.0 0.0 1.0`. Tanımadığı adları yok sayar, böylece aynı `skin.ini` başka bir programın renklerini de taşıyabilir. Dosyayı açamazsa `false` döndürür. Dosya olmazsa koddaki renkler (`theme/colores.cpp`) kalır; Whisk3D skin'i onları değiştirir ve örneğin `accent` yeşile döner.

Bazı renkler kodda yazılıdır ve paletten gelmez: kaydedilmemiş bir değişikliğin eşkenar dörtgeninin yeşili, hata bildiren bir `PropLabel`'ın kırmızısı, eksik bir `PropText`'in kırmızı kenarlığı ve devre dışı bir `PropButton`'ın grisi.

## Ölçek

Temel ölçüler `core/UI.h` içinde, piksel cinsindendir:

| Ölçü | Değer |
|---|---|
| `margin`, `padding` | 10 |
| `border` | 4 |
| `gap` | 5 |
| `RenglonHeight` | 12 |
| `LetterWidth`, `CharacterWidth` | 6 |
| `LetterHeight` | 11 |

`GlobalScale` onları her zaman bir tam sayıyla çarpar: böylece pikselli harfler keskin kalır. Örneğin N95'in 240 piksellik ekranında 1, bir PC'de 3.

Her ölçünün, sonunda `GS` olan, zaten çarpılmış bir sürümü vardır: `RenglonHeightGS`, `gapGS`, `borderGS`, `bordersGS` (kenarlığın iki katı), `CharacterWidthGS`, `LetterHeightGS`... Widget'lar bunları kullanır.

UI, bunları yeniden hesaplayan fonksiyonu içermez: editörünki `main/ui/ViewPorts/ViewPorts.cpp` içindeki `SetGlobalScale`'dir. Tek başına şöyle yapılır:

```cpp
void Escala(int s) {
    GlobalScale = s;
    marginGS = margin * s;  paddingGS = padding * s;  gapGS = gap * s;
    borderGS = border * s;  bordersGS = borderGS * 2;
    RenglonHeightGS = RenglonHeight * s;
    LetterWidthGS = LetterWidth * s;  LetterHeightGS = LetterHeight * s;
    CharacterWidthGS = CharacterWidth * s;
    SetIconScale(s);
    if (WhiskFont) WhiskFont->SetScale(s);
}
```

Her widget'ın boyutları kendi `Resize`'ında hesaplanır: ölçeği değiştirdikten sonra onu her kartta, düğmede ve menüde yeniden çağırmak gerekir.

## Kompakt ya da dokunmatik

`UIBotonAltura()` bir düğmenin yüksekliğini, `UIBarPadding()` ise bir çubuktaki düğmenin üstünde ve altında kalan boşluğu verir. İkisi de `UITactil`'e bağlıdır:

- `false`, N95 için tasarlanmış kompakt mod: düğme bir satır artı kenarlıklar eksi `2 * GlobalScale` ölçer ve çubuk üstte ve altta `GlobalScale` kadar boşluk bırakır.
- `true`: parmak sığsın diye düğmeye ve çubuğa bir `gap` eklenir. Kenarlıklar kalınlaşmaz.

`UITactil` `false` ile başlar ve bugün hiçbir şey onu açmaz.
