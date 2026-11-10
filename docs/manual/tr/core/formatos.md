# Matematik, sahne ve formatlar

PC'de ve retro sistemlerde aynı şekilde derlenen kendi **vektörleri, 4x4 matrisleri ve kuaterniyonları** (`math/`). Bunların üstünde **sahne** (`objects/`): ebeveyn ve çocuk hiyerarşisine sahip nesneler, mesh'ler, materyaller, ışıklar (yönlü, noktasal ve spot), kameralar ve iskeletler. Ve **animasyonlar** (`animation/`): anahtar kareler, iskeletler, köşeler, flipbook'lar ve hareket (locomotion).

Kendi formatları, küçük bir makinenin onları hızlı okuyabilmesi için tasarlanmıştır:

| Format | Nedir |
|---|---|
| `.w3d` | Proje: standart bir ZIP (herhangi bir arşiv açıcıyla açılır). Sıkıştırılmadan yazılır, sıkıştırılmışken de okunur. ZIP64 yok |
| `.w3dm` | Satır satır metin olarak geometri: okunabilir, elle düzenlenebilir ve N95'in okuması kolay |
| `.w3db` | Aynı mesh ikili biçimde, belleğe hazır: onu açmak dizileri kopyalamaktır |
| `.w3dt` | GPU'ya hazır bir doku (16 bit ve mipmap'leri yapılmış) |
| `.w3dpack` | ChaCha20 ile şifrelenmiş bir asset kapsayıcısı; anahtarı uygulama verir |
| Gömülü pak | Derlenmiş bir oyunun dosyaları, XOR ile gizlenmiş. Meraklılara karşıdır, kriptografi değildir |
| glTF / GLB | Editörün dışında kullanmak için tek bir header'da bir okuyucu ve renderer (`io/w3dGltf.h`) |
