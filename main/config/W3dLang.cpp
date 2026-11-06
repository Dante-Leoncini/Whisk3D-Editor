#include "W3dLang.h"
#include "w3dFilesystem.h"      // ReadFileBytes: la misma lectura en todas las plataformas (en Android, del APK)
#include <string.h>
#include <stdlib.h>
#include <vector>

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
#elif defined(W3D_DREAMCAST)
    #include <dc/flashrom.h>
#endif

W3dIdioma g_idioma = W3dLangEN;
bool g_idiomaForzado = false;

// en el ORDEN del enum
static const char* const kCodigos[W3dLangCantidad] = { "en", "es", "pt", "fr", "it", "tr", "de", "pl", "ru" };
// Cada idioma en SU nombre y bien escrito (las letras que no estan en font.png salen de font-ext.png).
static const char* const kNombres[W3dLangCantidad] = {
    "English", "Español", "Português", "Français", "Italiano", "Türkçe", "Deutsch", "Polski", "Русский" };

static W3dIdioma Valido(int id){ return (id >= 0 && id < (int)W3dLangCantidad) ? (W3dIdioma)id : W3dLangEN; }

void W3dIdiomaSet(W3dIdioma id){ g_idioma = Valido((int)id); }
const char* W3dIdiomaCodigo(W3dIdioma id){ return kCodigos[Valido((int)id)]; }
const char* W3dIdiomaNombre(W3dIdioma id){ return kNombres[Valido((int)id)]; }

// "es", "es-AR", "es_AR.UTF-8", "pt_BR"... -> el enum. Solo se miran las DOS primeras letras: la variante regional
// no cambia la traduccion (un "es-MX" lee igual que un "es-ES"), y asi entra cualquier formato sin parsearlo.
W3dIdioma W3dIdiomaDe(const char* cod){
    if (!cod || !cod[0] || !cod[1]) return W3dLangEN;
    const char a = (char)((cod[0] >= 'A' && cod[0] <= 'Z') ? cod[0] + 32 : cod[0]);
    const char b = (char)((cod[1] >= 'A' && cod[1] <= 'Z') ? cod[1] + 32 : cod[1]);
    for (int i = 1; i < (int)W3dLangCantidad; i++)
        if (kCodigos[i][0] == a && kCodigos[i][1] == b) return (W3dIdioma)i;
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
    if (l == ELangFrench)  { g_idioma = W3dLangFR; return; }
    if (l == ELangItalian) { g_idioma = W3dLangIT; return; }
    if (l == ELangTurkish) { g_idioma = W3dLangTR; return; }
    if (l == ELangGerman)  { g_idioma = W3dLangDE; return; }
    if (l == ELangPolish)  { g_idioma = W3dLangPL; return; }
    if (l == ELangRussian) { g_idioma = W3dLangRU; return; }
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

#elif defined(W3D_DREAMCAST)
    // La Dreamcast guarda el idioma elegido en el menu de la consola en su memoria flash (el que usan los juegos).
    // (con la BIOS emulada de Flycast la flash no tiene esa configuracion: queda en ingles, se cambia en Ajustes)
    flashrom_syscfg_t cfg;
    g_idioma = W3dLangEN;
    if (flashrom_get_syscfg(&cfg) >= 0){
        if (cfg.language == FLASHROM_LANG_SPANISH) g_idioma = W3dLangES;
        else if (cfg.language == FLASHROM_LANG_FRENCH) g_idioma = W3dLangFR;
        else if (cfg.language == FLASHROM_LANG_ITALIAN) g_idioma = W3dLangIT;
        else if (cfg.language == FLASHROM_LANG_GERMAN) g_idioma = W3dLangDE;
    }

#elif defined(_WIN32)
    // El idioma de la UI, no el de formato de numeros: son cosas distintas y el que importa aca es el de la UI.
    const LANGID id = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(id)){
        case LANG_SPANISH:    g_idioma = W3dLangES; break;
        case LANG_PORTUGUESE: g_idioma = W3dLangPT; break;
        case LANG_FRENCH:     g_idioma = W3dLangFR; break;
        case LANG_ITALIAN:    g_idioma = W3dLangIT; break;
        case LANG_TURKISH:    g_idioma = W3dLangTR; break;
        case LANG_GERMAN:     g_idioma = W3dLangDE; break;
        case LANG_POLISH:     g_idioma = W3dLangPL; break;
        case LANG_RUSSIAN:    g_idioma = W3dLangRU; break;
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
//  LAS TRADUCCIONES CARGADAS. Una tabla por idioma, que se carga la primera vez que se la pide y NO se libera (T()
//  promete que su puntero vive para siempre). En el uso normal hay UNA sola: la del idioma con que arranco.
//  De cada frase: dos hashes del ingles y el puntero a la traduccion, que vive en un unico bloque de memoria.
// ---------------------------------------------------------------------------------------------------------------
struct W3dLangFrase { unsigned int a, b; const char* texto; };
struct W3dLangTablaCargada { W3dLangFrase* frases; unsigned n; char* textos; bool intentado; };
static W3dLangTablaCargada s_tablas[W3dLangCantidad];   // (estatica: arranca en cero)
static std::string s_carpeta;                           // con el separador al final; vacia = todavia no se sabe

// Los MISMOS dos hashes que tools/genlang.py (que ademas verifica que no haya dos claves con el mismo par)
static void Hashes(const unsigned char* p, size_t n, unsigned int& a, unsigned int& b){
    unsigned int h1 = 2166136261u, h2 = 0u;                 // FNV-1a de 32 bits y sdbm
    for (size_t i = 0; i < n; i++){
        h1 = (h1 ^ p[i]) * 16777619u;
        h2 = (unsigned int)p[i] + (h2 << 6) + (h2 << 16) - h2;
    }
    a = h1; b = h2;
}

static int CompararFrase(const void* x, const void* y){
    const W3dLangFrase* p = (const W3dLangFrase*)x;
    const W3dLangFrase* q = (const W3dLangFrase*)y;
    if (p->a != q->a) return p->a < q->a ? -1 : 1;
    if (p->b != q->b) return p->b < q->b ? -1 : 1;
    return 0;
}

// \n \t \\ -> el caracter. Escribe en 'dst' (que puede ser el mismo 'src': solo achica). Devuelve el largo.
static size_t Desescapar(const char* src, size_t n, char* dst){
    size_t k = 0;
    for (size_t i = 0; i < n; i++){
        if (src[i] == '\\' && i + 1 < n && (src[i + 1] == 'n' || src[i + 1] == 't' || src[i + 1] == '\\')){
            dst[k++] = (src[i + 1] == 'n') ? '\n' : (src[i + 1] == 't') ? '\t' : '\\';
            i++;
        } else dst[k++] = src[i];
    }
    return k;
}

static void Cargar(W3dIdioma id){
    W3dLangTablaCargada& t = s_tablas[id];
    if (t.intentado || id == W3dLangEN || s_carpeta.empty()) return;
    t.intentado = true;                 // (si el archivo no esta, no se vuelve a buscar en cada T(): queda en ingles)
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(s_carpeta + kCodigos[id] + ".txt", datos) || datos.empty()) return;

    // cuantas frases hay (lineas con su TAB) y cuanto ocupan sus traducciones
    unsigned n = 0;
    for (size_t i = 0; i < datos.size(); i++) if (datos[i] == '\t') n++;
    if (!n) return;
    t.frases = (W3dLangFrase*)malloc(sizeof(W3dLangFrase) * n);
    t.textos = (char*)malloc(datos.size() + 1);
    if (!t.frases || !t.textos){ free(t.frases); free(t.textos); t.frases = 0; t.textos = 0; return; }

    char* buf = (char*)&datos[0];
    const size_t total = datos.size();
    size_t pos = 0, usado = 0;
    unsigned k = 0;
    while (pos < total && k < n){
        size_t fin = pos;
        while (fin < total && buf[fin] != '\n') fin++;
        size_t lin = fin;
        if (lin > pos && buf[lin - 1] == '\r') lin--;        // (un archivo guardado con CRLF tambien anda)
        size_t tab = pos;
        while (tab < lin && buf[tab] != '\t') tab++;
        if (tab < lin){
            // la clave se desescapa EN EL LUGAR (solo para hashearla: no se guarda)
            const size_t lc = Desescapar(buf + pos, tab - pos, buf + pos);
            const size_t lt = Desescapar(buf + tab + 1, lin - tab - 1, t.textos + usado);
            if (lt > 0){                                      // traduccion vacia = sin traducir: sale en ingles
                Hashes((const unsigned char*)(buf + pos), lc, t.frases[k].a, t.frases[k].b);
                t.frases[k].texto = t.textos + usado;
                t.textos[usado + lt] = 0;
                usado += lt + 1;
                k++;
            }
        }
        pos = fin + 1;
    }
    t.n = k;
    // achicar el bloque de textos a lo que se uso (los punteros se recalculan si realloc lo movio)
    char* chico = (char*)realloc(t.textos, usado ? usado : 1);
    if (chico && chico != t.textos){
        for (unsigned i = 0; i < k; i++) t.frases[i].texto = chico + (t.frases[i].texto - t.textos);
    }
    if (chico) t.textos = chico;
    qsort(t.frases, k, sizeof(W3dLangFrase), CompararFrase);
}

void W3dIdiomaCarpeta(const std::string& skinDir){
    // res/Skins/<skin>/ -> res/idiomas/ (PC, web, Android, 3DS, Dreamcast: res/ entero va con el programa).
    // Symbian instala todo en la carpeta privada, plana -> <privada>\idiomas\ (ver los .pkg).
    const char sep = skinDir.empty() ? '/' : skinDir[skinDir.size() - 1];
    const std::string marca = std::string(1, sep) + "Skins" + sep;
    const size_t p = skinDir.rfind(marca);
    if (p != std::string::npos) s_carpeta = skinDir.substr(0, p + 1) + "idiomas" + sep;
    else                         s_carpeta = skinDir + "idiomas" + sep;
}

unsigned W3dIdiomaFrases(W3dIdioma id){
    id = Valido((int)id);
    Cargar(id);
    return s_tablas[id].n;
}

// ---------------------------------------------------------------------------------------------------------------
//  BUSQUEDA. Los hashes del ingles y una biseccion sobre la tabla del idioma activo (ordenada al cargarla).
// ---------------------------------------------------------------------------------------------------------------
const char* T(const char* en){
    if (!en) return "";
    if (g_idioma == W3dLangEN) return en;          // el ingles ES la clave: no hay nada que buscar
    Cargar(g_idioma);
    const W3dLangTablaCargada& t = s_tablas[g_idioma];
    if (!t.n) return en;
    W3dLangFrase q;
    Hashes((const unsigned char*)en, strlen(en), q.a, q.b);
    int lo = 0, hi = (int)t.n - 1;
    while (lo <= hi){
        const int med = lo + (hi - lo) / 2;
        const int c = CompararFrase(&q, &t.frases[med]);
        if (c == 0) return t.frases[med].texto;
        if (c < 0) hi = med - 1; else lo = med + 1;
    }
    return en;                                     // no esta traducida -> ingles, que es lo correcto y no rompe
}
