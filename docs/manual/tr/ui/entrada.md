# Girdi ve metin kutuları

## UI olayları okumaz

Whisk3D UI pencere açmaz, fareyi, klavyeyi ya da dokunmatik ekranı dinlemez. Olayları program alır, neredeyse tüm sistemlerde SDL2 ile (Core'un [Girdi](../core/entrada.md) sayfası), ve her widget'a ona düşeni iletir:

| Olay | Kime gider |
|---|---|
| Fare ya da parmak hareket eder | `PopupMenu::MouseMove(x, y)`; düğmelerin ve sekmelerin `hover`'ı; kartlarda `PropHoverGroup` ve `PropHoverRow` |
| Tıklama ya da dokunuş | `PopupMenu::Click(x, y)`; `Button::Contains` ve `Tab::Contains`; bir kartta, programın hesapladığı satır |
| Tekerlek | `PopupMenu::Wheel(dir)` |
| Oklar | Menülerin, kartların ve satırların `button_up`, `button_down`, `button_left` ve `button_right` fonksiyonları |
| OK ya da Enter | `PopupMenu::Enter()`, `GroupPropertie::EnterPropertieSelect()` |
| İptal ya da Esc | `PopupMenu::Close()`, `GroupPropertie::Cancel()` |
| Bir harf | Odaklanmış metin kutusu |

Koordinatlar ekran pikselleridir; Y aşağı doğrudur.

## Faresiz

Her şey dört ok, OK ve İptal ile kullanılabilir: N95'te böyle kullanılır. UI'ın bunun için sağladıkları:

- Kartlar satır satır dolaşılır ve seçilemeyenler atlanır.
- Menüler oklarla dolaşılır ve vurgulanan seçenek asla görünümün dışına çıkmaz.
- Sayılar yazmadan, oklarla değiştirilir.
- Klavye odağı görünür: düğmelerde `focusMenu` ve sekmelerde `focus`, `accent` renginde, bir de eşkenar dörtgenin üzerinde `g_kfFocusProp`'un beyaz kenarlığı.

Telefonun hangi tuşunun hangisi olduğu, yazılım tuşları (soft key) ya da OK, UI'a ait değildir: bunu editör ve her sistem çevirir.

## Dokunmatik ekran

- Bir dokunuş bir tıklamadır.
- Bir alt menüyü açan dokunuş onun içinde hiçbir şey seçmez ([Açılır menüler](menus.md)).
- `UITouch` düğmeleri ve çubukları parmak için büyütür ([Renkler ve ölçek](tema.md)). Bugün hiçbir şey onu açmaz.

Kaydırmak için sürüklemeyi, basılı tutmayı ya da iki kez dokunmayı UI'ı kullanan taraf çözer.

## Metin kutuları

`TextField` (`widgets/TextField.h`), bir metin kutusunun durumudur: metin (`text`), imlecin konumu (`caret`) ve her şeyin seçili olup olmadığı (`selectAll`). Çizmez ve tuş okumaz: yalnızca veridir.

| Fonksiyon | Ne yapar |
|---|---|
| `SetText(t)` | Metni ayarlar, imleç sonda |
| `SelectAll()` | Her şeyi seçer: ilk tuş onun yerini alır |
| `InsertChar(c)` | İmlecin olduğu yere bir bayt yazar |
| `Backspace()`, `DelForward()` | Geriye ya da ileriye doğru siler |
| `CaretLeft()`, `CaretRight()` | İmleci hareket ettirir. Her şey seçiliyken seçimi kaldırır |

Silmek ve imleci hareket ettirmek, `ñ` ya da bir Kiril harfi gibi birkaç baytlık bir harfi tek bir harf olarak ele alır. Bunlardan birini yazmak için her bayt `InsertChar`'a verilir.

Aynı anda yalnızca tek bir kutu odaklanmış olur: `g_activeTextField` ya da hiçbiri yoksa `NULL`. Tuşlar ona gider.

```cpp
TextField name;
name.SetText("Jugador 1");
name.SelectAll();
g_activeTextField = &name;

// her harfte
if (g_activeTextField) g_activeTextField->InsertChar(c);

// çizerken, metin rengi ayarlıyken
RenderTextField(name, g_activeTextField == &name, textAlign::left, 200);
```

`RenderTextField(box, focus, alineación, width)` onu o anki başlangıç noktasına çizer:

- Odak yokken metnin kendisidir.
- Odak varken ve her şey seçiliyken metin `accentDark` üzerinde beyaz renkte olur.
- Odak varken ve seçim yokken imleç görünür: her 530 ms'de bir yanıp sönen küçük bir çizgi.

`TextFieldBlinkTick()` çizgi değiştiğinde `true` döndürür: yeniden çizme zamanı gelmiştir.

`TextField.h` ayrıca `TextFieldFocus`, `TextFieldCancel` (Esc: odaklanmadan önceki metne döner), `TextFieldInputChar` ve `TextFieldFrameEnd` fonksiyonlarını da bildirir. Bugün bunlar editörde, `main/ui/ViewPorts/NumInput.cpp` içinde tanımlıdır: UI tek başına bunları içermez.
