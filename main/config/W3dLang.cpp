#include "W3dLang.h"
#include "W3dLangTabla.h"   // la tabla: generada, ORDENADA por el ingles (ver el header)
#include <string.h>
#include <stdlib.h>

#ifdef W3D_SYMBIAN
    #include <e32std.h>
    #include <e32const.h>
#elif defined(__EMSCRIPTEN__)
    #include <emscripten.h>
#elif defined(__ANDROID__)
    #if defined(SDL_MAJOR_VERSION) || 1
        #include <SDL.h>
    #endif
#elif defined(_WIN32)
    #include <windows.h>
#endif

W3dIdioma g_idioma = W3dLangEN;
bool g_idiomaForzado = false;

void W3dIdiomaSet(W3dIdioma id){ g_idioma = id; }

const char* W3dIdiomaCodigo(W3dIdioma id){
    return (id == W3dLangES) ? "es" : (id == W3dLangPT) ? "pt" : "en";
}

const char* W3dIdiomaNombre(W3dIdioma id){
    // Cada idioma en SU nombre y bien escrito. La ñ esta en el atlas desde siempre; la ê del portugues no
    // (necesita sprite nuevo), asi que ese va sin acento hasta que exista.
    return (id == W3dLangES) ? "Español" : (id == W3dLangPT) ? "Portugues" : "English";
}

// "es", "es-AR", "es_AR.UTF-8", "pt_BR"... -> el enum. Solo se miran las DOS primeras letras: la variante regional
// no cambia la traduccion (un "es-MX" lee igual que un "es-ES"), y asi entra cualquier formato sin parsearlo.
W3dIdioma W3dIdiomaDe(const char* cod){
    if (!cod || !cod[0] || !cod[1]) return W3dLangEN;
    const char a = (char)((cod[0] >= 'A' && cod[0] <= 'Z') ? cod[0] + 32 : cod[0]);
    const char b = (char)((cod[1] >= 'A' && cod[1] <= 'Z') ? cod[1] + 32 : cod[1]);
    if (a == 'e' && b == 's') return W3dLangES;
    if (a == 'p' && b == 't') return W3dLangPT;
    return W3dLangEN;
}

// ---------------------------------------------------------------------------------------------------------------
//  DETECCION. Cada plataforma pregunta con SU api nativa y no con SDL: el proyecto compila con SDL2 y con SDL3, y
//  SDL_GetPreferredLocales tiene firma distinta en cada uno. Preguntarle al SO son 3 lineas y no depende de nada.
// ---------------------------------------------------------------------------------------------------------------
void W3dIdiomaDetectar(){
    if (g_idiomaForzado) return;   // lo eligio el usuario en el config: el SO no manda
#ifdef W3D_SYMBIAN
    // El telefono expone el idioma de la UI directamente. Las variantes regionales cuentan como el mismo idioma.
    const TLanguage l = User::Language();
    if (l == ELangSpanish || l == ELangLatinAmericanSpanish)   { g_idioma = W3dLangES; return; }
    if (l == ELangPortuguese || l == ELangBrazilianPortuguese) { g_idioma = W3dLangPT; return; }
    g_idioma = W3dLangEN;

#elif defined(__EMSCRIPTEN__)
    // El navegador: navigator.language ya viene como "es-AR" / "pt-BR".
    char cod[8];
    cod[0] = 0;
    EM_ASM({
        var l = (navigator.languages && navigator.languages.length) ? navigator.languages[0] : navigator.language;
        stringToUTF8(l ? l : "en", $0, 8);
    }, cod);
    g_idioma = W3dIdiomaDe(cod);

#elif defined(__ANDROID__)
    // Android no tiene LANG en el entorno: el idioma lo sabe el sistema y SDL lo trae ya resuelto.
    g_idioma = W3dLangEN;
    #if SDL_MAJOR_VERSION >= 3
        int n = 0;
        SDL_Locale** locs = SDL_GetPreferredLocales(&n);
        if (locs){
            if (n > 0 && locs[0] && locs[0]->language) g_idioma = W3dIdiomaDe(locs[0]->language);
            SDL_free(locs);
        }
    #else
        SDL_Locale* locs = SDL_GetPreferredLocales();
        if (locs){
            if (locs[0].language) g_idioma = W3dIdiomaDe(locs[0].language);
            SDL_free(locs);
        }
    #endif

#elif defined(_WIN32)
    // El idioma de la UI, no el de formato de numeros: son cosas distintas y el que importa aca es el de la UI.
    const LANGID id = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(id)){
        case LANG_SPANISH:    g_idioma = W3dLangES; break;
        case LANG_PORTUGUESE: g_idioma = W3dLangPT; break;
        default:              g_idioma = W3dLangEN; break;
    }

#else
    // Linux / Mac: el entorno. En orden de prioridad, que es el que define POSIX.
    const char* v = getenv("LC_ALL");
    if (!v || !v[0]) v = getenv("LC_MESSAGES");
    if (!v || !v[0]) v = getenv("LANG");
    g_idioma = W3dIdiomaDe(v);
#endif
}

// ---------------------------------------------------------------------------------------------------------------
//  BUSQUEDA. La tabla esta ordenada por el ingles (lo garantiza el generador), asi que se bisecta.
// ---------------------------------------------------------------------------------------------------------------
const char* T(const char* en){
    if (!en) return "";
    if (g_idioma == W3dLangEN) return en;          // el ingles ES la clave: no hay nada que buscar
    int lo = 0, hi = (int)W3D_LANG_ENTRADAS - 1;
    while (lo <= hi){
        const int med = lo + (hi - lo) / 2;
        const int c = strcmp(en, W3dLangTabla[med].en);
        if (c == 0){
            const char* tr = (g_idioma == W3dLangES) ? W3dLangTabla[med].es : W3dLangTabla[med].pt;
            return (tr && tr[0]) ? tr : en;        // entrada vacia = todavia sin traducir -> ingles
        }
        if (c < 0) hi = med - 1; else lo = med + 1;
    }
    return en;                                     // no esta en la tabla -> ingles, que es lo correcto y no rompe
}
