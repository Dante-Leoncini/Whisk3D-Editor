# Grafica

## L'API

Tutto ciò che si disegna passa per le funzioni di `w3dEngine` (`graphics/w3dGraphics.h`): attivare e disattivare capacità (`Enable`, `Disable`), matrici (`MatrixMode`, `PushMatrix`, `Translatef`, `Perspective`), array di vertici (`VertexPointer3f`, `NormalPointer3b`, `TexCoordPointer2f`), buffer sulla GPU (`GenBuffer`, `ArrayBufferData`), il disegno (`DrawTriangles`, `DrawLines`, `DrawPoints`), materiali (`Material`), luci (`LightNfv`, `LightNSpot`), nebbia, fusione, ritaglio per rettangolo, stencil e piani di clipping.

Somiglia all'OpenGL classico di proposito: è il modo di disegnare che capiscono più sistemi, dall'N95 al Dreamcast.

## Pipeline fissa e shader

Ci sono due famiglie di GPU:

- **Pipeline fissa.** La GPU ha i passi già pronti di fabbrica: trasformare i vertici, calcolare la luce per vertice, combinare la texture, applicare la nebbia. Il programma si limita ad accendere interruttori e passare valori. Così funzionano OpenGL ES 1.1 (l'N95), l'OpenGL classico del PC, il GE della PSP e la PVR del Dreamcast.
- **Shader.** La GPU non ha niente di pronto: ogni passo è un piccolo programma che gira su di essa. Così funzionano OpenGL ES 2.0, WebGL e Vulkan.

Il Core parla la lingua della pipeline fissa. Il backend a shader la **emula** con un "uber-shader" che fa gli stessi calcoli di OpenGL ES 1.1 (luce per vertice con fino a 8 luci, attenuazione, cono, nebbia, la combinazione di due texture). Per questo il browser e Android hanno lo stesso aspetto del PC e dell'N95.

## I backend

| Backend | Cos'è | Dove si usa |
|---|---|---|
| `opengl/` | Pipeline fissa: OpenGL desktop, OpenGL ES 1.1, GLdc, vitaGL | Linux, Windows, macOS, N95 e S60, Dreamcast, PS Vita |
| `gles2/` | Shader: OpenGL ES 2.0 e WebGL | Android, il browser, il Nokia N8 (Symbian^3), il PC (opzionale) |
| `vulkan/` | Lo stesso backend a shader, compilato sopra un OpenGL ES 2.0 realizzato con Vulkan | Il PC (opzionale) |
| `gu/` | Il GE della PSP | PSP |
| `citro3d/` | La GPU PICA200 del 3DS | Nintendo 3DS |
| `pvr/` | Disegno diretto sulla PVR del Dreamcast | Modulo opzionale per giochi nativi |
| `video/` | Video su texture | Opzionale: FFmpeg su desktop, `<video>` nel browser |

## Come si sceglie

Quasi sempre, **in compilazione**: ogni sistema compila un solo backend e le chiamate vanno dirette, senza alcun costo extra.

L'eccezione è il PC (Windows e Linux). Lì l'eseguibile include OpenGL, OpenGL ES 2.0 e Vulkan, e si sceglie **all'avvio**, in **Proprietà → Impostazioni → Grafica** (vale dopo il riavvio; vedi [Altre funzioni](../editor/otras-funciones.md)). Ogni backend si compila nel proprio namespace (`w3dEngine::gl`, `w3dEngine::es2`, `w3dEngine::vk`) e riempie una **tabella di dispatch** (`w3dBackendTabla.h`) con le sue funzioni. Ogni funzione di `w3dEngine` chiama quella del backend attivo:

```cpp
// graphics/w3dDespacho.inc, generado por tools/generar_despacho.py
void Enable(Cap c) { W3D_TABLA()->Enable(c); }
void DrawTriangles(int count, const MeshIndex* indices) { W3D_TABLA()->DrawTriangles(count, indices); }
```

Se quello scelto non funziona su quella macchina, resta OpenGL. Vulkan ha bisogno del driver, della GPU e del compilatore di shader `shaderc_shared` accanto all'eseguibile, ed è disattivato nelle build a 32 bit. Su macOS l'eseguibile include solo OpenGL.

## Il viaggio di un `DrawTriangles`

La stessa richiesta finisce in posti molto diversi:

![Uno stesso gfx::DrawTriangles passa per Whisk3D Core e scende a ogni backend: opengl (PC, macOS, N95, Dreamcast, PS Vita), gles2 (Android, browser, N8, PC), vulkan (PC), gu (PSP) e citro3d (3DS)](img/whisk3d-core-despacho.webp)

- **OpenGL fisso (PC, N95):** quasi diretto a `glDrawElements`. La matrice si invia solo subito prima del disegno, con un solo `glLoadMatrixf`.
- **OpenGL ES 2.0 e WebGL:** lo stack delle matrici vive sulla CPU e viaggia come uniform verso l'uber-shader. WebGL non accetta array nella RAM, quindi il backend li passa in buffer. Sulla GPU VideoCore IV di Android 2.3 costruisce un piccolo programma per ogni combinazione di stato e raggruppa i disegni dell'interfaccia: da 179 per fotogramma si passa a 16.
- **PSP (GE):** la CPU porta ogni vertice nello spazio di clipping e ritaglia lì, perché il GE non ritaglia contro i lati. Poi li scrive interlacciati in un anello di memoria che il GE legge. La luce (fino a quattro) e la nebbia vanno in hardware.
- **3DS (PICA200):** i vertici vengono copiati in memoria lineare e li trasforma un vertex shader proprio (`w3d.v.pica`). I piccoli disegni consecutivi con lo stesso stato escono insieme: da 191 per fotogramma si passa a 18. Le linee e i punti vengono costruiti sulla CPU come triangoli.
- **Dreamcast:** GLdc fa da OpenGL 1.x sopra la PVR e usa lo stesso backend fisso del PC e dell'N95, con patch proprie. Per un gioco che ha bisogno di ogni ciclo c'è `pvr/`: GLdc spende circa 400 cicli dell'SH-4 per vertice, mentre il disegno diretto trasforma ciascuno con una sola istruzione (`FTRV`). Non convivono nello stesso programma.
- **PS Vita:** vitaGL emula la pipeline fissa sulla GPU della console e compila i suoi shader durante l'esecuzione (per questo ha bisogno di `libshacccg.suprx`).

## Chiedere al backend cosa sa fare

Non tutte le GPU sanno fare le stesse cose, e il Core non lo nasconde: ogni backend dice cosa ha, e chi disegna sceglie un'altra strada se manca qualcosa.

| Domanda | Cosa succede se risponde di no |
|---|---|
| `VBOSoportado()` | Le mesh vanno come array nella RAM (Dreamcast, Vita) |
| `EstencilBits()` | Il riflesso dell'acqua ripiega sul ritaglio con i piani |
| `PlanosRecorteMax()` | Il riflesso si arrangia senza piani (la PVR non li ha) |
| `MezclaTexturasSoportada()` | I livelli di un materiale vanno in più passate |
| `TieneTexGen()` | Il riflesso sferico si calcola via software o con la matrice di texture (`TexMatrixMatcap`, che invece funziona sull'N95) |
| `SombreadoPorPixelSoportado()` | La luce resta per vertice e l'editor non offre nemmeno l'opzione |
| `ShadersEscritos()` | Il materiale con uno shader proprio si disegna come sempre (vedi [Shader scritti](../editor/shaders.md)) |

## La cache di stato

Ogni cambio di stato costa qualcosa al driver. Il Core ricorda cosa è impostato: attivare qualcosa che è già attivo, o associare la texture che è già associata, non genera nessuna chiamata. `IsEnabled` e `GetMatrix` rispondono da quella memoria e non interrogano la GPU, perché sull'N95 una domanda del genere obbliga ad aspettare che finisca di disegnare.

## Texture

`LoadTexture` ha due passi. Il **decode** (dal file ai pixel RGBA) dipende dal sistema: stb quasi ovunque, l'ICL su Symbian. L'**upload** (dai pixel alla GPU) è comune, e ogni backend converte nel formato del suo chip:

- **PSP:** potenza di due fino a 512, "swizzled", in 565 o 8888.
- **3DS:** blocchi di 8x8 in ordine Morton, da 8 a 1024 per lato.
- **Dreamcast:** potenza di due da 8 a 1024, a 16 bit (565 o 4444), senza mipmap.
- **N95:** solo potenze di due; le altre vengono ricampionate prima dell'upload.

Un progetto può chiedere texture a **16 bit** (metà della memoria) e, solo sull'N95, caricarle **a metà dimensione** oltre una certa grandezza (`texturasMitadDesde`). Il formato `.w3dt` salva una texture già pronta per la GPU, con le sue mipmap: caricarla significa leggerla e inviarla.

Un dettaglio onesto: l'upload delle texture e le luci chiamano ancora direttamente due o tre funzioni di OpenGL. Sulle console senza OpenGL, il backend include un piccolo strato che le implementa (`gu/w3dGLgu.cpp`, `citro3d/w3dGLc3d.cpp`).

## Lo schermo e l'area sicura

`PantallaSetTam` e `PantallaEsVertical` salvano una sola volta se lo schermo è in verticale o in orizzontale (orizzontale = più largo che alto), così qualsiasi parte del motore può chiederlo. L'**area sicura** (`SafeAreaGet`) dice quanti pixel di ogni bordo non vanno usati per l'interfaccia: il notch, la barra dei gesti, le barre del browser del cellulare. Lo sfondo si disegna su tutta la finestra e l'interfaccia dentro l'area sicura. Sul PC, su Symbian e sulle console i bordi valgono 0; nel browser vengono misurati.

## I limiti del più piccolo

Ciò che si scrive per il Core deve entrare nella macchina più limitata. Questi sono i limiti che si fanno sentire:

| Limite | Dove | Come lo gestisce il Core |
|---|---|---|
| Indici a 16 bit (fino a 65.535 vertici per mesh) | Symbian | `MeshIndex` lì è a 16 bit e altrove a 32; l'importatore rifiuta ciò che non ci sta |
| 8 luci alla volta | Tutti (`LucesMax`) | Il backend a shader le emula con lo stesso calcolo; la PSP ne fa 4 in hardware e Android 2.3 ne usa fino a 4 |
| Niente shader | I backend fissi | Le funzioni degli shader rispondono di no e il materiale si disegna come sempre |
| Z a 16 bit | PSP | Il piano vicino si adatta alla distanza di orbita, come sull'N95 |
| 16 MB per tutto | Dreamcast | Si compila con `-Os` e il linker scarta ciò che non si usa; le texture vanno a 16 bit |
| C++03 | Symbian | Niente `nullptr`, `auto` né `override` negli header (`W3D_OVERRIDE`); `tools/prueba_independencia.sh` compila ogni header da solo, in C++03 |
