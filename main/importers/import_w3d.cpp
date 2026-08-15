#include "importers/import_w3d.h"
#include "objects/ObjectMode.h"   // W3dNombresRepararEscena (duplicados de archivos ya guardados)
#include <math.h>                    // sqrtf: normal de la clave geometrica de los UV groups
#include "render/OpcionesRender.h"   // RenderType / g_redraw: son del editor
#include "stb/stb_image.h" // stb ahora vive en libs/Whisk3DCore/thirdparty
#include "script/W3dScript.h"   // colgar el <proyecto>.lua al objeto Script/gamepad
#include "io/W3dRecursos.h"   // listas de carga (cierre de proyecto) — Core
#include "animation/Animation.h" // auto modo-juego al abrir un proyecto con scripts
// ============================================================================
//  W3D_SIN_EDITOR — ESTE MISMO LECTOR, ADENTRO DE UN JUEGO COMPILADO.
//
//  Un juego 3D exportado tiene que cargar SU escena con el MISMO codigo que la
//  carga en el editor: si el runtime tuviera su propio lector, el juego se veria
//  distinto del Play en cuanto alguno de los dos cambiara. Asi que el .cpp es
//  uno solo y el juego lo compila con -DW3D_SIN_EDITOR, que apaga lo que solo
//  existe con editor delante:
//     - el LAYOUT de viewports (Viewport3D/Timeline/UVEditor/Console/IDE...),
//     - el icono de la VENTANA (SDL_SetWindowIcon) y el `cfg` del editor,
//     - la config de la tarjeta "Juego" (que en el juego ya esta resuelta),
//     - la SESION guardada (frame + seleccion del usuario),
//     - y la apertura de un proyecto EN CALIENTE (ReiniciarEscena / AbrirW3D).
//  Todo el resto -- objetos, mallas, materiales, animaciones, constraints,
//  curvas, camaras, particulas, culling y visibilidad -- es EL MISMO codigo.
//  La puerta de entrada del juego es W3dProyectoCargarEscena3D (mas abajo).
// ============================================================================
#ifndef W3D_SIN_EDITOR
#include "ViewPorts/Editor2D.h"  // layouts con Editor2D/Timeline (proyectos de juego)
#include "ViewPorts/LayoutInput.h" // LayoutPorDefecto: el fallback de un .w3d sin bloque Layout
#include "ViewPorts/Timeline.h"
#include "ViewPorts/UVEditor.h"  // BuildLayout: faltaban en el mapeo de lectura y un
#include "ViewPorts/Console.h"   // layout guardado con UV/Console caia al default
#include "ViewPorts/IDE.h"       // layouts con el editor de texto de scripts (IDE)
#endif
#include "w3dlog.h"              // avisos de layout tambien al log (los ve la Console)
#include "objects/UI.h"          // la UI del proyecto (rama UI { archivo: ... })
#include "objects/Empty.h"       // el nodo generico ("tipo": "objeto") vuelve como Empty
#include "objects/Instance.h"    // Instance ("tipo": "Instance"): usado antes del include de mas abajo
#include "objects/LOD.h"         // objeto LOD (un hijo por distancia a la camara)
#include "objects/Culling.h"     // objeto Culling (frustum culling de sus hijos)
#include "objects/Particulas.h"  // objeto Particulas (emisor de billboards del Core)
#include "objects/VisZona.h"     // objeto VisZona (celda de visibilidad: grilla/volumenes/curva)
#include "io/UI2DFormato.h"
#include "W3dEscena.h"           // escenaInicial / modoEscenas (multi-escena)
#include "W3dPaletas.h"          // paletas del PROYECTO (raiz "paletas" del .w3d v3)
#include "io/GuardarW3D.h"       // g_proyIcono: el icono del juego (ruta externa)
#include "io/W3dContenedor.h"    // FORMATO v4: el .w3d es un zip que se MONTA (no se extrae)
#include "io/W3dZip.h"           // W3dZipEs: detectar el zip sin levantar el archivo entero
#include "io/W3dMalla.h"         // .w3dm: la geometria propia (reemplaza al GLB como formato de guardado)
#include "objects/Materials.h"   // bloque raiz "materiales" (antes viajaban dentro del GLB)
#include "gfx/w3dGraphics.h"     // gfx::Mezcla: acotar el modo de mezcla que venga del archivo
#include "objects/Textures.h"
#include "importers/import_obj.h" // EncolarTextura: la carga de texturas es DIFERIDA (1 por frame)
#include "w3dFilesystem.h"
#include "W3dAviso.h"            // W3dAvisof: los problemas de carga TIENEN que verse en PANTALLA
#include "W3dNombres.h"          // W3dNombreUnico: el nombre del constraint migrado, en el espacio del objeto

//ESTO DESPUES TIENE QUE IR A UN ARCHIVO SEPARADOOOOO
#ifndef W3D_SYMBIAN  // icono de la ventana via SDL: solo desktop (en Symbian no hay ventana SDL)
SDL_Surface* LoadSurfaceSTB(const std::string& path) {

    int width, height, channels;

    // por ReadFileBytes y NO por stbi_load(path): asi el icono del proyecto sale
    // igual de ADENTRO del contenedor .w3d ("proyecto/icono.png") que de un
    // archivo suelto. stbi_load abre con fopen y no ve el montaje.
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(path, datos) || datos.empty())
        return NULL;
    stbi_uc* pixels = stbi_load_from_memory(&datos[0], (int)datos.size(),
                                            &width, &height, &channels, 4);

    if (!pixels)
        return NULL;

    // superficie DUENIA de su memoria: CreateRGBSurfaceFrom NO adopta 'pixels'
    // (el buffer de stb quedaba fugado en cada apertura de proyecto). Se copia
    // y se libera el buffer de stb aca mismo.
    SDL_Surface* surface = SDL_CreateRGBSurface(
        0, width, height, 32,
        0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    if (surface) {
        for (int y = 0; y < height; y++)
            memcpy((Uint8*)surface->pixels + (size_t)y * surface->pitch,
                   pixels + (size_t)y * width * 4, (size_t)width * 4);
    }
    stbi_image_free(pixels);
    return surface;
}
#endif // W3D_SYMBIAN (LoadSurfaceSTB via SDL)

// ============================================
// Helpers
// ============================================
// std::map::at() es C++11 y el STLport de RVCT (Symbian) no lo tiene. Este es el
// equivalente C++03: como los call sites ya chequean count() antes, la clave existe;
// si faltara devuelve "" en vez de tirar excepcion (mas robusto: el .w3d es editable
// a mano y un archivo corrupto NO puede colgar el editor).
static const std::string& w3dMapAt(const std::map<std::string,std::string>& m, const std::string& k){
    static const std::string empty;
    std::map<std::string,std::string>::const_iterator it = m.find(k);
    return it != m.end() ? it->second : empty;
}

float GetFloatOrDefault(const std::map<std::string,std::string>& props, const std::string& k, float def){
    std::map<std::string,std::string>::const_iterator it = props.find(k);
    if(it==props.end() || it->second.empty()) return def;
    const char* s = it->second.c_str(); char* end = 0;
    double v = strtod(s, &end);
    return (end == s) ? def : (float)v;   // nada parseable -> el default del caller (no 0)
}

int GetIntOrDefault(const std::map<std::string,std::string>& props, const std::string& k, int def){
    std::map<std::string,std::string>::const_iterator it = props.find(k);
    if(it == props.end() || it->second.empty()) return def;
    const char* s = it->second.c_str(); char* end = 0;
    long v = strtol(s, &end, 10);
    return (end == s) ? def : (int)v;
}

bool GetBoolOrDefault(const std::map<std::string,std::string>& props, const std::string& key, bool def=false){
    std::map<std::string,std::string>::const_iterator it = props.find(key);
    if(it == props.end()) return def;

    std::string val = it->second;
    return (val == "true" || val == "1");
}

GLenum GetLightIDOrDefault(std::string name, GLenum defaultLight){
    // "GL_LIGHT0".."GL_LIGHT7" -> GL_LIGHT0 + n: los enum GL de luz son consecutivos (spec GL)
    if (name.size() == 9 && name.compare(0, 8, "GL_LIGHT") == 0) {
        char d = name[8];
        if (d >= '0' && d <= '7') return (GLenum)(GL_LIGHT0 + (d - '0'));
    }
    return defaultLight;
}

std::string Unquote(const std::string& s){
    if(s.size()>=2 && ((s[0]=='"' && s[s.size()-1]=='"') || (s[0]=='\'' && s[s.size()-1]=='\'')))
        return s.substr(1,s.size()-2);
    return s;
}

std::string GetFilePath(const std::string& raw){
    // 1. Sacar comillas si hay
    std::string filename = Unquote(raw);

    // 2. Obtener base dir del archivo .w3d
    std::string baseDir;
    size_t pos = w3dPath.find_last_of("/\\");
    if (pos != std::string::npos)
        baseDir = w3dPath.substr(0, pos + 1);
    else
        baseDir = "";

    // 3. Unir rutas
    return baseDir + filename;
}

// ============================================
// Tokenizer
// ============================================
std::vector<std::string> Tokenize(const std::string& src){
    std::vector<std::string> T;
    size_t i = 0;
    while(i < src.size()){
        char c = src[i];
        if(isspace((unsigned char)c)){ ++i; continue; }
        if(c == '/' && i+1 < src.size() && src[i+1] == '/'){ i+=2; while(i<src.size()&&src[i]!='\n') ++i; continue; }
        if(c == '#'){ while(i<src.size()&&src[i]!='\n') ++i; continue; }
        if(c=='{' || c=='}'){ T.push_back(std::string(1,c)); ++i; continue; }
        if(c==':'){ T.push_back(":"); ++i; continue; }

        if(c=='"'||c=='\''){
            char q=c; ++i; std::string cur;
            while(i<src.size()&&src[i]!=q){
                if(src[i]=='\\' && i+1<src.size()){ ++i; cur.push_back(src[i]); }
                else cur.push_back(src[i]);
                ++i;
            }
            if(i<src.size()&&src[i]==q) ++i;
            T.push_back(std::string("\"")+cur+"\"");
            continue;
        }

        std::string cur;
        while(i<src.size()){
            char d=src[i];
            if(isspace((unsigned char)d)||d=='{'||d=='}'||d==':'||d=='#') break;
            if(d==','){ ++i; break; }
            cur.push_back(d); ++i;
        }
        if(!cur.empty()) T.push_back(cur);
    }
    return T;
}

// ============================================
// Node & Find
// ============================================
Node* Find(Node* root, const std::string& type){
    if(!root) return NULL;
    if(root->type == type) return root;
    for(size_t _i=0;_i<root->children.size();_i++){
        Node* c = root->children[_i];
        if(Node* f = Find(c,type)) return f;
    }
    return NULL;
}

Node* ParseNode(std::vector<std::string>& tk, size_t& i){
    if(i >= tk.size()) return NULL;
    Node* n = new Node();
    n->type = tk[i++];
    if(i>=tk.size() || tk[i]!="{") return n;
    i++;
    while(i<tk.size()){
        if(tk[i]=="}"){ i++; break; }
        if(i+2<tk.size() && tk[i+1]==":"){ n->props[tk[i]] = Unquote(tk[i+2]); i+=3; continue; }
        if(i+1<tk.size() && tk[i+1]!=":" && tk[i+1]!="{"){ Node* child = new Node(); child->type = tk[i]; n->children.push_back(child); i++; continue; }
        if(i+1<tk.size() && tk[i+1]=="{"){ n->children.push_back(ParseNode(tk,i)); continue; }
        i++;
    }
    return n;
}

// ============================================
// Builders
// ============================================
// EL LAYOUT DE VIEWPORTS es del EDITOR: un juego compilado no tiene outliner,
// timeline ni editor de UVs. Con -DW3D_SIN_EDITOR ni se compila (asi el juego no
// arrastra la mitad del editor por el link).
#ifndef W3D_SIN_EDITOR
void ApplyViewport3DProps(Viewport3D* v, const std::map<std::string,std::string>& p){
    if(!v) return;

    // UN LAYOUT CORRUPTO NO PUEDE COLGAR EL EDITOR. Antes esto era std::stof
    // directo: un "posX: hola" (o un numero fuera del rango de float) tiraba una
    // excepcion que NADIE atrapa -> el editor se cerraba al abrir el proyecto.
    // GetFloatOrDefault ya hace el try/catch y cae al default. El .w3d se vende
    // como editable a mano: el archivo NO puede ser una fuente de crash.
    // C++03 (RVCT/Symbian): sin lambdas -> functors locales con la MISMA sintaxis de llamada
    struct FGet { const std::map<std::string,std::string>& p; FGet(const std::map<std::string,std::string>& m):p(m){}
        float operator()(const std::string& k, float def=0.0f) const { return GetFloatOrDefault(p, k, def); } } F(p);
    struct BGet { const std::map<std::string,std::string>& p; BGet(const std::map<std::string,std::string>& m):p(m){}
        bool operator()(const std::string& k, bool def=false) const {
            std::map<std::string,std::string>::const_iterator it = p.find(k);
            return it != p.end() ? (it->second=="true" || it->second=="1") : def; } } B(p);

    /// --- bools ---
    if(p.count("orthographic"))           v->orthographic = B("orthographic");
    if(p.count("ViewFromCameraActive"))   v->ViewFromCameraActive = B("ViewFromCameraActive");
    if(p.count("showOverlays"))           v->showOverlays = B("showOverlays");
    // ("ShowUi" se dio de baja: los .w3d viejos que la traigan caen aca como cualquier
    //  clave desconocida -se ignora- y el chrome del viewport se dibuja SIEMPRE)
    if(p.count("showFloor"))              v->showFloor = B("showFloor");
    if(p.count("showYaxis"))              v->showYaxis = B("showYaxis");
    if(p.count("showXaxis"))              v->showXaxis = B("showXaxis");
    if(p.count("CameraToView"))           v->CameraToView = B("CameraToView");
    if(p.count("showOrigins"))            v->showOrigins = B("showOrigins");
    if(p.count("show3DCursor"))           v->show3DCursor = B("show3DCursor");
    if(p.count("ShowRelantionshipsLines"))v->ShowRelantionshipsLines = B("ShowRelantionshipsLines");
    if(p.count("limpiarPantalla"))        v->limpiarPantalla = B("limpiarPantalla");

    /// --- floats/doubles ---
    if(p.count("nearClip"))       v->nearClip       = F("nearClip", 0.01f);
    if(p.count("farClip"))        v->farClip        = F("farClip", 1000.0f);
    if(p.count("aspect"))         v->aspect         = F("aspect", 1.0f);

    if(p.count("orbitDistance")) v->orbitDistance = F("orbitDistance", 10.0f);
    // ORBITA A CERO = vista degenerada (la camara cae sobre el pivote y no se ve
    // nada). Un archivo a mano puede traerlo; se acota a algo usable.
    if(v->orbitDistance < 0.001f) v->orbitDistance = 0.001f;

    // ROTACION DE LA VISTA. El guardado nuevo escribe el QUATERNION (rotQX..W), que
    // es lo que la vista realmente guarda: pasar por euler y volver NO es la
    // identidad, asi que el encuadre que el usuario dejo no era el que volvia.
    // Los rotX/rotY/rotZ del formato de texto viejo se siguen leyendo (retrocompat).
    if(p.count("rotQW") || p.count("rotQX") || p.count("rotQY") || p.count("rotQZ")){
        Quaternion q(F("rotQW", 1.0f), F("rotQX", 0.0f), F("rotQY", 0.0f), F("rotQZ", 0.0f));
        // un quaternion NULO (todo ceros, de un archivo roto) no rota: no rompe nada
        // pero deja una matriz degenerada. Se cae a la vista default.
        float n2 = q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z;
        if(n2 > 1e-8f) v->viewRot = q;
    } else {
        v->viewRot = Quaternion::FromEulerYXZ(
            GetFloatOrDefault(p, "rotY", -30.0f),
            GetFloatOrDefault(p, "rotX", -30.0f),
            GetFloatOrDefault(p, "rotZ", 0.0f)
        );
    }

    if(p.count("posX")) v->pivot.x = F("posX"); //PivotX
    if(p.count("posY")) v->pivot.z = F("posY");
    if(p.count("posZ")) v->pivot.y = F("posZ");

    v->RecalcOrbitPosition();

    if(p.count("view"))  // string → enum
         v->view = StringToRenderType(w3dMapAt(p, "view"));
}

// MODO + VISTA del timeline guardados en su hoja del layout (ver el comentario grande de
// LayoutEscribirNodo en GuardarW3D.cpp: por que van con la hoja y no en "sesion" ni en el config).
// Sin las props (layout viejo, o escrito a mano) queda el default del constructor: dope sheet con
// el encuadre de siempre -- por eso el default de cada GetFloatOrDefault es el valor VIVO del
// timeline recien construido y no una constante repetida aca.
// Cualquier palabra que no sea "curvas" cae a dope: un layout raro no puede dejar el timeline en
// un modo invalido. Y los cuatro numeros de la vista pasan SIEMPRE por Timeline::VistaAplicar, que
// es la que valida (nan/inf/zoom<=0/desplazamientos absurdos) y acota con los topes de la rueda:
// un .w3d es editable a mano y no puede colgar el editor ni dejar el timeline invisible.
void ApplyTimelineProps(Timeline* t, const std::map<std::string,std::string>& p){
    if(!t) return;
    if(p.count("modo"))
        t->modo = (w3dMapAt(p, "modo") == "curvas") ? Timeline::TL_MODO_CURVAS : Timeline::TL_MODO_DOPE;
    t->VistaAplicar(GetFloatOrDefault(p, "zoomFrame",   t->pxPerFrame),
                    GetFloatOrDefault(p, "frameIzq",    t->viewStartF),
                    GetFloatOrDefault(p, "zoomValor",   t->pxPerUnit),
                    GetFloatOrDefault(p, "centroValor", t->viewCenterV));
}

// ----------------------------- Builders -----------------------------
ViewportBase* BuildLayout(Node* n){
    if(!n) return NULL;
    std::cout << "[BuildLayout] node=" << n->type << std::endl;

    if(n->type == "Viewport3D"){
        Viewport3D* v = new Viewport3D();
        ApplyViewport3DProps(v, n->props);
        return v;
    }
    if(n->type == "Outliner")  return new Outliner();
    if(n->type == "Properties")  return new Properties();
    if(n->type == "Editor2D")  return new Editor2D();
    if(n->type == "Timeline"){
        Timeline* t = new Timeline();
        ApplyTimelineProps(t, n->props);
        return t;
    }
    if(n->type == "UVEditor")  return new UVEditor();
    if(n->type == "Console")   return new Console();
    if(n->type == "IDE")       return new IDE();

    if(n->type == "ViewportRow" || n->type == "ViewportColumn"){
        bool isRow = (n->type == "ViewportRow");
        float split = GetFloatOrDefault(n->props, "Split", 0.5f);

        // --- Fallback: si el parser dejó A:/B: como props en lugar de children,
		//     convertimos esas props en nodos temporales para BuildLayout. ---
		if(n->children.size() < 2){
			bool made = false;
			// intenta crear children desde props "A" y "B" si existen
			if(n->props.count("A") || n->props.count("B")){
				if(n->props.count("A")){
					Node* a = new Node();
					a->type = n->props["A"];
					n->children.insert(n->children.begin(), a);
					made = true;
				}
				if(n->props.count("B")){
					Node* b = new Node();
					b->type = n->props["B"];
					n->children.push_back(b);
					made = true;
				}
			}
			// si hicimos algo, seguimos adelante y BuildLayout procesará estos nodos
			if(made){
				std::cerr << "[BuildLayout] Notice: creado children temporales desde props A/B para " << n->type << std::endl;
			}
		}

        ViewportBase* A = NULL;
        ViewportBase* B = NULL;

        if(n->children.size() > 0) A = BuildLayout(n->children[0]);
        if(n->children.size() > 1) B = BuildLayout(n->children[1]);

        // TRES O MAS HIJOS: un contenedor es BINARIO, pero un .w3d escrito a mano (el formato
        // se vende como editable) puede listar 3 paneles seguidos. Antes se tomaban los dos
        // primeros y el resto SE PERDIA EN SILENCIO: era la causa de "el panel de propiedades
        // no esta donde lo puse". Ahora los extras se ANIDAN en contenedores del mismo tipo:
        //   Column { 3D, Outliner, Properties }  ->  Column{ 3D, Column{ Outliner, Properties } }
        // con lo cual el orden del archivo se respeta (el primero arriba/izquierda) y no
        // desaparece ningun panel. El split de los anidados es el DEFAULT del contenedor
        // (0.40 en columna: el de arriba mas chico, que es lo que se quiere para el outliner).
        for (size_t k = 2; k < n->children.size(); k++) {
            ViewportBase* C = BuildLayout(n->children[k]);
            if (!C) continue;
            B = isRow ? (ViewportBase*)new ViewportRow(B ? B : new Viewport3D(), C, 0.5f)
                      : (ViewportBase*)new ViewportColumn(B ? B : new Viewport3D(), C, 0.4f);
        }
        if (n->children.size() > 2)
            w3dLogfW("[W3D] layout: '%s' traia %d paneles (un contenedor es binario): los extras "
                     "se anidaron en su propio contenedor en vez de perderse",
                     n->type.c_str(), (int)n->children.size());

        if(!A) A = new Viewport3D();
        if(!B) B = new Viewport3D();

        if(isRow)  return new ViewportRow(A,B,split);
        else       return new ViewportColumn(A,B,split);
    }

    // tipo DESCONOCIDO (ej: archivo guardado por una version mas nueva del
    // editor): NO se aborta el layout entero, va un viewport 3D en su lugar
    // y queda el aviso en consola y en el log (visible en la Console del editor)
    std::cerr << "[BuildLayout] AVISO: viewport desconocido '" << n->type
              << "', usando Viewport3D en su lugar" << std::endl;
    w3dLogfW("[W3D] layout: viewport desconocido '%s', va un Viewport3D en su lugar", n->type.c_str());
    return new Viewport3D();
}
#endif // W3D_SIN_EDITOR (layout de viewports)

// liberar un arbol de nodos parseados (el texto viejo y los snippets de layout)
static void FreeNode(Node* n) {
    if (!n) return;
    for (size_t i = 0; i < n->children.size(); i++) FreeNode(n->children[i]);
    delete n;
}

// ===========================================================================
//  APLICACION de los campos del PROYECTO — la UNICA fuente de verdad por campo.
//  Los tres formatos de .w3d (JSON plano nuevo, zip v2 viejo, texto viejo)
//  terminan llamando a ESTAS funciones: cero duplicacion de logica entre caminos.
// ===========================================================================

// carpeta del .w3d abierto (rutas relativas del proyecto)
static std::string DirDelProyecto() {
    size_t s = w3dPath.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : w3dPath.substr(0, s);
}

// el FPS del proyecto (timeline/animaciones). f < 1 = campo ausente o roto:
// queda el default del editor (no se rompe nada por un numero invalido).
static void AplicarFps(int f) {
    if (f < 1) return;
    if (f > 120) f = 120;
    AnimFPS = f;
}

// el "fullscreen" del archivo NO cambia la ventana al abrir (
// un proyecto no tiene que arrancar a pantalla completa). Solo actualiza cfg
// para que GuardarW3D lo re-escriba con el proyecto (y lo lea el juego final).
#ifndef W3D_SIN_EDITOR
static void AplicarFullscreen(bool v) { cfg.fullscreen = v; }
#else
// en el juego compilado el modo de ventana ya lo decidio "Compilar juego" (lo
// hornea el main.cpp generado): el campo del archivo no lo puede cambiar.
static void AplicarFullscreen(bool) {}
#endif

// estado de REPRODUCCION guardado en el .w3d v3 (campo raiz "reproduciendo"):
// -1 = ausente (archivos viejos) -> auto-play de siempre si hay scripts;
//  0 = guardado en PAUSA -> abre en pausa; 1 = guardado en PLAY -> abre en play.
// Lo consume el pie comun de AbrirW3D; se resetea a -1 en cada apertura.
static int g_proyReproduciendo = -1;

// ---------------------------------------------------------------------------
//  BLOQUE "sesion" (v3, opcional): donde estaba trabajando el usuario en ESTE
//  proyecto -- el frame actual y la seleccion. Ver el comentario grande de
//  EscribirSesion (GuardarW3D.cpp) para el reparto .w3d / config.ini.
//
//  Se GUARDA en estos globales durante el parseo y se APLICA en el pie de
//  AbrirW3D, no aca: la seleccion es por NOMBRE y los nombres recien quedan
//  definitivos despues de W3dNombresRepararEscena (un .w3d puede traer
//  duplicados y el reparador renombra). Un .w3d viejo no trae el bloque:
//  g_sesFrame queda en -1 y no se toca nada (retrocompat).
// ---------------------------------------------------------------------------
static int g_sesFrame = -1;                     // -1 = el archivo no trae "sesion"
static std::string g_sesActivo;                 // nombre del objeto ACTIVO ("" = ninguno)
static std::vector<std::string> g_sesSeleccion; // nombres de los objetos seleccionados

static void SesionReset() { g_sesFrame = -1; g_sesActivo.clear(); g_sesSeleccion.clear(); }

// APLICA lo leido. Se llama en el pie de AbrirW3D, con los nombres ya definitivos.
//  - frame: se ACOTA (viene de un archivo editable a mano; un frame negativo o
//    absurdo dejaria el playhead fuera de la regla y toda evaluacion de curvas
//    trabajando con un numero que no existe).
//  - seleccion: por NOMBRE. El que no aparece se IGNORA en silencio para el
//    usuario (queda en el log): la seleccion es una comodidad y jamas un motivo
//    para no abrir el proyecto. Se resuelve con FindObjectByName, que es el mismo
//    lookup que usan los targets y los scripts -> con homonimos entre escenas cae
//    en el PRIMERO, exactamente igual que ellos.
//  - activo: si el nombre no resuelve, ObjActivo queda como lo dejo la seleccion
//    (Object::Seleccionar ya lo va poniendo) y si no habia nada, en NULL.
static void SesionAplicar() {
    if (g_sesFrame < 0) return;                      // el archivo no traia el bloque
    const int kFrameMax = 1000000;                   // la misma cota que el resto de los frames del lector
    int f = g_sesFrame;
    if (f > kFrameMax) {
        w3dLogfW("[W3D] sesion: 'frame' vale %d, fuera de [0..%d] -> se acota", f, kFrameMax);
        f = kFrameMax;
    }
    CurrentFrame = f;
    DeseleccionarTodo();
    ObjActivo = NULL;
    int perdidos = 0;
    for (size_t i = 0; i < g_sesSeleccion.size(); i++) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, g_sesSeleccion[i]) : NULL;
        if (o) o->Seleccionar();
        else   perdidos++;
    }
    ObjActivo = (!g_sesActivo.empty() && SceneCollection) ? FindObjectByName(SceneCollection, g_sesActivo) : NULL;
    if (perdidos)
        w3dLogfW("[W3D] sesion: %d objeto(s) de la seleccion guardada ya no existen -> se ignoran", perdidos);
    w3dLogf("[W3D] sesion: frame %d, %d objeto(s) seleccionados, activo '%s'",
            CurrentFrame, (int)ObjSelects.size(), ObjActivo ? ObjActivo->name.c_str() : "");
    g_redraw = true;
}

// MULTI-ESCENA: la escena que ARRANCA + el modo. Sin esto, con varias UI el
// motor no sabia cual es la inicial (arrancaba la primera por orden).
static void AplicarEscenaInicial(const std::string& n) { W3dEscenaSetInicial(n); }
static void AplicarModoEscenas(bool v) { W3dEscenaSetModo(v); }

// ICONO del juego: ruta relativa a la carpeta del .w3d (externa SIEMPRE).
// Queda como icono del PROYECTO (g_proyIcono: tarjeta Juego / Compilar juego)
// y como icono de la VENTANA. Vacio o ilegible -> el icono default de Whisk3D.
// 'resuelta' ya paso por RutaJson: en v4 es la ENTRADA del contenedor
// ("proyecto/icono.png"), en v3 la ruta de disco. g_proyIcono guarda ESO, que es
// lo que sabe leer ReadFileBytes y lo que el guardado vuelve a meter adentro.
static void AplicarIcono(const std::string& resuelta) {
#if defined(W3D_SIN_EDITOR) || defined(W3D_SYMBIAN)
    // el juego compilado ya lleva SU icono (lo genera "Compilar juego" y lo pone
    // el main.cpp generado con SDL_SetWindowIcon): el del proyecto no aplica.
    (void)resuelta;
#else
    const std::string& rel = resuelta;
    g_proyIcono = rel;
    std::string ruta;
    if (!rel.empty()) {
        ruta = rel;
        if (!w3dFileSystem::FileExists(ruta)) {
            w3dLogfW("[W3D] icono no encontrado: %s (queda el default)", ruta.c_str());
            ruta.clear();
        }
    }
    if (ruta.empty()) ruta = w3dFileSystem::GetResDir() + "/Whisk3D.png";
    SDL_Surface* icon = LoadSurfaceSTB(ruta);
    if (icon) {
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    }
#endif
}

// una ESCENA UI del proyecto (.w3dui EXTERNO): la carga y la cuelga de la
// escena. Si el archivo falta o no parsea, la escena se OMITE con aviso y el
// RESTO del proyecto abre igual (nunca se aborta todo por una UI rota).
// 'refRel' es la ruta tal cual la referencia el .w3d: si es relativa se guarda
// como UI::archivoW3dui CON su subcarpeta ("contenido/menu.w3dui") para que el
// re-guardado escriba la escena EN EL MISMO lugar (UI2DCargar solo recuerda el
// nombre pelado y una escena bajo contenido/ se re-guardaba en la raiz).
// ESCENAS UI QUE NO SE PUDIERON CARGAR EN ESTA APERTURA (vacio = ninguna). Tercer miembro de
// la misma familia que W3dAjenos::noCargo (malla) y VertexAnimation::noCargo (vertex anim):
// el nodo NO se crea, asi que el proximo guardado escribe un proyecto.json SIN esa escena y
// el usuario la pierde de vista sin enterarse (los bytes del .w3dui sobreviven por
// PreservarPasajeras, pero ya no los referencia nadie: son un huerfano invisible). Con esto
// prendido el guardado se FRENA y el .w3d de origen no se toca. Lo limpia ReiniciarEscena.
std::string g_w3dUINoCargo;

static UI* CargarUIProyecto(const std::string& ruta, const std::string& refRel = std::string()) {
    if (ruta.empty()) {
        w3dLogfW("[W3D] nodo UI sin 'archivo': escena omitida");
        return NULL;
    }
    if (!w3dFileSystem::FileExists(ruta)) {
        w3dLogfE("[W3D] falta la escena UI %s: se omite (el resto del proyecto abre)", ruta.c_str());
        if (g_w3dUINoCargo.empty()) g_w3dUINoCargo = ruta;
        W3dAvisof(true, "Falta la escena UI '%s': el proyecto abre sin ella y NO lo voy a guardar encima (ver whisk3d.log)",
                  W3dNombreCorto(ruta).c_str());
        return NULL;
    }
    UI* u = UI2DCargar(ruta);
    if (!u) {
        w3dLogfE("[W3D] la escena UI %s no parsea: se omite", ruta.c_str());
        if (g_w3dUINoCargo.empty()) g_w3dUINoCargo = ruta;
        W3dAvisof(true, "La escena UI '%s' no se pudo leer: el proyecto abre sin ella y NO lo voy a guardar encima (ver whisk3d.log)",
                  W3dNombreCorto(ruta).c_str());
        return NULL;
    }
    if (!refRel.empty() && refRel[0] != '/' && !(refRel.size() > 1 && refRel[1] == ':'))
        u->archivoW3dui = refRel;
    return u;
}

// el LAYOUT desde su forma de TEXTO "Layout { ... }" (el arbol VIVO serializado
// por el guardado, ver LayoutSerializarVivo en GuardarW3D.cpp): se reconstruye
// con el pipeline probado del formato viejo (Tokenize + ParseNode + BuildLayout).
// Un nombre corto ("2d", archivos guardados antes de que el layout vivo se
// persistiera) o vacio cae al template estandar de siempre.
static void AplicarLayoutTexto(const std::string& cual) {
#ifdef W3D_SIN_EDITOR
    (void)cual;   // el juego compilado no tiene layout de viewports (ver W3D_SIN_EDITOR arriba)
#else
    const char* texto =
        "Layout {\n"
        "    ViewportRow {\n"
        "        ViewportColumn {\n"
        "            Editor2D\n"
        "            Timeline\n"
        "            Split: 0.8\n"
        "        }\n"
        "        ViewportColumn {\n"
        "            Outliner\n"
        "            Properties\n"
        "            Split: 0.4\n"
        "        }\n"
        "        Split: 0.72\n"
        "    }\n"
        "}\n";
    if (cual.compare(0, 6, "Layout") == 0)
        texto = cual.c_str();   // layout VIVO guardado en el archivo
    std::vector<std::string> tokens = Tokenize(texto);
    size_t i = 0;
    Node* layout = ParseNode(tokens, i);
    if (layout && !layout->children.empty())
        rootViewport = BuildLayout(layout->children[0]);
    FreeNode(layout);
#endif
}

void ApplyCommonProps(Object* obj, const std::map<std::string,std::string>& p){
    if(!obj) return;

    // C++03 (RVCT/Symbian): sin lambdas -> functor local, misma sintaxis de llamada
    struct BGet { const std::map<std::string,std::string>& p; BGet(const std::map<std::string,std::string>& m):p(m){}
        bool operator()(const std::string& k, bool def=false) const {
            std::map<std::string,std::string>::const_iterator it = p.find(k);
            return it != p.end() ? (it->second=="true" || it->second=="1") : def; } } B(p);

    // Nombre
    if(p.count("name")){
		// CARGA: el nombre entra CRUDO (el archivo puede traer duplicados de versiones
		// viejas). Los duplicados los repara de una sola vez W3dNombresRepararEscena al
		// final de AbrirW3D, que ademas arrastra las refs de scripts y los targets.
		obj->SetNameCrudo(w3dMapAt(p, "name"));
	}

    // Posición
    if(p.count("x")) obj->pos.x = GetFloatOrDefault(p,"x");
    if(p.count("y")) obj->pos.z = GetFloatOrDefault(p,"y");
    if(p.count("z")) obj->pos.y = GetFloatOrDefault(p,"z");

    // Rotación
    if(p.count("rx") || p.count("ry") || p.count("rz")){
        //rz y ry estan invertidos por openGl
        Quaternion qX = Quaternion::FromAxisAngle(Vector3(1, 0, 0), GetFloatOrDefault(p,"rx", 0)); // Pitch (Eje X)
        Quaternion qY = Quaternion::FromAxisAngle(Vector3(0, 1, 0), GetFloatOrDefault(p,"rz", 0)); // Yaw (Eje Y)
        Quaternion qZ = Quaternion::FromAxisAngle(Vector3(0, 0, 1), GetFloatOrDefault(p,"ry", 0)); // Roll (Eje Z)
        obj->SetRot(qY * qX * qZ); // Aplicación: (Z) * (X) * (Y)
    }

    
    if(p.count("desplegado")) obj->desplegado = B("desplegado");
    // el "ojito" del outliner: el formato de texto no lo escribia nunca, pero un .w3d
    // a mano tiene que poder ocultar un objeto (mismo idioma que el JSON v4)
    if(p.count("visible")) obj->visible = B("visible");
    if(p.count("showRelantionshipsLines")) obj->showRelantionshipsLines = B("showRelantionshipsLines");

    // Escala
    if(p.count("scale")){
        float s = GetFloatOrDefault(p,"scale",1);
        obj->scale.x = s;
        obj->scale.y = s;
        obj->scale.z = s;
        //std::cout << "[ApplyCommonProps] scale = " << s << std::endl; // <-- para debug
    } 
    else {
        if(p.count("sx")) obj->scale.x = GetFloatOrDefault(p,"sx",1);
        if(p.count("sy")) obj->scale.y = GetFloatOrDefault(p,"sy",1);
        if(p.count("sz")) obj->scale.z = GetFloatOrDefault(p,"sz",1);
    }
}

// ===========================================================================
//  MIGRACION DEL OBJETO "Constraint" VIEJO (dado de baja)
//
//  Hasta esta version un constraint era un NODO del arbol que apuntaba al objeto
//  que queria modificar y le escribia pos/rot en el render. Ahora el constraint es
//  una PROPIEDAD del objeto (Object::constraints) y el puntero es la FUENTE.
//
//  *** LA INVERSION DE SEMANTICA ES TODO EL TRABAJO ***
//      viejo: el nodo apunta al DESTINO y la fuente es siempre la vista.
//      nuevo: el constraint VIVE EN el destino y el puntero es la FUENTE.
//  O sea que las propiedades del nodo viejo van SOBRE EL OBJETO APUNTADO, y no
//  sobre el nodo ni sobre el que lo tiene de target. Hacerlo al reves compila,
//  abre y no avisa nada: simplemente la escena deja de verse como corresponde (lo fija el
//  test 'consmigra').
//
//  POR QUE EN DOS TIEMPOS (crear un Empty ahora, migrar al final):
//   1. Mientras se lee el nodo todavia no existe el objeto apuntado (puede venir
//      despues en el archivo), asi que no hay donde poner el constraint.
//   2. Las dos ramas -texto y JSON- TIENEN que devolver un objeto VALIDO. Si el
//      lector devuelve NULL, BuildObjectRecursive hace 'return' temprano y se
//      lleva puesto EL SUBARBOL ENTERO, en silencio (test 'consmigrasubarbol').
//  Por eso el nodo viejo entra como Empty con su nombre y su transform, y recien
//  al final MigrarConstraintsViejos reparte las propiedades y lo saca de la escena
//  si no tiene hijos.
//
//  LOS DEFAULTS DE CADA FORMATO NO SE UNIFICAN: el texto trae 'usePitch' con
//  default true y el JSON 'pitch' con default false. El default es parte del
//  formato historico de cada uno; unificarlo cambiaria como abren archivos que hoy
//  se ven bien.
// ===========================================================================
struct PendConsViejo {
    Object*     nodo;        // el Empty que ocupa el lugar del nodo viejo
    std::string target;      // a QUIEN modificaba: ahi es donde va a vivir el constraint
    bool        horizontal;  // useHorizontal / "horizontal"  -> Billboard bbYaw
    bool        pitch;       // usePitch      / "pitch"       -> Billboard bbPitch
    bool        copyX, copyY, copyZ;   // copyPos* -> Copy Location con fuente = la vista
};
static std::vector<PendConsViejo> gPendConsViejo;

// LA MISMA mecanica para los dos lectores, para que no diverjan: el Empty + la
// entrada pendiente. El que llama le aplica despues el nombre y la transform por
// el camino comun de su formato (ApplyCommonProps / JsonComunes).
static Object* NodoConstraintViejo(Object* parent, const std::string& target,
                                   bool horizontal, bool pitch,
                                   bool copyX, bool copyY, bool copyZ) {
    Empty* e = new Empty(parent);
    PendConsViejo p;
    p.nodo = e;              p.target = target;
    p.horizontal = horizontal; p.pitch = pitch;
    p.copyX = copyX; p.copyY = copyY; p.copyZ = copyZ;
    gPendConsViejo.push_back(p);
    return e;
}

// ===========================================================================
//  PREFABS del formato texto (Biblioteca/Prefab/Clon):
//
//      Biblioteca {
//          Prefab { name: "caja"  Wavefront { filePath: caja.obj ... } }
//      }
//      Clon { prefab: "caja" name: "Caja07" x: 1.0 y: 2.0 z: 0.0 val_vida: "3" }
//
//  La Biblioteca declara subarboles UNA vez SIN instanciarlos; cada Clon los
//  instancia con OVERRIDES: cualquier prop del Clon (posicion, rotacion,
//  script, ref_/opt_/val_) pisa la homonima de la raiz del prefab, y los
//  hijos propios del Clon se AGREGAN a los del prefab. Un Clon de caja pesa
//  ~2 lineas contra el subarbol completo re-declarado. La geometria/anims ya
//  se comparten por ruta (MallaDatos/AnimDatos), asi que N clones = N nodos
//  livianos + 1 juego de datos.
//
//  La Biblioteca tiene que aparecer ANTES que sus Clones (el archivo se
//  construye de arriba hacia abajo). Sin override de name, el clon recibe el
//  nombre unico automatico (N clones homonimos romperian las refs).
//
//  El bind lua `instanciar("caja", x, y, z)` (Core) entra por el hook
//  W3dInstanciarPrefabHook, que se instala al cargar una Biblioteca; los
//  clones runtime cuelgan de la raiz de la escena. Coordenadas del MOTOR.
//
//  V4: el guardado materializa los clones como objetos completos (la fuente
//  de verdad del dedup declarativo es el .w3d de texto que emite la build del
//  proyecto). Los prefabs viven mientras vive el proyecto: ReiniciarEscena
//  los limpia.
// ===========================================================================
static void LeerScriptsDeProps(Object* obj, const std::map<std::string,std::string>& p); // definida abajo

static std::map<std::string, Node*> gPrefabs;   // nombre -> COPIA propia del nodo Prefab

static Node* PrefabCopiarNodo(const Node* n) {
    Node* c = new Node();
    c->type = n->type;
    c->props = n->props;
    for (size_t i = 0; i < n->children.size(); i++)
        if (n->children[i]) c->children.push_back(PrefabCopiarNodo(n->children[i]));
    return c;
}
static void PrefabBorrarNodo(Node* n) {
    if (!n) return;
    for (size_t i = 0; i < n->children.size(); i++) PrefabBorrarNodo(n->children[i]);
    delete n;
}
void W3dPrefabsLimpiar() {
    for (std::map<std::string, Node*>::iterator it = gPrefabs.begin(); it != gPrefabs.end(); ++it)
        PrefabBorrarNodo(it->second);
    gPrefabs.clear();
}
int W3dPrefabsRegistrados() { return (int)gPrefabs.size(); }

// instancia el prefab bajo 'parent': overrides = props del Clon (pisan la raiz),
// hijosExtra = hijos propios del Clon (se agregan). NULL si no hay tal prefab.
static Object* InstanciarPrefabNodo(const std::string& nombre,
                                    const std::map<std::string,std::string>& overrides,
                                    const std::vector<Node*>* hijosExtra,
                                    Object* parent) {
    std::map<std::string, Node*>::iterator it = gPrefabs.find(nombre);
    if (it == gPrefabs.end() || !it->second) {
        std::cerr << "[Clon] no hay un Prefab '" << nombre << "' en la Biblioteca\n";
        return NULL;
    }
    Node* raiz = NULL;
    for (size_t i = 0; i < it->second->children.size() && !raiz; i++)
        raiz = it->second->children[i];
    if (!raiz) {
        std::cerr << "[Clon] el Prefab '" << nombre << "' esta vacio\n";
        return NULL;
    }
    Node tmp;
    tmp.type  = raiz->type;
    tmp.props = raiz->props;
    tmp.props.erase("name");   // sin override, nombre unico automatico
    for (std::map<std::string,std::string>::const_iterator kv = overrides.begin();
         kv != overrides.end(); ++kv)
        if (kv->first != "prefab") tmp.props[kv->first] = kv->second;
    tmp.children = raiz->children;
    if (hijosExtra)
        for (size_t i = 0; i < hijosExtra->size(); i++)
            if ((*hijosExtra)[i]) tmp.children.push_back((*hijosExtra)[i]);
    // el MISMO camino que BuildObjectRecursive, pero devolviendo la raiz creada
    Object* obj = CreateObjectFromNode(&tmp, parent);
    if (!obj) return NULL;
    ApplyCommonProps(obj, tmp.props);
    LeerScriptsDeProps(obj, tmp.props);
    for (size_t i = 0; i < tmp.children.size(); i++)
        if (tmp.children[i]) BuildObjectRecursive(tmp.children[i], obj);
    return obj;
}

// el hook del bind lua instanciar() (Core). Coordenadas del MOTOR -> props del
// formato de texto (la vertical del texto es z y la profundidad y).
static Object* HookInstanciarPrefab(const char* nombre, const float* posMotor) {
    if (!nombre || !*nombre) return NULL;
    std::map<std::string,std::string> ov;
    if (posMotor) {
        char b[48];
        snprintf(b, sizeof(b), "%g", posMotor[0]); ov["x"] = b;
        snprintf(b, sizeof(b), "%g", posMotor[2]); ov["y"] = b;   // profundidad
        snprintf(b, sizeof(b), "%g", posMotor[1]); ov["z"] = b;   // vertical
    }
    return InstanciarPrefabNodo(nombre, ov, NULL, SceneCollection);
}

Object* CreateObjectFromNode(Node* n, Object* parent){
    if(!n) return NULL;

    const std::map<std::string,std::string>& p = n->props;

    // --- Biblioteca de PREFABS: declara subarboles SIN instanciarlos ---
    if (n->type == "Biblioteca") {
        for (size_t i = 0; i < n->children.size(); i++) {
            Node* c = n->children[i];
            if (!c || c->type != "Prefab") continue;
            std::string nombre = c->props.count("name") ? Unquote(w3dMapAt(c->props, "name")) : std::string();
            if (nombre.empty()) { std::cerr << "[Biblioteca] Prefab sin name (ignorado)\n"; continue; }
            std::map<std::string, Node*>::iterator viejo = gPrefabs.find(nombre);
            if (viejo != gPrefabs.end()) { PrefabBorrarNodo(viejo->second); gPrefabs.erase(viejo); }
            gPrefabs[nombre] = PrefabCopiarNodo(c);
        }
        W3dInstanciarPrefabHook = HookInstanciarPrefab;   // habilita instanciar() en lua
        return NULL;   // la Biblioteca no agrega objetos a la escena
    }
    // --- Clon: instancia un Prefab de la Biblioteca con overrides ---
    if (n->type == "Clon") {
        if (!n->props.count("prefab")) { std::cerr << "[Clon] falta prefab:\n"; return NULL; }
        InstanciarPrefabNodo(Unquote(w3dMapAt(n->props, "prefab")), n->props, &n->children, parent);
        return NULL;   // todo construido adentro (props + scripts + hijos)
    }

    // ‼ Se crea el objeto por tipo (mínimo switch posible)
    if(n->type=="Mesh")
        return NewMesh(MeshType::cube, parent);

    // --- OBJ / Wavefront (.obj) ---
    if(n->type == "Wavefront"){
        std::string path;

        // Si el archivo viene con comillas --> Unquote
        if(n->props.count("filePath"))
            path = GetFilePath(w3dMapAt(n->props, "filePath"));
        else {
            std::cerr << "[Wavefront] Falta filePath\n";
            return NULL;
        }

        bool NoMerge = GetBoolOrDefault(n->props, "noMerge", false);        

        // --- IMPORTACIÓN ---
        Mesh* mesh = ImportWOBJ(path, parent, NoMerge);

        if (!mesh){
            std::cerr << "[Wobj] Se importo mal el wobj!\n";
            return NULL;
        }

        for (size_t _ci=0; _ci<n->children.size(); _ci++) {
            Node* child = n->children[_ci];
            if (child->type == "Animation") {
                std::map<std::string,std::string>& ap = child->props;

                bool UseNormals = GetBoolOrDefault(ap, "UseNormals", false);

                bool repeat = GetBoolOrDefault(ap, "repeat", true);

                int speed = GetIntOrDefault(ap, "speed", 1);

                int proximaAnimacion = GetIntOrDefault(ap, "proximaAnimacion", -1);

                // tolerante como el resto del lector: un archivo viejo sin estos
                // campos NO tira excepcion (at()/stoi lanzan y mataban el editor)
                VertexAnimation* anim =
                    new VertexAnimation(NULL, ap.count("name") ? w3dMapAt(ap, "name") : std::string("anim"),
                                        UseNormals, (float)speed, repeat, proximaAnimacion);

                anim->basePath   = ap.count("basePath") ? GetFilePath(w3dMapAt(ap, "basePath")) : std::string();
                anim->frameCount = GetIntOrDefault(ap, "frames", 0);
                anim->padding    = GetIntOrDefault(ap, "padding", 0);

                NewActiveVertexAnimation(mesh, anim);
            }
            // ANIMACION UV "tira de atlas" (Core, autoplay: ver struct UVAnimTira en Mesh.h).
            //   AnimacionUV { frames: 14  fps: 30  eje: u  desfase: 0 }
            // La textura es una TIRA de 'frames' celdas sobre 'eje'; el motor la recorre solo.
            if (child->type == "AnimacionUV") {
                std::map<std::string,std::string>& up = child->props;
                std::string eje = up.count("eje") ? Unquote(w3dMapAt(up, "eje")) : std::string("u");
                mesh->SetUVAnimTira(GetIntOrDefault(up, "frames", 1),
                                    GetFloatOrDefault(up, "fps", 30.0f),
                                    (eje == "v") ? 1 : 0,
                                    GetIntOrDefault(up, "desfase", 0));
            }
            // MODIFICADOR "Culling" POR TRIANGULO (ver edit/Modifier.h). Dos metodos:
            //   CullingTri { metodo: "triangulos"  sector: 0  pvs: "extra/x.pvs.json" }
            //   CullingTri { metodo: "celdas"  celda: 0  vis: "extra/x.w3dvis" }
            // Los datos NO van en el .w3d (son megabytes de indices): viven en el
            // sidecar (JSON de sectores o `.w3dvis` binario, formato/w3dvis.md) y se
            // releen en cada carga. `pvs:`/`vis:` son opcionales: sin ellos se derivan
            // del .obj de origen, como siempre. `celda:` es alias de `sector:` (los dos
            // 1-based; 0 = malla completa). El metodo viejo "bsp" (nunca corto nada)
            // cae a "celdas": sin `.w3dvis` sigue dibujando la malla completa.
            // `sectorFallback:` (opcional) = que dibujar cuando la celda ACTIVA quedo
            // SIN triangulos: "completa" = la malla entera, N = la lista de la celda N;
            // ausente = nada (el comportamiento de siempre). Ver Modifier.h.
            if (child->type == "CullingTri") {
                std::map<std::string,std::string>& cp = child->props;
                extern void W3dModPVSAgregar(Mesh*, const std::string&, int, const std::string&, int); // edit/MeshEdit.cpp
                std::string metodo = cp.count("metodo") ? Unquote(w3dMapAt(cp, "metodo")) : std::string("triangulos");
                int sector = GetIntOrDefault(cp, "sector", GetIntOrDefault(cp, "celda", 0));
                std::string archivo = cp.count("vis") ? Unquote(w3dMapAt(cp, "vis"))
                                    : (cp.count("pvs") ? Unquote(w3dMapAt(cp, "pvs")) : std::string());
                int fallback = 0;
                if (cp.count("sectorFallback")) {
                    std::string fs = Unquote(w3dMapAt(cp, "sectorFallback"));
                    fallback = (fs == "completa") ? -1 : GetIntOrDefault(cp, "sectorFallback", 0);
                }
                W3dModPVSAgregar(mesh, metodo, sector, archivo, fallback);
            }
        }

        LoadVertexFrames(mesh);
        { extern void W3dPVSSincronizar(Mesh*);   // PVS: carga el sidecar + arma el sector declarado
          W3dPVSSincronizar(mesh); }

        return mesh;
    }

    if (n->type=="UI"){
        // la INTERFAZ del proyecto: el .w3d referencia un .w3dui EXTERNO (el
        // arbol 2D completo, con sus scripts). Mismo camino que el JSON nuevo.
        if (!n->props.count("archivo")) {
            w3dLogfW("[W3D] nodo UI sin 'archivo': escena omitida");
            return NULL;
        }
        return (Object*)CargarUIProyecto(GetFilePath(w3dMapAt(n->props, "archivo")),
                                         Unquote(w3dMapAt(n->props, "archivo")));
        // (si cargo, ya quedo colgado de la escena)
    }

    if (n->type=="Mirror"){
        // Mirror { target: "..."  mirrorX/mirrorY/mirrorZ: true|false }
        // Los ejes espejados en el formato de TEXTO (el camino JSON ya los
        // traia via "ejes"): sin esto el reflejo del agua (mirrorY, plano
        // horizontal) no se podia declarar y el default quedaba en mirrorZ.
        Mirror* mirror = new Mirror(parent);
        if(p.count("target")) mirror->SetTarget(w3dMapAt(p, "target"));
        // MULTI-TARGET (opcional): targets: "Jugador, Enemigo1, Enemigo2". El primero
        // ocupa el slot historico, o sea que 'target' y 'targets' conviven y un
        // archivo viejo abre igual. Va DESPUES de 'target' a proposito: si estan
        // los dos, manda la lista.
        if(p.count("targets")) mirror->SetTargetsTexto(Unquote(w3dMapAt(p, "targets")));
        mirror->mirrorX = GetBoolOrDefault(p, "mirrorX", mirror->mirrorX);
        mirror->mirrorY = GetBoolOrDefault(p, "mirrorY", mirror->mirrorY);
        mirror->mirrorZ = GetBoolOrDefault(p, "mirrorZ", mirror->mirrorZ);
        // RECORTE DEL REFLEJO (ver Mirror.h): el rectangulo se mide en el PLANO
        // del espejo, sobre EjeU()/EjeV(). Claves nuevas limiteU0/U1/V0/V1.
        // Las cuatro viejas (limiteIzquierdo/Derecho/Fondo/Frente, que estaban
        // tomadas prestadas del vocabulario de un objeto de fisica y hablaban de
        // X/Z de mundo) se siguen leyendo como ALIAS: para el espejo horizontal
        // sin rotar, U = X y V = Z, asi que significan exactamente lo mismo.
        mirror->sinProfundidad = GetBoolOrDefault(p, "sinProfundidad", false);
        // hundimientoMaximo: DEPRECADO (ver Mirror.h). Se acepta para que el
        // archivo abra, se avisa por log, y NO tiene ningun efecto: la
        // reflexion es geometrica pura y el recorte es la silueta del agua.
        mirror->hundimientoMaximo = GetFloatOrDefault(p, "hundimientoMaximo", 0.0f);
        if (mirror->hundimientoMaximo > 0.0f)
            w3dLogfW("[W3D] Mirror '%s': hundimientoMaximo esta DEPRECADO y no tiene "
                     "efecto (la reflexion es geometrica pura; el recorte del reflejo "
                     "es la silueta del agua en pantalla)", mirror->name.c_str());
        if (p.count("limiteU0") && p.count("limiteU1") &&
            p.count("limiteV0") && p.count("limiteV1")) {
            mirror->usaLimites = true;
            mirror->limU0 = GetFloatOrDefault(p, "limiteU0", 0);
            mirror->limU1 = GetFloatOrDefault(p, "limiteU1", 0);
            mirror->limV0 = GetFloatOrDefault(p, "limiteV0", 0);
            mirror->limV1 = GetFloatOrDefault(p, "limiteV1", 0);
        } else if (p.count("limiteIzquierdo") && p.count("limiteDerecho") &&
                   p.count("limiteFondo") && p.count("limiteFrente")) {
            mirror->usaLimites = true;
            mirror->limU0 = GetFloatOrDefault(p, "limiteIzquierdo", 0);
            mirror->limU1 = GetFloatOrDefault(p, "limiteDerecho", 0);
            mirror->limV0 = GetFloatOrDefault(p, "limiteFondo", 0);
            mirror->limV1 = GetFloatOrDefault(p, "limiteFrente", 0);
        }
        // recorte del reflejo (default = hay limites). Solo se lee si viene.
        if (p.count("recorte")) {
            mirror->recorteDeclarado = true;
            mirror->recorte = GetBoolOrDefault(p, "recorte", true);
        }
        return mirror;
    }

    if (n->type=="LOD"){
        // LOD { distancias: "20, 45, 90" <hijos en orden de detalle> }
        // umbral i = hasta donde llega el hijo i; mas alla del ultimo, el ultimo hijo.
        LOD* lod = new LOD(parent);
        if (p.count("distancias")) lod->SetDistanciasTexto(Unquote(w3dMapAt(p, "distancias")));
        lod->soloCamaraActiva = GetBoolOrDefault(p, "soloCamaraActiva", false);
        return lod; // los hijos los cuelga BuildObjectRecursive
    }

    if (n->type=="Culling"){
        // Culling { activo: true|false soloCamaraActiva: true|false distanciaMax: 0 <hijos> }
        // (frustum culling por AABB + culling por distancia opcional; ver Culling.h)
        Culling* cu = new Culling(parent);
        cu->activo           = GetBoolOrDefault(p, "activo", true);   // ausente = prendido
        cu->soloCamaraActiva = GetBoolOrDefault(p, "soloCamaraActiva", false);
        cu->distanciaMax     = GetFloatOrDefault(p, "distanciaMax", 0.0f);
        return cu;
    }

    if (n->type=="Particulas"){
        // Particulas { textura: "polvo.png" cantidad: 20 vida: 1.2 tam: 0.4 vel: 2
        //              dispersion: 30 gravedad: 4 aditivo: false color: "1, 1, 1, 1"
        //              desvanecer: true activo: true }   (ver Particulas.h)
        Particulas* pt = new Particulas(parent);
        // la textura se resuelve contra la carpeta del .w3d (como filePath del Wavefront)
        if (p.count("textura")) pt->textura = GetFilePath(w3dMapAt(p, "textura"));
        pt->cantidad   = GetFloatOrDefault(p, "cantidad",   pt->cantidad);
        pt->vida       = GetFloatOrDefault(p, "vida",       pt->vida);
        pt->tam        = GetFloatOrDefault(p, "tam",        pt->tam);
        pt->vel        = GetFloatOrDefault(p, "vel",        pt->vel);
        pt->dispersion = GetFloatOrDefault(p, "dispersion", pt->dispersion);
        pt->gravedad   = GetFloatOrDefault(p, "gravedad",   pt->gravedad);
        pt->aditivo    = GetBoolOrDefault(p, "aditivo",     pt->aditivo);
        pt->sustractivo= GetBoolOrDefault(p, "sustractivo", pt->sustractivo);
        if (p.count("color")) pt->SetColorTexto(Unquote(w3dMapAt(p, "color")));
        pt->desvanecer = GetBoolOrDefault(p, "desvanecer",  pt->desvanecer);
        pt->activo     = GetBoolOrDefault(p, "activo",      pt->activo);
        // azar (defaults 0 = sin jitter ni deriva: los archivos viejos no cambian)
        pt->variacion   = GetFloatOrDefault(p, "variacion",   pt->variacion);
        pt->turbulencia = GetFloatOrDefault(p, "turbulencia", pt->turbulencia);
        // rotacion del billboard (defaults false/0 = derecho y quieto, como antes)
        pt->rotacion    = GetBoolOrDefault (p, "rotacion",    pt->rotacion);
        pt->velRotacion = GetFloatOrDefault(p, "velRotacion", pt->velRotacion);
        return pt;
    }

    if (n->type=="VisZona"){
        // VisZona { modo: manual|grilla|volumenes|curva  objetivo: "Escenario"
        //           ancla: "Jugador"  nx: 8 ny: 1 nz: 8  tamCelda: 4.0  pasoMax: 32
        //           riel: "RielPueblo"  fallback: "completa" }
        // decide la CELDA de visibilidad del objetivo (ver VisZona.h); en modo
        // volumenes sus HIJOS son los volumenes (los cuelga BuildObjectRecursive).
        // `riel:` (alias `curva:`, modo curva) = la Curve cuyos nodos indexan las
        // celdas: con la camara en OTRO riel la zona cae a `fallback:` ("completa"
        // = malla entera, N = celda N). Sin `riel:` nada cambia (legacy).
        VisZona* vz = new VisZona(parent);
        std::string modo = p.count("modo") ? Unquote(w3dMapAt(p, "modo")) : std::string("manual");
        vz->modo = (modo == "grilla") ? VisZona::ModoGrilla
                 : (modo == "volumenes") ? VisZona::ModoVolumenes
                 : (modo == "curva") ? VisZona::ModoCurva : VisZona::ModoManual;
        if (p.count("objetivo")) vz->objetivoNombre = Unquote(w3dMapAt(p, "objetivo"));
        if (p.count("ancla"))    vz->anclaNombre    = Unquote(w3dMapAt(p, "ancla"));
        vz->nx       = GetIntOrDefault(p, "nx", 1);
        vz->ny       = GetIntOrDefault(p, "ny", 1);
        vz->nz       = GetIntOrDefault(p, "nz", 1);
        vz->tamCelda = GetFloatOrDefault(p, "tamCelda", 1.0f);
        vz->pasoMax  = GetIntOrDefault(p, "pasoMax", 32);
        if (p.count("riel"))       vz->rielNombre = Unquote(w3dMapAt(p, "riel"));
        else if (p.count("curva")) vz->rielNombre = Unquote(w3dMapAt(p, "curva"));
        if (p.count("fallback")) {
            std::string fb = Unquote(w3dMapAt(p, "fallback"));
            vz->celdaFallback = (fb == "completa") ? 0 : GetIntOrDefault(p, "fallback", 0);
        }
        return vz;
    }

    if (n->type=="Instance"){
        Instance* instance = new Instance(parent);
        if(p.count("target")) instance->SetTarget(w3dMapAt(p, "target"));
        if(p.count("count")){   // misma cota que el camino JSON: 'count' manda un for del render
            int c = GetIntOrDefault(p, "count", 1);
            if (c < 0) c = 0; else if (c > 4096) c = 4096;
            instance->count = (size_t)c;
        }
        return instance;
    }

    if (n->type=="Gamepad"){
        // ===== MIGRACION: el objeto Script/Control (clase Gamepad) SE DIO DE BAJA =====
        // Cualquier objeto acepta scripts (pestania "Scripts" del panel), asi que un objeto
        // cuya unica razon de ser era llevar un .lua no tenia sentido. Un .w3d viejo que
        // traiga uno SIGUE ABRIENDO: el nodo vuelve como Empty con el MISMO nombre, la misma
        // transformacion, los mismos hijos y los mismos scripts (que es lo unico que se
        // usaba). Se PIERDEN los campos de fisica hardcodeada (velocidad/gravedad/limites/
        // salto) y el 'target': esa fisica ya vivia en lua desde hace rondas. Se avisa en
        // pantalla para que nadie se entere por casualidad.
        Empty* gamepad = new Empty(parent);
        W3dAvisof(false, "El objeto Script '%s' ya no existe: se migro a un objeto vacio con sus "
                         "scripts. Su fisica hardcodeada (velocidad/gravedad/limites) NO se migra.",
                  p.count("name") ? Unquote(w3dMapAt(p, "name")).c_str() : "(sin nombre)");

        // el SCRIPT declarado con `script:` lo levanta LeerScriptsDeProps (en
        // BuildObjectRecursive, para CUALQUIER objeto). Aca queda solo la
        // convencion <proyecto>.lua, que es EXCLUSIVA del Gamepad.
        // convencion ALTERNATIVA: si no declara script pero junto al .w3d hay un
        // "<nombre del proyecto>.lua" (MiJuego.w3d -> MiJuego.lua), se
        // cuelga solo. La fisica que antes vivia hardcodeada en el editor ahora
        // es un script lua del proyecto.
        if (!p.count("script")) {
            extern std::string w3dPath;
            size_t sl = w3dPath.find_last_of("/\\");
            std::string dir  = (sl == std::string::npos) ? std::string(".") : w3dPath.substr(0, sl);
            std::string base = (sl == std::string::npos) ? w3dPath : w3dPath.substr(sl + 1);
            size_t pt = base.find_last_of('.');
            if (pt != std::string::npos) base = base.substr(0, pt);
            std::string lua = dir + "/" + base + ".lua";
            if (w3dFileSystem::FileExists(lua)) {
                if (!gamepad->scriptDatos) gamepad->scriptDatos = new W3dScriptDatos();
                W3dScriptEntrada e; e.ruta = lua;
                gamepad->scriptDatos->scripts.push_back(e);
            }
        }   // (fin de la convencion sin "script:")

        return gamepad;
    }

    if (n->type=="Curve"){
        std::string path;

        // Si el archivo viene con comillas --> Unquote
        if(n->props.count("filePath"))
            path = GetFilePath(w3dMapAt(n->props, "filePath"));
        else {
            std::cerr << "[Curve] Falta filePath\n";
            return NULL;
        }

        Curve* curve = new Curve(parent);
        if (curve->LoadFromFile(path)){
            // LISTA DE CARGA (streaming por riel): sidecar declarado como hijo
            //     Curve { filePath: "riel.cap"  ListaCarga { filePath: "riel.cargas.json" } }
            for (size_t _i=0; _i<n->children.size(); _i++) {
                Node* c = n->children[_i];
                if (!c || c->type != "ListaCarga") continue;
                if (!c->props.count("filePath")) { std::cerr << "[ListaCarga] falta filePath\n"; continue; }
                curve->CargarListaCarga(GetFilePath(w3dMapAt(c->props, "filePath")));
            }
            return curve;
        }
        // fallo la carga: el ctor ya lo colgo del padre -> sacarlo del arbol y
        // liberarlo (antes quedaba una Curve vacia fantasma en la escena)
        if (parent) {
            std::vector<Object*>& hs = parent->Childrens;
            for (size_t i = 0; i < hs.size(); i++)
                if (hs[i] == curve) { hs.erase(hs.begin() + i); break; }
        }
        delete curve;
        return NULL;
    }

    if (n->type=="Constraint"){
        // el objeto Constraint se dio de baja: entra como Empty y sus propiedades pasan
        // al objeto APUNTADO al final de la carga (ver PendConsViejo, arriba).
        // OJO: en el formato de TEXTO 'usePitch' tiene default TRUE (en el JSON es false).
        return NodoConstraintViejo(parent,
                                   p.count("target") ? w3dMapAt(p, "target") : std::string(),
                                   GetBoolOrDefault(p, "useHorizontal", true),
                                   GetBoolOrDefault(p, "usePitch", true),
                                   GetBoolOrDefault(p, "copyPosX", false),
                                   GetBoolOrDefault(p, "copyPosY", false),
                                   GetBoolOrDefault(p, "copyPosZ", false));
    }

    if(n->type=="Camera"){
        Camera* camera = new Camera(parent, Vector3(0,0,0), Vector3(0, 0, 0));
        if(p.count("target")) camera->SetTarget(w3dMapAt(p, "target"));
        if(p.count("riel")) camera->SetRiel(w3dMapAt(p, "riel"));
        if(p.count("offsetRiel")) camera->offsetRiel = GetIntOrDefault(p, "offsetRiel", 0);
        // MIRADA DEL RIEL: la orientacion sale del yaw/pitch AUTORAL del nodo en vez del
        // look-at al target (ver Camera.h). Ausente = false = comportamiento de siempre.
        if(p.count("miradaRiel")) camera->usarRotacionDelRiel = GetBoolOrDefault(p, "miradaRiel", false);
        // LENTE: la rama JSON ya leia los tres ("fov"/"near"/"far", JsonObjetoCrear) pero
        // el formato texto solo el fov, y una camara con planos de recorte propios quedaba
        // clavada en los defaults (0.01 / 1000). Los tres se pueden ademas leer y escribir
        // desde lua con lenteDe() / setLente().
        if(p.count("fov"))  camera->fov      = GetFloatOrDefault(p, "fov", camera->fov);
        if(p.count("near")) camera->nearClip = GetFloatOrDefault(p, "near", camera->nearClip);
        if(p.count("far"))  camera->farClip  = GetFloatOrDefault(p, "far", camera->farClip);
        // ENCUADRE DECLARADO (aspecto de imagen, ej 1.481481 = 40:27): el modo juego
        // dibuja ESE frustum y letterboxea el resto (ver Camera::aspecto). Ausente =
        // 0 = como siempre (o heredado de la cabecera `aspecto` del .cap del riel).
        if(p.count("aspecto")) camera->aspecto = GetFloatOrDefault(p, "aspecto", 0.0f);
        return camera;
    }

    if(n->type=="Light"){
        Light* L=Light::Create(parent,X,Y,Z);
        L->SetDiffuse(
            GetFloatOrDefault(n->props,"r",1),
            GetFloatOrDefault(n->props,"g",1),
            GetFloatOrDefault(n->props,"b",1)
        );
        L->LightID = GetLightIDOrDefault(p.count("LightID") ? w3dMapAt(p, "LightID") : "GL_LIGHT0");

        return L;
    }

    if(n->type=="Collection"){
        // Collection { ordenarPorCamara: true|false ordenarUnaVez: true|false
        //              lote: "estatico" <hijos> }
        // (orden lejos->cerca para transparentes + lote estatico; ver Collection.h)
        Collection* C = new Collection(parent);
        if(!CollectionActive) CollectionActive = C;
        C->ordenarPorCamara = GetBoolOrDefault(p, "ordenarPorCamara", false);
        C->ordenarUnaVez    = GetBoolOrDefault(p, "ordenarUnaVez", false);
        C->lote = (p.count("lote") && Unquote(w3dMapAt(p, "lote")) == "estatico") ? 1 : 0;
        return C;
    }

    return NULL; // objeto desconocido → ignorado
}

// ===========================================================================
//  SCRIPTS estilo Unity en el formato de TEXTO, para CUALQUIER objeto:
//      script: "fruta.lua"      el .lua colgado (uno por nodo en este formato)
//      ref_pelota: "Pelota"     referencia a un objeto (propiedades tipo 0)
//      opt_modo: "facil"        opcion de un desplegable (tipo 1)
//      val_id: "3"              valor por instancia (tipo 2: numero/bool/texto)
//  Los tres prefijos viajan por el MISMO canal (W3dScriptEntrada.refs, par
//  nombre -> texto): quien decide que es cada par es el .lua al leerlo con
//  objeto()/opcion()/propiedad(). Antes esto vivia solo en la rama Gamepad; la
//  convencion <proyecto>.lua (sin `script:`) SI sigue siendo solo del Gamepad.
// ===========================================================================
static void LeerScriptsDeProps(Object* obj, const std::map<std::string,std::string>& p){
    if (!obj || !p.count("script")) return;
    if (!obj->scriptDatos) obj->scriptDatos = new W3dScriptDatos();
    W3dScriptEntrada e;
    e.ruta = GetFilePath(w3dMapAt(p, "script"));
    for (std::map<std::string,std::string>::const_iterator it = p.begin(); it != p.end(); ++it)
        if (it->first.compare(0, 4, "ref_") == 0 ||
            it->first.compare(0, 4, "opt_") == 0 ||
            it->first.compare(0, 4, "val_") == 0)
            e.refs.push_back(std::make_pair(it->first.substr(4), Unquote(it->second)));
    obj->scriptDatos->scripts.push_back(e);
}

void BuildObjectRecursive(Node* n, Object* parent){
    Object* obj = CreateObjectFromNode(n, parent);
    if(!obj) return;

    ApplyCommonProps(obj, n->props);
    LeerScriptsDeProps(obj, n->props);   // `script:` + ref_/opt_/val_ (cualquier objeto)

    for(size_t _i=0;_i<n->children.size();_i++)
        BuildObjectRecursive(n->children[_i], obj);
}

void BuildScene(Node* root){
    if(!root || root->type!="Escena") return;

    // TODOS los campos del proyecto van por los MISMOS aplicadores que el JSON
    // (una sola fuente de verdad por campo). "iconApp" es el alias del formato
    // de texto viejo para "icono" (solo existe en este lector).
    AplicarIcono(root->props.count("iconApp") ? Unquote(w3dMapAt(root->props, "iconApp"))
                                              : std::string());

    if(root->props.count("fullscreen")){
        std::string v = w3dMapAt(root->props, "fullscreen");
        AplicarFullscreen(v == "true" || v == "1");
    }

    // el FPS del proyecto (timeline/animaciones): "fps: 60" en la Escena
    AplicarFps(GetIntOrDefault(root->props, "fps", 0));

    // MULTI-ESCENA: la escena que ARRANCA + el modo (lo lee Compilar juego)
    if(root->props.count("escenaInicial"))
        AplicarEscenaInicial(Unquote(w3dMapAt(root->props, "escenaInicial")));
    if(root->props.count("modoEscenas")){
        std::string v = w3dMapAt(root->props, "modoEscenas");
        AplicarModoEscenas(v == "true" || v == "1");
    }

#ifndef W3D_SIN_EDITOR
    if(root->props.count("Antialiasing")){
        std::string v = w3dMapAt(root->props, "Antialiasing");
        cfg.enableAntialiasing = (v == "true" || v == "1");   // opcion del EDITOR (cfg)
    }
#endif

    // PIXELADO GLOBAL de la escena (ver w3dGraphics.h). Reporte del dueno:
    // "pixela todas las texturas y no uses filtros de textura". Es DEL PROYECTO,
    // no de cada material: se declara una vez en la cabecera del .w3d con
    //     pixelado: true
    // y a partir de ahi todo el 3D va NEAREST y sin mipmaps. Default false: los
    // ejemplos que ya andan no cambian de aspecto.
    if(root->props.count("pixelado")){
        std::string v = w3dMapAt(root->props, "pixelado");
        w3dEngine::SetPixeladoGlobal(v == "true" || v == "1");
    }

    if(root->props.count("background")){
        std::string bg = Unquote(w3dMapAt(root->props, "background")); // <- quita comillas si las hay
        std::replace(bg.begin(), bg.end(), ',', ' '); // reemplaza comas por espacios
        std::stringstream ss(bg);
        float r,g,b,a;
        if(ss >> r >> g >> b >> a){
#ifndef W3D_SIN_EDITOR
            scene->SetBackground(r,g,b,a);   // el objeto Scene vive en el editor
#endif
        } else {
            std::cerr << "Error al parsear background: '" << bg << "'\n";
        }
    }

    for(size_t _i=0;_i<root->children.size();_i++)
        BuildObjectRecursive(root->children[_i], SceneCollection);  // el parent puede ser la escena global

    if(!CollectionActive) 
        CollectionActive = SceneCollection;        // seguridad
}

// ===========================================================================
//  LECTURA JSON — compartida por el .w3d JSON PLANO nuevo (v3: el .w3d ES el
//  json) y el zip v2 viejo (escena.json adentro). Construye la escena llamando
//  a los MISMOS aplicadores de campos que el formato de texto (ver arriba).
// ===========================================================================
#include "io/JsonW3d.h"
#include "io/W3dZip.h"
#include "objects/Mesh.h"
#include "objects/Instance.h"            // AUDIT versiones: espejo/instancia/curva en el JSON
#include "objects/Armature.h"            // ARMATURES en el .w3d (Fase 3)
#include "animation/Armature2DAnimation.h" // clips del armature 2D ("anims2d")
#include "animation/SkeletalAnimation.h" // clips del esqueleto + PrepararSkin/PrepararSkinAutorado
#include "edit/Modifier.h"               // modificador Armature de la malla (referencia por nombre)
#include "edit/WeightPaint.h"            // WeightPaintAsegurarMapa (reconstruir vertex groups por posicion)
#include "animation/VertexAnimation.h"   // cargar frames de vertices (blob binario) + curvas
#include "script/W3dScript.h"
#include "w3dFilesystem.h"

// carpeta del .w3d ABIERTO. En el JSON plano las rutas relativas cuelgan de aca
// directo; en un zip v2 primero se busca en la carpeta EXTRAIDA y despues aca
// (los assets EXTERNOS del v2 eran relativos al .w3d, no al zip).
static std::string gDirProyecto;

// v4: el proyecto abierto es un CONTENEDOR y las rutas del JSON son NOMBRES DE
// ENTRADA. Lo prende AbrirW3D en el brazo del contenedor y lo apaga al terminar.
static bool gProyectoV4 = false;

// una ruta del json -> ruta real (LA unica resolucion de referencias del .w3d).
//   "ext:..."   -> EXTERNA deliberada: se saca el prefijo y se resuelve contra la
//                  carpeta del .w3d (absoluta queda tal cual). Queda ANOTADA para
//                  que el proximo guardado la vuelva a escribir externa.
//   v4, pelada  -> es una ENTRADA del contenedor: se devuelve TAL CUAL y
//                  ReadFileBytes la resuelve por el montaje.
//   v3 y viejos -> la regla de siempre: absoluta tal cual; relativa contra 'base'
//                  y, si ahi no existe pero al lado del .w3d si, contra el proyecto.
static std::string RutaJson(const std::string& r, const std::string& base) {
    if (r.empty()) return r;
    if (r.size() > 4 && r.compare(0, 4, "ext:") == 0) {
        std::string x = r.substr(4);
        std::string res = x;
        if (!(x[0] == '/' || (x.size() > 2 && x[1] == ':')))
            res = (gDirProyecto.empty() ? base : gDirProyecto) + "/" + x;
        W3dRefExternaMarcar(res);
        return res;
    }
    if (r[0] == '/' || (r.size() > 2 && r[1] == ':')) return r;   // absoluta
    if (gProyectoV4 && W3dEsNombreDeEntrada(r)) return r;         // entrada del contenedor
    std::string enBase = base + "/" + r;
    if (!w3dFileSystem::FileExists(enBase) &&
        !gDirProyecto.empty() && gDirProyecto != base) {
        std::string enProy = gDirProyecto + "/" + r;
        if (w3dFileSystem::FileExists(enProy)) return enProy;
    }
    return enBase;
}

// ===========================================================================
//  COTAS DEL LECTOR. El .w3d se promociona como JSON EDITABLE A MANO: TODO
//  numero que salga del archivo y se use para RESERVAR memoria, INDEXAR o
//  ITERAR tiene que estar acotado contra algo REAL (no vale confiar en "el
//  archivo lo escribimos nosotros"). Un .w3d corrupto o adversarial tiene que
//  fallar LIMPIO (aviso al log + default sensato), nunca colgar el editor ni
//  corromper memoria.
// ===========================================================================

// entero del archivo ACOTADO a [lo, hi]. Fuera de rango: se acota y se avisa
// (el valor sigue siendo utilizable, el editor no se cae).
static int JIRango(JVal* o, const char* k, int def, int lo, int hi, const char* quien) {
    int v = JI(o, k, def);
    if (v < lo || v > hi) {
        w3dLogfW("[W3D] %s: '%s' vale %d, fuera de [%d..%d] -> se acota", quien, k, v, lo, hi);
        v = (v < lo) ? lo : hi;
    }
    return v;
}

// SANEA una jerarquia de huesos leida del archivo: indices de padre fuera de
// rango, auto-padres y CICLOS quedan como raiz (-1). Sin esto un .w3d a mano
// con "padre" cruzado (0->1, 1->0) cuelga cualquier recorrido que suba por la
// cadena (ej: UVEditor.cpp, borrar huesos). Devuelve cuantos arreglo.
static int SanearPadres(std::vector<int>& padre, const char* quien) {
    const int n = (int)padre.size();
    int arreglados = 0;
    for (int i = 0; i < n; i++) {
        if (padre[i] < -1 || padre[i] >= n || padre[i] == i) {
            if (padre[i] != -1) { padre[i] = -1; arreglados++; }
            continue;
        }
        int p = padre[i], guard = 0;               // subir: si no llega a una raiz en n pasos, hay ciclo
        while (p >= 0 && guard++ <= n) p = padre[p];
        if (p >= 0) { padre[i] = -1; arreglados++; }
    }
    if (arreglados)
        w3dLogfW("[W3D] %s: %d hueso(s) con 'padre' invalido o en ciclo -> quedan como raiz", quien, arreglados);
    return arreglados;
}

static void JsonComunes(JVal* j, Object* o) {
    o->name = JS(j, "nombre", o->name);
    o->visible = JB(j, "visible", true);
    o->showRelantionshipsLines = JB(j, "lineasParentales", true); // ausente = true (archivos viejos)
    o->paleta = JS(j, "paleta", "");   // seleccion de paleta ("" = hereda del padre)
    JVal* v;
    if ((v = JHijo(j, "pos", 5)) && v->lista.size() >= 3) {
        o->pos.x = (float)v->lista[0]->num; o->pos.y = (float)v->lista[1]->num; o->pos.z = (float)v->lista[2]->num;
    }
    // ROTACION: el euler del archivo ES el dato (trae sus vueltas) y de el se deriva el quaternion, que es
    // lo que se RENDERIZA. Antes esto escribia los tres floats de rotEuler a mano y no tocaba el quaternion:
    // la escena abria SIN ROTAR ("la camara esta todo en 0 grados"). Va por la puerta (ver Objects.h).
    if ((v = JHijo(j, "rot", 5)) && v->lista.size() >= 3) {
        o->SetRotEuler(Vector3((float)v->lista[0]->num, (float)v->lista[1]->num, (float)v->lista[2]->num));
    }
    if ((v = JHijo(j, "escala", 5)) && v->lista.size() >= 3) {
        o->scale.x = (float)v->lista[0]->num; o->scale.y = (float)v->lista[1]->num; o->scale.z = (float)v->lista[2]->num;
    }
}

// los scripts lua del objeto: [{archivo, refs{}}]
static void JsonScripts(JVal* j, Object* o, const std::string& base) {
    JVal* js = JHijo(j, "scripts", 5);
    if (!js) return;
    for (size_t i = 0; i < js->lista.size(); i++) {
        JVal* e = js->lista[i];
        if (e->tipo != 4) continue;
        if (!o->scriptDatos) o->scriptDatos = new W3dScriptDatos();
        W3dScriptEntrada ent;
        ent.ruta = RutaJson(JS(e, "archivo", ""), base);
        JVal* refs = JHijo(e, "refs", 4);
        if (refs)
            for (std::map<std::string, JVal*>::iterator r = refs->obj.begin(); r != refs->obj.end(); ++r)
                if (r->second->tipo == 2)
                    ent.refs.push_back(std::make_pair(r->first, r->second->str));
        o->scriptDatos->scripts.push_back(ent);
    }
}

// UNA curva {p, c, k:[[frame,value,interp,htype,inDF,inDV,outDF,outDV],...]} -> AnimProperty.
// Compartida por las curvas de las vertex anims y por los tracks de los clips de esqueleto (Fase 3).
static bool CargarCurvaJson(JVal* cj, AnimProperty& ap) {
    if (!cj || cj->tipo != 4) return false;
    ap.Property = JI(cj, "p", 0); ap.component = JI(cj, "c", 0);
    JVal* ks = JHijo(cj, "k", 5);
    // COTA: ninguna cantidad del archivo se usa sin acotar. Un millon de keyframes en
    // UNA curva no es un proyecto, es un archivo roto (o hecho a mano): se cargan los
    // primeros y se dice cuantos quedaron afuera, en vez de comerse la RAM en silencio.
    size_t nKeys = ks ? ks->lista.size() : 0;
    if (nKeys > 1000000) {
        w3dLogfW("[W3D] una curva declara %d keyframes: se cargan los primeros 1000000", (int)nKeys);
        nKeys = 1000000;
    }
    if (ks) for (size_t i = 0; i < nKeys; i++) {
        JVal* row = ks->lista[i]; if (!row || row->tipo != 5 || row->lista.size() < 3) continue;
        keyFrame kf;
        kf.frame = (int)row->lista[0]->num; kf.value = (float)row->lista[1]->num;
        kf.Interpolation = (int)row->lista[2]->num;
        if (row->lista.size() >= 8) {
            kf.handleType = (int)row->lista[3]->num;
            kf.inDF = (float)row->lista[4]->num; kf.inDV = (float)row->lista[5]->num;
            kf.outDF = (float)row->lista[6]->num; kf.outDV = (float)row->lista[7]->num;
        }
        ap.keyframes.push_back(kf);
    }
    return true;
}

// curvas de transform de una anim de objeto: [{p, c, k:[[frame,value,interp,htype,inDF,inDV,outDF,outDV],...]}]
static void CargarCurvasAnim(JVal* aj, VertexAnimation* anim) {
    JVal* cur = JHijo(aj, "curvas", 5); if (!cur) return;
    for (size_t c = 0; c < cur->lista.size(); c++) {
        AnimProperty ap;
        if (CargarCurvaJson(cur->lista[c], ap)) anim->curvas.push_back(ap);
    }
}

// carga las vertex anims de una malla desde el JSON: rango/fps + FRAMES (del blob binario
// self-contained, o legacy de los .obj por basePath) + curvas de transform. Compartido por
// las ramas "modelo" y "glb". 'base' es la carpeta donde se extrajo el zip.
// CUANTAS vertex anims necesitaron el remapeo por cercania de posicion en la ultima apertura.
// Lo mira el harness ('quadsave'): con la geometria en .w3dm tiene que quedar en CERO.
int g_w3dVaRemapeos = 0;

// ANIMACION UV "tira de atlas" del JSON v4: { "frames": N, "fps": F, "eje": "u"|"v",
// "desfase": D } bajo la clave "animUV" del objeto malla (la escribe GuardarW3D).
static void CargarAnimUV(JVal* j, Mesh* mesh) {
    JVal* a = JHijo(j, "animUV", 4);
    if (!a || !mesh) return;
    mesh->SetUVAnimTira(JI(a, "frames", 1), JF(a, "fps", 30.0f),
                        JS(a, "eje", "u") == "v" ? 1 : 0, JI(a, "desfase", 0));
}

static void CargarAnimsVertex(JVal* j, Mesh* mesh, const std::string& base) {
    JVal* anims = JHijo(j, "anims", 5); if (!anims || !mesh) return;
    for (size_t i = 0; i < anims->lista.size(); i++) {
        JVal* a = anims->lista[i]; if (a->tipo != 4) continue;
        VertexAnimation* anim = new VertexAnimation(
            NULL, JS(a, "nombre", "anim"), JB(a, "normales", false),
            JF(a, "velocidad", 1.0f), JB(a, "repetir", true), JI(a, "proxima", -1));
        anim->basePath  = RutaJson(JS(a, "base", ""), base);
        // padding = ancho del numero de frame en el nombre de archivo ("0001.obj"). Va DIRECTO
        // a un std::setw: un valor absurdo del archivo hacia que LoadFrames armara una cadena de
        // ese tamano (padding: 2000000000 = 2 GB por nombre de archivo).
        anim->padding   = JIRango(a, "padding", 0, 0, 16, "vertex anim");
        anim->startFrame= JI(a, "inicio", 1);
        anim->endFrame  = JI(a, "fin", 250);
        anim->fps       = JI(a, "fps", 30);
        anim->target    = mesh;
        // FRAMES: preferir el blob binario (self-contained). Legacy: frameCount + .obj.
        std::string buffers = JS(a, "buffers", "");
        bool cargado = false;
        if (!buffers.empty()) {
            std::vector<unsigned char> datos;
            // por RutaJson como TODAS las demas referencias: en v4 el blob es la
            // entrada "animaciones/<slug>.bin" y sale del contenedor montado; en v3
            // era un archivo suelto relativo a la carpeta del proyecto/zip
            if (w3dFileSystem::ReadFileBytes(RutaJson(buffers, base), datos) && !datos.empty())
                cargado = VertexAnimDeserializar(*anim, &datos[0], datos.size());
            // ---- EL BLOB ESTABA Y NO SE PUDO LEER: NI SILENCIO NI BORRARLO -------------
            //  Hermano exacto de W3dAjenos::noCargo de la malla, y quedaba abierto: aca no
            //  salia NI UNA LINEA de log. La anim quedaba con 0 frames y el proximo guardado
            //  la borraba PARA SIEMPRE (EscribirAnimsVertex no emite "buffers" sin frames y
            //  PreservarPasajeras descarta a proposito todo lo huerfano bajo "animaciones/",
            //  o sea que el blob viejo desaparece del zip y la anim queda como cascara vacia).
            //  Se marca y se avisa EN PANTALLA; el guardado se frena en EscribirAnimsVertex.
            if (!cargado) {
                anim->noCargo = true;
                w3dLogfE("[W3D] '%s': no pude leer los frames de la vertex anim '%s' (%s): queda VACIA y no la voy a guardar encima",
                         mesh->name.c_str(), anim->name.c_str(), buffers.c_str());
                W3dAvisof(true, "No pude leer los frames de la animacion '%s' de '%s': queda VACIA y no la voy a guardar encima (ver whisk3d.log)",
                          W3dNombreCorto(anim->name).c_str(), W3dNombreCorto(mesh->name).c_str());
            }
        }
        if (cargado && mesh->vertexSize > 0 && anim->vcount != mesh->vertexSize) {
            // ULTIMO RECURSO, y desde la etapa 3 casi nunca se usa: los keyframes son paralelos a
            // vertex[] (POR RENDER-VERT) y el GLB re-splitteaba los verts (24 -> 36), asi que
            // habia que ADIVINAR el vertice por CERCANIA DE POSICION contra la pose base. Con la
            // geometria en .w3dm el split se reproduce EXACTO y los keyframes entran DIRECTO;
            // esto queda solo para los .w3d viejos (y para una malla editada por fuera).
            w3dLogfW("[W3D] '%s': la vertex anim '%s' trae %d vertices y la malla tiene %d -> remapeo por POSICION (aproximado)",
                     mesh->name.c_str(), anim->name.c_str(), anim->vcount, mesh->vertexSize);
            g_w3dVaRemapeos++;
            anim->target = mesh;
            VertexAnimRemapPorPosicion(*anim, mesh);
        }
        // legacy .obj (LoadVertexFrames los lee): es el tope del bucle que abre un .obj por frame.
        // Acotado para que un numero disparatado no se traduzca en millones de aperturas de archivo.
        if (!cargado) anim->frameCount = JIRango(a, "frames", 0, 0, 100000, "vertex anim");
        CargarCurvasAnim(a, anim);
        NewActiveVertexAnimation(mesh, anim);
    }
    LoadVertexFrames(mesh);   // solo carga de .obj los que NO trajeron blob (frames vacios)
}

// ===========================================================================
//  ARMATURES en el .w3d (Fase 3): lectura del bloque "armature" + la referencia
//  "modArmature" de las mallas (se resuelve POR NOMBRE al final de la carga,
//  cuando ya existen todos los objetos) + los "vgroups" por POSICION.
// ===========================================================================
static void JVec3(JVal* j, const char* k, Vector3& v) {
    JVal* x = JHijo(j, k, 5);
    if (x && x->lista.size() >= 3) { v.x = (float)x->lista[0]->num; v.y = (float)x->lista[1]->num; v.z = (float)x->lista[2]->num; }
}
static void JMat16(JVal* j, const char* k, Matrix4& m) {
    JVal* x = JHijo(j, k, 5);
    if (x && x->lista.size() >= 16) for (int i = 0; i < 16; i++) m.m[i] = (float)x->lista[i]->num;
}
struct PendModArm { Mesh* m; std::string nombre; bool cache; float skip; };
static std::vector<PendModArm> gPendModArm; // referencias modArmature a resolver al final de la carga

// AUDIT versiones: el STACK de modificadores (Mirror/Screw/SubSurf/Array/Boolean)
// ahora viaja en el .w3d ("modificadores"). El target del Mirror va POR NOMBRE y se
// resuelve al final de la carga (como modArmature); las mallas con stack se anotan
// para regenerar su malla de render cuando ya esta todo resuelto.
struct PendModTgt { Modifier* md; std::string nombre; };
static std::vector<PendModTgt> gPendModTgt;
static std::vector<Mesh*> gPendModGen;

static int TipoModInt(const std::string& t) {
    if (t == "screw")   return ModifierType::Screw;
    if (t == "mirror")  return ModifierType::Mirror;
    if (t == "array")   return ModifierType::Array;
    if (t == "subsurf") return ModifierType::SubdivisionSurface;
    if (t == "boolean") return ModifierType::Boolean;
    if (t == "cullingtri") return ModifierType::CullingTri;
    return -1;   // desconocido (o "armature", que va por modArmature): se ignora
}

// lee una lista JSON de 3 bools ("ejes"/"copyPos") con defaults
static void JBools3(JVal* j, const char* k, bool* a, bool* b, bool* c) {
    JVal* v = JHijo(j, k, 5);
    if (!v) return;
    if (v->lista.size() > 0 && v->lista[0]->tipo == 3) *a = v->lista[0]->b;
    if (v->lista.size() > 1 && v->lista[1]->tipo == 3) *b = v->lista[1]->b;
    if (v->lista.size() > 2 && v->lista[2]->tipo == 3) *c = v->lista[2]->b;
}

static void CargarModificadores(JVal* j, Mesh* mesh) {
    JVal* jm = JHijo(j, "modificadores", 5);
    if (!jm || !mesh) return;
    for (size_t i = 0; i < jm->lista.size(); i++) {
        JVal* e = jm->lista[i]; if (!e || e->tipo != 4) continue;
        int tipo = TipoModInt(JS(e, "tipo", ""));
        if (tipo < 0) { w3dLogfW("[W3D] modificador de tipo desconocido '%s': se ignora", JS(e, "tipo", "").c_str()); continue; }
        Modifier* md = new Modifier(tipo, JS(e, "nombre", NombreTipoModificador(tipo)));
        md->mostrarViewport = JB(e, "viewport", true);
        md->mostrarEdit     = JB(e, "edit", true);
        if (tipo == ModifierType::Mirror) {
            JBools3(e, "ejes", &md->ejeX, &md->ejeY, &md->ejeZ);
            md->merge     = JB(e, "merge", md->merge);
            md->mergeDist = JF(e, "mergeDist", md->mergeDist);
            md->clipping  = JB(e, "clipping", md->clipping);
            std::string tg = JS(e, "target", "");
            if (!tg.empty()) { PendModTgt p; p.md = md; p.nombre = tg; gPendModTgt.push_back(p); }
        } else if (tipo == ModifierType::SubdivisionSurface) {
            md->subLevel       = JF(e, "nivel", md->subLevel);
            md->subRenderLevel = JF(e, "nivelRender", md->subRenderLevel);
            md->subSimple      = JB(e, "simple", md->subSimple);
        } else if (tipo == ModifierType::Screw) {
            md->screwAngle       = JF(e, "angulo", md->screwAngle);
            md->screwHeight      = JF(e, "altura", md->screwHeight);
            md->screwSteps       = JF(e, "pasos", md->screwSteps);
            md->screwRenderSteps = JF(e, "pasosRender", md->screwRenderSteps);
            md->screwAxis        = JI(e, "eje", md->screwAxis);
            md->screwStretchU    = JB(e, "stretchU", md->screwStretchU);
            md->screwStretchV    = JB(e, "stretchV", md->screwStretchV);
            md->screwSmooth      = JB(e, "suave", md->screwSmooth);
            md->screwMerge       = JB(e, "soldar", md->screwMerge);
            md->screwFlip        = JB(e, "flip", md->screwFlip);
        } else if (tipo == ModifierType::CullingTri) {
            // Culling por triangulo: metodo + sector/celda activo + NOMBRE del sidecar.
            // Los DATOS no viajan en el .w3d (son megabytes de indices): el sidecar se
            // ingiere al contenedor como cualquier asset y se relee de ahi. Sin "pvs"/
            // "vis" (archivos guardados antes de esto) se derivan del origen, como
            // siempre. "bsp" (el metodo viejo que nunca corto nada) cae a "celdas".
            std::string metodo = JS(e, "metodo", "triangulos");
            md->metodoPVS  = (metodo == "celdas" || metodo == "bsp") ? 1 : 0;
            md->sectorPVS  = JI(e, "sector", 0);
            // fallback de celda vacia (-1 = completa, N = celda N, 0/ausente = nada)
            md->sectorFallback = JI(e, "sectorFallback", 0);
            md->pvsArchivo = JS(e, "pvs", "");
            md->visArchivo = JS(e, "vis", "");
        }
        mesh->modificadores.push_back(md);
    }
    if (!mesh->modificadores.empty()) {
        mesh->modificadorActivo = (int)mesh->modificadores.size() - 1;
        gPendModGen.push_back(mesh);   // regenerar cuando los targets esten resueltos
    }
}

static Armature* BuscarArmaturePorNombre(Object* nodo, const std::string& n) {
    if (!nodo) return NULL;
    if (nodo->getType() == ObjectType::armature && nodo->name == n) return (Armature*)nodo;
    for (size_t i = 0; i < nodo->Childrens.size(); i++) {
        Armature* r = BuscarArmaturePorNombre(nodo->Childrens[i], n);
        if (r) return r;
    }
    return NULL;
}
// al FINAL de la carga (ya existen todos los objetos): cada malla con "modArmature" recupera su
// modificador Armature + skinArmature, y el rig autorado queda preparado para deformar.
static void ResolverModArmaturePendientes() {
    for (size_t i = 0; i < gPendModArm.size(); i++) {
        PendModArm& p = gPendModArm[i];
        Armature* a = BuscarArmaturePorNombre(SceneCollection, p.nombre);
        if (!a) { w3dLogfW("[W3D] modArmature '%s': no hay un armature con ese nombre (la malla queda sin rig)", p.nombre.c_str()); continue; }
        Modifier* md = new Modifier(ModifierType::Armature, NombreTipoModificador(ModifierType::Armature));
        md->target = (Object*)a;
        md->cacheAnim = p.cache;
        md->cacheSkip = p.skip;
        p.m->modificadores.push_back(md);
        p.m->skinArmature = a;
        p.m->skinCacheOn = p.cache;
        p.m->skinCacheSkip = (int)(p.skip + 0.5f);
        p.m->lastSkinFrame = -999999;
        PrepararSkinAutorado(a); // no-op en rigs importados
    }
    gPendModArm.clear();
    // targets del stack de modificadores (el plano del Mirror puede ser CUALQUIER
    // objeto de la escena): recien ahora existen todos -> resolver por nombre
    for (size_t i = 0; i < gPendModTgt.size(); i++) {
        Object* t = FindObjectByName(SceneCollection, gPendModTgt[i].nombre);
        if (t) gPendModTgt[i].md->target = t;
        else w3dLogfW("[W3D] modificador con target '%s': no hay un objeto con ese nombre", gPendModTgt[i].nombre.c_str());
    }
    gPendModTgt.clear();
    // con los targets resueltos, regenerar la malla de render de cada stack cargado
    for (size_t i = 0; i < gPendModGen.size(); i++)
        gPendModGen[i]->GenerarMallaModificada();
    gPendModGen.clear();
}
// lee un bloque "corners" ([[x,y,z,u,v,nx,ny,nz,w], ...]) y lo carga en 'ug' (UV GROUP: pesos por
// RENDER-VERT). Clave GEOMETRICA (ver el comentario largo en GuardarW3D.cpp):
//     v2: [x,y,z, u,v, nx,ny,nz, w]   (9 numeros)
//     v1: [x,y,z, u,v, w]             (6 numeros, sin normal: AMBIGUA en el cubo por defecto,
//                                      donde las 6 caras comparten el uv)
// El GLB del .w3d TRIANGULA la malla, asi que ni el indice de cara ni el de render-vert
// sobreviven; lo que sobrevive exacto es la geometria del corner.
//
// MATCH: se busca la MENOR distancia combinada (pos + uv + normal) y despues se le da el peso a
// TODOS los render-verts que empatan con esa distancia. El empate no es un error: el re-split del
// GLB parte un corner de quad en 2 triangulos y quedan 2 render-verts con la MISMA pos/uv/normal
// (son el mismo punto en el espacio UV) -> los dos tienen que llevar el peso, sino la mitad de los
// pesos vuelve en 0. La clave UV se compara contra el REST del skinning 2D si es valido (mesh->uv
// puede venir ya deformado por la pose). Fila sin match: se AVISA por el log (antes se descartaba
// en silencio y la perdida era invisible).
static void CargarCornersEnUVGroup(JVal* cor, Mesh* mesh, UVGroup* ug) {
    if (!cor || !mesh || !ug || !mesh->uv) return;
    const bool rest = ((int)mesh->uv2dRest.size() == mesh->vertexSize * 2);
    const GLfloat* uvSrc = rest ? &mesh->uv2dRest[0] : mesh->uv;
    std::vector<char> yaPuesto((size_t)mesh->vertexSize, 0); // sin entradas duplicadas al mismo render-vert
    int sinMatch = 0, filasV1 = 0, repetidas = 0;
    for (size_t k = 0; k < cor->lista.size(); k++) {
        JVal* row = cor->lista[k]; if (!row || row->tipo != 5 || row->lista.size() < 6) continue;
        const bool conNormal = (row->lista.size() >= 9);
        if (!conNormal) filasV1++;
        float x = (float)row->lista[0]->num, y = (float)row->lista[1]->num, z = (float)row->lista[2]->num;
        float u = (float)row->lista[3]->num, v = (float)row->lista[4]->num;
        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
        if (conNormal) { nx = (float)row->lista[5]->num; ny = (float)row->lista[6]->num; nz = (float)row->lista[7]->num; }
        float w = (float)row->lista[conNormal ? 8 : 5]->num;
        // pasada 1: la mejor distancia. pasada 2: todos los que empatan con ella.
        float mejorD = 1e-4f; bool hay = false;
        for (int pasada = 0; pasada < 2; pasada++) {
            for (int i = 0; i < mesh->vertexSize; i++) {
                float dx = mesh->vertex[i*3]-x, dy = mesh->vertex[i*3+1]-y, dz = mesh->vertex[i*3+2]-z;
                float du = uvSrc[i*2]-u, dv = uvSrc[i*2+1]-v;
                float d = dx*dx + dy*dy + dz*dz + du*du + dv*dv;
                if (conNormal && mesh->normals) {
                    float mx = mesh->normals[i*3]/127.0f, my = mesh->normals[i*3+1]/127.0f, mz = mesh->normals[i*3+2]/127.0f;
                    float L = sqrtf(mx*mx + my*my + mz*mz);
                    if (L > 1e-6f) { mx /= L; my /= L; mz /= L; }
                    float dnx = mx-nx, dny = my-ny, dnz = mz-nz;
                    d += dnx*dnx + dny*dny + dnz*dnz;
                }
                if (pasada == 0) { if (d < mejorD) { mejorD = d; hay = true; } }
                else if (d <= mejorD + 1e-8f) {
                    if (yaPuesto[(size_t)i]) { repetidas++; continue; }
                    yaPuesto[(size_t)i] = 1;
                    ug->verts.push_back(i); ug->pesos.push_back(w);
                }
            }
            if (!hay) break;
        }
        if (!hay) sinMatch++;
    }
    if (sinMatch)  w3dLogfW("[W3D] UV group '%s': %d de %d filas sin match en la malla '%s' (esos corners quedan en peso 0)",
                            ug->nombre.c_str(), sinMatch, (int)cor->lista.size(), mesh->name.c_str());
    if (repetidas) w3dLogfW("[W3D] UV group '%s': %d filas cayeron en un render-vert ya asignado (clave ambigua); gana la primera",
                            ug->nombre.c_str(), repetidas);
    if (filasV1)   w3dLogfW("[W3D] UV group '%s': %d filas en formato viejo (sin normal); la clave puede ser ambigua entre caras que se pisan en UV. Volve a guardar el proyecto para migrarlas",
                            ug->nombre.c_str(), filasV1);
}

// "vgroups" / "uvgroups" de una malla. Los DOS grupos son entidades distintas (ver el bloque
// VertexGroup/UVGroup en Mesh.h) y viajan en bloques distintos:
//   "vgroups"  -> pesos por POSICION del control-point ([x,y,z,w]), re-mapeados contra el
//                 vertCtrlPoint de la malla RECARGADA (robusto al re-split de vertices del GLB).
//   "uvgroups" -> pesos por CORNER con clave geometrica ([x,y,z,u,v,nx,ny,nz,w]).
// MIGRACION SILENCIOSA: en el formato de UN DIA los corners venian DENTRO de cada "vgroups" (la
// capa por corner que tenia el vertex group antes de separar las entidades). Si aparecen ahi, se
// leen igual y se convierten en un UV GROUP del MISMO NOMBRE -> el binding con el hueso 2D sigue
// funcionando y al re-guardar el proyecto ya sale en "uvgroups". Un .w3d mas viejo (sin corners de
// ningun tipo) abre sin UV groups: sus vertex groups quedan intactos y el skinning 2D no deforma
// hasta que se pinten UV groups (avisado por log si la malla tiene armature 2D).
static void JsonArm2DCargar(JVal* j, Mesh* mesh); // armatures 2D (formato nuevo + migracion del viejo), mas abajo
class Material;
static Material* MaterialPorNombre(const std::string& nom); // bloque raiz "materiales", mas abajo

static void CargarRigMesh(JVal* j, Mesh* mesh) {
    if (!j || !mesh) return;
    // QUIEN MANDA CUANDO ESTAN LOS DOS: si la malla ya trajo sus grupos del .w3dm (bloques VG y
    // UVG, por INDICE), el bloque viejo del JSON -que los guardaba por CLAVE GEOMETRICA porque el
    // GLB re-splitteaba los vertices- es una version ANTERIOR de lo mismo y se ignora con aviso.
    // Sin esta regla, un archivo con los dos abriria con los grupos DUPLICADOS.
    const bool vgDelW3dm = !mesh->vertexGroups.empty();
    const bool ugDelW3dm = !mesh->uvGroups.empty();
    JVal* jg = JHijo(j, "vgroups", 5);
    if (jg && vgDelW3dm) {
        w3dLogf("[W3D] '%s': la geometria ya trajo sus vertex groups (.w3dm) -> ignoro el bloque 'vgroups' del JSON (formato viejo)",
                mesh->name.c_str());
        jg = NULL;
    }
    if (jg && mesh->vertex && mesh->vertexSize > 0) {
        WeightPaintAsegurarMapa(mesh); // garantiza el mapa render-vert -> control-point
        // posicion representativa de cada control-point (primer render-vert que lo usa)
        int nCP = 0;
        for (int i = 0; i < mesh->vertexSize && i < (int)mesh->vertCtrlPoint.size(); i++)
            if (mesh->vertCtrlPoint[i] + 1 > nCP) nCP = mesh->vertCtrlPoint[i] + 1;
        std::vector<float> px(nCP, 0.0f), py(nCP, 0.0f), pz(nCP, 0.0f);
        std::vector<char> tiene(nCP, 0);
        for (int i = 0; i < mesh->vertexSize && i < (int)mesh->vertCtrlPoint.size(); i++) {
            int cp = mesh->vertCtrlPoint[i];
            if (cp < 0 || cp >= nCP || tiene[cp]) continue;
            px[cp] = mesh->vertex[i*3]; py[cp] = mesh->vertex[i*3+1]; pz[cp] = mesh->vertex[i*3+2];
            tiene[cp] = 1;
        }
        for (size_t g = 0; g < jg->lista.size(); g++) {
            JVal* e = jg->lista[g]; if (!e || e->tipo != 4) continue;
            VertexGroup* vg = new VertexGroup(JS(e, "nombre", "Group"));
            JVal* pts = JHijo(e, "puntos", 5);
            if (pts) for (size_t k = 0; k < pts->lista.size(); k++) {
                JVal* row = pts->lista[k]; if (!row || row->tipo != 5 || row->lista.size() < 4) continue;
                float x = (float)row->lista[0]->num, y = (float)row->lista[1]->num, z = (float)row->lista[2]->num;
                float w = (float)row->lista[3]->num;
                int mejor = -1; float mejorD = 1e-4f; // tolerancia (los GLB guardan float32 exacto)
                for (int cp = 0; cp < nCP; cp++) {
                    if (!tiene[cp]) continue;
                    float dx = px[cp]-x, dy = py[cp]-y, dz = pz[cp]-z;
                    float d = dx*dx + dy*dy + dz*dz;
                    if (d < mejorD) { mejorD = d; mejor = cp; }
                }
                if (mejor >= 0) { vg->verts.push_back(mejor); vg->pesos.push_back(w); }
            }
            // MIGRACION del formato de un dia: los corners venian DENTRO del vertex group. Se
            // convierten en un UV GROUP del mismo nombre (entidad nueva) y el vertex group queda
            // puro por control-point. Al re-guardar el proyecto ya salen en "uvgroups".
            JVal* corViejo = JHijo(e, "corners", 5);
            if (corViejo && !corViejo->lista.empty() && mesh->uv) {
                UVGroup* ug = new UVGroup(vg->nombre);
                CargarCornersEnUVGroup(corViejo, mesh, ug);
                mesh->uvGroups.push_back(ug);
                w3dLogf("[W3D] '%s': el vertex group '%s' traia pesos por corner del formato viejo -> migrados a un UV group homonimo",
                        mesh->name.c_str(), ug->nombre.c_str());
            }
            mesh->vertexGroups.push_back(vg);
        }
        if (!mesh->vertexGroups.empty()) mesh->grupoActivo = 0;
        mesh->lastSkinFrame = -999999;
    }
    // UV GROUPS (bloque propio). Se leen DESPUES de los vgroups para que la migracion de arriba
    // no pise a un archivo que ya tiene los dos (no puede pasar, pero el orden lo deja explicito).
    //
    // *** UN GRUPO DEL ARCHIVO = UN GRUPO EN MEMORIA ***  (perdida de datos F5)
    // Antes se buscaba un uvGroup HOMONIMO en la malla y, si aparecia, se le cargaban los
    // corners ENCIMA. Con eso un .w3d con dos "Mano" abria con UNO SOLO: los pesos del segundo
    // se mezclaban con los del primero (y encima duplicados, porque CargarCornersEnUVGroup
    // lleva su propio 'yaPuesto' por llamada) y el segundo grupo desaparecia sin un aviso.
    // Ademas ese espacio nunca llegaba a la reparacion de nombres, que es la que renumera los
    // repetidos con el criterio de siempre (primero-gana).
    // El UNICO merge que ERA intencional es el de la MIGRACION de arriba: los corners que
    // venian dentro de un "vgroups" del formato de un dia crean un UV group homonimo, y si el
    // MISMO archivo trajera tambien el bloque "uvgroups" (no puede pasar hoy, pero es el caso
    // que el codigo viejo queria cubrir) ese bloque es la version NUEVA del mismo grupo. Ese
    // caso se distingue EXPLICITAMENTE: solo se completan los grupos que creo la migracion, UNO
    // POR NOMBRE (marcados como consumidos), y se les LIMPIAN los corners migrados antes de
    // cargar los del bloque nuevo (mezclar las dos versiones daba pesos duplicados).
    // Cualquier otro homonimo entra como GRUPO SEPARADO y lo renumera W3dNombresRepararEscena.
    JVal* jug = JHijo(j, "uvgroups", 5);
    if (jug && ugDelW3dm) {
        w3dLogf("[W3D] '%s': la geometria ya trajo sus UV groups (.w3dm) -> ignoro el bloque 'uvgroups' del JSON (formato viejo)",
                mesh->name.c_str());
        jug = NULL;
    }
    if (jug && mesh->vertex && mesh->vertexSize > 0 && mesh->uv) {
        const size_t nMigrados = mesh->uvGroups.size();   // los que dejo la migracion de "vgroups"
        std::vector<char> migradoUsado(nMigrados, 0);
        for (size_t g = 0; g < jug->lista.size(); g++) {
            JVal* e = jug->lista[g]; if (!e || e->tipo != 4) continue;
            std::string nom = JS(e, "nombre", "UV Group");
            UVGroup* ug = NULL;
            for (size_t k = 0; k < nMigrados; k++)       // SOLO contra los migrados, y una sola vez
                if (!migradoUsado[k] && mesh->uvGroups[k]->nombre == nom) {
                    migradoUsado[k] = 1;
                    ug = mesh->uvGroups[k];
                    ug->verts.clear(); ug->pesos.clear();  // el bloque nuevo MANDA sobre el migrado
                    w3dLogf("[W3D] '%s': el UV group '%s' venia migrado de 'vgroups' y el archivo trae "
                            "tambien su bloque 'uvgroups' -> gana el bloque nuevo", mesh->name.c_str(), nom.c_str());
                    break;
                }
            if (!ug) { ug = new UVGroup(nom); mesh->uvGroups.push_back(ug); }
            CargarCornersEnUVGroup(JHijo(e, "corners", 5), mesh, ug);
        }
        // el activo del formato viejo: el JSON no lo guardaba, asi que queda el 0. VA ADENTRO del
        // if a proposito: cuando los grupos vienen del .w3dm el activo lo dice el archivo
        // (directiva uvgactiva) y pisarlo aca rompia el round-trip byte a byte.
        if (!mesh->uvGroups.empty()) mesh->uvGrupoActivo = 0;
    }
    std::string ma = JS(j, "modArmature", "");
    if (!ma.empty()) {
        PendModArm p; p.m = mesh; p.nombre = ma;
        p.cache = JB(j, "modArmatureCache", false);
        p.skip  = JF(j, "modArmatureSkip", 0.0f);
        gPendModArm.push_back(p);
    }
    // ARMATURES 2D DEL MESH: huesos en espacio UV (campo del objeto malla, junto a vgroups).
    // La POSE del archivo se RE-APLICA aca mismo (Armature2DAplicar): el editor y el juego
    // compilado ven las UV posadas al abrir.
    //
    // DOS FORMATOS:
    //   NUEVO  "armatures2d": [ { nombre, huesos:[...], anims:[...], animActiva } ] + "armature2dActivo"
    //   VIEJO  "armature2d": [huesos] + "anims2d": [clips] + "anim2dActiva"   (UN solo rig)
    // El viejo se MIGRA en silencio al armature 0 (se llama "Armature 2D") y al guardar sale ya en
    // el formato nuevo. Ausencia de los dos = malla sin rig 2D -> RETROCOMPAT total (la capa uv
    // horneada de la vertex anim sigue reproduciendose como siempre).
    // Reusa CargarCurvaJson (generico, no sabe de huesos).
    JsonArm2DCargar(j, mesh);
}

// lee los huesos de un bloque JSON (lista de objetos) dentro de 'arm'
static void JsonArm2DHuesos(JVal* jl, Armature2D* arm) {
    if (!jl || !arm) return;
    for (size_t b = 0; b < jl->lista.size(); b++) {
        JVal* e = jl->lista[b]; if (!e || e->tipo != 4) continue;
        W3dBone2D hb;
        hb.nombre = JS(e, "nombre", "Bone");
        hb.padre  = JI(e, "padre", -1);
        hb.conectado = !JB(e, "suelto", false); // "Disconnect Bone": emparentado sin soldar
        JVal* h = JHijo(e, "head", 5);
        if (h && h->lista.size() >= 2) { hb.headU = (float)h->lista[0]->num; hb.headV = (float)h->lista[1]->num; }
        JVal* t = JHijo(e, "tail", 5);
        if (t && t->lista.size() >= 2) { hb.tailU = (float)t->lista[0]->num; hb.tailV = (float)t->lista[1]->num; }
        JVal* p = JHijo(e, "pose", 5);
        if (p && p->lista.size() >= 5) {
            hb.poseTU  = (float)p->lista[0]->num; hb.poseTV = (float)p->lista[1]->num;
            hb.poseRot = (float)p->lista[2]->num;
            hb.poseSX  = (float)p->lista[3]->num; hb.poseSY = (float)p->lista[4]->num;
        }
        arm->huesos.push_back(hb);
    }
    // misma cota que el armature 3D: 'padre' fuera de rango o en ciclo -> raiz. Aca ademas
    // evita colgar los recorridos que suben por la cadena sin contador (UVEditor: borrar huesos).
    { std::vector<int> pad(arm->huesos.size());
      for (size_t i = 0; i < arm->huesos.size(); i++) pad[i] = arm->huesos[i].padre;
      if (SanearPadres(pad, arm->nombre.c_str()))
          for (size_t i = 0; i < arm->huesos.size(); i++) arm->huesos[i].padre = pad[i]; }
}

// lee los clips de un bloque JSON (lista de objetos) dentro de 'arm'
static void JsonArm2DClips(JVal* jl, Armature2D* arm) {
    if (!jl || !arm) return;
    for (size_t i = 0; i < jl->lista.size(); i++) {
        JVal* e = jl->lista[i]; if (!e || e->tipo != 4) continue;
        Armature2DAnimation* an = new Armature2DAnimation(JS(e, "nombre", "Anim2D"));
        an->fps        = JI(e, "fps", 30);
        an->startFrame = JI(e, "inicio", 1);
        an->endFrame   = JI(e, "fin", 0);
        JVal* jt = JHijo(e, "tracks", 5);
        if (jt) for (size_t t = 0; t < jt->lista.size(); t++) {
            JVal* tj = jt->lista[t]; if (!tj || tj->tipo != 4) continue;
            Bone2DTrack tr;
            tr.bone = JI(tj, "hueso", -1);
            JVal* jc = JHijo(tj, "curvas", 5);
            if (jc) for (size_t c = 0; c < jc->lista.size(); c++) {
                AnimProperty ap;
                if (CargarCurvaJson(jc->lista[c], ap)) tr.Propertys.push_back(ap);
            }
            an->tracks.push_back(tr);
        }
        arm->anims.push_back(an);
    }
}

static void JsonArm2DCargar(JVal* j, Mesh* mesh) {
    JVal* jarms = JHijo(j, "armatures2d", 5);
    if (jarms) {                                   // ---- FORMATO NUEVO (varios armatures) ----
        for (size_t a = 0; a < jarms->lista.size(); a++) {
            JVal* e = jarms->lista[a]; if (!e || e->tipo != 4) continue;
            int idx = mesh->Arm2DAgregar(JS(e, "nombre", "Armature 2D"));
            Armature2D* arm = mesh->armatures2d[idx];
            JsonArm2DHuesos(JHijo(e, "huesos", 5), arm);
            JsonArm2DClips(JHijo(e, "anims", 5), arm);
            if (!arm->anims.empty()) {
                arm->animActiva = JI(e, "animActiva", 0);
                // indice de lista: fuera de rango (por arriba O por abajo) -> el primero
                if (arm->animActiva < 0 || arm->animActiva >= (int)arm->anims.size()) arm->animActiva = 0;
            }
        }
        mesh->armature2dActivo = JI(j, "armature2dActivo", 0);
        if (mesh->armature2dActivo < 0 || mesh->armature2dActivo >= (int)mesh->armatures2d.size())
            mesh->armature2dActivo = mesh->armatures2d.empty() ? -1 : 0;
    } else {                                       // ---- FORMATO VIEJO (un solo rig): MIGRAR ----
        JVal* jh = JHijo(j, "armature2d", 5);
        JVal* jc = JHijo(j, "anims2d", 5);
        if (!jh && !jc) return;                    // malla sin rig 2D: nada que hacer
        Armature2D* arm = mesh->armatures2d[mesh->Arm2DAgregar("Armature 2D")];
        JsonArm2DHuesos(jh, arm);
        JsonArm2DClips(jc, arm);
        if (!arm->anims.empty()) {
            arm->animActiva = JI(j, "anim2dActiva", 0);
            if (arm->animActiva < 0 || arm->animActiva >= (int)arm->anims.size()) arm->animActiva = 0;
        }
    }
    if (!mesh->TieneArm2D()) return;
    mesh->Armature2DRestCapturar(); // el UV del GLB ES el rest (se exporta sin la pose)
    mesh->Armature2DAplicar();      // la pose guardada se ve al abrir (editor y runtime)
    // .w3d ANTERIOR a los pesos por corner: el rig 2D existe pero no hay UV groups (los
    // pesos viejos eran vertex groups por control-point, que el hueso 2D ya no mira). No se
    // hornea nada en silencio (eso es justo el bake que el dueno rechazo): se avisa y el
    // usuario re-pinta los UV groups en el editor UV.
    bool hayHuesos = false;
    for (size_t a = 0; a < mesh->armatures2d.size() && !hayHuesos; a++)
        if (mesh->armatures2d[a] && !mesh->armatures2d[a]->huesos.empty()) hayHuesos = true;
    if (hayHuesos && mesh->uvGroups.empty())
        w3dLogfW("[W3D] '%s' tiene armature 2D pero ningun UV group: el skinning 2D no deforma nada hasta que pintes pesos en el editor UV (los .w3d viejos pesaban con vertex groups, que ahora son SOLO del armature 3D)",
                 mesh->name.c_str());
}

// ---------------------------------------------------------------------------
//  RETOPOLOGIA: reconstruir los QUADS/NGONS de una malla modelada (rama "tipo":"glb").
//
//  SOLO MIGRACION: esto es como abrian los .w3d de antes del .w3dm. Hoy la geometria viaja con sus
//  poligonos NATIVOS (bloque F) y nada de esto hace falta; se conserva para que esos archivos
//  sigan abriendo (y queden convertidos en el primer guardado).
//
//  El GLB solo lleva TRIANGULOS: el importador arma un MeshFace por triangulo y, como cada corner
//  empuja su propia normal/uv, tampoco mergea los render-verts (un cubo entra 24 verts/6 quads y
//  vuelve 36 verts/12 tris). El .w3d guarda aparte el bloque "topologia" (ver GuardarW3D.cpp), que
//  es lo unico que falta: que corners forman cada poligono, con los indices DEL VERTEX ARRAY DEL GLB.
//
//  Como el orden de emision de triangulos del exportador es deterministico (OrdenCarasGLB) y el
//  importador lo conserva en faces3d, se camina en LOCKSTEP: la cara i de "caras" consume sus n-2
//  triangulos y de ahi salen los render-verts de cada corner (abanico 0-1-2, 0-2-3, ...). Con eso se
//  arma la tabla indiceGLB -> render-vert y se REHACEN los arrays EN EL ORDEN DEL GLB, que es el
//  mismo orden que tenian en el editor al guardar -> vertexSize / orden / atributos identicos
//  (importante: la vertex anim vuelve a matchear 1:1 y el round-trip byte a byte cierra).
//
//  Cualquier inconsistencia (el .glb no es pareja del .w3d, alguien lo edito por fuera, el abanico
//  no cierra) ABORTA y la malla queda EXACTAMENTE como hoy, triangulada: el peor caso es el presente.
//  Devuelve true si reconstruyo.
// ---------------------------------------------------------------------------

// mismos atributos (pos + normal + uv + color)? Los corners que vienen del MISMO vertice del GLB
// son copias exactas (el importador los copia del mismo accessor), asi que la comparacion es EXACTA.
static bool MismoRenderVert(Mesh* m, int a, int b) {
    if (a == b) return true;
    if (a < 0 || b < 0 || a >= m->vertexSize || b >= m->vertexSize) return false;
    for (int q = 0; q < 3; q++) if (m->vertex[a*3+q] != m->vertex[b*3+q]) return false;
    if (m->normals) for (int q = 0; q < 3; q++) if (m->normals[a*3+q] != m->normals[b*3+q]) return false;
    if (m->uv)      for (int q = 0; q < 2; q++) if (m->uv[a*2+q]      != m->uv[b*2+q])      return false;
    return true;
}

// ============================================================================
//  BORRAR 'origen' EN TODO EL SUBARBOL IMPORTADO (rama legacy "glb")
//
//  ImportModeloPorExtension marca Mesh::origen con el archivo importado, pero en
//  la rama legacy ese "archivo" es una ENTRADA DEL PROPIO .w3d, no un archivo del
//  usuario: dejarlo puesto llena EXTERNOS.txt de rutas internas que no existen.
//  Antes se limpiaba SOLO el nodo de arriba: un .glb con un ARMATURE que trae la
//  malla COMO HIJA dejaba a la hija con origen = ruta interna del contenedor y al
//  guardar aparecia en EXTERNOS.txt como archivo que falta.
// ============================================================================
static void LimpiarOrigenRec(Object* o) {
    if (!o) return;
    if (o->getType() == ObjectType::mesh) ((Mesh*)o)->origen.clear();
    for (size_t i = 0; i < o->Childrens.size(); i++) LimpiarOrigenRec(o->Childrens[i]);
}

static bool RetopologizarDesdeTopologia(JVal* j, Mesh* m) {
    if (!j || !m) return false;
    JVal* topo = JHijo(j, "topologia", 4);
    if (!topo) return false;                                  // .w3d viejo: camino de siempre (triangulado)
    JVal* caras = JHijo(topo, "caras", 5);
    if (!caras || caras->lista.empty()) return false;
    const char* nom = m->name.c_str();
    if (!m->vertex || m->vertexSize <= 0 || m->faces3d.empty()) {
        w3dLogfW("[W3D] '%s': hay bloque 'topologia' pero la malla del GLB vino vacia -> se ignora", nom);
        return false;
    }

    // ---- pasada 0: validar las filas + contar triangulos + mayor indice ----
    int maxIdx = -1; size_t totTri = 0;
    for (size_t i = 0; i < caras->lista.size(); i++) {
        JVal* c = caras->lista[i];
        if (!c || c->tipo != 5 || c->lista.size() < 3) {
            w3dLogfW("[W3D] '%s': 'topologia' con una cara de menos de 3 corners -> se ignora el bloque (la malla queda triangulada)", nom);
            return false;
        }
        totTri += c->lista.size() - 2;
        for (size_t k = 0; k < c->lista.size(); k++) {
            if (!c->lista[k] || c->lista[k]->tipo != 1) { w3dLogfW("[W3D] '%s': 'topologia' con un indice no numerico -> se ignora el bloque", nom); return false; }
            int v = (int)c->lista[k]->num;
            if (v < 0) { w3dLogfW("[W3D] '%s': 'topologia' con un indice negativo -> se ignora el bloque", nom); return false; }
            if (v > maxIdx) maxIdx = v;
        }
    }
    int nGlb = JI(topo, "verts", -1);
    if (nGlb <= 0) nGlb = maxIdx + 1;                          // sin "verts": se deduce
    // COTA DURA contra algo REAL antes de reservar NADA: la malla TRIANGULADA que trae el GLB
    // tiene siempre >= nGlb render-verts (el importador del GLB no mergea: cada corner es un
    // render-vert propio). Un "verts" gigante o corrupto pedia dos vector<int> de ese tamano
    // (con "verts": 2000000000 eran 2x8 GB: el editor se comia la RAM y no volvia). Fuera de
    // rango se cae al fallback triangulado, igual que cualquier otra topologia inconsistente.
    if (nGlb > m->vertexSize) {
        w3dLogfW("[W3D] '%s': 'topologia' declara %d vertices y el GLB solo trae %d render-verts -> se ignora el bloque (la malla queda triangulada)",
                 nom, nGlb, m->vertexSize);
        return false;
    }
    if (maxIdx >= nGlb) {
        w3dLogfW("[W3D] '%s': 'topologia' referencia el vertice %d pero declara %d -> se ignora el bloque", nom, maxIdx, nGlb);
        return false;
    }
    if (totTri != m->faces3d.size()) {
        w3dLogfW("[W3D] '%s': 'topologia' pide %d triangulos y el GLB trae %d (el .glb no es pareja del .w3d) -> la malla queda triangulada",
                 nom, (int)totTri, (int)m->faces3d.size());
        return false;
    }

    // ---- pasada 1: lockstep triangulo a triangulo -> tabla indiceGLB -> render-vert ----
    std::vector<int> glbARender((size_t)nGlb, -1);
    std::vector<MeshFace> nuevas; nuevas.reserve(caras->lista.size());
    size_t t = 0;
    for (size_t i = 0; i < caras->lista.size(); i++) {
        JVal* c = caras->lista[i];
        const int n = (int)c->lista.size(), nTri = n - 2;
        const MeshFace& T0 = m->faces3d[t];
        if (T0.idx.size() != 3) { w3dLogfW("[W3D] '%s': el GLB no vino triangulado -> se ignora 'topologia'", nom); return false; }
        std::vector<int> rv((size_t)n, -1);
        rv[0] = T0.idx[0]; rv[1] = T0.idx[1]; rv[2] = T0.idx[2];
        for (int k = 1; k < nTri; k++) {
            const MeshFace& Tk = m->faces3d[t + (size_t)k];
            // el abanico tiene que cerrar: corner 0 comun y corner 1 = el ultimo corner del triangulo previo.
            // (se compara por ATRIBUTOS, no por indice: el importador del GLB no mergea, cada corner es un
            //  render-vert distinto aunque venga del mismo vertice del GLB)
            if (Tk.idx.size() != 3 || !MismoRenderVert(m, Tk.idx[0], rv[0]) || !MismoRenderVert(m, Tk.idx[1], rv[k+1])) {
                w3dLogfW("[W3D] '%s': el abanico de la cara %d de 'topologia' no cierra contra el GLB -> la malla queda triangulada", nom, (int)i);
                return false;
            }
            rv[k+2] = Tk.idx[2];
        }
        MeshFace mf; mf.mat = T0.mat; mf.smooth = -1;
        mf.idx.resize((size_t)n);
        for (int q = 0; q < n; q++) {
            int g = (int)c->lista[(size_t)q]->num;
            if (glbARender[(size_t)g] < 0) glbARender[(size_t)g] = rv[(size_t)q];
            else if (!MismoRenderVert(m, glbARender[(size_t)g], rv[(size_t)q])) {
                w3dLogfW("[W3D] '%s': el vertice %d de 'topologia' cae en dos render-verts distintos -> la malla queda triangulada", nom, g);
                return false;
            }
            mf.idx[(size_t)q] = g;   // por ahora en espacio GLB; se reindexa abajo
        }
        nuevas.push_back(mf);
        t += (size_t)nTri;
    }
    if (t != m->faces3d.size()) return false;   // (no deberia pasar: totTri ya lo garantiza)

    // shading POR CARA (sparse): [indiceDeCara, 0=flat|1=smooth]
    JVal* shade = JHijo(topo, "shade", 5);
    if (shade) for (size_t k = 0; k < shade->lista.size(); k++) {
        JVal* p = shade->lista[k]; if (!p || p->tipo != 5 || p->lista.size() < 2) continue;
        int fi = (int)p->lista[0]->num;
        if (fi >= 0 && fi < (int)nuevas.size()) nuevas[(size_t)fi].smooth = ((int)p->lista[1]->num) ? 1 : 0;
    }

    // ---- pasada 2: rehacer los arrays EN EL ORDEN DEL GLB (= el orden que tenian al guardar) ----
    std::vector<int> nuevoDe((size_t)nGlb, -1);
    int nuevoN = 0;
    for (int g = 0; g < nGlb; g++) if (glbARender[(size_t)g] >= 0) nuevoDe[(size_t)g] = nuevoN++;
    if (nuevoN <= 0) return false;
    if (nuevoN != nGlb)
        w3dLogfW("[W3D] '%s': %d vertices del GLB no los usa ninguna cara -> los indices se compactan (%d de %d). La numeracion sigue siendo deterministica, pero la vertex anim se remapea por posicion",
                 nom, nGlb - nuevoN, nuevoN, nGlb);

    GLfloat* nvx = new GLfloat[(size_t)nuevoN * 3];
    GLbyte*  nnr = m->normals     ? new GLbyte[(size_t)nuevoN * 3]  : NULL;
    GLfloat* nuv = m->uv          ? new GLfloat[(size_t)nuevoN * 2] : NULL;
    GLubyte* ncl = m->vertexColor ? new GLubyte[(size_t)nuevoN * 4] : NULL;
    std::vector<int> nCP; if (!m->vertCtrlPoint.empty()) nCP.assign((size_t)nuevoN, -1);
    std::vector<int> oldToNew((size_t)m->vertexSize, -1);   // para remapear la geometria suelta
    for (int g = 0; g < nGlb; g++) {
        int d = nuevoDe[(size_t)g]; if (d < 0) continue;
        int src = glbARender[(size_t)g];
        oldToNew[(size_t)src] = d;
        for (int q = 0; q < 3; q++) nvx[d*3+q] = m->vertex[src*3+q];
        if (nnr) for (int q = 0; q < 3; q++) nnr[d*3+q] = m->normals[src*3+q];
        if (nuv) for (int q = 0; q < 2; q++) nuv[d*2+q] = m->uv[src*2+q];
        if (ncl) for (int q = 0; q < 4; q++) ncl[d*4+q] = m->vertexColor[src*4+q];
        if (!nCP.empty() && src < (int)m->vertCtrlPoint.size()) nCP[(size_t)d] = m->vertCtrlPoint[(size_t)src];
    }
    for (size_t i = 0; i < nuevas.size(); i++)
        for (size_t k = 0; k < nuevas[i].idx.size(); k++) nuevas[i].idx[k] = nuevoDe[(size_t)nuevas[i].idx[k]];

    // geometria SUELTA: el GLB no la trae (se pierde igual que antes), pero si algo quedo, se remapea
    { std::vector<int> le; le.reserve(m->looseEdges.size());
      for (size_t i = 0; i + 1 < m->looseEdges.size(); i += 2) {
          int a = m->looseEdges[i], b = m->looseEdges[i+1];
          if (a < 0 || b < 0 || a >= (int)oldToNew.size() || b >= (int)oldToNew.size()) continue;
          if (oldToNew[(size_t)a] < 0 || oldToNew[(size_t)b] < 0) continue;
          le.push_back(oldToNew[(size_t)a]); le.push_back(oldToNew[(size_t)b]); }
      m->looseEdges.swap(le); }
    { std::vector<int> lv; lv.reserve(m->looseVerts.size());
      for (size_t i = 0; i < m->looseVerts.size(); i++) {
          int a = m->looseVerts[i];
          if (a >= 0 && a < (int)oldToNew.size() && oldToNew[(size_t)a] >= 0) lv.push_back(oldToNew[(size_t)a]); }
      m->looseVerts.swap(lv); }

    delete[] m->vertex;                            m->vertex = nvx;
    if (m->normals)     { delete[] m->normals;     m->normals = nnr; }
    if (m->uv)          { delete[] m->uv;          m->uv = nuv; }
    if (m->vertexColor) { delete[] m->vertexColor; m->vertexColor = ncl; }
    m->vertexSize = nuevoN;
    if (!nCP.empty()) m->vertCtrlPoint.swap(nCP);
    m->faces3d.swap(nuevas);
    m->uvSelVert.clear();          // seleccion del editor UV: era del render triangulado
    m->LiberarCapas(false);        // capas por corner (nC cambio); NO tocar vertex/UV groups
    // meshSmooth EXPLICITO: antes se adivinaba con MeshShadingImportadoEsSmooth sobre la malla triangulada
    m->meshSmooth = JB(topo, "suave", m->meshSmooth);
    m->ReagruparMeshParts();       // faces[] + rangos por mesh part desde faces3d.mat (+ VBO)
    m->CalcularBordes();           // edges/posRep/centroGeom: las diagonales dejan de ser aristas
    m->SincronizarCornerNormal();  // cornerNormal desde normals[]: el 1er GenerarRender no reinventa el shading
    return true;
}

static void JsonObjeto(JVal* j, Object* parent, const std::string& base);

// ---------------------------------------------------------------------------
//  CREA el objeto de UN nodo del .w3d y lo devuelve; los HIJOS NO se leen aca
//  (los lee JsonObjeto, en UN solo lugar, ver abajo). Es el ESPEJO exacto de
//  EscribirObjeto/EscribirHijos (GuardarW3D.cpp): antes cada rama leia -o se
//  OLVIDABA de leer- "hijos", y camara/luz/script/ui/modelo/glb no lo leian, o
//  sea que ni siquiera un .w3d editado a mano podia colgarles nada.
//  NULL = el nodo no creo ningun objeto (tipo desconocido, o la carga fallo):
//  sus hijos cuelgan del padre, que es lo que ya hacia el fallback generico.
// ---------------------------------------------------------------------------
static Object* JsonObjetoCrear(JVal* j, Object* parent, const std::string& base) {
    std::string tipo = JS(j, "tipo", "");

    if (tipo == "coleccion") {
        Collection* c = new Collection(parent);
        // JsonComunes y no solo el nombre: el guardado nuevo escribe visible/pos/rot/
        // escala (una Collection oculta volvia VISIBLE al reabrir). Tolerante: los
        // archivos viejos sin esos campos quedan con los defaults de siempre.
        JsonComunes(j, c);
        // orden de dibujo para transparentes (ausente = false: archivos viejos igual)
        c->ordenarPorCamara = JB(j, "ordenarPorCamara", false);
        c->ordenarUnaVez    = JB(j, "ordenarUnaVez", false);
        // lote estatico (P4): ausente = off (archivos viejos, cero cambio)
        c->lote = (JS(j, "lote", "") == "estatico") ? 1 : 0;
        return c;
    }
    if (tipo == "camara") {
        Camera* c = new Camera(parent);
        JsonComunes(j, c);
        c->fov = JF(j, "fov", c->fov);           // lente (default 45 si el .w3d es viejo)
        c->orthographic = JB(j, "orto", c->orthographic);
        c->nearClip = JF(j, "near", c->nearClip);
        c->farClip  = JF(j, "far",  c->farClip);
        // ENCUADRE DECLARADO (ver Camera::aspecto). Ausente = 0 = como siempre.
        c->aspecto  = JF(j, "aspecto", c->aspecto);
        std::string t = JS(j, "target", "");
        if (!t.empty()) c->SetTarget(t);   // se resuelve en ReloadAll al final
        // RIEL (AUDIT versiones): la Curve por la que viaja la camara + su offset
        std::string riel = JS(j, "riel", "");
        if (!riel.empty()) c->SetRiel(riel);   // idem: ReloadRiel al final
        c->offsetRiel = JI(j, "offsetRiel", c->offsetRiel);
        // MIRADA DEL RIEL (ver Camera.h). Ausente = false = look-at de siempre.
        c->usarRotacionDelRiel = JB(j, "miradaRiel", c->usarRotacionDelRiel);
        return c;
    }
    if (tipo == "espejo") {   // objeto Espejo (AUDIT versiones: antes se perdia al guardar)
        Mirror* mi = new Mirror(parent);
        JsonComunes(j, mi);
        JBools3(j, "ejes", &mi->mirrorX, &mi->mirrorY, &mi->mirrorZ);
        std::string t = JS(j, "target", "");
        if (!t.empty()) mi->SetTarget(t);
        // MULTI-TARGET: "targets" es UNA cadena "A, B, C" (mismo idioma que
        // "distancias" del LOD). Opcional: sin ella manda "target" y un archivo
        // viejo abre exactamente igual.
        std::string ts = JS(j, "targets", "");
        if (!ts.empty()) mi->SetTargetsTexto(ts);
        // reflejo en agua (ver Mirror.h): sin z + acotado al estanque.
        // "limites" = [u0, u1, v0, v1] en el PLANO del espejo (mismo idioma que "distancias").
        mi->sinProfundidad = JB(j, "sinProfundidad", false);
        // hundimientoMaximo: DEPRECADO (ver Mirror.h). Se acepta, se avisa, sin efecto.
        mi->hundimientoMaximo = JF(j, "hundimientoMaximo", 0.0f);
        if (mi->hundimientoMaximo > 0.0f)
            w3dLogfW("[W3D] Mirror '%s': hundimientoMaximo esta DEPRECADO y no tiene "
                     "efecto (la reflexion es geometrica pura; el recorte del reflejo "
                     "es la silueta del agua en pantalla)", mi->name.c_str());
        if (JHijo(j, "recorte", 3)) { mi->recorteDeclarado = true; mi->recorte = JB(j, "recorte", true); }   // 3 = bool
        JVal* lim = JHijo(j, "limites", 5);
        if (lim && lim->lista.size() == 4 && lim->lista[0] && lim->lista[1]
            && lim->lista[2] && lim->lista[3]) {
            mi->usaLimites = true;
            mi->limU0 = (float)lim->lista[0]->num;
            mi->limU1 = (float)lim->lista[1]->num;
            mi->limV0 = (float)lim->lista[2]->num;
            mi->limV1 = (float)lim->lista[3]->num;
        }
        return mi;
    }
    if (tipo == "instancia") {   // Instance: copias enlazadas del target
        Instance* in = new Instance(parent);
        JsonComunes(j, in);
        std::string t = JS(j, "target", "");
        if (!t.empty()) in->SetTarget(t);
        // 'count' es un size_t que manda un for del RENDER: un valor negativo del archivo se
        // convertia en ~1.8e19 vueltas (el editor no volvia del primer frame) y uno gigante
        // tambien. Se acota a algo que se pueda dibujar.
        in->count     = (size_t)JIRango(j, "count", (int)in->count, 0, 4096, "instancia");
        in->mirror    = JB(j, "espejado", false);
        in->mirrorEje = JIRango(j, "espejoEje", 0, 0, 2, "instancia");
        return in;
    }
    if (tipo == "constraint") {   // el objeto Constraint VIEJO (dado de baja): se MIGRA
        // misma mecanica que la rama de texto (ver PendConsViejo): Empty + entrada
        // pendiente, y las props van al objeto APUNTADO al final de la carga. Aca el
        // default de "pitch" es FALSE, que es el del formato JSON y NO el del texto.
        bool cx = false, cy = false, cz = false;
        JBools3(j, "copyPos", &cx, &cy, &cz);
        Object* e = NodoConstraintViejo(parent, JS(j, "target", ""),
                                        JB(j, "horizontal", true), JB(j, "pitch", false),
                                        cx, cy, cz);
        JsonComunes(j, e);
        return e;
    }
    if (tipo == "curva") {   // Curve cargada de archivo (riel de camara)
        std::string rel = JS(j, "archivo", "");
        Curve* cv = new Curve(parent);
        JsonComunes(j, cv);
        // "archivo" vacio = curva sin archivo de origen (el guardado avisa y no
        // serializa su geometria): vuelve VACIA, pero el nodo y sus hijos vuelven
        if (rel.empty())
            w3dLogfW("[W3D] la curva '%s' no tiene archivo de origen (queda vacia)", cv->name.c_str());
        else if (!cv->LoadFromFile(RutaJson(rel, base)))
            w3dLogfW("[W3D] no pude cargar la curva %s (queda vacia)", rel.c_str());
        // LISTA DE CARGA (streaming): el sidecar .cargas.json (opcional)
        { std::string cg = JS(j, "cargas", "");
          if (!cg.empty()) cv->CargarListaCarga(RutaJson(cg, base)); }
        return cv;
    }
    if (tipo == "luz") {
        Light* l = Light::Create(parent);
        JsonComunes(j, l);
        JVal* col = JHijo(j, "color", 5);
        if (col && col->lista.size() >= 3)
            for (int i = 0; i < 3; i++) l->diffuse[i] = (float)col->lista[i]->num;
        return l;
    }
    if (tipo == "script") {
        // MIGRACION del objeto Script/Control (ver el comentario largo de la rama "Gamepad"
        // del .w3d de texto): vuelve como Empty con nombre, transformacion, hijos y scripts.
        // La fisica hardcodeada ("fisica") y el "target" se descartan a proposito.
        Empty* g = new Empty(parent);
        JsonComunes(j, g);
        W3dAvisof(false, "El objeto Script '%s' ya no existe: se migro a un objeto vacio con sus "
                         "scripts. Su fisica hardcodeada (velocidad/gravedad/limites) NO se migra.",
                  g->name.c_str());
        // (los scripts los lee el call site COMUN de JsonObjeto, como los constraints)
        return g;
    }
    if (tipo == "ui") {
        std::string archivo = JS(j, "archivo", "");
        UI* u = CargarUIProyecto(archivo.empty() ? archivo : RutaJson(archivo, base), archivo);
        // (si no cargo, ya se aviso y el resto del proyecto sigue abriendo)
        // la escena UI se crea SIEMPRE colgada de la raiz: si en el .w3d venia como
        // HIJO de otro objeto, re-colgarla de su padre real (igual que la rama glb)
        if (u && parent && parent != SceneCollection && u->Parent != parent) {
            Object* vp = u->Parent ? u->Parent : SceneCollection;
            if (vp) for (size_t k = 0; k < vp->Childrens.size(); k++)
                if (vp->Childrens[k] == (Object*)u) { vp->Childrens.erase(vp->Childrens.begin() + k); break; }
            u->Parent = parent;
            parent->Childrens.push_back((Object*)u);
        }
        return (Object*)u;
    }
    if (tipo == "malla") {
        // ==================================================================
        //  LA RAMA DE HOY: la geometria es una entrada .w3dm del contenedor.
        //  Es interna SIEMPRE (el proyecto es autocontenido por construccion),
        //  asi que no depende de ningun archivo del disco del usuario.
        //  "origen" es solo INFORMATIVO ("de donde salio" / "reimportar"): si
        //  falta, se avisa y NO pasa nada, la malla ya esta adentro.
        // ==================================================================
        std::string geo = JS(j, "geometria", "");
        Mesh* mesh = new Mesh(parent, Vector3(0, 0, 0));
        std::vector<unsigned char> datos;
        W3dMallaInfo info;
        bool cargo = false;
        if (geo.empty())
            w3dLogfE("[W3D] una malla del proyecto no dice de que archivo sale (queda vacia)");
        else if (!w3dFileSystem::ReadFileBytes(RutaJson(geo, base), datos) || datos.empty())
            w3dLogfE("[W3D] no pude leer la geometria %s (la malla queda vacia)", geo.c_str());
        else if (!(cargo = W3dMallaLeer((const char*)&datos[0], datos.size(), mesh, &info)))
            w3dLogfE("[W3D] %s no es un .w3dm que este Whisk3D pueda leer (la malla queda vacia)", geo.c_str());
        // ---- EL .w3dm ESTABA Y NO SE PUDO LEER: NI SILENCIO NI HORNEAR ----------
        //  El lector se NIEGA (no es W3DMESH / lexico de una version mas nueva / falta V o F)
        //  y aca ya hay un Mesh NUEVO y VACIO en la escena, con el nombre y el transform del
        //  proyecto pero sin geometria. Antes esto salia SOLO por el log: el usuario veia el
        //  objeto vacio, guardaba, y el primer guardado HORNEABA la nada (el .w3dm quedaba con
        //  0 puntos / 0 caras). Exactamente el degradado silencioso que el formato existe para
        //  impedir, y en el caso estrella: un .w3d escrito por un Whisk3D MAS NUEVO.
        //  Se marca la malla como "no cargo" (EscribirMallaW3dm se niega a guardarla encima,
        //  igual que con el freno "requiere") y se avisa EN PANTALLA. El usuario puede seguir
        //  trabajando; lo que no puede es destruir el archivo sin enterarse.
        if (!cargo && !geo.empty()) {
            mesh->w3dmAjenos.noCargo = true;
            W3dAvisof(true, "No pude leer la geometria de '%s': queda VACIA y no la voy a guardar encima (ver whisk3d.log)",
                      W3dNombreCorto(geo).c_str());
        }
        // SILENCIOSO JAMAS: cada problema del archivo sale por el log con su numero de
        // linea. FUERA del 'if (cargo)' a proposito: cuando la lectura FRACASA es cuando
        // mas falta hace el motivo (ej: "el archivo no trae el bloque F"), y adentro del
        // if se perdia justo en ese caso.
        for (size_t k = 0; k < info.avisos.size(); k++)
            w3dLogfW("[W3D] %s: %s", geo.c_str(), info.avisos[k].c_str());
        if (cargo) {
            if (info.soloLectura)
                w3dLogfW("[W3D] %s pide un bloque que este Whisk3D no entiende: la malla abre pero guardar encima la degradaria", geo.c_str());
            // el material vive en la ESCENA y el .w3dm lo referencia POR NOMBRE (bloque PARTMAT):
            // el Core no puede resolver el puntero, lo hace el editor con el bloque "materiales".
            for (size_t k = 0; k < mesh->materialsGroup.size() && k < info.materiales.size(); k++)
                mesh->materialsGroup[k].material = MaterialPorNombre(info.materiales[k]);
            // el cierre que el lector NO hace (es del Core y no puede llamar al editor):
            // index buffer + rangos por mesh part, bordes/posRep, y las capas activas al render.
            mesh->ReagruparMeshParts();
            mesh->CalcularBordes();
            mesh->AplicarCapasAlRender();
            // el mapeo render-vert -> control-point (vertCtrlPoint = posRep). SIN ESTO el skinning
            // 3D se corta en seco (SkinearMesh sale si el mapa no cubre los verts) y la malla
            // rigueada abria SIN DEFORMAR, en silencio.
            WeightPaintAsegurarMapa(mesh);
            mesh->lastSkinFrame = -999999;   // los vertex groups del .w3dm reestrenan el skinning
        }
        JsonComunes(j, mesh);   // nombre/transform del PROYECTO (manda sobre el "nombre" del .w3dm)
        // ORIGEN: el .obj/.fbx que trajo el usuario. Externo a proposito (lo edita otro programa).
        std::string org = JS(j, "origen", "");
        if (!org.empty()) {
            mesh->origen = RutaJson(org, base);
            if (!w3dFileSystem::FileExists(mesh->origen))
                w3dLogfW("[W3D] '%s': el archivo de origen '%s' no esta. La malla carga igual (viaja adentro del .w3d); solo no vas a poder reimportarla del original",
                         mesh->name.c_str(), mesh->origen.c_str());
        }
        CargarAnimsVertex(j, mesh, base);   // vertex anims (blob binario + curvas + rango)
        CargarRigMesh(j, mesh);             // modArmature + armatures 2D (los grupos vienen del .w3dm)
        CargarModificadores(j, mesh);       // el resto del stack (Mirror/Screw/SubSurf/...)
        CargarAnimUV(j, mesh);              // animacion UV "tira de atlas" (autoplay)
        return mesh;
    }
    if (tipo == "modelo") {   // wavefront referenciado + sus vertex anims
        std::string archivo = RutaJson(JS(j, "archivo", ""), base);
        Mesh* mesh = ImportWOBJ(archivo, parent, false);
        if (!mesh) { w3dLogfE("[W3D] no pude importar el modelo %s (objeto omitido)", archivo.c_str()); return NULL; }
        JsonComunes(j, mesh);
        CargarAnimsVertex(j, mesh, base);   // vertex anims (blob binario + curvas + rango)
        CargarRigMesh(j, mesh);             // vgroups por posicion + referencia modArmature (Fase 3)
        CargarModificadores(j, mesh);       // el resto del stack (Mirror/Screw/SubSurf/...)
        CargarAnimUV(j, mesh);              // animacion UV "tira de atlas" (autoplay)
        return mesh;
    }
    if (tipo == "glb") {      // malla modelada, exportada a GLB dentro del zip
        std::string archivo = RutaJson(JS(j, "archivo", ""), base);
        size_t antes = SceneCollection ? SceneCollection->Childrens.size() : 0;
        extern bool ImportModeloPorExtension(const std::string& path);
        ImportModeloPorExtension(archivo);
        Object* creado = NULL;
        // lo importado quedo al FINAL de la escena: aplicarle nombre/transform
        if (SceneCollection && SceneCollection->Childrens.size() > antes) {
            Object* nuevo = SceneCollection->Childrens.back();
            // EL GLB DE ADENTRO DEL .w3d NO ES UN "ORIGEN": es una entrada del propio
            // contenedor. Se limpia el SUBARBOL ENTERO (no solo el nodo de arriba: un
            // armature con la malla como hija dejaba a la hija con una ruta INTERNA y
            // esa ruta terminaba en EXTERNOS.txt como archivo que falta). Va ANTES del
            // reparent para que no dependa de donde quede colgado.
            LimpiarOrigenRec(nuevo);
            // el import cuelga SIEMPRE de la raiz; si este nodo venia como HIJO (ej: malla bajo su
            // armature), re-colgarlo del padre real (preserva la jerarquia del rig al reabrir).
            if (parent && parent != SceneCollection) {
                SceneCollection->Childrens.pop_back();
                nuevo->Parent = parent;
                parent->Childrens.push_back(nuevo);
            }
            JsonComunes(j, nuevo);
            if (nuevo->getType() == ObjectType::mesh) {
                // QUADS/NGONS: el GLB solo trae triangulos -> reconstruir los poligonos (y el orden de
                // los render-verts) desde el bloque "topologia". VA PRIMERO: los uvgroups se matchean
                // contra los render-verts y la vertex anim compara su vcount contra vertexSize.
                RetopologizarDesdeTopologia(j, (Mesh*)nuevo);
                CargarAnimsVertex(j, (Mesh*)nuevo, base);   // vertex anims de la malla modelada
                CargarRigMesh(j, (Mesh*)nuevo);             // vgroups por posicion + modArmature (Fase 3)
                CargarModificadores(j, (Mesh*)nuevo);       // el resto del stack (Mirror/Screw/SubSurf/...)
            }
            creado = nuevo;
        }
        return creado;
    }
    if (tipo == "armature") { // ARMATURE (Fase 3): huesos + clips + hijos
        Armature* a = new Armature(parent);
        JsonComunes(j, a);
        bool autorado = JB(j, "autorado", false);
        JVal* jb = JHijo(j, "bones", 5);
        if (jb) for (size_t i = 0; i < jb->lista.size(); i++) {
            JVal* e = jb->lista[i]; if (!e || e->tipo != 4) continue;
            W3dBone b;
            b.name = JS(e, "nombre", "Bone");
            b.parent = JI(e, "padre", -1);
            b.conectado = !JB(e, "suelto", false); // "Disconnect Bone": emparentado sin soldar
            JVec3(e, "head", b.head); JVec3(e, "tail", b.tail);
            if (JB(e, "rest", false)) { // rig importado guardado con fidelidad de FK/skinning
                b.hasRest = true;
                JVec3(e, "restT", b.restT); JVec3(e, "restR", b.restR); JVec3(e, "restS", b.restS);
                JVec3(e, "preRot", b.preRot); JVec3(e, "postRot", b.postRot);
                b.rotOrder = JI(e, "rotOrder", 0);
                JMat16(e, "bind", b.bind); JMat16(e, "cluster", b.clusterTransform);
            }
            b.poseT = b.restT; b.poseR = b.restR; b.poseS = b.restS;
            a->bones.push_back(b);
        }
        // jerarquia SANEADA (indices fuera de rango / ciclos -> raiz): los recorridos que suben
        // por 'parent' asumen que la cadena termina (ver SanearPadres)
        { std::vector<int> pad(a->bones.size());
          for (size_t i = 0; i < a->bones.size(); i++) pad[i] = a->bones[i].parent;
          if (SanearPadres(pad, a->name.c_str()))
              for (size_t i = 0; i < a->bones.size(); i++) a->bones[i].parent = pad[i]; }
        JVal* ja = JHijo(j, "anims", 5);
        if (ja) for (size_t i = 0; i < ja->lista.size(); i++) {
            JVal* e = ja->lista[i]; if (!e || e->tipo != 4) continue;
            SkeletalAnimation* an = new SkeletalAnimation(JS(e, "nombre", "Animation"));
            an->FrameRate  = JI(e, "fps", 24);
            an->startFrame = JI(e, "inicio", 1);
            an->endFrame   = JI(e, "fin", 0);
            JVal* jt = JHijo(e, "tracks", 5);
            if (jt) for (size_t t = 0; t < jt->lista.size(); t++) {
                JVal* tj = jt->lista[t]; if (!tj || tj->tipo != 4) continue;
                BoneTrack tr;
                tr.bone = JI(tj, "hueso", -1);
                JVal* jc = JHijo(tj, "curvas", 5);
                if (jc) for (size_t c = 0; c < jc->lista.size(); c++) {
                    AnimProperty ap;
                    if (CargarCurvaJson(jc->lista[c], ap)) tr.Propertys.push_back(ap);
                }
                an->tracks.push_back(tr);
            }
            a->animations.push_back(an);
        }
        a->animActiva = JI(j, "animActiva", a->animations.empty() ? -1 : 0);
        if (a->animActiva < -1 || a->animActiva >= (int)a->animations.size())
            a->animActiva = a->animations.empty() ? -1 : 0;
        if (autorado) PrepararSkinAutorado(a);          // rig del editor: rest/skin desde head/tail
        else { bool hayRest = false;
               for (size_t i = 0; i < a->bones.size(); i++) if (a->bones[i].hasRest) { hayRest = true; break; }
               if (hayRest) PrepararSkin(a); }          // rig importado: recomputar matrices de skinning
        return a;
    }
    if (tipo == "lod") {   // LOD: un hijo por distancia a la camara (los hijos los lee JsonObjeto)
        LOD* l = new LOD(parent);
        JsonComunes(j, l);
        JVal* dl = JHijo(j, "distancias", 5);   // lista de numeros (mismo idioma que "color")
        if (dl) for (size_t i = 0; i < dl->lista.size(); i++)
            if (dl->lista[i]) l->distancias.push_back((float)dl->lista[i]->num);
        l->soloCamaraActiva = JB(j, "soloCamaraActiva", false); // ausente en los .w3d viejos
        return l;
    }
    if (tipo == "culling") {   // Culling: frustum culling por AABB de los hijos
        Culling* cu = new Culling(parent);
        JsonComunes(j, cu);
        cu->activo           = JB(j, "activo", true);         // ausente = prendido
        cu->soloCamaraActiva = JB(j, "soloCamaraActiva", false);
        cu->distanciaMax     = JF(j, "distanciaMax", 0.0f);   // 0 = sin limite (archivos viejos)
        return cu;
    }
    if (tipo == "viszona") {   // VisZona: celda de visibilidad (ver VisZona.h)
        VisZona* vz = new VisZona(parent);
        JsonComunes(j, vz);
        std::string modo = JS(j, "modo", "manual");
        vz->modo = (modo == "grilla") ? VisZona::ModoGrilla
                 : (modo == "volumenes") ? VisZona::ModoVolumenes
                 : (modo == "curva") ? VisZona::ModoCurva : VisZona::ModoManual;
        vz->objetivoNombre = JS(j, "objetivo", "");
        vz->anclaNombre    = JS(j, "ancla", "");
        vz->nx       = JI(j, "nx", 1);
        vz->ny       = JI(j, "ny", 1);
        vz->nz       = JI(j, "nz", 1);
        vz->tamCelda = JF(j, "tamCelda", 1.0f);
        vz->pasoMax  = JI(j, "pasoMax", 32);
        // multi-riel (ver VisZona.h): riel que indexa las celdas + celda fallback
        vz->rielNombre    = JS(j, "riel", "");
        vz->celdaFallback = JI(j, "fallback", 0);
        return vz;
    }
    if (tipo == "particulas") {   // Particulas: emisor de billboards (ver Particulas.h)
        Particulas* pt = new Particulas(parent);
        JsonComunes(j, pt);
        pt->textura    = JS(j, "textura", "");   // entrada del contenedor v4 (o ruta externa)
        pt->cantidad   = JF(j, "cantidad",   pt->cantidad);
        pt->vida       = JF(j, "vida",       pt->vida);
        pt->tam        = JF(j, "tam",        pt->tam);
        pt->vel        = JF(j, "vel",        pt->vel);
        pt->dispersion = JF(j, "dispersion", pt->dispersion);
        pt->gravedad   = JF(j, "gravedad",   pt->gravedad);
        pt->aditivo    = JB(j, "aditivo",    pt->aditivo);
        pt->sustractivo= JB(j, "sustractivo", pt->sustractivo);   // ausente = false (archivos viejos)
        JVal* c = JHijo(j, "color", 5);   // [r, g, b, a] (mismo idioma que "ejes")
        if (c) for (size_t i = 0; i < c->lista.size() && i < 4; i++)
            if (c->lista[i]) pt->color[i] = (float)c->lista[i]->num;
        pt->desvanecer = JB(j, "desvanecer", pt->desvanecer);
        pt->activo     = JB(j, "activo",     pt->activo);
        // azar (ausente en archivos v4 viejos -> 0: mismo comportamiento de antes)
        pt->variacion   = JF(j, "variacion",   pt->variacion);
        pt->turbulencia = JF(j, "turbulencia", pt->turbulencia);
        // rotacion del billboard (ausente -> false/0: derecho y quieto, como antes)
        pt->rotacion    = JB(j, "rotacion",    pt->rotacion);
        pt->velRotacion = JF(j, "velRotacion", pt->velRotacion);
        return pt;
    }
    if (tipo == "objeto") {
        // el nodo GENERICO que escribe el 'else' de EscribirObjeto (Empty y cualquier tipo
        // sin rama propia): antes NO tenia rama de lectura, asi que el objeto se perdia y
        // sus hijos se re-colgaban del abuelo. Vuelve como Empty (el objeto vacio del editor,
        // que es lo que el usuario puede crear) con su nombre y su transform.
        Empty* o = new Empty(parent);
        JsonComunes(j, o);
        return o;
    }
    return NULL;   // tipo desconocido: sus hijos cuelgan del padre (nunca se pierde el arbol)
}

// ---------------------------------------------------------------------------
//  "padre2d": el nodo NO cuelga del UI, cuelga de uno de sus ELEMENTOS 2D (fallo B).
//
//  El .w3d escribe estos objetos como hijos del nodo UI porque el .w3dui no los sabe
//  llevar (solo escribe tipos 2D) y el .w3d no recorre el subarbol 2D; ver el bloque de
//  JuntarHuespedesUI en GuardarW3D.cpp. Al llegar aca el .w3dui ya se cargo, o sea que el
//  elemento existe y se lo busca POR NOMBRE dentro de la escena (el espacio de nombres es
//  por escena: no hay dos elementos homonimos adentro de una UI).
//  Si el elemento no aparece -un .w3dui editado a mano, o una escena que no cargo- el
//  objeto se QUEDA colgado del UI: pierde su lugar exacto, pero NO se pierde (que es lo
//  que pasaba antes) y queda avisado en el log.
// ---------------------------------------------------------------------------
static Object* BuscarElemento2D(Object* raiz, const std::string& nombre) {
    if (!raiz) return NULL;
    for (size_t i = 0; i < raiz->Childrens.size(); i++) {
        Object* h = raiz->Childrens[i];
        if (h->name == nombre) return h;
        Object* r = BuscarElemento2D(h, nombre);
        if (r) return r;
    }
    return NULL;
}
static void ReColgarDePadre2D(Object* o, Object* ui, const std::string& nombre) {
    if (!o || !ui || nombre.empty()) return;
    Object* dest = BuscarElemento2D(ui, nombre);
    if (!dest || dest == o) {
        w3dLogfW("[W3D] '%s' referencia el elemento 2D '%s', que no esta en la escena: queda colgado del UI",
                 o->name.c_str(), nombre.c_str());
        return;
    }
    for (Object* q = dest; q; q = q->Parent) if (q == o) return;   // ciclo: no tocar
    Object* viejo = o->Parent ? o->Parent : SceneCollection;
    if (viejo) for (size_t k = 0; k < viejo->Childrens.size(); k++)
        if (viejo->Childrens[k] == o) { viejo->Childrens.erase(viejo->Childrens.begin() + k); break; }
    o->Parent = dest;
    dest->Childrens.push_back(o);
}

// ---------------------------------------------------------------------------
//  EL STACK DE CONSTRAINTS (simetrico con EscribirConstraints del guardado).
//
//  Un solo call site, para CUALQUIER tipo de objeto: los constraints son una
//  propiedad de Object y no de una rama (una Coleccion tambien puede tener).
//
//  El 'tipo' llega como STRING: uno que esta version no conoce SE SALTEA con log y
//  el objeto abre igual. Perder un constraint es perder un efecto; abortar el nodo
//  seria perder el objeto y su subarbol, que es incomparablemente peor.
//
//  La FUENTE queda por NOMBRE nomas: el puntero lo resuelve
//  W3dConstraintsResolverNombres al final de la carga, cuando ya existen todos los
//  objetos (aca todavia no existe el que viene despues en el archivo).
// ---------------------------------------------------------------------------
static int TipoConsInt(const std::string& t) {
    if (t == "copyLocation") return W3dConstraintTipo::CopyLocation;
    if (t == "copyRotation") return W3dConstraintTipo::CopyRotation;
    if (t == "billboard")    return W3dConstraintTipo::Billboard;
    return -1;   // desconocido (un .w3d de una version mas nueva)
}

static void LeerConstraints(JVal* j, Object* o) {
    JVal* jc = JHijo(j, "constraints", 5);
    if (!jc || !o) return;
    for (size_t i = 0; i < jc->lista.size(); i++) {
        JVal* e = jc->lista[i]; if (!e || e->tipo != 4) continue;
        const std::string ts = JS(e, "tipo", "");
        const int tipo = TipoConsInt(ts);
        if (tipo < 0) {
            w3dLogfW("[W3D] '%s': constraint de tipo desconocido '%s': se ignora (el objeto abre igual)",
                     o->name.c_str(), ts.c_str());
            continue;
        }
        W3dConstraint* c = new W3dConstraint(tipo);
        c->nombre     = JS(e, "nombre", W3dNombreTipoConstraint(tipo));
        c->activo     = JB(e, "activo", true);
        c->mostrarEdit = JB(e, "vered", false);   // ausente = OFF (el default; ver W3dConstraint.h)
        c->influencia = JF(e, "influencia", c->influencia);
        if (tipo == W3dConstraintTipo::Billboard) {
            c->bbYaw   = JB(e, "yaw",   c->bbYaw);
            c->bbPitch = JB(e, "pitch", c->bbPitch);
        } else {
            // "fuente" es el TIPO de fuente y es explicito: cualquier cosa que no sea
            // "vista" es un objeto (que es el default y el caso del 99%).
            c->fuenteTipo = (JS(e, "fuente", "objeto") == "vista")
                                ? W3dConstraintFuente::Vista : W3dConstraintFuente::Objeto;
            if (c->fuenteTipo == W3dConstraintFuente::Objeto)
                c->fuenteNombre = JS(e, "objeto", "");
            JBools3(e, "ejes", &c->ejeX, &c->ejeY, &c->ejeZ);
        }
        // el archivo lo pudo editar cualquiera: clampea la influencia a [0,100] y arregla
        // el billboard que no gira en ningun eje (ver W3dConstraint::Normalizar)
        c->Normalizar();
        o->constraints.push_back(c);
    }
    if (!o->constraints.empty()) o->constraintActivo = (int)o->constraints.size() - 1;
}

// UN nodo del .w3d: el objeto + SUS HIJOS. La lectura de "hijos" esta ACA y en
// ningun otro lado (simetrico con EscribirHijos del guardado).
static void JsonObjeto(JVal* j, Object* parent, const std::string& base) {
    if (!j || j->tipo != 4) return;
    Object* creado = JsonObjetoCrear(j, parent, base);
    // el stack de constraints del nodo: DESPUES de crearlo (es donde viven) y ANTES de
    // bajar a los hijos, para que el orden del archivo sea el orden de la lista.
    if (creado) LeerConstraints(j, creado);
    // los SCRIPTS lua, de TODOS los tipos, en UN solo call site (espejo del
    // EscribirScripts comun de GuardarW3D). Antes solo gamepad/malla/modelo/glb
    // los leian: un script en una luz o un vacio se perdia al abrir.
    if (creado) JsonScripts(j, creado, base);
    const std::string padre2d = JS(j, "padre2d", "");
    if (creado && !padre2d.empty() && parent && parent->getType() == ObjectType::ui)
        ReColgarDePadre2D(creado, parent, padre2d);
    JVal* h = JHijo(j, "hijos", 5);
    if (!h) return;
    Object* p = creado ? creado : parent;
    for (size_t i = 0; i < h->lista.size(); i++) JsonObjeto(h->lista[i], p, base);
}

// CONFIG de la tarjeta Juego (bloque raiz "compilar", opcional): pisa los
// defaults ya reseteados en AbrirW3D. Strings legibles ("ventana",
// "pantallaCompleta", ...); un valor desconocido o ausente deja el default del
// campo (W3dCompilar*Int). Solo existe en el camino JSON: los formatos viejos
// (zip v2 sin el campo, texto Whisk3D{}) quedan con los defaults.
static void AplicarCompilar(JVal* jc) {
    if (!jc) return;
#ifdef W3D_SIN_EDITOR
    // la tarjeta "Juego" es del editor: en el juego COMPILADO estas opciones ya
    // estan horneadas en el binario (las aplico "Compilar juego" al generarlo).
    (void)jc;
#else
    g_proyCompilar.modoVentana = W3dCompilarModoVentanaInt(JS(jc, "modoVentana", ""));
    g_proyCompilar.orientacion = W3dCompilarOrientacionInt(JS(jc, "orientacion", ""));
    g_proyCompilar.usarFisica  = JB(jc, "fisica", true);
    g_proyCompilar.usarSonido  = JB(jc, "sonido", true);
    g_proyCompilar.modoDebug   = JB(jc, "debug", false);
    g_proyCompilar.assetsModo  = W3dCompilarAssetsInt(JS(jc, "assets", ""));
    g_proyCompilar.plataforma  = W3dCompilarPlataformaInt(JS(jc, "plataforma", ""));
    g_proyCompilar.uid         = (unsigned)strtoul(JS(jc, "uid", "0").c_str(), 0, 16);  // UID3 Symbian (hex string)
#endif
}

// ===========================================================================
//  MATERIALES (bloque raiz "materiales")
//
//  Antes viajaban ADENTRO del GLB de cada malla y volvian por ImportGLTF. Desde que la geometria
//  es .w3dm, el material vive en el proyecto.json y cada .w3dm lo referencia POR NOMBRE (bloque
//  PARTMAT). Se cargan ANTES que los objetos para que la malla ya los encuentre.
//
//  REUSO POR NOMBRE, pero SOLO si es REALMENTE el mismo material: ReiniciarEscena borra la escena
//  pero NO el registro global de materiales, asi que sin esto reabrir el proyecto multiplicaria
//  los materiales (Material.001, .002, ...). Si el homonimo tiene OTRO contenido, el constructor
//  uniquifica y se avisa (mismo criterio que ImportGLTF).
// ===========================================================================
static std::map<std::string, Material*> gMatDeNombre;   // nombre EN EL ARCHIVO -> material vivo

static bool W3dMatCasi(float a, float b) { float d = a - b; if (d < 0) d = -d; return d <= 1e-4f; }
static void JColor4(JVal* j, const char* k, float* out) {
    JVal* v = JHijo(j, k, 5);
    if (v && v->lista.size() >= 4) for (int i = 0; i < 4; i++) out[i] = (float)v->lista[i]->num;
}
// la ruta de la textura de un material YA existente: la cargada, o la que tiene ENCOLADA (la
// carga es diferida: recien cargado, mat->texture sigue NULL).
static std::string W3dMatRutaTex(const Material* m) {
    if (m->texture && !m->texture->path.empty()) return m->texture->path;
    return TexturaPendienteDe(m);
}
static bool W3dMismoMaterial(const Material* v, const Material& n, const std::string& texPath) {
    for (int k = 0; k < 4; k++) {
        if (!W3dMatCasi(v->diffuse[k],  n.diffuse[k]))  return false;
        if (!W3dMatCasi(v->specular[k], n.specular[k])) return false;
        if (!W3dMatCasi(v->emission[k], n.emission[k])) return false;
        if (!W3dMatCasi(v->ambient[k],  n.ambient[k]))  return false;
    }
    if (!W3dMatCasi(v->shininess, n.shininess)) return false;
    if (v->interpolacion != n.interpolacion) return false;
    if (v->reflectMode   != n.reflectMode)   return false;
    if (v->textureOn != n.textureOn || v->filtrado != n.filtrado || v->repeat != n.repeat ||
        v->transparent != n.transparent || v->lighting != n.lighting || v->vertexColor != n.vertexColor ||
        v->culling != n.culling || v->depth_test != n.depth_test || v->chrome != n.chrome ||
        v->normalMap != n.normalMap || v->uv8bit != n.uv8bit) return false;
    if (v->depth_write != n.depth_write || v->orden_pasada != n.orden_pasada ||
        v->mezcla != n.mezcla || !W3dMatCasi(v->depth_bias, n.depth_bias)) return false;
    if (v->lineas != n.lineas || !W3dMatCasi(v->grosorLinea, n.grosorLinea)) return false;
    if (W3dMatRutaTex(v) != texPath) return false;
    return true;
}
static void W3dCopiarMaterial(const Material& src, Material* dst) {
    for (int k = 0; k < 4; k++) {
        dst->diffuse[k]  = src.diffuse[k];  dst->specular[k] = src.specular[k];
        dst->emission[k] = src.emission[k]; dst->ambient[k]  = src.ambient[k];
    }
    dst->shininess = src.shininess; dst->interpolacion = src.interpolacion;
    dst->reflectMode = src.reflectMode;
    dst->textureOn = src.textureOn; dst->filtrado = src.filtrado; dst->repeat = src.repeat;
    dst->transparent = src.transparent; dst->lighting = src.lighting; dst->vertexColor = src.vertexColor;
    dst->culling = src.culling; dst->depth_test = src.depth_test; dst->chrome = src.chrome;
    dst->normalMap = src.normalMap; dst->uv8bit = src.uv8bit;
    dst->depth_write = src.depth_write; dst->depth_bias = src.depth_bias;
    dst->orden_pasada = src.orden_pasada; dst->mezcla = src.mezcla;
    dst->lineas = src.lineas; dst->grosorLinea = src.grosorLinea;
}
// una textura por RUTA, reusando la que ya este cargada con esa misma ruta (el normal map y las
// capas extra son pocas y las quiere el primer frame: van SINCRONICAS, no por la cola diferida).
static Texture* W3dTexturaDeRuta(const std::string& path) {
    // ANTES: barrido lineal del vector global + `new Texture` a mano, y solo lo
    // usaban los normal maps y las capas. Ahora es EL MISMO cache por ruta con
    // refcount que usa la cola diferida (Textures.h): una sola tabla, un solo
    // dueno, y lo que se toma aca tambien se libera al cerrar el proyecto.
    Texture* t = TexturaTomar(path);
    if (!t && !path.empty()) w3dLogfW("[W3D] no pude cargar la textura '%s'", path.c_str());
    return t;
}

static void CargarMateriales(JVal* raiz, const std::string& base) {
    gMatDeNombre.clear();
    JVal* jm = JHijo(raiz, "materiales", 5);
    if (!jm) return;
    for (size_t i = 0; i < jm->lista.size(); i++) {
        JVal* e = jm->lista[i]; if (!e || e->tipo != 4) continue;
        const std::string nom = JS(e, "nombre", "Material");
        // temporal SIN registrar (el flag "material por defecto" saltea el alta global y el
        // uniquificado del constructor): asi se puede DECIDIR si crear uno o reusar el homonimo.
        Material tmp(nom, true);
        JColor4(e, "difuso",    tmp.diffuse);
        JColor4(e, "especular", tmp.specular);
        JColor4(e, "emision",   tmp.emission);
        JColor4(e, "ambiente",  tmp.ambient);
        tmp.shininess     = JF(e, "brillo", tmp.shininess);
        tmp.interpolacion = JI(e, "interpolacion", tmp.interpolacion);
        tmp.reflectMode   = JI(e, "reflejoModo", tmp.reflectMode);
        // DECAL / mezcla. Ausentes en TODO lo guardado hasta hoy -> el default deja el material igual.
        tmp.depth_bias    = JF(e, "sesgoProfundidad", tmp.depth_bias);
        tmp.orden_pasada  = JI(e, "ordenPasada",      tmp.orden_pasada);
        tmp.mezcla        = JI(e, "mezcla",           tmp.mezcla);
        // LINEAS (aristas por material). Ausentes en lo guardado hasta hoy -> default apagado.
        tmp.lineas        = JB(e, "lineas",           tmp.lineas);
        tmp.grosorLinea   = JF(e, "grosorLinea",      tmp.grosorLinea);
        if (tmp.grosorLinea < 1.0f) tmp.grosorLinea = 1.0f;
        if (tmp.orden_pasada < 0) tmp.orden_pasada = 0;
        if (tmp.orden_pasada > 2) tmp.orden_pasada = 2;
        if (tmp.mezcla < 0 || tmp.mezcla >= (int)w3dEngine::MezclaCount_) tmp.mezcla = 0;
        JVal* fl = JHijo(e, "flags", 4);
        if (fl) {
            tmp.depth_write = JB(fl, "escribeProfundidad", tmp.depth_write);
            tmp.textureOn   = JB(fl, "textura",      tmp.textureOn);
            tmp.filtrado    = JB(fl, "filtrado",     tmp.filtrado);
            tmp.repeat      = JB(fl, "repetir",      tmp.repeat);
            tmp.transparent = JB(fl, "transparente", tmp.transparent);
            tmp.lighting    = JB(fl, "luz",          tmp.lighting);
            tmp.vertexColor = JB(fl, "colorVertice", tmp.vertexColor);
            tmp.culling     = JB(fl, "culling",      tmp.culling);
            tmp.depth_test  = JB(fl, "profundidad",  tmp.depth_test);
            tmp.chrome      = JB(fl, "reflejo",      tmp.chrome);
            tmp.normalMap   = JB(fl, "normalMap",    tmp.normalMap);
            tmp.uv8bit      = JB(fl, "uv8bit",       tmp.uv8bit);
        }
        std::string texPath = JS(e, "textura", "");
        if (!texPath.empty()) texPath = RutaJson(texPath, base);
        Material* viejo = BuscarMaterialPorNombre(nom);
        if (viejo && W3dMismoMaterial(viejo, tmp, texPath)) { gMatDeNombre[nom] = viejo; continue; }
        Material* mat = new Material(nom);   // el ctor uniquifica: homonimo DISTINTO -> "X.001"
        W3dCopiarMaterial(tmp, mat);
        if (!texPath.empty()) EncolarTextura(mat, texPath);   // carga DIFERIDA (1 por frame)
        std::string nrm = JS(e, "normalTextura", "");
        if (!nrm.empty()) mat->normalTexture = W3dTexturaDeRuta(RutaJson(nrm, base));
        JVal* jc = JHijo(e, "capas", 5);
        if (jc) for (size_t c = 0; c < jc->lista.size(); c++) {
            JVal* ce = jc->lista[c]; if (!ce || ce->tipo != 4) continue;
            Texture* t = W3dTexturaDeRuta(RutaJson(JS(ce, "textura", ""), base));
            if (!t) continue;
            TexLayer tl; tl.tex = t; tl.blend = JI(ce, "mezcla", 0); tl.on = JB(ce, "on", true);
            mat->capas.push_back(tl);
        }
        if (viejo) w3dLogfW("[W3D] ya habia un material '%s' con OTRO contenido -> se creo '%s'",
                            nom.c_str(), mat->name.c_str());
        gMatDeNombre[nom] = mat;
    }
}

// el material que le toca a un mesh part por el NOMBRE que trae el .w3dm ("" = ninguno).
// Cae a BuscarMaterialPorNombre para los .w3d escritos a mano y para los nombres que ya
// existian en la sesion.
static Material* MaterialPorNombre(const std::string& nom) {
    if (nom.empty()) return NULL;
    std::map<std::string, Material*>::iterator it = gMatDeNombre.find(nom);
    if (it != gMatDeNombre.end()) return it->second;
    return BuscarMaterialPorNombre(nom);
}

// ===========================================================================
//  ANIMACIONES DE ESCENA (bloque raiz "animaciones") -> SceneAnimations
//
//  La contraparte de EscribirAnimacionesEscena (io/GuardarW3D.cpp), donde esta
//  el comentario largo de por que existe y por que vive en el proyecto.json.
//  RETROCOMPAT: un .w3d SIN el bloque (o sea todos los guardados hasta hoy, que
//  es el bug que reporto el dueno) abre igual, con la escena "Scene" default.
//
//  TODA CANTIDAD LEIDA DEL ARCHIVO VA ACOTADA. El parser ya limito el tamano al
//  del archivo, pero las cotas de aca son las que evitan que un .w3d hecho a
//  mano (el formato se vende como editable) meta cien mil escenas de animacion
//  y deje el editor inusable sin un solo aviso.
// ===========================================================================
enum { kAnimMaxEscenas = 1024, kAnimMaxObjetos = 100000, kAnimMaxCurvas = 4096 };

// BUSQUEDA POR NOMBRE RESPETANDO EL SCOPE. Los nombres son unicos POR ESCENA
// (W3dNombreScopeDe), asi que buscar en todo el arbol podria devolver el widget
// homonimo de una UI en vez del objeto de la escena. La particion es LA MISMA
// que la de IndexarScopeGlobal/IndexarScopeUI (Objects.cpp): en el scope global
// cuenta todo lo que no esta dentro de ninguna UI, mas las RAICES UI.
static Object* AnimBuscarScopeGlobal(Object* nodo, const std::string& n, bool dentroDeUI) {
    if (!nodo) return NULL;
    for (size_t i = 0; i < nodo->Childrens.size(); i++) {
        Object* h = nodo->Childrens[i];
        const bool esUI = (h->getType() == ObjectType::ui);
        if ((esUI || !dentroDeUI) && h->name == n) return h;
        Object* r = AnimBuscarScopeGlobal(h, n, dentroDeUI || esUI);
        if (r) return r;
    }
    return NULL;
}
static Object* AnimBuscarScopeUI(Object* nodo, const std::string& n) {
    if (!nodo) return NULL;
    for (size_t i = 0; i < nodo->Childrens.size(); i++) {
        Object* h = nodo->Childrens[i];
        if (h->getType() == ObjectType::ui) continue;   // otro scope (el suyo)
        if (h->name == n) return h;
        Object* r = AnimBuscarScopeUI(h, n);
        if (r) return r;
    }
    return NULL;
}

static void CargarAnimacionesEscena(JVal* raiz) {
    JVal* ja = JHijo(raiz, "animaciones", 4);
    if (!ja) { InitSceneAnimations(); return; }   // .w3d viejo: sin bloque, abre igual
    JVal* jes = JHijo(ja, "escenas", 5);
    if (!jes || jes->lista.empty()) { InitSceneAnimations(); return; }
    // se reemplaza la lista entera (ReiniciarEscena dejo la "Scene" vacia de arranque)
    for (size_t i = 0; i < SceneAnimations.size(); i++) delete SceneAnimations[i];
    SceneAnimations.clear();
    AnimationObjects.clear();
    SceneAnimActiva = 0;

    size_t nEsc = jes->lista.size();
    if (nEsc > (size_t)kAnimMaxEscenas) {
        w3dLogfW("[W3D] el proyecto declara %d animaciones de escena: se cargan las primeras %d",
                 (int)nEsc, (int)kAnimMaxEscenas);
        nEsc = (size_t)kAnimMaxEscenas;
    }
    int perdidos = 0;             // curvas cuyo objeto no aparecio
    std::string primerPerdido;
    for (size_t i = 0; i < nEsc; i++) {
        JVal* je = jes->lista[i];
        if (!je || je->tipo != 4) continue;
        SceneAnimation* esc = new SceneAnimation(JS(je, "nombre", "Scene"));
        esc->startFrame = JI(je, "inicio", 1);
        esc->endFrame   = JI(je, "fin", 250);
        esc->fps        = JI(je, "fps", 30);
        // rango/fps SANEADOS: un archivo a mano puede traer fin < inicio o fps 0 y eso
        // deja el timeline inutilizable (y AnimTick en un loop de un solo frame)
        if (esc->startFrame < 0) esc->startFrame = 0;
        if (esc->endFrame < esc->startFrame) esc->endFrame = esc->startFrame;
        if (esc->fps < 1) esc->fps = 1;
        if (esc->fps > 120) esc->fps = 120;
        SceneAnimations.push_back(esc);

        JVal* jobjs = JHijo(je, "objetos", 5);
        if (!jobjs) continue;
        size_t nObj = jobjs->lista.size();
        if (nObj > (size_t)kAnimMaxObjetos) {
            w3dLogfW("[W3D] la animacion '%s' declara %d objetos: se cargan los primeros %d",
                     esc->name.c_str(), (int)nObj, (int)kAnimMaxObjetos);
            nObj = (size_t)kAnimMaxObjetos;
        }
        for (size_t o = 0; o < nObj; o++) {
            JVal* jo = jobjs->lista[o];
            if (!jo || jo->tipo != 4) continue;
            const std::string nombre = JS(jo, "objeto", "");
            if (nombre.empty()) continue;
            // SCOPE: "escena" = la UI adentro de la cual vive el objeto (ausente = global)
            const std::string scope = JS(jo, "escena", "");
            Object* obj = NULL;
            if (scope.empty()) {
                obj = AnimBuscarScopeGlobal(SceneCollection, nombre, false);
            } else {
                Object* ui = AnimBuscarScopeGlobal(SceneCollection, scope, false);
                if (ui && ui->getType() == ObjectType::ui) obj = AnimBuscarScopeUI(ui, nombre);
            }
            if (!obj) {
                // EL OBJETO NO ESTA (lo borraron o lo renombraron despues de guardar). La curva
                // NO se cuelga de cualquier lado: se descarta y se AVISA. Colgarla del primer
                // homonimo que apareciera seria animar un objeto que el usuario no eligio.
                if (perdidos == 0) primerPerdido = scope.empty() ? nombre : (scope + "/" + nombre);
                perdidos++;
                w3dLogfW("[W3D] la animacion '%s' referencia al objeto '%s'%s%s, que ya no esta: sus curvas se descartan",
                         esc->name.c_str(), nombre.c_str(),
                         scope.empty() ? "" : " de la escena ", scope.c_str());
                continue;
            }
            AnimationObject ao;
            ao.obj = obj; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
            JVal* jcur = JHijo(jo, "curvas", 5);
            if (jcur) {
                size_t nCur = jcur->lista.size();
                if (nCur > (size_t)kAnimMaxCurvas) {
                    w3dLogfW("[W3D] '%s' declara %d curvas: se cargan las primeras %d",
                             nombre.c_str(), (int)nCur, (int)kAnimMaxCurvas);
                    nCur = (size_t)kAnimMaxCurvas;
                }
                for (size_t c = 0; c < nCur; c++) {
                    AnimProperty ap;
                    if (!CargarCurvaJson(jcur->lista[c], ap)) continue;
                    if (ap.keyframes.empty()) continue;   // curva sin keys: no es dato
                    ap.SortKeyFrames();                    // el archivo puede venir desordenado
                    ao.Propertys.push_back(ap);
                }
            }
            if (ao.Propertys.empty()) continue;
            ao.UpdateFirstLastFrame();
            SceneAnimations.back()->objetos.push_back(ao);
        }
    }
    if (SceneAnimations.empty()) { InitSceneAnimations(); return; }
    // LA ACTIVA: sus curvas NO viven en su SceneAnimation sino en el global
    // AnimationObjects (ver el invariante del swap en Animation.h). Se hace el
    // mismo swap que SetEscenaActiva, o el proyecto abriria con la escena que el
    // usuario estaba viendo aparentemente vacia.
    int activa = JI(ja, "activa", 0);
    if (activa < 0 || activa >= (int)SceneAnimations.size()) activa = 0;
    SceneAnimActiva = activa;
    AnimationObjects.swap(SceneAnimations[activa]->objetos);
    // rango + fps PROPIOS de la animacion activa -> StartFrame/EndFrame/AnimFPS
    AnimCargarRangoActivo();
    if (perdidos > 0)
        W3dAvisof(true, "%d curva(s) de animacion quedaron sin objeto (la primera: '%s'): ver whisk3d.log",
                  perdidos, W3dNombreCorto(primerPerdido).c_str());
}

// abre el proyecto desde el TEXTO json 'datos' (el .w3d plano entero, o el
// escena.json de un zip v2 viejo), con las rutas relativas resueltas contra
// 'base'. false + aviso si el json no parsea (el editor sigue vivo igual).
static bool AbrirEscenaJson(const char* datos, size_t n, const std::string& base) {
    JParser parser(datos, n);
    JVal* raiz = parser.Valor();
    if (parser.error || raiz->tipo != 4) {
        w3dLogfE("[W3D] el JSON del proyecto no parsea: %s (se abre vacio)", w3dPath.c_str());
        delete raiz;
        return false;
    }
    // fps: canonico en la raiz (v3); el zip v2 lo traia dentro de "escena"
    AplicarFps(JI(raiz, "fps", 0));
    // PALETAS del proyecto (v3), ANTES de los objetos: asi las paletas del
    // .w3d ganan el merge contra las bakeadas en los .w3dui (primera gana).
    // Un .w3d viejo sin el campo: se adoptan las de las UIs al cargarlas.
    {
        JVal* jps = JHijo(raiz, "paletas", 5);
        if (jps && !jps->lista.empty()) {
            std::vector<Paleta> ps;
            for (size_t i = 0; i < jps->lista.size() && i < 8; i++) {
                JVal* jp = jps->lista[i];
                if (!jp || jp->tipo != 4) continue;
                Paleta pa;
                pa.nombre = JS(jp, "nombre", "Paleta");
                JVal* jc = JHijo(jp, "colores", 5);
                if (jc)
                    for (size_t c = 0; c < jc->lista.size() && c < 32; c++) {
                        JVal* e = jc->lista[c];
                        if (!e || e->tipo != 4) continue;
                        PaletaColor pc;
                        pc.nombre = JS(e, "nombre", "Color");
                        pc.rgba[0] = pc.rgba[1] = pc.rgba[2] = pc.rgba[3] = 1.0f;
                        JColor(e, "color", pc.rgba);
                        pa.colores.push_back(pc);
                    }
                ps.push_back(pa);
            }
            W3dPaletasAdoptar(ps);
        }
    }
    // MATERIALES del proyecto, ANTES de los objetos: cada .w3dm los referencia por nombre.
    // Un .w3d viejo (mallas en GLB) no trae el bloque y sus materiales siguen viniendo de
    // adentro del .glb, como siempre.
    CargarMateriales(raiz, base);
    JVal* esc = JHijo(raiz, "escena", 4);
    if (esc) {
        AplicarFps(JI(esc, "fps", 0));
        JVal* objs = JHijo(esc, "objetos", 5);
        if (objs) {
            extern void ProgresoActualizar(float);
            for (size_t i = 0; i < objs->lista.size(); i++) {
                // construir los objetos = 30%->95% de la barra (los imports de modelo de
                // adentro muestran ademas su propia barra). Throttle interno a ~1.5%.
                ProgresoActualizar(0.30f + 0.65f * (float)i / (float)objs->lista.size());
                JsonObjeto(objs->lista[i], SceneCollection, base);
            }
        }
    } else {
        w3dLogfW("[W3D] el proyecto no trae \"escena\": abre sin objetos");
    }
    // ya existen TODOS los objetos: resolver las referencias modArmature (malla -> armature por nombre)
    ResolverModArmaturePendientes();
    // ...y las CURVAS DE ANIMACION de los objetos, que tambien los referencian por nombre
    CargarAnimacionesEscena(raiz);
    // "fullscreen" solo si esta presente (no pisar la config del editor con un default)
    if (JHijo(raiz, "fullscreen", 3)) AplicarFullscreen(JB(raiz, "fullscreen", false));
    // estado de reproduccion al GUARDAR (v3, opcional): guardado en pausa -> abre en
    // pausa. Ausente queda -1 y el pie de AbrirW3D hace el auto-play de siempre.
    if (JHijo(raiz, "reproduciendo", 3))
        g_proyReproduciendo = JB(raiz, "reproduciendo", true) ? 1 : 0;
    // MULTI-ESCENA: escena inicial + modo (opcionales). Si el .w3d trae varios nodos "ui",
    // esta es la que arranca; el resto quedan cargadas y se activan con cambiarEscena().
    // Ausentes: escenaInicial "" (arranca la primera) y modo apagado (retrocompat 1 escena).
    AplicarEscenaInicial(JS(raiz, "escenaInicial", ""));
    AplicarModoEscenas(JB(raiz, "modoEscenas", false));
    // PIXELADO GLOBAL (ver la rama de texto arriba y w3dGraphics.h). Ausente =
    // false: nada de lo que ya andaba cambia de aspecto.
    w3dEngine::SetPixeladoGlobal(JB(raiz, "pixelado", false));
    // icono del juego (opcional): ruta EXTERNA relativa al .w3d. La usa la tarjeta
    // Juego y Compilar juego (genera los tamanos chicos al compilar).
    AplicarIcono(RutaJson(JS(raiz, "icono", ""), base));
    // config de la tarjeta Juego (opcional): modo ventana, orientacion, flags,
    // assets y plataforma del juego COMPILADO. Ausente = defaults del editor.
    AplicarCompilar(JHijo(raiz, "compilar", 4));
    // SESION (opcional): frame actual + seleccion. Solo se LEE aca; se aplica en el
    // pie de AbrirW3D, con los nombres ya definitivos (ver g_sesFrame arriba).
    {
        JVal* ses = JHijo(raiz, "sesion", 4);
        if (ses) {
            // el -1 significa "no habia bloque": un frame negativo del archivo se acota a 0 aca
            // y no se disfraza de ausente (la cota de arriba la pone SesionAplicar)
            g_sesFrame = JI(ses, "frame", 0);
            if (g_sesFrame < 0) {
                w3dLogfW("[W3D] sesion: 'frame' vale %d (negativo) -> se acota a 0", g_sesFrame);
                g_sesFrame = 0;
            }
            g_sesActivo = JS(ses, "activo", "");
            JVal* sel = JHijo(ses, "seleccion", 5);
            if (sel)
                for (size_t i = 0; i < sel->lista.size(); i++)
                    if (sel->lista[i] && sel->lista[i]->tipo == 2)   // 2 = string
                        g_sesSeleccion.push_back(sel->lista[i]->str);
        }
    }
    AplicarLayoutTexto(JS(raiz, "layout", "2d"));
    delete raiz;
    return true;
}

// ---------------------------------------------------------------------------
//  MigrarConstraintsViejos: el segundo tiempo de la migracion (ver PendConsViejo).
//
//  Corre al final de AbrirW3D, con el arbol COMPLETO (por eso es un segundo tiempo:
//  el objeto apuntado puede venir despues en el archivo) y JUSTO ANTES de
//  W3dNombresRepararEscena.
//
//  *** POR QUE ANTES DE LA REPARACION DE NOMBRES (y no despues, como estaba) ***
//  Un nodo constraint viejo puede llamarse IGUAL que un objeto real, y en el formato
//  viejo eso era inofensivo: nadie buscaba a un nodo constraint por nombre y no habia
//  reparacion de duplicados. Hoy el choque si importa. Si la reparacion corre con los
//  nodos todavia en la escena, ve dos "X" y renombra a uno de los dos. Cuando el nodo
//  gana el preorden (viene antes en el archivo), la que se lleva el ".001" es LA MALLA
//  DEL USUARIO -- y el nodo se borra unas lineas mas abajo, asi que ese ".001" queda
//  para siempre en la escena y en el archivo guardado, sin ningun "X" al lado
//  que lo explique, y los objeto("X") de lua dejan de encontrarla. Corriendo esta
//  pasada primero, los nodos que desaparecen ya no estan cuando se cuentan los
//  duplicados y no hay nada que renumerar (lo fija el test 'consmigrachoque').
//
//  Y NO se pierde nada por resolver el target antes: la reparacion NUNCA le cambia
//  el nombre al PRIMER portador (los duplicados posteriores son los que se van a
//  ".001"), que es justo el que devuelve el preorden de la busqueda por nombre. Los
//  nodos viejos CON hijos sobreviven como Empty y entran a la reparacion como
//  cualquier otro objeto, que es lo que corresponde: a partir de aca son objetos
//  comunes de la escena.
//
//  Ademas, borrar los nodos ANTES del ReloadAll deja de ser una carrera: un
//  targetName que apuntaba a un nodo viejo se resuelve contra un arbol que ya no lo
//  tiene (queda en NULL) en vez de quedar apuntando a memoria liberada.
//
//  No hace falta que sea deshacible: corre al ABRIR, antes de que exista un solo
//  paso de historial. Un boton de "convertir" en caliente pediria un paso atomico
//  aparte (queda fuera de alcance a proposito).
// ---------------------------------------------------------------------------
static bool MigConNombreExiste(const std::string& n, void* ctx) {
    Object* ob = (Object*)ctx;
    for (size_t i = 0; i < ob->constraints.size(); i++)
        if (ob->constraints[i] && ob->constraints[i]->nombre == n) return true;
    return false;
}
// el nodo viejo sale de la escena (solo se llama cuando NO tiene hijos, asi que la
// promocion de hijos de ~Object no tiene nada que mover y no se corre geometria)
static void SoltarNodoViejo(Object* o) {
    if (!o) return;
    Object* pa = o->Parent;
    if (pa)
        for (size_t k = 0; k < pa->Childrens.size(); k++)
            if (pa->Childrens[k] == o) { pa->Childrens.erase(pa->Childrens.begin() + k); break; }
    o->Parent = NULL;
    delete o;
}
// un nodo viejo que VA A DESAPARECER (no tiene hijos) no puede ser el destino de nadie:
// el constraint que se le empujara se iria con el, mudo, unas iteraciones mas abajo. Y
// como el orden del vector es el del archivo, que se pierda o no dependeria de cual de
// los dos nodos viene primero. Los nodos viejos CON hijos NO entran aca: sobreviven como
// Empty, o sea que a partir de esta migracion son objetos comunes y destinos validos.
static bool MigNodoSeVa(const Object* o) {
    for (size_t i = 0; i < gPendConsViejo.size(); i++)
        if (gPendConsViejo[i].nodo == o && o->Childrens.empty()) return true;
    return false;
}
// la MISMA busqueda por nombre que FindObjectByName (preorden, la raiz incluida) pero
// salteando los nodos que se van. Saltea SOLO al nodo, no a su subarbol: un hijo suyo es
// geometria del usuario y puede ser un destino perfectamente valido.
static Object* MigBuscarDestino(Object* nodo, const std::string& nombre) {
    if (!nodo) return NULL;
    if (nodo->name == nombre && !MigNodoSeVa(nodo)) return nodo;
    for (size_t i = 0; i < nodo->Childrens.size(); i++) {
        Object* r = MigBuscarDestino(nodo->Childrens[i], nombre);
        if (r) return r;
    }
    return NULL;
}
static void MigrarConstraintsViejos() {
    if (gPendConsViejo.empty()) return;
    int puestos = 0, sinTarget = 0, conHijos = 0, aOtroNodo = 0;
    // los NOMBRES de los nodos que se van, juntados ANTES de tocar nada. Van por nombre y no
    // por puntero porque se consultan cuando el nodo apuntado ya puede estar borrado: asi el
    // aviso de "apuntaba a otro constraint viejo" sale igual venga el target antes o despues
    // en el archivo (con el puntero, el que venia antes ya no estaba y el aviso mentia).
    std::vector<std::string> seVan;
    for (size_t i = 0; i < gPendConsViejo.size(); i++)
        if (gPendConsViejo[i].nodo && gPendConsViejo[i].nodo->Childrens.empty())
            seVan.push_back(gPendConsViejo[i].nodo->name);
    for (size_t i = 0; i < gPendConsViejo.size(); i++) {
        PendConsViejo& p = gPendConsViejo[i];
        if (!p.nodo) continue;
        const std::string nombreNodo = p.nodo->name;

        Object* dest = (p.target.empty() || !SceneCollection)
                           ? NULL : MigBuscarDestino(SceneCollection, p.target);
        // el nodo NO puede ser su propio destino. Con MigBuscarDestino esto ya no puede pasar
        // por el preorden (un nodo que se va no se devuelve nunca), pero se deja: si el nodo
        // TIENE hijos sobrevive, es un destino valido para los demas... y para si mismo seria
        // un constraint que se escribe encima, o sea lo que el objeto viejo hacia = nada.
        if (dest == p.nodo) dest = NULL;
        if (!dest) {
            // POR QUE no hay destino: si el nombre lo tiene OTRO nodo constraint viejo (de los
            // que se van), el aviso generico -"no esta en la escena"- seria falso: el objeto
            // esta, es que no puede recibir nada. Imposible en un archivo real (el objeto viejo
            // apuntaba a geometria), pero si no se dijera el constraint se perderia MUDO.
            bool otroNodo = false;
            if (!p.target.empty() && p.target != nombreNodo)   // apuntarse a si mismo no es "otro"
                for (size_t k = 0; k < seVan.size(); k++)
                    if (seVan[k] == p.target) { otroNodo = true; break; }
            if (otroNodo) {
                aOtroNodo++;
                w3dLogfW("[W3D] '%s' era un constraint viejo y su target '%s' es OTRO constraint viejo "
                         "que sale de la escena: no hay a quien migrarlo, no se migra nada",
                         nombreNodo.c_str(), p.target.c_str());
                W3dAvisof(true, "'%s' apuntaba a otro constraint ('%s'): no se pudo migrar",
                          W3dNombreCorto(nombreNodo).c_str(), W3dNombreCorto(p.target).c_str());
            } else {
                // el constraint viejo arrancaba con "if (!target) return": SIN target no hacia
                // absolutamente nada. No se inventa un constraint apuntando a la nada; se avisa
                // y listo (el nodo corre la misma suerte que los demas, mas abajo).
                sinTarget++;
                w3dLogfW("[W3D] '%s' era un constraint viejo y su target '%s' no esta en la escena: "
                         "no hacia nada, no se migra nada",
                         nombreNodo.c_str(), p.target.c_str());
            }
        } else {
            // *** LA INVERSION: las props del nodo van SOBRE EL OBJETO APUNTADO ***
            // POSICION PRIMERO y rotacion despues, que es el orden en el que las aplicaba
            // el RenderObject viejo (y el stack se evalua en orden).
            if (p.copyX || p.copyY || p.copyZ) {
                W3dConstraint* c = new W3dConstraint(W3dConstraintTipo::CopyLocation);
                // la fuente vieja era la camara del viewport (viewPosGlobal), no un objeto
                c->fuenteTipo = W3dConstraintFuente::Vista;
                c->ejeX = p.copyX; c->ejeY = p.copyY; c->ejeZ = p.copyZ;
                c->nombre = W3dNombreUnico(c->nombre, "Constraint", MigConNombreExiste, dest);
                dest->constraints.push_back(c);
                puestos++;
            }
            if (p.horizontal || p.pitch) {
                W3dConstraint* c = new W3dConstraint(W3dConstraintTipo::Billboard);
                c->bbYaw = p.horizontal; c->bbPitch = p.pitch;   // las 4 combinaciones migran exacto
                c->Normalizar();
                c->nombre = W3dNombreUnico(c->nombre, "Constraint", MigConNombreExiste, dest);
                dest->constraints.push_back(c);
                puestos++;
            }
            if (!dest->constraints.empty())
                dest->constraintActivo = (int)dest->constraints.size() - 1;
        }

        // ---- que pasa con el NODO viejo ----
        // La transform propia del nodo es dato MUERTO para el efecto (RenderObject nunca la
        // usaba). Lo unico que puede importar son sus HIJOS:
        //  - sin hijos (los 7 nodos reales de los .w3d que lo destaparon): desaparece.
        //  - con hijos: se queda como Empty, con el MISMO nombre y la MISMA pos/rot/escala.
        //    Es la opcion de CERO MATEMATICA. Borrarlo los haria subir por ~Object con
        //    "pos += pos del nodo", que solo conserva la posicion de mundo si el nodo tenia
        //    rotacion identidad y escala 1: si no, mueve geometria del usuario EN SILENCIO.
        if (p.nodo->Childrens.empty()) {
            SoltarNodoViejo(p.nodo);
        } else {
            conHijos++;
            w3dLogfW("[W3D] '%s' era un constraint viejo y tiene %d hijo(s): queda como Empty "
                     "en la misma posicion (sus propiedades ya pasaron al objeto apuntado)",
                     nombreNodo.c_str(), (int)p.nodo->Childrens.size());
            W3dAvisof(false, "'%s' era un constraint: ahora es un Empty, sus %d hijo(s) quedaron donde estaban",
                      W3dNombreCorto(nombreNodo).c_str(), (int)p.nodo->Childrens.size());
        }
        p.nodo = NULL;
    }
    w3dLogf("[W3D] constraints viejos migrados: %d nodo(s) -> %d constraint(s) sobre el objeto "
            "apuntado (%d sin target, %d apuntando a otro constraint viejo, %d quedaron como "
            "Empty por tener hijos)",
            (int)gPendConsViejo.size(), puestos, sinTarget, aOtroNodo, conHijos);
    if (sinTarget > 0)
        W3dAvisof(true, "%d constraint(s) viejo(s) sin target: no hacian nada, no se migro nada", sinTarget);
    gPendConsViejo.clear();
}

// ============================================================================
//  W3dProyectoCargarEscena3D — LA ESCENA 3D DEL JUEGO COMPILADO.
//
//  Es la puerta de entrada que usa el runtime (w3drun) y el UNICO codigo propio
//  del juego en todo este archivo: por dentro llama al MISMO AbrirEscenaJson que
//  usa el editor, asi que el .deb/APK arma exactamente el mismo arbol de escena
//  que el Play — mallas, materiales, animaciones, camaras, curvas/rieles,
//  culling, LOD, visibilidad por celdas, particulas y constraints.
//
//  'datos' son los bytes del proyecto.json que "Compilar juego" dejo al lado del
//  binario. Las rutas de adentro se tratan como NOMBRES DE ENTRADA (el staging
//  del compilador es un espejo exacto del contenedor .w3d), asi que
//  "mallas/pueblo.w3dm" resuelve contra la carpeta del juego sin traducir nada.
//
//  NO hace el pie del editor (layout, sesion, seleccion, auto-play): hace el que
//  necesita un juego — resolver targets, recargar el arbol, ligar los constraints
//  por nombre y dejar las texturas YA cargadas (el juego no puede arrancar con la
//  cola diferida a medias, se veria gris el primer segundo).
// ============================================================================
bool W3dProyectoCargarEscena3D(const void* datos, size_t n) {
    if (!datos || n == 0) return false;
    if (!SceneCollection) SceneCollection = new Object(NULL, "Scene");
    // los nombres entran CRUDOS (igual que en el editor): el archivo ya viene con
    // los nombres definitivos del proyecto guardado y las refs de los scripts
    // resuelven contra ESOS.
    W3dNombresCargando = true;
    gPendModArm.clear(); gPendModTgt.clear(); gPendModGen.clear();
    gPendConsViejo.clear();
    gDirProyecto.clear();
    gProyectoV4 = true;              // las rutas del json son nombres de entrada
    g_w3dRefsEntradas = true;        // ...y las de los .w3dui tambien
    // ---- DETECCION DE FORMATO, LA MISMA QUE HACE EL EDITOR AL ABRIR ----------
    // El proyecto.json que deja "Compilar juego" es UNA COPIA del .w3d, y un .w3d
    // NO es siempre JSON: el formato TEXTO viejo (`Whisk3D { Escena {...} }`, con
    // comentarios // al principio) sigue abriendo en el editor y sigue siendo el
    // formato de proyectos reales. Aca se parseaba SIEMPRE como JSON, asi que un
    // proyecto de texto compilaba bien, instalaba bien... y el juego abria con la
    // escena 3D VACIA: pantalla NEGRA con el HUD encima. Ahora se mira el primer
    // byte util, igual que AbrirW3D (json = '{', texto = 'W'/comentario).
    const char* txt = (const char*)datos;
    size_t ini = 0;
    while (ini < n && (txt[ini] == ' ' || txt[ini] == '\t' ||
                       txt[ini] == '\r' || txt[ini] == '\n')) ini++;
    bool esJson = (ini < n && txt[ini] == '{');
    bool ok;
    if (esJson) {
        ok = AbrirEscenaJson(txt + ini, n - ini, std::string("."));
    } else {
        // TEXTO viejo: el MISMO pipeline del editor (Tokenize + ParseNode +
        // BuildScene), sin el Layout -- un juego no tiene viewports.
        std::vector<std::string> tokens = Tokenize(std::string(txt, n));
        size_t it = 0;
        Node* proyecto = ParseNode(tokens, it);
        ok = (proyecto && proyecto->type == "Whisk3D");
        if (!ok) {
            w3dLogfE("[juego] la escena 3D no es JSON ni texto Whisk3D{} (vino '%s')",
                     proyecto ? proyecto->type.c_str() : "(nada)");
        } else {
            Node* escena = Find(proyecto, "Escena");
            if (escena) BuildScene(escena);
            else        w3dLogfW("[juego] el proyecto de texto no trae 'Escena': sin objetos");
        }
        FreeNode(proyecto);
    }
    g_w3dRefsEntradas = false;
    gProyectoV4 = false;
    W3dNombresCargando = false;
    // MIGRACION del objeto Constraint VIEJO (el nodo suelto que apunta a su
    // objetivo): sus propiedades pasan al objeto apuntado. El editor la corre
    // siempre al abrir; el juego NO la corria, asi que un proyecto con nodos
    // constraint viejos -- todos los .w3d de texto -- perdia sus constraints en
    // el binario. En el demo eso se ve clarito: el domo del CIELO se engancha a
    // la vista con uno de estos, y sin migrar se quedaba plantado en el origen
    // (arrancabas viendo cielo y a los pocos metros el fondo quedaba NEGRO).
    MigrarConstraintsViejos();
    // targets de los modificadores/objetos, y el arbol recargado
    SearchLoop();
    if (SceneCollection) SceneCollection->ReloadAll();
    // la FUENTE de cada constraint viaja por NOMBRE: se liga con el arbol completo
    // (la pasada vive en el Core justamente porque el juego necesita esto).
    if (SceneCollection) W3dConstraintsResolverNombres(SceneCollection);
    // las texturas se encolan DIFERIDAS al importar: el juego las quiere todas
    // decodificadas antes del primer frame.
    CargarTodasTexturasPendientes();
    w3dLogf("[W3D] escena 3D del juego cargada: %d objeto(s) raiz",
            SceneCollection ? (int)SceneCollection->Childrens.size() : 0);
    return ok;
}

#ifndef W3D_SIN_EDITOR
// ---------------------------------------------------------------------------
//  REINICIAR la escena (cerrar el proyecto actual) para abrir otro EN CALIENTE
// ---------------------------------------------------------------------------
void ReiniciarEscena() {
    // parar el juego/simulacion y soltar los scripts
    extern bool SimActiva(); extern void SimStop();
    if (SimActiva()) SimStop();
    PlayAnimation = false;
    AnimEsJuego = false;
    // la ANIMACION ACTIVA (escena/armature/vertex) apunta a objetos que estamos por
    // BORRAR: soltarla ANTES o queda colgando (ActiveAnimMesh/ActiveAnimArm dangling ->
    // Properties::ActualizarPestanias crasheaba al abrir un proyecto con una vertex anim
    // activa). Volver a modo ESCENA sin objetivo.
    { extern int ActiveAnimKind; extern Mesh* ActiveAnimMesh; extern Armature* ActiveAnimArm;
      ActiveAnimKind = 0; ActiveAnimMesh = NULL; ActiveAnimArm = NULL; }
    // salir de Edit Mode: abrir un proyecto en Edit Mode dejaba g_editMesh COLGANDO al
    // borrar la escena (mismo crash que ActiveAnimMesh). Volver a Modo Objeto sin malla.
    { extern int InteractionMode; extern Object* g_editMesh;
      InteractionMode = ObjectMode; g_editMesh = NULL; }
    W3dScriptDescargarTodo();
    // el CONTENEDOR del proyecto que se cierra: se suelta su FILE* y su indice
    // (las rutas en memoria que apuntaban a sus entradas mueren con la escena)
    W3dContenedorDesmontar();
    W3dRefExternasLimpiar();
    g_w3dUINoCargo.clear();   // idem el freno de las escenas UI que no cargaron
    // el icono del juego es DEL PROYECTO: al cerrar uno no debe heredarse al
    // siguiente (el que abre lo vuelve a setear si su .w3d lo trae)
    g_proyIcono.clear();
    // idem las PALETAS del proyecto: el que abre trae las suyas (del .w3d o
    // adoptadas de sus .w3dui)
    W3dPaletasLimpiar();
    // nada seleccionado ni activo
    DeseleccionarTodo();
    ObjActivo = NULL;
    // el undo guarda punteros a objetos que van a morir
    extern void UndoLimpiar();
    UndoLimpiar();
    // Y LAS CURVAS DE OBJETO TAMBIEN (fallo [E] de la ronda 7): una AnimationObject referencia a su objeto POR
    // PUNTERO, y aca se borra la escena entera. Esto limpiaba... nada: ni AnimationObjects ni las listas de las
    // OTRAS animaciones de escena. Al abrir otro proyecto quedaban curvas apuntando a objetos LIBERADOS y el
    // primer cambio de frame las desreferenciaba en AplicarAnimacionObjetos (ObjectMode.cpp) -> use-after-free;
    // ademas el primer objeto nuevo que cayera en una direccion reciclada nacia ya animado.
    // Se limpian TODAS las escenas, no solo la activa: sus curvas son la misma lista swapeada (Animation.cpp).
    InitSceneAnimations();
    AnimationObjects.clear();
    for (size_t i = 0; i < SceneAnimations.size(); i++)
        if (SceneAnimations[i]) SceneAnimations[i]->objetos.clear();
    // borrar TODO el arbol, cada hijo del tope CON SU SUBARBOL (W3dLiberarSubarbol).
    // Antes era un 'delete o' pelado, confiando en que ~Object PROMUEVE los nietos a
    // SceneCollection y la vuelta siguiente los agarraba. Eso solo pasa cuando el borrado
    // tiene Parent: los objetos de PRIMER NIVEL nacen con Parent NULL (solo se meten en
    // SceneCollection->Childrens, ver Object::Object), asi que sus hijos se quedaban con
    // Parent NULL, fuera de la lista de todos, y NO se liberaban nunca -> cada "abrir otro
    // proyecto" fugaba el subarbol entero de cada padre de primer nivel (test 'delarbol',
    // fase D). OJO: hay que SACARLO del vector ANTES de liberar (nadie se quita a si mismo
    // del padre; eso lo hace siempre el caller) o el puntero muerto queda en Childrens.
    if (SceneCollection)
        while (!SceneCollection->Childrens.empty()) {
            Object* o = SceneCollection->Childrens.back();
            SceneCollection->Childrens.pop_back();
            W3dLiberarSubarbol(o);
        }
    CollectionActive = SceneCollection;

    // ---- TEXTURAS: liberar de verdad (reporte del dueno) ----------------------
    // Hasta hoy ReiniciarEscena() no tocaba `Textures` ni una vez: abrir un
    // proyecto tras otro dejaba TODAS las texturas del anterior en la GPU y en
    // el heap para el resto de la vida del proceso. Va DESPUES de destruir el
    // arbol (ninguna malla puede seguir bindeando lo que se libera) y en este
    // orden:
    //   1. tirar la cola diferida: sus entradas apuntan a materiales del
    //      proyecto que se cierra (y como `Materials` no se poda, el chequeo de
    //      "material vivo" decia que si SIEMPRE y terminaba cargando texturas
    //      de un proyecto muerto encima del nuevo);
    //   2. liberar las texturas no fijadas, dejando en NULL los Texture* de
    //      todos los materiales (TexturasLiberarEscena lo hace adentro).
    // el PIXELADO es DEL PROYECTO: el que abre trae el suyo (o ninguno)
    w3dEngine::SetPixeladoGlobal(false);
    // PREFABS: la Biblioteca es DEL PROYECTO (y el hook de instanciar con ella)
    W3dPrefabsLimpiar();
    W3dInstanciarPrefabHook = 0;
    // LISTAS DE CARGA: son del proyecto (las registro Curve::CargarListaCarga).
    // Y sus referencias TRANSITORIAS mueren en bloque: cerrar la escena es el
    // "volver al menu" del modelo de streaming (deja las paginas en 0).
    W3dRecursosPurgarTransitorias();
    W3dCargasOlvidarTodas();
    { extern void OlvidarTexturasPendientes(); OlvidarTexturasPendientes(); }
    { const int n = TexturasLiberarEscena();
      if (n > 0) w3dLogf("[W3D] cierre de proyecto: %d textura(s) liberada(s)", n); }

    // ---- MATERIALES: podarlos de verdad (bug anotado y sin arreglar hasta hoy) --
    // `Materials` es un registro GLOBAL DE PROCESO y aca no se tocaba: abrir un
    // proyecto tras otro dejaba vivos TODOS los materiales del anterior, el ctor
    // de Material los veia como homonimos y uniquificaba, asi que el proyecto
    // reabierto volvia entero con sufijo ".001" (y guardar dejaba el sufijo en el
    // disco). Va DESPUES de destruir el arbol Y DESPUES de TexturasLiberarEscena,
    // que recorre `Materials` para soltar sus Texture*.
    { const int n = MaterialesLiberarEscena();
      if (n > 0) w3dLogf("[W3D] cierre de proyecto: %d material(es) liberado(s)", n); }

    // los punteros de viewport activo apuntan al layout VIEJO: soltarlos (el
    // primer render del layout nuevo los re-engancha)
    Viewport3DActive = NULL;
    g_redraw = true;
}

// la apertura REAL (destruye la escena y arma el layout nuevo). Solo debe correr
// con el stack limpio: desde el inicio del frame (main) o desde el modo --script.
void AbrirProyectoAhora(const std::string& ruta) {
    // BARRA DE PROGRESO: abrir un .w3d bloquea el frame (montar el contenedor, descomprimir,
    // parsear el JSON y construir los objetos). En el N95 eso tarda MUCHO y la pantalla queda
    // congelada -> parece trabado. La barra bombea el render (swap) desde adentro, como el
    // import de modelos: el usuario ve que avanza (y si se traba, en que % quedo).
    extern void ProgresoIniciar(const std::string&); extern void ProgresoFin();
    ProgresoIniciar("Abriendo proyecto...");
    ReiniciarEscena();
    // OJO: rootViewport se reemplaza por el layout del proyecto; el viejo queda
    // (los viewports globales del editor se reusan). El layout nuevo pisa el puntero.
    rootViewport = NULL;
    AbrirW3D(ruta);
    // el layout nuevo nunca paso por Resize: darle el tamano ACTUAL de la ventana
    // (sin esto los viewports quedan en 0x0 y el editor parece TRABADO)
    extern int MenuPantallaW, MenuPantallaH;
    if (rootViewport && MenuPantallaW > 0 && MenuPantallaH > 0)
        rootViewport->Resize(MenuPantallaW, MenuPantallaH);
    extern void ProyectoSincronizarCampos();
    ProyectoSincronizarCampos();
    // MODO JUEGO (.sisx bundleado): full-screen del 3D -> el juego se ve solo, sin el chrome
    // del editor. El auto-play (game mode + Play) ya lo hizo AbrirW3D si el proyecto tiene scripts.
    { extern bool g_modoJuego;   // definido mas abajo en este archivo
      if (g_modoJuego) { extern void LayoutMaximizar3DParaJuego(); LayoutMaximizar3DParaJuego(); } }
    ProgresoFin();
    g_redraw = true;
}

// abrir un proyecto DESDE LA TARJETA Archivo: NO se abre aca mismo (el boton que
// disparo esto vive en el layout que vamos a destruir; destruirlo con su stack
// activo TRABABA el editor). Se anota y el main lo abre al inicio del proximo frame.
std::string g_proyAbrirPendiente;
// MODO JUEGO: lo prende el arranque (Whisk3D.cpp) cuando el .sisx trae un proyecto bundleado.
// Hace que al abrir el proyecto se full-screene el viewport 3D (el juego se ve solo).
bool g_modoJuego = false;
void AbrirProyectoDesde(const std::string& ruta) {
    g_proyAbrirPendiente = ruta;
    g_redraw = true;
}

// ===========================================================================
//  ENTRADA UNICA de lectura: AbrirW3D(ruta) detecta el formato POR CONTENIDO
//  y rutea. Los tres formatos convergen en las mismas estructuras y en los
//  mismos aplicadores de campos (una sola fuente de verdad por campo):
//    '{'        -> JSON PLANO nuevo (v3: el .w3d ES el json)
//    'PK'       -> zip v2 viejo (escena.json adentro; se extrae como antes)
//    'Whisk3D'  -> texto viejo (Whisk3D { Escena { ... } Layout { ... } })
//  Cualquier problema (archivo inexistente, json roto, formato desconocido,
//  .w3dui faltante) deja el editor VIVO: aviso claro en el log (visible en el
//  viewport Console) + default sensato. NUNCA crash ni abortar todo.
// ===========================================================================

// nombre de archivo pelado sin extension ("a/b/consola.w3d" -> "consola")
static std::string NombreSinExt(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    std::string b = (s == std::string::npos) ? r : r.substr(s + 1);
    size_t p = b.find_last_of('.');
    return (p == std::string::npos) ? b : b.substr(0, p);
}

// ---------------------------------------------------------------------------
//  camino CONTENEDOR v4: el .w3d es un ZIP con proyecto.json adentro.
//  NO SE EXTRAE NADA: se indexa, se MONTA como origen virtual de
//  w3dFileSystem::ReadFileBytes y las rutas del JSON son nombres de entrada.
//  Por eso texturas, fuentes, lua, .w3dui y GLB cargan de adentro sin tocar sus
//  call sites, y el editor nunca trabaja sobre una copia extraida.
//
//  "mimetype" y "LEEME.txt" (las entradas de servicio del estilo OpenDocument)
//  NO se leen NUNCA: son para el que abre el zip con un descompresor. El que
//  manda para decidir si esto es un proyecto es proyecto.json, asi que un .w3d
//  guardado ANTES del mimetype abre igual y se migra en el primer guardado.
// ---------------------------------------------------------------------------
static bool AbrirW3DContenedor(const std::string& ruta) {
    extern void ProgresoActualizar(float);
    if (!W3dContenedorMontar(ruta)) {
        w3dLogfE("[W3D] no pude montar el contenedor %s (se abre vacio)", ruta.c_str());
        return false;
    }
    ProgresoActualizar(0.12f);   // contenedor montado (indice del zip descomprimido)
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes("proyecto.json", datos) || datos.empty()) {
        w3dLogfE("[W3D] el contenedor %s no trae proyecto.json legible (se abre vacio)", ruta.c_str());
        return false;
    }
    ProgresoActualizar(0.22f);   // proyecto.json leido y descomprimido
    gProyectoV4 = true;
    g_w3dRefsEntradas = true;          // los .w3dui tambien traen nombres de entrada
    g_w3dRefExtMarcar = W3dRefExternaMarcar;
    bool ok = AbrirEscenaJson((const char*)&datos[0], datos.size(), std::string());
    g_w3dRefsEntradas = false;
    g_w3dRefExtMarcar = NULL;
    return ok;
}

// camino ZIP v2 viejo: extraer a userdata (los .glb/.bin referenciados
// necesitan vivir en disco) y abrir el escena.json de adentro.
static bool AbrirW3DZip(const std::string& ruta) {
    std::string base = w3dFileSystem::GetUserDataDir() + "/w3d_abierto/" + NombreSinExt(ruta);
    if (!W3dZipExtraer(ruta, base)) {
        w3dLogfE("[W3D] no pude extraer el zip: %s (se abre vacio)", ruta.c_str());
        return false;
    }
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(base + "/escena.json", datos) || datos.empty()) {
        w3dLogfE("[W3D] el zip no trae escena.json: %s (se abre vacio)", ruta.c_str());
        return false;
    }
    return AbrirEscenaJson((const char*)&datos[0], datos.size(), base);
}

// camino TEXTO viejo: Whisk3D { Escena {...} Layout {...} }
static bool AbrirW3DTexto(const std::string& src) {
    std::vector<std::string> tokens = Tokenize(src);
    size_t i = 0;
    Node* project = ParseNode(tokens, i);
    bool ok = project && project->type == "Whisk3D";
    if (!ok) {
        w3dLogfE("[W3D] se esperaba un root Whisk3D{}, vino '%s' (se abre vacio)",
                 project ? project->type.c_str() : "(nada)");
    } else {
        Node* escena = Find(project, "Escena");
        Node* layout = Find(project, "Layout");
        if (!escena) w3dLogfW("[W3D] el archivo no trae 'Escena': abre sin objetos");
        if (escena) BuildScene(escena);
        if (layout && !layout->children.empty())
            rootViewport = BuildLayout(layout->children[0]);
        else
            w3dLogfW("[W3D] el archivo no trae 'Layout': va el default");
    }
    FreeNode(project);
    return ok;
}


void AbrirW3D(const std::string& ruta) {
    // los nombres entran CRUDOS mientras dura la carga (los .w3d ya guardados TIENEN
    // duplicados) y se reparan todos juntos al final, con el arbol completo y los
    // vinculos por nombre a la vista. Ademas evita el O(n^2) de uniquificar uno a uno.
    W3dNombresCargando = true;
    w3dPath = ruta;
    gDirProyecto = DirDelProyecto();
    g_w3dDirProyecto = gDirProyecto;   // base de las rutas "ext:" relativas (UI2DFormato)
    gPendModArm.clear(); // referencias modArmature de una carga anterior (por si quedo algo)
    gPendModTgt.clear(); // idem targets de modificadores y mallas a regenerar
    gPendModGen.clear();
    gPendConsViejo.clear(); // idem los nodos del objeto Constraint VIEJO (se migran al final)

    // campos DEL PROYECTO: arrancan limpios (el archivo los setea si los trae;
    // sin esto un proyecto sin escenaInicial heredaba la del proyecto anterior)
    AplicarEscenaInicial(std::string());
    AplicarModoEscenas(false);
    // idem la config de la tarjeta Juego: un .w3d sin el bloque "compilar" abre
    // con los defaults (no hereda el modo ventana del proyecto anterior)
    W3dCompilarReset();
    // idem el estado de reproduccion guardado: ausente = -1 (auto-play de siempre)
    g_proyReproduciendo = -1;
    // idem la SESION (frame + seleccion). El frame vuelve al 1: un .w3d sin el bloque
    // (los viejos) abre en el default del editor y NO se queda con el playhead donde lo
    // habia dejado el proyecto anterior. Si el archivo trae "sesion", SesionAplicar lo
    // pisa en el pie. La seleccion ya la limpia el DeseleccionarTodo del pie.
    SesionReset();
    CurrentFrame = 1;
    // idem el FPS: AplicarFps ignora un "fps" ausente/0, asi que sin este reset
    // un proyecto sin el campo HEREDABA el fps del proyecto anterior
    AnimFPS = 30;

    // el proyecto ANTERIOR deja de estar montado (su FILE* se suelta) y sus refs
    // externas dejan de valer: las del que abre las trae SU archivo
    W3dContenedorDesmontar();
    W3dRefExternasLimpiar();
    g_w3dUINoCargo.clear();   // el freno de las escenas UI es DEL proyecto que se abre, no del anterior
    gProyectoV4 = false;

    std::vector<unsigned char> datos;
    // un ZIP no se levanta entero solo para detectarlo: W3dZipEs mira 4 bytes y
    // el contenedor despues se indexa por el directorio central (nunca completo)
    bool esZip = W3dZipEs(ruta);
    bool leido = esZip || (w3dFileSystem::ReadFileBytes(ruta, datos) && !datos.empty());
    const char* formato = "desconocido";
    bool ok = false;

    if (esZip) {
        if (W3dContenedorEs(ruta)) {
            formato = "contenedor v4";
            ok = AbrirW3DContenedor(ruta);
        } else {
            formato = "zip v2";
            ok = AbrirW3DZip(ruta);
        }
    } else if (!leido) {
        w3dLogfE("[W3D] no pude leer %s (no existe o esta vacio): se abre vacio", ruta.c_str());
    } else {
        // deteccion POR CONTENIDO (no por extension): primer byte util
        size_t i = 0;
        while (i < datos.size() && (datos[i] == ' ' || datos[i] == '\t' ||
                                    datos[i] == '\r' || datos[i] == '\n')) i++;
        bool esTexto = (datos.size() - i >= 7 && memcmp(&datos[i], "Whisk3D", 7) == 0) ||
                       (i < datos.size() && (datos[i] == '/' || datos[i] == '#'));   // comentario inicial
        if (i < datos.size() && datos[i] == '{') {
            formato = "json";
            ok = AbrirEscenaJson((const char*)&datos[0] + i, datos.size() - i, gDirProyecto);
        } else if (esTexto) {
            formato = "texto viejo";
            ok = AbrirW3DTexto(std::string((const char*)&datos[0], datos.size()));
        } else {
            w3dLogfE("[W3D] %s no parece un .w3d (ni JSON, ni zip, ni texto Whisk3D): se abre vacio",
                     ruta.c_str());
        }
    }

    // ---- pie COMUN a los tres formatos (tambien si algo fallo: el editor sigue) ----
    if (!CollectionActive) CollectionActive = SceneCollection;
    bool layoutDelProyecto = (rootViewport != NULL);
    if (!rootViewport) {
        // fallback: el .w3d no trae bloque Layout -> EL layout por defecto (por tamano de pantalla,
        // el MISMO que el arranque sin archivo). MenuPantallaW/H estan seteados en PC y en Symbian.
        extern int MenuPantallaW, MenuPantallaH;
        rootViewport = LayoutPorDefecto(MenuPantallaW, MenuPantallaH, NULL);
    }

    // nada seleccionado al abrir (los constructores van seleccionando lo
    // ultimo creado; un proyecto recien abierto arranca LIMPIO)
    DeseleccionarTodo();
    ObjActivo = NULL;

    // un proyecto CON SCRIPTS es un JUEGO: arranca con el selector del timeline
    // en "Juego" y en PLAY (asi el doble click abre jugando y las vertex
    // animations corren de una). PERO si el .w3d v3 trae "reproduciendo": false
    // (se GUARDO en pausa), se respeta y abre en pausa.
    // Ausente (-1, archivos viejos) = auto-play de siempre.
    {
        extern bool SimHayScripts();
        if (SimHayScripts()) {
            extern int ActiveAnimKind;
            ActiveAnimKind = 2; AnimEsJuego = true;
            StartFrame = 1;
            PlayAnimation = (g_proyReproduciendo != 0);
            g_redraw = true;
        }
    }

    W3dNombresCargando = false;

    // MIGRACION del objeto Constraint viejo: sus propiedades pasan al objeto que APUNTABA (la
    // inversion de semantica) y el nodo desaparece si no tiene hijos. Va PRIMERA, antes de la
    // reparacion de nombres, porque los nodos que se van no tienen que contar como duplicados
    // de un objeto real: si contaran, la que se lleva el ".001" (para siempre) puede ser la
    // malla del usuario. El motivo completo esta arriba, en el bloque de MigrarConstraintsViejos.
    MigrarConstraintsViejos();

    // NOMBRES UNICOS: reparar los duplicados que venian en el archivo. Va ANTES del
    // ReloadAll (que resuelve targetName/RielName por nombre) para que los punteros se
    // resuelvan contra los nombres YA definitivos. Avisa de forma visible: renombrar
    // puede romper scripts lua que buscan por nombre.
    W3dNombresRepararEscena(true);

    // recargar los targets de todos los objetos modificadores
    SearchLoop();
    if (SceneCollection) SceneCollection->ReloadAll();

    // ...y la FUENTE de cada constraint, que tambien viaja por nombre. Va DESPUES de
    // W3dNombresRepararEscena (los nombres recien ahi son los definitivos: resolver antes
    // ligaria contra un nombre que la reparacion todavia va a mover) y despues del
    // ReloadAll, o sea en el mismo lugar y por el mismo motivo que targetName / RielName.
    // La pasada vive en el CORE (Objects.cpp) porque el runtime de un juego carga el mismo
    // archivo y necesita exactamente esto.
    // (los constraints que acaba de crear la migracion NO dependen de esta pasada: su fuente
    // es la VISTA, sin nombre que resolver. Por eso puede ir antes y esta no la alcanza.)
    if (SceneCollection) W3dConstraintsResolverNombres(SceneCollection);

    // SESION: el frame y la seleccion que el usuario tenia en ESTE proyecto. Va DESPUES
    // de W3dNombresRepararEscena (los nombres recien ahi son los definitivos) y despues
    // del DeseleccionarTodo de arriba, que es el que deja limpio al que no trae bloque.
    SesionAplicar();

    // RESUMEN corto de la apertura (queda en el log; se ve en el viewport Console)
    if (ok) {
        int uis = 0;
        int objs = SceneCollection ? (int)SceneCollection->Childrens.size() : 0;
        if (SceneCollection)
            for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
                if (SceneCollection->Childrens[i]->getType() == ObjectType::ui) uis++;
        w3dLogf("[W3D] abierto %s (%s): %d objeto(s), %d escena(s) UI, escenaInicial '%s', "
                "layout %s, icono '%s'",
                ruta.c_str(), formato, objs, uis, W3dEscenaInicial().c_str(),
                layoutDelProyecto ? "del proyecto" : "default", g_proyIcono.c_str());
    }
}

// compat: los llamadores viejos (constructor con w3dPath ya seteado) siguen andando
void OpenW3D() { AbrirW3D(w3dPath); }
#endif // W3D_SIN_EDITOR (abrir un proyecto EN CALIENTE: es cosa del editor)