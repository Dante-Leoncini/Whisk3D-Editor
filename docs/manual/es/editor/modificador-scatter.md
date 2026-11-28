# Dispersar (Scatter)

El modificador **Dispersar** reparte copias de otros objetos sobre la malla: las piedras de un suelo, el pasto de un
jardín, los tornillos de un panel. Es como un sistema de partículas que instancia objetos, sin la física: las copias
quedan quietas donde caen. Las piedras se modelan una vez (en la escena, en una colección escondida o en otro `.w3d`) y
el modificador las desparrama.

## Cómo se agrega

1. Elegí la malla que hace de suelo (el **emisor**).
2. En Propiedades, abrí la pestaña **Modificadores** (el ícono de la llave).
3. Tocá **Añadir** y elegí **Scatter**.
4. En **Fuente**, elegí de dónde salen las copias.

## De dónde salen las copias (Fuente)

| Fuente | Qué copia |
|---|---|
| Objeto | Un objeto de la escena, con sus hijos. |
| Colección | Los objetos de una colección: cada copia elige **uno al azar**. La colección puede estar oculta (el ojo apagado): las copias se ven igual. |
| Escena | Los objetos de otra escena del proyecto (cada copia elige uno). |
| .w3d externo | Los objetos de una escena de **otro archivo `.w3d`** (una librería): en **Librería** se elige uno ya vinculado o **Link .w3d...** para vincular uno nuevo, y en **Escena** cuál (si no se elige, la primera). Así todas las piedras viven en un solo archivo que usan todos los niveles. |

Cada copia lleva el giro y la escala de su objeto fuente, pero **no su lugar**: nace en el punto de la malla. Por eso la
colección de piedras puede estar escondida en un rincón de la escena.

## Parámetros

| Parámetro | Qué hace |
|---|---|
| Desde | De dónde de la malla salen las copias: **Vértices** (una en cada vértice), **Aristas** (en cualquier punto de las aristas, parejo por largo), **Centros de caras** (una en el centro de cada cara) o **Caras (cualquier punto)** (al azar sobre la superficie, parejo por área: una cara del doble de grande recibe el doble). |
| Cantidad | Cuántas copias. En Vértices y Centros de caras, si alcanza, una en cada uno; si no, las que se pidan, elegidas al azar. |
| Semilla | El azar. La misma semilla da siempre las mismas copias, en cualquier máquina; otra semilla, otro reparto. |
| Escala | El tamaño de cada copia. |
| Escala al azar | De 0 a 1: cuánto se achican al azar (1 = de 0 a la Escala). Cambiarla no mueve las copias de lugar. |
| Alinear a la normal | Prendido, cada copia se para sobre la superficie (en una ladera queda inclinada como la ladera). Apagado, todas derechas. |
| Giro al azar | De 0 a 1: cuánto gira cada copia alrededor de su "arriba" (1 = cualquier ángulo). |
| Inclinación al azar | Grados: cuánto se puede inclinar cada copia hacia un costado. |
| Corrimiento | Metros sobre su "arriba": negativo, las entierra un poco (piedras medio enterradas). |
| Unida \| Instancias | El modo (ver abajo). |
| Mostrar emisor | Apagado, el suelo no se dibuja: solo las copias (el emisor queda como molde). |

## Unida o Instancias

- **Unida:** las copias se suman a la malla del emisor. Se dibuja todo junto (una llamada de dibujo por material) pero
  cada copia gasta su memoria. Recibe y proyecta sombras, entra en los lightmaps y en el trazado de rayos como cualquier
  malla. Es lo mejor para muchas copias chicas en una PC.
- **Instancias:** cada copia dibuja la malla de su objeto fuente en otro lugar. No gasta memoria por copia (todas
  usan la misma malla), pero cada una es una llamada de dibujo. Proyecta sombras en tiempo real. Es lo mejor cuando
  hay pocas copias de una malla pesada, o cuando falta memoria.

## Aplicar

- En **Unida**, **Aplicar Modificador** deja todo en la malla del emisor (con sus quads y los materiales de las piedras).
- En **Instancias**, cada copia pasa a ser un objeto (que comparte la malla con su fuente, como un Alt+D), adentro de
  una colección nueva `<emisor>_Scatter`. Con un `.w3d` externo no se puede: aplicá en modo Unida, o traé las piedras
  al proyecto.

Las dos se deshacen con Ctrl+Z.
