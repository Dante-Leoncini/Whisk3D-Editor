# Ağ

**w3dRed**, UDP üzerinden yerel ağda, Wi-Fi'da ya da internette oynatır. Biri oturumu **barındırır** (host, port 47023), diğerleri onu yerel ağda **arar** ya da bir adresle **katılır**. Her şey ev sahibinden geçer; en fazla 8 oyuncu. Mesajlar **güvenilir** (hepsi ve sırayla ulaşır: "hazır", "başla") ya da **güvenilmez** (her karede gönderilenler; bir sonraki, kaybolanın yerini alır) olabilir.

```cpp
w3dNet::Host("mi_juego", "La partida de Dante", 2);
// ya da diğer taraftan:
w3dNet::Find("mi_juego");   // ... w3dNet::Matches() ... w3dNet::Join(direccion);
w3dNet::Send(-1, data, bytes, true);   // -1 = diğer herkese
```

| Sistem | Soketler |
|---|---|
| Windows | Winsock |
| Linux, macOS, Android | POSIX (arayüz listesi, Android 7'den itibaren) |
| Symbian | Open C soketleri |
| Dreamcast | Ağ adaptörü üzerinden KallistiOS soketleri |
| Tarayıcı, PSP, PS Vita, 3DS | Şimdilik ağ yok: `Supported()` `false` döndürür |

Uygulama izin vermezse (`SetAllowed`) bir oyun ağı kullanmaz: editör, ağı kullanan bir projeyi açarken sorar. Lua'dan: [Ağda oynama](../lua/api-red.md).
