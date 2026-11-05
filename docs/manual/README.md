# Manuales de Whisk3D (fuente)

Los manuales están en Markdown, una página por tema, sin estilo: el diseño lo pone la página web.

- `es/editor/`: manual del editor (para usuarios).
- `es/lua/`: programar juegos con Lua.
- `es/tutoriales/`: tutoriales paso a paso, con capturas del editor.

Cada carpeta tiene un `README.md` que es el **índice**: el orden de las páginas y la navegación de la web salen de ahí. Para agregar una página, crear el `.md` y sumarlo al `README.md` de su carpeta.

## Convenciones

- Cada página empieza con un solo título `# ...`.
- Teclas: `<kbd>Ctrl+Z</kbd>`.
- Notas y consejos, como en GitHub: `> [!NOTE]` y `> [!TIP]`.
- Links entre páginas: al archivo `.md`, por ejemplo `[Atajos](atajos.md)` o `[Lua](../lua/README.md)`. Para ir a un título, agregar `#ancla` al estilo GitHub (`modo-edicion.md#modelar`).
- Dentro de una tabla, el carácter `|` se escribe `\|` (también dentro de `código`).
- Las tablas de `es/editor/atajos.md` que están entre `<!--ATAJOS-INICIO-->` y `<!--ATAJOS-FIN-->` las genera `tools/genmanual_atajos.py` desde el código: no se editan a mano.

## Capturas de los tutoriales

Las imágenes de `<idioma>/tutoriales/img/` las genera `tools/capturas/capturas.py` manejando el editor (ver `tools/capturas/README.md`): **no se editan a mano**. Si cambia la interfaz, se regeneran. El editor está en castellano, inglés y portugués, así que hay capturas en `es/`, `en/` y `pt/`; las páginas en otros idiomas usan las de `en/`, y la web las busca ahí.

## Traducciones

Copiar `es/` a otra carpeta de idioma (`en/`, `pt/`…) con los mismos nombres de archivo y traducir el contenido. Si falta una página traducida, la web muestra la versión en castellano.
