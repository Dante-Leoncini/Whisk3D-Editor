# Açılır menüler

`PopupMenu` (`widgets/PopupMenu.h`), seçeneklerden oluşan yüzen bir listedir: bir çubuğun menüsü, bir bağlam menüsü, bir seçici. Her seçeneğin (`MenuItem`) bir metni vardır ve şunlara sahip olabilir:

- solda bir **simge**,
- sağda bir okla bir **alt menü**,
- bir `bool` değerini gösteren ve değiştiren bir **onay kutusu**,
- iki değer arasında bir `float` düzenleyen bir **kaydırıcı**,
- sağda ve soluk griyle bir **kısayol** ("Shift D"),
- `verde`: etkin seçenektir, simgesi ve metni `accent` renginde,
- `gris`: bir `bool`'a işaret eder; o `false` olduğu sürece seçenek yarı opak görünür ve yanıt vermez,
- `lineaArriba`: onu üstteki gruptan ayıran bir çizgi.

Menünün bir **başlığı** (`titulo`) olabilir: beyaz renkte, altında bir çizgiyle.

## Bir tabloyla kurmak

Önerilen yol bir `MenuDef` tablosudur: her seçeneğin metni ve ne yaptığı aynı satırda durur. Başka bir yerdeki bir `switch`'e bağlanacak bir numara yoktur.

```cpp
static bool grilla = true;
static PopupMenu* recientes = NULL;
static void Nuevo() { /* ... */ }
static void Abrir() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // metin     eylem   kısayol   simge                    alt menü    onay kutusu
    { "Nuevo",   Nuevo,  "Ctrl N", ICONO(IconType::mas) },
    { "Abrir",   Abrir,  "Ctrl O", ICONO(IconType::carpeta) },
    { "Recientes", NULL, NULL,     ICONO(IconType::lista),  &recientes },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grilla },
};

recientes = new PopupMenu();          // alt menü, kurmadan önce var olmalı
PopupMenu* archivo = new PopupMenu();
archivo->titulo = "Archivo";
archivo->Construir(ARCHIVO, 4);
```

`ICONO(x)` simgeye 1 ekler: bir tabloda 0 "simge yok" anlamına gelir. Her seçeneğin id'si kendi satırıdır.

Programın kurduğu iki isteğe bağlı kanca:

```cpp
W3dMenuTraducir = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuAtajoTexto = MiAtajo;      // std::string MiAtajo(const std::string& atajo)
```

`W3dMenuTraducir` ile tablonun metinleri, menü kurulurken çevrilmiş olarak çıkar. `W3dMenuAtajoTexto` ile bir kısayol `@` ile başlayan bir ad olabilir ve kanca karşılık gelen tuşu döndürür. Bu kanca olmadan `@` içeren bir kısayol gösterilmez.

## Elle kurmak

```cpp
PopupMenu* vista = new PopupMenu();
vista->Agregar("Frente", 0, IconType::camera);
vista->AgregarCheck("Mostrar piso", 1, &verPiso);
vista->AgregarFloat("Tamaño", 2, &tamanio, 0.0f, 4.0f);
MenuItem* it = vista->Agregar("Alambre", 3);
it->verde = esAlambre;
it->lineaArriba = true;
vista->action = AlElegir;              // void AlElegir(int id)
```

`Agregar`, geri kalanını değiştirebilmen için `MenuItem*` döndürür. Bir seçenek seçildiğinde `Ejecutar(id)`, seçeneğin bir `accion`'ı varsa onu, yoksa menünün `action(id)`'sini çağırır. `Limpiar()` tüm seçenekleri siler; değişen menüler için.

## Açmak

```cpp
archivo->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
MenuAbierto = archivo;
```

Ona verdiğin noktada açılır: imleçte ya da bir düğmenin kenarında. Sığmazsa ekranın içine doğru kayar. `Abrir`, `MenuAbierto`'ya dokunmaz: onu açan taraf ayarlar. `Cerrar()` onu yeniden `NULL` yapar.

Açılırken ölçülür:

- Genişlik, metni ve sağ sütunuyla (kısayol, ok, onay kutusu ya da kaydırıcı) en uzun satırın genişliğidir. Bir satırda simge varsa hepsi ona yer bırakır, böylece metinler hizalı kalır.
- Asla ekrandan geniş olmaz. Sığmazsa her satırın metni, sağ sütunundan önce kesilir: onay kutuları görünmeye devam eder.
- Asla ekrandan yüksek olmaz. Daha fazla satırı varsa, sığanları sağda bir kaydırma çubuğuyla gösterir. Tekerlekle (`Wheel`) kaydırılır.

Bir alt menü kendi satırının yanında açılır: sığıyorsa sağda, sığmıyorsa solda. Hiçbir tarafa sığmazsa daralır ve arkadaki menünün bir sütununu görünür bırakır; böylece fare başka bir seçeneğe geçirilebilir.

## Fare ve parmak

```cpp
// hareket ettirirken
if (MenuAbierto) MenuAbierto->MouseMove(mx, my);

// tıklarken ya da dokunurken
if (MenuAbierto) {
    PopupMenu* m = MenuAbierto;
    int id = m->Click(mx, my);
    if (id >= 0) m->Ejecutar(id);
}

// çizerken, tam ekran bir projeksiyonla
if (MenuAbierto) MenuAbierto->Render();
```

`Click` seçilen id'yi, hiçbir şey seçilmediyse `-1`'i ya da bir alt menü açıldıysa `-2`'yi döndürür.

- Fareyi üzerinden geçirmek satırı vurgular ve tıklamadan alt menüsünü açar.
- Fare `60 * GlobalScale` pikselden fazla uzaklaşırsa menü kendiliğinden kapanır. `cerrarLejos = false` ile kapanmaz: farede açılmamış bir menü için.
- Dışarıya bir tıklama onu kapatır.
- Sıradan bir seçenek menüyü kapatır. Bir onay kutusu değişir ve menüyü açık bırakır; böylece birkaçı açılabilir. Bir kaydırıcıya tıklama, değeri tıklamanın düştüğü yere göre ayarlar.
- Bir kaydırıcının üzerinde basılı tutup sürüklemek değeri canlı olarak değiştirir. Fareyi yöneten taraf, `MenuSliderDragActivo()` `true` olduğu sürece her hareketi `MenuSliderDragMover(mx)`'e iletir ve bırakınca `MenuSliderDragSoltar()`'ı çağırır.
- Dokunmatik ekranda, bir alt menüyü açan dokunuş, alt menü parmağın altına gelmiş olsa bile onun içinde hiçbir şey seçmez. İkinci bir dokunuş gerekir.
- Bir alt menüde seçilen, o alt menüde çalıştırılır: her menünün kendi id'leri vardır.

## Klavye

Odak, en içteki açık alt menüdedir:

```cpp
PopupMenu* m = MenuAbierto;
while (m->submenuAbierto && m->submenuAbierto->abierto) m = m->submenuAbierto;
MenuItem* it = m->ItemActual();      // vurgulanan seçenek ya da NULL
```

| Fonksiyon | Ne yapar |
|---|---|
| `button_up()`, `button_down()` | Vurguyu, sona gelince başa dönerek hareket ettirir. Alt menü açmaz |
| `AbrirSubmenuActual()` | Vurgulanan seçeneğin alt menüsünü açar ve onun ilk seçeneğini vurgular |
| `AjustarSlider(+1)`, `AjustarSlider(-1)` | Kaydırıcıyı aralığının yirmide biri kadar hareket ettirir |
| `Enter()` | Vurgulanan seçeneğe bir tıklama gibidir ve aynı şeyi döndürür. `MenuAbierto` üzerinde çağrıldığında kendiliğinden en içteki alt menüye ulaşır |
| `Cerrar()` | Menüyü ve alt menülerini kapatır |

Klavyeyle, vurgulanan seçenek her zaman görünür kalır. Hangi tuşun neyi çağıracağına menüyü kullanan taraf karar verir.

`MenuPantallaW` ve `MenuPantallaH` ekranın boyutunu taşımalıdır: menü onlarla yerleştirilir.
