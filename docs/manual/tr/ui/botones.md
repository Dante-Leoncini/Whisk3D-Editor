# Düğmeler, sekmeler ve onay kutuları

## Düğme

`Button` (`widgets/Button.h`), metni ve solda isteğe bağlı bir simgesi olan bir karttır. Onu düğme gibi gösteren koyu kenarlığıdır. Farenin altındayken (`hover`) kenarlık beyaza döner ve arka plan biraz açılır.

```cpp
Button* guardar = new Button("Guardar", IconType::guardar);   // genişlik metni izler
guardar->Resize(200);                                         // asla bundan geniş olmaz

gfx::PushMatrix();
gfx::Translatef(x, y, 0);
guardar->Render();
gfx::PopMatrix();

guardar->sx = x; guardar->sy = y;      // fare için, nereye yerleştiği
guardar->hover = guardar->Contains(mx, my);
```

`Render` o anki başlangıç noktasına çizer ve ekranda nereye yerleştiğini bilmez: `sx` ve `sy` değerlerini onu çizen taraf ayarlar ve `Contains` bunlarla karşılaştırır. Yapıcının üçüncü argümanı `adapt`'dır: `false` olduğunda düğme, `Resize`'ın verdiği genişliğin tamamını kaplar.

| Alan | Ne yapar |
|---|---|
| `centered` | Simge ve metin ortada, Kabul et ya da İptal'deki gibi |
| `square` | Genişlik yüksekliğe eşit: yalnızca simgeli düğmeler |
| `rowHeight` | Bir çubuğunki yerine bir özellik satırının yüksekliği |
| `caretMenu` | Sağda küçük bir ok: bir menü açtığını haber verir |
| `tint` | `gray` yerine arka plan rengi |
| `textColor`, `borderColor` | Metnin ve simgenin rengi, bir de kenarlığın rengi |
| `focusMenu` | Klavyeyle odaklanmış: kenarlık ve metin `accent` renginde |
| `alpha` | Arkadakinin görünmesi için arka planın ve kenarlığın opaklığı |
| `iconFlip` | Aynalanmış simge: 1 yatay, 2 dikey |
| `bar` | 0 ile 1 arasında bir sayı: düğme oraya kadar `accentDark` ile dolar, metin ortada |
| `fill` | İçinde o renkte bir kare: bir renk seçmek için |
| `editField` | Bir `TextField`: düğme, yerinde yeniden adlandırmak için metin kutusu olarak çizilir |
| `visible` | `false` olduğunda çizilmez |

Düğmenin sakladığı ama çizmediği alanlar da vardır: bunları onu içeren taraf kullanır. `dropdown` bir menü açtığını söyler. `separator` bir çubukta öncesine bir çizgi ister. `role` onu bir çubukta sıraya bağlı kalmadan bulmaya yarar. `help`, fare üzerinde bekleyince çıkan yardım balonunun metnidir; balon UI'a ait değildir.

Yükseklik `UIButtonHeight()`'dır ([Renkler ve ölçek](tema.md)), `rowHeight` ile ise `RowHeightGS + 2 * GlobalScale`. Asla bir satırdan dar olmaz.

## Sekme

`Tab` (`widgets/Tab.h`), alt kenarlığı olmayan bir düğmedir: altındakiyle kaynaşır. Etkin olan (`active`) biraz daha yüksektir ve içerikle aynı renktedir; diğerleri daha koyudur. Metin, klavye odağına sahipse (`focus`) `accent` renginde, etkinse ya da farenin altındaysa beyaz, değilse gridir.

Düğme gibi kullanılır: `Resize(maxW)`, `Render()` ve `Contains(mx, my)`; `sx` ve `sy` değerlerini onu çizen taraf ayarlar.

## Onay kutuları ve renk örnekleri

Bir onay kutusunun nasıl görüneceğine tek bir yer karar verir, `widgets/card.h`; böylece bir menüdeki ile bir karttaki kutu farklı olamaz.

| Fonksiyon | Ne yapar |
|---|---|
| `DrawCheckbox(side, on)` | Kutuyu ve, açıksa, onay işaretini çizer |
| `DrawSampleColor(side, rgba)` | Onay kutusunun köşeleriyle o renkte bir kare çizer |
| `CheckboxSide()` | Kenar uzunluğu: bir satırın yüksekliği |
| `CheckboxBoxColor(on)`, `CheckboxCheckColor()` | Kutunun ve onay işaretinin renkleri |
| `CheckboxCheck()` | Onay işaretinin karakteri, `✔` |

Açıkken kutu `grayUI`, onay işareti ise arka plan rengindedir. Kapalıyken kutu arka plan rengindedir ve onay işareti yoktur.

```cpp
DrawCheckbox(CheckboxSide(), onSound);
```
