# Klavye kısayolları

Aşağıdaki tablolardaki tüm işlevler yeniden atanabilir ve her birinin **bir ya da birden fazla** kombinasyonu olabilir (ya da hiç olmayabilir). PC'de, Android'de (fiziksel klavyeyle) ve Web'de çalışır. N95'te pencere tuşları yakalayamaz: orada `atajos.ini` dosyası elle düzenlenir.

## Bir kısayolu değiştirme

1. **Özellikler → Dosya/Render sekmesi → Ayarlar kartı → Klavye Kısayolları…** yolunu aç.
2. **Bağlam**'ı seç (Genel, 3D Görünüm, Dönüşüm sırasında, UV Düzenleyici, Zaman Çizelgesi, Outliner, 2D Düzenleyici, Özellikler, Betik Düzenleyici). Sol/sağ ok tuşları da onu değiştirir.
3. Listeden işlevi seç ve **Kısayol ekle**'ye dokun (ya da seçili satıra tekrar dokun). "Bir tuşa basın…" yazısı belirir.
4. Yeni kombinasyona bas (örneğin <kbd>Ctrl+M</kbd>). <kbd>Esc</kbd> hiçbir şeyi değiştirmeden iptal eder.

| Düğme | Ne yapar |
|---|---|
| Kısayol ekle | İşleve bir kombinasyon ekler. Aynı bağlamdaki başka bir işlevde bu kombinasyon varsa ondan kaldırılır ve uyarı verilir. |
| Temizle | İşlevi kısayolsuz bırakır. |
| Sıfırla | İşlevi varsayılan kısayoluna döndürür. |
| Tümünü sıfırla | Tüm işlevler varsayılan hâline döner. |

Menüler her seçeneğin yanında **ayarlanmış** kısayolu gösterir: bir işlevi yeniden atarsan menü yeni kombinasyonu gösterir. N95'te menüler telefonun tuşunu gösterir (örneğin Taşı için <kbd>1</kbd> ya da Sil için <kbd>C</kbd>); bağlı bir Bluetooth klavye varsa ve tuşu tanıyorsa, o klavyenin tuşunu gösterir.

Değiştirilmiş kombinasyonlar yeşil görünür. Varsayılan bir kısayol işlevinden kaldırıldığında o tuş boşta kalır: o bağlamda artık hiçbir şey yapmaz.

Her bağlamın kısayolları, işaretçi o panelin üzerindeyken geçerlidir; **Genel** kısayolları her yerde geçerlidir. **Dönüşüm sırasında** kısayolları bir şey taşınırken, döndürülürken ya da ölçeklenirken geçerlidir ve önceliklidir. Betik Düzenleyici'de harfler her zaman yazı yazar: orada yalnızca <kbd>Ctrl</kbd>, <kbd>Alt</kbd> ya da F tuşlarıyla yapılan kombinasyonlar yeniden atanabilir.

## atajos.ini dosyası

Değişiklikler kendiliğinden `config.ini`'nin yanındaki `res/atajos.ini` dosyasına kaydedilir. Yalnızca değiştirilmiş işlevler yer alır ve dosya elle düzenlenebilir:

```ini
# işlev = kombinasyon, başka kombinasyon   (boş = kısayol yok)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

Tuş adları: harfler ve rakamlar olduğu gibi, `Enter`, `Esc`, `Tab`, `Space`, `Backspace`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`, `F1`…`F12`, `Num0`…`Num9`, `Num.`, `NumEnter`, `Comma`, `Plus`. Değiştirici tuşlar: `Ctrl+`, `Shift+`, `Alt+`.

## Varsayılan kısayollar

<!-- Aşağıdaki tablolar, ATAJOS-FIN'e kadar, koddan tools/genmanual_atajos.py tarafından üretilir: elle düzenleme. -->

<!--ATAJOS-INICIO-->

### Genel (herhangi bir panel)

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | atajos.ini içindeki adı |
|---|---|---|---|
| Projeyi kaydet | <kbd>Ctrl+S</kbd> | — | `global.guardar` |
| Geri Al | <kbd>Ctrl+Z</kbd> | <kbd>C+Sol</kbd> | `global.deshacer` |
| Yinele | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Sağ</kbd> | `global.rehacer` |
| Nesne / Düzenleme Modu | <kbd>Tab</kbd> | <kbd>Tamam</kbd> | `global.modo` |
| Mıknatısı aç/kapat | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Mıknatıs menüsü (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Oynat / Duraklat | <kbd>Space</kbd> | <kbd>Yeşil+Sol soft</kbd> | `global.play` |
| Menü çubuğu | <kbd>M</kbd> | <kbd>Sol soft</kbd> | `global.barraMenu` |
| Ebeveyn | <kbd>Ctrl+P</kbd> | — | `global.emparentar` |
| Ebeveyni Kaldır | <kbd>Alt+P</kbd> | — | `global.desemparentar` |
| Tam ekran | <kbd>F11</kbd> | — | `global.pantalla` |
| Görüntü Render Al | <kbd>F12</kbd> | — | `global.render` |
| Animasyon Render Al | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### 3D Görünüm

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | atajos.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | <kbd>Kalem+Yukarı</kbd> | `3d.seleccionarTodo` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | <kbd>Kalem+Yukarı</kbd> | `3d.deseleccionar` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Kutu Seçimi | <kbd>B</kbd> | — | `3d.caja` |
| Bağlantılıyı Seç | <kbd>L</kbd> | — | `3d.vinculado` |
| Ekle | <kbd>Shift+A</kbd> | — | `3d.agregar` |
| Uygula | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Taşı | <kbd>G</kbd> | <kbd>1</kbd> | `3d.mover` |
| Döndür | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotar` |
| Ölçekle | <kbd>S</kbd> | <kbd>3</kbd> | `3d.escalar` |
| Konumu Temizle (Poz) | <kbd>Alt+G</kbd> | — | `3d.limpiarPos` |
| Dönmeyi Temizle (Poz) | <kbd>Alt+R</kbd> | — | `3d.limpiarRot` |
| Daralt/Şişir / Ölçeği Temizle | <kbd>Alt+S</kbd> | — | `3d.limpiarEsc` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `3d.duplicar` |
| Bağlantılı Çoğalt | <kbd>Alt+D</kbd> | — | `3d.instancia` |
| Sil | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.borrar` |
| Gizle / Göster | <kbd>H</kbd> | — | `3d.ocultar` |
| Birleştir | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Anahtar Kare Ekle | <kbd>I</kbd> | — | `3d.keyframe` |
| Anahtar kare ekle (tümü) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Orantılı Düzenleme | <kbd>O</kbd> | — | `3d.proporcional` |
| Bindirmeler | <kbd>K</kbd> | — | `3d.overlays` |
| Yerel görünüm | <kbd>/</kbd> | — | `3d.vistaLocal` |
| Seçileni Çerçevele | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.encuadrar` |
| Ön görünüm | <kbd>Num1</kbd> | <kbd>#+Sol/Sağ</kbd> | `3d.vistaFrente` |
| Arka görünüm | <kbd>Ctrl+Num1</kbd> | <kbd>#+Sol/Sağ</kbd> | `3d.vistaAtras` |
| Sağ görünüm | <kbd>Num3</kbd> | <kbd>#+Sol/Sağ</kbd> | `3d.vistaDerecha` |
| Sol görünüm | <kbd>Ctrl+Num3</kbd> | <kbd>#+Sol/Sağ</kbd> | `3d.vistaIzquierda` |
| Üst görünüm | <kbd>Num7</kbd> | <kbd>#+Yukarı</kbd> | `3d.vistaArriba` |
| Alt görünüm | <kbd>Ctrl+Num7</kbd> | <kbd>#+Aşağı</kbd> | `3d.vistaAbajo` |
| Perspektif / Ortografik | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspectiva` |
| Görünümü sola döndür | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Görünümü sağa döndür | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Kamera görünümü | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Etkin Kamera Yap | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Ekstrüde | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| Köşelerden yeni yüz | <kbd>F</kbd> | — | `3d.cara` |
| Normalleri Yeniden Hesapla | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Döngü Kes ve Kaydır | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Köşe Yolunu Bağla | <kbd>J</kbd> | — | `3d.conectar` |
| Yırt | <kbd>V</kbd> | — | `3d.rip` |
| Köşe Kaydır | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Ayır | <kbd>P</kbd> | — | `3d.separar` |
| Kaynaştır | <kbd>M</kbd> | — | `3d.merge` |
| Bağlam menüsü | <kbd>W</kbd> | — | `3d.menuContexto` |
| Üçgenle | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| UV menüsü | <kbd>U</kbd> | — | `3d.menuUV` |

### Dönüşüm sırasında (taşıma / döndürme / ölçekleme)

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | atajos.ini içindeki adı |
|---|---|---|---|
| Onayla | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>Tamam</kbd> | `xf.confirmar` |
| İptal | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancelar` |
| X ekseni | <kbd>X</kbd> | <kbd>1</kbd> | `xf.ejeX` |
| Y ekseni | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.ejeY` |
| Z ekseni | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.ejeZ` |
| X hariç (düzlem) | <kbd>Shift+X</kbd> | — | `xf.planoX` |
| Y hariç (düzlem) | <kbd>Shift+Y</kbd> | — | `xf.planoY` |
| Z hariç (düzlem) | <kbd>Shift+Z</kbd> | — | `xf.planoZ` |
| Orantılı yarıçap + | <kbd>PageUp</kbd> | — | `xf.radioMas` |
| Orantılı yarıçap - | <kbd>PageDown</kbd> | — | `xf.radioMenos` |

### UV Düzenleyici

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | atajos.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | — | `uv.seleccionarTodo` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | — | `uv.deseleccionar` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `uv.invertir` |
| Bağlantılıyı Seç | <kbd>L</kbd> | — | `uv.vinculado` |
| Kutu Seçimi | <kbd>B</kbd> | — | `uv.caja` |
| Taşı | <kbd>G</kbd> | <kbd>1</kbd> | `uv.mover` |
| Döndür | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotar` |
| Ölçekle | <kbd>S</kbd> | <kbd>3</kbd> | `uv.escalar` |
| Anahtar Kare Ekle | <kbd>I</kbd> | — | `uv.keyframe` |
| Anahtar kare ekle (tümü) | <kbd>Shift+I</kbd> | — | `uv.keyframeTodo` |
| Ekstrüde | <kbd>E</kbd> | — | `uv.extruir` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `uv.duplicar` |
| Sil | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.borrar` |
| Seçileni Çerçevele | <kbd>Num.</kbd> | — | `uv.encuadrar` |

### Zaman Çizelgesi

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | atajos.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Taşı | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Ölçekle | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Tutamaçları döndür | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Sil | <kbd>X</kbd> | — | `tl.borrar` |
| Enterpolasyon | <kbd>T</kbd> | — | `tl.interp` |
| Tutamaç türü | <kbd>V</kbd> | — | `tl.handle` |
| Seçileni Çerçevele | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Önceki kare | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Sonraki kare | <kbd>Right</kbd> | — | `tl.frameSig` |
| Önceki anahtar kare | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Sonraki anahtar kare | <kbd>Up</kbd> | — | `tl.keySig` |
| Başa git | <kbd>Home</kbd> | — | `tl.inicio` |
| Sona git | <kbd>End</kbd> | — | `tl.fin` |

### Outliner

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | atajos.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | — | `out.seleccionarTodo` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | — | `out.deseleccionar` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `out.invertir` |
| Taşı (yeniden sırala) | <kbd>G</kbd> | <kbd>1</kbd> | `out.mover` |
| Yeniden adlandır | <kbd>F2</kbd> | — | `out.renombrar` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `out.duplicar` |
| Bağlantılı Çoğalt | <kbd>Alt+D</kbd> | — | `out.instancia` |
| Gizle / Göster | <kbd>H</kbd> | <kbd>2</kbd> | `out.ocultar` |
| Sil | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.borrar` |
| Etkini göster | <kbd>Num.</kbd> | — | `out.encuadrar` |

### 2D Düzenleyici

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | atajos.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | — | `2d.seleccionarTodo` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | — | `2d.deseleccionar` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `2d.invertir` |
| Taşı | <kbd>G</kbd> | <kbd>1</kbd> | `2d.mover` |
| Döndür | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotar` |
| Ölçekle | <kbd>S</kbd> | <kbd>3</kbd> | `2d.escalar` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `2d.duplicar` |
| Sil | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.borrar` |
| Seçileni Çerçevele | <kbd>Num.</kbd> | — | `2d.encuadrar` |
| Oyun görünümü | <kbd>Num0</kbd> | — | `2d.vistaJuego` |

### Özellikler (rutin adımları)

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | atajos.ini içindeki adı |
|---|---|---|---|
| Adımı tut | <kbd>G</kbd> | — | `props.agarrar` |
| Adımı sil | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Anahtar Kare Ekle | <kbd>I</kbd> | — | `props.keyframe` |

### Betik Düzenleyici

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | atajos.ini içindeki adı |
|---|---|---|---|
| Betiği kaydet | <kbd>Ctrl+S</kbd> | — | `ide.guardar` |
| Geri Al | <kbd>Ctrl+Z</kbd> | — | `ide.deshacer` |
| Kopyala | <kbd>Ctrl+C</kbd> | — | `ide.copiar` |
| Kes | <kbd>Ctrl+X</kbd> | — | `ide.cortar` |
| Yapıştır | <kbd>Ctrl+V</kbd> | — | `ide.pegar` |
| Tümünü Seç | <kbd>Ctrl+A</kbd> | — | `ide.todo` |
| Yinele | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.rehacer` |
| Bul | <kbd>Ctrl+F</kbd> | — | `ide.buscar` |
| Değiştir | <kbd>Ctrl+H</kbd> | — | `ide.reemplazar` |

<!--ATAJOS-FIN-->

## Sabit tuşlar (yeniden atanamaz)

Bazı tuşlar, o an ne yapıldığına bağlı oldukları için listede yer almaz: menülerde ve panellerde gezinirken ok tuşları, menülerde ve pencerelerde <kbd>Enter</kbd> ile <kbd>Esc</kbd>, bir satır taşınırken Outliner'ın ok tuşları, yazarken IDE'nin klavyesi ve N95'te telefonun tuş takımı ([Gezinme](navegacion.md) bölümüne bak).
