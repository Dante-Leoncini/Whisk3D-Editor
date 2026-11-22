# Açılır menüler

`PopupMenu` (`widgets/PopupMenu.h`), seçeneklerden oluşan yüzen bir listedir: bir çubuğun menüsü, bir bağlam menüsü, bir seçici. Her seçeneğin (`MenuItem`) bir metni vardır ve şunlara sahip olabilir:

- solda bir **simge**,
- sağda bir okla bir **alt menü**,
- bir `bool` değerini gösteren ve değiştiren bir **onay kutusu**,
- iki değer arasında bir `float` düzenleyen bir **kaydırıcı**,
- sağda ve soluk griyle bir **kısayol** ("Shift D"),
- `green`: etkin seçenektir, simgesi ve metni `accent` renginde,
- `gray`: bir `bool`'a işaret eder; o `false` olduğu sürece seçenek yarı opak görünür ve yanıt vermez,
- `lineUp`: onu üstteki gruptan ayıran bir çizgi.

Menünün bir **başlığı** (`title`) olabilir: beyaz renkte, altında bir çizgiyle.

## Bir tabloyla kurmak

Önerilen yol bir `MenuDef` tablosudur: her seçeneğin metni ve ne yaptığı aynı satırda durur. Başka bir yerdeki bir `switch`'e bağlanacak bir numara yoktur.

```cpp
static bool grid = true;
static PopupMenu* recent = NULL;
static void New() { /* ... */ }
static void Open() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // metin     eylem   kısayol   simge                    alt menü    onay kutusu
    { "Nuevo",   New,  "Ctrl N", ICON(IconType::more) },
    { "Abrir",   Open,  "Ctrl O", ICON(IconType::folder) },
    { "Recientes", NULL, NULL,     ICON(IconType::list),  &recent },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grid },
};

recent = new PopupMenu();          // alt menü, kurmadan önce var olmalı
PopupMenu* archivo = new PopupMenu();
archivo->title = "Archivo";
archivo->Build(ARCHIVO, 4);
```

`ICON(x)` simgeye 1 ekler: bir tabloda 0 "simge yok" anlamına gelir. Her seçeneğin id'si kendi satırıdır.

Programın kurduğu iki isteğe bağlı kanca:

```cpp
W3dMenuTranslate = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuShortcutText = MiShortcut;      // std::string MiAtajo(const std::string& atajo)
```

`W3dMenuTranslate` ile tablonun metinleri, menü kurulurken çevrilmiş olarak çıkar. `W3dMenuShortcutText` ile bir kısayol `@` ile başlayan bir ad olabilir ve kanca karşılık gelen tuşu döndürür. Bu kanca olmadan `@` içeren bir kısayol gösterilmez.

## Elle kurmak

```cpp
PopupMenu* view = new PopupMenu();
view->Add("Frente", 0, IconType::camera);
view->AddCheck("Mostrar piso", 1, &showFloor);
view->AddFloat("Tamaño", 2, &size, 0.0f, 4.0f);
MenuItem* it = view->Add("Alambre", 3);
it->green = isWire;
it->lineUp = true;
view->action = OnPick;              // void AlElegir(int id)
```

`Add`, geri kalanını değiştirebilmen için `MenuItem*` döndürür. Bir seçenek seçildiğinde `Run(id)`, seçeneğin bir `action`'ı varsa onu, yoksa menünün `action(id)`'sini çağırır. `Clear()` tüm seçenekleri siler; değişen menüler için.

## Açmak

```cpp
archivo->Open(mx, my, MenuScreenW, MenuScreenH);
OpenMenu = archivo;
```

Ona verdiğin noktada açılır: imleçte ya da bir düğmenin kenarında. Sığmazsa ekranın içine doğru kayar. `Open`, `OpenMenu`'ya dokunmaz: onu açan taraf ayarlar. `Close()` onu yeniden `NULL` yapar.

Açılırken ölçülür:

- Genişlik, metni ve sağ sütunuyla (kısayol, ok, onay kutusu ya da kaydırıcı) en uzun satırın genişliğidir. Bir satırda simge varsa hepsi ona yer bırakır, böylece metinler hizalı kalır.
- Asla ekrandan geniş olmaz. Sığmazsa her satırın metni, sağ sütunundan önce kesilir: onay kutuları görünmeye devam eder.
- Asla ekrandan yüksek olmaz. Daha fazla satırı varsa, sığanları sağda bir kaydırma çubuğuyla gösterir. Tekerlekle (`Wheel`) kaydırılır.

Bir alt menü kendi satırının yanında açılır: sığıyorsa sağda, sığmıyorsa solda. Hiçbir tarafa sığmazsa daralır ve arkadaki menünün bir sütununu görünür bırakır; böylece fare başka bir seçeneğe geçirilebilir.

## Fare ve parmak

```cpp
// hareket ettirirken
if (OpenMenu) OpenMenu->MouseMove(mx, my);

// tıklarken ya da dokunurken
if (OpenMenu) {
    PopupMenu* m = OpenMenu;
    int id = m->Click(mx, my);
    if (id >= 0) m->Run(id);
}

// çizerken, tam ekran bir projeksiyonla
if (OpenMenu) OpenMenu->Render();
```

`Click` seçilen id'yi, hiçbir şey seçilmediyse `-1`'i ya da bir alt menü açıldıysa `-2`'yi döndürür.

- Fareyi üzerinden geçirmek satırı vurgular ve tıklamadan alt menüsünü açar.
- Fare `60 * GlobalScale` pikselden fazla uzaklaşırsa menü kendiliğinden kapanır. `closeFar = false` ile kapanmaz: farede açılmamış bir menü için.
- Dışarıya bir tıklama onu kapatır.
- Sıradan bir seçenek menüyü kapatır. Bir onay kutusu değişir ve menüyü açık bırakır; böylece birkaçı açılabilir. Bir kaydırıcıya tıklama, değeri tıklamanın düştüğü yere göre ayarlar.
- Bir kaydırıcının üzerinde basılı tutup sürüklemek değeri canlı olarak değiştirir. Fareyi yöneten taraf, `MenuSliderDragActive()` `true` olduğu sürece her hareketi `MenuSliderDragMove(mx)`'e iletir ve bırakınca `MenuSliderDragRelease()`'ı çağırır.
- Dokunmatik ekranda, bir alt menüyü açan dokunuş, alt menü parmağın altına gelmiş olsa bile onun içinde hiçbir şey seçmez. İkinci bir dokunuş gerekir.
- Bir alt menüde seçilen, o alt menüde çalıştırılır: her menünün kendi id'leri vardır.

## Klavye

Odak, en içteki açık alt menüdedir:

```cpp
PopupMenu* m = OpenMenu;
while (m->openSubmenu && m->openSubmenu->open) m = m->openSubmenu;
MenuItem* it = m->CurrentItem();      // vurgulanan seçenek ya da NULL
```

| Fonksiyon | Ne yapar |
|---|---|
| `button_up()`, `button_down()` | Vurguyu, sona gelince başa dönerek hareket ettirir. Alt menü açmaz |
| `OpenSubmenuCurrent()` | Vurgulanan seçeneğin alt menüsünü açar ve onun ilk seçeneğini vurgular |
| `FitSlider(+1)`, `FitSlider(-1)` | Kaydırıcıyı aralığının yirmide biri kadar hareket ettirir |
| `Enter()` | Vurgulanan seçeneğe bir tıklama gibidir ve aynı şeyi döndürür. `OpenMenu` üzerinde çağrıldığında kendiliğinden en içteki alt menüye ulaşır |
| `Close()` | Menüyü ve alt menülerini kapatır |

Klavyeyle, vurgulanan seçenek her zaman görünür kalır. Hangi tuşun neyi çağıracağına menüyü kullanan taraf karar verir.

`MenuScreenW` ve `MenuScreenH` ekranın boyutunu taşımalıdır: menü onlarla yerleştirilir.
