# Assi, unità e oggetti

- **3D:** negli script l'asse **Z punta verso l'alto**, come nel pannello Proprietà: X e Y sono il pavimento (la larghezza e la profondità) e Z è l'altezza. Quello che vedi nel pannello è quello che lo script legge e scrive. Posizioni, rotazioni e scale sono relative al genitore dell'oggetto. Le rotazioni sono in **gradi**.
- **2D (schermo e UI):** pixel della tela, con lo (0, 0) al **centro**; +x verso destra e +y verso il **basso**.
- **Oggetti:** le funzioni che cercano o restituiscono oggetti (`yo`, `buscar`, `objeto`…) danno un valore che non ha campi né metodi: lo passi ad altre funzioni. Due oggetti si possono confrontare con `==`. Se l'oggetto è stato distrutto, si comporta come `nil`.
- **Tolleranza:** quasi tutte le funzioni accettano `nil` o un oggetto del tipo sbagliato senza rompere niente: quelle che leggono restituiscono 0, `""` o `false` e quelle che scrivono non fanno nulla.
- **Indici:** come in Lua, quasi tutto parte da 1 (vertici, dita, livelli, passi delle routine). L'eccezione sono le memorie delle routine (`memoria`), che vanno da 0 a 255.
