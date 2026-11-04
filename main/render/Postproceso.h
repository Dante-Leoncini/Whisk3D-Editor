#ifndef W3D_POSTPROCESO_H
#define W3D_POSTPROCESO_H
// ============================================================================
//  Postproceso.h - EL POSTPROCESADO de la vista Rendered (y del render a archivo): una PILA de efectos, como los
//  modificadores (se agregan, se ordenan, se quitan; cada uno con sus parametros). Pensado para el tiempo real de un
//  juego: con shaders (GLES 2.0 / WebGL / Vulkan) va entero en la GPU (gfx::PostEfecto, sobre copias de la pantalla);
//  sin shaders (GLES 1.1, el N95) lo mismo por CPU, mas lento (el panel avisa).
//
//  Los efectos de hoy:
//    GRANO de camara     v[0] intensidad (0..1), v[1] tamanio del grano (px); opc = de color
//    DESENFOQUE DE CAMPO v[0] distancia de foco (m), v[1] rango nitido (m), v[2] desenfoque maximo (% del alto de la
//                        imagen); opc = usar el foco de la camara (si la camara activa tiene la profundidad de campo
//                        prendida, su foco -su objeto o su distancia- manda sobre v[0]). Lee la PROFUNDIDAD
//    DESENFOQUE DE MOVIMIENTO v[0] obturador (0..1). Lee el pase de MOVIMIENTO (render/VectoresMovimiento.h)
//    BLOOM               v[0] intensidad, v[1] umbral (brillo desde el que brilla, 0..1 como se ve), v[2] rodilla suave
//                        (0 = corte duro, 1 = transicion suave), v[3] tope (el brillo de cada pixel se recorta a esto antes
//                        de brillar), v[4] difusion (1..10: cuanto se desparrama, relativo al alto de la imagen), v[5]
//                        relacion anamorfica (-1..1: < 0 se estira vertical, > 0 horizontal); col = el tinte; opc = modo
//                        rapido (menos muestras). Un filtro del brillo + un desenfoque separable (horizontal y vertical)
//                        que se suma a la imagen
//  Son independientes de los del trazado de rayos (su desenfoque de movimiento y la profundidad de campo de la camara):
//  se puede usar uno, el otro o los dos.
// ============================================================================
#include <vector>

enum W3dPostTipo { W3dPostGrano = 0, W3dPostFoco = 1, W3dPostMovimiento = 2, W3dPostBloom = 3, W3dPostTipos = 4 };
// opc: la opcion de si/no del efecto (arriba); v: sus numeros; col: su color (el tinte del bloom)
struct W3dPostEfecto { int tipo; bool activo; bool opc; float v[8]; float col[4]; };
int W3dPostNumeros(int tipo);                    // cuantos de v[] usa (y guarda) ese efecto
struct W3dPostOpciones {
    bool on;                               // la tarjeta "Post Processing" tildada
    std::vector<W3dPostEfecto> efectos;    // en el orden en que se aplican
    int activo;                            // el elegido en la lista del panel (-1 = ninguno)
    W3dPostOpciones() : on(false), activo(-1) {}
};
// la pila con la que trabajan las funciones de abajo: la del objeto Postproceso que se esta aplicando (o mostrando en el
// panel). NULL = ninguna (apagado, sin efectos)
void W3dPostUsar(const W3dPostOpciones* o);
const W3dPostOpciones& W3dPostActual();

const char* W3dPostNombre(int tipo);             // en ingles: T() lo traduce
void W3dPostDefectos(W3dPostEfecto& e, int tipo);
bool W3dPostHay();                               // prendido y con algun efecto activo
bool W3dPostNecesitaProfundidad();
bool W3dPostNecesitaMovimiento();
bool W3dPostPorCPU();                            // sin shaders: va por CPU (el aviso del panel)
float W3dPostDistanciaFoco(const W3dPostEfecto& e);       // la distancia de foco de ese desenfoque de campo (m)
// los numeros con los que desenfoca ese efecto en una imagen de 'h' pixeles de alto: el foco (m), el rango nitido (m) y
// el radio maximo (px). Con "usar el foco de la camara", manda la CAMARA ACTIVA: su profundidad de campo apagada = no
// desenfoca (false) y su APERTURA (f-stop) da el radio (lo de atras de todo, como lo veria su lente)
bool W3dPostFocoNumeros(const W3dPostEfecto& e, int h, float& foco, float& rango, float& radioMax);

// POR CPU, sobre una imagen RGBA (filas de arriba abajo). prof = la distancia de cada pixel a la camara en m (NULL si
// no hay: el desenfoque de campo no hace nada); vec = el pase de movimiento (NULL: el de movimiento no hace nada)
void W3dPostCPU(unsigned char* rgba, int w, int h, const float* prof, const unsigned char* vec, unsigned semilla);

// POR GPU, sobre el rectangulo (x, y, w, h: coordenadas de GL, desde abajo a la izquierda) de la pantalla, que ya
// tiene la imagen. Si algun efecto lee datos, 'dibujarDatos(ctx, profundidad)' tiene que dibujar en ese mismo rectangulo
// (lo limpia el) el pase de la PROFUNDIDAD (PaseDatos, 'lejos' es su escala) o el del MOVIMIENTO (PaseVectores). false = sin shaders (usar el de CPU)
bool W3dPostGPU(int x, int y, int w, int h, void (*dibujarDatos)(void*, bool profundidad), void* ctx, float lejos, unsigned semilla);
// LOS DATOS DE ANTEMANO (el postprocesado en el arbol): la profundidad y/o el movimiento se dibujan y se guardan ANTES
// de la escena, y W3dPostGPU usa esos en vez de redibujar la escena en el medio del recorrido. Hasta W3dPostDatosOlvidar
bool W3dPostDatosPrevios(int x, int y, int w, int h, void (*dibujarDatos)(void*, bool profundidad), void* ctx, bool prof, bool mov);
void W3dPostDatosOlvidar();
#endif
