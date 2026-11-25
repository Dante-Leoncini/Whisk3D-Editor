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
#elif defined(__ANDROID__) || defined(W3D_VITA) || defined(W3D_PSP)
    #if defined(SDL_MAJOR_VERSION) || 1
        #include <SDL.h>
    #endif
#elif defined(_WIN32)
    #include <windows.h>
#elif defined(W3D_DREAMCAST)
    #include <dc/flashrom.h>
#endif
#include "app/W3dNoNearFar.h"   // (Windows: windows.h define near/far como macros vacias)

W3dLanguage g_language = W3dLangEN;
bool g_languageForced = false;

// en el ORDEN del enum
static const char* const kCodes[W3dLangCount] = { "en", "es", "pt", "fr", "it", "tr", "de", "pl", "ru" };
// Cada idioma en SU nombre y bien escrito (las letras que no estan en font.png salen de font-ext.png).
static const char* const kNames[W3dLangCount] = {
    "English", "Español", "Português", "Français", "Italiano", "Türkçe", "Deutsch", "Polski", "Русский" };

static W3dLanguage Valid(int id){ return (id >= 0 && id < (int)W3dLangCount) ? (W3dLanguage)id : W3dLangEN; }

void W3dLanguageSet(W3dLanguage id){ g_language = Valid((int)id); }
const char* W3dLanguageCode(W3dLanguage id){ return kCodes[Valid((int)id)]; }
const char* W3dLanguageName(W3dLanguage id){ return kNames[Valid((int)id)]; }

// "es", "es-AR", "es_AR.UTF-8", "pt_BR"... -> el enum. Solo se miran las DOS primeras letras: la variante regional
// no cambia la traduccion (un "es-MX" lee igual que un "es-ES"), y asi entra cualquier formato sin parsearlo.
W3dLanguage W3dLanguageOf(const char* cod){
    if (!cod || !cod[0] || !cod[1]) return W3dLangEN;
    const char a = (char)((cod[0] >= 'A' && cod[0] <= 'Z') ? cod[0] + 32 : cod[0]);
    const char b = (char)((cod[1] >= 'A' && cod[1] <= 'Z') ? cod[1] + 32 : cod[1]);
    for (int i = 1; i < (int)W3dLangCount; i++)
        if (kCodes[i][0] == a && kCodes[i][1] == b) return (W3dLanguage)i;
    return W3dLangEN;
}

// ---------------------------------------------------------------------------------------------------------------
//  DETECCION. Cada plataforma pregunta con SU api nativa y no con SDL: el proyecto compila con SDL2 y con SDL3, y
//  SDL_GetPreferredLocales tiene firma distinta en cada uno. Preguntarle al SO son 3 lineas y no depende de nada.
// ---------------------------------------------------------------------------------------------------------------
void W3dLanguageDetect(){
    if (g_languageForced) return;   // lo eligio el usuario en el config: el SO no manda
#ifdef W3D_SYMBIAN
    // El telefono expone el idioma de la UI directamente. Las variantes regionales cuentan como el mismo idioma.
    const TLanguage l = User::Language();
    if (l == ELangSpanish || l == ELangLatinAmericanSpanish)   { g_language = W3dLangES; return; }
    if (l == ELangPortuguese || l == ELangBrazilianPortuguese) { g_language = W3dLangPT; return; }
    if (l == ELangFrench)  { g_language = W3dLangFR; return; }
    if (l == ELangItalian) { g_language = W3dLangIT; return; }
    if (l == ELangTurkish) { g_language = W3dLangTR; return; }
    if (l == ELangGerman)  { g_language = W3dLangDE; return; }
    if (l == ELangPolish)  { g_language = W3dLangPL; return; }
    if (l == ELangRussian) { g_language = W3dLangRU; return; }
    g_language = W3dLangEN;

#elif defined(__EMSCRIPTEN__)
    // El navegador: navigator.language ya viene como "es-AR" / "pt-BR".
    char cod[8];
    cod[0] = 0;
    EM_ASM({
        var l = (navigator.languages && navigator.languages.length) ? navigator.languages[0] : navigator.language;
        stringToUTF8(l ? l : "en", $0, 8);
    }, cod);
    g_language = W3dLanguageOf(cod);

#elif defined(__ANDROID__) || defined(W3D_VITA) || defined(W3D_PSP)
    // Android no tiene LANG en el entorno: el idioma lo sabe el sistema y SDL lo trae ya resuelto (en la Vita y la
    // PSP, el del menu de la consola).
    g_language = W3dLangEN;
    #if SDL_MAJOR_VERSION >= 3
        int n = 0;
        SDL_Locale** locs = SDL_GetPreferredLocales(&n);
        if (locs){
            if (n > 0 && locs[0] && locs[0]->language) g_language = W3dLanguageOf(locs[0]->language);
            SDL_free(locs);
        }
    #else
        SDL_Locale* locs = SDL_GetPreferredLocales();
        if (locs){
            if (locs[0].language) g_language = W3dLanguageOf(locs[0].language);
            SDL_free(locs);
        }
    #endif

#elif defined(W3D_DREAMCAST)
    // La Dreamcast guarda el idioma elegido en el menu de la consola en su memoria flash (el que usan los juegos).
    // (con la BIOS emulada de Flycast la flash no tiene esa configuracion: queda en ingles, se cambia en Ajustes)
    flashrom_syscfg_t cfg;
    g_language = W3dLangEN;
    if (flashrom_get_syscfg(&cfg) >= 0){
        if (cfg.language == FLASHROM_LANG_SPANISH) g_language = W3dLangES;
        else if (cfg.language == FLASHROM_LANG_FRENCH) g_language = W3dLangFR;
        else if (cfg.language == FLASHROM_LANG_ITALIAN) g_language = W3dLangIT;
        else if (cfg.language == FLASHROM_LANG_GERMAN) g_language = W3dLangDE;
    }

#elif defined(_WIN32)
    // El idioma de la UI, no el de formato de numeros: son cosas distintas y el que importa aca es el de la UI.
    const LANGID id = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(id)){
        case LANG_SPANISH:    g_language = W3dLangES; break;
        case LANG_PORTUGUESE: g_language = W3dLangPT; break;
        case LANG_FRENCH:     g_language = W3dLangFR; break;
        case LANG_ITALIAN:    g_language = W3dLangIT; break;
        case LANG_TURKISH:    g_language = W3dLangTR; break;
        case LANG_GERMAN:     g_language = W3dLangDE; break;
        case LANG_POLISH:     g_language = W3dLangPL; break;
        case LANG_RUSSIAN:    g_language = W3dLangRU; break;
        default:              g_language = W3dLangEN; break;
    }

#else
    // Linux / Mac: el entorno. En orden de prioridad, que es el que define POSIX.
    const char* v = getenv("LC_ALL");
    if (!v || !v[0]) v = getenv("LC_MESSAGES");
    if (!v || !v[0]) v = getenv("LANG");
    g_language = W3dLanguageOf(v);
#endif
}

// ---------------------------------------------------------------------------------------------------------------
//  LAS TRADUCCIONES CARGADAS. Una tabla por idioma, que se carga la primera vez que se la pide y NO se libera (T()
//  promete que su puntero vive para siempre). En el uso normal hay UNA sola: la del idioma con que arranco.
//  De cada frase: dos hashes del ingles y el puntero a la traduccion, que vive en un unico bloque de memoria.
// ---------------------------------------------------------------------------------------------------------------
struct W3dLangPhrase { unsigned int a, b; const char* text; };
struct W3dLangLoadedTable { W3dLangPhrase* phrases; unsigned n; char* texts; bool tried; };
static W3dLangLoadedTable s_tables[W3dLangCount];   // (estatica: arranca en cero)
static std::string s_folder;                        // con el separador al final; vacia = todavia no se sabe

// Los MISMOS dos hashes que tools/genlang.py (que ademas verifica que no haya dos claves con el mismo par)
static void Hashes(const unsigned char* p, size_t n, unsigned int& a, unsigned int& b){
    unsigned int h1 = 2166136261u, h2 = 0u;                 // FNV-1a de 32 bits y sdbm
    for (size_t i = 0; i < n; i++){
        h1 = (h1 ^ p[i]) * 16777619u;
        h2 = (unsigned int)p[i] + (h2 << 6) + (h2 << 16) - h2;
    }
    a = h1; b = h2;
}

static int ComparePhrase(const void* x, const void* y){
    const W3dLangPhrase* p = (const W3dLangPhrase*)x;
    const W3dLangPhrase* q = (const W3dLangPhrase*)y;
    if (p->a != q->a) return p->a < q->a ? -1 : 1;
    if (p->b != q->b) return p->b < q->b ? -1 : 1;
    return 0;
}

// \n \t \\ -> el caracter. Escribe en 'dst' (que puede ser el mismo 'src': solo achica). Devuelve el largo.
static size_t Unescape(const char* src, size_t n, char* dst){
    size_t k = 0;
    for (size_t i = 0; i < n; i++){
        if (src[i] == '\\' && i + 1 < n && (src[i + 1] == 'n' || src[i + 1] == 't' || src[i + 1] == '\\')){
            dst[k++] = (src[i + 1] == 'n') ? '\n' : (src[i + 1] == 't') ? '\t' : '\\';
            i++;
        } else dst[k++] = src[i];
    }
    return k;
}

static void Load(W3dLanguage id){
    W3dLangLoadedTable& t = s_tables[id];
    if (t.tried || id == W3dLangEN || s_folder.empty()) return;
    t.tried = true;                 // (si el archivo no esta, no se vuelve a buscar en cada T(): queda en ingles)
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(s_folder + kCodes[id] + ".txt", data) || data.empty()) return;

    // cuantas frases hay (lineas con su TAB) y cuanto ocupan sus traducciones
    unsigned n = 0;
    for (size_t i = 0; i < data.size(); i++) if (data[i] == '\t') n++;
    if (!n) return;
    t.phrases = (W3dLangPhrase*)malloc(sizeof(W3dLangPhrase) * n);
    t.texts   = (char*)malloc(data.size() + 1);
    if (!t.phrases || !t.texts){ free(t.phrases); free(t.texts); t.phrases = 0; t.texts = 0; return; }

    char* buf = (char*)&data[0];
    const size_t total = data.size();
    size_t pos = 0, used = 0;
    unsigned k = 0;
    while (pos < total && k < n){
        size_t end = pos;
        while (end < total && buf[end] != '\n') end++;
        size_t lin = end;
        if (lin > pos && buf[lin - 1] == '\r') lin--;        // (un archivo guardado con CRLF tambien anda)
        size_t tab = pos;
        while (tab < lin && buf[tab] != '\t') tab++;
        if (tab < lin){
            // la clave se desescapa EN EL LUGAR (solo para hashearla: no se guarda)
            const size_t lc = Unescape(buf + pos, tab - pos, buf + pos);
            const size_t lt = Unescape(buf + tab + 1, lin - tab - 1, t.texts + used);
            if (lt > 0){                                      // traduccion vacia = sin traducir: sale en ingles
                Hashes((const unsigned char*)(buf + pos), lc, t.phrases[k].a, t.phrases[k].b);
                t.phrases[k].text = t.texts + used;
                t.texts[used + lt] = 0;
                used += lt + 1;
                k++;
            }
        }
        pos = end + 1;
    }
    t.n = k;
    // achicar el bloque de textos a lo que se uso (los punteros se recalculan si realloc lo movio)
    char* small = (char*)realloc(t.texts, used ? used : 1);
    if (small && small != t.texts){
        for (unsigned i = 0; i < k; i++) t.phrases[i].text = small + (t.phrases[i].text - t.texts);
    }
    if (small) t.texts = small;
    qsort(t.phrases, k, sizeof(W3dLangPhrase), ComparePhrase);
}

void W3dLanguageFolder(const std::string& skinDir){
#ifdef W3D_DREAMCAST
    // (la Dreamcast los lee del CD: en el romdisk -que queda en RAM entero- eran 777 KB para usar uno. Sin el disco
    //  -el .elf suelto en un emulador- queda el ingles)
    (void)skinDir;
    s_folder = "/cd/languages/";
    return;
#endif
    // res/Skins/<skin>/ -> res/languages/ (PC, web, Android, 3DS: res/ entero va con el programa).
    // Symbian instala todo en la carpeta privada -> <privada>\languages\ (ver los .pkg).
    const char sep = skinDir.empty() ? '/' : skinDir[skinDir.size() - 1];
    const std::string mark = std::string(1, sep) + "Skins" + sep;
    const size_t p = skinDir.rfind(mark);
    if (p != std::string::npos) s_folder = skinDir.substr(0, p + 1) + "languages" + sep;
    else                         s_folder = skinDir + "languages" + sep;
}

unsigned W3dLanguagePhrases(W3dLanguage id){
    id = Valid((int)id);
    Load(id);
    return s_tables[id].n;
}

// ---------------------------------------------------------------------------------------------------------------
//  BUSQUEDA. Los hashes del ingles y una biseccion sobre la tabla del idioma activo (ordenada al cargarla).
// ---------------------------------------------------------------------------------------------------------------
const char* T(const char* in){
    if (!in) return "";
    if (g_language == W3dLangEN) return in;          // el ingles ES la clave: no hay nada que buscar
    Load(g_language);
    const W3dLangLoadedTable& t = s_tables[g_language];
    if (!t.n) return in;
    W3dLangPhrase q;
    Hashes((const unsigned char*)in, strlen(in), q.a, q.b);
    int lo = 0, hi = (int)t.n - 1;
    while (lo <= hi){
        const int med = lo + (hi - lo) / 2;
        const int c = ComparePhrase(&q, &t.phrases[med]);
        if (c == 0) return t.phrases[med].text;
        if (c < 0) hi = med - 1; else lo = med + 1;
    }
    return in;                                     // no esta traducida -> ingles, que es lo correcto y no rompe
}
