# Modos de pintura

> [!NOTE]
> Los tres modos de pintura son **solo para mallas 3D**: no aparecen con cámaras, luces, esqueletos, curvas ni elementos 2D. Mientras se pinta, el click no cambia la selección; para pintar otro objeto se lo elige en el Outliner.

## Lo común a los tres

| Función | Descripción | Cómo se usa |
|---|---|---|
| Pintar | Pinta solo las caras de frente que se ven (con Rayos X, también las de atrás). Cada trazo es un paso de deshacer. | Click y arrastrar |
| Radio | El tamaño del pincel en píxeles. Al cambiarlo aparece un círculo en el centro de la vista con el tamaño real. | Barra arrastrable sobre la barra de herramientas |
| Valor | La intensidad del pincel. | Barra arrastrable |
| Caída | Cómo se suaviza el borde del pincel; se puede dibujar una curva propia. | Botón de caída |
| Vista (táctil) | Prendido: el dedo navega. Apagado: el dedo pinta. | Botón con forma de monitor |

## Pintura de pesos

Pinta cuánto influye un hueso en cada vértice (azul = nada, rojo = todo). Es lo que hace que la malla se deforme bien con el esqueleto.

| Función | Descripción |
|---|---|
| Modo del pincel | Sumar (+), Restar (−) o Igualar (=, pone el valor exacto). |
| Grupo de vértices | Qué grupo (hueso) se pinta; se pueden agregar grupos. Si no hay ninguno, el primer trazo crea uno. |
| Marcas | Muestra un cuadradito por vértice; el pincel actúa solo sobre ellos. |
| Solo lo seleccionado | Protege lo no seleccionado (se ve oscurecido). |

## Pintura de vértices

Pinta colores sobre los vértices de una **malla 3D** (no sirve para imágenes ni elementos 2D: para eso está la pintura de textura). Es muy liviana y es la forma recomendada de dar color en el N95.

| Función | Descripción |
|---|---|
| Color | Abre el selector de color (RGB, HSV, hexadecimal o un índice de la paleta). |
| Por vértice / por esquina | Por vértice, el color se comparte entre las caras; por esquina, cada cara puede tener el suyo (bordes duros). |
| Capa de color | Elegir o crear capas de color. |
| Marcas / Solo lo seleccionado | Igual que en pintura de pesos. |

## Pintura de textura

Pinta directamente sobre el objeto 3D y el trazo va a parar a su textura.

| Función | Descripción |
|---|---|
| Textura automática | Si la malla no tiene textura, al primer trazo se crea sola: un material propio, UV automáticas y una imagen de 1024×1024 (256×256 en el N95) del color del material. |
| Color, caída, radio, valor | Como en los otros modos. |
| Guardar textura | La pintura vive en memoria hasta que se guarda. El diskette se pone verde si hay cambios sin guardar. Opciones: Guardar, Guardar como…, Guardar en el w3d y Guardar fuera del w3d… |
