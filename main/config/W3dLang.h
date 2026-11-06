#ifndef W3DLANG_H
#define W3DLANG_H

#include <string>

// ---------------------------------------------------------------------------------------------------------------
//  IDIOMAS. Es del EDITOR y solo del editor: ni el Core ni WhiskUI saben que esto existe -- a ellos les llegan los
//  textos YA traducidos. Un boton recibe un const char*; de donde salio no es asunto suyo.
//
//  La CLAVE es el propio texto en INGLES. T("Add") devuelve "Agregar" en espanol y "Add" en ingles. Eso da tres
//  cosas gratis:
//    - el ingles no necesita archivo: es el default y no se puede desincronizar de si mismo
//    - lo que todavia no este traducido se ve en INGLES, no roto ni mostrando una clave tipo MENU_ADD_LABEL
//    - el codigo se sigue leyendo: en el call site dice lo que sale en pantalla
//
//  LAS TRADUCCIONES ESTAN EN DISCO: res/idiomas/<codigo>.txt, una por idioma (formato y reglas en tools/genlang.py).
//  Al arrancar se carga SOLO la del idioma elegido: en el N95 la RAM vale, y antes todas vivian siempre en memoria
//  compiladas en el ejecutable. De cada frase se guarda la traduccion y dos hashes de 32 bits del ingles (no el
//  ingles: ese ya esta en el codigo, en el T("...") que la pide). La busqueda es una biseccion sobre los hashes.
//
//  PARA AGREGAR UNA FRASE: T("...") en el codigo y la linea "<ingles> TAB <traduccion>" en res/idiomas/es.txt y
//  pt.txt (y en los demas si se puede: lo que falte sale en ingles). Despues: python3 tools/genlang.py.
// ---------------------------------------------------------------------------------------------------------------

enum W3dIdioma { W3dLangEN = 0, W3dLangES = 1, W3dLangPT = 2, W3dLangFR = 3, W3dLangIT = 4, W3dLangTR = 5,
                 W3dLangDE = 6, W3dLangPL = 7, W3dLangRU = 8, W3dLangCantidad = 9 };

extern W3dIdioma g_idioma;

// El texto en el idioma activo. Si no esta traducido (o el idioma es ingles) devuelve 'en' TAL CUAL: nunca NULL,
// nunca una clave. El puntero devuelto vive para siempre (una traduccion cargada no se libera): se puede guardar.
const char* T(const char* en);

// Donde estan los archivos de idioma, a partir de la carpeta del skin (res/Skins/<skin>/ -> res/idiomas/; en
// Symbian, la carpeta privada es plana -> <privada>\idiomas\). Lo llama W3dInitUI. Hasta entonces T() da ingles.
void W3dIdiomaCarpeta(const std::string& skinDir);

// Idioma del SISTEMA (cada plataforma con su api). Lo que no se reconoce -> ingles. Se llama UNA vez al arrancar.
void W3dIdiomaDetectar();

void        W3dIdiomaSet(W3dIdioma id);
W3dIdioma   W3dIdiomaDe(const char* cod);   // "es", "es-AR", "pt_BR", "ru_RU.UTF-8"... -> el enum (ingles si no)
const char* W3dIdiomaNombre(W3dIdioma id);  // en SU idioma: "English" / "Español" / "Русский"...
const char* W3dIdiomaCodigo(W3dIdioma id);  // "en" / "es" / "ru"... (lo que va al config.ini y el nombre del .txt)
unsigned    W3dIdiomaFrases(W3dIdioma id);  // cuantas frases traducidas tiene (lo carga si hace falta; 0 = ninguna)

// El config.ini puede FORZAR un idioma: "idioma = es". Vacio o "auto" = seguir al sistema, que es el default.
// Va aparte de W3dIdiomaDetectar porque el que quiere el editor en ingles con el SO en espaniol tiene que poder.
extern bool g_idiomaForzado;   // true = lo mando el config, no el SO -> W3dIdiomaDetectar() no lo pisa

#endif
