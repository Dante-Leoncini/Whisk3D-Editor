# Script Editor (IDE)

Éditeur de texte pour les scripts Lua et les [shaders écrits](shaders.md) (`.glsl`), avec numéros de ligne, coloration syntaxique, guides d'indentation et prise en charge de la ñ et des accents. Tant qu'il a le focus, les lettres s'écrivent (elles ne déclenchent pas les raccourcis de l'éditeur). Sur Android, toucher le texte ouvre le clavier. Sur le N95, on écrit avec le T9.

| Fonction | Description | Raccourci |
|---|---|---|
| Script | Choisir le .lua du projet à modifier, ou Load .lua… (un fichier externe). | Menu Script |
| Save | Enregistre le script. S'il se trouve dans le .w3d, enregistre aussi le projet. | <kbd>Ctrl+S</kbd> ou File → Save |
| Save As… | Un autre nom, dans le projet ou en dehors, dans le dossier de votre choix. | File |
| Save inside the .w3d | Uniquement si le script est externe : le place dans le projet. | File |
| Save outside the .w3d… | Uniquement si le script est interne : le sort dans un fichier ; les objets utilisent alors ce fichier. | File |
| Refresh | Recharge le fichier tel qu'il est sur le disque (ce qui a été modifié avec un autre éditeur). S'il y a des modifications non enregistrées dans l'éditeur, il demande avant de les abandonner (et vous pouvez les récupérer avec <kbd>Ctrl+Z</kbd>). Si le jeu tourne, recharge en direct les objets qui utilisent le script. | Bouton |
| Shaders (.glsl) | S'ouvrent depuis le matériau (le petit bouton de la ligne **Shader**), depuis la Library ou depuis le sélecteur, et sont colorés comme du GLSL. À côté du nom apparaît **Compile** : il compile ce que vous écrivez (si un matériau l'utilise, il l'enregistre et compile ce matériau). Enregistrer un shader le compile dans tous les matériaux qui l'utilisent et indique s'il a compilé, ou l'erreur avec sa ligne. | <kbd>Ctrl+S</kbd> |
| Protection à l'enregistrement | Si le fichier externe a été modifié à l'extérieur depuis son ouverture, Save demande avant de l'écraser. Save recharge aussi le script en direct si le jeu tourne. | Automatique |
| Indentation (Tab) | <kbd>Tab</kbd> complète jusqu'au prochain multiple de 4 ; avec plusieurs lignes sélectionnées, il leur ajoute un niveau et <kbd>Shift+Tab</kbd> le retire. Les lignes verticales marquent chaque niveau ; celle du bloc du curseur est plus claire. | <kbd>Tab</kbd> / <kbd>Shift+Tab</kbd> |
| Menu Edit | Undo, Redo, Cut, Copy, Paste, Find… et Replace… | Bouton Edit |
| Menu Select | Select All. | Bouton Select |
| Undo / Redo | Sur le texte du script (pas sur la scène). | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> ou <kbd>Ctrl+Shift+Z</kbd> |
| Find / Replace | Ouvre un panneau en haut à droite : le texte à chercher, le nombre de correspondances (« 3 of 7 »), Match case, Whole word, Previous / Next, et dans Replace le nouveau texte avec Replace et Replace all (une seule étape d'annulation). Les correspondances sont surlignées dans le code. Le panneau ne bloque pas l'éditeur : avec le pointeur sur le script, vous continuez à écrire. Il se ferme avec Close ou <kbd>Esc</kbd>. | <kbd>Ctrl+F</kbd> / <kbd>Ctrl+H</kbd> |
| Copy / Cut / Paste | Avec le presse-papiers du système. | <kbd>Ctrl+C</kbd> / <kbd>Ctrl+X</kbd> / <kbd>Ctrl+V</kbd> |
| Select All / Undo | Undo agit sur le texte, pas sur la scène. | <kbd>Ctrl+A</kbd> / <kbd>Ctrl+Z</kbd> |
| Sélectionner au doigt | Un double toucher sélectionne le mot, un triple toucher la ligne entière. Avec une sélection, deux poignées (cercles verts) apparaissent à ses extrémités : faites-les glisser pour l'ajuster. | Touchers |
| Loupe (tactile) | Laisser le doigt immobile un instant sur le code affiche au-dessus du doigt la ligne agrandie avec le curseur ; en faisant glisser, le curseur suit le doigt et reste là où vous le relâchez. | Appui long |
| Sélectionner sur le N95 | Les flèches déplacent le curseur ; en maintenant la touche crayon, les flèches sélectionnent à partir de la position du curseur. Tant que la touche crayon est maintenue, une barre apparaît en bas : la touche de sélection gauche **copie** et la droite **colle**. | Crayon + flèches / softkeys |
| Se déplacer dans le texte | Flèches, Home, End, PageUp, PageDown (avec <kbd>Shift</kbd>, elles sélectionnent) ; <kbd>Ctrl+Home</kbd> / <kbd>Ctrl+End</kbd> vont au début ou à la fin. | Clavier |
