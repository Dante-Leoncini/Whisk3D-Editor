# Whisk 3D (antiguamente Blendersito)
[![en](https://img.shields.io/badge/lang-en-red.svg)](/)

Whisk3D es una herramienta de creación 3D desarrollada desde cero, con el objetivo de llevar el modelado, render y animación y desarrollo de videojuegos a cualquier dispositivo, incluso aquellos “Obsoletos”.

Nació como un clon liviano de Blender para Nokia, pero hoy es mucho más: un entorno creativo, centrado en la estética low-poly y la identidad visual de PlayStation, Dreamcast y la PC retro. Whisk3D prioriza la velocidad, la accesibilidad y la diversión por encima de la complejidad excesiva.

La licencia MIT permite que cualquiera pueda usarlo libremente para crear juegos, proyectos comerciales, educativos o artísticos, e incluso hacer forks y expandir la herramienta sin restricciones.

Aun esta en pre-alpha, una primera mirada a algo que todavía está creciendo, pero con una visión clara: que cualquier persona pueda modelar, animar, diseñar, renderizar y desarrollar juegos 3D sin necesitar una PC moderna ni enfrentar curvas de aprendizaje intimidantes.
Whisk3D se construye con una filosofía sencilla: el hardware no debería morir por el software. Busco demostrar que la obsolescencia programada puede revertirse desde el código, que una máquina considerada “vieja” puede seguir creando, imaginando y generando contenido nuevo.

El proyecto, y los videojuegos que se desarrollen con él, planean sostenerse mediante donaciones voluntarias, no con microtransacciones, publicidad molesta, loot boxes, ni mecanismos diseñados para generar adicción o manipular al jugador.
Whisk3D NO instala servicios ocultos, no espía al usuario ni vende sus datos, no exige cuentas ni conexión permanente. Respetamos a los creadores y jugadores.

Queremos devolverte el control de tu software, democratizar la creación y permitirle a las nuevas generaciones experimentar con el 3D sin barreras técnicas ni económicas.

![](https://i.ibb.co/hp1w0DH/Captura-desde-2025-11-26-15-19-37.png)

YouTube: [Youtube Video](https://youtu.be/dMe-Vit5OT0)

## Instalacion:
Necesitas tener instalado PyS60, o el programa no se ejecutará, Se ha incluido el instalador in la carpeta "Dependencies" del repositorio. La última versión estará siempre en la carpeta "sis" con el nombre de "Whisk3D_gcce.sisx", aunque la última versión quizás no funcione correctamente, la versión verificada estará siempre en la carpeta "releases" con la fecha de salida y nombre referenciando los videos e imágenes de demostración mostrados en [Instagram](https://www.instagram.com/dante_leoncini?igsh=MWpjYTd5YzU5dTBkNg==). 

Este programa no esta listo para producción y tiene una serie de errores conocidos, se encuentra en constante desarrollo, y se esta constantemente añadiendo o quitando características. Por ejemplo, la opción para modelado 3d ha sido removida en las últimas versiones, pero si usas una versión anterior como "Whisk3D_gcce_beta_24-08-15_FERNET.sisx", el modelado 3d sigue disponible.


Sí el instalador falla, probablemente debas de "hackear" tu teléfono para ignorar las restricciones y certificados de Symbian. Puedes ver el siguiente video mostrando como hacerlo: [Tutorial Hack](https://www.youtube.com/watch?v=UJJICzbk3TA)

## Notas de Linux
Para compilar en Linux 32bit y SDL 1.2 necesitas las siguientes dependencias:
sudo apt install libsdl1.2-dev

necesario para compilar en 32bit
sudo apt install gcc-multilib g++-multilib
sudo apt install libsdl1.2-dev:i386 libgl1-mesa-dev:i386 libglu1-mesa-dev:i386 libc6-dev:i386

Para linux estaba usando SDL3... pero para gente nueva no es lo mejor ya que tienen que clonar el repo. compilar SDl3... y un largo etc. asi que volvi al SDL2 el cual solo necesitan instalar un par de librerias que ya estan en los repositorios de linux y listo.

Se puede compilar Whisk3D desde la carpeta "Linux" usando el comando "make". o "./CompilarYrun.sh" (este ultimo compila y abre Whisk3D)

## Nota para Android
Para compilar. desde el directorio raiz usa
cd platform/android && ./gradlew assembleDebug

## Nota para Windows
Para compilar. 
1) instala MSYS2 desde https://www.msys2.org/
2) instala desde la consola GNU Make + MinGW-W64 con el comando "pacman -S make mingw-w64-x86_64-gcc"
3) instala desde la consola "pacman -S mingw-w64-x86_64-gcc"
4) instala la libreria de SDL2 (a menos que quieras usar SDL3 o SDL1) con "pacman -S mingw-w64-x86_64-SDL2" y "pacman -S mingw-w64-x86_64-SDL2_image"
5) anda a la carpeta "Whisk3D/platform/windows"
6) desde la terminal de MINGW64 (es azul) escribi "make windows"

con eso tenes por defecto la version de 64bit de windwos en "Whisk3D/build/windows/Whisk3D_xxx.exe"

## Aclaraciones:

1. No es un port: Este programa no comparte código con el original.
2. Funcionalidad básica: El programa es básico, y algunos medios han exagerado sus capacidades actuales.
3. Futuras actualizaciones: Está en desarrollo la capacidad para modelar, renderizar, animar, entre otras funciones Las novedades se compartirán en redes sociales.
4. Obsolescencia programada: El objetivo del proyecto es evidenciar la obsolescencia programada de los teléfonos, tanto de hace 18 años como de los actuales. Busco generar una demanda por parte de los consumidores de teléfonos más sostenibles y duraderos, reduciendo la enorme cantidad de residuos electrónicos. también ayudaría a que mas gente acceda a la tecnología gracias al reciclaje de estos antiguos dispositivos.
5. Conocimiento en informática: Queremos resaltar la falta de conocimiento general en informática. No debería sorprender que Whisk 3D funcione en un Nokia N95, un teléfono que en su tiempo era de gama alta, comparable con un iPhone 15 Pro Max de hoy. Quiero dejar claro que con 300 MHz y 128 MB de RAM se pueden realizar tareas que hoy en día requieren teléfonos con 8 GB de RAM y procesadores multicore.
6. Uso del logo de Blender: El logo de Blender pertenece a la Fundación Blender. Me gustaría dialogar con la fundación para dividir el proyecto en dos: uno completamente libre como motor de juegos 3D con su propia identidad, y otro que pueda ser parte de la Fundación Blender, usando su logo y assets para evitar múltiples clones del proyecto (como ocurre con distribuciones de linux). tambien seria interesante ahora si que Whisk 3D empiece a ser compatible con el blender de toda la vida. La idea es que sea algo que sume. "The Freedom to Create". blender hace un gran trabajo en ese camino, Whisk 3D es solo darle la posibilidad a mas gente de entrar a este hermoso mundo.
7. Renderización y trazado de rayos: Whisk 3D ya puede hacer renderizaciones. La renderización, en términos informáticos, es el proceso de generar una imagen a partir de un modelo 2D o 3D. Si bien aún no he implementado el trazado de rayos, el Nokia N95 es capaz de hacerlo mediante su CPU y FPU (Unidad de Punto Flotante). En películas como Terminator 2 y Jurassic Park, se utilizaron computadoras SGI Onyx de 1993. El Nokia N95, lanzado en 2007, tiene 14 años de diferencia y es comparable en cuanto a potencia. Es preocupante que tanta gente se sorprenda de que un teléfono antiguo pueda realizar un render.
8. Desempeño gráfico: Whisk 3D se ve fluido gracias a la aceleración gráfica por hardware del N95 (PowerVR MBX Lite) y su FPU, lo que permite mover modelos 3D complejos como Arthur Morgan de Red Dead Redemption 2.

![](https://pbs.twimg.com/media/GPawZAKWsAA7Rw9?format=png) ![](https://pbs.twimg.com/media/GPawalTWUAAe-J1?format=png)

9. Este proyecto funciona en todos los Nokia s60v3. Sin embargo, muchos teléfonos, incluso modelos lanzados posteriormente, no tienen aceleración gráfica debido a la mala decisión de Nokia de ahorrar dinero eliminando esta característica. Aunque estos teléfonos puedan parecer más rápidos en papel que el N95, en la práctica son más lentos y económicos debido a la falta de aceleración gráfica.

## Sobre Blender
Me indigna bastante la gran cantidad de comentarios negativos hacia Blender, calificándolo de pesado e ineficiente en sus computadoras. Blender es un programa extremadamente potente y, aun así, puede funcionar en versiones de PC bastante humildes. Lo uso desde hace años y es un software que me encanta. Incluso se pueden descargar versiones antiguas de Blender que funcionan muy bien en computadoras antiguas. He utilizado versiones modernas que también funcionan excelentemente en computadoras de hace 10-15 años, solo con la aceleración gráfica del CPU.

Realmente recomiendo usar Linux, instalar un disco SSD en sus PCs, y optimizar sus proyectos de acuerdo a su hardware y necesidades. Verán que se puede trabajar sin problemas con hardware antiguo.

## ¡Gracias!
Gracias a todos los que compartieron el proyecto. Espero que este esfuerzo sirva para aumentar la conciencia y exigir a las empresas que fabriquen teléfonos más duraderos y sostenibles.
#Gracias a Marcig por el nuevo nombre!

(Actualmente no hay soporte para s60v5 o Symbian belle. Pero en teoría no sería difícil de lograr)

[Primera Demo en Video](https://youtu.be/dMe-Vit5OT0)

Fue optimizado para funcionar en un Nokia N95-3 con 128mb de ram.
Puede funcionar en otros Nokia s60v3 con 64mb o menos. pero como la mayoría no tiene aceleración gráfica, no se va a ver tan fluido como en las demos que estoy mostrando.

# Contribuye
Tengo pensado agregar mas funciones. 
Cualquier aporte que quiera realizar al código es Bienvenida.
