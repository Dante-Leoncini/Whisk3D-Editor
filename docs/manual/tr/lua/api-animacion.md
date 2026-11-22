# Animasyon

| Fonksiyon | Ne yapar |
|---|---|
| `animScene(name [, loop])`, `stopScene()`, `animSceneFrame(f)` | Tüm sahnenin animasyonları. |
| `animObject(root, "clip" [, loop])`, `animObjectStop`, `animObjectFinished`, `animObjectSpeed` | Bir nesnenin ve çocuklarının animasyon klipleri. |
| `animClip(armature, clip [, loop])`, `animFinished`, `animSpeed`, `animTransition` | Bir iskeletin animasyonları (yürüme, zıplama…) ve aralarındaki yumuşak geçiş. |
| `animLayer(esq, n, clip [, weight, mode])` ve benzerleri | Birden fazla animasyonu aynı anda katmanlarda (1'den 16'ya) karıştırma, örneğin koşma + ateş etme. |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | Bir kemiğin nerede olduğu (ele bir silah koymak için). |
| `animate(mesh, n [, frame])` | Bir örgünün köşe animasyonunu değiştirir. |
