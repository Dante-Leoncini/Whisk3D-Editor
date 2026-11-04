# Shaders écrits

Un **shader écrit** est un fichier texte (`.glsl`) qui décide comment un matériau est dessiné, pixel par pixel. Il sert aux effets que le matériau ordinaire n'a pas : du verre dépoli qui laisse voir flou ce qui est derrière, de l'eau qui bouge, une distorsion de chaleur, un rendu dessin animé, une lueur qui pulse…

Vous n'écrivez que votre partie : une fonction `Fragmento()` (la couleur de chaque pixel) et, si vous le souhaitez, une `Vertice()` (déplacer les sommets). Whisk3D ajoute tout le reste.

## Où ça marche

Le même fichier marche de la même façon sur tous les backends graphiques :

| Backend | `@vertice` | `@fragmento` |
|---|---|---|
| OpenGL ES 2.0 : Android, navigateur (WebGL), Nokia N8, PC avec **Graphics → OpenGL ES 2.0** | Sur le GPU | Sur le GPU |
| Vulkan (PC) | Sur le GPU | Sur le GPU |
| OpenGL du PC (celui par défaut), OpenGL ES 1.1 (Nokia N95), Nintendo 3DS, Dreamcast | **Sur le CPU** | Non : le matériau est dessiné comme d'habitude |

Sur un backend sans shaders, rien ne casse : le **`@vertice` s'exécute sur le CPU** (l'eau avec des vagues bouge de la même façon sur le N95) et la couleur vient du matériau normal. La carte Material le signale : *"This graphics backend has no shaders: the @vertice section runs on the CPU and the rest of the material is drawn as usual"* (ce backend graphique n'a pas de shaders : la section @vertice s'exécute sur le CPU et le reste du matériau est dessiné comme d'habitude).

### Le `@vertice` sur le CPU

Là où il n'y a pas de shaders, Whisk3D embarque un interpréteur qui lit le **même texte** du `@vertice` et l'exécute sommet par sommet avant de dessiner. Inutile d'écrire quoi que ce soit de différent. Il comprend ce qui sert à déplacer des sommets :

- `float`, `int`, `bool`, `vec2`, `vec3`, `vec4` ; les constantes (`const float VELOCIDAD = 2.0;`) et vos propres fonctions (avec `in`, `out`, `inout`) ;
- `if` / `else`, `for` (avec `break` et `continue`), `return`, le ternaire `? :` ;
- les opérateurs, les composantes (`pos.y`, `n.xz`, aussi en écriture) et les constructeurs (`vec3(...)`) ;
- les fonctions numériques de GLSL : `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `pow`, `exp`, `log`, `sqrt`, `abs`, `sign`, `floor`, `ceil`, `fract`, `mod`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `length`, `distance`, `dot`, `cross`, `normalize`, `reflect`, `refract` ;
- `w3d_tiempo`, `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`.

Il ne comprend ni les matrices, ni les tableaux, ni `while`, ni `#define` : si vous les utilisez, l'erreur l'indique avec sa ligne (et sur le GPU, ça marcherait). Il s'exécute une fois par image sur tous les sommets du maillage : sur le N95, mieux vaut l'utiliser sur de petits maillages (quelques centaines de sommets, comme l'eau d'exemple). Il fonctionne si **toutes** les parties du maillage utilisent le même shader ; sinon, le maillage est dessiné sans bouger.

> **Pourquoi le même fichier marche partout :** le fichier ne contient rien de propre à un backend. Chaque backend ajoute devant ce qui lui est propre (les entrées `w3d_*`, la précision, le `main`). OpenGL ES 2.0 le compile directement ; Vulkan le convertit en SPIR-V en interne. Si demain Whisk3D ajoute un autre backend, votre shader servira toujours.

## Utiliser un shader dans un matériau

1. Sélectionnez l'objet et ouvrez l'onglet **Material** des Properties.
2. Tout en bas, cochez **Advanced: custom shader**. Si le matériau n'a pas encore de fichier, un nouveau (`shader.glsl`) est créé à partir d'un modèle, utilisé tout de suite et ouvert dans l'IDE.
3. Dans **Shader**, vous choisissez le fichier :
   - les `.glsl` déjà présents dans le projet ;
   - **New shader** : un autre fichier avec le modèle ;
   - **Load from disk...** : un `.glsl` de votre ordinateur. Il est copié dans le projet, comme les textures et les scripts.
4. Le petit bouton à droite de **Shader** ouvre le fichier dans l'**IDE** (il faut un IDE dans le layout).
5. Vous écrivez, et **Ctrl+S** enregistre et compile. Vous pouvez aussi appuyer sur **Compile** dans le matériau ou dans la barre de l'IDE (il apparaît à côté du nom du fichier quand c'est un `.glsl`).
6. Sous **Compile** s'affiche le résultat : **Compiled**, ou l'erreur en rouge avec sa ligne.

Pas besoin d'enregistrer le projet pour commencer. S'il n'est pas encore enregistré, le shader est écrit dans un dossier de travail de l'éditeur ; quand vous enregistrez le projet, il passe dans le `.w3d`, dans le dossier `shaders/`, et le matériau et l'IDE utilisent alors cette copie. Dès lors, il voyage avec le projet (sur le téléphone, dans le jeu compilé, sur un autre ordinateur). Si vous décochez la case, le matériau est de nouveau dessiné comme d'habitude ; le fichier reste sélectionné.

Si un shader ne compile pas, l'objet est dessiné avec son matériau normal jusqu'à ce que vous le corrigiez. L'éditeur ne plante pas.

### Depuis la Library : glisser un shader sur un maillage

Les shaders du projet apparaissent dans la **Library** (l'Outliner en mode Library), sous **Shaders**, avec le nombre d'objets qui les utilisent. De là :

- **Glissez-le sur un maillage** dans le 3D Viewport : pendant que vous le déplacez, la **partie du maillage** sous le pointeur est dessinée avec ce shader, pour que vous voyiez à quelle partie vous allez l'appliquer. Si vous passez sur une autre partie ou sortez du maillage, elle retrouve son matériau. Au relâchement, le matériau de cette partie utilise le shader (avec le matériau par défaut, la partie reçoit un nouveau matériau). **Ctrl+Z** l'annule.
- Pareil avec un **matériau** : pendant que vous le glissez, la partie s'affiche avec lui.
- **Open in IDE**, le renommer, le changer de dossier ou le sortir du `.w3d`, comme un script.
- Un `.glsl` glissé **depuis l'explorateur de fichiers de votre ordinateur** dans la fenêtre entre dans le projet ; si vous le relâchez sur un maillage, il est en plus appliqué à cette partie.

## À quoi ressemble le fichier

```glsl
// les commentaires s'écrivent avec // ou /* */

@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    // facultatif : déplacer les sommets
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
```

- **`@fragmento`** marque le début du code des pixels et **`@vertice`** celui des sommets. Les deux sont facultatives : un fichier sans marque est entièrement du fragment.
- **`vec4 Fragmento()`** est obligatoire : elle renvoie la couleur du pixel (rouge, vert, bleu, alpha ; de 0 à 1).
- **`void Vertice(inout vec3 pos, inout vec3 normal)`** est facultative. Elle reçoit la position et la normale du sommet dans l'espace de l'**objet** (Y vers le haut, en mètres) et vous pouvez les modifier.
- Vous pouvez écrire vos propres fonctions et constantes (`const float VELOCIDAD = 2.0;`), au-dessus de l'endroit où vous les utilisez.
- Le langage est le **GLSL ES 1.00**, celui de WebGL 1. Vous avez `float`, `vec2/3/4`, `mat3/4`, `sin`, `cos`, `mix`, `clamp`, `pow`, `dot`, `normalize`, `reflect`, `refract`, `fract`, `smoothstep` et le reste du GLSL.
- **N'écrivez pas** de `main`, et ne déclarez ni `uniform`, ni `attribute`, ni `varying`, ni `precision` : c'est Whisk3D qui les ajoute.
- Les erreurs indiquent la ligne de **votre fichier** (par exemple `0(4)` ou `w3d.frag:4`, c'est la ligne 4).

## Ce que vous pouvez utiliser

### Dans `Fragmento()`

| Nom | Type | Ce que c'est |
|---|---|---|
| `w3d_uv` | `vec2` | Les coordonnées UV du point (de la première couche UV). |
| `w3d_Normal()` | `vec3` | La normale du point, déjà normalisée, dans l'espace de la caméra. Si l'on voit la face arrière, elle pointe de l'autre côté. |
| `w3d_normal` | `vec3` | La normale ni normalisée ni retournée. |
| `w3d_posVista` | `vec3` | La position du point dans l'espace de la caméra (la caméra est en 0,0,0 et regarde vers −Z). |
| `w3d_HaciaCamara()` | `vec3` | La direction du point vers la caméra. |
| `w3d_HaciaLuz()` | `vec3` | La direction du point vers la première lumière. |
| `w3d_Luz(n)` | `vec3` | La lumière reçue avec la normale `n` : l'ambiante plus la première lumière (diffuse). |
| `w3d_luzColor` | `vec4` | La couleur de la première lumière (avec son intensité). |
| `w3d_luzPos` | `vec4` | La position de la première lumière dans l'espace de la caméra (`w` = 0 : c'est une direction, le soleil). |
| `w3d_ambiente` | `vec4` | La lumière ambiante. |
| `w3d_color` | `vec4` | La **Color** du matériau (avec son alpha). |
| `w3d_emision` | `vec4` | L'**Emission** du matériau. |
| `w3d_Textura(uv)` | `vec4` | La texture du matériau à ces UV (blanc s'il n'en a pas). Le `v` commence **en haut** de l'image (0 = la ligne du haut, 1 = celle du bas). |
| `w3d_colorVertice` | `vec4` | La couleur de sommet (si le maillage en a et que le matériau utilise **Vertex color**). |
| `w3d_rugosidad`, `w3d_metalico` | `float` | La **Roughness** et le **Metallic** du matériau. |
| `w3d_ior`, `w3d_transmision` | `float` | L'**IOR** et la **Transmission** du matériau (ceux du Ray Tracing). |
| `w3d_tiempo` | `float` | Les secondes, pour animer. Dans l'éditeur, c'est le temps de la **timeline** (l'image actuelle ÷ les FPS) : en pause il reste figé, avec **Play** il avance, et en faisant glisser la timeline vous voyez n'importe quel instant. Dans le jeu exporté, c'est le temps écoulé depuis le lancement. |
| `w3d_PuntoPantalla()` | `vec2` | Où se trouve le pixel dans la vue : (0,0) en bas à gauche, (1,1) en haut à droite. |
| `w3d_Pixel()` | `vec2` | La taille d'un pixel dans ces mêmes unités. |
| `w3d_Pantalla(p)` | `vec4` | **Ce qui est déjà dessiné** au point `p` de la vue. Voir plus bas. |
| `w3d_AMundo(v)` | `vec3` | Passe une direction de l'espace de la caméra à celui du **monde** (Y vers le haut). Sert à lire une carte d'environnement qui ne tourne pas avec la caméra (voir *Reflet d'environnement*). |

### Dans `Vertice()`

`pos` et `normal` (de l'objet), et `w3d_tiempo`.

## Voir ce qu'il y a derrière : `w3d_Pantalla`

Un matériau qui lit l'écran avec `w3d_Pantalla` peut montrer ce qu'il y a derrière l'objet, et le décaler, le flouter ou le teinter. C'est ainsi qu'on fait le verre, l'eau, l'air chaud ou un champ de force.

- Les maillages avec un shader qui lit l'écran sont dessinés **en dernier**, après le reste de la scène, pour que ce qui est derrière soit déjà dessiné. Peu importe où ils se trouvent dans l'Outliner.
- Avant chacun, la vue est copiée dans une texture (une copie par maillage et par partie de maillage). Sur un PC ou un téléphone récent, c'est peu coûteux. Sur un vieux téléphone, mieux vaut peu d'objets de ce genre.
- Les **particules** sont dessinées après, elles ne se voient donc pas à travers le verre.
- Il vaut mieux que le matériau **ne soit pas** **Transparent** : le shader mélange déjà ce qui est derrière et renvoie un alpha de 1.

## Exemples

Tous ces exemples existent comme fichiers dans **`examples/shaders/`** du dépôt, avec leurs textures dans `examples/shaders/texturas/`, et appliqués à des objets dans le projet **`examples/shaders/Ejemplos_Shaders.w3d`** : ouvrez-le et essayez-les. Vous pouvez aussi glisser les `.glsl` dans votre projet.

| Fichier | Ce qu'il fait |
|---|---|
| `vidrio_esmerilado.glsl` | Du verre pour n'importe quelle forme : ce qui est derrière, flou et dévié. |
| `vidrio_plano.glsl` | Du verre dépoli pour un **plan** (une fenêtre, une paroi vitrée) ; la texture indique où il est dépoli. |
| `reflejo_entorno.glsl` | Du métal ou du plastique qui reflète une photo panoramique de l'environnement (la texture). |
| `reflejo_matcap.glsl` | Le reflet le moins coûteux : une texture avec une sphère peinte. |
| `latido.glsl` | Une lueur qui pulse (`w3d_tiempo`). |
| `dibujo_animado.glsl` | La lumière en trois tons. |
| `agua.glsl` | Des vagues avec `@vertice` et ce qui est dessous décalé. |
| `bandera.glsl` | Un drapeau qui flotte (`@vertice` seul : marche aussi sur le N95). |
| `respirar.glsl` | Il se gonfle et se dégonfle le long de ses normales (`@vertice` seul). |
| `torcer.glsl` | Il se tord autour de l'axe vertical (`@vertice` seul). |
| `saltar.glsl` | Il rebondit et s'écrase en touchant le sol (`@vertice` seul). |

Les exemples de sommets sont appliqués à de petits maillages dans **`examples/shaders/Ejemplos_Vertex_N95.w3d`**, pensé pour le N95 (où le `@vertice` s'exécute sur le CPU). Ils bougent avec le **Play** de la timeline.

### Une couleur qui pulse

```glsl
vec4 Fragmento() {
    float latido = 0.5 + 0.5 * sin(w3d_tiempo * 4.0);
    vec3 luz = w3d_Luz(w3d_Normal());
    return vec4(w3d_color.rgb * luz + vec3(1.0, 0.2, 0.1) * latido * 0.5, 1.0);
}
```

Un shader qui utilise `w3d_tiempo` bouge avec le **Play** de la timeline (comme une animation) ; en pause il reste figé et l'éditeur ne redessine pas inutilement. Dans le jeu exporté, il bouge tout seul.

### Dessin animé (trois tons)

```glsl
vec4 Fragmento() {
    float l = dot(w3d_Normal(), w3d_HaciaLuz());
    float tono = l > 0.6 ? 1.0 : (l > 0.1 ? 0.65 : 0.35);
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * tono, base.a);
}
```

### De l'eau qui bouge (`@vertice`)

```glsl
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    float ola = sin(pos.x * 2.0 + w3d_tiempo * 2.0) * 0.1 + sin(pos.z * 3.0 + w3d_tiempo * 1.5) * 0.05;
    pos.y += ola;
}

@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 p = w3d_PuntoPantalla() + n.xy * 0.03;          // ce qui est dessous, un peu décalé
    vec3 fondo = w3d_Pantalla(p).rgb;
    float borde = pow(1.0 - max(dot(n, w3d_HaciaCamara()), 0.0), 3.0);
    return vec4(mix(fondo * w3d_color.rgb, vec3(0.8, 0.9, 1.0), borde * 0.6), 1.0);
}
```

Pour que la vague se voie, le plan a besoin de sommets : subdivisez-le en Edit Mode.

### Verre dépoli

C'est `examples/shaders/vidrio_esmerilado.glsl` (la sphère de verre du projet d'exemples). Ce qui est derrière apparaît flou et un peu déformé :

```glsl
@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // la réfraction : on lit l'écran décalé selon la normale
    vec2 p = w3d_PuntoPantalla() + n.xy * (w3d_ior - 1.0) * 0.15;

    // le flou : la moyenne de 13 lectures autour ; la rugosité élargit le cercle
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

    vec3 atras = c * mix(vec3(1.0), w3d_color.rgb, 0.35);     // teinté par la couleur du verre
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 col = mix(superficie, atras, w3d_transmision);

    // le reflet de la lumière et le bord plus clair
    vec3 medio = normalize(w3d_HaciaLuz() + ojo);
    float brillo = pow(max(dot(n, medio), 0.0), 80.0) * (1.0 - w3d_rugosidad);
    float borde = pow(1.0 - max(dot(n, ojo), 0.0), 3.0);
    col += w3d_luzColor.rgb * brillo + vec3(0.2) * borde;
    return vec4(col, 1.0);
}
```

Il se règle depuis le matériau : **Transmission** (0 = verre opaque de la couleur du matériau, 1 = on voit tout ce qui est derrière), **IOR** (à quel point ce qui est derrière est dévié) et **Roughness** (à quel point c'est flou). Ce sont les mêmes valeurs que celles du **Ray Tracing**, donc en Render Preview le même objet est du vrai verre.

### Verre plat dépoli

Pour un **plan** : une fenêtre, une paroi vitrée, un écran de verre. C'est `examples/shaders/vidrio_plano.glsl`. Il fait la moyenne de 17 lectures de ce qui est derrière sur deux anneaux ; la **Roughness** élargit les anneaux. La **texture** du matériau indique *où* il est dépoli : blanc = flou, noir = lisse. Avec `texturas/escarcha.png`, on obtient des traces, une petite fenêtre propre et des gouttes qui coulent ; sans texture, tout le verre est dépoli. Mieux vaut désactiver le **Culling** du plan pour le voir des deux côtés.

```glsl
float esmerilado = w3d_Textura(w3d_uv).r * w3d_rugosidad;   // à quel point CE point est flou
vec2 r = w3d_Pixel() * (1.0 + 26.0 * esmerilado);           // le rayon des lectures
```

### Reflet d'environnement (comment utiliser une texture pour refléter)

C'est `examples/shaders/reflejo_entorno.glsl`. La **texture du matériau** est une photo **panoramique** de ce qui entoure l'objet : une image *équirectangulaire*, deux fois plus large que haute, qui couvre 360° tout autour et 180° de haut en bas (comme `texturas/entorno.png` : ciel, soleil, immeubles et un sol en damier). Pour chaque pixel :

1. `reflect(-ojo, n)` indique dans quelle direction **rebondit le regard**, dans l'espace de la caméra.
2. `w3d_AMundo(...)` passe cette direction dans le **monde**, pour que le reflet reste immobile même si la caméra tourne.
3. `Panorama(...)` convertit la direction en un point de la photo : l'angle autour (`atan`) est le `u` et l'angle de haut en bas (`acos`) est le `v`.
4. On mélange avec la surface selon **Fresnel** (de biais, ça reflète davantage) et le **Metallic** (un métal reflète tout, teinté de sa **Color**).

```glsl
@fragmento
const float PI = 3.14159265;

vec2 Panorama(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;      // le tour complet (0..1)
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;       // du haut (0) vers le bas (1)
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

N'importe quelle photo panoramique 2:1 convient (les photos 360° d'un téléphone, ou des HDRI converties en PNG).

### Reflet matcap

C'est `examples/shaders/reflejo_matcap.glsl` : une seule lecture de texture, il marche sur n'importe quel GPU. La texture est une **sphère peinte** (`texturas/matcap.png`) ; chaque pixel prend la couleur du point de la sphère orienté du même côté que sa normale. Le reflet suit la caméra (pas le monde).

```glsl
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 uv = vec2(n.x, -n.y) * 0.49 + 0.5;   // (le v de la texture commence en haut : y inversé)
    return vec4(w3d_Textura(uv).rgb * w3d_color.rgb, 1.0);
}
```

## Dans une routine : l'étape **Shader**

Un shader écrit est comme un **état GL** : on le met, et ce qui est dessiné ensuite l'utilise. Dans une **routine** (Add → Draw → **Shader**), l'étape a le **fichier** `.glsl`, la case **On** et quatre nombres : **IOR**, **Transmission**, **Roughness** et **Metallic** (ils arrivent comme `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico` ; on peut les animer ou les lire depuis une mémoire).

```
Shader          vidrio_plano.glsl  On   (IOR 1.5, Roughness 0.6)
Vertex pointer  Ventana
Draw elements   Ventana
Shader          Off
Draw elements   Marco           ← désormais sans shader
```

Avec **On**, les « Draw elements » qui suivent utilisent le shader ; case décochée, il est retiré. À la fin de la routine, il est retiré automatiquement. Ça marche en **OpenGL ES 2.0 et Vulkan** ; en OpenGL ES 1.1 et en GL fixe, l'étape ne fait rien (les dessins d'une routine pointent directement sur les données du maillage : le `@vertice` sur le CPU est réservé aux matériaux). Un shader qui lit l'écran (`w3d_Pantalla`) voit ce que la routine et la scène ont dessiné **avant** cette étape : c'est vous qui décidez de l'ordre.

## Avec le ray tracing

Le Ray Tracing n'exécute pas votre shader. Il utilise les valeurs du matériau : **Color**, **Roughness**, **Metallic**, **Transmission**, **IOR** et la texture. Si votre shader utilise ces mêmes valeurs (comme le verre dépoli), Material Preview et Render Preview montrent la même chose.

**Transmission** (de 0 à 1, dans la carte Material avec le Ray Tracing activé) indique quelle part de la surface est du verre : la lumière la traverse déviée par l'**IOR** et, avec **Roughness**, ressort dépolie. L'**alpha** est autre chose : ce qui est transparent par l'alpha (ou par sa texture) n'existe pas, et à travers on voit ce qui est derrière en ligne droite, sans déviation. Avec un alpha de 50 % et une transmission de 1, la moitié passe tout droit et l'autre moitié est réfractée. Avec une transmission de 0, l'IOR dévie ce qu'on voit à travers l'alpha, comme dans les versions précédentes.

## Ce qu'il ne fait pas (encore)

- Il n'utilise qu'une lumière : la première de la scène (ou celle de l'aperçu).
- Il ne prend pas en compte le **brouillard** ni les **plans de découpe** des miroirs.
- En **Solid** et dans les vues de débogage (normales, alpha, vecteurs), c'est le matériau normal qui s'affiche.
- Il n'a pas de paramètres propres : il utilise ceux du matériau (couleur, rugosité, métallique, transmission, IOR, émission).

## Erreurs courantes

| Message | Ce qui se passe |
|---|---|
| `undefined variable` / `undeclared identifier` | Un nom mal écrit ou qui n'existe pas (`w3d_Normal()` avec ses parenthèses ?). |
| `cannot convert` / `wrong operand types` | Mélange de types : `1` est un `int`, écrivez `1.0`. Un `vec3` ne s'additionne pas avec un `vec4`. |
| `Fragmento: no matching overloaded function` ou ça ne compile pas sans `Fragmento` | La fonction `vec4 Fragmento()` manque ou renvoie un autre type. |
| `redefinition of main` | N'écrivez pas de `main` : c'est Whisk3D qui l'ajoute. |
| On ne voit pas ce qui est derrière | Le shader utilise `w3d_Pantalla` mais le matériau est en **Transparent** avec un alpha faible, ou ce qui est derrière est dessiné après (particules). |
