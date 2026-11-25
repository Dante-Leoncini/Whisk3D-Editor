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

## shortcuts.ini dosyası

Değişiklikler kendiliğinden `config.ini`'nin yanındaki `res/shortcuts.ini` dosyasına kaydedilir. Yalnızca değiştirilmiş işlevler yer alır ve dosya elle düzenlenebilir:

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

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | shortcuts.ini içindeki adı |
|---|---|---|---|
| Projeyi kaydet | <kbd>Ctrl+S</kbd> | — | `global.save` |
| Geri Al | <kbd>Ctrl+Z</kbd> | <kbd>C+Sol</kbd> | `global.undo` |
| Yinele | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Sağ</kbd> | `global.redo` |
| Nesne / Düzenleme Modu | <kbd>Tab</kbd> | <kbd>Tamam</kbd> | `global.mode` |
| Mıknatısı aç/kapat | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Mıknatıs menüsü (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Oynat / Duraklat | <kbd>Space</kbd> | <kbd>Yeşil+Sol soft</kbd> | `global.play` |
| Menü çubuğu | <kbd>M</kbd> | <kbd>Sol soft</kbd> | `global.menuBar` |
| Ebeveyn | <kbd>Ctrl+P</kbd> | — | `global.parent` |
| Ebeveyni Kaldır | <kbd>Alt+P</kbd> | — | `global.clearParent` |
| Tam ekran | <kbd>F11</kbd> | — | `global.screen` |
| Görüntü Render Al | <kbd>F12</kbd> | — | `global.render` |
| Animasyon Render Al | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### 3D Görünüm

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | shortcuts.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | <kbd>Kalem+Yukarı</kbd> | `3d.selectAll` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | <kbd>Kalem+Yukarı</kbd> | `3d.deselect` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `3d.invert` |
| Kutu Seçimi | <kbd>B</kbd> | — | `3d.box` |
| Bağlantılıyı Seç | <kbd>L</kbd> | — | `3d.linked` |
| Ekle | <kbd>Shift+A</kbd> | — | `3d.add` |
| Uygula | <kbd>Ctrl+A</kbd> | — | `3d.apply` |
| Taşı | <kbd>G</kbd> | <kbd>1</kbd> | `3d.move` |
| Döndür | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotate` |
| Ölçekle | <kbd>S</kbd> | <kbd>3</kbd> | `3d.scale` |
| Konumu Temizle (Poz) | <kbd>Alt+G</kbd> | — | `3d.clearPos` |
| Dönmeyi Temizle (Poz) | <kbd>Alt+R</kbd> | — | `3d.clearRot` |
| Daralt/Şişir / Ölçeği Temizle | <kbd>Alt+S</kbd> | — | `3d.clearScale` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `3d.duplicate` |
| Bağlantılı Çoğalt | <kbd>Alt+D</kbd> | — | `3d.instance` |
| Sil | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.delete` |
| Gizle / Göster | <kbd>H</kbd> | — | `3d.hide` |
| Birleştir | <kbd>Ctrl+J</kbd> | — | `3d.join` |
| Anahtar Kare Ekle | <kbd>I</kbd> | — | `3d.keyframe` |
| Anahtar kare ekle (tümü) | <kbd>Shift+I</kbd> | — | `3d.keyframeAll` |
| Orantılı Düzenleme | <kbd>O</kbd> | — | `3d.proportional` |
| Bindirmeler | <kbd>K</kbd> | — | `3d.overlays` |
| Yerel görünüm | <kbd>/</kbd> | — | `3d.viewLocal` |
| Seçileni Çerçevele | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.focus` |
| Ön görünüm | <kbd>Num1</kbd> | <kbd>#+Sol/Sağ</kbd> | `3d.viewFront` |
| Arka görünüm | <kbd>Ctrl+Num1</kbd> | <kbd>#+Sol/Sağ</kbd> | `3d.viewBack` |
| Sağ görünüm | <kbd>Num3</kbd> | <kbd>#+Sol/Sağ</kbd> | `3d.viewRight` |
| Sol görünüm | <kbd>Ctrl+Num3</kbd> | <kbd>#+Sol/Sağ</kbd> | `3d.viewLeft` |
| Üst görünüm | <kbd>Num7</kbd> | <kbd>#+Yukarı</kbd> | `3d.viewTop` |
| Alt görünüm | <kbd>Ctrl+Num7</kbd> | <kbd>#+Aşağı</kbd> | `3d.viewBottom` |
| Perspektif / Ortografik | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspective` |
| Görünümü sola döndür | <kbd>Num4</kbd> | — | `3d.rollLeft` |
| Görünümü sağa döndür | <kbd>Num6</kbd> | — | `3d.rollRight` |
| Kamera görünümü | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.viewCamera` |
| Etkin Kamera Yap | <kbd>Ctrl+Num0</kbd> | — | `3d.activeCamera` |
| Ekstrüde | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extrude` |
| Köşelerden yeni yüz | <kbd>F</kbd> | — | `3d.face` |
| Normalleri Yeniden Hesapla | <kbd>Shift+N</kbd> | — | `3d.normals` |
| Döngü Kes ve Kaydır | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Köşe Yolunu Bağla | <kbd>J</kbd> | — | `3d.connect` |
| Yırt | <kbd>V</kbd> | — | `3d.rip` |
| Köşe Kaydır | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Ayır | <kbd>P</kbd> | — | `3d.separate` |
| Kaynaştır | <kbd>M</kbd> | — | `3d.merge` |
| Bağlam menüsü | <kbd>W</kbd> | — | `3d.contextMenu` |
| Üçgenle | <kbd>Ctrl+T</kbd> | — | `3d.triangulate` |
| UV menüsü | <kbd>U</kbd> | — | `3d.menuUV` |

### Dönüşüm sırasında (taşıma / döndürme / ölçekleme)

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | shortcuts.ini içindeki adı |
|---|---|---|---|
| Onayla | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>Tamam</kbd> | `xf.confirm` |
| İptal | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancel` |
| X ekseni | <kbd>X</kbd> | <kbd>1</kbd> | `xf.axisX` |
| Y ekseni | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.axisY` |
| Z ekseni | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.axisZ` |
| X hariç (düzlem) | <kbd>Shift+X</kbd> | — | `xf.planeX` |
| Y hariç (düzlem) | <kbd>Shift+Y</kbd> | — | `xf.planeY` |
| Z hariç (düzlem) | <kbd>Shift+Z</kbd> | — | `xf.planeZ` |
| Orantılı yarıçap + | <kbd>PageUp</kbd> | — | `xf.radiusPlus` |
| Orantılı yarıçap - | <kbd>PageDown</kbd> | — | `xf.radiusMinus` |

### UV Düzenleyici

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | shortcuts.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | — | `uv.selectAll` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | — | `uv.deselect` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `uv.invert` |
| Bağlantılıyı Seç | <kbd>L</kbd> | — | `uv.linked` |
| Kutu Seçimi | <kbd>B</kbd> | — | `uv.box` |
| Taşı | <kbd>G</kbd> | <kbd>1</kbd> | `uv.move` |
| Döndür | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotate` |
| Ölçekle | <kbd>S</kbd> | <kbd>3</kbd> | `uv.scale` |
| Anahtar Kare Ekle | <kbd>I</kbd> | — | `uv.keyframe` |
| Anahtar kare ekle (tümü) | <kbd>Shift+I</kbd> | — | `uv.keyframeAll` |
| Ekstrüde | <kbd>E</kbd> | — | `uv.extrude` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `uv.duplicate` |
| Sil | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.delete` |
| Seçileni Çerçevele | <kbd>Num.</kbd> | — | `uv.focus` |

### Zaman Çizelgesi

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | shortcuts.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | — | `tl.selectAll` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | — | `tl.deselect` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `tl.invert` |
| Taşı | <kbd>G</kbd> | <kbd>1</kbd> | `tl.move` |
| Ölçekle | <kbd>S</kbd> | <kbd>3</kbd> | `tl.scale` |
| Tutamaçları döndür | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotate` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `tl.duplicate` |
| Sil | <kbd>X</kbd> | — | `tl.delete` |
| Enterpolasyon | <kbd>T</kbd> | — | `tl.interp` |
| Tutamaç türü | <kbd>V</kbd> | — | `tl.handle` |
| Seçileni Çerçevele | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.focus` |
| Önceki kare | <kbd>Left</kbd> | — | `tl.prevFrame` |
| Sonraki kare | <kbd>Right</kbd> | — | `tl.nextFrame` |
| Önceki anahtar kare | <kbd>Down</kbd> | — | `tl.prevKey` |
| Sonraki anahtar kare | <kbd>Up</kbd> | — | `tl.nextKey` |
| Başa git | <kbd>Home</kbd> | — | `tl.start` |
| Sona git | <kbd>End</kbd> | — | `tl.end` |

### Outliner

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | shortcuts.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | — | `out.selectAll` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | — | `out.deselect` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `out.invert` |
| Taşı (yeniden sırala) | <kbd>G</kbd> | <kbd>1</kbd> | `out.move` |
| Yeniden adlandır | <kbd>F2</kbd> | — | `out.rename` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `out.duplicate` |
| Bağlantılı Çoğalt | <kbd>Alt+D</kbd> | — | `out.instance` |
| Gizle / Göster | <kbd>H</kbd> | <kbd>2</kbd> | `out.hide` |
| Sil | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.delete` |
| Etkini göster | <kbd>Num.</kbd> | — | `out.focus` |

### 2D Düzenleyici

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | shortcuts.ini içindeki adı |
|---|---|---|---|
| Tümünü Seç | <kbd>A</kbd> | — | `2d.selectAll` |
| Tüm seçimi kaldır | <kbd>Alt+A</kbd> | — | `2d.deselect` |
| Seçimi tersine çevir | <kbd>Ctrl+I</kbd> | — | `2d.invert` |
| Taşı | <kbd>G</kbd> | <kbd>1</kbd> | `2d.move` |
| Döndür | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotate` |
| Ölçekle | <kbd>S</kbd> | <kbd>3</kbd> | `2d.scale` |
| Çoğalt | <kbd>Shift+D</kbd> | — | `2d.duplicate` |
| Sil | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.delete` |
| Seçileni Çerçevele | <kbd>Num.</kbd> | — | `2d.focus` |
| Oyun görünümü | <kbd>Num0</kbd> | — | `2d.gameView` |

### Özellikler (rutin adımları)

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | shortcuts.ini içindeki adı |
|---|---|---|---|
| Adımı tut | <kbd>G</kbd> | — | `props.grab` |
| Adımı sil | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.delete` |
| Anahtar Kare Ekle | <kbd>I</kbd> | — | `props.keyframe` |

### Betik Düzenleyici

| İşlev | Varsayılan kısayol | Nokia N95 (telefon tuş takımı) | shortcuts.ini içindeki adı |
|---|---|---|---|
| Betiği kaydet | <kbd>Ctrl+S</kbd> | — | `ide.save` |
| Geri Al | <kbd>Ctrl+Z</kbd> | — | `ide.undo` |
| Kopyala | <kbd>Ctrl+C</kbd> | — | `ide.copy` |
| Kes | <kbd>Ctrl+X</kbd> | — | `ide.cut` |
| Yapıştır | <kbd>Ctrl+V</kbd> | — | `ide.paste` |
| Tümünü Seç | <kbd>Ctrl+A</kbd> | — | `ide.all` |
| Yinele | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.redo` |
| Bul | <kbd>Ctrl+F</kbd> | — | `ide.find` |
| Değiştir | <kbd>Ctrl+H</kbd> | — | `ide.replace` |

<!--ATAJOS-FIN-->

## Sabit tuşlar (yeniden atanamaz)

Bazı tuşlar, o an ne yapıldığına bağlı oldukları için listede yer almaz: menülerde ve panellerde gezinirken ok tuşları, menülerde ve pencerelerde <kbd>Enter</kbd> ile <kbd>Esc</kbd>, bir satır taşınırken Outliner'ın ok tuşları, yazarken IDE'nin klavyesi ve N95'te telefonun tuş takımı ([Gezinme](navegacion.md) bölümüne bak).
