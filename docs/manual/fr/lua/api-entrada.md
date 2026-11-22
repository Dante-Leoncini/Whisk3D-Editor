# Clavier, manette, tactile et souris

| Fonction | Ce qu'elle fait |
|---|---|
| `key(name) → bool` | La touche est enfoncée (tant que vous la maintenez). |
| `keyPressed(name) → bool` | La touche vient d'être enfoncée à cette image (une seule fois par appui). |
| `button(name [, gamepad])`, `buttonPressed(name [, gamepad])` | La même chose avec un bouton de la manette. Avec `gamepad` (1 à 4), celui de cette manette ; sans lui, celui de n'importe laquelle. |
| `stick(["left"\|"right"] [, gamepad]) → x, y` | Stick de la manette, de -1 à 1 (le haut vaut -1). |
| `trigger(["left"\|"right"] [, gamepad]) → 0..1` | Gâchette analogique. |
| `controls() → n`, `control([n]) → name, tipo` | Le nombre de manettes et leur type (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `tap() → x, y, touching` | Le doigt (ou le clic de la souris) sur l'écran. |
| `finger([n]) → x, y, active` | Multitouch : le doigt numéro n (1 à 4). |
| `touch() → bool` | L'écran est tactile : un téléphone ou une tablette (Android, le navigateur d'un iPhone…) ou tout appareil sur lequel on a déjà touché l'écran. Pour n'afficher les commandes à l'écran que là où elles sont utiles. |
| `tilt() → x, y, z` | L'accéléromètre du téléphone, en g, selon les axes de l'écran tel qu'on le voit (déjà tourné) : `x` indique de combien il est tourné vers la droite, comme un volant (le sinus de l'angle) ; debout et droit, il donne `y = 1`, posé à plat face vers le haut, `z = 1`. `nil` si l'appareil n'a pas d'accéléromètre. |
| `vibrate(ms [, force])` | Vibre pendant `ms` millisecondes avec une `force` de 0 à 1 : le vibreur du téléphone (Android, le navigateur) et le moteur des manettes (PC). Sur un téléphone, la force raccourcit l'impulsion. Ne fait rien là où il n'y a rien pour vibrer, ni si le joueur a désactivé la vibration. |
| `pressed(obj) → bool` | Cet élément 2D a été touché ou cliqué à cette image (boutons). |
| `mouse() → x, y, dentro` | La position de la souris. |
| `captureMouse(bool)`, `mouseRel() → dx, dy`, `mouseCaptured()` | Souris capturée pour regarder autour avec la souris (jeux à la première personne). Sur PC et dans le navigateur ; dans le navigateur, la souris est capturée au premier clic ou à la première touche dans le jeu. <kbd>Esc</kbd> la libère et met le jeu en pause. |
