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

`Render` o anki başlangıç noktasına çizer ve ekranda nereye yerleştiğini bilmez: `sx` ve `sy` değerlerini onu çizen taraf ayarlar ve `Contains` bunlarla karşılaştırır. Yapıcının üçüncü argümanı `adaptar`'dır: `false` olduğunda düğme, `Resize`'ın verdiği genişliğin tamamını kaplar.

| Alan | Ne yapar |
|---|---|
| `centrado` | Simge ve metin ortada, Kabul et ya da İptal'deki gibi |
| `cuadrado` | Genişlik yüksekliğe eşit: yalnızca simgeli düğmeler |
| `altoRenglon` | Bir çubuğunki yerine bir özellik satırının yüksekliği |
| `caretMenu` | Sağda küçük bir ok: bir menü açtığını haber verir |
| `tinte` | `gris` yerine arka plan rengi |
| `colorTexto`, `colorBorde` | Metnin ve simgenin rengi, bir de kenarlığın rengi |
| `focoMenu` | Klavyeyle odaklanmış: kenarlık ve metin `accent` renginde |
| `alpha` | Arkadakinin görünmesi için arka planın ve kenarlığın opaklığı |
| `iconFlip` | Aynalanmış simge: 1 yatay, 2 dikey |
| `barra` | 0 ile 1 arasında bir sayı: düğme oraya kadar `accentDark` ile dolar, metin ortada |
| `relleno` | İçinde o renkte bir kare: bir renk seçmek için |
| `editField` | Bir `TextField`: düğme, yerinde yeniden adlandırmak için metin kutusu olarak çizilir |
| `visible` | `false` olduğunda çizilmez |

Düğmenin sakladığı ama çizmediği alanlar da vardır: bunları onu içeren taraf kullanır. `desplegable` bir menü açtığını söyler. `separador` bir çubukta öncesine bir çizgi ister. `rol` onu bir çubukta sıraya bağlı kalmadan bulmaya yarar. `ayuda`, fare üzerinde bekleyince çıkan yardım balonunun metnidir; balon UI'a ait değildir.

Yükseklik `UIBotonAltura()`'dır ([Renkler ve ölçek](tema.md)), `altoRenglon` ile ise `RenglonHeightGS + 2 * GlobalScale`. Asla bir satırdan dar olmaz.

## Sekme

`Tab` (`widgets/Tab.h`), alt kenarlığı olmayan bir düğmedir: altındakiyle kaynaşır. Etkin olan (`activa`) biraz daha yüksektir ve içerikle aynı renktedir; diğerleri daha koyudur. Metin, klavye odağına sahipse (`foco`) `accent` renginde, etkinse ya da farenin altındaysa beyaz, değilse gridir.

Düğme gibi kullanılır: `Resize(maxW)`, `Render()` ve `Contains(mx, my)`; `sx` ve `sy` değerlerini onu çizen taraf ayarlar.

## Onay kutuları ve renk örnekleri

Bir onay kutusunun nasıl görüneceğine tek bir yer karar verir, `widgets/card.h`; böylece bir menüdeki ile bir karttaki kutu farklı olamaz.

| Fonksiyon | Ne yapar |
|---|---|
| `DibujarCheckbox(lado, prendida)` | Kutuyu ve, açıksa, onay işaretini çizer |
| `DibujarMuestraColor(lado, rgba)` | Onay kutusunun köşeleriyle o renkte bir kare çizer |
| `CheckboxLado()` | Kenar uzunluğu: bir satırın yüksekliği |
| `CheckboxColorCaja(prendida)`, `CheckboxColorTilde()` | Kutunun ve onay işaretinin renkleri |
| `CheckboxTilde()` | Onay işaretinin karakteri, `✔` |

Açıkken kutu `grisUI`, onay işareti ise arka plan rengindedir. Kapalıyken kutu arka plan rengindedir ve onay işareti yoktur.

```cpp
DibujarCheckbox(CheckboxLado(), sonidoPrendido);
```
