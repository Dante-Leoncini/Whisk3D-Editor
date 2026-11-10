# Ses ve müzik

**W3dAudio**, C++03 ile yazılmış ve tüm sistemlerde aynı olan bir **yazılım karıştırıcısıdır** (Quake'in kullandığı fikrin aynısı). En fazla 64 sesi, doğrusal enterpolasyonla (perde değişiklikleri kumlu duyulmaz) 16 bitlik bir stereo akışa karıştırır; bir tepe sınırlayıcısı, ADSR zarfları, yankı (reverb) ve bir müzik sıralayıcısının karenin ne zaman çalıştığına bağlı kalmaması için kendi saati vardır.

Her sistem yalnızca **çıkışı** katar: cihazı açan ve karıştırıcıdan bir sonraki parçayı isteyen dört fonksiyon (`W3dAudioBackendInit`, `Shutdown`, `Lock`, `Unlock`, `W3dAudioBackend.h` içinde).

| Sistem | Çıkış |
|---|---|
| PC, Android, tarayıcı | SDL2 (44.100 Hz) |
| Symbian | Kendi iş parçacığında, çift buffer'lı `CMdaAudioOutputStream` (N95'te 22.050 Hz) |
| Konsollar | Konsol için derlenen editör sessizdir. "Ses kullan" ile derlenen bir oyun, SDL çıkışıyla birlikte karıştırıcıyı taşır |
| Dreamcast | Ayrıca `w3dAICA`: ses çipini doğrudan kullanan isteğe bağlı bir modül (donanımla 64 kanal ve 4 bitlik ADPCM). W3dAudio ile bir arada bulunmaz |

Üç parça daha var:

- **W3dMusic:** uzun müzik (mp3, ogg, aac) PCM'e dönüştürülmez: onu sistem, neredeyse her zaman donanımla çözer. Bugün tarayıcı için bir arka ucu var (bir `<audio>` öğesi); Symbian ve masaüstü arka uçları beklemede.
- **W3dClip:** kısa arayüz sesleri. Tarayıcıda WebAudio ile çalar, çünkü iOS'ta onları `<audio>` ile tetiklemek telefonu kilitler. Diğerlerinde karıştırıcının sesleridir.
- **W3dVolumen:** genel ses düzeyi ve sessize alma, çalan her şey için geçerlidir ve ayarlara kendiliğinden kaydedilir.

Neden önemli: Android oyunu, .wav dosyaları APK'nın içine düzgünce paketlenmiş olmasına rağmen **sessiz** çıkıyordu, çünkü `fopen` ile açılıyorlardı ve orada dosya değiller. Ses `ReadFileBytes` ile yüklendiğinden beri her yerde çalışıyor. Lua'dan: [Ses](../lua/api-sonido.md).
