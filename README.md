# Whisk 3D

<p align="center">
    <img src="logo_outlined.svg" width="400" alt="Whisk3D logo">
</p>

## Motor multiplataforma 2D y 3D estilo retro

Whisk3D es una herramienta de creación 2D/3D estilo retro, con el objetivo de llevar el modelado, render, animación y desarrollo de videojuegos a cualquier dispositivo, incluso aquellos “Obsoletos”.

Nació como un clon liviano de Blender para Nokia, pero hoy es mucho más: un entorno creativo, centrado en la estética low-poly y la identidad visual de PlayStation, Dreamcast y la PC retro. Whisk3D prioriza la velocidad, la accesibilidad y la diversión por encima de la complejidad excesiva.

## ¡Software Libre!

La licencia MIT permite que cualquiera pueda usarlo libremente para crear juegos, proyectos comerciales, educativos o artísticos, e incluso hacer forks y expandir la herramienta sin restricciones.

**Aun esta en pre-alpha**, una primera mirada a algo que todavía está creciendo, pero con una visión clara: que cualquier persona pueda modelar, animar, diseñar, renderizar y desarrollar juegos 3D sin necesitar una PC moderna ni enfrentar curvas de aprendizaje intimidantes.
Whisk3D se construye con una filosofía sencilla: **el hardware no debería morir por el software**. Busco demostrar que la obsolescencia programada puede revertirse desde el código, que una máquina considerada “vieja” puede seguir creando, imaginando y generando contenido nuevo.

El proyecto, y los videojuegos que se desarrollen con él, planean sostenerse mediante donaciones voluntarias, **no con microtransacciones, publicidad molesta, loot boxes, ni mecanismos diseñados para generar adicción o manipular al jugador.**
Whisk3D **NO** instala servicios ocultos, no espía al usuario ni vende sus datos, no exige cuentas ni conexión permanente. **Respetamos a los creadores y jugadores**.

Queremos **devolverte el control de tu software**, democratizar la creación y permitirle a las nuevas generaciones experimentar con el 3D ****sin barreras técnicas ni económicas**.

![](https://i.ibb.co/hp1w0DH/Captura-desde-2025-11-26-15-19-37.png)

YouTube: [Youtube Video](https://youtu.be/dMe-Vit5OT0)

## Compilar e instalar

Whisk3D corre en Linux, Windows, Mac, Android, Symbian y Web actualmente. Las instrucciones de cada sistema están en:

- [Windows](platform/windows/README.md) · [GNU/Linux](platform/linux/README.md) · [macOS](platform/mac/README.md)
- [Web (WebGL)](platform/web/README.md) · [Android](platform/android/README.md) · [Symbian s60v3/s60v5/Symbian^3 (N95/N8)](platform/symbian/README.md)

En todos, primero: `git submodule update --init --recursive` (los scripts `platform/<sistema>/build_*` ya lo hacen por vos).

## Aclaraciones:

1. No es un port: Este programa **no comparte** código con Blender.
2. Obsolescencia programada: El objetivo del proyecto **es evidenciar la obsolescencia programada** de los teléfonos, antiguos como actuales. Busco generar una demanda por parte de los consumidores de teléfonos más sostenibles y duraderos, reduciendo la enorme cantidad de residuos electrónicos. también ayudaría a que mas gente acceda a la tecnología gracias al reciclaje de estos antiguos dispositivos.
3. Conocimiento en informática: Queremos resaltar la falta de conocimiento general en informática. No debería sorprender que Whisk 3D funcione en un Nokia N95, un teléfono que en su tiempo era de gama alta, comparable con un iPhone 17 Pro Max de hoy. Quiero dejar claro que con 300 MHz y 128 MB de RAM se pueden realizar tareas que hoy en día requieren teléfonos con 8 GB de RAM y procesadores multicore.
4. Renderización y trazado de rayos: Whisk 3D ya puede hacer renderizaciones. La renderización, en términos informáticos, es el proceso de generar una imagen a partir de un modelo 2D o 3D. Si bien aún no he implementado el trazado de rayos, el Nokia N95 es capaz de hacerlo mediante su CPU y FPU (Unidad de Punto Flotante). En películas como Terminator 2 y Jurassic Park, se utilizaron computadoras SGI Onyx de 1993. El Nokia N95, lanzado en 2007, tiene 14 años de diferencia y es comparable en cuanto a potencia. Es preocupante que tanta gente se sorprenda de que un teléfono antiguo pueda realizar un render.
5. Desempeño gráfico: Whisk 3D se ve fluido gracias a la **aceleración gráfica** por hardware del N95 (PowerVR MBX Lite) y su FPU, lo que permite mover modelos 3D complejos como Arthur Morgan de Red Dead Redemption 2.

![](https://pbs.twimg.com/media/GPawZAKWsAA7Rw9?format=png) ![](https://pbs.twimg.com/media/GPawalTWUAAe-J1?format=png)

## ¡Gracias!

Gracias a todos los que compartieron el proyecto. y a la gente que esta empezando a contribuir con el codigo
Gracias a Marcig por el nuevo nombre!

# ¡Participa!

Tengo pensado agregar mas funciones. 
Cualquier aporte que quiera realizar al código es Bienvenida.
