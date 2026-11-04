# Som

| Função | O que faz |
|---|---|
| `beep(freq [, ms, vol])` | Um bipe (sem arquivo). |
| `sonido("sonidos/x.wav" [, vol, tono, loop, pan]) → h` | Reproduz um som; devolve um identificador. |
| `sonido("sonidos/x.wav", {vol, tono, loop, pan, ataque, decaimiento, sostenido, caida, liberacion, duracion, reverb}) → h` | O mesmo com nomes, mais o envelope (ataque, decaimento, sustentação de 0 a 1, `caida`: a sustentação cai 60 dB nesses segundos, liberação e `duracion`: a nota se solta sozinha depois desses segundos) e quanto do som vai para a reverberação (0 a 1). |
| `pararSonido(h [, fundido])`, `pararSonidos()`, `ajustarSonido(h, vol [, tono])` | Interromper ou ajustar sons. |
| `soltarSonido(h)` | Solta a nota: o envelope passa para a liberação. |
| `reverb(sala, amortiguacion, nivel)` | A reverberação geral: tamanho da sala, amortecimento e nível (0 a 1 cada; nível 0 a desliga). |
| `silenciar(bool)`, `estaMudo()` | Silêncio geral. |
| `tocarSonido(obj [, seg])`, `callarSonido(obj)` | Toca de novo um **objeto Som** (a partir de `seg` segundos do seu trecho) ou o cala. |
| `sonidoObj(obj) → t`, `setSonidoObj(obj, t)` | Ler / mudar um objeto Som: `archivo` (a fonte), `activo`, `volumen`, `tono`, `reverb`, `paneo`, `inicio`, `recorteIni`, `recorteFin`, `bucle`, `posicional`, `radio`, `alcance` (e ao ler `duracion`, `sonando`). |
| `crearSonido("sonidos/x.wav" [, padre]) → obj` | Um objeto Som novo (`destruir(obj)` o remove). Num prefab é instanciado como qualquer objeto. |
