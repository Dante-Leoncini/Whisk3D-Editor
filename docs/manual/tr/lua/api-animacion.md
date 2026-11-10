# Animasyon

| Fonksiyon | Ne yapar |
|---|---|
| `animEscena(nombre [, loop])`, `pararEscena()`, `animEscenaFrame(f)` | Tüm sahnenin animasyonları. |
| `animObjeto(raiz, "clip" [, loop])`, `animObjetoParar`, `animObjetoTermino`, `animObjetoVelocidad` | Bir nesnenin ve çocuklarının animasyon klipleri. |
| `animClip(esqueleto, clip [, loop])`, `animTermino`, `animVelocidad`, `animTransicion` | Bir iskeletin animasyonları (yürüme, zıplama…) ve aralarındaki yumuşak geçiş. |
| `animCapa(esq, n, clip [, peso, modo])` ve benzerleri | Birden fazla animasyonu aynı anda katmanlarda (1'den 16'ya) karıştırma, örneğin koşma + ateş etme. |
| `huesoPunto(esq, "hueso" [, x, y, z]) → x, y, z` | Bir kemiğin nerede olduğu (ele bir silah koymak için). |
| `animar(malla, n [, frame])` | Bir örgünün köşe animasyonunu değiştirir. |
