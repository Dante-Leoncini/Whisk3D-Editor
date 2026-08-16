#ifndef GUARDARW3D_H
#define GUARDARW3D_H
#include <string>

// ============================================================================
//  GuardarW3D — guarda el PROYECTO como .w3d.
//
//  FORMATO v4: EL .w3d ES UN ARCHIVO. Un ZIP estandar ESTILO OPENDOCUMENT (se
//  abre con cualquier descompresor) con el proyecto ENTERO adentro:
//
//      mimetype           PRIMERA entrada y SIN COMPRIMIR: los 36 bytes
//                         "application/vnd.whisk3d.proyecto+zip". Como el local
//                         header mide 30 bytes fijos, el contenido cae en el
//                         offset 38, que es donde file(1) busca la firma
//      LEEME.txt          le explica el arbol al humano que abra el zip. Se
//                         REGENERA en cada guardado y NUNCA se lee
//      proyecto.json      el MISMO JSON de siempre, legible y editable a mano
//                         (lo unico que cambio es que ahora vive adentro del zip)
//      escenas/           los .w3dui
//      scripts/           los .lua internos
//      texturas/  fuentes/  sonidos/  videos/
//      mallas/            la geometria (.w3dm propio; .glb en los archivos viejos)
//      animaciones/       los blobs de vertex anim
//      modelos/  proyecto/  extra/
//      EXTERNOS.txt       solo si hay alguna referencia "ext:"
//
//  Se acabaron los archivos sueltos: contenido/, modelos/*.glb, vtxanim/*.bin y
//  los .w3dui hermanos ya no se escriben. Abrir y guardar tocan UN SOLO ARCHIVO.
//  Los formatos viejos (JSON plano v3, zip v2 y el texto Whisk3D{}) se siguen
//  ABRIENDO y se MIGRAN en el primer guardado; NUNCA se borra nada de lo viejo.
//
//  REFERENCIA INTERNA vs EXTERNA: interna = ruta pelada, que es un nombre de
//  entrada del contenedor ("texturas/pausa.png"); externa = prefijo "ext:". Los
//  externos NO se copian: se guarda la ruta, se listan en EXTERNOS.txt y si
//  falta alguno se avisa CLARO. Una referencia rota se CONSERVA (un pendrive
//  sacado no puede convertirse en perdida de configuracion).
//
//  LA GEOMETRIA VA EN mallas/<slug>.w3dm (formato propio, W3dMalla.h): poligonos
//  nativos, capas UV y de color, costuras, bordes marcados, normales del usuario,
//  geometria suelta, mesh parts y los pesos de los grupos, TODO por indice. El GLB
//  quedo solo para importar y para el "Export to..." del usuario. Los materiales,
//  que viajaban adentro del GLB, ahora son el bloque raiz "materiales" del JSON y
//  cada .w3dm los referencia POR NOMBRE.
//
//  Cubre: mallas, materiales, vertex anims, armatures + clips, modArmature,
//  armature 2D, el STACK de modificadores (Mirror/Screw/SubSurf/...),
//  espejo/instancia/curva (con target por nombre), el stack de CONSTRAINTS de
//  cualquier objeto (con la fuente por nombre), riel de camara,
//  paletas, layout, compilar e icono. Lo UNICO que queda afuera a proposito es
//  una Curve sin archivo de origen (no hay editor que la produzca); se avisa por
//  notificacion al guardar, sin bloquear.
//
//  QUEDA AFUERA A PROPOSITO (y se marca como "ext:", a la vista): el .obj/.wobj
//  del que se importo una malla y el .txt de una Curve. El del .obj YA NO ES UNA
//  PERDIDA: la geometria se hornea en el .w3dm y el archivo original queda solo
//  como "de donde salio" (si no esta, se avisa y la malla carga igual). El .txt
//  de la Curve sigue siendo una dependencia real: Curve::LoadFromFile usa
//  std::ifstream y no pasa por el VFS.
// ============================================================================
//
//  ESCRITURA ATOMICA: el zip ENTERO se arma en "<destino>.w3dtmp", en la MISMA
//  carpeta del destino, y recien al final se renombra encima (rename POSIX =
//  atomico). UN SOLO RENAME = UN SOLO PUNTO DE FALLO. Si algo falla a mitad
//  (disco lleno, ruta no escribible, export que no salio) se borra el temporal y
//  la version ANTERIOR del proyecto queda intacta.
//
//  VERIFICACION ANTES DE CERRAR: se exige que TODA referencia interna del JSON y
//  de cada .w3dui tenga su entrada escrita. Si falta una sola, el guardado
//  ABORTA sin renombrar. Sin esto el proyecto abriria "sin la textura" y no
//  fallaria nada, que es el fallo mas caro que puede tener este diseno.
//
//  SALIDA REPRODUCIBLE: mimetype primero, proyecto.json segundo, el resto
//  alfabetico, metodo STORE
//  y fecha FIJA (W3dZip.cpp). Guardar dos veces sin cambios da el MISMO archivo
//  byte a byte. Esa fecha fija parece un descuido y NO lo es: sin ella se rompen
//  el diff en git, el dedup entre versiones y el round-trip de los tests.
//
//  EL CICLO DEL ARCHIVO ABIERTO: el contenedor mantiene su FILE* abierto toda la
//  sesion para leer por demanda. En POSIX el rename sobre un archivo abierto
//  anda; en Windows falla. Por eso el guardado cierra el zip nuevo, DESMONTA,
//  renombra y vuelve a montar. En Linux este bug no aparece nunca.
// ============================================================================
bool GuardarW3D(const std::string& ruta);

// Ctrl+S: guarda a w3dPath, o abre el explorador en modo guardar si no hay
void GuardarProyecto();
// "Guardar como": siempre pide destino (si el .w3d elegido ya existe, pide
// confirmacion antes de pisarlo, igual que Render/Export)
void GuardarProyectoComo();

// TEST del harness (--script, comando 'saveatom'): fuerza un fallo de escritura
// del .w3d DESPUES de abrir el archivo destino (simula el disco lleno). Sirve
// para verificar que un guardado a medias NO destruye la version anterior.
// Siempre false en el editor real.
extern bool g_w3dFallarEscritura;

// ICONO del juego (tarjeta Juego): un PNG con alpha en su maxima definicion.
// Se guarda en el .w3d como RUTA EXTERNA relativa al .w3d (nunca embebido);
// "Compilar juego" genera de ahi los tamanos chicos (.desktop/hicolor del .deb,
// mipmaps del APK, SDL_SetWindowIcon del main). Vacio = sin icono.
extern std::string g_proyIcono;

// ============================================================================
//  CONFIG de la tarjeta Juego (Compilar juego): viaja con el PROYECTO.
//  Antes eran statics del editor (Properties.cpp) que se reseteaban en cada
//  arranque: el "Modo ventana" elegido se perdia y el juego compilaba siempre
//  con los defaults (pantalla completa). Ahora la config vive aca, se escribe
//  en el .w3d v3 como objeto raiz "compilar" (strings legibles, editable a
//  mano) y se carga al ABRIR el proyecto (import_w3d). La tarjeta Juego edita
//  estos campos directo y GuardarW3D escribe los valores vigentes al guardar
//  (mismo patron que g_proyIcono).
// ============================================================================
struct W3dCompilarCfg {
    int  modoVentana;   // 0 Ventana, 1 Pantalla completa, 2 Sin bordes
    int  orientacion;   // 0 Todas, 1 Solo vertical, 2 Solo horizontal
    int  assetsModo;    // 0 Sueltos (editables), 1 Empaquetados (protegidos)
    int  plataforma;    // 0 Linux .deb, 1 Linux AppImage, 2 WebGL, 3 Android
    bool usarFisica;    // false = W3D_SIN_FISICA (binds stub no-op)
    bool usarSonido;    // false = sin W3D_ENABLE_AUDIO (beep() mudo)
    bool modoDebug;     // true = W3D_DEV_LOG=1 (log + ring + depurar())
    unsigned uid;       // UID3 de Symbian del juego (app propia). 0 = sin asignar. Rango self-signed: 0xE0000000-0xEFFFFFFF
    int  volumen;       // 0..100 volumen del gameplay (audio del juego). 100 = sin atenuar.
};
extern W3dCompilarCfg g_proyCompilar;

// vuelve a los DEFAULTS del editor (pantalla completa, todas las orientaciones,
// assets sueltos, Linux .deb, fisica y sonido si, debug no). Se llama al abrir
// un proyecto: un .w3d viejo sin el bloque "compilar" queda con los defaults.
void W3dCompilarReset();

// int <-> string legible del JSON (bloque "compilar"). Un string desconocido o
// vacio devuelve el default del campo (un typo editando a mano no rompe nada).
const char* W3dCompilarModoVentanaStr(int m);   // "ventana"|"pantallaCompleta"|"sinBordes"
int         W3dCompilarModoVentanaInt(const std::string& s);
const char* W3dCompilarOrientacionStr(int o);   // "todas"|"vertical"|"horizontal"
int         W3dCompilarOrientacionInt(const std::string& s);
const char* W3dCompilarAssetsStr(int a);        // "sueltos"|"empaquetados"
int         W3dCompilarAssetsInt(const std::string& s);
const char* W3dCompilarPlataformaStr(int p);    // "linux-deb"|"appimage"|"web"|"android"
int         W3dCompilarPlataformaInt(const std::string& s);

#endif // GUARDARW3D_H
