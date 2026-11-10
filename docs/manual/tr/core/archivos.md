# Dosyalar

Okumak ve yazmak için tek bir API: `w3dFileSystem::ReadFileBytes`, `ReadTextFile`, `WriteTextFile`, `ReadFileRange` (mesh'leri parça parça yüklemek için bir parça), `ListDir`, `FileExists`. Kimse tek başına `fopen` çağırmaz.

`ReadFileBytes` şu sırayla arar:

1. **Bağlı bir kütüphane** (`lib:<nombre>/...` yolları).
2. **Gömülü pak:** "Varlıklar: Paketli" ile derlenmiş bir oyun, tüm dosyalarını çalıştırılabilir dosyanın içinde taşır.
3. **Bağlanmış kapsayıcı:** açık olan .w3d. Kapsayıcı **bağlanır, çıkarılmaz**: dokular, yazı tipleri, script'ler ve arayüzler, kimse kodunu değiştirmeden zip'in içinden yüklenir.
4. **APK'nın asset'leri** (Android): göreli bir yol, NDK'nın `AAssetManager`'ı ile APK'dan okunur.
5. **Disk.**

Yollar aramadan önce normalleştirilir (`.` ve `..` olmadan). `modelos/crash/../../texturas/sombra_blob.png` adını veren bir .mtl hiçbir indekste bulunmuyordu ve APK'nın içinde doku beyaz çıkıyordu.

| Sistem | Kaynaklar (`res/`) | Nereye yazılır |
|---|---|---|
| Windows, Linux | Çalıştırılabilir dosyanın yanında (Linux'ta ayrıca `<prefijo>/share/Whisk3d/res`) | Kullanıcı klasörü; ayarlar SDL'in klasörüne (`SDL_GetPrefPath`) |
| Android | APK'nın içinde | `/storage/emulated/0`; dışa aktarılanlar İndirilenler'e |
| Tarayıcı | `.data` içine paketlenmiş, `/res` altına bağlanmış | Ayarlar `localStorage`'a; kaydetmek dosyayı indirir ([nasıl](../editor/proyecto.md)) |
| Symbian | Telefonun kendi yolları | `E:/` (hafıza kartı) ya da `C:/` |
| Dreamcast | Çalıştırılabilir dosyanın içinde bir romdisk (`/rd`) | Kapatınca silinen `/ram` |
| PSP | `EBOOT.PBP` dosyasının yanında | `ms0:/` (PSP Go'da `ef0:/`) |
| PS Vita | `.vpk` dosyasının içinde (`app0:res`) | `ux0:` |
| 3DS | `.3dsx` dosyasının romfs'i (`romfs:`) | SD kart (`sdmc:/`) |

Modülün iki parçası daha:

- **W3dRecursos** her ağır verinin (dokular, mesh'ler, animasyonlar) hesabını tutar: onu kimin kullandığını ve ne zaman serbest bırakılacağını. Hemen gerekmeyenler dilim dilim, her karede biraz yüklenir.
- **W3dConfig** ayarları `clave=valor` biçiminde kaydeder. Tarayıcıda `localStorage`'a, diğerlerinde bir metin dosyasına gider. Dinamik bellek istemez, böylece Symbian'da da aynı şekilde işe yarar. Lua'dan: [Kaydedilen ayarlar](../lua/api-configuracion.md).
