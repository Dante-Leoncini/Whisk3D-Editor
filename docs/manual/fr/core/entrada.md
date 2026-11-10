# Entrées

Sur presque tous les systèmes, **SDL2** fournit le clavier, la souris, les doigts et les manettes, et l'éditeur comme les jeux sont écrits sur ces événements. Le Core complète ce que SDL ne couvre pas :

| Quoi | Comment ça arrive |
|---|---|
| Tactile | SDL sur Android, dans le navigateur, sur la Vita et la 3DS. Sous Symbian, `W3dTactil` fait la même conversion que SDL (le premier doigt déplace une souris, les autres sont signalés comme des doigts) : un doigt sur S60 5th, multitouch sur le N8 |
| Inclinaison | Sur le N95, la Sensor API de Nokia, chargée au démarrage (sans elle, le jeu fonctionne quand même). Sur Android et sur PC, elle arrive par SDL |
| Manettes | SDL ailleurs. Sous Symbian, les manettes PS3 et PS4 en Bluetooth (le pilote bthid-s60), dans le même ordre de boutons que SDL. Jusqu'à 4 |
| Vibration | Android, avec le vibreur du téléphone. Navigateur, `navigator.vibrate` (sur iOS, le « tic » du système grâce à une astuce). Bureau : rien. L'API promet peu : « une petite vibration, si possible » |
| Presse-papiers | SDL (y compris celui du navigateur) ; `CClipboard` sous Symbian |

Sur les consoles sans écran tactile (PSP, Dreamcast), la manette déplace un curseur, comme une souris.

Pourquoi c'est important : les gestes tactiles de Symbian passent par le même chemin que ceux d'Android, donc une amélioration des gestes sur Android arrive d'elle-même sur le téléphone. Depuis Lua : [Clavier, manette, tactile et souris](../lua/api-entrada.md).
