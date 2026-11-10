# Clavier, manette, tactile et souris

| Fonction | Ce qu'elle fait |
|---|---|
| `tecla(nombre) → bool` | La touche est enfoncée (tant que vous la maintenez). |
| `teclaApretada(nombre) → bool` | La touche vient d'être enfoncée à cette image (une seule fois par appui). |
| `boton(nombre [, mando])`, `botonApretado(nombre [, mando])` | La même chose avec un bouton de la manette. Avec `mando` (1 à 4), celui de cette manette ; sans lui, celui de n'importe laquelle. |
| `stick(["izq"\|"der"] [, mando]) → x, y` | Stick de la manette, de -1 à 1 (le haut vaut -1). |
| `gatillo(["izq"\|"der"] [, mando]) → 0..1` | Gâchette analogique. |
| `controles() → n`, `control([n]) → nombre, tipo` | Le nombre de manettes et leur type (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `toque() → x, y, tocando` | Le doigt (ou le clic de la souris) sur l'écran. |
| `dedo([n]) → x, y, activo` | Multitouch : le doigt numéro n (1 à 4). |
| `tactil() → bool` | L'écran est tactile : un téléphone ou une tablette (Android, le navigateur d'un iPhone…) ou tout appareil sur lequel on a déjà touché l'écran. Pour n'afficher les commandes à l'écran que là où elles sont utiles. |
| `inclinacion() → x, y, z` | L'accéléromètre du téléphone, en g, selon les axes de l'écran tel qu'on le voit (déjà tourné) : `x` indique de combien il est tourné vers la droite, comme un volant (le sinus de l'angle) ; debout et droit, il donne `y = 1`, posé à plat face vers le haut, `z = 1`. `nil` si l'appareil n'a pas d'accéléromètre. |
| `vibrar(ms [, fuerza])` | Vibre pendant `ms` millisecondes avec une `fuerza` de 0 à 1 : le vibreur du téléphone (Android, le navigateur) et le moteur des manettes (PC). Sur un téléphone, la force raccourcit l'impulsion. Ne fait rien là où il n'y a rien pour vibrer, ni si le joueur a désactivé la vibration. |
| `apretado(obj) → bool` | Cet élément 2D a été touché ou cliqué à cette image (boutons). |
| `raton() → x, y, dentro` | La position de la souris. |
| `capturarRaton(bool)`, `ratonRel() → dx, dy`, `ratonCapturado()` | Souris capturée pour regarder autour avec la souris (jeux à la première personne). Sur PC et dans le navigateur ; dans le navigateur, la souris est capturée au premier clic ou à la première touche dans le jeu. <kbd>Esc</kbd> la libère et met le jeu en pause. |
