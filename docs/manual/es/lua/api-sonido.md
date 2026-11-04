# Sonido

| Función | Qué hace |
|---|---|
| `beep(freq [, ms, vol])` | Un pitido (sin archivo). |
| `sonido("sonidos/x.wav" [, vol, tono, loop, pan]) → h` | Reproduce un sonido; devuelve un identificador. |
| `sonido("sonidos/x.wav", {vol, tono, loop, pan, ataque, decaimiento, sostenido, caida, liberacion, duracion, reverb}) → h` | Lo mismo con nombres, más la envolvente (tiempos en segundos; `sostenido` de 0 a 1; `caida`: el sostenido baja 60 dB en esos segundos; la nota se suelta sola a los `duracion` segundos) y lo que el sonido manda a la reverb (0 a 1). |
| `pararSonido(h [, fundido])`, `pararSonidos()`, `ajustarSonido(h, vol [, tono])` | Cortar o ajustar sonidos. |
| `soltarSonido(h)` | Suelta la nota: la envolvente pasa a su liberación. |
| `reverb(sala, amortiguacion, nivel)` | La reverb general (cada valor de 0 a 1; `nivel` 0 la apaga). |
| `silenciar(bool)`, `estaMudo()` | Silencio general. |
| `tocarSonido(obj [, seg])`, `callarSonido(obj)` | Arranca de nuevo un **objeto Sonido** (desde `seg` segundos de su tramo) o lo calla. |
| `sonidoObj(obj) → t`, `setSonidoObj(obj, t)` | Leer / cambiar un objeto Sonido: `archivo` (la fuente), `activo`, `volumen`, `tono`, `reverb`, `paneo`, `inicio`, `recorteIni`, `recorteFin`, `bucle`, `posicional`, `radio`, `alcance` (y al leer `duracion`, `sonando`). |
| `crearSonido("sonidos/x.wav" [, padre]) → obj` | Un objeto Sonido nuevo (`destruir(obj)` lo saca). En un prefab se instancia como cualquier objeto. |
