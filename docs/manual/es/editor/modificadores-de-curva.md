# Modificadores de curva

Cambian lo que devuelve una curva de animación sin tocar sus keyframes. Se ven en la línea de tiempo y se usan igual en el juego.

Están en **Propiedades → Modificadores**, con una curva activa en la línea de tiempo (la del keyframe activo, o la única curva elegida). La tarjeta lleva el nombre de la curva (por ejemplo "Modificadores de curva - X Location") para no confundirse.

| Botón | Qué hace |
|---|---|
| Agregar | Suma un modificador al final de la pila. |
| Quitar | Saca el modificador elegido. |
| Subir / Bajar | Cambia el orden (con dos o más). |

Abajo, la tarjeta del modificador elegido. Todos tienen **Activado**, **Usar influencia** (cuánto actúa, de 0 a 1) y **Restringir rango de cuadros** (actúa solo entre Inicio y Fin, con entrada y salida graduales). Todo se puede deshacer.

## Ciclos

Repite la curva antes del primer keyframe y después del último.

| Valor | Qué hace |
|---|---|
| Modo antes / Modo después | Sin ciclos, Repetir movimiento (igual), Repetir con desplazamiento (cada vuelta sigue donde terminó la anterior) o Repetir en espejo (una vuelta para adelante y otra para atrás). |
| Repeticiones | Cuántas vueltas; 0 = para siempre. |

Los proyectos de antes que tenían una curva con "Repetir" la abren con un modificador Ciclos que hace lo mismo.

## Ruido

Le suma ruido a la curva (un temblor, una vibración).

| Valor | Qué hace |
|---|---|
| Tipo de mezcla | Reemplazar (alrededor de la curva), Sumar, Restar o Multiplicar. |
| Escala | Qué tan lento cambia (más grande = más suave). |
| Fuerza | Cuánto se mueve. |
| Desplazamiento / Fase | Corren el ruido en el tiempo / lo cambian por otro. |
| Profundidad | Detalle (capas de ruido más finas). |
| Lacunaridad / Rugosidad | Qué tan fina y qué tan fuerte es cada capa. **Ruido clásico** las deja fijas. |

## Escalonado

La curva salta de a un **Tamaño del paso** en frames (animación "de a dos", stop motion). **Desplazamiento** corre los saltos; **Usar cuadro inicial / final** lo limita a un tramo.

## Suave

Suaviza la curva con un filtro: **Ancho del filtro** en frames a cada lado y **Sigma** (qué tanto pesan los frames lejanos).
