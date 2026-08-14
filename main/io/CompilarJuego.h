#ifndef COMPILARJUEGO_H
#define COMPILARJUEGO_H

#include <string>

class UI;

// ============================================================================
//  CompilarJuego — el boton "Compilar juego" del editor. Convierte el PROYECTO
//  (la UI + los scripts lua colgados de los objetos) en un JUEGO que corre SIN
//  el editor, para la plataforma elegida:
//     0 = Linux .deb    1 = Linux AppImage    2 = WebGL    3 = Android (APK)
//
//  Como: exporta el .w3dui, copia los .lua, GENERA el codigo del runtime
//  (un main.cpp que carga la UI + resuelve las referencias, y el CMake /
//  script de build que reusan examples/game/w3drun), y ejecuta la compilacion
//  informando cada paso por notificaciones. En la carpeta del proyecto quedan:
//     platform-build/<plat>/  lo generado para compilar (autocontenido por
//                             plataforma: fuentes + assets + build.log + intermedios)
//     build/<plat>/           SOLO los resultados (ejecutable/.deb, .html/.js/.wasm, .apk)
//  Las dos carpetas se regeneran solas y CompilarJuego asegura un .gitignore
//  del proyecto que las excluye.
//
//  Compilar es INDEPENDIENTE del editor: platform-build/<plat> se puede compilar
//  a mano (cmake/emcc) sin abrir Whisk3D. El editor solo automatiza el proceso.
//
//  'modoVentana' (el desplegable "Modo ventana" de la tarjeta Juego) controla la
//  ventana del juego GENERADO:
//     0 = Ventana (redimensionable)   1 = Pantalla completa   2 = Sin bordes
//  En Web no aplica (manda el canvas); en Android solo cambia si va inmersivo o no.
//
//  'orientacion' (el desplegable "Orientacion") clava la orientacion del juego:
//     0 = Todas   1 = Solo vertical (portrait)   2 = Solo horizontal (landscape)
//  Afecta a Android (screenOrientation del manifest + hint SDL); desktop/web no aplica.
//
//  'usarFisica' / 'usarSonido' (los checkboxes de la tarjeta): destildados, el
//  juego se compila SIN ese subsistema (mas liviano):
//     - sin fisica: no se compila physics/W3dFisica.cpp y se define W3D_SIN_FISICA
//       (los binds velocidad/rebotar/... quedan como stubs no-op, sin crash).
//     - sin sonido: no se compila audio/W3dAudioSDL.cpp ni se define W3D_ENABLE_AUDIO
//       (W3dAudio.cpp queda como dispatcher stub para linkear; beep() es mudo).
//
//  'modoDebug' (el checkbox "Modo debug", default destildado = PRODUCCION):
//  define W3D_DEV_LOG (base/w3dlog.h) en los 4 targets generados.
//     - tildado (debug): W3D_DEV_LOG=1 -> log + ring buffer prendidos, depurar()
//       emite y esDebug() devuelve true en los scripts lua.
//     - destildado (produccion): W3D_DEV_LOG=0 -> w3dLog* son stubs no-op, sin
//       ring (logCantidad() = 0) y depurar() no emite (build mas liviano).
//
//  'empaquetarAssets' (el desplegable "Assets" de la tarjeta Juego):
//     - false (Sueltos, default): como siempre, los archivos del juego (.w3dui,
//       .lua, .png, assets/) viajan SUELTOS al lado del binario / en el APK /
//       embebidos por emcc. Visibles y editables: modding facil.
//     - true (Empaquetados): TODOS los archivos del juego se hornean en un PAK
//       propio ofuscado (XOR de flujo, clave derivada del nombre; ofuscacion
//       anti-curiosos, no criptografia) que se compila DENTRO del binario
//       (pak.cpp generado). El Core (w3dFilesystem) lee primero del pak; los
//       targets NO copian/instalan/embeben archivos sueltos: el .deb queda solo
//       binario+launcher+desktop+iconos, el APK sin assets visibles, el wasm
//       sin --embed-file.
//
//  ASINCRONICO: CompilarJuego valida + exporta + genera TODO en el hilo de UI
//  (rapido: es lo unico que toca la escena) y dispara la COMPILACION (cmake/emcc/
//  ndk/gradle) + la copia final en un HILO WORKER. Devuelve true si el build
//  ARRANCO (no si termino). Mientras corre, el hilo principal lee el estado con
//  las funciones de abajo y dibuja la barra (ProgresoOverlayRender); el worker
//  NUNCA toca la escena ni la UI: solo corre procesos, lee build.log y publica
//  etapa/porcentaje en el estado compartido.
// ============================================================================
bool CompilarJuego(UI* u, int plataforma, int modoVentana, int orientacion,
                   bool usarFisica, bool usarSonido, bool modoDebug,
                   bool empaquetarAssets);

// ============================================================================
//  CUANTAS UNIDADES DE COMPILACION EN PARALELO puede lanzar el editor.
//
//  El "Compilar juego" de Linux corria "cmake --build out -j" con el -j PELADO, o sea
//  PARALELISMO ILIMITADO: ~75 unidades (40 .cpp del Core/UI + ~35 .c de lua) arrancando
//  a la vez, a -std=c++17. En una maquina de 15 GB eso dispara el OOM killer y le cierra
//  TODO al usuario (paso tres veces en la maquina del dueno). Android hacia lo mismo con
//  ndk-build -j$(nproc), y ademas por CADA ABI.
//
//  Regla: la MITAD de los nucleos, minimo 1, tope 8. La mitad porque un g++ con estos
//  headers pide del orden de 1 GB de pico; el tope porque de ahi para arriba no acelera
//  (el link serializa) y si multiplica la RAM. Es UNA sola funcion para que no vuelva a
//  aparecer un -j pelado en otro generador de comandos.
// ============================================================================
int W3dCompilarJobs();
// el flag TAL CUAL va en la linea de comandos ("-j6"). TODO generador de comandos del
// editor (cmake --build, ndk-build) tiene que usar ESTE string: asi no hay forma de que
// vuelva a colarse un "-j" pelado, y el test 'compjobs' lo verifica de una sola vez.
std::string W3dCompilarJobsFlag();

// ---- estado del build asincronico (lo consulta el hilo PRINCIPAL, 1x por frame) ----
bool CompilarJuegoEnCurso();                    // hay un build corriendo (bloquea re-disparo)
int  CompilarJuegoProgreso(std::string* etapa); // avance GLOBAL 0..100 + texto de la etapa actual
void CompilarJuegoTick();                       // 1x por frame en el hilo de UI: mantiene vivo el
                                                // render y, al terminar el worker, hace join +
                                                // Notificar() de exito/error (nunca desde el worker)

#endif // COMPILARJUEGO_H
