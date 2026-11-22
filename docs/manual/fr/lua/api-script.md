# Le script et la communication

| Fonction | Ce qu'elle fait |
|---|---|
| `self() → obj` | L'objet propriétaire de ce script. |
| `object(prop) → obj \| nil` | L'objet choisi dans le champ `prop` de `properties`. |
| `option(prop) → text` | L'option choisie dans une liste déroulante de `properties` ; si aucune n'a été choisie, la première de la liste (celle par défaut). |
| `property(prop) → value` | Le nombre, le oui/non ou le texte propre à cet objet (ou la valeur par défaut déclarée). |
| `param(prop [, def]) → número` | Comme `property`, mais toujours un nombre ; s'il n'existe pas, renvoie `def` (ou 0). |
| `target() → obj \| nil` | La cible (« target ») de l'objet propriétaire. |
| `getShared(clave) → value` | Lit l'espace partagé par tous les scripts. |
| `setShared(clave, value)` | Écrit un nombre, un oui/non ou un texte dans l'espace partagé ; `nil` l'efface. |
