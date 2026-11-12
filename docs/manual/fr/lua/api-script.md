# Le script et la communication

| Fonction | Ce qu'elle fait |
|---|---|
| `yo() → obj` | L'objet propriétaire de ce script. |
| `objeto(prop) → obj \| nil` | L'objet choisi dans le champ `prop` de `propiedades`. |
| `opcion(prop) → texto` | L'option choisie dans une liste déroulante de `propiedades` ; si aucune n'a été choisie, la première de la liste (celle par défaut). |
| `propiedad(prop) → valor` | Le nombre, le oui/non ou le texte propre à cet objet (ou la valeur par défaut déclarée). |
| `parametro(prop [, def]) → número` | Comme `propiedad`, mais toujours un nombre ; s'il n'existe pas, renvoie `def` (ou 0). |
| `objetivo() → obj \| nil` | La cible (« target ») de l'objet propriétaire. |
| `compartido(clave) → valor` | Lit l'espace partagé par tous les scripts. |
| `setCompartido(clave, valor)` | Écrit un nombre, un oui/non ou un texte dans l'espace partagé ; `nil` l'efface. |
