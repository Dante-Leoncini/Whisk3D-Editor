# Whisk3D nedir

Whisk3D, sıfırdan yazılmış, özgür bir **3D editör ve oyun motorudur**. Amaç, elinde ne varsa onunla istediğini yaratabilmek: PC'de, telefonda, ayrıca konsollarda ve eski cihazlarda çalışır; modern donanıma ya da tescilli yazılıma bağımlı değildir.

## Neler yapılabilir

**Whisk3D Editor** ile şunları yapabilirsin:

- 3D **modelleme**: köşeleri, kenarları ve yüzleri olan örgüler ve örgüyü bozmadan çalışan [değiştiriciler](objeto-malla.md#değiştiriciler-sekmesi).
- Onlara **rig** yapma: bir karakterin [iskeletini](esqueletos.md) kurma ve her kemiğin ne kadar hareket ettirdiğini boyama.
- **Animasyon**: anahtar kareler, eğri editörü ve iskeletle yapılan animasyonlar ([Zaman Çizelgesi](linea-de-tiempo.md)).
- Sahneyi nokta ışıklar, güneş ışıkları ve spot ışıklarla **aydınlatma**.
- Gerçek zamanlı ya da **ışın izleme** ile (yumuşak gölgeler, yansımalar ve saydamlıklar) görüntü ve video **render alma**.
- Köşe renklerini ve dokuları doğrudan modelin üzerine **boyama** ([Boyama modları](pintura.md)).
- **Oyun yapma**: mantık, editöre entegre [script editörüyle](editor-de-scripts.md) [Lua](../lua/README.md) ile yazılır; arayüzler (menüler, HUD) [2D Düzenleyici'de](editor-2d.md) kurulur ve oyun, aynı editörden her sistem için derlenir.
- Modelleri OBJ, FBX, glTF, GLB ve STL olarak içe ve dışa aktarma.

Whisk3D'nin ayrıca projeleri ve oyunları kaydetmek için **kendi özgür formatı** vardır: **`.w3d`**. `.w3d` biçimindeki bir oyun, Whisk3D'nin çalıştığı herhangi bir sistemde doğrudan açılabilir ya da donanımından en iyi şekilde yararlanmak için belirli bir sistem için **derlenebilir**. [W3D: video oyunları ölmesin diye özgür bir format](https://leoncini.com.ar/articulo.php?id=w3d-formato-abierto&lang=es) yazısına bak.

Oyunlar **editör olmadan**, doğrudan **Whisk3D Core** üzerinde C ve C++ ile programlanarak da yapılabilir. Bu yol yalnızca C ve C++ ile programlamayı zaten bilen deneyimli kullanıcılar içindir: [Whisk3D Core kılavuzuna](../core/empezar.md) bak.

## Nerede çalışır

| Sistem | |
|---|---|
| **PC** | Windows, Linux ve macOS |
| **Telefonlar** | Android (2.3'ten itibaren) ve Nokia N95 ile N8 (Symbian) |
| **Tarayıcı** | Herhangi bir modern tarayıcı (WebGL) |
| **Konsollar** | Sega Dreamcast, Sony PSP, PlayStation Vita ve Nintendo 3DS |

## Nasıl kurulmuştur

Whisk3D, birbirinin üzerinde duran üç projeden oluşur; birlikte dönen üç dişli gibi:

- **Whisk3D Editor** (yeşil dişli): yukarıdaki her şeyiyle editör.
- **[Whisk3D UI](https://leoncini.com.ar/proyecto.php?id=whisk3d-ui&lang=es)** (sarı): **arayüzle** ilgilenir: düğmeler, menüler, özellik kartları, metin ve simgeler. Editör onunla yapılmıştır. [Whisk3D UI kılavuzuna](../ui/README.md) bak.
- **[Whisk3D Core](https://leoncini.com.ar/proyecto.php?id=whisk3d-core&lang=es)** (kırmızı): **soyutlama katmanı**. Whisk3D UI, yaptığı her şey için onu kullanır.

Editörde yapılan bir oyun ya da proje, mantığı Lua ile yazılmış olarak editörün **içinde** yaşar. Ama Editor ve Whisk3D UI **isteğe bağlıdır**: kendi araçların da kullanılabilir. C ya da C++ ile yazılmış bir oyun, editör olmadan arayüzü için Whisk3D UI'ı kullanabilir ya da kendi arayüzü ve kendi araçlarıyla **doğrudan Core ile** konuşabilir. Olmazsa olmaz tek şey Core'dur.

![Whisk3D'nin üç katmanlı mimarisi: üstte Editor (yeşil), içinde Lua ile yazılmış oyunun ya da projen; ortada Whisk3D UI (sarı); altta Whisk3D Core (kırmızı). Editor ve UI isteğe bağlıdır: dışarıdan, C++ ile yazılmış bir oyun UI'ı, bir başkası da doğrudan Core'u kullanabilir. Core her sistemle o sisteme ait backend üzerinden konuşur](img/whisk3d-capas.webp)

Core işin **gizli malzemesidir**. Her sistem kendi yöntemiyle çizer, ses çalar, dosya okur ve tuşları alır: bir PC OpenGL kullanır, Dreamcast'in kendi grafik çipi vardır, PSP'nin ve 3DS'nin kendi kütüphaneleri, tarayıcının da WebGL'i. Ne editör ne de oyunlar her sistemle ayrı ayrı konuşur: istediklerini her zaman aynı şekilde Core'dan isterler ("bu örgüyü çiz", "bu sesi çal", "bu dosyayı aç") ve Core bunu her birinde nasıl yapacağını bilir. Bu yüzden **aynı kod** bu kadar farklı makinelerde çalışır ve yeni bir sistem eklemek, editörü yeniden yazmak değil, Core'a o sistemle konuşmayı öğretmektir.

Bunu nasıl yaptığı, tüm ayrıntılarıyla (grafikler, ses, dosyalar, girdi, ağ, log'lar): [Whisk3D Core kılavuzu](../core/README.md).

## Proje

Tüm çalışma bir `.w3d` proje dosyasına kaydedilir. İçinde sahne ve kaynaklar birlikte taşınır: dokular (`texturas/`), script'ler (`scripts/`), sesler, sahneler, prefab'lar. Bir kaynak .w3d'nin **içinde** ya da diskte ayrı bir dosya olarak **dışında** durabilir; ".w3d içine kaydet" ve ".w3d dışına kaydet" menüleri onu bir yerden diğerine taşır. [Proje](proyecto.md) bölümüne bak.

> [!NOTE]
> Bu kılavuz Whisk3D'nin bugün yapabildiklerini anlatır. İşlev adları, editörde Türkçe olarak gördüğün adlardır. Belirtilen tuşlar varsayılan tuşlardır: [Klavye kısayolları](atajos.md) bölümünden değiştirilebilirler.
