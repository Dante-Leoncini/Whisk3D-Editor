# Eksenler, birimler ve nesneler

- **3D:** script'lerde **Y ekseni yukarıyı gösterir**. Özellikler paneli Z'yi yukarı olarak gösterir, yani paneldeki "Z" alanı script'teki `y`'dir. Konumlar, dönüşler ve ölçekler nesnenin ebeveynine göredir. Dönüşler **derece** cinsindendir.
- **2D (ekran ve UI):** tuvalin pikselleri; (0, 0) **merkezdedir**; +x sağa, +y **aşağıya** doğrudur.
- **Nesneler:** nesne bulan ya da döndüren fonksiyonlar (`yo`, `buscar`, `objeto`…) alanı ya da metodu olmayan bir değer verir: onu başka fonksiyonlara aktarırsın. İki nesne `==` ile karşılaştırılabilir. Nesne yok edildiyse `nil` gibi davranır.
- **Hata toleransı:** neredeyse tüm fonksiyonlar `nil`'i ya da yanlış türde bir nesneyi hiçbir şeyi bozmadan kabul eder: okuyanlar 0, `""` ya da `false` döndürür, yazanlar ise hiçbir şey yapmaz.
- **Dizinler:** Lua'da olduğu gibi neredeyse her şey 1'den başlar (köşeler, parmaklar, katmanlar, rutin adımları). İstisna, rutinlerin bellekleridir (`memoria`): 0'dan 255'e kadar giderler.
