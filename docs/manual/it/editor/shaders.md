# Shader scritti

Uno **shader scritto** è un file di testo (`.glsl`) che decide come si disegna un materiale, pixel per pixel. Serve per effetti che il materiale normale non ha: vetro smerigliato che lascia vedere sfocato ciò che sta dietro, acqua che si muove, distorsione da calore, stile cartone animato, un bagliore che pulsa…

Scrivi solo la tua parte: una funzione `Fragmento()` (il colore di ogni pixel) e, se vuoi, una `Vertice()` (per spostare i vertici). Tutto il resto lo aggiunge Whisk3D.

## Dove funziona

Lo stesso file funziona allo stesso modo su tutti i backend grafici:

| Backend | `@vertice` | `@fragmento` |
|---|---|---|
| OpenGL ES 2.0: Android, browser (WebGL), Nokia N8, PC con **Grafica → OpenGL ES 2.0** | Sulla GPU | Sulla GPU |
| Vulkan (PC) | Sulla GPU | Sulla GPU |
| OpenGL del PC (quello predefinito), OpenGL ES 1.1 (Nokia N95), Nintendo 3DS, Dreamcast | **Sulla CPU** | No: il materiale si disegna come sempre |

Su un backend senza shader non si rompe niente: il **`@vertice` gira sulla CPU** (l'acqua con le onde si muove allo stesso modo sull'N95) e il colore viene dal materiale normale. La scheda Materiale lo segnala: *"Questo backend grafico non ha shader: la sezione @vertice gira sulla CPU e il resto del materiale si disegna come sempre"*.

### Il `@vertice` sulla CPU

Dove non ci sono shader, Whisk3D include un interprete che legge lo **stesso testo** del `@vertice` e lo esegue vertice per vertice prima di disegnare. Non bisogna scrivere niente di diverso. Capisce ciò che serve per spostare i vertici:

- `float`, `int`, `bool`, `vec2`, `vec3`, `vec4`; costanti (`const float VELOCIDAD = 2.0;`) e funzioni tue (con `in`, `out`, `inout`);
- `if` / `else`, `for` (con `break` e `continue`), `return`, l'operatore ternario `? :`;
- gli operatori, le componenti (`pos.y`, `n.xz`, anche in scrittura) e i costruttori (`vec3(...)`);
- le funzioni numeriche di GLSL: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `pow`, `exp`, `log`, `sqrt`, `abs`, `sign`, `floor`, `ceil`, `fract`, `mod`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `length`, `distance`, `dot`, `cross`, `normalize`, `reflect`, `refract`;
- `w3d_tiempo`, `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`.

Non capisce matrici, array, `while` né `#define`: se li usi, l'errore lo dice con la sua riga (e sulla GPU funzionerebbe). Gira una volta per fotogramma su tutti i vertici della mesh: sull'N95 conviene usarlo su mesh piccole (qualche centinaio di vertici, come l'acqua d'esempio). Funziona se **tutte** le parti della mesh usano lo stesso shader; altrimenti la mesh si disegna senza muoversi.

> **Perché lo stesso file funziona ovunque:** il file non ha niente di specifico di un backend. Ogni backend ci aggiunge davanti la sua parte (gli ingressi `w3d_*`, la precisione, il `main`). OpenGL ES 2.0 lo compila direttamente; Vulkan lo converte in SPIR-V internamente. Se domani Whisk3D aggiunge un altro backend, il tuo shader continua a funzionare.

## Usare uno shader in un materiale

1. Seleziona l'oggetto e apri la scheda **Materiale** di Proprietà.
2. In fondo alla scheda, spunta **Avanzato: shader personalizzato**. Se il materiale non ha ancora un file, se ne crea uno nuovo (`shader.glsl`) con un modello, viene usato subito e si apre nell'IDE.
3. In **Shader** scegli il file:
   - i `.glsl` già presenti nel progetto;
   - **Nuovo shader**: un altro file con il modello;
   - **Carica dal disco...**: un `.glsl` del tuo computer. Viene copiato dentro il progetto, come le texture e gli script.
4. Il pulsantino a destra di **Shader** apre il file nell'**IDE** (serve un IDE nel layout).
5. Scrivi, e **Ctrl+S** salva e compila. Puoi anche premere **Compila** nel materiale o nella barra dell'IDE (compare accanto al nome del file quando è un `.glsl`).
6. Sotto **Compila** compare il risultato: **Compilato**, oppure l'errore in rosso con la sua riga.

Non serve salvare il progetto per cominciare. Se non l'hai ancora salvato, lo shader viene scritto in una cartella di lavoro dell'editor; quando salvi il progetto entra dentro il `.w3d`, nella cartella `shaders/`, e il materiale e l'IDE passano a usare quella copia. Da lì viaggia con il progetto (sul telefono, nel gioco compilato, su un altro computer). Se togli la spunta, il materiale torna a disegnarsi come sempre; il file resta selezionato.

Se uno shader non compila, l'oggetto si disegna con il suo materiale normale finché non lo correggi. L'editor non va in crash.

### Dalla Libreria: trascinare uno shader su una mesh

Gli shader del progetto compaiono nella **Libreria** (l'Outliner in modalità Libreria), in **Shader**, con quanti oggetti li usano. Da lì:

- **Trascinalo su una mesh** nella Vista 3D: mentre lo muovi, la **parte della mesh** sotto il puntatore si disegna con quello shader, così vedi a quale parte lo applicherai. Se passi a un'altra parte o esci dalla mesh, torna al suo materiale. Al rilascio, il materiale di quella parte passa a usare lo shader (con il materiale predefinito, la parte riceve un materiale nuovo). Si annulla con **Ctrl+Z**.
- Lo stesso con un **materiale**: mentre lo trascini, la parte si vede con quello.
- **Apri nell'IDE**, rinominarlo, spostarlo di cartella o portarlo fuori dal `.w3d`, come uno script.
- Un `.glsl` trascinato **dal file explorer del tuo computer** nella finestra entra nel progetto; se lo rilasci su una mesh, viene anche applicato a quella parte.

## Com'è fatto il file

```glsl
// i commenti si scrivono con // o /* */

@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    // facoltativo: spostare i vertici
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
```

- **`@fragmento`** segna dove inizia il codice dei pixel e **`@vertice`** quello dei vertici. Entrambi sono facoltativi: un file senza segni è tutto fragment.
- **`vec4 Fragmento()`** è obbligatoria: restituisce il colore del pixel (rosso, verde, blu, alfa; da 0 a 1).
- **`void Vertice(inout vec3 pos, inout vec3 normal)`** è facoltativa. Riceve la posizione e la normale del vertice nello spazio dell'**oggetto** (Y verso l'alto, in metri) e puoi modificarle.
- Puoi scrivere funzioni e costanti tue (`const float VELOCIDAD = 2.0;`), sopra il punto in cui le usi.
- Il linguaggio è **GLSL ES 1.00**, quello di WebGL 1. Hai `float`, `vec2/3/4`, `mat3/4`, `sin`, `cos`, `mix`, `clamp`, `pow`, `dot`, `normalize`, `reflect`, `refract`, `fract`, `smoothstep` e il resto di GLSL.
- **Non** scrivere `main` e non dichiarare `uniform`, `attribute`, `varying` o `precision`: li aggiunge Whisk3D.
- Gli errori indicano la riga del **tuo file** (per esempio `0(4)` o `w3d.frag:4` è la riga 4).

## Cosa hai a disposizione

### In `Fragmento()`

| Nome | Tipo | Cos'è |
|---|---|---|
| `w3d_uv` | `vec2` | Le coordinate UV del punto (del primo livello UV). |
| `w3d_Normal()` | `vec3` | La normale del punto, già normalizzata, nello spazio della camera. Se si vede la faccia posteriore, punta dall'altra parte. |
| `w3d_normal` | `vec3` | La normale non normalizzata né invertita. |
| `w3d_posVista` | `vec3` | La posizione del punto nello spazio della camera (la camera è in 0,0,0 e guarda verso −Z). |
| `w3d_HaciaCamara()` | `vec3` | La direzione dal punto verso la camera. |
| `w3d_HaciaLuz()` | `vec3` | La direzione dal punto verso la prima luce. |
| `w3d_Luz(n)` | `vec3` | La luce che arriva con la normale `n`: l'ambiente più la prima luce (diffusa). |
| `w3d_luzColor` | `vec4` | Il colore della prima luce (con la sua intensità). |
| `w3d_luzPos` | `vec4` | La posizione della prima luce nello spazio della camera (`w` = 0: è una direzione, il sole). |
| `w3d_ambiente` | `vec4` | La luce ambiente. |
| `w3d_color` | `vec4` | Il **Colore** del materiale (con il suo alfa). |
| `w3d_emision` | `vec4` | L'**Emissione** del materiale. |
| `w3d_Textura(uv)` | `vec4` | La texture del materiale in quelle UV (bianco se non ne ha). La `v` parte dall'**alto** dell'immagine (0 = la riga in alto, 1 = quella in basso). |
| `w3d_colorVertice` | `vec4` | Il colore dei vertici (se la mesh ce l'ha e il materiale usa **Colore vertice**). |
| `w3d_rugosidad`, `w3d_metalico` | `float` | La **Rugosità** e il **Metallico** del materiale. |
| `w3d_ior`, `w3d_transmision` | `float` | L'**IOR** e la **Trasmissione** del materiale (quelli del ray tracing). |
| `w3d_tiempo` | `float` | I secondi, per animare. Nell'editor è il tempo della **timeline** (il fotogramma attuale ÷ gli FPS): in pausa resta fermo, con **Play** avanza e trascinando la timeline vedi qualsiasi momento. Nel gioco esportato è il tempo dall'avvio. |
| `w3d_PuntoPantalla()` | `vec2` | Dove si trova il pixel nella vista: (0,0) in basso a sinistra, (1,1) in alto a destra. |
| `w3d_Pixel()` | `vec2` | La dimensione di un pixel nelle stesse unità. |
| `w3d_Pantalla(p)` | `vec4` | **Ciò che è già disegnato** nel punto `p` della vista. Vedi sotto. |
| `w3d_AMundo(v)` | `vec3` | Porta una direzione dallo spazio della camera a quello del **mondo** (Y verso l'alto). Serve per leggere una mappa d'ambiente che non ruoti con la camera (vedi *Riflesso dell'ambiente*). |

### In `Vertice()`

`pos` e `normal` (dell'oggetto), e `w3d_tiempo`.

## Vedere ciò che sta dietro: `w3d_Pantalla`

Un materiale che legge lo schermo con `w3d_Pantalla` può mostrare ciò che sta dietro l'oggetto, e spostarlo, sfocarlo o colorarlo. Così si fanno il vetro, l'acqua, l'aria calda o un campo di forza.

- Le mesh con uno shader che legge lo schermo si disegnano **per ultime**, dopo il resto della scena, così ciò che sta dietro è già disegnato. Non importa dove siano nell'Outliner.
- Prima di ognuna, la vista viene copiata in una texture (una copia per mesh e per parte di mesh). Su un PC o su un telefono moderno costa poco. Su un telefono vecchio, meglio pochi oggetti così.
- Le **particelle** si disegnano dopo, quindi non si vedono attraverso il vetro.
- Conviene che il materiale **non** sia **Trasparente**: lo shader mescola già ciò che sta dietro e restituisce alfa 1.

## Esempi

Tutti questi esempi sono file in **`examples/shaders/`** del repository, con le loro texture in `examples/shaders/texturas/`, e applicati a oggetti nel progetto **`examples/shaders/Ejemplos_Shaders.w3d`**: aprilo e provali. Puoi anche trascinare i `.glsl` nel tuo progetto.

| File | Cosa fa |
|---|---|
| `vidrio_esmerilado.glsl` | Vetro per qualsiasi forma: ciò che sta dietro sfocato e piegato. |
| `vidrio_plano.glsl` | Vetro smerigliato per un **piano** (una finestra, un box doccia); la texture dice dove è smerigliato. |
| `reflejo_entorno.glsl` | Metallo o plastica che riflette una foto panoramica dell'ambiente (la texture). |
| `reflejo_matcap.glsl` | Il riflesso più economico: una texture con una sfera dipinta. |
| `latido.glsl` | Un bagliore che pulsa (`w3d_tiempo`). |
| `dibujo_animado.glsl` | La luce in tre toni. |
| `agua.glsl` | Onde con `@vertice` e ciò che sta sotto spostato. |
| `bandera.glsl` | Una bandiera che sventola (solo `@vertice`: funziona anche sull'N95). |
| `respirar.glsl` | Si gonfia e si sgonfia lungo le sue normali (solo `@vertice`). |
| `torcer.glsl` | Si torce attorno all'asse verticale (solo `@vertice`). |
| `saltar.glsl` | Rimbalza e si schiaccia toccando il pavimento (solo `@vertice`). |

Quelli sui vertici sono applicati a mesh piccole in **`examples/shaders/Ejemplos_Vertex_N95.w3d`**, pensato per l'N95 (dove il `@vertice` gira sulla CPU). Si muovono con il **Play** della timeline.

### Un colore che pulsa

```glsl
vec4 Fragmento() {
    float latido = 0.5 + 0.5 * sin(w3d_tiempo * 4.0);
    vec3 luz = w3d_Luz(w3d_Normal());
    return vec4(w3d_color.rgb * luz + vec3(1.0, 0.2, 0.1) * latido * 0.5, 1.0);
}
```

Uno shader che usa `w3d_tiempo` si muove con il **Play** della timeline (come un'animazione); in pausa resta fermo e l'editor non ridisegna inutilmente. Nel gioco esportato si muove da solo.

### Cartone animato (tre toni)

```glsl
vec4 Fragmento() {
    float l = dot(w3d_Normal(), w3d_HaciaLuz());
    float tono = l > 0.6 ? 1.0 : (l > 0.1 ? 0.65 : 0.35);
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * tono, base.a);
}
```

### Acqua che si muove (`@vertice`)

```glsl
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    float ola = sin(pos.x * 2.0 + w3d_tiempo * 2.0) * 0.1 + sin(pos.z * 3.0 + w3d_tiempo * 1.5) * 0.05;
    pos.y += ola;
}

@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 p = w3d_PuntoPantalla() + n.xy * 0.03;          // ciò che sta sotto, un po' spostato
    vec3 fondo = w3d_Pantalla(p).rgb;
    float borde = pow(1.0 - max(dot(n, w3d_HaciaCamara()), 0.0), 3.0);
    return vec4(mix(fondo * w3d_color.rgb, vec3(0.8, 0.9, 1.0), borde * 0.6), 1.0);
}
```

Perché l'onda si veda, il piano ha bisogno di vertici: suddividilo in Modalità Modifica.

### Vetro smerigliato

È `examples/shaders/vidrio_esmerilado.glsl` (la sfera di vetro del progetto di esempi). Ciò che sta dietro si vede sfocato e un po' piegato:

```glsl
@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // la rifrazione: si legge lo schermo spostato secondo la normale
    vec2 p = w3d_PuntoPantalla() + n.xy * (w3d_ior - 1.0) * 0.15;

    // la sfocatura: la media di 13 letture intorno; la rugosità allarga il cerchio
    vec2 r = w3d_Pixel() * (1.0 + 18.0 * w3d_rugosidad);
    vec3 c = w3d_Pantalla(p).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 2.5,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  2.5)).rgb;
    c += w3d_Pantalla(p + r * vec2(-2.5, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0, -2.5)).rgb;
    c /= 13.0;

    vec3 atras = c * mix(vec3(1.0), w3d_color.rgb, 0.35);     // tinto dal colore del vetro
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 col = mix(superficie, atras, w3d_transmision);

    // il riflesso della luce e il bordo più chiaro
    vec3 medio = normalize(w3d_HaciaLuz() + ojo);
    float brillo = pow(max(dot(n, medio), 0.0), 80.0) * (1.0 - w3d_rugosidad);
    float borde = pow(1.0 - max(dot(n, ojo), 0.0), 3.0);
    col += w3d_luzColor.rgb * brillo + vec3(0.2) * borde;
    return vec4(col, 1.0);
}
```

Si controlla dal materiale: **Trasmissione** (0 = vetro opaco del colore del materiale, 1 = si vede tutto ciò che sta dietro), **IOR** (quanto si piega ciò che sta dietro) e **Rugosità** (quanto si sfoca). Sono gli stessi valori che usa il **ray tracing**, quindi in Anteprima render lo stesso oggetto è vetro vero.

### Vetro piano smerigliato

Per un **piano**: una finestra, un box doccia, uno schermo di vetro. È `examples/shaders/vidrio_plano.glsl`. Fa la media di 17 letture di ciò che sta dietro su due anelli; la **Rugosità** allarga gli anelli. La **texture** del materiale dice *dove* è smerigliato: bianco = sfocato, nero = liscio. Con `texturas/escarcha.png` restano aloni, una finestrella pulita e gocce che scendono; senza texture, tutto il vetro è smerigliato. Conviene togliere il **Culling** al piano per vederlo da entrambi i lati.

```glsl
float esmerilado = w3d_Textura(w3d_uv).r * w3d_rugosidad;   // quanto si sfoca QUESTO punto
vec2 r = w3d_Pixel() * (1.0 + 26.0 * esmerilado);           // il raggio delle letture
```

### Riflesso dell'ambiente (come si usa una texture per riflettere)

È `examples/shaders/reflejo_entorno.glsl`. La **texture del materiale** è una foto **panoramica** di ciò che circonda l'oggetto: un'immagine *equirettangolare*, larga il doppio di quanto è alta, che copre 360° intorno e 180° dall'alto in basso (come `texturas/entorno.png`: cielo, sole, palazzi e un pavimento a scacchi). Per ogni pixel:

1. `reflect(-ojo, n)` dice dove **rimbalza lo sguardo**, nello spazio della camera.
2. `w3d_AMundo(...)` porta quella direzione nel **mondo**, così il riflesso resta fermo anche se la camera ruota.
3. `Panorama(...)` converte la direzione in un punto della foto: l'angolo intorno (`atan`) è la `u` e l'angolo dall'alto in basso (`acos`) è la `v`.
4. Si mescola con la superficie secondo **Fresnel** (di taglio riflette di più) e il **Metallico** (un metallo riflette tutto, tinto del suo **Colore**).

```glsl
@fragmento
const float PI = 3.14159265;

vec2 Panorama(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;      // il giro intorno (0..1)
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;       // dall'alto (0) al basso (1)
    return vec2(u, v);
}

vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();
    vec3 r = reflect(-ojo, n);
    vec3 entorno = w3d_Textura(Panorama(normalize(w3d_AMundo(r)))).rgb;
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(n, ojo), 0.0), 5.0);
    float cuanto = mix(fresnel, 1.0, w3d_metalico);
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 reflejo = entorno * mix(vec3(1.0), w3d_color.rgb, w3d_metalico);
    return vec4(mix(superficie, reflejo, cuanto), 1.0);
}
```

Va bene qualsiasi foto panoramica 2:1 (quelle a 360° di un telefono, o le HDRI convertite in PNG).

### Riflesso matcap

È `examples/shaders/reflejo_matcap.glsl`: una sola lettura di texture, funziona su qualsiasi GPU. La texture è una **sfera dipinta** (`texturas/matcap.png`); ogni pixel prende il colore del punto della sfera rivolto dalla stessa parte della sua normale. Il riflesso segue la camera (non il mondo).

```glsl
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 uv = vec2(n.x, -n.y) * 0.49 + 0.5;   // (la v della texture parte dall'alto: y al contrario)
    return vec4(w3d_Textura(uv).rgb * w3d_color.rgb, 1.0);
}
```

## In una routine: il passo **Shader**

Uno shader scritto è come uno **stato di GL**: lo si attiva e ciò che si disegna dopo lo usa. In una **routine** (Aggiungi → Draw → **Shader**) il passo ha il **file** `.glsl`, la spunta **On** e quattro numeri: **IOR**, **Transmission**, **Roughness** e **Metallic** (arrivano come `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`; si possono animare o leggere da una memoria).

```
Shader          vidrio_plano.glsl  On   (IOR 1.5, Roughness 0.6)
Vertex pointer  Ventana
Draw elements   Ventana
Shader          Off
Draw elements   Marco           ← ormai senza shader
```

Con **On**, i "Draw elements" che seguono usano lo shader; togliendo la spunta lo si toglie. Alla fine della routine si toglie da solo. Funziona in **OpenGL ES 2.0 e Vulkan**; in OpenGL ES 1.1 e nel GL fisso il passo non fa niente (i disegni di una routine puntano direttamente ai dati della mesh: il `@vertice` sulla CPU è per i materiali). Uno shader che legge lo schermo (`w3d_Pantalla`) vede ciò che la routine e la scena hanno disegnato **prima** di quel passo: l'ordine lo decidi tu.

## Con il ray tracing

Il ray tracing non esegue il tuo shader. Usa i valori del materiale: **Colore**, **Rugosità**, **Metallico**, **Trasmissione**, **IOR** e la texture. Se nello shader usi quegli stessi valori (come il vetro smerigliato), Anteprima materiale e Anteprima render dicono la stessa cosa.

**Trasmissione** (da 0 a 1, nella scheda Materiale con il ray tracing attivo) dice quanta parte della superficie è vetro: la luce passa piegata dall'**IOR** e, con **Rugosità**, esce smerigliata. L'**alfa** è un'altra cosa: ciò che è trasparente per l'alfa (o per la sua mappa) non c'è, e lì si vede ciò che sta dietro dritto, senza piegarsi. Con alfa al 50 % e trasmissione 1, metà passa dritta e l'altra metà rifratta. Con trasmissione 0, l'IOR piega ciò che si vede attraverso l'alfa, come nelle versioni precedenti.

## Cosa non fa (ancora)

- Usa una sola luce: la prima della scena (o quella dell'anteprima).
- Non applica la **nebbia** né i **piani di taglio** degli specchi.
- In **Solido** e nelle viste di debug (normali, alfa, vettori) si vede il materiale normale.
- Non ha parametri propri: usa quelli del materiale (colore, rugosità, metallicità, transmission, IOR, emissione).

## Errori comuni

| Messaggio | Cosa succede |
|---|---|
| `undefined variable` / `undeclared identifier` | Un nome scritto male o che non esiste (`w3d_Normal()` con le parentesi?). |
| `cannot convert` / `wrong operand types` | Tipi mescolati: `1` è un `int`, scrivi `1.0`. Un `vec3` non si somma con un `vec4`. |
| `Fragmento: no matching overloaded function` o non compila senza `Fragmento` | Manca la funzione `vec4 Fragmento()` o restituisce un altro tipo. |
| `redefinition of main` | Non scrivere `main`: lo aggiunge Whisk3D. |
| Non si vede ciò che sta dietro | Lo shader usa `w3d_Pantalla` ma il materiale è **Trasparente** con alfa basso, oppure ciò che sta dietro si disegna dopo (particelle). |
