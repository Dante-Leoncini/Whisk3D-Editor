#ifndef W3DLANG_H
#define W3DLANG_H

// ---------------------------------------------------------------------------------------------------------------
//  IDIOMAS. Es del EDITOR y solo del editor: ni el Core ni WhiskUI saben que esto existe -- a ellos les llegan los
//  textos YA traducidos. Un boton recibe un const char*; de donde salio no es asunto suyo.
//
//  La CLAVE es el propio texto en INGLES. T("Add") devuelve "Agregar" en espanol y "Add" en ingles. Eso da tres
//  cosas gratis:
//    - el ingles no necesita tabla: es el default y no se puede desincronizar de si mismo
//    - lo que todavia no este traducido se ve en INGLES, no roto ni mostrando una clave tipo MENU_ADD_LABEL
//    - el codigo se sigue leyendo: en el call site dice lo que sale en pantalla
//
//  La tabla (W3dLangTabla.h) esta ORDENADA y se busca por biseccion: sin heap, sin std::map -- que en el N95
//  (STLport, C++03) instanciaria plantillas por cada tipo. Es una tabla estatica y una busqueda log(n).
// ---------------------------------------------------------------------------------------------------------------

enum W3dIdioma { W3dLangEN = 0, W3dLangES = 1, W3dLangPT = 2 };

extern W3dIdioma g_idioma;

// El texto en el idioma activo. Si no esta traducido (o el idioma es ingles) devuelve 'en' TAL CUAL: nunca NULL,
// nunca una clave. El puntero devuelto es estatico y vive para siempre: se puede guardar.
const char* T(const char* en);

// Idioma del SISTEMA. Espaniol -> es, portugues -> pt, cualquier otra cosa -> ingles. Se llama UNA vez al arrancar.
void W3dIdiomaDetectar();

void        W3dIdiomaSet(W3dIdioma id);
W3dIdioma   W3dIdiomaDe(const char* cod);   // "es", "es-AR", "pt_BR"... -> el enum (ingles si no lo reconoce)
const char* W3dIdiomaNombre(W3dIdioma id);  // "English" / "Espanol" / "Portugues"
const char* W3dIdiomaCodigo(W3dIdioma id);  // "en" / "es" / "pt" (lo que va al config.ini)

// El config.ini puede FORZAR un idioma: "idioma = es". Vacio o "auto" = seguir al sistema, que es el default.
// Va aparte de W3dIdiomaDetectar porque el que quiere el editor en ingles con el SO en espaniol tiene que poder.
extern bool g_idiomaForzado;   // true = lo mando el config, no el SO -> W3dIdiomaDetectar() no lo pisa

#endif
