# Fizik

Her şey ev yapımı; C++03 ile, harici bir motor olmadan ve her karede bellek istemeden. Kimse kullanmıyorsa hiçbir maliyeti yoktur: her adım daha ilk satırda geri döner.

- **W3dFisica:** eksenlere hizalı kutuların hızı ve sekmesi. O kadar.
- **W3dRigido:** kütleli yönlendirilmiş kutular, yerçekimi, sürtünmeli ve sekmeli itkiler, raycast ve temas olayları.
- **W3dColMalla:** bir sahnenin üçgenleri üzerinde yürümek (rampalar, basamaklar) ve duvarların durdurması; binlerce üçgeni kontrol etmemek için bir ızgarayla.
- **W3dHitbox:** yalnızca haber veren kutular (`alEntrar`, `alQuedarse`, `alSalir`, `alTocar`).
- **W3dRagdoll:** bez bebekler (ragdoll), iki kemikli ters kinematik ve FABRIK.
- **W3dNav:** zemindeki bir ızgara üzerinde bir noktadan diğerine giden yol (A* ve ardından yolu gerdirme).

Neden önemli: editörün Oynat düğmesi ve derlenmiş oyun aynı adımı aynı `dt` ile çağırır, bu yüzden oyun ikisinde de, PC'de de N95'te de aynı davranır. Lua'dan: [Basit fizik](../lua/api-fisica-simple.md) ve [Katı cisimler, çarpışmalar ve hitbox'lar](../lua/api-cuerpos-rigidos.md).
