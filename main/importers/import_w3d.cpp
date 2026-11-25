#include "importers/import_w3d.h"
#include "io/EditedTexture.h"   // TexEditClearAll al reiniciar la escena
#include "objects/ObjectMode.h"   // W3dNamesRepairScene (duplicados de archivos ya guardados)
#include <math.h>                    // sqrtf: normal de la clave geometrica de los UV groups
#include "render/RenderOptions.h"
#include "render/RealTimeShadows.h"   // g_shadows: la tarjeta "Sombras en tiempo real"
#include "render/PostProcess.h"   // los efectos del postprocesado
#include "objects/PostProcessObj.h"
#include "render/RayTracer.h"   // g_rt: el trazado de rayos del proyecto   // RenderType / g_redraw: son del editor
#include "stb/stb_image.h" // stb ahora vive en libs/Whisk3DCore/thirdparty
#include "script/W3dScript.h"  // colgar el <proyecto>.lua al objeto Script/gamepad
#include "physics/W3dRigid.h"  // W3dRigidDef (bloque "fisica") + W3dRigidEnsure (anexo en runtime)
#include "io/W3dResources.h"   // listas de carga (cierre de proyecto) — Core
#include "animation/Animation.h" // auto modo-juego al abrir un proyecto con scripts
// ============================================================================
//  W3D_NO_EDITOR — ESTE MISMO LECTOR, ADENTRO DE UN JUEGO COMPILADO.
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
//     - y la apertura de un proyecto EN CALIENTE (RestartScene / OpenW3D).
//  Todo el resto -- objetos, mallas, materiales, animaciones, constraints,
//  curvas, camaras, particulas, culling y visibilidad -- es EL MISMO codigo.
//  La puerta de entrada del juego es W3dProjectLoadScene3D (mas abajo).
// ============================================================================
#ifndef W3D_NO_EDITOR
#include "ViewPorts/Editor2D.h"  // layouts con Editor2D/Timeline (proyectos de juego)
#include "ViewPorts/LayoutInput.h" // LayoutDefault: el fallback de un .w3d sin bloque Layout
#include "ViewPorts/Timeline.h"
#include "ViewPorts/UVEditor.h"  // BuildLayout: faltaban en el mapeo de lectura y un
#include "ViewPorts/Console.h"   // layout guardado con UV/Console caia al default
#include "ViewPorts/IDE.h"       // layouts con el editor de texto de scripts (IDE)
#include "ViewPorts/LayoutTree.h" // abrir un proyecto LIBERA el layout anterior (ViewportDeleteTree)
#include "ViewPorts/Outliner.h"   // BuildLayout: la VISTA del outliner (escena / recursos) viaja con la hoja
#include "io/ProjectResources.h"  // las carpetas del outliner por recursos (se leen del proyecto)
#endif
#include "w3dlog.h"                // avisos de layout tambien al log (los ve la Console)
#include "W3dLang.h"               // T(): el texto de la barra de progreso en el idioma del editor
#include "objects/UI.h"            // la UI del proyecto (rama UI { archivo: ... })
#include "objects/Empty.h"         // el nodo generico ("tipo": "objeto") vuelve como Empty
#include "objects/Instance.h"      // Instance ("tipo": "Instance"): usado antes del include de mas abajo
#include "objects/LOD.h"           // objeto LOD (un hijo por distancia a la camara)
#include "objects/Culling.h"       // objeto Culling (frustum culling de sus hijos)
#include "objects/Routine.h"       // objeto Rutina (lista de pasos del render)
#include "objects/Fog.h"           // objeto Niebla (glFog en el arbol)
#include "objects/Sound.h"         // objeto Sonido (audio puesto en la escena)
#include "objects/ClipRegion.h"    // ClearZ / Recorte (composicion de la pantalla en el arbol)
#include "objects/Hitbox.h"        // HITBOX: caja de deteccion con eventos lua
#include "objects/LightProbe.h"   // la sonda de luz
#include "objects/ReflectionProbe.h"   // la sonda de reflejo
#include "objects/InstancePrefab.h" // INSTANCIA DE PREFAB ("tipo":"prefab"): sus hijos se generan (io/Prefabs.h)
#include "io/Prefabs.h"         // la definicion de cada prefab y la generacion de sus instancias
#include "objects/ProxyW3d.h"   // PROXY W3D ("tipo":"proxy"): un prefab o una escena de una LIBRERIA externa
#include "io/Libraries.h"       // las LIBRERIAS externas: su contexto de carga (rutas "lib:" y nombres con prefijo)
#include "objects/Particles.h"  // objeto Particulas (emisor de billboards del Core)
#include "objects/VisZone.h"    // objeto VisZone (celda de visibilidad: grilla/volumenes/curva)
#include "io/UI2DFormat.h"
#include "W3dScene.h"           // escenaInicial / modoEscenas (multi-escena)
#include "W3dRoots.h"           // las ESCENAS 3D y los PREFABS del proyecto (registro + carga perezosa)
#include "audio/W3dVolume.h"    // VolumeApplyProject (volumen inicial por-proyecto del .w3d)
#include "W3dPalettes.h"        // paletas del PROYECTO (raiz "paletas" del .w3d v3)
#include "io/SaveW3D.h"         // g_projIcon: el icono del juego (ruta externa)
#include "io/W3dContainer.h"    // FORMATO v4: el .w3d es un zip que se MONTA (no se extrae)
#include "io/W3dZip.h"          // W3dZipIs: detectar el zip sin levantar el archivo entero
#include "io/W3dMesh.h"         // .w3dm: la geometria propia (reemplaza al GLB como formato de guardado)
#include "io/W3dMeshBin.h"      // .w3db: la misma geometria en BINARIO, lista para memoria (sin derivar al abrir)
#include "objects/MeshResource.h" // las mallas 3D como RECURSO: registro "mallas" + "malla": <nombre> del objeto
#include "io/Streaming.h"            // las instancias DIFERIDAS ("carga": "distancia") no se generan al abrir el juego
#include "io/W3dNodes.h"         // .w3dnodos: oclusion por nodos de riel (estilo SLST de PS1)
#include "objects/Materials.h"   // bloque raiz "materiales" (antes viajaban dentro del GLB)
#include "graphics/w3dGraphics.h"     // gfx::Mezcla: acotar el modo de mezcla que venga del archivo
#include "graphics/w3dTexture.h"      // Textures16Always: el formato de las texturas del proyecto
#include "objects/Textures.h"
#include "importers/import_obj.h" // EnqueueTexture: la carga de texturas es DIFERIDA (1 por frame)
#include "w3dFilesystem.h"
#include "W3dNotice.h"          // W3dAvisof: los problemas de carga TIENEN que verse en PANTALLA
#include "W3dNames.h"           // W3dUniqueName: el nombre del constraint migrado, en el espacio del objeto

// MEDICION de la apertura por fase (ver import_w3d.h). Sin reloj (lo normal) no se mide.
W3dLoadPhases g_w3dLoadPhases;
double (*g_w3dLoadClock)() = NULL;
// formato de las mallas al guardar (ver import_w3d.h): el del proyecto + el forzado del harness
int g_w3dFormatMeshesProject = W3D_MESHES_BINARY;
int g_w3dFormatForcedMeshes  = -1;
// las mallas .w3db se leen COMO EL JUEGO (solo render; ver import_w3d.h). El juego, siempre.
#ifndef W3D_NO_EDITOR
bool g_w3dMeshesAsGame = false;
#endif
// LA PILA DE EFECTOS de un postprocesado (el objeto, o el bloque viejo de los proyectos de antes)
struct JVal;
static void W3dPostRead(JVal* j, W3dPostOptions& o);

static bool MeshesAsGame() {
#ifdef W3D_NO_EDITOR
    return true;
#else
    return g_w3dMeshesAsGame;
#endif
}
// ms del reloj de medicion, o 0 si nadie esta midiendo
static double LoadNow() { return g_w3dLoadClock ? g_w3dLoadClock() : 0.0; }
// DE DONDE SALIO CADA MALLA en la ULTIMA apertura: OpenW3D los pone en 0 y los escribe en la
// linea "[CARGA]" del log. Se cuentan SIEMPRE, con o sin reloj de medicion: en el telefono no hay
// harness y el log (e:\whisk3d.log) es la forma de ver que las .w3db abrieron LISTAS, sin derivar.
static int s_binaryMeshes = 0, s_derivedMeshes = 0, s_meshesRejectedBin = 0;

//ESTO DESPUES TIENE QUE IR A UN ARCHIVO SEPARADOOOOO
#ifndef W3D_SYMBIAN  // icono de la ventana via SDL: solo desktop (en Symbian no hay ventana SDL)
SDL_Surface* LoadSurfaceSTB(const std::string& path) {

    int width, height, channels;

    // por ReadFileBytes y NO por stbi_load(path): asi el icono del proyecto sale
    // igual de ADENTRO del contenedor .w3d ("project/icono.png") que de un
    // archivo suelto. stbi_load abre con fopen y no ve el montaje.
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(path, data) || data.empty())
        return NULL;
    stbi_uc* pixels = stbi_load_from_memory(&data[0], (int)data.size(),
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
#ifndef W3D_NO_EDITOR
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
    // overlay de STATS (fps/gl/faces/...) PER-VIEWPORT: se leen para que un .w3d pueda abrir con el overlay YA
    // prendido (ej un juego que quiere ver los fps). Independientes de showOverlays (RenderStats no gatea).
    if(p.count("OverlayFps"))             v->OverlayFps = B("OverlayFps");
    if(p.count("OverlayStatVertices"))    v->OverlayStatVertices = B("OverlayStatVertices");
    if(p.count("OverlayStatFaces"))       v->OverlayStatFaces = B("OverlayStatFaces");
    if(p.count("OverlayStatGL"))          v->OverlayStatGL = B("OverlayStatGL");
    if(p.count("OverlayStatModgen"))      v->OverlayStatModgen = B("OverlayStatModgen");
    if(p.count("OverlayStatTimes"))       v->OverlayStatTimes = B("OverlayStatTimes");
    // submenu Overlays>"Objects": mostrar/ocultar el overlay de cada TIPO (esqueleto/luces/camaras/EMPTIES).
    // Se guardan por-viewport (ej: un juego que no quiere ver los gizmos de los Empty/Gamepad abre con showEmpty:false).
    if(p.count("showArmature"))           v->showArmature = B("showArmature");
    if(p.count("showLights"))             v->showLights = B("showLights");
    if(p.count("showCamera"))             v->showCamera = B("showCamera");
    if(p.count("showEmpty"))              v->showEmpty = B("showEmpty");
    if(p.count("showParticles"))         v->showParticles = B("showParticles");
    if(p.count("showCurves"))             v->showCurves = B("showCurves");
    if(p.count("showHitbox"))             v->showHitbox = B("showHitbox");
    if(p.count("showSounds"))            v->showSounds = B("showSounds");
    // ("ShowUi" se dio de baja: los .w3d viejos que la traigan caen aca como cualquier
    //  clave desconocida -se ignora- y el chrome del viewport se dibuja SIEMPRE)
    if(p.count("showFloor"))              v->showFloor = B("showFloor");
    if(p.count("showYaxis"))              v->showYaxis = B("showYaxis");
    if(p.count("showXaxis"))              v->showXaxis = B("showXaxis");
    if(p.count("CameraToView"))           v->CameraToView = B("CameraToView");
    if(p.count("showOrigins"))            v->showOrigins = B("showOrigins");
    if(p.count("show3DCursor"))           v->show3DCursor = B("show3DCursor");
    if(p.count("ShowRelationshipsLines"))v->ShowRelationshipsLines = B("ShowRelationshipsLines");
    if(p.count("clearScreen"))        v->clearScreen = B("clearScreen");

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
// LayoutWriteNode en SaveW3D.cpp: por que van con la hoja y no en "sesion" ni en el config).
// Sin las props (layout viejo, o escrito a mano) queda el default del constructor: dope sheet con
// el encuadre de siempre -- por eso el default de cada GetFloatOrDefault es el valor VIVO del
// timeline recien construido y no una constante repetida aca.
// Cualquier palabra que no sea "curvas" cae a dope: un layout raro no puede dejar el timeline en
// un modo invalido. Y los cuatro numeros de la vista pasan SIEMPRE por Timeline::ViewApply, que
// es la que valida (nan/inf/zoom<=0/desplazamientos absurdos) y acota con los topes de la rueda:
// un .w3d es editable a mano y no puede colgar el editor ni dejar el timeline invisible.
void ApplyTimelineProps(Timeline* t, const std::map<std::string,std::string>& p){
    if(!t) return;
    if(p.count("mode"))
        t->mode = (w3dMapAt(p, "mode") == "curves") ? Timeline::TL_MODE_CURVES : Timeline::TL_MODE_DOPE;
    t->ViewApply(GetFloatOrDefault(p, "zoomFrame",   t->pxPerFrame),
                    GetFloatOrDefault(p, "frameLeft",    t->viewStartF),
                    GetFloatOrDefault(p, "zoomValue",   t->pxPerUnit),
                    GetFloatOrDefault(p, "centerValue", t->viewCenterV));
}

// ----------------------------- Builders -----------------------------
ViewportBase* BuildLayout(Node* n){
    if(!n) return NULL;
    w3dLogf("[BuildLayout] node=%s", n->type.c_str());  // log PROPIO, nunca stdout (abria la consola en CADA carga)

    if(n->type == "Viewport3D"){
        Viewport3D* v = new Viewport3D();
        ApplyViewport3DProps(v, n->props);
        return v;
    }
    if(n->type == "Outliner"){
        // la VISTA del outliner viaja con la hoja: "vista: biblioteca" (+ "filtro: mallas", "cuadricula: 1"). La
        // de la fase anterior ("vista: materiales") es la biblioteca con ese filtro. Una clave que no se conoce
        // (un .w3d de una version mas nueva, o una libreria que ya no esta vinculada) cae a la escena.
        // (un outliner que se guardo mirando los recursos -la fase anterior, con el selector Escena / Biblioteca- abre
        // como el viewport BIBLIOTECA: el outliner ya no la muestra)
        const bool aResources = n->props.count("view") && w3dMapAt(n->props, "view") != "scene";
        Outliner* o = aResources ? (Outliner*)new Library() : new Outliner();
        if (aResources) o->ChangeGrid(false);   // (lo que tenia: la lista, salvo "cuadricula: 1")
        if (n->props.count("view")) o->ViewByKey(w3dMapAt(n->props, "view"));
        if (n->props.count("filter")) {
            const int f = W3dViewByKey(w3dMapAt(n->props, "filter"));
            if (f > W3D_VIEW_SCENE) o->ChangeFilter(f);
        }
        if (n->props.count("grid")) o->ChangeGrid(w3dMapAt(n->props, "grid") == "1");
        return o;
    }
    if(n->type == "Library"){
        Library* b = new Library();
        if (n->props.count("view")) b->ViewByKey(w3dMapAt(n->props, "view"));
        if (n->props.count("filter")) {
            const int f = W3dViewByKey(w3dMapAt(n->props, "filter"));
            if (f > W3D_VIEW_SCENE) b->ChangeFilter(f);
        }
        if (n->props.count("grid")) b->ChangeGrid(w3dMapAt(n->props, "grid") == "1");
        return b;
    }
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
				w3dLogfW("[BuildLayout] children temporales desde props A/B para %s", n->type.c_str());
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

        // contenedor de UN SOLO hijo (ej 'ViewportRow { Viewport3D {} }'): COLAPSAR al hijo, NO inventar un
        // segundo Viewport3D fantasma. Antes 'if(!B) B=new Viewport3D()' devolvia un split 50/50 con un 3D de mas
        // -> por eso un .w3d que pedia 1 viewport (minimal.w3d) mostraba 2 (en PC y en el N95, codigo compartido).
        if(A && !B) return A;
        if(B && !A) return B;
        if(!A && !B) return new Viewport3D();

        if(isRow)  return new ViewportRow(A,B,split);
        else       return new ViewportColumn(A,B,split);
    }

    // tipo DESCONOCIDO (ej: archivo guardado por una version mas nueva del
    // editor): NO se aborta el layout entero, va un viewport 3D en su lugar
    // y queda el aviso en consola y en el log (visible en la Console del editor)
    w3dLogfW("[W3D] layout: viewport desconocido '%s', va un Viewport3D en su lugar", n->type.c_str());
    return new Viewport3D();
}
#endif // W3D_NO_EDITOR (layout de viewports)

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
static std::string DirOfProject() {
    size_t s = w3dPath.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : w3dPath.substr(0, s);
}

// el FPS del proyecto (timeline/animaciones). f < 1 = campo ausente o roto:
// queda el default del editor (no se rompe nada por un numero invalido).
static void ApplyFps(int f) {
    if (f < 1) return;
    if (f > 120) f = 120;
    AnimFPS = f;
}

// TOPE DE RENDER del proyecto (`fpsCap` en la Escena), DISTINTO de `fps` (que es AnimFPS, el ritmo de
// animacion): cuantos frames/seg dibuja el motor. Lo lee el loop de escritorio (main.cpp) para pacear;
// en Symbian el vblank ya topa a <=60 y nadie lo lee (solo se clampea). Default 60.
int g_fpsCap = 60;
static void ApplyFpsCap(int f) {
    if (f < 1) return;   // ausente/roto: queda el default (reseteado a 60 al cargar cada proyecto)
#if defined(W3D_SYMBIAN)
    if (f > 60) f = 60;  // Symbian: nunca supera 60 (el N95 no llega igual)
#else
    if (f > 120) f = 120; // PC/Android: tope 120
    if (f < 30)  f = 30;
#endif
    g_fpsCap = f;
}

// el "fullscreen" del archivo NO cambia la ventana al abrir (
// un proyecto no tiene que arrancar a pantalla completa). Solo actualiza cfg
// para que SaveW3D lo re-escriba con el proyecto (y lo lea el juego final).
#ifndef W3D_NO_EDITOR
static void ApplyFullscreen(bool v) { cfg.fullscreen = v; }
#else
// en el juego compilado el modo de ventana ya lo decidio "Compilar juego" (lo
// hornea el main.cpp generado): el campo del archivo no lo puede cambiar.
static void ApplyFullscreen(bool) {}
#endif

// estado de REPRODUCCION guardado en el .w3d v3 (campo raiz "reproduciendo"):
// -1 = ausente (archivos viejos) -> auto-play de siempre si hay scripts;
//  0 = guardado en PAUSA -> abre en pausa; 1 = guardado en PLAY -> abre en play.
// Lo consume el pie comun de OpenW3D; se resetea a -1 en cada apertura.
static int g_projPlaying = -1;

// ---------------------------------------------------------------------------
//  BLOQUE "sesion" (v3, opcional): donde estaba trabajando el usuario en ESTE
//  proyecto -- el frame actual y la seleccion. Ver el comentario grande de
//  WriteSession (SaveW3D.cpp) para el reparto .w3d / config.start.
//
//  Se GUARDA en estos globales durante el parseo y se APLICA en el pie de
//  OpenW3D, no aca: la seleccion es por NOMBRE y los nombres recien quedan
//  definitivos despues de W3dNamesRepairScene (un .w3d puede traer
//  duplicados y el reparador renombra). Un .w3d viejo no trae el bloque:
//  g_sessionFrame queda en -1 y no se toca nada (retrocompat).
// ---------------------------------------------------------------------------
static int g_sessionFrame = -1;                     // -1 = el archivo no trae "sesion"
static std::string g_activeSession;                 // nombre del objeto ACTIVO ("" = ninguno)
static std::vector<std::string> g_sessionSelection; // nombres de los objetos seleccionados
// la RAIZ que se estaba editando (una escena 3D que no es la del bloque, o un prefab: ver
// W3dRoots.h). "" = la del bloque. La seleccion de arriba es de ESA raiz.
static std::string g_sessionRoot;
static int g_sessionRootType = W3D_ROOT_SCENE;
// QUE MIRABA LA LINEA DE TIEMPO si no era una animacion de escena ("animTipo": armature / objeto / armature2d /
// flipbook; 0 = la de escena, que ya repone "animaciones"."activa"), de QUIEN (por nombre) y, en una malla, que clip
static int g_sessionAnimType = 0;
static std::string g_sessionAnimObject;
static int g_sessionAnimClip = -1;

static void SessionReset() {
    g_sessionFrame = -1; g_activeSession.clear(); g_sessionSelection.clear();
    g_sessionRoot.clear(); g_sessionRootType = W3D_ROOT_SCENE;
    g_sessionAnimType = 0; g_sessionAnimObject.clear(); g_sessionAnimClip = -1;
}

// APLICA lo leido. Se llama en el pie de OpenW3D, con los nombres ya definitivos.
//  - frame: se ACOTA (viene de un archivo editable a mano; un frame negativo o
//    absurdo dejaria el playhead fuera de la regla y toda evaluacion de curvas
//    trabajando con un numero que no existe).
//  - seleccion: por NOMBRE. El que no aparece se IGNORA en silencio para el
//    usuario (queda en el log): la seleccion es una comodidad y jamas un motivo
//    para no abrir el proyecto. Se resuelve con FindObjectByName, que es el mismo
//    lookup que usan los targets y los scripts -> con homonimos entre escenas cae
//    en el PRIMERO, exactamente igual que ellos.
//  - activo: si el nombre no resuelve, ActiveObj queda como lo dejo la seleccion
//    (Object::Seleccionar ya lo va poniendo) y si no habia nada, en NULL.
static void SessionApply() {
    if (g_sessionFrame < 0) return;                   // el archivo no traia el bloque
    const int kFrameMax = 1000000;                    // la misma cota que el resto de los frames del lector
    int f = g_sessionFrame;
    if (f > kFrameMax) {
        w3dLogfW("[W3D] sesion: 'frame' vale %d, fuera de [0..%d] -> se acota", f, kFrameMax);
        f = kFrameMax;
    }
#ifndef W3D_NO_EDITOR
    // la ANIMACION de la linea de tiempo va ANTES del frame: elegirla carga SU rango y acota el frame a el
    extern bool g_animMix;
    if (g_sessionAnimType != 0 && !g_animMix) {
        extern bool AnimSelRestore(int type, Object* o, const std::string& name, int clip);
        Object* o = (!g_sessionAnimObject.empty() && SceneCollection) ? FindObjectByName(SceneCollection, g_sessionAnimObject) : NULL;
        if (!AnimSelRestore(g_sessionAnimType, o, g_sessionAnimObject, g_sessionAnimClip))
            w3dLogfW("[W3D] sesion: la animacion de '%s' ya no esta -> queda la de escena", g_sessionAnimObject.c_str());
    }
#endif
    CurrentFrame = f;
#ifndef W3D_NO_EDITOR
    // la pose de ESE cuadro ya (como al arrastrar el cabezal): un clip de un juego deja a sus objetos en el estado base
    if (g_sessionAnimType != 0) { extern void ApplyAnimationObjects(); ApplyAnimationObjects(); }
#endif
    DeselectAll();
    ActiveObj = NULL;
    int lost = 0;
    for (size_t i = 0; i < g_sessionSelection.size(); i++) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, g_sessionSelection[i]) : NULL;
        if (o) o->Select();
        else   lost++;
    }
    ActiveObj = (!g_activeSession.empty() && SceneCollection) ? FindObjectByName(SceneCollection, g_activeSession) : NULL;
    if (lost)
        w3dLogfW("[W3D] sesion: %d objeto(s) de la seleccion guardada ya no existen -> se ignoran", lost);
    w3dLogf("[W3D] sesion: frame %d, %d objeto(s) seleccionados, activo '%s'",
            CurrentFrame, (int)ObjSelects.size(), ActiveObj ? ActiveObj->name.c_str() : "");
    g_redraw = true;
}

// MULTI-ESCENA: la escena que ARRANCA + el modo. Sin esto, con varias UI el
// motor no sabia cual es la inicial (arrancaba la primera por orden).
static void ApplyInitialScene(const std::string& n) { W3dSceneSetInitial(n); }
static void ApplySceneMode(bool v) { W3dSceneSetMode(v); }

// ICONO del juego: ruta relativa a la carpeta del .w3d (externa SIEMPRE).
// Queda como icono del PROYECTO (g_projIcon: tarjeta Juego / Compilar juego)
// y como icono de la VENTANA. Vacio o ilegible -> el icono default de Whisk3D.
// 'resuelta' ya paso por JsonPath: en v4 es la ENTRADA del contenedor
// ("project/icono.png"), en v3 la ruta de disco. g_projIcon guarda ESO, que es
// lo que sabe leer ReadFileBytes y lo que el guardado vuelve a meter adentro.
static void ApplyIcon(const std::string& resolved) {
#if defined(W3D_NO_EDITOR) || defined(W3D_SYMBIAN)
    // el juego compilado ya lleva SU icono (lo genera "Compilar juego" y lo pone
    // el main.cpp generado con SDL_SetWindowIcon): el del proyecto no aplica.
    (void)resolved;
#else
    const std::string& rel = resolved;
    g_projIcon = rel;
    std::string path;
    if (!rel.empty()) {
        path = rel;
        if (!w3dFileSystem::FileExists(path)) {
            w3dLogfW("[W3D] icono no encontrado: %s (queda el default)", path.c_str());
            path.clear();
        }
    }
    if (path.empty()) path = w3dFileSystem::GetResDir() + "/Whisk3D.png";
    SDL_Surface* icon = LoadSurfaceSTB(path);
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
// como UI::fileW3dui CON su subcarpeta ("content/menu.w3dui") para que el
// re-guardado escriba la escena EN EL MISMO lugar (UI2DLoad solo recuerda el
// nombre pelado y una escena bajo content/ se re-guardaba en la raiz).
// ESCENAS UI QUE NO SE PUDIERON CARGAR EN ESTA APERTURA (vacio = ninguna). Tercer miembro de
// la misma familia que W3dForeign::notLoaded (malla) y VertexAnimation::notLoaded (vertex anim):
// el nodo NO se crea, asi que el proximo guardado escribe un project.json SIN esa escena y
// el usuario la pierde de vista sin enterarse (los bytes del .w3dui sobreviven por
// PreserveTransient, pero ya no los referencia nadie: son un huerfano invisible). Con esto
// prendido el guardado se FRENA y el .w3d de origen no se toca. Lo limpia RestartScene.
std::string g_w3dUINotLoaded;

static UI* LoadUIProject(const std::string& path, const std::string& refRel = std::string()) {
    if (path.empty()) {
        w3dLogfW("[W3D] nodo UI sin 'archivo': escena omitida");
        return NULL;
    }
    if (!w3dFileSystem::FileExists(path)) {
        w3dLogfE("[W3D] falta la escena UI %s: se omite (el resto del proyecto abre)", path.c_str());
        if (g_w3dUINotLoaded.empty()) g_w3dUINotLoaded = path;
        W3dAvisof(true, T("The UI scene '%s' is missing: the project opens without it and I will NOT save over it (see whisk3d.log)"),
                  W3dShortName(path).c_str());
        return NULL;
    }
    UI* u = UI2DLoad(path);
    if (!u) {
        w3dLogfE("[W3D] la escena UI %s no parsea: se omite", path.c_str());
        if (g_w3dUINotLoaded.empty()) g_w3dUINotLoaded = path;
        W3dAvisof(true, T("The UI scene '%s' could not be read: the project opens without it and I will NOT save over it (see whisk3d.log)"),
                  W3dShortName(path).c_str());
        return NULL;
    }
    if (!refRel.empty() && refRel[0] != '/' && !(refRel.size() > 1 && refRel[1] == ':'))
        u->fileW3dui = refRel;
    return u;
}

// el LAYOUT desde su forma de TEXTO "Layout { ... }" (el arbol VIVO serializado
// por el guardado, ver LayoutSerializeLive en SaveW3D.cpp): se reconstruye
// con el pipeline probado del formato viejo (Tokenize + ParseNode + BuildLayout).
// Un nombre corto ("2d", archivos guardados antes de que el layout vivo se
// persistiera) o vacio cae al template estandar de siempre.
#ifndef W3D_NO_EDITOR
// un arbol NUEVO de viewports desde el texto "Layout { ... }" (los layouts guardados del programa, el de cada orientacion
// del telefono). No toca rootViewport: lo instala quien lo pide. NULL = el texto no arma nada
ViewportBase* W3dLayoutFromText(const std::string& text) {
    if (text.compare(0, 6, "Layout") != 0) return NULL;
    std::vector<std::string> tokens = Tokenize(text.c_str());
    size_t i = 0;
    Node* layout = ParseNode(tokens, i);
    ViewportBase* r = (layout && !layout->children.empty()) ? BuildLayout(layout->children[0]) : NULL;
    FreeNode(layout);
    return r;
}
#endif
static void ApplyLayoutText(const std::string& which) {
#ifdef W3D_NO_EDITOR
    (void)which;   // el juego compilado no tiene layout de viewports (ver W3D_NO_EDITOR arriba)
#else
    const char* text =
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
    if (which.compare(0, 6, "Layout") == 0)
        text = which.c_str();   // layout VIVO guardado en el archivo
    std::vector<std::string> tokens = Tokenize(text);
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
		// viejas). Los duplicados los repara de una sola vez W3dNamesRepairScene al
		// final de OpenW3D, que ademas arrastra las refs de scripts y los targets.
		obj->SetRawName(w3dMapAt(p, "name"));
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

    
    if(p.count("expanded")) obj->expanded = B("expanded");
    // el "ojito" del outliner: el formato de texto no lo escribia nunca, pero un .w3d
    // a mano tiene que poder ocultar un objeto (mismo idioma que el JSON v4)
    if(p.count("visible")) obj->visible = B("visible");
    if(p.count("static")) obj->isStatic = B("static"); // GridCull: false = dinamico (se re-evalua su celda por frame)
    if(p.count("showRelationshipsLines")) obj->showRelationshipsLines = B("showRelationshipsLines");

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
//  al final MigrateOldConstraints reparte las propiedades y lo saca de la escena
//  si no tiene hijos.
//
//  LOS DEFAULTS DE CADA FORMATO NO SE UNIFICAN: el texto trae 'usePitch' con
//  default true y el JSON 'pitch' con default false. El default es parte del
//  formato historico de cada uno; unificarlo cambiaria como abren archivos que hoy
//  se ven bien.
// ===========================================================================
struct OldPendCons {
    Object*     node;        // el Empty que ocupa el lugar del nodo viejo
    std::string target;      // a QUIEN modificaba: ahi es donde va a vivir el constraint
    bool        horizontal;  // useHorizontal / "horizontal"  -> Billboard bbYaw
    bool        pitch;       // usePitch      / "pitch"       -> Billboard bbPitch
    bool        copyX, copyY, copyZ;   // copyPos* -> Copy Location con fuente = la vista
};
static std::vector<OldPendCons> gOldPendCons;

// LA MISMA mecanica para los dos lectores, para que no diverjan: el Empty + la
// entrada pendiente. El que llama le aplica despues el nombre y la transform por
// el camino comun de su formato (ApplyCommonProps / CommonJson).
static Object* NodeOldConstraint(Object* parent, const std::string& target,
                                   bool horizontal, bool pitch,
                                   bool copyX, bool copyY, bool copyZ) {
    Empty* e = new Empty(parent);
    OldPendCons p;
    p.node = e;              p.target = target;
    p.horizontal = horizontal; p.pitch = pitch;
    p.copyX = copyX; p.copyY = copyY; p.copyZ = copyZ;
    gOldPendCons.push_back(p);
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
//  se comparten por ruta (MeshData/AnimData), asi que N clones = N nodos
//  livianos + 1 juego de datos.
//
//  La Biblioteca tiene que aparecer ANTES que sus Clones (el archivo se
//  construye de arriba hacia abajo). Sin override de name, el clon recibe el
//  nombre unico automatico (N clones homonimos romperian las refs).
//
//  El bind lua `instanciar("caja", x, y, z)` (Core) entra por el hook
//  W3dInstantiatePrefabHook, que se instala al cargar una Biblioteca; los
//  clones runtime cuelgan de la raiz de la escena. Coordenadas del MOTOR.
//
//  V4: el guardado materializa los clones como objetos completos (la fuente
//  de verdad del dedup declarativo es el .w3d de texto que emite la build del
//  proyecto). Los prefabs viven mientras vive el proyecto: RestartScene
//  los limpia.
// ===========================================================================
static void ReadPropScripts(Object* obj, const std::map<std::string,std::string>& p); // definida abajo

static std::map<std::string, Node*> gPrefabs;   // nombre -> COPIA propia del nodo Prefab

static Node* PrefabCopyNode(const Node* n) {
    Node* c = new Node();
    c->type = n->type;
    c->props = n->props;
    for (size_t i = 0; i < n->children.size(); i++)
        if (n->children[i]) c->children.push_back(PrefabCopyNode(n->children[i]));
    return c;
}
static void PrefabDeleteNode(Node* n) {
    if (!n) return;
    for (size_t i = 0; i < n->children.size(); i++) PrefabDeleteNode(n->children[i]);
    delete n;
}
void W3dPrefabsClear() {
    for (std::map<std::string, Node*>::iterator it = gPrefabs.begin(); it != gPrefabs.end(); ++it)
        PrefabDeleteNode(it->second);
    gPrefabs.clear();
}
int W3dPrefabsRegistered() { return (int)gPrefabs.size(); }

// instancia el prefab bajo 'parent': overrides = props del Clon (pisan la raiz),
// childrenExtra = hijos propios del Clon (se agregan). NULL si no hay tal prefab.
static Object* InstantiatePrefabNode(const std::string& name,
                                    const std::map<std::string,std::string>& overrides,
                                    const std::vector<Node*>* childrenExtra,
                                    Object* parent) {
    std::map<std::string, Node*>::iterator it = gPrefabs.find(name);
    if (it == gPrefabs.end() || !it->second) {
        std::cerr << "[Clon] no hay un Prefab '" << name << "' en la Biblioteca\n";
        return NULL;
    }
    Node* root = NULL;
    for (size_t i = 0; i < it->second->children.size() && !root; i++)
        root = it->second->children[i];
    if (!root) {
        std::cerr << "[Clon] el Prefab '" << name << "' esta vacio\n";
        return NULL;
    }
    Node tmp;
    tmp.type  = root->type;
    tmp.props = root->props;
    tmp.props.erase("name");   // sin override, nombre unico automatico
    for (std::map<std::string,std::string>::const_iterator kv = overrides.begin();
         kv != overrides.end(); ++kv)
        if (kv->first != "prefab") tmp.props[kv->first] = kv->second;
    tmp.children = root->children;
    if (childrenExtra)
        for (size_t i = 0; i < childrenExtra->size(); i++)
            if ((*childrenExtra)[i]) tmp.children.push_back((*childrenExtra)[i]);
    // el MISMO camino que BuildObjectRecursive, pero devolviendo la raiz creada
    Object* obj = CreateObjectFromNode(&tmp, parent);
    if (!obj) return NULL;
    ApplyCommonProps(obj, tmp.props);
    ReadPropScripts(obj, tmp.props);
    for (size_t i = 0; i < tmp.children.size(); i++)
        if (tmp.children[i]) BuildObjectRecursive(tmp.children[i], obj);
    return obj;
}

// el hook del bind lua instanciar() (Core). Coordenadas del MOTOR -> props del
// formato de texto (la vertical del texto es z y la profundidad y).
static Object* HookInstantiatePrefab(const char* name, const float* posMotor) {
    if (!name || !*name) return NULL;
    std::map<std::string,std::string> ov;
    if (posMotor) {
        char b[48];
        snprintf(b, sizeof(b), "%g", posMotor[0]); ov["x"] = b;
        snprintf(b, sizeof(b), "%g", posMotor[2]); ov["y"] = b;   // profundidad
        snprintf(b, sizeof(b), "%g", posMotor[1]); ov["z"] = b;   // vertical
    }
    return InstantiatePrefabNode(name, ov, NULL, SceneCollection);
}

Object* CreateObjectFromNode(Node* n, Object* parent){
    if(!n) return NULL;

    const std::map<std::string,std::string>& p = n->props;

    // --- Biblioteca de PREFABS: declara subarboles SIN instanciarlos ---
    if (n->type == "Library") {
        for (size_t i = 0; i < n->children.size(); i++) {
            Node* c = n->children[i];
            if (!c || c->type != "Prefab") continue;
            std::string name = c->props.count("name") ? Unquote(w3dMapAt(c->props, "name")) : std::string();
            if (name.empty()) { std::cerr << "[Biblioteca] Prefab sin name (ignorado)\n"; continue; }
            std::map<std::string, Node*>::iterator old = gPrefabs.find(name);
            if (old != gPrefabs.end()) { PrefabDeleteNode(old->second); gPrefabs.erase(old); }
            gPrefabs[name] = PrefabCopyNode(c);
        }
        W3dInstantiatePrefabHook = HookInstantiatePrefab;   // habilita instanciar() en lua
        return NULL;   // la Biblioteca no agrega objetos a la escena
    }
    // --- Clon: instancia un Prefab de la Biblioteca con overrides ---
    if (n->type == "Clone") {
        if (!n->props.count("prefab")) { std::cerr << "[Clon] falta prefab:\n"; return NULL; }
        InstantiatePrefabNode(Unquote(w3dMapAt(n->props, "prefab")), n->props, &n->children, parent);
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
        // escenario CERRADO a edicion (sin bordes/edges al importar; Tab lo ignora)
        bool noEditable = GetBoolOrDefault(n->props, "noEditable", false);

        // --- IMPORTACIÓN ---
        Mesh* mesh = ImportWOBJ(path, parent, NoMerge, noEditable);

        if (!mesh){
            std::cerr << "[Wobj] Se importo mal el wobj!\n";
            return NULL;
        }
        mesh->onlyRoutine = GetBoolOrDefault(n->props, "routineOnly", false);   // la dibuja una rutina (Mesh.h)
        mesh->lockNormals = GetBoolOrDefault(n->props, "lockNormals", false);


        for (size_t _ci=0; _ci<n->children.size(); _ci++) {
            Node* child = n->children[_ci];
            if (child->type == "Animation") {
                std::map<std::string,std::string>& ap = child->props;

                bool UseNormals = GetBoolOrDefault(ap, "UseNormals", false);

                bool repeat = GetBoolOrDefault(ap, "repeat", true);

                int speed = GetIntOrDefault(ap, "speed", 1);

                int nextAnimation = GetIntOrDefault(ap, "nextAnimation", -1);

                // tolerante como el resto del lector: un archivo viejo sin estos
                // campos NO tira excepcion (at()/stoi lanzan y mataban el editor)
                VertexAnimation* anim =
                    new VertexAnimation(NULL, ap.count("name") ? w3dMapAt(ap, "name") : std::string("anim"),
                                        UseNormals, (float)speed, repeat, nextAnimation);

                anim->basePath   = ap.count("basePath") ? GetFilePath(w3dMapAt(ap, "basePath")) : std::string();
                anim->frameCount = GetIntOrDefault(ap, "frames", 0);
                anim->padding    = GetIntOrDefault(ap, "padding", 0);

                NewActiveVertexAnimation(mesh, anim);
            }
            // ANIMACION UV "tira de atlas" (Core, autoplay: ver struct UVAnimTira en Mesh.h).
            //   AnimacionUV { frames: 14  fps: 30  eje: u  desfase: 0  ancho: 0.5 }
            // La textura es una TIRA de 'frames' celdas sobre 'eje'; el motor la recorre solo.
            // `ancho`/`alto` (default 1): fraccion de la textura que ocupa la tira -- con la
            // tira metida en el ATLAS UNICO el paso por celda es ancho/frames, no 1/frames.
            if (child->type == "AnimationUV") {
                std::map<std::string,std::string>& up = child->props;
                std::string axis = up.count("axis") ? Unquote(w3dMapAt(up, "axis")) : std::string("u");
                mesh->SetUVAnimStrip(GetIntOrDefault(up, "frames", 1),
                                    GetFloatOrDefault(up, "fps", 30.0f),
                                    (axis == "v") ? 1 : 0,
                                    GetIntOrDefault(up, "offset", 0),
                                    GetFloatOrDefault(up, "width", 1.0f),
                                    GetFloatOrDefault(up, "height", 1.0f));
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
            if (child->type == "CullingTri" || child->type == "Occlusion") {   // "Oclusion" = el buen nombre; CullingTri sigue por retrocompat
                std::map<std::string,std::string>& cp = child->props;
                extern void W3dModPVSAdd(Mesh*, const std::string&, int, const std::string&, int,
                                             const std::string&, bool); // edit/MeshEdit.cpp
                std::string method = cp.count("method") ? Unquote(w3dMapAt(cp, "method")) : std::string("triangles");
                int sector = GetIntOrDefault(cp, "sector", GetIntOrDefault(cp, "cell", 0));
                std::string file = cp.count("vis") ? Unquote(w3dMapAt(cp, "vis"))
                                    : (cp.count("pvs") ? Unquote(w3dMapAt(cp, "pvs")) : std::string());
                int fallback = 0;
                if (cp.count("sectorFallback")) {
                    std::string fs = Unquote(w3dMapAt(cp, "sectorFallback"));
                    fallback = (fs == "complete") ? -1 : GetIntOrDefault(cp, "sectorFallback", 0);
                }
                // path: objeto del recorrido (Curve o malla de aristas) -> el motor elige la celda solo
                std::string path = cp.count("path") ? Unquote(w3dMapAt(cp, "path")) : std::string();
                bool onlyCam = GetBoolOrDefault(cp, "activeCameraOnly", true);
                W3dModPVSAdd(mesh, method, sector, file, fallback, path, onlyCam);
                if (cp.count("branches")) {   // "1101" = rama 2 apagada (filtro del nodo-mas-cercano)
                    extern void W3dModPVSBranches(Mesh*, const std::string&);
                    W3dModPVSBranches(mesh, Unquote(w3dMapAt(cp, "branches")));
                }
            }
        }

        LoadVertexFrames(mesh);
        { extern void W3dPVSSync(Mesh*);   // PVS: carga el sidecar + arma el sector declarado
          W3dPVSSync(mesh); }

        return mesh;
    }

    if (n->type=="UI"){
        // la INTERFAZ del proyecto: el .w3d referencia un .w3dui EXTERNO (el
        // arbol 2D completo, con sus scripts). Mismo camino que el JSON nuevo.
        if (!n->props.count("file")) {
            w3dLogfW("[W3D] nodo UI sin 'archivo': escena omitida");
            return NULL;
        }
        return (Object*)LoadUIProject(GetFilePath(w3dMapAt(n->props, "file")),
                                         Unquote(w3dMapAt(n->props, "file")));
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
        if(p.count("targets")) mirror->SetTargetsText(Unquote(w3dMapAt(p, "targets")));
        mirror->mirrorX = GetBoolOrDefault(p, "mirrorX", mirror->mirrorX);
        mirror->mirrorY = GetBoolOrDefault(p, "mirrorY", mirror->mirrorY);
        mirror->mirrorZ = GetBoolOrDefault(p, "mirrorZ", mirror->mirrorZ);
        // RECORTE DEL REFLEJO (ver Mirror.h): el rectangulo se mide en el PLANO
        // del espejo, sobre AxisU()/AxisV(). Claves nuevas limiteU0/U1/V0/V1.
        // Las cuatro viejas (limiteIzquierdo/Derecho/Fondo/Frente, que estaban
        // tomadas prestadas del vocabulario de un objeto de fisica y hablaban de
        // X/Z de mundo) se siguen leyendo como ALIAS: para el espejo horizontal
        // sin rotar, U = X y V = Z, asi que significan exactamente lo mismo.
        mirror->noDepth = GetBoolOrDefault(p, "noDepth", false);
        // sinkMax: DEPRECADO (ver Mirror.h). Se acepta para que el
        // archivo abra, se avisa por log, y NO tiene ningun efecto: la
        // reflexion es geometrica pura y el recorte es la silueta del agua.
        mirror->sinkMax = GetFloatOrDefault(p, "maxSink", 0.0f);
        if (mirror->sinkMax > 0.0f)
            w3dLogfW("[W3D] Mirror '%s': hundimientoMaximo esta DEPRECADO y no tiene "
                     "efecto (la reflexion es geometrica pura; el recorte del reflejo "
                     "es la silueta del agua en pantalla)", mirror->name.c_str());
        if (p.count("limitU0") && p.count("limitU1") &&
            p.count("limitV0") && p.count("limitV1")) {
            mirror->usesLimits = true;
            mirror->limU0 = GetFloatOrDefault(p, "limitU0", 0);
            mirror->limU1 = GetFloatOrDefault(p, "limitU1", 0);
            mirror->limV0 = GetFloatOrDefault(p, "limitV0", 0);
            mirror->limV1 = GetFloatOrDefault(p, "limitV1", 0);
        } else if (p.count("leftLimit") && p.count("rightLimit") &&
                   p.count("backLimit") && p.count("frontLimit")) {
            mirror->usesLimits = true;
            mirror->limU0 = GetFloatOrDefault(p, "leftLimit", 0);
            mirror->limU1 = GetFloatOrDefault(p, "rightLimit", 0);
            mirror->limV0 = GetFloatOrDefault(p, "backLimit", 0);
            mirror->limV1 = GetFloatOrDefault(p, "frontLimit", 0);
        }
        // recorte del reflejo (default = hay limites). Solo se lee si viene.
        if (p.count("clip")) {
            mirror->clipDeclared = true;
            mirror->clip = GetBoolOrDefault(p, "clip", true);
        }
        return mirror;
    }

    if (n->type=="LOD"){
        // LOD { distancias: "20, 45, 90" <hijos en orden de detalle> }
        // umbral i = hasta donde llega el hijo i; mas alla del ultimo, el ultimo hijo.
        LOD* lod = new LOD(parent);
        if (p.count("distances")) lod->SetDistancesText(Unquote(w3dMapAt(p, "distances")));
        lod->onlyCameraActive = GetBoolOrDefault(p, "activeCameraOnly", false);
        return lod; // los hijos los cuelga BuildObjectRecursive
    }

    if (n->type=="Culling"){
        // Culling { activo: true|false onlyCameraActive: true|false distanceMax: 0 <hijos> }
        // (frustum culling por AABB + culling por distancia opcional; ver Culling.h)
        Culling* cu = new Culling(parent);
        // metodo de culling (frustum/grid/triangulo/bsp): ausente = frustum (retrocompat .w3d viejos)
        cu->method           = p.count("method") ? CullingMethodFrom(w3dMapAt(p, "method")) : (int)Culling::Frustum;
        cu->active           = GetBoolOrDefault(p, "active", true);   // ausente = prendido
        cu->onlyCameraActive = GetBoolOrDefault(p, "activeCameraOnly", false);
        cu->distanceMax      = GetFloatOrDefault(p, "maxDistance", 0.0f);
        cu->orderAlpha       = GetBoolOrDefault(p, "alphaOrder", false); // translucido: cullea + ordena back->forward
        cu->cellSize         = GetFloatOrDefault(p, "cellSize", 16.0f);  // metodo grid: lado de celda
        cu->mode3D           = GetBoolOrDefault(p, "mode3D", false);     // metodo grid: 2D XZ vs 3D
        // metodo riel: la Curve del recorrido + el .w3dvis de HIJOS visibles por nodo
        if (p.count("rail"))     cu->railName      = Unquote(w3dMapAt(p, "rail"));
        if (p.count("visChildren")) cu->visChildrenFile = Unquote(w3dMapAt(p, "visChildren"));
        return cu;
    }

    if (n->type=="GridCull"){
        // COMPAT: el nodo GridCull viejo ahora es un Culling con metodo=Grid (el objeto se unifico).
        Culling* g = new Culling(parent);
        g->method           = Culling::Grid;
        g->active           = GetBoolOrDefault(p, "active", true);
        g->cellSize         = GetFloatOrDefault(p, "cellSize", 16.0f);
        g->mode3D           = GetBoolOrDefault(p, "mode3D", false);
        g->distanceMax      = GetFloatOrDefault(p, "maxDistance", 0.0f);
        g->onlyCameraActive = GetBoolOrDefault(p, "activeCameraOnly", false);
        g->orderAlpha       = GetBoolOrDefault(p, "alphaOrder", false); // celdas de lejos a cerca (transparencias)
        return g;
    }

    if (n->type=="Particles"){
        // Particulas { textura: "polvo.png" cantidad: 20 vida: 1.2 tam: 0.4 vel: 2
        //              dispersion: 30 gravedad: 4 aditivo: false color: "1, 1, 1, 1"
        //              desvanecer: true activo: true }   (ver Particles.h)
        Particles* pt = new Particles(parent);
        // la textura se resuelve contra la carpeta del .w3d (como filePath del Wavefront)
        if (p.count("texture")) pt->texture = GetFilePath(w3dMapAt(p, "texture"));
        pt->count      = GetFloatOrDefault(p, "count",   pt->count);
        pt->life       = GetFloatOrDefault(p, "life",       pt->life);
        pt->size       = GetFloatOrDefault(p, "size",        pt->size);
        pt->vel        = GetFloatOrDefault(p, "vel",        pt->vel);
        pt->dispersion = GetFloatOrDefault(p, "dispersion", pt->dispersion);
        pt->gravity    = GetFloatOrDefault(p, "gravity",   pt->gravity);
        // MEZCLA: el .w3d nuevo trae "mezcla" (int, w3dEngine::Mezcla); los viejos, aditivo/sustractivo (bool)
        if (p.count("mix"))                              pt->mix = GetIntOrDefault(p, "mix", pt->mix);
        else if (GetBoolOrDefault(p, "sustractivo", false)) pt->mix = w3dEngine::MixSubtract;
        else if (GetBoolOrDefault(p, "additive", false))     pt->mix = w3dEngine::MixAdd;
        if (p.count("color")) pt->SetTextColor(Unquote(w3dMapAt(p, "color")));
        pt->fade     = GetBoolOrDefault(p, "fade",  pt->fade);
        pt->active   = GetBoolOrDefault(p, "active",      pt->active);
        pt->filtered = GetBoolOrDefault(p, "filtered",    pt->filtered);   // ausente -> true (suave)
        // azar (defaults 0 = sin jitter ni deriva: los archivos viejos no cambian)
        pt->variation  = GetFloatOrDefault(p, "variation",   pt->variation);
        pt->turbulence = GetFloatOrDefault(p, "turbulence", pt->turbulence);
        pt->emissionRadius = GetFloatOrDefault(p, "emissionRadius", pt->emissionRadius);
        // rotacion del billboard (defaults false/0 = derecho y quieto, como antes)
        pt->rotation    = GetBoolOrDefault (p, "rotation",    pt->rotation);
        pt->velRotation = GetFloatOrDefault(p, "rotationSpeed", pt->velRotation);
        pt->flipFrames  = GetIntOrDefault(p, "flipFrames", pt->flipFrames);   // flipbook por edad
        pt->flipCols    = GetIntOrDefault(p, "flipCols",    pt->flipCols);
        pt->flipRows    = GetIntOrDefault(p, "flipRows",   pt->flipRows);
        // SUB-RECT del atlas unico: "u0, v0, u1, v1" (ausente = textura entera)
        if (p.count("uvRect")) {
            std::string s = Unquote(w3dMapAt(p, "uvRect"));
            float r[4] = { 0, 0, 1, 1 };
            if (sscanf(s.c_str(), "%f , %f , %f , %f", &r[0], &r[1], &r[2], &r[3]) == 4)
                for (int k = 0; k < 4; k++) pt->uvRect[k] = r[k];
        }
        return pt;
    }

    if (n->type=="VisZone"){
        // VisZone { modo: manual|grilla|volumenes|curva  objetivo: "Escenario"
        //           ancla: "Jugador"  nx: 8 ny: 1 nz: 8  cellSize: 4.0  stepMax: 32
        //           riel: "RielPueblo"  fallback: "completa" }
        // decide la CELDA de visibilidad del objetivo (ver VisZone.h); en modo
        // volumenes sus HIJOS son los volumenes (los cuelga BuildObjectRecursive).
        // `riel:` (alias `curva:`, modo curva) = la Curve cuyos nodos indexan las
        // celdas: con la camara en OTRO riel la zona cae a `fallback:` ("completa"
        // = malla entera, N = celda N). Sin `riel:` nada cambia (legacy).
        VisZone* vz = new VisZone(parent);
        std::string mode = p.count("mode") ? Unquote(w3dMapAt(p, "mode")) : std::string("manual");
        vz->mode = (mode == "grid") ? VisZone::ModeGrid
                 : (mode == "volumes") ? VisZone::ModeVolumes
                 : (mode == "curve") ? VisZone::ModeCurve : VisZone::ModeManual;
        if (p.count("target")) vz->targetName = Unquote(w3dMapAt(p, "target"));
        if (p.count("anchor"))    vz->anchorName    = Unquote(w3dMapAt(p, "anchor"));
        vz->nx       = GetIntOrDefault(p, "nx", 1);
        vz->ny       = GetIntOrDefault(p, "ny", 1);
        vz->nz       = GetIntOrDefault(p, "nz", 1);
        vz->cellSize = GetFloatOrDefault(p, "cellSize", 1.0f);
        vz->stepMax  = GetIntOrDefault(p, "maxStep", 32);
        if (p.count("rail"))       vz->railName = Unquote(w3dMapAt(p, "rail"));
        else if (p.count("curve")) vz->railName = Unquote(w3dMapAt(p, "curve"));
        if (p.count("fallback")) {
            std::string fb = Unquote(w3dMapAt(p, "fallback"));
            vz->cellFallback = (fb == "complete") ? 0 : GetIntOrDefault(p, "fallback", 0);
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
        W3dAvisof(false, T("The Script object '%s' no longer exists: it was migrated to an empty object with its "
                           "scripts. Its hardcoded physics (speed/gravity/limits) is NOT migrated."),
                  p.count("name") ? Unquote(w3dMapAt(p, "name")).c_str() : T("(unnamed)"));

        // el SCRIPT declarado con `script:` lo levanta ReadPropScripts (en
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
                if (!gamepad->scriptData) gamepad->scriptData = new W3dScriptData();
                W3dScriptEntry e; e.path = lua;
                gamepad->scriptData->scripts.push_back(e);
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
            curve->RecalculateBranches();   // ramas (componentes) para el filtro del modificador Oclusion
            // LISTA DE CARGA (streaming por riel): sidecar declarado como hijo
            //     Curve { filePath: "riel.cap"  ListaCarga { filePath: "riel.cargas.json" } }
            for (size_t _i=0; _i<n->children.size(); _i++) {
                Node* c = n->children[_i];
                if (!c || c->type != "LoadList") continue;
                if (!c->props.count("filePath")) { std::cerr << "[ListaCarga] falta filePath\n"; continue; }
                curve->LoadLoadList(GetFilePath(w3dMapAt(c->props, "filePath")));
            }
            return curve;
        }
        // fallo la carga: el ctor ya lo colgo del padre -> sacarlo del arbol y
        // liberarlo (antes quedaba una Curve vacia fantasma en la escena). LOG del fallo: sin esto la Curve del
        // riel desaparecia en SILENCIO -> la camara con riel se quedaba en el origen sin ninguna pista del por que.
        w3dLogfW("[W3D] no pude cargar la curva/riel '%s' (queda sin riel: la camara que dependa de el no se posiciona)", path.c_str());
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
        // al objeto APUNTADO al final de la carga (ver OldPendCons, arriba).
        // OJO: en el formato de TEXTO 'usePitch' tiene default TRUE (en el JSON es false).
        return NodeOldConstraint(parent,
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
        if(p.count("rail")) camera->SetRail(w3dMapAt(p, "rail"));
        if(p.count("railOffset")) camera->offsetRail = GetIntOrDefault(p, "railOffset", 0);
        // MIRADA DEL RIEL: la orientacion sale del yaw/pitch AUTORAL del nodo en vez del
        // look-at al target (ver Camera.h). Ausente = false = comportamiento de siempre.
        if(p.count("railLook")) camera->useRailRotation = GetBoolOrDefault(p, "railLook", false);
        // LENTE: la rama JSON ya leia los tres ("fov"/"near"/"far", JsonObjectCreate) pero
        // el formato texto solo el fov, y una camara con planos de recorte propios quedaba
        // clavada en los defaults (0.01 / 1000). Los tres se pueden ademas leer y escribir
        // desde lua con lenteDe() / setLente().
        if(p.count("fov"))  camera->fov      = GetFloatOrDefault(p, "fov", camera->fov);
        if(p.count("near")) camera->nearClip = GetFloatOrDefault(p, "near", camera->nearClip);
        if(p.count("far"))  camera->farClip  = GetFloatOrDefault(p, "far", camera->farClip);
        // ENCUADRE DECLARADO (aspecto de imagen, ej 1.481481 = 40:27): el modo juego
        // dibuja ESE frustum y letterboxea el resto (ver Camera::aspecto). Ausente =
        // 0 = como siempre (o heredado de la cabecera `aspecto` del .cap del riel).
        if(p.count("aspect")) camera->aspect = GetFloatOrDefault(p, "aspect", 0.0f);
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
        // Collection { sortByCamera: true|false sortOnce: true|false
        //              lote: "estatico" <hijos> }
        // (orden far->near para transparentes + lote estatico; ver Collection.h)
        Collection* C = new Collection(parent);
        if(!CollectionActive) CollectionActive = C;
        C->sortByCamera = GetBoolOrDefault(p, "sortByCamera", false);
        C->sortOnce     = GetBoolOrDefault(p, "sortOnce", false);
        C->batch = (p.count("batch") && Unquote(w3dMapAt(p, "batch")) == "static") ? 1 : 0;
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
//  Los tres prefijos viajan por el MISMO canal (W3dScriptEntry.refs, par
//  nombre -> texto): quien decide que es cada par es el .lua al leerlo con
//  object()/option()/property(). Antes esto vivia solo en la rama Gamepad; la
//  convencion <proyecto>.lua (sin `script:`) SI sigue siendo solo del Gamepad.
// ===========================================================================
static void ReadPropScripts(Object* obj, const std::map<std::string,std::string>& p){
    if (!obj || !p.count("script")) return;
    if (!obj->scriptData) obj->scriptData = new W3dScriptData();
    W3dScriptEntry e;
    e.path = GetFilePath(w3dMapAt(p, "script"));
    for (std::map<std::string,std::string>::const_iterator it = p.begin(); it != p.end(); ++it)
        if (it->first.compare(0, 4, "ref_") == 0 ||
            it->first.compare(0, 4, "opt_") == 0 ||
            it->first.compare(0, 4, "val_") == 0)
            e.refs.push_back(std::make_pair(it->first.substr(4), Unquote(it->second)));
    obj->scriptData->scripts.push_back(e);
}

// PROGRESO de la carga de TEXTO (Crash): el camino de texto NO bombeaba la barra -> saltaba 0->100 y en
// el N95 (carga de minutos) parecia colgada. Se cuenta el total de nodos y cada BuildObjectRecursive avanza
// su fraccion; ademas exporta [g_progObjStart,g_progObjEnd] para que ReadWOBJ bombee DENTRO de una malla
// grande. El throttle del popup (ProgressPopup ~1.5%) acota los clear+swap por mas llamadas que se hagan.
size_t g_bsTotal = 0, g_bsDone = 0;
float  g_progObjStart = 0.30f, g_progObjEnd = 0.95f;
static size_t CountNodes(Node* n){
    size_t c = 0;
    for(size_t i=0;i<n->children.size();i++) c += 1 + CountNodes(n->children[i]);
    return c;
}

void BuildObjectRecursive(Node* n, Object* parent){
    extern void ProgressUpdate(float);
    float t0 = g_bsTotal ? (float)g_bsDone / (float)g_bsTotal : 0.0f;
    g_bsDone++;
    float t1 = g_bsTotal ? (float)g_bsDone / (float)g_bsTotal : 1.0f;
    g_progObjStart = 0.30f + 0.65f * t0;
    g_progObjEnd   = 0.30f + 0.65f * t1;
    ProgressUpdate(g_progObjStart);

    const double tMed = LoadNow();   // medicion por fase (0 = nadie mide)
    Object* obj = CreateObjectFromNode(n, parent);
    if(!obj) return;
    // el nodo SOLO (los hijos se miden en su propia vuelta), igual que JsonObject del JSON.
    // El texto viejo no tiene armatures: solo se reparten las mallas.
    if (g_w3dLoadClock && obj->getType() == ObjectType::mesh) {
        g_w3dLoadPhases.meshesMs += LoadNow() - tMed;
        g_w3dLoadPhases.meshes++;
        g_w3dLoadPhases.derivedMeshes++;   // el texto viejo deriva todo al abrir (ImportWOBJ)
    }

    ApplyCommonProps(obj, n->props);
    ReadPropScripts(obj, n->props);   // `script:` + ref_/opt_/val_ (cualquier objeto)

    for(size_t _i=0;_i<n->children.size();_i++)
        BuildObjectRecursive(n->children[_i], obj);
}

void BuildScene(Node* root){
    if(!root || root->type!="Scene") return;

    // TODOS los campos del proyecto van por los MISMOS aplicadores que el JSON
    // (una sola fuente de verdad por campo). "iconApp" es el alias del formato
    // de texto viejo para "icono" (solo existe en este lector).
    ApplyIcon(root->props.count("iconApp") ? Unquote(w3dMapAt(root->props, "iconApp"))
                                              : std::string());

    if(root->props.count("fullscreen")){
        std::string v = w3dMapAt(root->props, "fullscreen");
        ApplyFullscreen(v == "true" || v == "1");
    }

    // el FPS del proyecto (timeline/animaciones): "fps: 60" en la Escena
    ApplyFps(GetIntOrDefault(root->props, "fps", 0));
    // TOPE DE RENDER del proyecto: "fpsCap: 120" en la Escena (distinto de fps=AnimFPS). Symbian lo clampea a 60.
    ApplyFpsCap(GetIntOrDefault(root->props, "fpsCap", 0));

    // MULTI-ESCENA: la escena que ARRANCA + el modo (lo lee Compilar juego)
    if(root->props.count("initialScene"))
        ApplyInitialScene(Unquote(w3dMapAt(root->props, "initialScene")));
    if(root->props.count("sceneMode")){
        std::string v = w3dMapAt(root->props, "sceneMode");
        ApplySceneMode(v == "true" || v == "1");
    }

#ifndef W3D_NO_EDITOR
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
    if(root->props.count("pixelated")){
        std::string v = w3dMapAt(root->props, "pixelated");
        w3dEngine::SetPixelatedGlobal(v == "true" || v == "1");
    }

    // MIPMAPPING del proyecto: sin la prop manda el default del EDITOR (cfg.mipmaps,
    // Ajustes); con `mipmaps:` en la cabecera manda el proyecto ("capaz el editor las
    // tiene pero el proyecto no"). Se aplica ANTES de que la cola diferida suba las
    // texturas, asi que gobierna esta carga.
    w3dEngine::SetMipmapsGlobal(cfg.mipmaps);
    if(root->props.count("mipmaps")){
        std::string v = w3dMapAt(root->props, "mipmaps");
        w3dEngine::SetMipmapsGlobal(v == "true" || v == "1");
    }
    w3dEngine::SetTextures16Always(root->props.count("textures16") &&
                                    (w3dMapAt(root->props, "textures16") == "true" || w3dMapAt(root->props, "textures16") == "1"));
    w3dEngine::SetTextures16Mobile(root->props.count("textures16Mobile") &&
                                  (w3dMapAt(root->props, "textures16Mobile") == "true" || w3dMapAt(root->props, "textures16Mobile") == "1"));
    w3dEngine::SetTexturesHalfFrom(GetIntOrDefault(root->props, "halfTexturesFrom", 0));   // (en el N95)
    // MALLAS SOLO EN GPU (ausente = false): las que solo dibujan rutinas sueltan su RAM al quedar en VBO (MeshResource.h)
    W3dMeshesOnlyGPUActivate(root->props.count("gpuOnlyMeshes") &&
                            (w3dMapAt(root->props, "gpuOnlyMeshes") == "true" || w3dMapAt(root->props, "gpuOnlyMeshes") == "1"));

    // VOLUMEN INICIAL del proyecto (0..1). Vive en el .w3d ("volumen: 0.5") y se aplica SOLO a esta
    // sesion: no pisa el volumen global del usuario ni vale para otros proyectos. Ausente = no se toca.
    if(root->props.count("volume")){
        std::stringstream ss(w3dMapAt(root->props, "volume"));
        float vv = 0.5f; ss >> vv;
        w3dEngine::VolumeApplyProject(vv);
    }

    if(root->props.count("background")){
        std::string bg = Unquote(w3dMapAt(root->props, "background")); // <- quita comillas si las hay
        std::replace(bg.begin(), bg.end(), ',', ' '); // reemplaza comas por espacios
        std::stringstream ss(bg);
        float r,g,b,a;
        if(ss >> r >> g >> b >> a){
#ifndef W3D_NO_EDITOR
            scene->SetBackground(r,g,b,a);   // el objeto Scene vive en el editor
#endif
        } else {
            std::cerr << "Error al parsear background: '" << bg << "'\n";
        }
    }

    g_bsTotal = CountNodes(root);
    g_bsDone  = 0;
    for(size_t _i=0;_i<root->children.size();_i++)
        BuildObjectRecursive(root->children[_i], SceneCollection);  // el parent puede ser la escena global

    if(!CollectionActive) 
        CollectionActive = SceneCollection;        // seguridad
}

// ===========================================================================
//  LECTURA JSON — compartida por el .w3d JSON PLANO nuevo (v3: el .w3d ES el
//  json) y el zip v2 viejo (scene.json adentro). Construye la escena llamando
//  a los MISMOS aplicadores de campos que el formato de texto (ver arriba).
// ===========================================================================
#include "io/JsonW3d.h"
#include "io/W3dZip.h"
#include "objects/Mesh.h"
#include "objects/Instance.h"            // AUDIT versiones: espejo/instancia/curva en el JSON
#include "objects/Armature.h"            // ARMATURES en el .w3d (Fase 3)
#include "animation/Armature2DAnimation.h" // clips del armature 2D ("anims2d")
#include "animation/SkeletalAnimation.h" // clips del esqueleto + PrepareSkin/PrepareAuthoredSkin
#include "animation/W3dAnimSet.h"        // ANIMSETS: los clips en binario (.w3da), compartidos entre armatures
#include "edit/Modifier.h"               // modificador Armature de la malla (referencia por nombre)
#include "edit/MeshEdit.h"               // W3dStackGeneratesMesh: el juego materializa la edicion si el stack genera
#include "edit/WeightPaint.h"            // WeightPaintEnsureMap (reconstruir vertex groups por posicion)
#include "animation/VertexAnimation.h"   // cargar frames de vertices (blob binario) + curvas
#include "script/W3dScript.h"
#include "w3dFilesystem.h"

// carpeta del .w3d ABIERTO. En el JSON plano las rutas relativas cuelgan de aca
// directo; en un zip v2 primero se busca en la carpeta EXTRAIDA y despues aca
// (los assets EXTERNOS del v2 eran relativos al .w3d, no al zip).
static std::string gDirProject;

// v4: el proyecto abierto es un CONTENEDOR y las rutas del JSON son NOMBRES DE
// ENTRADA. Lo prende OpenW3D en el brazo del contenedor y lo apaga al terminar.
static bool gProjectV4 = false;
// true mientras el JUEGO COMPILADO carga su proyecto (W3dProjectLoadScene3D): si la escena
// que arranca no es la del bloque "escena", los objetos del bloque ni se arman (se carga la
// inicial desde su entrada, ver W3dRoots.h)
static bool gLoadGame = false;
// la BASE de las rutas relativas con la que se abrio el proyecto (OpenSceneJson): las escenas y
// prefabs que se cargan despues desde su entrada resuelven sus rutas ("ext:" incluidas) igual
static std::string gBaseProject;

// una ruta del json -> ruta real (LA unica resolucion de referencias del .w3d).
//   "ext:..."   -> EXTERNA deliberada: se saca el prefijo y se resuelve contra la
//                  carpeta del .w3d (absoluta queda tal cual). Queda ANOTADA para
//                  que el proximo guardado la vuelva a escribir externa.
//   v4, pelada  -> es una ENTRADA del contenedor: se devuelve TAL CUAL y
//                  ReadFileBytes la resuelve por el montaje.
//   v3 y viejos -> la regla de siempre: absoluta tal cual; relativa contra 'base'
//                  y, si ahi no existe pero al lado del .w3d si, contra el proyecto.
static std::string JsonPath(const std::string& r, const std::string& base) {
    if (r.empty()) return r;
    // UNA ENTRADA DE UNA LIBRERIA ya nombrada como tal (el proyecto usa una textura o un script de una libreria
    // vinculada): tal cual, la resuelve el almacen de esa libreria (io/W3dStore.h). Adentro del contenido de
    // una libreria, "lib:<otra>/..." nombra una de SU registro (una libreria de adentro): la que le toca
    if (r.compare(0, 4, "lib:") == 0) {
        if (!W3dLibContext().empty()) {
            const std::string an = W3dLibsNameNested(W3dLibContext(), r.substr(4));
            if (!an.empty()) return "lib:" + an;
        }
        return r;
    }
    // CONTENIDO DE UNA LIBRERIA (lo que genera un proxy, io/Libraries.h): sus rutas son entradas de ELLA
    // ("lib:<libreria>/textures/x.png"), y un "ext:" suyo es relativo a SU .w3d (no se anota como externa
    // del proyecto: el proyecto no la guarda). En el JUEGO COMPILADO ese archivo viaja empaquetado con la libreria
    // ("lib:<libreria>/_ext/<rel>", W3dLibsExternalEntry: lo copia Compilar juego)
    {
        const std::string& lib = W3dLibContext();
        if (!lib.empty()) {
            if (r.size() > 4 && r.compare(0, 4, "ext:") == 0) {
                std::string x = r.substr(4);
                if (x.empty() || x[0] == '/' || (x.size() > 2 && x[1] == ':')) return x;
                if (!W3dLibsFromDisk()) return "lib:" + lib + "/" + W3dLibsExternalEntry(x);
                std::string dir = W3dLibsDiskPathOf(lib);
                const size_t b = dir.find_last_of("/\\");
                dir = (b == std::string::npos) ? std::string(".") : dir.substr(0, b);
                return w3dFileSystem::JoinPath(dir, x);
            }
            if (r[0] == '/' || (r.size() > 2 && r[1] == ':')) return r;   // absoluta
            return "lib:" + lib + "/" + r;
        }
    }
    if (r.size() > 4 && r.compare(0, 4, "ext:") == 0) {
        std::string x = r.substr(4);
        std::string res = x;
        if (!(x[0] == '/' || (x.size() > 2 && x[1] == ':')))
            res = (gDirProject.empty() ? base : gDirProject) + "/" + x;
        W3dExternalRefMark(res);
        return res;
    }
    if (r[0] == '/' || (r.size() > 2 && r[1] == ':')) return r;   // absoluta
    if (gProjectV4 && W3dIsEntryName(r)) return r;                // entrada del contenedor
    std::string inBase = base + "/" + r;
    if (!w3dFileSystem::FileExists(inBase) &&
        !gDirProject.empty() && gDirProject != base) {
        std::string inProj = gDirProject + "/" + r;
        if (w3dFileSystem::FileExists(inProj)) return inProj;
    }
    return inBase;
}

// la textura de un paso "Bind texture" de una Rutina: la misma regla que la de un material (ctx = la base)
static std::string RoutineRefTexture(const std::string& saved, void* base) {
    return JsonPath(saved, *(const std::string*)base);
}

// los campos de RUTA que el lector guarda CRUDOS (se resuelven mas tarde, al dibujar o al usarlos: la textura de
// las particulas, los hijos visibles por nodo y el .w3dbsp de un Culling, los sidecars pvs/vis del culling por
// triangulo, el atlas de un flipbook). En el proyecto siguen crudos (como siempre: su guardado los escribe tal
// cual); en el contenido de una LIBRERIA pasan por JsonPath (son entradas de ella: "lib:<libreria>/...")
static std::string JsonPathField(const std::string& r, const std::string& base) {
    return W3dLibContext().empty() ? r : JsonPath(r, base);
}
// las rutas de una ESCENA UI que es contenido de una libreria (su .w3dui): las resuelve JsonPath en el contexto de
// la libreria (UI2DFormat.h: g_w3dPathLibHook). La guarda lo pone mientras se carga y lo saca despues
static std::string PathUIOfLib(const std::string& g) { return JsonPath(g, gBaseProject); }
struct GuardPathUILib {
    W3dLoadPathFn prev;
    bool active;
    explicit GuardPathUILib(bool on) : prev(g_w3dPathLibHook), active(on) { if (active) g_w3dPathLibHook = PathUIOfLib; }
    ~GuardPathUILib() { if (active) g_w3dPathLibHook = prev; }
private:
    GuardPathUILib(const GuardPathUILib&);
    GuardPathUILib& operator=(const GuardPathUILib&);
};
// un NOMBRE de recurso del JSON (una malla, un animset, un flipbook) -> el nombre con el que vive en memoria.
// Adentro de una LIBRERIA lleva su prefijo ("Personajes/Cuerpo": su espacio de nombres); desde el proyecto, uno
// con el prefijo de una libreria vinculada la deja lista (sus recursos se registran recien al usarla)
// (adentro de una libreria, uno con el prefijo de una libreria de SU registro -una de adentro- es de esa)
static std::string ResourceNameJson(const std::string& n) {
    if (n.empty()) return n;
    const std::string& lib = W3dLibContext();
    if (!lib.empty()) {
        if (n.find('/') != std::string::npos) {
            const std::string an = W3dLibsNameNested(lib, n);
            if (!an.empty()) return an;
        }
        return lib + "/" + n;
    }
    if (n.find('/') != std::string::npos) W3dLibsByName(n);
    return n;
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
static int JIRange(JVal* o, const char* k, int def, int lo, int hi, const char* who) {
    int v = JI(o, k, def);
    if (v < lo || v > hi) {
        w3dLogfW("[W3D] %s: '%s' vale %d, fuera de [%d..%d] -> se acota", who, k, v, lo, hi);
        v = (v < lo) ? lo : hi;
    }
    return v;
}

// SANEA una jerarquia de huesos leida del archivo: indices de padre fuera de
// rango, auto-padres y CICLOS quedan como raiz (-1). Sin esto un .w3d a mano
// con "padre" cruzado (0->1, 1->0) cuelga cualquier recorrido que suba por la
// cadena (ej: UVEditor.cpp, borrar huesos). Devuelve cuantos arreglo.
static int SanitizeParents(std::vector<int>& parent, const char* who) {
    const int n = (int)parent.size();
    int fixed = 0;
    for (int i = 0; i < n; i++) {
        if (parent[i] < -1 || parent[i] >= n || parent[i] == i) {
            if (parent[i] != -1) { parent[i] = -1; fixed++; }
            continue;
        }
        int p = parent[i], guard = 0;               // subir: si no llega a una raiz en n pasos, hay ciclo
        while (p >= 0 && guard++ <= n) p = parent[p];
        if (p >= 0) { parent[i] = -1; fixed++; }
    }
    if (fixed)
        w3dLogfW("[W3D] %s: %d hueso(s) con 'padre' invalido o en ciclo -> quedan como raiz", who, fixed);
    return fixed;
}

static void CommonJson(JVal* j, Object* o) {
    o->name = JS(j, "name", o->name);
    o->visible = JB(j, "visible", true);
    o->isStatic = JB(j, "static", true); // GridCull: false = dinamico (se re-evalua su celda por frame)
    o->projectsShadow = JB(j, "castsShadow", true);   // (las sombras en tiempo real)
    o->receivesShadow = JB(j, "receivesShadow", true);
    o->bouncesLight = JB(j, "bouncesLight", true);        // (la luz precalculada: la indirecta)
    o->catchesShadows = JB(j, "catchesShadows", false);  // (captador de sombras)
    o->inProbes = JB(j, "inProbes", true);             // (lo ven las sondas de reflejo y de luz)
    o->showRelationshipsLines = JB(j, "parentLines", true); // ausente = true (archivos viejos)
    o->palette = JS(j, "palette", "");   // seleccion de paleta ("" = hereda del padre)
    JVal* v;
    if ((v = JChild(j, "pos", 5)) && v->list.size() >= 3) {
        o->pos.x = (float)v->list[0]->num; o->pos.y = (float)v->list[1]->num; o->pos.z = (float)v->list[2]->num;
    }
    // ROTACION: el euler del archivo ES el dato (trae sus vueltas) y de el se deriva el quaternion, que es
    // lo que se RENDERIZA. Antes esto escribia los tres floats de rotEuler a mano y no tocaba el quaternion:
    // la escena abria SIN ROTAR ("la camara esta todo en 0 grados"). Va por la puerta (ver Objects.h).
    if ((v = JChild(j, "rot", 5)) && v->list.size() >= 3) {
        o->SetRotEuler(Vector3((float)v->list[0]->num, (float)v->list[1]->num, (float)v->list[2]->num));
    }
    if ((v = JChild(j, "scale", 5)) && v->list.size() >= 3) {
        o->scale.x = (float)v->list[0]->num; o->scale.y = (float)v->list[1]->num; o->scale.z = (float)v->list[2]->num;
    }
    // FISICA de cuerpo rigido (opcional; espejo exacto de EscribirFisica del
    // guardado). Ver physics/W3dRigid.h.
    JVal* jf = JChild(j, "physics", 4);
    if (jf) {
        W3dRigidDef* d = o->physics ? o->physics : new W3dRigidDef();
        const std::string t = JS(jf, "type", "dynamic");
        d->type = (t == "static") ? 0 : (t == "character") ? 2 : (t == "mesh") ? 3 : 1;
        d->mass     = JF(jf, "mass", d->mass);
        d->friction = JF(jf, "friction", d->friction);
        d->bounce   = JF(jf, "bounce", d->bounce);
        if ((v = JChild(jf, "box", 5)) && v->list.size() >= 3)
            for (int k = 0; k < 3; k++) d->box[k] = (float)v->list[k]->num;
        if ((v = JChild(jf, "center", 5)) && v->list.size() >= 3)
            for (int k = 0; k < 3; k++) d->center[k] = (float)v->list[k]->num;
        o->physics = d;
    }
}

// los scripts lua del objeto: [{archivo, refs{}}]
static void JsonScripts(JVal* j, Object* o, const std::string& base) {
    JVal* js = JChild(j, "scripts", 5);
    if (!js) return;
    for (size_t i = 0; i < js->list.size(); i++) {
        JVal* e = js->list[i];
        if (e->type != 4) continue;
        if (!o->scriptData) o->scriptData = new W3dScriptData();
        W3dScriptEntry ent;
        ent.path = JsonPath(JS(e, "file", ""), base);
        ent.off = JB(e, "off", false);   // el ojo de la lista de scripts (ausente = prendido)
        // una UI trae sus scripts en su .w3dui Y el guardado los repite en el nodo del proyecto: sin esto el mismo
        // script quedaba cargado DOS veces (dos instancias corriendo a la par)
        bool repeated = false;
        for (size_t k = 0; k < o->scriptData->scripts.size(); k++)
            if (o->scriptData->scripts[k].path == ent.path) repeated = true;
        if (repeated) continue;
        JVal* refs = JChild(e, "refs", 4);
        if (refs)
            for (std::map<std::string, JVal*>::iterator r = refs->obj.begin(); r != refs->obj.end(); ++r)
                if (r->second->type == 2)
                    ent.refs.push_back(std::make_pair(r->first, r->second->str));
        o->scriptData->scripts.push_back(ent);
    }
}

// UNA curva {p, c, k:[[frame,value,interp,htype,inDF,inDV,outDF,outDV],...]} -> AnimProperty.
// Compartida por las curvas de las vertex anims y por los tracks de los clips de esqueleto (Fase 3).
static bool LoadCurveJson(JVal* cj, AnimProperty& ap) {
    if (!cj || cj->type != 4) return false;
    ap.Property = JI(cj, "p", 0); ap.component = JI(cj, "c", 0);
    { const int ca = JI(cj, "ca", 0), cd = JI(cj, "cd", 0);   // el modificador Repetir (ausente = no)
      ap.cyclePrev  = (ca >= KfCycleNo && ca <= KfCycleOffset) ? ca : KfCycleNo;
      ap.cycleAfter = (cd >= KfCycleNo && cd <= KfCycleOffset) ? cd : KfCycleNo; }
    // la pila de MODIFICADORES (ver WriteCurve): una fila de numeros por modificador; cota de 64
    ap.mods.clear();
    if (JVal* jm = JChild(cj, "m", 5)) {
        for (size_t i = 0; i < jm->list.size() && i < 64; i++) {
            JVal* row = jm->list[i]; if (!row || row->type != 5) continue;
            const int n = JRowLen(row);
            float f[KfModifier::NFloats];
            for (int q = 0; q < KfModifier::NFloats; q++) f[q] = (q < n) ? JRowNum(row, q, 0) : 0.0f;
            KfModifier m; m.FromFloats(f, n < KfModifier::NFloats ? n : (int)KfModifier::NFloats);
            ap.mods.push_back(m);
        }
    }
    ap.RepeatToModifier();   // (el Repetir de los archivos de antes: queda como modificador Ciclos)
    JVal* ks = JChild(cj, "k", 5);
    // COTA: ninguna cantidad del archivo se usa sin acotar. Un millon de keyframes en
    // UNA curva no es un proyecto, es un archivo roto (o hecho a mano): se cargan los
    // primeros y se dice cuantos quedaron afuera, en vez de comerse la RAM en silencio.
    size_t nKeys = ks ? ks->list.size() : 0;
    if (nKeys > 1000000) {
        w3dLogfW("[W3D] una curva declara %d keyframes: se cargan los primeros 1000000", (int)nKeys);
        nKeys = 1000000;
    }
    // las filas [frame,value,...] vienen COMPACTAS del parser (JVal::nums): se leen con JFila*
    if (ks) for (size_t i = 0; i < nKeys; i++) {
        JVal* row = ks->list[i]; if (!row || row->type != 5 || JRowLen(row) < 3) continue;
        keyFrame kf;
        kf.frame = (int)JRowNum(row, 0, 0); kf.value = JRowNum(row, 1, 0);
        kf.Interpolation = (int)JRowNum(row, 2, 0);
        if (JRowLen(row) >= 8) {
            kf.handleType = (int)JRowNum(row, 3, 0);
            kf.inDF = JRowNum(row, 4, 0); kf.inDV = JRowNum(row, 5, 0);
            kf.outDF = JRowNum(row, 6, 0); kf.outDV = JRowNum(row, 7, 0);
        }
        ap.keyframes.push_back(kf);
    }
    return true;
}

// curvas de transform de una anim de objeto: [{p, c, k:[[frame,value,interp,htype,inDF,inDV,outDF,outDV],...]}]
static void LoadCurvesAnim(JVal* settings, VertexAnimation* anim) {
    JVal* cur = JChild(settings, "curves", 5); if (!cur) return;
    for (size_t c = 0; c < cur->list.size(); c++) {
        AnimProperty ap;
        if (LoadCurveJson(cur->list[c], ap)) anim->curves.push_back(ap);
    }
}

// carga las vertex anims de una malla desde el JSON: rango/fps + FRAMES (del blob binario
// self-contained, o legacy de los .obj por basePath) + curvas de transform. Compartido por
// las ramas "modelo" y "glb". 'base' es la carpeta donde se extrajo el zip.
// CUANTAS vertex anims necesitaron el remapeo por cercania de posicion en la ultima apertura.
// Lo mira el harness ('quadsave'): con la geometria en .w3dm tiene que quedar en CERO.
int g_w3dVaRemaps = 0;

// ANIMACION UV "tira de atlas" del JSON v4: { "frames": N, "fps": F, "eje": "u"|"v",
// "desfase": D, "ancho": A, "alto": A } bajo la clave "animUV" del objeto malla (la
// escribe SaveW3D; ancho/alto default 1 = tira sobre la textura entera).
static void LoadAnimUV(JVal* j, Mesh* mesh) {
    JVal* a = JChild(j, "animUV", 4);
    if (!a || !mesh) return;
    mesh->SetUVAnimStrip(JI(a, "frames", 1), JF(a, "fps", 30.0f),
                        JS(a, "axis", "u") == "v" ? 1 : 0, JI(a, "offset", 0),
                        JF(a, "width", 1.0f), JF(a, "height", 1.0f));
}

// true mientras se GENERAN los hijos de una instancia de prefab (W3dPrefabBuildChildren, mas abajo): sus
// vertex anims COMPARTEN los frames entre instancias (ver KeyBlobAnim)
static bool gGeneratingPrefab = false;
// la CLAVE de un blob de frames para compartirlo (VertexAnimAdoptKey): su ruta + su medida + su CRC. El
// contenido entra en la clave a proposito: re-guardar reescribe la entrada con otros frames bajo el MISMO
// nombre, y las instancias generadas antes siguen con los viejos hasta regenerarse
static std::string KeyBlobAnim(const std::string& path, const std::vector<unsigned char>& data) {
    char b[48];
    snprintf(b, sizeof(b), "|%lu|%08x", (unsigned long)data.size(), W3dZipCrc32(&data[0], data.size()));
    return "blob:" + path + b;
}

static void LoadAnimsVertex(JVal* j, Mesh* mesh, const std::string& base) {
    JVal* anims = JChild(j, "anims", 5); if (!anims || !mesh) return;
    for (size_t i = 0; i < anims->list.size(); i++) {
        JVal* a = anims->list[i]; if (a->type != 4) continue;
        VertexAnimation* anim = new VertexAnimation(
            NULL, JS(a, "name", "anim"), JB(a, "normals", false),
            JF(a, "speed", 1.0f), JB(a, "repeat", true), JI(a, "next", -1));
        anim->basePath  = JsonPath(JS(a, "base", ""), base);
        // padding = ancho del numero de frame en el nombre de archivo ("0001.obj"). Va DIRECTO
        // a un std::setw: un valor absurdo del archivo hacia que LoadFrames armara una cadena de
        // ese tamano (padding: 2000000000 = 2 GB por nombre de archivo).
        anim->padding   = JIRange(a, "padding", 0, 0, 16, "vertex anim");
        anim->startFrame= JI(a, "start", 1);
        anim->endFrame  = JI(a, "end", 250);
        anim->fps       = JI(a, "fps", 30);
        anim->target    = mesh;
        // FRAMES: preferir el blob binario (self-contained). Legacy: frameCount + .obj.
        std::string buffers = JS(a, "buffers", "");
        bool loaded = false;
        if (!buffers.empty()) {
            std::vector<unsigned char> data;
            // por JsonPath como TODAS las demas referencias: en v4 el blob es la
            // entrada "animations/<slug>.bin" y sale del contenedor montado; en v3
            // era un archivo suelto relativo a la carpeta del proyecto/zip
            if (w3dFileSystem::ReadFileBytes(JsonPath(buffers, base), data) && !data.empty()) {
                // (una INSTANCIA DE PREFAB: las N mallas generadas de la misma definicion leen el MISMO blob ->
                //  la primera registra sus frames y las demas los adoptan: UN juego de frames, como la geometria)
                const std::string key = gGeneratingPrefab ? KeyBlobAnim(JsonPath(buffers, base), data) : std::string();
                if (!key.empty() && VertexAnimAdoptKey(*anim, key, mesh->vertexSize)) loaded = true;
                else {
                    loaded = VertexAnimDeserialize(*anim, &data[0], data.size());
                    if (loaded && !key.empty() && anim->vcount == mesh->vertexSize) VertexAnimRegisterKey(*anim, key);
                }
            }
            // ---- EL BLOB ESTABA Y NO SE PUDO LEER: NI SILENCIO NI BORRARLO -------------
            //  Hermano exacto de W3dForeign::notLoaded de la malla, y quedaba abierto: aca no
            //  salia NI UNA LINEA de log. La anim quedaba con 0 frames y el proximo guardado
            //  la borraba PARA SIEMPRE (WriteAnimsVertex no emite "buffers" sin frames y
            //  PreserveTransient descarta a proposito todo lo huerfano bajo "animations/",
            //  o sea que el blob viejo desaparece del zip y la anim queda como cascara vacia).
            //  Se marca y se avisa EN PANTALLA; el guardado se frena en WriteAnimsVertex.
            if (!loaded) {
                anim->notLoaded = true;
                w3dLogfE("[W3D] '%s': no pude leer los frames de la vertex anim '%s' (%s): queda VACIA y no la voy a guardar encima",
                         mesh->name.c_str(), anim->name.c_str(), buffers.c_str());
                W3dAvisof(true, T("Could not read the frames of the animation '%s' of '%s': it stays EMPTY and I will not save over it (see whisk3d.log)"),
                          W3dShortName(anim->name).c_str(), W3dShortName(mesh->name).c_str());
            }
        }
        if (loaded && mesh->vertexSize > 0 && anim->vcount != mesh->vertexSize) {
            // ULTIMO RECURSO, y desde la etapa 3 casi nunca se usa: los keyframes son paralelos a
            // vertex[] (POR RENDER-VERT) y el GLB re-splitteaba los verts (24 -> 36), asi que
            // habia que ADIVINAR el vertice por CERCANIA DE POSICION contra la pose base. Con la
            // geometria en .w3dm el split se reproduce EXACTO y los keyframes entran DIRECTO;
            // esto queda solo para los .w3d viejos (y para una malla editada por fuera).
            w3dLogfW("[W3D] '%s': la vertex anim '%s' trae %d vertices y la malla tiene %d -> remapeo por POSICION (aproximado)",
                     mesh->name.c_str(), anim->name.c_str(), anim->vcount, mesh->vertexSize);
            g_w3dVaRemaps++;
            anim->target = mesh;
            VertexAnimRemapByPosition(*anim, mesh);
        }
        // legacy .obj (LoadVertexFrames los lee): es el tope del bucle que abre un .obj por frame.
        // Acotado para que un numero disparatado no se traduzca en millones de aperturas de archivo.
        if (!loaded) anim->frameCount = JIRange(a, "frames", 0, 0, 100000, "vertex anim");
        LoadCurvesAnim(a, anim);
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
    JVal* x = JChild(j, k, 5);
    if (x && x->list.size() >= 3) { v.x = (float)x->list[0]->num; v.y = (float)x->list[1]->num; v.z = (float)x->list[2]->num; }
}
static void ReadLayers(JVal* jc, std::vector<W3dLayerAnim>& out); // capas del MIX (definida mas abajo)
static void JMat16(JVal* j, const char* k, Matrix4& m) {
    JVal* x = JChild(j, k, 5);
    if (x && x->list.size() >= 16) for (int i = 0; i < 16; i++) m.m[i] = (float)x->list[i]->num;
}
struct PendModArm { Mesh* m; std::string name; };
static std::vector<PendModArm> gPendModArm; // referencias modArmature a resolver al final de la carga

// AUDIT versiones: el STACK de modificadores (Mirror/Screw/SubSurf/Array/Boolean)
// ahora viaja en el .w3d ("modificadores"). El target del Mirror va POR NOMBRE y se
// resuelve al final de la carga (como modArmature); las mallas con stack se anotan
// para regenerar su malla de render cuando ya esta todo resuelto.
struct PendModTgt { Modifier* md; std::string name; Mesh* m; };   // m = la malla del modificador (el scope)
static std::vector<PendModTgt> gPendModTgt;
static std::vector<Mesh*> gPendModGen;

static int ModTypeInt(const std::string& t) {
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
    JVal* v = JChild(j, k, 5);
    if (!v) return;
    if (v->list.size() > 0 && v->list[0]->type == 3) *a = v->list[0]->b;
    if (v->list.size() > 1 && v->list[1]->type == 3) *b = v->list[1]->b;
    if (v->list.size() > 2 && v->list[2]->type == 3) *c = v->list[2]->b;
}

static void LoadModifiers(JVal* j, Mesh* mesh) {
    JVal* jm = JChild(j, "modifiers", 5);
    if (!jm || !mesh) return;
    for (size_t i = 0; i < jm->list.size(); i++) {
        JVal* e = jm->list[i]; if (!e || e->type != 4) continue;
        int type = ModTypeInt(JS(e, "type", ""));
        if (type < 0) { w3dLogfW("[W3D] modificador de tipo desconocido '%s': se ignora", JS(e, "type", "").c_str()); continue; }
        Modifier* md = new Modifier(type, JS(e, "name", ModifierTypeName(type)));
        md->showViewport = JB(e, "viewport", true);
        md->showEdit     = JB(e, "edit", true);
        if (type == ModifierType::Mirror) {
            JBools3(e, "axes", &md->axisX, &md->axisY, &md->axisZ);
            md->merge     = JB(e, "merge", md->merge);
            md->mergeDist = JF(e, "mergeDist", md->mergeDist);
            md->clipping  = JB(e, "clipping", md->clipping);
            std::string tg = JS(e, "target", "");
            if (!tg.empty()) { PendModTgt p; p.md = md; p.name = tg; p.m = mesh; gPendModTgt.push_back(p); }
        } else if (type == ModifierType::Boolean) {
            const std::string op = JS(e, "op", "difference");
            md->boolOp = (op == "intersect") ? 0 : (op == "union") ? 1 : 2;
            std::string tg = JS(e, "target", "");
            if (!tg.empty()) { PendModTgt p; p.md = md; p.name = tg; p.m = mesh; gPendModTgt.push_back(p); }
        } else if (type == ModifierType::Array) {
            md->arrCount   = JF(e, "copies", md->arrCount);
            md->arrRelOn   = JB(e, "relative", md->arrRelOn);
            md->arrConstOn = JB(e, "constant", md->arrConstOn);
            JVal* r = JChild(e, "rel", 5);     // (5 = lista; puede venir compactada: JFila*)
            if (r && JRowLen(r) >= 3) { md->arrRelX = JRowNum(r, 0, md->arrRelX); md->arrRelY = JRowNum(r, 1, md->arrRelY); md->arrRelZ = JRowNum(r, 2, md->arrRelZ); }
            JVal* c = JChild(e, "const", 5);
            if (c && JRowLen(c) >= 3) { md->arrConstX = JRowNum(c, 0, md->arrConstX); md->arrConstY = JRowNum(c, 1, md->arrConstY); md->arrConstZ = JRowNum(c, 2, md->arrConstZ); }
            md->arrMerge     = JB(e, "merge", md->arrMerge);
            md->arrMergeDist = JF(e, "mergeDist", md->arrMergeDist);
            std::string tg = JS(e, "target", "");
            if (!tg.empty()) { PendModTgt p; p.md = md; p.name = tg; p.m = mesh; gPendModTgt.push_back(p); }
        } else if (type == ModifierType::SubdivisionSurface) {
            md->subLevel       = JF(e, "level", md->subLevel);
            md->subRenderLevel = JF(e, "renderLevel", md->subRenderLevel);
            md->subSimple      = JB(e, "simple", md->subSimple);
        } else if (type == ModifierType::Screw) {
            md->screwAngle       = JF(e, "angle", md->screwAngle);
            md->screwHeight      = JF(e, "height", md->screwHeight);
            md->screwSteps       = JF(e, "steps", md->screwSteps);
            md->screwRenderSteps = JF(e, "renderSteps", md->screwRenderSteps);
            md->screwAxis        = JI(e, "axis", md->screwAxis);
            md->screwStretchU    = JB(e, "stretchU", md->screwStretchU);
            md->screwStretchV    = JB(e, "stretchV", md->screwStretchV);
            md->screwSmooth      = JB(e, "smooth", md->screwSmooth);
            md->screwMerge       = JB(e, "weld", md->screwMerge);
            md->screwFlip        = JB(e, "flip", md->screwFlip);
        } else if (type == ModifierType::CullingTri) {
            // Culling por triangulo: metodo + sector/celda activo + NOMBRE del sidecar.
            // Los DATOS no viajan en el .w3d (son megabytes de indices): el sidecar se
            // ingiere al contenedor como cualquier asset y se relee de ahi. Sin "pvs"/
            // "vis" (archivos guardados antes de esto) se derivan del origen, como
            // siempre. "bsp" (el metodo viejo que nunca corto nada) cae a "celdas".
            std::string method = JS(e, "method", "triangles");
            md->methodPVS  = (method == "cells" || method == "bsp") ? 1 : 0;
            md->sectorPVS  = JI(e, "sector", 0);
            // fallback de celda vacia (-1 = completa, N = celda N, 0/ausente = nada)
            md->sectorFallback = JI(e, "sectorFallback", 0);
            md->pvsFile  = JsonPathField(JS(e, "pvs", ""), gBaseProject);   // (entradas de la libreria si es de una)
            md->visFile  = JsonPathField(JS(e, "vis", ""), gBaseProject);
            md->pathName = JS(e, "path", "");                       // recorrido: el motor elige la celda solo
            md->onlyCameraActive = JB(e, "activeCameraOnly", true);
            { std::string mask = JS(e, "branches", "");                  // "1101" = rama 2 apagada
              for (size_t r = 0; r < mask.size(); r++) md->branchesOn.push_back(mask[r] != '0'); }
        }
        mesh->modifiers.push_back(md);
    }
    if (!mesh->modifiers.empty()) {
        mesh->activeModifier = (int)mesh->modifiers.size() - 1;
        gPendModGen.push_back(mesh);   // regenerar cuando los targets esten resueltos
    }
}

static Armature* FindArmatureByName(Object* node, const std::string& n) {
    if (!node) return NULL;
    if (node->getType() == ObjectType::armature && node->name == n) return (Armature*)node;
    for (size_t i = 0; i < node->Childrens.size(); i++) {
        Armature* r = FindArmatureByName(node->Childrens[i], n);
        if (r) return r;
    }
    return NULL;
}
// el scope de una INSTANCIA DE PREFAB (sus hijos generados, sin entrar a las anidadas): el armature del
// enemigo 7 es el SUYO, no el del primero (ver W3dIsBoundaryScope en Objects.h)
static Armature* FindArmatureInScope(Object* node, const std::string& n) {
    for (size_t i = 0; i < node->Childrens.size(); i++) {
        Object* h = node->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::armature && h->name == n) return (Armature*)h;
        if (W3dIsBoundaryScope(h)) continue;
        Armature* r = FindArmatureInScope(h, n);
        if (r) return r;
    }
    return NULL;
}
// el armature 'n' COMO LO VE la malla 'm': primero adentro de su instancia de prefab (y las de afuera),
// despues en todo el arbol del que cuelga (la regla de siempre: el primero en preorden)
static Armature* FindArmatureFrom(Object* m, const std::string& n) {
    for (Object* inst = W3dInstanceOf(m); inst; inst = W3dInstanceOf(inst)) {
        Armature* a = FindArmatureInScope(inst, n);
        if (a) return a;
    }
    Object* root = W3dRootOf(m);
    return FindArmatureByName(root ? root : SceneCollection, n);
}
// al FINAL de la carga (ya existen todos los objetos): cada malla con "modArmature" recupera su
// modificador Armature + skinArmature, y el rig autorado queda preparado para deformar.
static void ResolvePendingModArmature() {
    for (size_t i = 0; i < gPendModArm.size(); i++) {
        PendModArm& p = gPendModArm[i];
        Armature* a = FindArmatureFrom(p.m, p.name);
        if (!a) { w3dLogfW("[W3D] modArmature '%s': no hay un armature con ese nombre (la malla queda sin rig)", p.name.c_str()); continue; }
        Modifier* md = new Modifier(ModifierType::Armature, ModifierTypeName(ModifierType::Armature));
        md->target = (Object*)a;
        p.m->modifiers.push_back(md);
        p.m->skinArmature = a;
        p.m->lastSkinFrame = -999999;
        PrepareAuthoredSkin(a); // no-op en rigs importados
    }
    gPendModArm.clear();
    // targets del stack de modificadores (el plano del Mirror puede ser CUALQUIER
    // objeto de la escena): recien ahora existen todos -> resolver por nombre
    for (size_t i = 0; i < gPendModTgt.size(); i++) {
        Object* t = W3dFindNameFrom(gPendModTgt[i].m, gPendModTgt[i].name);
        if (t) gPendModTgt[i].md->target = t;
        else w3dLogfW("[W3D] modificador con target '%s': no hay un objeto con ese nombre", gPendModTgt[i].name.c_str());
    }
    gPendModTgt.clear();
    // con los targets resueltos, regenerar la malla de render de cada stack cargado
    for (size_t i = 0; i < gPendModGen.size(); i++)
        gPendModGen[i]->GenerateMeshModified();
    gPendModGen.clear();
}
// lee un bloque "corners" ([[x,y,z,u,v,nx,ny,nz,w], ...]) y lo carga en 'ug' (UV GROUP: pesos por
// RENDER-VERT). Clave GEOMETRICA (ver el comentario largo en SaveW3D.cpp):
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
static void LoadCornersInUVGroup(JVal* cor, Mesh* mesh, UVGroup* ug) {
    if (!cor || !mesh || !ug || !mesh->uv) return;
    const bool rest = ((int)mesh->uv2dRest.size() == mesh->vertexSize * 2);
    const GLfloat* uvSrc = rest ? &mesh->uv2dRest[0] : mesh->uv;
    std::vector<char> alreadyPlaced((size_t)mesh->vertexSize, 0); // sin entradas duplicadas al mismo render-vert
    int noMatch = 0, rowsV1 = 0, repeated = 0;
    for (size_t k = 0; k < cor->list.size(); k++) {
        JVal* row = cor->list[k]; if (!row || row->type != 5 || JRowLen(row) < 6) continue;   // filas compactas (JVal::nums)
        const bool withNormal = (JRowLen(row) >= 9);
        if (!withNormal) rowsV1++;
        float x = JRowNum(row, 0, 0), y = JRowNum(row, 1, 0), z = JRowNum(row, 2, 0);
        float u = JRowNum(row, 3, 0), v = JRowNum(row, 4, 0);
        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
        if (withNormal) { nx = JRowNum(row, 5, 0); ny = JRowNum(row, 6, 0); nz = JRowNum(row, 7, 0); }
        float w = JRowNum(row, withNormal ? 8 : 5, 0);
        // pasada 1: la mejor distancia. pasada 2: todos los que empatan con ella.
        float bestD = 1e-4f; bool has = false;
        for (int pass = 0; pass < 2; pass++) {
            for (int i = 0; i < mesh->vertexSize; i++) {
                float dx = mesh->vertex[i*3]-x, dy = mesh->vertex[i*3+1]-y, dz = mesh->vertex[i*3+2]-z;
                float du = uvSrc[i*2]-u, dv = uvSrc[i*2+1]-v;
                float d = dx*dx + dy*dy + dz*dz + du*du + dv*dv;
                if (withNormal && mesh->normals) {
                    float mx = mesh->normals[i*3]/127.0f, my = mesh->normals[i*3+1]/127.0f, mz = mesh->normals[i*3+2]/127.0f;
                    float L = sqrtf(mx*mx + my*my + mz*mz);
                    if (L > 1e-6f) { mx /= L; my /= L; mz /= L; }
                    float dnx = mx-nx, dny = my-ny, dnz = mz-nz;
                    d += dnx*dnx + dny*dny + dnz*dnz;
                }
                if (pass == 0) { if (d < bestD) { bestD = d; has = true; } }
                else if (d <= bestD + 1e-8f) {
                    if (alreadyPlaced[(size_t)i]) { repeated++; continue; }
                    alreadyPlaced[(size_t)i] = 1;
                    ug->verts.push_back(i); ug->weights.push_back(w);
                }
            }
            if (!has) break;
        }
        if (!has) noMatch++;
    }
    if (noMatch)  w3dLogfW("[W3D] UV group '%s': %d de %d filas sin match en la malla '%s' (esos corners quedan en peso 0)",
                            ug->name.c_str(), noMatch, (int)cor->list.size(), mesh->name.c_str());
    if (repeated) w3dLogfW("[W3D] UV group '%s': %d filas cayeron en un render-vert ya asignado (clave ambigua); gana la primera",
                            ug->name.c_str(), repeated);
    if (rowsV1)   w3dLogfW("[W3D] UV group '%s': %d filas en formato viejo (sin normal); la clave puede ser ambigua entre caras que se pisan en UV. Volve a guardar el proyecto para migrarlas",
                            ug->name.c_str(), rowsV1);
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
static void JsonArm2DLoad(JVal* j, Mesh* mesh); // armatures 2D (formato nuevo + migracion del viejo), mas abajo
class Material;
static Material* MaterialByName(const std::string& name); // bloque raiz "materiales", mas abajo

static void LoadRigMesh(JVal* j, Mesh* mesh) {
    if (!j || !mesh) return;
    // QUIEN MANDA CUANDO ESTAN LOS DOS: si la malla ya trajo sus grupos del .w3dm (bloques VG y
    // UVG, por INDICE), el bloque viejo del JSON -que los guardaba por CLAVE GEOMETRICA porque el
    // GLB re-splitteaba los vertices- es una version ANTERIOR de lo mismo y se ignora con aviso.
    // Sin esta regla, un archivo con los dos abriria con los grupos DUPLICADOS.
    const bool vgOfW3dm = !mesh->vertexGroups.empty();
    const bool ugOfW3dm = !mesh->uvGroups.empty();
    JVal* jg = JChild(j, "vgroups", 5);
    if (jg && vgOfW3dm) {
        w3dLogf("[W3D] '%s': la geometria ya trajo sus vertex groups (.w3dm) -> ignoro el bloque 'vgroups' del JSON (formato viejo)",
                mesh->name.c_str());
        jg = NULL;
    }
    if (jg && mesh->vertex && mesh->vertexSize > 0) {
        WeightPaintEnsureMap(mesh); // garantiza el mapa render-vert -> control-point
        // posicion representativa de cada control-point (primer render-vert que lo usa)
        int nCP = 0;
        for (int i = 0; i < mesh->vertexSize && i < (int)mesh->vertCtrlPoint.size(); i++)
            if (mesh->vertCtrlPoint[i] + 1 > nCP) nCP = mesh->vertCtrlPoint[i] + 1;
        std::vector<float> px(nCP, 0.0f), py(nCP, 0.0f), pz(nCP, 0.0f);
        std::vector<char> has(nCP, 0);
        for (int i = 0; i < mesh->vertexSize && i < (int)mesh->vertCtrlPoint.size(); i++) {
            int cp = mesh->vertCtrlPoint[i];
            if (cp < 0 || cp >= nCP || has[cp]) continue;
            px[cp] = mesh->vertex[i*3]; py[cp] = mesh->vertex[i*3+1]; pz[cp] = mesh->vertex[i*3+2];
            has[cp] = 1;
        }
        for (size_t g = 0; g < jg->list.size(); g++) {
            JVal* e = jg->list[g]; if (!e || e->type != 4) continue;
            VertexGroup* vg = new VertexGroup(JS(e, "name", "Group"));
            JVal* pts = JChild(e, "points", 5);
            if (pts) for (size_t k = 0; k < pts->list.size(); k++) {
                JVal* row = pts->list[k]; if (!row || row->type != 5 || JRowLen(row) < 4) continue;   // filas compactas
                float x = JRowNum(row, 0, 0), y = JRowNum(row, 1, 0), z = JRowNum(row, 2, 0);
                float w = JRowNum(row, 3, 0);
                int best = -1; float bestD = 1e-4f; // tolerancia (los GLB guardan float32 exacto)
                for (int cp = 0; cp < nCP; cp++) {
                    if (!has[cp]) continue;
                    float dx = px[cp]-x, dy = py[cp]-y, dz = pz[cp]-z;
                    float d = dx*dx + dy*dy + dz*dz;
                    if (d < bestD) { bestD = d; best = cp; }
                }
                if (best >= 0) { vg->verts.push_back(best); vg->weights.push_back(w); }
            }
            // MIGRACION del formato de un dia: los corners venian DENTRO del vertex group. Se
            // convierten en un UV GROUP del mismo nombre (entidad nueva) y el vertex group queda
            // puro por control-point. Al re-guardar el proyecto ya salen en "uvgroups".
            JVal* oldCor = JChild(e, "corners", 5);
            if (oldCor && !oldCor->list.empty() && mesh->uv) {
                UVGroup* ug = new UVGroup(vg->name);
                LoadCornersInUVGroup(oldCor, mesh, ug);
                mesh->uvGroups.push_back(ug);
                w3dLogf("[W3D] '%s': el vertex group '%s' traia pesos por corner del formato viejo -> migrados a un UV group homonimo",
                        mesh->name.c_str(), ug->name.c_str());
            }
            mesh->vertexGroups.push_back(vg);
        }
        if (!mesh->vertexGroups.empty()) mesh->activeGroup = 0;
        mesh->lastSkinFrame = -999999;
    }
    // UV GROUPS (bloque propio). Se leen DESPUES de los vgroups para que la migracion de arriba
    // no pise a un archivo que ya tiene los dos (no puede pasar, pero el orden lo deja explicito).
    //
    // *** UN GRUPO DEL ARCHIVO = UN GRUPO EN MEMORIA ***  (perdida de datos F5)
    // Antes se buscaba un uvGroup HOMONIMO en la malla y, si aparecia, se le cargaban los
    // corners ENCIMA. Con eso un .w3d con dos "Mano" abria con UNO SOLO: los pesos del segundo
    // se mezclaban con los del primero (y encima duplicados, porque LoadCornersInUVGroup
    // lleva su propio 'alreadyPlaced' por llamada) y el segundo grupo desaparecia sin un aviso.
    // Ademas ese espacio nunca llegaba a la reparacion de nombres, que es la que renumera los
    // repetidos con el criterio de siempre (primero-gana).
    // El UNICO merge que ERA intencional es el de la MIGRACION de arriba: los corners que
    // venian dentro de un "vgroups" del formato de un dia crean un UV group homonimo, y si el
    // MISMO archivo trajera tambien el bloque "uvgroups" (no puede pasar hoy, pero es el caso
    // que el codigo viejo queria cubrir) ese bloque es la version NUEVA del mismo grupo. Ese
    // caso se distingue EXPLICITAMENTE: solo se completan los grupos que creo la migracion, UNO
    // POR NOMBRE (marcados como consumidos), y se les LIMPIAN los corners migrados antes de
    // cargar los del bloque nuevo (mezclar las dos versiones daba pesos duplicados).
    // Cualquier otro homonimo entra como GRUPO SEPARADO y lo renumera W3dNamesRepairScene.
    JVal* jug = JChild(j, "uvgroups", 5);
    if (jug && ugOfW3dm) {
        w3dLogf("[W3D] '%s': la geometria ya trajo sus UV groups (.w3dm) -> ignoro el bloque 'uvgroups' del JSON (formato viejo)",
                mesh->name.c_str());
        jug = NULL;
    }
    if (jug && mesh->vertex && mesh->vertexSize > 0 && mesh->uv) {
        const size_t nMigrated = mesh->uvGroups.size();   // los que dejo la migracion de "vgroups"
        std::vector<char> migratedUsed(nMigrated, 0);
        for (size_t g = 0; g < jug->list.size(); g++) {
            JVal* e = jug->list[g]; if (!e || e->type != 4) continue;
            std::string name = JS(e, "name", "UV Group");
            UVGroup* ug = NULL;
            for (size_t k = 0; k < nMigrated; k++)       // SOLO contra los migrados, y una sola vez
                if (!migratedUsed[k] && mesh->uvGroups[k]->name == name) {
                    migratedUsed[k] = 1;
                    ug = mesh->uvGroups[k];
                    ug->verts.clear(); ug->weights.clear();  // el bloque nuevo MANDA sobre el migrado
                    w3dLogf("[W3D] '%s': el UV group '%s' venia migrado de 'vgroups' y el archivo trae "
                            "tambien su bloque 'uvgroups' -> gana el bloque nuevo", mesh->name.c_str(), name.c_str());
                    break;
                }
            if (!ug) { ug = new UVGroup(name); mesh->uvGroups.push_back(ug); }
            LoadCornersInUVGroup(JChild(e, "corners", 5), mesh, ug);
        }
        // el activo del formato viejo: el JSON no lo guardaba, asi que queda el 0. VA ADENTRO del
        // if a proposito: cuando los grupos vienen del .w3dm el activo lo dice el archivo
        // (directiva uvgactiva) y pisarlo aca rompia el round-trip byte a byte.
        if (!mesh->uvGroups.empty()) mesh->uvActiveGroup = 0;
    }
    std::string ma = JS(j, "modArmature", "");
    if (!ma.empty()) {
        PendModArm p; p.m = mesh; p.name = ma;
        gPendModArm.push_back(p);
    }
    // ARMATURES 2D DEL MESH: huesos en espacio UV (campo del objeto malla, junto a vgroups).
    // La POSE del archivo se RE-APLICA aca mismo (Armature2DApply): el editor y el juego
    // compilado ven las UV posadas al abrir.
    //
    // DOS FORMATOS:
    //   NUEVO  "armatures2d": [ { nombre, huesos:[...], anims:[...], activeAnim } ] + "armature2dActive"
    //   VIEJO  "armature2d": [huesos] + "anims2d": [clips] + "anim2dActiva"   (UN solo rig)
    // El viejo se MIGRA en silencio al armature 0 (se llama "Armature 2D") y al guardar sale ya en
    // el formato nuevo. Ausencia de los dos = malla sin rig 2D -> RETROCOMPAT total (la capa uv
    // horneada de la vertex anim sigue reproduciendose como siempre).
    // Reusa LoadCurveJson (generico, no sabe de huesos).
    JsonArm2DLoad(j, mesh);
}

// lee los huesos de un bloque JSON (lista de objetos) dentro de 'arm'
static void JsonArm2DBones(JVal* jl, Armature2D* arm) {
    if (!jl || !arm) return;
    for (size_t b = 0; b < jl->list.size(); b++) {
        JVal* e = jl->list[b]; if (!e || e->type != 4) continue;
        W3dBone2D hb;
        hb.name   = JS(e, "name", "Bone");
        hb.parent = JI(e, "parent", -1);
        hb.connected = !JB(e, "loose", false); // "Disconnect Bone": emparentado sin soldar
        JVal* h = JChild(e, "head", 5);
        if (h && h->list.size() >= 2) { hb.headU = (float)h->list[0]->num; hb.headV = (float)h->list[1]->num; }
        JVal* t = JChild(e, "tail", 5);
        if (t && t->list.size() >= 2) { hb.tailU = (float)t->list[0]->num; hb.tailV = (float)t->list[1]->num; }
        JVal* p = JChild(e, "pose", 5);
        if (p && p->list.size() >= 5) {
            hb.poseTU  = (float)p->list[0]->num; hb.poseTV = (float)p->list[1]->num;
            hb.poseRot = (float)p->list[2]->num;
            hb.poseSX  = (float)p->list[3]->num; hb.poseSY = (float)p->list[4]->num;
        }
        arm->bones.push_back(hb);
    }
    // misma cota que el armature 3D: 'padre' fuera de rango o en ciclo -> raiz. Aca ademas
    // evita colgar los recorridos que suben por la cadena sin contador (UVEditor: borrar huesos).
    { std::vector<int> pad(arm->bones.size());
      for (size_t i = 0; i < arm->bones.size(); i++) pad[i] = arm->bones[i].parent;
      if (SanitizeParents(pad, arm->name.c_str()))
          for (size_t i = 0; i < arm->bones.size(); i++) arm->bones[i].parent = pad[i]; }
}

// lee los clips de un bloque JSON (lista de objetos) dentro de 'arm'
static void JsonArm2DClips(JVal* jl, Armature2D* arm) {
    if (!jl || !arm) return;
    for (size_t i = 0; i < jl->list.size(); i++) {
        JVal* e = jl->list[i]; if (!e || e->type != 4) continue;
        Armature2DAnimation* an = new Armature2DAnimation(JS(e, "name", "Anim2D"));
        an->fps        = JI(e, "fps", 30);
        an->startFrame = JI(e, "start", 1);
        an->endFrame   = JI(e, "end", 0);
        JVal* jt = JChild(e, "tracks", 5);
        if (jt) for (size_t t = 0; t < jt->list.size(); t++) {
            JVal* tj = jt->list[t]; if (!tj || tj->type != 4) continue;
            Bone2DTrack tr;
            tr.bone = JI(tj, "bone", -1);
            JVal* jc = JChild(tj, "curves", 5);
            if (jc) for (size_t c = 0; c < jc->list.size(); c++) {
                AnimProperty ap;
                if (LoadCurveJson(jc->list[c], ap)) tr.Properties.push_back(ap);
            }
            an->tracks.push_back(tr);
        }
        arm->anims.push_back(an);
    }
}

static void JsonArm2DLoad(JVal* j, Mesh* mesh) {
    JVal* jarms = JChild(j, "armatures2d", 5);
    if (jarms) {                                   // ---- FORMATO NUEVO (varios armatures) ----
        for (size_t a = 0; a < jarms->list.size(); a++) {
            JVal* e = jarms->list[a]; if (!e || e->type != 4) continue;
            int idx = mesh->Arm2DAdd(JS(e, "name", "Armature 2D"));
            Armature2D* arm = mesh->armatures2d[idx];
            JsonArm2DBones(JChild(e, "bones", 5), arm);
            JsonArm2DClips(JChild(e, "anims", 5), arm);
            if (!arm->anims.empty()) {
                arm->activeAnim = JI(e, "activeAnim", 0);
                // indice de lista: fuera de rango (por arriba O por abajo) -> el primero
                if (arm->activeAnim < 0 || arm->activeAnim >= (int)arm->anims.size()) arm->activeAnim = 0;
            }
        }
        mesh->armature2dActive = JI(j, "activeArmature2d", 0);
        if (mesh->armature2dActive < 0 || mesh->armature2dActive >= (int)mesh->armatures2d.size())
            mesh->armature2dActive = mesh->armatures2d.empty() ? -1 : 0;
    } else {                                       // ---- FORMATO VIEJO (un solo rig): MIGRAR ----
        JVal* jh = JChild(j, "armature2d", 5);
        JVal* jc = JChild(j, "anims2d", 5);
        if (!jh && !jc) return;                    // malla sin rig 2D: nada que hacer
        Armature2D* arm = mesh->armatures2d[mesh->Arm2DAdd("Armature 2D")];
        JsonArm2DBones(jh, arm);
        JsonArm2DClips(jc, arm);
        if (!arm->anims.empty()) {
            arm->activeAnim = JI(j, "activeAnim2d", 0);
            if (arm->activeAnim < 0 || arm->activeAnim >= (int)arm->anims.size()) arm->activeAnim = 0;
        }
    }
    if (!mesh->HasArm2D()) return;
    mesh->Armature2DRestCapture(); // el UV del GLB ES el rest (se exporta sin la pose)
    mesh->Armature2DApply();       // la pose guardada se ve al abrir (editor y runtime)
    // .w3d ANTERIOR a los pesos por corner: el rig 2D existe pero no hay UV groups (los
    // pesos viejos eran vertex groups por control-point, que el hueso 2D ya no mira). No se
    // hornea nada en silencio (eso es justo el bake que el dueno rechazo): se avisa y el
    // usuario re-pinta los UV groups en el editor UV.
    bool hasBones = false;
    for (size_t a = 0; a < mesh->armatures2d.size() && !hasBones; a++)
        if (mesh->armatures2d[a] && !mesh->armatures2d[a]->bones.empty()) hasBones = true;
    if (hasBones && mesh->uvGroups.empty())
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
//  vuelve 36 verts/12 tris). El .w3d guarda aparte el bloque "topologia" (ver SaveW3D.cpp), que
//  es lo unico que falta: que corners forman cada poligono, con los indices DEL VERTEX ARRAY DEL GLB.
//
//  Como el orden de emision de triangulos del exportador es deterministico (OrderFacesGLB) y el
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
static bool SameRenderVert(Mesh* m, int a, int b) {
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
//  ImportModelByExtension marca Mesh::origen con el archivo importado, pero en
//  la rama legacy ese "archivo" es una ENTRADA DEL PROPIO .w3d, no un archivo del
//  usuario: dejarlo puesto llena EXTERNALS.txt de rutas internas que no existen.
//  Antes se limpiaba SOLO el nodo de arriba: un .glb con un ARMATURE que trae la
//  malla COMO HIJA dejaba a la hija con origen = ruta interna del contenedor y al
//  guardar aparecia en EXTERNALS.txt como archivo que falta.
// ============================================================================
static void ClearOriginRec(Object* o) {
    if (!o) return;
    if (o->getType() == ObjectType::mesh) ((Mesh*)o)->origin.clear();
    for (size_t i = 0; i < o->Childrens.size(); i++) ClearOriginRec(o->Childrens[i]);
}

static bool RetopologizeFromTopology(JVal* j, Mesh* m) {
    if (!j || !m) return false;
    JVal* topo = JChild(j, "topology", 4);
    if (!topo) return false;                                  // .w3d viejo: camino de siempre (triangulado)
    JVal* faces = JChild(topo, "faces", 5);
    if (!faces || faces->list.empty()) return false;
    const char* name = m->name.c_str();
    if (!m->vertex || m->vertexSize <= 0 || m->faces3d.empty()) {
        w3dLogfW("[W3D] '%s': hay bloque 'topologia' pero la malla del GLB vino vacia -> se ignora", name);
        return false;
    }

    // ---- pasada 0: validar las filas + contar triangulos + mayor indice ----
    // (las caras vienen COMPACTAS del parser -- JVal::nums -- y se leen con JFila*)
    int maxIdx = -1; size_t totTri = 0;
    for (size_t i = 0; i < faces->list.size(); i++) {
        JVal* c = faces->list[i];
        if (!c || c->type != 5 || JRowLen(c) < 3) {
            w3dLogfW("[W3D] '%s': 'topologia' con una cara de menos de 3 corners -> se ignora el bloque (la malla queda triangulada)", name);
            return false;
        }
        totTri += JRowLen(c) - 2;
        for (size_t k = 0; k < JRowLen(c); k++) {
            if (!JRowIsNum(c, k)) { w3dLogfW("[W3D] '%s': 'topologia' con un indice no numerico -> se ignora el bloque", name); return false; }
            int v = (int)JRowNum(c, k, -1);
            if (v < 0) { w3dLogfW("[W3D] '%s': 'topologia' con un indice negativo -> se ignora el bloque", name); return false; }
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
                 name, nGlb, m->vertexSize);
        return false;
    }
    if (maxIdx >= nGlb) {
        w3dLogfW("[W3D] '%s': 'topologia' referencia el vertice %d pero declara %d -> se ignora el bloque", name, maxIdx, nGlb);
        return false;
    }
    if (totTri != m->faces3d.size()) {
        w3dLogfW("[W3D] '%s': 'topologia' pide %d triangulos y el GLB trae %d (el .glb no es pareja del .w3d) -> la malla queda triangulada",
                 name, (int)totTri, (int)m->faces3d.size());
        return false;
    }

    // ---- pasada 1: lockstep triangulo a triangulo -> tabla indiceGLB -> render-vert ----
    std::vector<int> glbARender((size_t)nGlb, -1);
    std::vector<MeshFace> newValue; newValue.reserve(faces->list.size());
    size_t t = 0;
    for (size_t i = 0; i < faces->list.size(); i++) {
        JVal* c = faces->list[i];
        const int n = (int)JRowLen(c), nTri = n - 2;
        const MeshFace& T0 = m->faces3d[t];
        if (T0.idx.size() != 3) { w3dLogfW("[W3D] '%s': el GLB no vino triangulado -> se ignora 'topologia'", name); return false; }
        std::vector<int> rv((size_t)n, -1);
        rv[0] = T0.idx[0]; rv[1] = T0.idx[1]; rv[2] = T0.idx[2];
        for (int k = 1; k < nTri; k++) {
            const MeshFace& Tk = m->faces3d[t + (size_t)k];
            // el abanico tiene que cerrar: corner 0 comun y corner 1 = el ultimo corner del triangulo previo.
            // (se compara por ATRIBUTOS, no por indice: el importador del GLB no mergea, cada corner es un
            //  render-vert distinto aunque venga del mismo vertice del GLB)
            if (Tk.idx.size() != 3 || !SameRenderVert(m, Tk.idx[0], rv[0]) || !SameRenderVert(m, Tk.idx[1], rv[k+1])) {
                w3dLogfW("[W3D] '%s': el abanico de la cara %d de 'topologia' no cierra contra el GLB -> la malla queda triangulada", name, (int)i);
                return false;
            }
            rv[k+2] = Tk.idx[2];
        }
        MeshFace mf; mf.mat = T0.mat; mf.smooth = -1;
        mf.idx.resize((size_t)n);
        for (int q = 0; q < n; q++) {
            int g = (int)JRowNum(c, (size_t)q, 0);
            if (glbARender[(size_t)g] < 0) glbARender[(size_t)g] = rv[(size_t)q];
            else if (!SameRenderVert(m, glbARender[(size_t)g], rv[(size_t)q])) {
                w3dLogfW("[W3D] '%s': el vertice %d de 'topologia' cae en dos render-verts distintos -> la malla queda triangulada", name, g);
                return false;
            }
            mf.idx[(size_t)q] = g;   // por ahora en espacio GLB; se reindexa abajo
        }
        newValue.push_back(mf);
        t += (size_t)nTri;
    }
    if (t != m->faces3d.size()) return false;   // (no deberia pasar: totTri ya lo garantiza)

    // shading POR CARA (sparse): [indiceDeCara, 0=flat|1=smooth]
    JVal* shade = JChild(topo, "shade", 5);
    if (shade) for (size_t k = 0; k < shade->list.size(); k++) {
        JVal* p = shade->list[k]; if (!p || p->type != 5 || JRowLen(p) < 2) continue;   // pares compactos
        int fi = (int)JRowNum(p, 0, -1);
        if (fi >= 0 && fi < (int)newValue.size()) newValue[(size_t)fi].smooth = ((int)JRowNum(p, 1, 0)) ? 1 : 0;
    }

    // ---- pasada 2: rehacer los arrays EN EL ORDEN DEL GLB (= el orden que tenian al guardar) ----
    std::vector<int> newOf((size_t)nGlb, -1);
    int newN = 0;
    for (int g = 0; g < nGlb; g++) if (glbARender[(size_t)g] >= 0) newOf[(size_t)g] = newN++;
    if (newN <= 0) return false;
    if (newN != nGlb)
        w3dLogfW("[W3D] '%s': %d vertices del GLB no los usa ninguna cara -> los indices se compactan (%d de %d). La numeracion sigue siendo deterministica, pero la vertex anim se remapea por posicion",
                 name, nGlb - newN, newN, nGlb);

    GLfloat* nvx = new GLfloat[(size_t)newN * 3];
    GLbyte*  nnr = m->normals     ? new GLbyte[(size_t)newN * 3]  : NULL;
    GLfloat* nuv = m->uv          ? new GLfloat[(size_t)newN * 2] : NULL;
    GLubyte* ncl = m->vertexColor ? new GLubyte[(size_t)newN * 4] : NULL;
    std::vector<int> nCP; if (!m->vertCtrlPoint.empty()) nCP.assign((size_t)newN, -1);
    std::vector<int> oldToNew((size_t)m->vertexSize, -1);   // para remapear la geometria suelta
    for (int g = 0; g < nGlb; g++) {
        int d = newOf[(size_t)g]; if (d < 0) continue;
        int src = glbARender[(size_t)g];
        oldToNew[(size_t)src] = d;
        for (int q = 0; q < 3; q++) nvx[d*3+q] = m->vertex[src*3+q];
        if (nnr) for (int q = 0; q < 3; q++) nnr[d*3+q] = m->normals[src*3+q];
        if (nuv) for (int q = 0; q < 2; q++) nuv[d*2+q] = m->uv[src*2+q];
        if (ncl) for (int q = 0; q < 4; q++) ncl[d*4+q] = m->vertexColor[src*4+q];
        if (!nCP.empty() && src < (int)m->vertCtrlPoint.size()) nCP[(size_t)d] = m->vertCtrlPoint[(size_t)src];
    }
    for (size_t i = 0; i < newValue.size(); i++)
        for (size_t k = 0; k < newValue[i].idx.size(); k++) newValue[i].idx[k] = newOf[(size_t)newValue[i].idx[k]];

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
    m->vertexSize = newN;
    if (!nCP.empty()) m->vertCtrlPoint.swap(nCP);
    m->faces3d.swap(newValue);
    m->uvSelVert.clear();        // seleccion del editor UV: era del render triangulado
    m->FreeLayers(false);        // capas por corner (nC cambio); NO tocar vertex/UV groups
    // meshSmooth EXPLICITO: antes se adivinaba con MeshShadingImportedIsSmooth sobre la malla triangulada
    m->meshSmooth = JB(topo, "smooth", m->meshSmooth);
    m->RegroupMeshParts();  // faces[] + rangos por mesh part desde faces3d.mat (+ VBO)
    m->ComputeBorders();    // edges/posRep/centerGeom: las diagonales dejan de ser aristas
    m->SyncCornerNormal();  // cornerNormal desde normals[]: el 1er GenerateRender no reinventa el shading
    return true;
}

static void JsonObject(JVal* j, Object* parent, const std::string& base);

// ---------------------------------------------------------------------------
//  CREA el objeto de UN nodo del .w3d y lo devuelve; los HIJOS NO se leen aca
//  (los lee JsonObject, en UN solo lugar, ver abajo). Es el ESPEJO exacto de
//  WriteObject/WriteChildren (SaveW3D.cpp): antes cada rama leia -o se
//  OLVIDABA de leer- "hijos", y camara/luz/script/ui/modelo/glb no lo leian, o
//  sea que ni siquiera un .w3d editado a mano podia colgarles nada.
//  NULL = el nodo no creo ningun objeto (tipo desconocido, o la carga fallo):
//  sus hijos cuelgan del padre, que es lo que ya hacia el fallback generico.
// ---------------------------------------------------------------------------
static Object* JsonObjectCreate(JVal* j, Object* parent, const std::string& base) {
    std::string type = JS(j, "type", "");

    if (type == "collection") {
        Collection* c = new Collection(parent);
        // CommonJson y no solo el nombre: el guardado nuevo escribe visible/pos/rot/
        // escala (una Collection oculta volvia VISIBLE al reabrir). Tolerante: los
        // archivos viejos sin esos campos quedan con los defaults de siempre.
        CommonJson(j, c);
        // orden de dibujo para transparentes (ausente = false: archivos viejos igual)
        c->sortByCamera = JB(j, "sortByCamera", false);
        c->sortOnce     = JB(j, "sortOnce", false);
        // lote estatico (P4): ausente = off (archivos viejos, cero cambio)
        c->batch = (JS(j, "batch", "") == "static") ? 1 : 0;
        return c;
    }
    if (type == "camera") {
        Camera* c = new Camera(parent);
        CommonJson(j, c);
        c->fov = JF(j, "fov", c->fov);           // lente (default 45 si el .w3d es viejo)
        c->orthographic = JB(j, "ortho", c->orthographic);
        c->nearClip = JF(j, "near", c->nearClip);
        c->farClip  = JF(j, "far",  c->farClip);
        // ENCUADRE DECLARADO (ver Camera::aspecto). Ausente = 0 = como siempre.
        c->aspect  = JF(j, "aspect", c->aspect);
        // PROFUNDIDAD DE CAMPO (ausente = apagada, f/2.8, a 5 m)
        c->dof = JB(j, "dof", c->dof); c->fStop = JF(j, "fStop", c->fStop); c->focusDist = JF(j, "focus", c->focusDist);
        c->focusObject = JS(j, "focusObject", "");
        if (c->fStop < 0.1f) c->fStop = 0.1f; if (c->focusDist < 0.01f) c->focusDist = 0.01f;
        std::string t = JS(j, "target", "");
        if (!t.empty()) c->SetTarget(t);   // se resuelve en ReloadAll al final
        // RIEL (AUDIT versiones): la Curve por la que viaja la camara + su offset
        std::string rail = JS(j, "rail", "");
        if (!rail.empty()) c->SetRail(rail);   // idem: ReloadRail al final
        c->offsetRail = JI(j, "railOffset", c->offsetRail);
        // MIRADA DEL RIEL (ver Camera.h). Ausente = false = look-at de siempre.
        c->useRailRotation = JB(j, "railLook", c->useRailRotation);
        return c;
    }
    if (type == "mirror") {   // objeto Espejo (AUDIT versiones: antes se perdia al guardar)
        Mirror* mi = new Mirror(parent);
        CommonJson(j, mi);
        JBools3(j, "axes", &mi->mirrorX, &mi->mirrorY, &mi->mirrorZ);
        std::string t = JS(j, "target", "");
        if (!t.empty()) mi->SetTarget(t);
        // MULTI-TARGET: "targets" es UNA cadena "A, B, C" (mismo idioma que
        // "distancias" del LOD). Opcional: sin ella manda "target" y un archivo
        // viejo abre exactamente igual.
        std::string ts = JS(j, "targets", "");
        if (!ts.empty()) mi->SetTargetsText(ts);
        // reflejo en agua (ver Mirror.h): sin z + acotado al estanque.
        // "limites" = [u0, u1, v0, v1] en el PLANO del espejo (mismo idioma que "distancias").
        mi->noDepth = JB(j, "noDepth", false);
        // sinkMax: DEPRECADO (ver Mirror.h). Se acepta, se avisa, sin efecto.
        mi->sinkMax = JF(j, "maxSink", 0.0f);
        if (mi->sinkMax > 0.0f)
            w3dLogfW("[W3D] Mirror '%s': hundimientoMaximo esta DEPRECADO y no tiene "
                     "efecto (la reflexion es geometrica pura; el recorte del reflejo "
                     "es la silueta del agua en pantalla)", mi->name.c_str());
        if (JChild(j, "clip", 3)) { mi->clipDeclared = true; mi->clip = JB(j, "clip", true); }   // 3 = bool
        JVal* lim = JChild(j, "limits", 5);
        if (lim && lim->list.size() == 4 && lim->list[0] && lim->list[1]
            && lim->list[2] && lim->list[3]) {
            mi->usesLimits = true;
            mi->limU0 = (float)lim->list[0]->num;
            mi->limU1 = (float)lim->list[1]->num;
            mi->limV0 = (float)lim->list[2]->num;
            mi->limV1 = (float)lim->list[3]->num;
        }
        return mi;
    }
    if (type == "instance") {   // Instance: copias enlazadas del target
        Instance* in = new Instance(parent);
        CommonJson(j, in);
        std::string t = JS(j, "target", "");
        if (!t.empty()) in->SetTarget(t);
        // 'count' es un size_t que manda un for del RENDER: un valor negativo del archivo se
        // convertia en ~1.8e19 vueltas (el editor no volvia del primer frame) y uno gigante
        // tambien. Se acota a algo que se pueda dibujar.
        in->count      = (size_t)JIRange(j, "count", (int)in->count, 0, 4096, "instance");
        in->mirror     = JB(j, "mirrored", false);
        in->mirrorAxis = JIRange(j, "mirrorAxis", 0, 0, 2, "instance");
        return in;
    }
    if (type == "constraint") {   // el objeto Constraint VIEJO (dado de baja): se MIGRA
        // misma mecanica que la rama de texto (ver OldPendCons): Empty + entrada
        // pendiente, y las props van al objeto APUNTADO al final de la carga. Aca el
        // default de "pitch" es FALSE, que es el del formato JSON y NO el del texto.
        bool cx = false, cy = false, cz = false;
        JBools3(j, "copyPos", &cx, &cy, &cz);
        Object* e = NodeOldConstraint(parent, JS(j, "target", ""),
                                        JB(j, "horizontal", true), JB(j, "pitch", false),
                                        cx, cy, cz);
        CommonJson(j, e);
        return e;
    }
    if (type == "curve") {   // Curve cargada de archivo (riel de camara)
        std::string rel = JS(j, "file", "");
        Curve* cv = new Curve(parent);
        CommonJson(j, cv);
        // curva AUTORADA (Add > Path / riel editado): los nodos vienen INLINE en "puntos"
        JVal* pts = JChild(j, "points", 5);
        if (pts && pts->list.size() >= 3) {
            int n = (int)pts->list.size() / 3;
            cv->vertexSize = n;
            cv->vertex = new GLfloat[n * 3];
            for (int i = 0; i < n * 3; i++)
                cv->vertex[i] = pts->list[i] ? (GLfloat)pts->list[i]->num : 0.0f;
            cv->indices = new GLushort[n];
            for (int i = 0; i < n; i++) cv->indices[i] = (GLushort)i;
            cv->signZ = 1.0f;   // autorada en el espacio del motor (sin espejo de .cap)
            JVal* ar = JChild(j, "edges", 5);   // GRAFO con ramas (pares); ausente = polilinea
            if (ar) for (size_t i = 0; i + 1 < ar->list.size(); i += 2) {
                cv->edges.push_back((GLushort)(ar->list[i]   ? (int)ar->list[i]->num   : 0));
                cv->edges.push_back((GLushort)(ar->list[i+1] ? (int)ar->list[i+1]->num : 0));
            }
            cv->BuildKDTree();
        }
        // "archivo" vacio y sin puntos = curva sin origen (queda vacia, el nodo vuelve)
        else if (rel.empty())
            w3dLogfW("[W3D] la curva '%s' no tiene archivo de origen (queda vacia)", cv->name.c_str());
        else if (!cv->LoadFromFile(JsonPath(rel, base)))
            w3dLogfW("[W3D] no pude cargar la curva %s (queda vacia)", rel.c_str());
        cv->RecalculateBranches();   // ramas (componentes) para el filtro del modificador Oclusion
        // LISTA DE CARGA (streaming): el sidecar .cargas.json (opcional)
        { std::string cg = JS(j, "loads", "");
          if (!cg.empty()) cv->LoadLoadList(JsonPath(cg, base)); }
        return cv;
    }
    if (type == "light") {
        Light* l = Light::Create(parent);
        if (!l) {   // tope de GL (MAX_LIGHTS): se saltea la luz (antes: crash con NULL en CommonJson)
            W3dAvisof(false, T("The light '%s' was not loaded: the scene already has the maximum number of lights (%d)."),
                      JS(j, "name", "").c_str(), (int)MAX_LIGHTS_SCENE);
            return NULL;
        }
        CommonJson(j, l);
        JVal* col = JChild(j, "color", 5);
        if (col && col->list.size() >= 3)
            for (int i = 0; i < 3; i++) l->diffuse[i] = (float)col->list[i]->num;
        l->directional = JB(j, "directional", l->directional);
        l->ignoreOrder = JB(j, "ignoreOrder", true);   // ausente = alumbra toda la escena (el pre-pase)
        JVal* at = JChild(j, "atenuacion", 5);   // (5 = lista)
        if (at && JRowLen(at) >= 3) { l->attConstant = JRowNum(at, 0, l->attConstant); l->attLinear = JRowNum(at, 1, l->attLinear); l->attQuadratic = JRowNum(at, 2, l->attQuadratic); }
        l->rtRadius = JF(j, "rtRadius", 0.0f);   // trazado de rayos: tamano de la lampara (sin la clave: un proyecto de antes, cuando la lampara nueva era puntual)
        l->rtRays   = JF(j, "rtRays", l->rtRays);   // rayos de sombra (0 = global)
        // FOCO (cono en grados + concentracion) y el AMBIENTE de la luz (ausentes = los de fabrica)
        JVal* sp = JChild(j, "spot", 5);
        if (sp && JRowLen(sp) >= 2) { l->spotCutoff = JRowNum(sp, 0, l->spotCutoff); l->spotExponent = JRowNum(sp, 1, l->spotExponent); }
        l->intensity = JF(j, "intensity", 1.0f);
        l->distance = JF(j, "distance", 0.0f);
        JVal* am = JChild(j, "ambient", 5);
        if (am && am->list.size() >= 3) for (int i = 0; i < 3; i++) l->ambient[i] = (float)am->list[i]->num;
        return l;
    }
    if (type == "script") {
        // MIGRACION del objeto Script/Control (ver el comentario largo de la rama "Gamepad"
        // del .w3d de texto): vuelve como Empty con nombre, transformacion, hijos y scripts.
        // La fisica hardcodeada ("fisica") y el "target" se descartan a proposito.
        Empty* g = new Empty(parent);
        CommonJson(j, g);
        W3dAvisof(false, T("The Script object '%s' no longer exists: it was migrated to an empty object with its "
                           "scripts. Its hardcoded physics (speed/gravity/limits) is NOT migrated."),
                  g->name.c_str());
        // (los scripts los lee el call site COMUN de JsonObject, como los constraints)
        return g;
    }
    if (type == "ui") {
        std::string file = JS(j, "file", "");
        // una escena UI que es CONTENIDO DE UNA LIBRERIA: sus rutas (texturas, fuentes, scripts) son de ELLA
        // (UI2DFormato: g_w3dPathLibHook), no del proyecto
        GuardPathUILib guardUI(!W3dLibContext().empty());
        UI* u = LoadUIProject(file.empty() ? file : JsonPath(file, base), file);
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
    if (type == "mesh") {
        // ==================================================================
        //  LA RAMA DE HOY: la geometria es una entrada .w3dm del contenedor.
        //  Es interna SIEMPRE (el proyecto es autocontenido por construccion),
        //  asi que no depende de ningun archivo del disco del usuario.
        //  "origen" es solo INFORMATIVO ("de donde salio" / "reimportar"): si
        //  falta, se avisa y NO pasa nada, la malla ya esta adentro.
        // ==================================================================
        std::string geo = JS(j, "geometry", "");
        // LA MALLA COMO RECURSO (objects/MeshResource.h): el objeto nombra una malla del
        // registro del proyecto ("mallas") y la COMPARTE con los demas que la usan: se lee una
        // sola vez para todos, sin derivar nada, y la edicion queda pendiente hasta que alguien
        // la pida. "geometria" (una entrada propia del objeto) es el formato anterior.
        const std::string nameMesh = ResourceNameJson(JS(j, "mesh", ""));   // (con el prefijo de su libreria)
        Mesh* mesh = new Mesh(parent, Vector3(0, 0, 0));
        mesh->onlyRoutine = JB(j, "routineOnly", false);   // la dibuja una rutina (Mesh.h)
        mesh->lockNormals = JB(j, "lockNormals", false);   // Bloquear normales (Mesh.h)
        { JVal* ju = JChild(j, "uvMaterials", 5);   // el UV PERSONALIZADO de los mapas de sus materiales
          if (ju) for (size_t c = 0; c < ju->list.size(); c++) {
              JVal* ue = ju->list[c]; if (!ue || ue->type != 4) continue;
              const std::string mn = JS(ue, "material", ""), mp = JS(ue, "map", ""), un = JS(ue, "uv", "");
              const int ix = JI(ue, "index", -1);
              if (mn.empty() || W3dMatMapByKey(mp) < 0) continue;
              if (!un.empty()) mesh->uvPersonal[mn + "\x1f" + mp] = un;
              else if (ix >= 0) mesh->uvPersonalIdx[mn + "\x1f" + mp] = ix;
          } }
        std::vector<unsigned char> data;
        W3dMeshInfo info;
        bool loaded = false;
        // medicion por sub-fase (0 = nadie mide): leer la entrada / parsear el .w3dm / derivados
        double tMesh = LoadNow();
        bool read = false;
        MeshResource* recMesh = NULL;
        if (!nameMesh.empty()) {
            mesh->noEditable = JB(j, "noEditable", false);   // el AABB del recurso depende del modo
            recMesh = W3dMeshResourceByName(nameMesh);
            // UNA MALLA DE UNA LIBRERIA QUE NO ESTA (desvinculada, o su .w3d falta): el objeto queda vacio pero la
            // REFERENCIA se conserva (io/Libraries.h: W3dLibRefAnnotate). El guardado la escribe tal cual -sin frenar:
            // no hay geometria del proyecto que perder- y vincular la libreria la resuelve. Solo del PROYECTO: lo que
            // genera un proxy no se guarda
            bool refLib = false;
            if (!recMesh && W3dLibContext().empty() && !W3dLibPrefixOf(nameMesh).empty()) {
                W3dLibRef ref; ref.name = nameMesh;
                W3dLibRefAnnotate(mesh, W3D_LIBREF_MESH, ref);
                refLib = true;
                w3dLogfW("[W3D] la malla '%s' es de la libreria '%s', que no esta: el objeto queda vacio hasta vincularla "
                         "(la referencia se conserva)", nameMesh.c_str(), W3dLibPrefixOf(nameMesh).c_str());
                W3dAvisof(false, T("'%s' uses the mesh '%s' from a missing library: it stays empty until you link it"),
                          W3dShortName(JS(j, "name", "")).c_str(), W3dShortName(nameMesh).c_str());
            } else if (!recMesh)
                w3dLogfE("[W3D] la malla '%s' no esta en el registro del proyecto (el objeto queda vacio)", nameMesh.c_str());
            else if (!(loaded = W3dMeshLink(mesh, recMesh))) {
                w3dLogfE("[W3D] no pude cargar la malla '%s' (%s): el objeto queda vacio",
                         nameMesh.c_str(), recMesh->entry.c_str());
                s_meshesRejectedBin++;
            }
            if (!loaded && !refLib) geo = recMesh ? recMesh->entry : nameMesh;   // para el aviso y el freno de abajo
            if (g_w3dLoadClock) { const double t = LoadNow(); g_w3dLoadPhases.meshParseMs += t - tMesh; tMesh = t; }
        } else if (geo.empty())
            w3dLogfE("[W3D] una malla del proyecto no dice de que archivo sale (queda vacia)");
        else if (!(read = w3dFileSystem::ReadFileBytes(JsonPath(geo, base), data) && !data.empty()))
            w3dLogfE("[W3D] no pude leer la geometria %s (la malla queda vacia)", geo.c_str());
        if (g_w3dLoadClock) { const double t = LoadNow(); g_w3dLoadPhases.meshReadMs += t - tMesh; tMesh = t; }
        // ESCENARIO CERRADO A EDICION: lo dice el proyecto (el .w3db hornea su AABB para el
        // modo con el que se guardo; si el proyecto pide el otro, el lector lo recalcula)
        const bool noEditable = JB(j, "noEditable", false);
        // EL .w3db (binario, io/W3dMeshBin.h): la malla sale LISTA de la entrada. Se reconoce
        // por su firma, no por la extension (un .w3d escrito a mano puede llamarla como quiera).
        // (La de un RECURSO ya salio lista del vinculo, arriba: cuenta como binaria.)
        const bool isBin = recMesh ? true : (read && W3dMeshBinIs(&data[0], data.size()));
        if (recMesh) {
            // (nada: el vinculo dejo la malla lista y la edicion pendiente del recurso)
        } else if (isBin) {
            W3dMeshBinOptions op;
            op.noEditable = noEditable;
            // el JUEGO no edita: solo los bloques de render. Los de edicion quedan PENDIENTES y se
            // leen recien si alguien pide los poligonos de la malla: su propio stack si GENERA
            // malla (se resuelve abajo, tras LoadModifiers) o un Boolean que la usa de
            // target (BuildPolyMesh). El PVS/Oclusion o un modificador apagado no los piden.
            if (MeshesAsGame()) op.edit = false;
            if (!(loaded = W3dMeshBinRead(&data[0], data.size(), mesh, &info, op))) {
                w3dLogfE("[W3D] %s no es un .w3db que este Whisk3D pueda leer (la malla queda vacia)", geo.c_str());
                s_meshesRejectedBin++;
            } else if (!op.edit) mesh->pendingEdit = JsonPath(geo, base);
        } else if (read && !(loaded = W3dMeshRead((const char*)&data[0], data.size(), mesh, &info)))
            w3dLogfE("[W3D] %s no es un .w3dm que este Whisk3D pueda leer (la malla queda vacia)", geo.c_str());
        if (g_w3dLoadClock) { const double t = LoadNow(); g_w3dLoadPhases.meshParseMs += t - tMesh; tMesh = t; }
        // ---- EL .w3dm ESTABA Y NO SE PUDO LEER: NI SILENCIO NI HORNEAR ----------
        //  El lector se NIEGA (no es W3DMESH / lexico de una version mas nueva / falta V o F)
        //  y aca ya hay un Mesh NUEVO y VACIO en la escena, con el nombre y el transform del
        //  proyecto pero sin geometria. Antes esto salia SOLO por el log: el usuario veia el
        //  objeto vacio, guardaba, y el primer guardado HORNEABA la nada (el .w3dm quedaba con
        //  0 puntos / 0 caras). Exactamente el degradado silencioso que el formato existe para
        //  impedir, y en el caso estrella: un .w3d escrito por un Whisk3D MAS NUEVO.
        //  Se marca la malla como "no cargo" (WriteMeshW3dm se niega a guardarla encima,
        //  igual que con el freno "requiere") y se avisa EN PANTALLA. El usuario puede seguir
        //  trabajando; lo que no puede es destruir el archivo sin enterarse.
        if (!loaded && !geo.empty()) {
            mesh->w3dmForeign.notLoaded = true;
            W3dAvisof(true, T("Could not read the geometry of '%s': it stays EMPTY and I will not save over it (see whisk3d.log)"),
                      W3dShortName(geo).c_str());
        }
        // SILENCIOSO JAMAS: cada problema del archivo sale por el log con su numero de
        // linea. FUERA del 'if (cargo)' a proposito: cuando la lectura FRACASA es cuando
        // mas falta hace el motivo (ej: "el archivo no trae el bloque F"), y adentro del
        // if se perdia justo en ese caso.
        for (size_t k = 0; k < info.notices.size(); k++)
            w3dLogfW("[W3D] %s: %s", geo.c_str(), info.notices[k].c_str());
        if (loaded) {
            if (info.readOnly)
                w3dLogfW("[W3D] %s pide un bloque que este Whisk3D no entiende: la malla abre pero guardar encima la degradaria", geo.c_str());
            // el material vive en la ESCENA y el .w3dm lo referencia POR NOMBRE (bloque PARTMAT):
            // el Core no puede resolver el puntero, lo hace el editor con el bloque "materiales".
            // (El de un RECURSO lo resolvio su primera carga: W3dMeshMaterialByNameHook.)
            if (!recMesh)
                for (size_t k = 0; k < mesh->materialsGroup.size() && k < info.materials.size(); k++)
                    mesh->materialsGroup[k].material = MaterialByName(info.materials[k]);
            mesh->noEditable = noEditable;
            if (isBin && recMesh && recMesh->textBytes) {
                // UNA MALLA DEL REGISTRO EN TEXTO (.w3dm): nada que derivar para ESTE objeto (el
                // vinculo la dejo lista), pero la malla se derivo al abrir (la conversion del texto,
                // una vez por malla): cuenta como .w3dm, igual que la "geometria" de texto de antes
                if (g_w3dLoadClock) g_w3dLoadPhases.derivedMeshes++;
                s_derivedMeshes++;
            } else if (isBin) {
                // NADA QUE DERIVAR: el index buffer (agrupado + Forsyth), las aristas, el AABB,
                // las capas activas en el render y el mapa de puntos vinieron hechos del guardado.
                if (g_w3dLoadClock) g_w3dLoadPhases.meshesBin++;
                s_binaryMeshes++;
            } else {
                // el cierre que el lector de TEXTO no hace (es del Core y no puede llamar al editor):
                // index buffer + rangos por mesh part, bordes/posRep, y las capas activas al render.
                mesh->RegroupMeshParts();
                // ESCENARIO CERRADO A EDICION: mismo trato que ImportWOBJ con noEditable
                // (solo el AABB; sin posRep/edges la malla queda identica a la guardada)
                if (mesh->noEditable) mesh->ComputeAABBOnly();
                else mesh->ComputeBorders(true, false);   // CONSERVA los puntos del archivo (no re-suelda por posicion)
                mesh->ApplyLayersToRender();
                // el mapeo render-vert -> control-point (vertCtrlPoint = posRep). SIN ESTO el skinning
                // 3D se corta en seco (SkinMesh sale si el mapa no cubre los verts) y la malla
                // rigueada abria SIN DEFORMAR, en silencio.
                WeightPaintEnsureMap(mesh);
                if (g_w3dLoadClock) g_w3dLoadPhases.derivedMeshes++;
                s_derivedMeshes++;
            }
            mesh->lastSkinFrame = -999999;   // los vertex groups del archivo reestrenan el skinning
            // lo que la fase de mallas gasta en DERIVAR (index buffer, bordes, capas, mapa de puntos)
            if (g_w3dLoadClock) g_w3dLoadPhases.derivedMeshMs += LoadNow() - tMesh;
        }
        CommonJson(j, mesh);   // nombre/transform del PROYECTO (manda sobre el "nombre" del .w3dm)
        // LAS CAPAS DE MATERIAL de encima de cada parte (MaterialGroup::layersMat), por nombre (los materiales ya cargaron)
        { JVal* jc = JChild(j, "materialLayers", 5);
          if (jc) for (size_t c = 0; c < jc->list.size(); c++) {
              JVal* e = jc->list[c]; if (!e || e->type != 4) continue;
              const int part = JI(e, "part", -1);
              JVal* lm = JChild(e, "materials", 5);
              if (!lm || part < 0 || part >= (int)mesh->materialsGroup.size()) continue;
              std::vector<Material*>& cm = mesh->materialsGroup[(size_t)part].layersMat;
              cm.clear();
              for (size_t k = 0; k < lm->list.size(); k++) {
                  if (!lm->list[k] || lm->list[k]->type != 2) continue;
                  Material* mt = MaterialByName(lm->list[k]->str);
                  if (mt) cm.push_back(mt);
              }
          } }
        // ORIGEN: el .obj/.fbx que trajo el usuario. Externo a proposito (lo edita otro programa).
        std::string org = JS(j, "origin", "");
        if (!org.empty()) {
            mesh->origin = JsonPath(org, base);
            if (!w3dFileSystem::FileExists(mesh->origin))
                w3dLogfW("[W3D] '%s': el archivo de origen '%s' no esta. La malla carga igual (viaja adentro del .w3d); solo no vas a poder reimportarla del original",
                         mesh->name.c_str(), mesh->origin.c_str());
        }
        LoadAnimsVertex(j, mesh, base);   // vertex anims (blob binario + curvas + rango)
        LoadRigMesh(j, mesh);             // modArmature + armatures 2D (los grupos vienen del .w3dm)
        LoadModifiers(j, mesh);           // el resto del stack (Mirror/Screw/SubSurf/...)
        // EL JUEGO: una malla cuyo stack GENERA malla (Mirror, SubSurf, Screw, Boolean... encendido)
        // arma sus poligonos apenas se resuelvan los modificadores (gPendModGen): su edicion se
        // materializa YA, de los bytes que estan en memoria (sin releer la entrada). Un stack que
        // no genera nada (solo el PVS/Oclusion, o todo apagado) la deja pendiente: la malla mas
        // grande de un nivel con PVS no carga caras, capas ni aristas que nadie va a usar.
        if (!mesh->pendingEdit.empty() && W3dStackGeneratesMesh(mesh)) {
            if (recMesh) W3dMeshBinMaterializeEdit(mesh);   // de los bytes del RECURSO
            else W3dMeshBinMaterializeEdit(mesh, &data[0], data.size());
        }
        LoadAnimUV(j, mesh);              // animacion UV "tira de atlas" (autoplay, propia de la malla)
        { std::string fbn = ResourceNameJson(JS(j, "flipbook", ""));   // o un flipbook CON NOMBRE compartido (SceneFlipbooks)
          if (!fbn.empty()) { Flipbook* fb = FlipbookByName(fbn);
              if (fb) mesh->UseFlipbook(fb, JI(j, "flipOffset", 0)); } }
        return mesh;
    }
    if (type == "model") {   // wavefront referenciado + sus vertex anims
        std::string file = JsonPath(JS(j, "file", ""), base);
        Mesh* mesh = ImportWOBJ(file, parent, false);
        if (!mesh) { w3dLogfE("[W3D] no pude importar el modelo %s (objeto omitido)", file.c_str()); return NULL; }
        if (g_w3dLoadClock) g_w3dLoadPhases.derivedMeshes++;   // ImportWOBJ deriva todo al abrir
        CommonJson(j, mesh);
        LoadAnimsVertex(j, mesh, base);   // vertex anims (blob binario + curvas + rango)
        LoadRigMesh(j, mesh);             // vgroups por posicion + referencia modArmature (Fase 3)
        LoadModifiers(j, mesh);           // el resto del stack (Mirror/Screw/SubSurf/...)
        LoadAnimUV(j, mesh);              // animacion UV "tira de atlas" (autoplay, propia de la malla)
        { std::string fbn = ResourceNameJson(JS(j, "flipbook", ""));   // o un flipbook CON NOMBRE compartido (SceneFlipbooks)
          if (!fbn.empty()) { Flipbook* fb = FlipbookByName(fbn);
              if (fb) mesh->UseFlipbook(fb, JI(j, "flipOffset", 0)); } }
        return mesh;
    }
    if (type == "glb") {      // malla modelada, exportada a GLB dentro del zip
        std::string file = JsonPath(JS(j, "file", ""), base);
        size_t prev = SceneCollection ? SceneCollection->Childrens.size() : 0;
        extern bool ImportModelByExtension(const std::string& path);
        ImportModelByExtension(file);
        Object* created = NULL;
        // lo importado quedo al FINAL de la escena: aplicarle nombre/transform
        if (SceneCollection && SceneCollection->Childrens.size() > prev) {
            Object* newValue = SceneCollection->Childrens.back();
            // EL GLB DE ADENTRO DEL .w3d NO ES UN "ORIGEN": es una entrada del propio
            // contenedor. Se limpia el SUBARBOL ENTERO (no solo el nodo de arriba: un
            // armature con la malla como hija dejaba a la hija con una ruta INTERNA y
            // esa ruta terminaba en EXTERNALS.txt como archivo que falta). Va ANTES del
            // reparent para que no dependa de donde quede colgado.
            ClearOriginRec(newValue);
            // el import cuelga SIEMPRE de la raiz; si este nodo venia como HIJO (ej: malla bajo su
            // armature), re-colgarlo del padre real (preserva la jerarquia del rig al reabrir).
            if (parent && parent != SceneCollection) {
                SceneCollection->Childrens.pop_back();
                newValue->Parent = parent;
                parent->Childrens.push_back(newValue);
            }
            CommonJson(j, newValue);
            if (newValue->getType() == ObjectType::mesh) {
                // QUADS/NGONS: el GLB solo trae triangulos -> reconstruir los poligonos (y el orden de
                // los render-verts) desde el bloque "topologia". VA PRIMERO: los uvgroups se matchean
                // contra los render-verts y la vertex anim compara su vcount contra vertexSize.
                RetopologizeFromTopology(j, (Mesh*)newValue);
                LoadAnimsVertex(j, (Mesh*)newValue, base);   // vertex anims de la malla modelada
                LoadRigMesh(j, (Mesh*)newValue);             // vgroups por posicion + modArmature (Fase 3)
                LoadModifiers(j, (Mesh*)newValue);           // el resto del stack (Mirror/Screw/SubSurf/...)
            }
            created = newValue;
        }
        return created;
    }
    if (type == "armature") { // ARMATURE (Fase 3): huesos + clips + hijos
        Armature* a = new Armature(parent);
        CommonJson(j, a);
        bool authored = JB(j, "authored", false);
        a->skinGltf = !authored && JB(j, "gltf", false); // rig glTF: FK Y-up + skin = world * invBind
        JVal* jb = JChild(j, "bones", 5);
        if (jb) for (size_t i = 0; i < jb->list.size(); i++) {
            JVal* e = jb->list[i]; if (!e || e->type != 4) continue;
            W3dBone b;
            b.name = JS(e, "name", "Bone");
            b.parent = JI(e, "parent", -1);
            b.connected = !JB(e, "loose", false); // "Disconnect Bone": emparentado sin soldar
            JVec3(e, "head", b.head); JVec3(e, "tail", b.tail);
            if (JB(e, "rest", false)) { // rig importado guardado con fidelidad de FK/skinning
                b.hasRest = true;
                JVec3(e, "restT", b.restT); JVec3(e, "restR", b.restR); JVec3(e, "restS", b.restS);
                JVec3(e, "preRot", b.preRot); JVec3(e, "postRot", b.postRot);
                b.rotOrder = JI(e, "rotOrder", 0);
                JMat16(e, "bind", b.bind); JMat16(e, "cluster", b.clusterTransform);
                if (a->skinGltf) { // la inverseBindMatrix; sin el campo (guardado viejo) = inversa del bind
                    b.skinInvBind = b.bind.Inverse();
                    JMat16(e, "invBind", b.skinInvBind);
                }
            }
            b.poseT = b.restT; b.poseR = b.restR; b.poseS = b.restS;
            a->bones.push_back(b);
        }
        // jerarquia SANEADA (indices fuera de rango / ciclos -> raiz): los recorridos que suben
        // por 'parent' asumen que la cadena termina (ver SanitizeParents)
        { std::vector<int> pad(a->bones.size());
          for (size_t i = 0; i < a->bones.size(); i++) pad[i] = a->bones[i].parent;
          if (SanitizeParents(pad, a->name.c_str()))
              for (size_t i = 0; i < a->bones.size(); i++) a->bones[i].parent = pad[i]; }
        // CLIPS: "anims" es el NOMBRE de un animset del registro (+ "clips": que indices usa, en
        // su orden) o, en los .w3d de antes, la LISTA de clips inline (se sigue leyendo; el
        // proximo guardado la migra a un animset). Ver animation/W3dAnimSet.h.
        std::string animSet = ResourceNameJson(JS(j, "anims", ""));   // (con el prefijo de su libreria)
        if (!animSet.empty()) {
            std::vector<int> idx; bool withIdx = false;
            JVal* jcl = JChild(j, "clips", 5);
            if (jcl) { withIdx = true;
                       for (size_t i = 0; i < JRowLen(jcl); i++) idx.push_back((int)JRowNum(jcl, i, -1.0f)); }
            std::string reason;
            if (!W3dArmatureAssignAnimSet(a, animSet, withIdx ? &idx : NULL, &reason)) {
                if (W3dLibContext().empty() && !W3dLibPrefixOf(animSet).empty() && W3dAnimSetsEntryOf(animSet).empty()) {
                    // EL ANIMSET DE UNA LIBRERIA QUE NO ESTA (desvinculada, o su .w3d falta): el esqueleto queda sin
                    // clips pero la REFERENCIA se conserva (io/Libraries.h): se guarda tal cual -sin frenar: no hay
                    // clips del proyecto que perder- y vincular la libreria la resuelve
                    W3dLibRef ref; ref.name = animSet; ref.clips = idx; ref.withClips = withIdx;
                    W3dLibRefAnnotate(a, W3D_LIBREF_ANIMSET, ref);
                    w3dLogfW("[W3D] '%s': su animset '%s' es de una libreria que no esta: sin clips hasta vincularla "
                             "(la referencia se conserva)", a->name.c_str(), animSet.c_str());
                    W3dAvisof(false, T("'%s' uses the animations '%s' from a missing library: no clips until you link it"),
                              W3dShortName(a->name).c_str(), W3dShortName(animSet).c_str());
                } else {
                // NI SILENCIO NI BORRARLO: el armature queda sin clips, se avisa en pantalla y el
                // guardado se FRENA (guardar encima lo dejaria sin animaciones para siempre)
                a->animSetNotLoaded = animSet;
                w3dLogfE("[W3D] '%s': no pude cargar sus clips (animset '%s'): %s",
                         a->name.c_str(), animSet.c_str(), reason.c_str());
                W3dAvisof(true, T("Could not read the animations of '%s' (%s): they stay EMPTY and I will not save over it (see whisk3d.log)"),
                          W3dShortName(a->name).c_str(), W3dShortName(animSet).c_str());
                }
            }
        }
        JVal* ja = JChild(j, "anims", 5);
        if (ja) for (size_t i = 0; i < ja->list.size(); i++) {
            JVal* e = ja->list[i]; if (!e || e->type != 4) continue;
            SkeletalAnimation* an = new SkeletalAnimation(JS(e, "name", "Animation"));
            an->FrameRate  = JI(e, "fps", 24);
            an->startFrame = JI(e, "start", 1);
            an->endFrame   = JI(e, "end", 0);
            JVal* jt = JChild(e, "tracks", 5);
            if (jt) for (size_t t = 0; t < jt->list.size(); t++) {
                JVal* tj = jt->list[t]; if (!tj || tj->type != 4) continue;
                BoneTrack tr;
                tr.bone = JI(tj, "bone", -1);
                JVal* jc = JChild(tj, "curves", 5);
                if (jc) for (size_t c = 0; c < jc->list.size(); c++) {
                    AnimProperty ap;
                    if (LoadCurveJson(jc->list[c], ap)) tr.Properties.push_back(ap);
                }
                an->tracks.push_back(tr);
            }
            a->animations.push_back(an);
        }
        a->activeAnim = JI(j, "activeAnim", a->animations.empty() ? -1 : 0);
        ReadLayers(JChild(j, "layers", 5), a->layers);
        a->activeLayer = a->layers.empty() ? -1 : 0;
        if (a->activeAnim < -1 || a->activeAnim >= (int)a->animations.size())
            a->activeAnim = a->animations.empty() ? -1 : 0;
        if (authored) PrepareAuthoredSkin(a);          // rig del editor: rest/skin desde head/tail
        else { bool hasRest = false;
               for (size_t i = 0; i < a->bones.size(); i++) if (a->bones[i].hasRest) { hasRest = true; break; }
               if (hasRest) PrepareSkin(a); }          // rig importado: recomputar matrices de skinning
        return a;
    }
    if (type == "lod") {   // LOD: un hijo por distancia a la camara (los hijos los lee JsonObject)
        LOD* l = new LOD(parent);
        CommonJson(j, l);
        JVal* dl = JChild(j, "distances", 5);   // lista de numeros (mismo idioma que "color")
        if (dl) for (size_t i = 0; i < dl->list.size(); i++)
            if (dl->list[i]) l->distances.push_back((float)dl->list[i]->num);
        l->onlyCameraActive = JB(j, "activeCameraOnly", false); // ausente en los .w3d viejos
        return l;
    }
    if (type == "sound") {    // SONIDO: audio puesto en la escena (los tiempos en segundos)
        Sound* so = new Sound(parent);
        CommonJson(j, so);
        so->file       = JS(j, "file", "");
        so->active     = JB(j, "active", true);
        so->volume     = JF(j, "volume", 1.0f);
        so->tone       = JF(j, "pitch", 1.0f);
        so->reverb     = JF(j, "reverb", 0.0f);
        so->pan        = JF(j, "pan", 0.0f);
        so->start      = JF(j, "start", 0.0f);
        so->clipStart  = JF(j, "clipStart", 0.0f);
        so->clipEnd    = JF(j, "clipEnd", 0.0f);
        so->loop       = JB(j, "loop", false);
        so->positional = JB(j, "positional", false);
        so->radius     = JF(j, "radius", so->radius);
        so->range      = JF(j, "range", so->range);
        return so;
    }
    if (type == "fog") {    // NIEBLA: glFog puesta en el arbol (solo en Render)
        Fog* ni = new Fog(parent);
        CommonJson(j, ni);
        ni->active   = JB(j, "active", true);
        std::string m = JS(j, "mode", "linear");
        ni->mode    = (m == "exp2") ? 2 : (m == "exp" ? 1 : 0);
        ni->start   = JF(j, "start", ni->start);
        ni->end     = JF(j, "end", ni->end);
        ni->density = JF(j, "density", ni->density);
        JVal* col = JChild(j, "color", 5);
        if (col && col->list.size() >= 3)
            for (int i = 0; i < 3; i++) ni->color[i] = (float)col->list[i]->num;
        ni->background    = JB(j, "background", false);
        return ni;
    }
    if (type == "postProcess") {   // POSTPROCESADO: sus efectos sobre lo ya dibujado en ese punto del arbol
        PostProcess* pp = new PostProcess(parent);
        CommonJson(j, pp);
        W3dPostRead(j, pp->opt);
        return pp;
    }
    if (type == "clearz") {  // LIMPIAR Z: borra la profundidad en ese punto del arbol
        ClearZ* lz = new ClearZ(parent);
        CommonJson(j, lz);
        lz->active = JB(j, "active", true);
        return lz;
    }
    if (type == "clip") {   // RECORTE: los hijos se dibujan en un rectangulo (fracciones, origen en el centro)
        ClipRegion* rc = new ClipRegion(parent);
        CommonJson(j, rc);
        rc->active     = JB(j, "active", true);
        rc->x          = JF(j, "x", 0.0f);
        rc->y          = JF(j, "y", 0.0f);
        rc->width      = JF(j, "width", 1.0f);
        rc->height     = JF(j, "height", 1.0f);
        rc->camera     = JS(j, "camera", "");
        rc->clearZ     = JB(j, "clearZ", false);
        rc->background = JB(j, "background", false);
        JVal* col = JChild(j, "color", 5);
        if (col) for (size_t i = 0; i < col->list.size() && i < 4; i++) rc->color[i] = (float)col->list[i]->num;
        return rc;
    }
    if (type == "reflectionprobe") {   // SONDA DE REFLEJO: su imagen 360 (objects/ReflectionProbe.cpp)
        ReflectionProbe* sr = new ReflectionProbe(parent);
        CommonJson(j, sr);
        ReflectionProbeReadFields(j, sr);
        return sr;
    }
    if (type == "sondaluz") {  // SONDA DE LUZ: la grilla con la luz indirecta horneada (objects/LightProbe.cpp)
        LightProbe* sl = new LightProbe(parent);
        CommonJson(j, sl);
        LightProbeReadFields(j, sl);
        return sl;
    }
    if (type == "hitbox") {    // HITBOX: caja de deteccion con eventos lua (los campos: objects/Hitbox.cpp)
        Hitbox* hb = new Hitbox(parent);
        CommonJson(j, hb);
        HitboxReadFields(j, hb);
        return hb;
    }
    if (type == "routine") {    // Rutina: lista de pasos del render (sin transformacion propia)
        Routine* ru = new Routine(parent);
        CommonJson(j, ru);
        RoutineReadFields(j, ru, RoutineRefTexture, (void*)&base);   // ("Bind texture": como la de un material)
        return ru;
    }
    if (type == "culling") {   // Culling: contenedor de culling (frustum/grid/triangulo/bsp)
        Culling* cu = new Culling(parent);
        CommonJson(j, cu);
        cu->method           = CullingMethodFrom(JS(j, "method", "frustum")); // ausente = frustum (retrocompat)
        cu->active           = JB(j, "active", true);         // ausente = prendido
        cu->onlyCameraActive = JB(j, "activeCameraOnly", false);
        cu->distanceMax      = JF(j, "maxDistance", 0.0f);   // 0 = sin limite (archivos viejos)
        cu->orderAlpha       = JB(j, "alphaOrder", false);    // translucido: cullea + ordena back->forward
        cu->orderProximity   = JB(j, "distanceOrder", false); // opaco: forward->back puro (sin agrupar por material)
        cu->cellSize         = JF(j, "cellSize", 16.0f);      // metodo grid
        cu->mode3D           = JB(j, "mode3D", false);        // metodo grid
        cu->railName         = JS(j, "rail", "");             // metodo riel: la Curve del recorrido
        cu->visChildrenFile  = JsonPathField(JS(j, "visChildren", ""), base);   // metodo riel: hijos visibles por nodo
        cu->bspFile          = JsonPathField(JS(j, "bsp", ""), base);        // metodo bsp: arbol + PVS + clusters por hijo (.w3dbsp)
        cu->visibleMemory    = JS(j, "visibleMemory", "");                 // metodo bsp: el PVS por hijo a una memoria (rutinas)
        return cu;
    }
    if (type == "gridcull") {   // COMPAT: gridcull viejo -> Culling con metodo=Grid (objeto unificado)
        Culling* g = new Culling(parent);
        g->method           = Culling::Grid;
        CommonJson(j, g);
        g->active           = JB(j, "active", true);          // ausente = prendido
        g->cellSize         = JF(j, "cellSize", 16.0f);
        g->mode3D           = JB(j, "mode3D", false);
        g->distanceMax      = JF(j, "maxDistance", 0.0f);    // 0 = sin limite
        g->onlyCameraActive = JB(j, "activeCameraOnly", false);
        g->orderAlpha       = JB(j, "alphaOrder", false);     // celdas de lejos a cerca (transparencias)
        return g;
    }
    if (type == "viszona") {   // VisZone: celda de visibilidad (ver VisZone.h)
        VisZone* vz = new VisZone(parent);
        CommonJson(j, vz);
        std::string mode = JS(j, "mode", "manual");
        vz->mode = (mode == "grid") ? VisZone::ModeGrid
                 : (mode == "volumes") ? VisZone::ModeVolumes
                 : (mode == "curve") ? VisZone::ModeCurve : VisZone::ModeManual;
        vz->targetName = JS(j, "target", "");
        vz->anchorName = JS(j, "anchor", "");
        vz->nx       = JI(j, "nx", 1);
        vz->ny       = JI(j, "ny", 1);
        vz->nz       = JI(j, "nz", 1);
        vz->cellSize = JF(j, "cellSize", 1.0f);
        vz->stepMax  = JI(j, "maxStep", 32);
        // multi-riel (ver VisZone.h): riel que indexa las celdas + celda fallback
        vz->railName     = JS(j, "rail", "");
        vz->cellFallback = JI(j, "fallback", 0);
        return vz;
    }
    if (type == "particles") {   // Particulas: emisor de billboards (ver Particles.h)
        Particles* pt = new Particles(parent);
        CommonJson(j, pt);
        pt->texture    = JsonPathField(JS(j, "texture", ""), base);   // entrada del contenedor v4 (o ruta externa)
        pt->count      = JF(j, "count",   pt->count);
        pt->life       = JF(j, "life",       pt->life);
        pt->size       = JF(j, "size",        pt->size);
        pt->vel        = JF(j, "vel",        pt->vel);
        pt->dispersion = JF(j, "dispersion", pt->dispersion);
        pt->gravity    = JF(j, "gravity",   pt->gravity);
        // MEZCLA: v4 nuevo trae "mezcla" (int); archivos viejos, aditivo/sustractivo (bool)
        int mz = JI(j, "mix", -1);
        if (mz >= 0)                          pt->mix = mz;
        else if (JB(j, "sustractivo", false)) pt->mix = w3dEngine::MixSubtract;
        else if (JB(j, "additive",     false)) pt->mix = w3dEngine::MixAdd;
        JVal* c = JChild(j, "color", 5);   // [r, g, b, a] (mismo idioma que "ejes")
        if (c) for (size_t i = 0; i < c->list.size() && i < 4; i++)
            if (c->list[i]) pt->color[i] = (float)c->list[i]->num;
        pt->fade     = JB(j, "fade", pt->fade);
        pt->active   = JB(j, "active",     pt->active);
        pt->filtered = JB(j, "filtered",   pt->filtered);   // ausente -> true (suave)
        // azar (ausente en archivos v4 viejos -> 0: mismo comportamiento de antes)
        pt->variation  = JF(j, "variation",   pt->variation);
        pt->turbulence = JF(j, "turbulence", pt->turbulence);
        pt->emissionRadius = JF(j, "emissionRadius", pt->emissionRadius);
        // rotacion del billboard (ausente -> false/0: derecho y quieto, como antes)
        pt->rotation    = JB(j, "rotation",    pt->rotation);
        pt->velRotation = JF(j, "rotationSpeed", pt->velRotation);
        pt->flipFrames  = JI(j, "flipFrames", pt->flipFrames);   // flipbook por edad
        pt->flipCols    = JI(j, "flipCols",    pt->flipCols);
        pt->flipRows    = JI(j, "flipRows",   pt->flipRows);
        // SUB-RECT del atlas unico: [u0, v0, u1, v1] (ausente = textura entera)
        {
            JVal* r = JChild(j, "uvRect", 5);
            if (r && r->list.size() >= 4)
                for (int k = 0; k < 4; k++)
                    if (r->list[k]->type == 1) pt->uvRect[k] = (float)r->list[k]->num;
        }
        // LO AVANZADO (ausente = el default del ctor = el emisor de siempre)
        {
            const char* v3[4] = { "randomVel", "randomPos", "acceleration", "velLocal" };
            float* d3[4] = { pt->velRandom, pt->posRandom, pt->acceleration, pt->velLocal };
            for (int q = 0; q < 4; q++) {
                JVal* r = JChild(j, v3[q], 5);
                if (r) for (size_t k = 0; k < r->list.size() && k < 3; k++)
                    if (r->list[k] && r->list[k]->type == 1) d3[q][k] = (float)r->list[k]->num;
            }
            JVal* cf = JChild(j, "colorFinal", 5);
            if (cf) {
                pt->useColorFinal = true;
                for (size_t k = 0; k < cf->list.size() && k < 4; k++)
                    if (cf->list[k] && cf->list[k]->type == 1) pt->colorFinal[k] = (float)cf->list[k]->num;
            }
        }
        pt->drag           = JF(j, "drag",         pt->drag);
        pt->growth         = JF(j, "growth",      pt->growth);
        pt->brakeGrowth    = JF(j, "growthBrake", pt->brakeGrowth);
        pt->alphaDecays    = JF(j, "alphaDecay",       pt->alphaDecays);
        pt->alphaDeath     = JF(j, "alphaDeath",      pt->alphaDeath);
        pt->initialRot     = JF(j, "initialRot",       pt->initialRot);
        pt->rotRandom      = JF(j, "randomRot",          pt->rotRandom);
        pt->velRotRandom   = JF(j, "randomRotVel",       pt->velRotRandom);
        pt->turnSignRandom = JB(j, "randomSpinSign",    pt->turnSignRandom);
        pt->appear         = JF(j, "appear",         pt->appear);
        {   // fundidos por distancia a la camara: [desde, hasta] (m)
            JVal* fc = JChild(j, "fadeNear", 5);
            if (fc && fc->list.size() >= 2) { pt->fadeNear[0] = (float)fc->list[0]->num; pt->fadeNear[1] = (float)fc->list[1]->num; }
            JVal* fl = JChild(j, "fadeFar", 5);
            if (fl && fl->list.size() >= 2) { pt->fadeFar[0] = (float)fl->list[0]->num; pt->fadeFar[1] = (float)fl->list[1]->num; }
        }
        pt->flipFps        = JF(j, "flipFps",          pt->flipFps);
        pt->flipOnce       = JB(j, "flipOnce",       pt->flipOnce);
        {   // forma: "billboard" | "estirada" | "linea" (string: el enum no viaja como numero)
            std::string f = JS(j, "shape", "billboard");
            pt->shape = (f == "stretched") ? w3dEngine::ShapeStretched : (f == "line") ? w3dEngine::ShapeLine : w3dEngine::ShapeBillboard;
        }
        pt->stretch           = JF(j, "stretch",     pt->stretch);
        pt->trailPoints       = JI(j, "trailPoints",     pt->trailPoints);
        pt->trailStep         = JF(j, "trailStep",       pt->trailStep);
        pt->thicknessLine     = JF(j, "lineThickness",      pt->thicknessLine);
        return pt;
    }
    if (type == "prefab") {    // INSTANCIA DE PREFAB: el nodo + su prefab + sus overrides; los hijos se GENERAN
        InstancePrefab* ip = new InstancePrefab(parent);
        CommonJson(j, ip);
        InstancePrefabReadFields(j, ip);
        // adentro de una LIBRERIA, un prefab anidado es un prefab de ESA libreria (su clave en la cache)
        if (!W3dLibContext().empty() && !ip->prefab.empty())
            ip->prefab = W3dLibsKey(W3dLibContext(), W3D_LIB_PREFAB, ip->prefab);
        // (el guardado no escribe "hijos" de una instancia: lo que tiene sale de la definicion del prefab)
        // una DIFERIDA (io/Streaming.h) en el juego compilado -o en el editor con la vista previa del streaming- no
        // genera nada al abrir: lo genera el streaming cuando su objetivo se acerque
        if (!W3dStreamingDeferOnLoad(ip)) W3dPrefabGenerate(ip);
        return ip;
    }
    if (type == "proxy") {     // PROXY W3D: un prefab o una escena de una LIBRERIA; los hijos se GENERAN (solo lectura)
        ProxyW3d* px = new ProxyW3d(parent);
        CommonJson(j, px);
        ProxyW3dReadFields(j, px);
        if (!W3dStreamingDeferOnLoad(px)) W3dPrefabGenerate(px);   // (una diferida: como la instancia)
        return px;
    }
    if (type == "object") {
        // el nodo GENERICO que escribe el 'else' de WriteObject (Empty y cualquier tipo
        // sin rama propia): antes NO tenia rama de lectura, asi que el objeto se perdia y
        // sus hijos se re-colgaban del abuelo. Vuelve como Empty (el objeto vacio del editor,
        // que es lo que el usuario puede crear) con su nombre y su transform.
        Empty* o = new Empty(parent);
        CommonJson(j, o);
        // como se dibuja (solo la vista): sin estos campos (archivos de antes) queda el circulo en el piso
        const int fo = (int)JF(j, "shape", (float)o->shape);
        if (fo >= 0 && fo < EmptyShapes) o->shape = fo;
        o->drawingSize = JF(j, "drawSize", o->drawingSize);
        JVal* rd = JChild(j, "drawRot", 5);   // (5 = lista)
        if (rd && rd->list.size() >= 3) o->rotDrawing = Vector3((float)rd->list[0]->num, (float)rd->list[1]->num, (float)rd->list[2]->num);
        return o;
    }
    return NULL;   // tipo desconocido: sus hijos cuelgan del padre (nunca se pierde el arbol)
}

// ---------------------------------------------------------------------------
//  "padre2d": el nodo NO cuelga del UI, cuelga de uno de sus ELEMENTOS 2D (fallo B).
//
//  El .w3d escribe estos objetos como hijos del nodo UI porque el .w3dui no los sabe
//  llevar (solo escribe tipos 2D) y el .w3d no recorre el subarbol 2D; ver el bloque de
//  CollectGuestsUI en SaveW3D.cpp. Al llegar aca el .w3dui ya se cargo, o sea que el
//  elemento existe y se lo busca POR NOMBRE dentro de la escena (el espacio de nombres es
//  por escena: no hay dos elementos homonimos adentro de una UI).
//  Si el elemento no aparece -un .w3dui editado a mano, o una escena que no cargo- el
//  objeto se QUEDA colgado del UI: pierde su lugar exacto, pero NO se pierde (que es lo
//  que pasaba antes) y queda avisado en el log.
// ---------------------------------------------------------------------------
static Object* FindElement2D(Object* root, const std::string& name) {
    if (!root) return NULL;
    for (size_t i = 0; i < root->Childrens.size(); i++) {
        Object* h = root->Childrens[i];
        if (h->name == name) return h;
        Object* r = FindElement2D(h, name);
        if (r) return r;
    }
    return NULL;
}
static void ReHangFromParent2D(Object* o, Object* ui, const std::string& name) {
    if (!o || !ui || name.empty()) return;
    Object* dest = FindElement2D(ui, name);
    if (!dest || dest == o) {
        w3dLogfW("[W3D] '%s' referencia el elemento 2D '%s', que no esta en la escena: queda colgado del UI",
                 o->name.c_str(), name.c_str());
        return;
    }
    for (Object* q = dest; q; q = q->Parent) if (q == o) return;   // ciclo: no tocar
    Object* old = o->Parent ? o->Parent : SceneCollection;
    if (old) for (size_t k = 0; k < old->Childrens.size(); k++)
        if (old->Childrens[k] == o) { old->Childrens.erase(old->Childrens.begin() + k); break; }
    o->Parent = dest;
    dest->Childrens.push_back(o);
}

// ---------------------------------------------------------------------------
//  EL STACK DE CONSTRAINTS (simetrico con WriteConstraints del guardado).
//
//  Un solo call site, para CUALQUIER tipo de objeto: los constraints son una
//  propiedad de Object y no de una rama (una Coleccion tambien puede tener).
//
//  El 'tipo' llega como STRING: uno que esta version no conoce SE SALTEA con log y
//  el objeto abre igual. Perder un constraint es perder un efecto; abortar el nodo
//  seria perder el objeto y su subarbol, que es incomparablemente peor.
//
//  La FUENTE queda por NOMBRE nomas: el puntero lo resuelve
//  W3dConstraintsResolveNames al final de la carga, cuando ya existen todos los
//  objetos (aca todavia no existe el que viene despues en el archivo).
// ---------------------------------------------------------------------------
static int ConsTypeInt(const std::string& t) {
    if (t == "copyLocation") return W3dConstraintType::CopyLocation;
    if (t == "copyRotation") return W3dConstraintType::CopyRotation;
    if (t == "billboard")    return W3dConstraintType::Billboard;
    if (t == "childOf")      return W3dConstraintType::ChildOf;
    return -1;   // desconocido (un .w3d de una version mas nueva)
}

// capas del MIX (ver WriteLayers en SaveW3D.cpp): la misma forma para armatures y escenas
static void ReadLayers(JVal* jc, std::vector<W3dLayerAnim>& out) {
    out.clear();
    if (!jc) return;
    for (size_t i = 0; i < jc->list.size(); i++) {
        JVal* e = jc->list[i]; if (!e || e->type != 4) continue;
        W3dLayerAnim c;
        c.anim = JS(e, "anim", "");
        c.influence = JF(e, "influence", c.influence);
        { std::string m = JS(e, "mode", "mix"); c.mode = (m == "add") ? 1 : (m == "subtract") ? 2 : 0; }
        c.visible = JB(e, "visible", true);
        c.bone = JS(e, "bone", "");
        c.from = JF(e, "from", 0.0f);
        c.vel = JF(e, "vel", 1.0f);
        c.loop = JB(e, "loop", true);
        out.push_back(c);
    }
}

static void ReadConstraints(JVal* j, Object* o) {
    JVal* jc = JChild(j, "constraints", 5);
    if (!jc || !o) return;
    for (size_t i = 0; i < jc->list.size(); i++) {
        JVal* e = jc->list[i]; if (!e || e->type != 4) continue;
        const std::string ts = JS(e, "type", "");
        const int type = ConsTypeInt(ts);
        if (type < 0) {
            w3dLogfW("[W3D] '%s': constraint de tipo desconocido '%s': se ignora (el objeto abre igual)",
                     o->name.c_str(), ts.c_str());
            continue;
        }
        W3dConstraint* c = new W3dConstraint(type);
        c->name       = JS(e, "name", W3dConstraintTypeName(type));
        c->active     = JB(e, "active", true);
        c->showEdit = JB(e, "vered", false);   // ausente = OFF (el default; ver W3dConstraint.h)
        c->influence = JF(e, "influence", c->influence);
        if (type == W3dConstraintType::Billboard) {
            c->bbYaw   = JB(e, "yaw",   c->bbYaw);
            c->bbPitch = JB(e, "pitch", c->bbPitch);
        } else {
            // "fuente" es el TIPO de fuente y es explicito: cualquier cosa que no sea
            // "vista" es un objeto (que es el default y el caso del 99%).
            c->sourceType = (JS(e, "font", "object") == "view")
                                ? W3dConstraintSource::View : W3dConstraintSource::Object;
            if (c->sourceType == W3dConstraintSource::Object)
                c->sourceName = JS(e, "object", "");
            JBools3(e, "axes", &c->axisX, &c->axisY, &c->axisZ);
        }
        if (type == W3dConstraintType::ChildOf) {
            c->bone = JS(e, "bone", "");
            JBools3(e, "inherits", &c->coLoc, &c->coRot, &c->coEsc);
            JMat16(e, "inverse", c->inverse);   // ausente = identidad (el default del ctor)
        }
        // el archivo lo pudo editar cualquiera: clampea la influencia a [0,100] y arregla
        // el billboard que no gira en ningun eje (ver W3dConstraint::Normalizar)
        c->Normalize();
        o->constraints.push_back(c);
    }
    if (!o->constraints.empty()) o->activeConstraint = (int)o->constraints.size() - 1;
}

static void ReadClipsObject(JVal* j, Object* o);   // los "clips" del nodo (definida con las animaciones de escena)

// UN nodo del .w3d: el objeto + SUS HIJOS. La lectura de "hijos" esta ACA y en
// ningun otro lado (simetrico con WriteChildren del guardado).
// (la generacion de una instancia de prefab desde una definicion de MEMORIA pide, por cada objeto creado, el
//  "_origen" de su nodo: el serial del objeto de la plantilla del que salio. NULL = nadie lo pide)
static std::vector<std::pair<Object*, unsigned> >* gOrigins = NULL;
static void JsonObject(JVal* j, Object* parent, const std::string& base) {
    if (!j || j->type != 4) return;
    const double tMed = LoadNow();   // medicion por fase (0 = nadie mide)
    std::vector<std::pair<Object*, unsigned> >* origins = gOrigins;
    gOrigins = NULL;   // (una instancia anidada genera con su propia definicion: no es de esta plantilla)
    Object* created = JsonObjectCreate(j, parent, base);
    gOrigins = origins;
    if (gOrigins && created) gOrigins->push_back(std::make_pair(created, JU(j, "_origin", 0u)));
    // el nodo SOLO (los hijos se miden en su propia vuelta): mallas y armatures por separado
    if (g_w3dLoadClock && created) {
        const double dt = LoadNow() - tMed;
        if (created->getType() == ObjectType::mesh) {
            g_w3dLoadPhases.meshesMs += dt; g_w3dLoadPhases.meshes++;
        } else if (created->getType() == ObjectType::armature) {
            g_w3dLoadPhases.clipsMs += dt; g_w3dLoadPhases.armatures++;
            g_w3dLoadPhases.clips += (int)((Armature*)created)->animations.size();
        }
    }
    // el stack de constraints del nodo: DESPUES de crearlo (es donde viven) y ANTES de
    // bajar a los hijos, para que el orden del archivo sea el orden de la lista.
    if (created) ReadConstraints(j, created);
    // los SCRIPTS lua, de TODOS los tipos, en UN solo call site (espejo del
    // WriteScripts comun de SaveW3D). Antes solo gamepad/malla/modelo/glb
    // los leian: un script en una luz o un vacio se perdia al abrir.
    if (created) JsonScripts(j, created, base);
    // su BIBLIOTECA DE CLIPS DE JERARQUIA (animation/W3dAnimSet.h): un animset del registro "animsets".
    // Si no se puede leer queda anotada (notLoaded): el guardado se frena en vez de perderla.
    if (created) {
        const std::string lib = ResourceNameJson(JS(j, "hierarchyClips", ""));
        if (!lib.empty()) {
            std::string reason;
            if (!W3dHierAssign(created, lib, &reason))
                W3dAvisof(true, T("'%s': could not load its clips (%s)"), W3dShortName(created->name).c_str(), reason.c_str());
        }
        // su RETARGET PROPIO (un armature: el de sus clips de esqueleto; si no, el de sus clips de jerarquia) y
        // las CAPAS de clips de jerarquia de su Mix (ver WriteClipsHierarchy en SaveW3D.cpp)
        const std::string ret = JS(j, "retarget", "");
        if (!ret.empty()) {
            const int m = (ret == "rotations") ? W3D_RETARGET_ROTATIONS : W3D_RETARGET_COMPLETE;
            if (created->getType() == ObjectType::armature) W3dArmatureRetarget((Armature*)created, m);
            else W3dAnimObjectRetarget(created, m);
        }
        JVal* layersHier = JChild(j, "hierLayers", 5);
        if (layersHier) {
            if (!created->clipsHier) created->clipsHier = new W3dHierRoot();
            ReadLayers(layersHier, created->clipsHier->layers);
            created->clipsHier->activeLayer = created->clipsHier->layers.empty() ? -1 : 0;
            W3dHierLayersChanged(created);
        }
    }
    // los CLIPS DE OBJETO del formato de la primera version de esta fase ("clips" inline en el nodo): se
    // pasan a clips de jerarquia cuando ya estan todos sus objetos (AttachPendingClips)
    if (created) ReadClipsObject(j, created);
    const std::string parent2d = JS(j, "parent2d", "");
    if (created && !parent2d.empty() && parent && parent->getType() == ObjectType::ui)
        ReHangFromParent2D(created, parent, parent2d);
    // una INSTANCIA DE PREFAB no tiene hijos propios en el archivo: los genera su prefab. Unos "hijos"
    // escritos a mano se ignoran (si no, quedarian mezclados con lo generado y se borrarian al regenerar)
    if (created && W3dIsInstanceType(created->getType())) return;   // (un proxy tampoco: los genera su libreria)
    JVal* h = JChild(j, "children", 5);
    if (!h) return;
    Object* p = created ? created : parent;
    for (size_t i = 0; i < h->list.size(); i++) JsonObject(h->list[i], p, base);
    // un proyecto de antes: la limpieza de la pantalla venia ADENTRO del constructor (corre una vez); va afuera
    if (created && created->getType() == ObjectType::routine) W3dConstructorRemoveClear(created);
}

// CONFIG de la tarjeta Juego (bloque raiz "compilar", opcional): pisa los
// defaults ya reseteados en OpenW3D. Strings legibles ("ventana",
// "pantallaCompleta", ...); un valor desconocido o ausente deja el default del
// campo (W3dCompilar*Int). Solo existe en el camino JSON: los formatos viejos
// (zip v2 sin el campo, texto Whisk3D{}) quedan con los defaults.
namespace w3dEngine { void W3dAudioGameVolume(float v); }  // aplicar el volumen del juego (0..1) al mixer del Core

static void ApplyCompile(JVal* jc) {
    if (!jc) return;
#ifdef W3D_NO_EDITOR
    // la tarjeta "Juego" es del editor: en el juego COMPILADO estas opciones ya
    // estan horneadas en el binario (las aplico "Compilar juego" al generarlo).
    (void)jc;
#else
    g_projCompile.windowMode  = W3dCompileWindowModeInt(JS(jc, "windowMode", ""));
    g_projCompile.orientation = W3dCompileOrientationInt(JS(jc, "orientation", ""));
    g_projCompile.usePhysics  = JB(jc, "physics", true);
    g_projCompile.useSound    = JB(jc, "sound", true);
    g_projCompile.debugMode   = JB(jc, "debug", false);
    g_projCompile.assetsMode  = W3dCompileAssetsInt(JS(jc, "assets", ""));
    g_projCompile.platform    = W3dCompilePlatformInt(JS(jc, "platform", ""));
    g_projCompile.backend     = W3dCompileBackendInt(JS(jc, "backend", ""));      // auto|fijo|shaders|vulkan
    g_projCompile.uid         = (unsigned)strtoul(JS(jc, "uid", "0").c_str(), 0, 16);  // UID3 Symbian (hex string)
    g_projCompile.volume      = JIRange(jc, "volume", 100, 0, 100, "compile");        // 0..100 volumen del gameplay
    g_projCompile.resolution  = JIRange(jc, "resolution", 100, 25, 100, "compile");    // % de la pantalla (Android 2.3)
    w3dEngine::W3dAudioGameVolume(g_projCompile.volume / 100.0f);                      // aplicar YA la ganancia al mixer
    { extern void W3dGameVolRefUI(); W3dGameVolRefUI(); }                              // refrescar el numero de la tarjeta Juego
#endif
}

// ===========================================================================
//  MATERIALES (bloque raiz "materiales")
//
//  Antes viajaban ADENTRO del GLB de cada malla y volvian por ImportGLTF. Desde que la geometria
//  es .w3dm, el material vive en el project.json y cada .w3dm lo referencia POR NOMBRE (bloque
//  PARTMAT). Se cargan ANTES que los objetos para que la malla ya los encuentre.
//
//  REUSO POR NOMBRE, pero SOLO si es REALMENTE el mismo material: RestartScene borra la escena
//  pero NO el registro global de materiales, asi que sin esto reabrir el proyecto multiplicaria
//  los materiales (Material.001, .002, ...). Si el homonimo tiene OTRO contenido, el constructor
//  uniquifica y se avisa (mismo criterio que ImportGLTF).
// ===========================================================================
static std::map<std::string, Material*> gMatByName;   // nombre EN EL ARCHIVO -> material vivo

static bool W3dMatAlmost(float a, float b) { float d = a - b; if (d < 0) d = -d; return d <= 1e-4f; }
static void JColor4(JVal* j, const char* k, float* out) {
    JVal* v = JChild(j, k, 5);
    if (v && v->list.size() >= 4) for (int i = 0; i < 4; i++) out[i] = (float)v->list[i]->num;
}
// la ruta de la textura de un material YA existente: la cargada, o la que tiene ENCOLADA (la
// carga es diferida: recien cargado, mat->texture sigue NULL).
static std::string W3dMatTexPath(const Material* m) {
    if (m->texture && !m->texture->path.empty()) return m->texture->path;
    return PendingTextureOf(m);
}
static bool W3dSameMaterial(const Material* v, const Material& n, const std::string& texPath) {
    for (int k = 0; k < 4; k++) {
        if (!W3dMatAlmost(v->diffuse[k],  n.diffuse[k]))  return false;
        if (!W3dMatAlmost(v->specular[k], n.specular[k])) return false;
        if (!W3dMatAlmost(v->emission[k], n.emission[k])) return false;
        if (!W3dMatAlmost(v->ambient[k],  n.ambient[k]))  return false;
    }
    if (!W3dMatAlmost(v->shininess, n.shininess)) return false;
    if (v->interpolation != n.interpolation) return false;
    if (v->reflectMode   != n.reflectMode)   return false;
    if (v->reflectionProbe  != n.reflectionProbe)  return false;
    if (!W3dMatAlmost(v->rtRoughness, n.rtRoughness) || !W3dMatAlmost(v->rtMetallic, n.rtMetallic)) return false;
    if (v->textureOn != n.textureOn || v->filtered != n.filtered || v->repeat != n.repeat ||
        v->transparent != n.transparent || v->lighting != n.lighting || v->vertexColor != n.vertexColor ||
        v->culling != n.culling || v->depth_test != n.depth_test || v->chrome != n.chrome ||
        v->normalMap != n.normalMap || v->uv8bit != n.uv8bit) return false;
    if (v->depth_write != n.depth_write || v->order_pass != n.order_pass ||
        v->mix != n.mix || !W3dMatAlmost(v->depth_bias, n.depth_bias) || !W3dMatAlmost(v->bias_meters, n.bias_meters) || !W3dMatAlmost(v->alphaTest, n.alphaTest)) return false;
    if (!W3dMatAlmost(v->mixB, n.mixB) || v->toon != n.toon || !W3dMatAlmost(v->outline, n.outline)) return false;
    if (v->lines != n.lines || !W3dMatAlmost(v->thicknessLine, n.thicknessLine)) return false;
    if (v->perPixel != n.perPixel) return false;
    if (v->lightIndirect != n.lightIndirect) return false;
    if (!W3dMatAlmost(v->ior, n.ior) || !W3dMatAlmost(v->transmission, n.transmission)) return false;
    if (v->ssr != n.ssr) return false;
    if (v->shaderOn != n.shaderOn || v->shaderFile != n.shaderFile) return false;
    for (int k = 0; k < W3D_MAPS; k++) if (v->uvMap[k] != n.uvMap[k] || v->mapWithTexture[k] != n.mapWithTexture[k]) return false;
    if (W3dMatTexPath(v) != texPath) return false;
    return true;
}
static void W3dCopyMaterial(const Material& src, Material* dst) {
    for (int k = 0; k < 4; k++) {
        dst->diffuse[k]  = src.diffuse[k];  dst->specular[k] = src.specular[k];
        dst->emission[k] = src.emission[k]; dst->ambient[k]  = src.ambient[k];
    }
    dst->shininess = src.shininess; dst->interpolation = src.interpolation;
    dst->reflectMode = src.reflectMode;
    dst->reflectionProbe = src.reflectionProbe;
    dst->rtRoughness = src.rtRoughness; dst->rtMetallic = src.rtMetallic;
    dst->textureOn = src.textureOn; dst->filtered = src.filtered; dst->repeat = src.repeat;
    dst->transparent = src.transparent; dst->lighting = src.lighting; dst->vertexColor = src.vertexColor;
    dst->culling = src.culling; dst->depth_test = src.depth_test; dst->chrome = src.chrome;
    dst->normalMap = src.normalMap; dst->uv8bit = src.uv8bit;
    dst->depth_write = src.depth_write; dst->depth_bias = src.depth_bias; dst->bias_meters = src.bias_meters; dst->alphaTest = src.alphaTest;
    dst->order_pass = src.order_pass; dst->mix = src.mix; dst->mixB = src.mixB; dst->toon = src.toon; dst->outline = src.outline;
    dst->lines = src.lines; dst->thicknessLine = src.thicknessLine;
    dst->perPixel = src.perPixel;
    dst->lightIndirect = src.lightIndirect;
    dst->ior = src.ior; dst->transmission = src.transmission; dst->ssr = src.ssr;
    if (dst->shaderFile != src.shaderFile) { dst->shaderId = 0; dst->shaderState = 0; }   // (se compila al dibujar)
    dst->shaderOn = src.shaderOn; dst->shaderFile = src.shaderFile;
    for (int k = 0; k < W3D_MAPS; k++) { dst->uvMap[k] = src.uvMap[k]; dst->mapWithTexture[k] = src.mapWithTexture[k]; }
}
// (el normal map y las capas extra de un material se cargan al DESPERTAR sus texturas, sincronicas
// y por el mismo cache por ruta con refcount que usa la cola diferida: ver WakeOne en import_obj)
// 'lib' = los materiales de una LIBRERIA externa (io/Libraries.h): van a SU espacio de nombres ("Personajes/Piel",
// Material::libreria), de solo lectura, sin tocar gMatByName ni reusar/renumerar los del proyecto. El que llama
// deja puesto el contexto de la libreria (sus texturas son entradas de ella)
static void LoadMaterials(JVal* root, const std::string& base, const std::string& lib = std::string()) {
    if (lib.empty()) gMatByName.clear();
    JVal* jm = JChild(root, "materials", 5);
    if (!jm) return;
    for (size_t i = 0; i < jm->list.size(); i++) {
        JVal* e = jm->list[i]; if (!e || e->type != 4) continue;
        const std::string name = W3dLibsPrefix(lib, JS(e, "name", "Material"));
        // temporal SIN registrar (el flag "material por defecto" saltea el alta global y el
        // uniquificado del constructor): asi se puede DECIDIR si crear uno o reusar el homonimo.
        Material tmp(name, true);
        JColor4(e, "diffuse",    tmp.diffuse);
        JColor4(e, "specular", tmp.specular);
        JColor4(e, "emission",   tmp.emission);
        JColor4(e, "ambient",  tmp.ambient);
        tmp.shininess       = JF(e, "brightness", tmp.shininess);
        tmp.interpolation   = JI(e, "interpolation", tmp.interpolation);
        tmp.reflectMode     = JI(e, "reflectionMode", tmp.reflectMode);
        tmp.reflectionProbe = JB(e, "reflectionProbe", false);   // (el reflejo de la sonda de reflejo mas cercana)
        tmp.rtRoughness     = JF(e, "roughness", tmp.rtRoughness);   // trazado de rayos (ausentes = default)
        tmp.rtMetallic      = JF(e, "metallic",  tmp.rtMetallic);
        // DECAL / mezcla. Ausentes en TODO lo guardado hasta hoy -> el default deja el material igual.
        tmp.depth_bias   = JF(e, "depthBias", tmp.depth_bias);
        tmp.bias_meters  = JF(e, "biasMeters",      tmp.bias_meters);   // sesgo de profundidad en metros (ausente = 0)
        tmp.alphaTest    = JF(e, "alphaCut",        tmp.alphaTest);    // recorte por alfa (rejas); ausente = 0
        tmp.mixB         = JF(e, "mixB",          tmp.mixB);         // mezcla de texturas (textureB); ausente = 0
        tmp.ior          = JF(e, "ior",              tmp.ior);          // indice de refraccion; ausente = 1 (sin refraccion)
        tmp.transmission = JF(e, "transmission",      tmp.transmission); // lo que es vidrio (trazado); ausente = 0
        tmp.shaderFile   = JS(e, "shader", "");                         // el shader escrito (objects/WrittenShader.h)
        tmp.shaderOn     = JB(e, "shaderOn", false);
        // LOS MAPAS: la capa UV y numero / textura de cada uno (las texturas van dormidas, abajo)
        std::string mapTexs[W3D_MAPS];
        { JVal* jm = JChild(e, "maps", 5);
          if (jm) for (size_t c = 0; c < jm->list.size(); c++) {
              JVal* me = jm->list[c]; if (!me || me->type != 4) continue;
              const int k = W3dMatMapByKey(JS(me, "map", ""));
              if (k < 0) continue;
              tmp.uvMap[k] = JI(me, "uv", 0);
              tmp.mapWithTexture[k] = JB(me, "withTexture", false);
              const std::string mt = JS(me, "texture", "");
              if (!mt.empty() && k >= W3D_MAP_EMISSION) mapTexs[k] = JsonPath(mt, base);
          } }
        tmp.toon          = JI(e, "toon",             tmp.toon);        // sombreado toon (textureB = la rampa); ausente = 0
        if (tmp.toon < 0 || tmp.toon > 2) tmp.toon = 0;
        tmp.outline      = JF(e, "outline",         tmp.outline);    // borde del dibujo animado; ausente = 0
        if (tmp.outline < 0.0f) tmp.outline = 0.0f;
        tmp.order_pass  = JI(e, "passOrder",      tmp.order_pass);
        tmp.mix         = JI(e, "mix",           tmp.mix);
        // LINEAS (aristas por material). Ausentes en lo guardado hasta hoy -> default apagado.
        tmp.lines          = JB(e, "lines",           tmp.lines);
        tmp.perPixel       = JB(e, "perPixelLight",      tmp.perPixel);   // (ausente = por vertice)
        tmp.lightIndirect  = JB(e, "indirectLight",     tmp.lightIndirect);   // (la sonda de luz por vertice; ausente = no)
        tmp.ssr            = JB(e, "screenReflections", tmp.ssr);        // (ausente = sin reflejos en pantalla)
        tmp.thicknessLine  = JF(e, "lineThickness",      tmp.thicknessLine);
        if (tmp.thicknessLine < 1.0f) tmp.thicknessLine = 1.0f;
        if (tmp.order_pass < 0) tmp.order_pass = 0;
        if (tmp.order_pass > 2) tmp.order_pass = 2;
        if (tmp.mix < 0 || tmp.mix >= (int)w3dEngine::MixCount_) tmp.mix = 0;
        JVal* fl = JChild(e, "flags", 4);
        if (fl) {
            tmp.depth_write = JB(fl, "depthWrite", tmp.depth_write);
            tmp.textureOn   = JB(fl, "texture",      tmp.textureOn);
            tmp.filtered    = JB(fl, "filtered",     tmp.filtered);
            tmp.repeat      = JB(fl, "repeat",      tmp.repeat);
            tmp.transparent = JB(fl, "transparent", tmp.transparent);
            tmp.lighting    = JB(fl, "light",          tmp.lighting);
            tmp.vertexColor = JB(fl, "vertexColor", tmp.vertexColor);
            tmp.culling     = JB(fl, "culling",      tmp.culling);
            tmp.depth_test  = JB(fl, "depth",  tmp.depth_test);
            tmp.chrome      = JB(fl, "reflection",      tmp.chrome);
            tmp.normalMap   = JB(fl, "normalMap",    tmp.normalMap);
            tmp.uv8bit      = JB(fl, "uv8bit",       tmp.uv8bit);
        }
        std::string texPath = JS(e, "texture", "");
        if (!texPath.empty()) texPath = JsonPath(texPath, base);
        // LA TEXTURA B de antes (la mezcla de texturas, "textureB" + "mixB") ahora es una CAPA DE TEXTURA Mix con su
        // factor: entra ADELANTE de las capas (se ve igual: la unidad 1 tambien la mezclaba con la base ya iluminada).
        // Con toon no: ahi la textura B es la rampa
        std::string texBMigrated; float factorBMigrated = 0.0f;
        if (tmp.toon == 0) {
            const std::string tbm = JS(e, "textureB", "");
            if (!tbm.empty()) { texBMigrated = JsonPath(tbm, base); factorBMigrated = tmp.mixB; }
            tmp.mixB = 0.0f;
        }
        // CARPETA cosmetica (outliner por recursos): ausente = la raiz
        const std::string folder = W3dMeshFolderNormalize(JS(e, "folder", ""));
        // UN MATERIAL DE LIBRERIA: uno por nombre con prefijo en toda la sesion (si la libreria se vuelve a
        // registrar -otra vez vinculada- se reusa el que ya estaba: sus usuarios lo siguen nombrando)
        if (!lib.empty()) {
            if (W3dLibMaterial(name)) continue;
            Material* mat = new Material(name);   // (el prefijo lo aparta de los del proyecto)
            W3dCopyMaterial(tmp, mat);
            mat->folder = folder;
            mat->lib = lib;
            W3dLibMaterialRegister(name, mat);
            AsleepTex dz;
            dz.mat = mat;
            dz.base = texPath;
            std::string nrm = JS(e, "normalTexture", "");
            if (!nrm.empty()) dz.normal = JsonPath(nrm, base);
            std::string tb = JS(e, "textureB", "");
            if (!tb.empty() && texBMigrated.empty() && tmp.toon != 0) dz.texB = JsonPath(tb, base);
            for (int k = 0; k < W3D_MAPS; k++) dz.maps[k] = mapTexs[k];
            JVal* jc = JChild(e, "layers", 5);
            if (jc) for (size_t c = 0; c < jc->list.size(); c++) {
                JVal* ce = jc->list[c]; if (!ce || ce->type != 4) continue;
                const std::string ct = JS(ce, "texture", "");
                if (ct.empty()) continue;
                TexAsleepLayer cd; cd.texture = JsonPath(ct, base); cd.mix = JI(ce, "mix", 0); cd.on = JB(ce, "on", true);
                cd.uv = JI(ce, "uv", 0);
                cd.force = JF(ce, "force", 1.0f); cd.baked = JB(ce, "baked", false);
                cd.maskSun = JB(ce, "sunMask", false);
                dz.layers.push_back(cd);
            }
            if (!texBMigrated.empty()) {   // (la textura B de antes: la primera capa, Mix con su factor)
                TexAsleepLayer cd; cd.texture = texBMigrated; cd.mix = W3D_LAYER_MIX; cd.force = factorBMigrated;
                dz.layers.insert(dz.layers.begin(), cd);
            }
            TexturesSleep(dz);   // (despiertan cuando una malla generada por un proxy los usa)
            continue;
        }
        Material* old = FindMaterialByName(name);
        if (old && W3dSameMaterial(old, tmp, texPath)) {
            if (!folder.empty()) old->folder = folder;
            gMatByName[name] = old; continue;
        }
        Material* mat = new Material(name);   // el ctor uniquifica: homonimo DISTINTO -> "X.001"
        W3dCopyMaterial(tmp, mat);
        mat->folder = folder;
        // SUS TEXTURAS QUEDAN DORMIDAS (import_obj.h) hasta saber si alguna malla lo usa: el proyecto
        // guarda tambien los materiales HUERFANOS y no tienen por que ocupar la GPU (ni en el editor ni
        // en el juego compilado). Al terminar de armar la escena, TexturesWakeUsed manda las de
        // los materiales usados a la cola diferida (la base) y carga el normal map y las capas.
        AsleepTex dz;
        dz.mat = mat;
        dz.base = texPath;
        std::string nrm = JS(e, "normalTexture", "");
        if (!nrm.empty()) dz.normal = JsonPath(nrm, base);
        std::string tb = JS(e, "textureB", "");
        if (!tb.empty() && texBMigrated.empty() && tmp.toon != 0) dz.texB = JsonPath(tb, base);
        for (int k = 0; k < W3D_MAPS; k++) dz.maps[k] = mapTexs[k];
        JVal* jc = JChild(e, "layers", 5);
        if (jc) for (size_t c = 0; c < jc->list.size(); c++) {
            JVal* ce = jc->list[c]; if (!ce || ce->type != 4) continue;
            const std::string ct = JS(ce, "texture", "");
            if (ct.empty()) continue;
            TexAsleepLayer cd; cd.texture = JsonPath(ct, base); cd.mix = JI(ce, "mix", 0); cd.on = JB(ce, "on", true);
            cd.uv = JI(ce, "uv", 0);   // capa UV de la malla con la que se dibuja (ausente = 0, la base)
            cd.force = JF(ce, "force", 1.0f); cd.baked = JB(ce, "baked", false);   // el factor de la mezcla (ausente = 1)
            cd.maskSun = JB(ce, "sunMask", false);   // (el alfa de la luz horneada: cuanto es del sol)
            dz.layers.push_back(cd);
        }
        if (!texBMigrated.empty()) {   // (la textura B de antes: la primera capa, Mix con su factor)
            TexAsleepLayer cd; cd.texture = texBMigrated; cd.mix = W3D_LAYER_MIX; cd.force = factorBMigrated;
            dz.layers.insert(dz.layers.begin(), cd);
        }
        TexturesSleep(dz);
        if (old) w3dLogfW("[W3D] ya habia un material '%s' con OTRO contenido -> se creo '%s'",
                            name.c_str(), mat->name.c_str());
        gMatByName[name] = mat;
    }
}

// el material que le toca a un mesh part por el NOMBRE que trae el .w3dm ("" = ninguno).
// Cae a FindMaterialByName para los .w3d escritos a mano y para los nombres que ya
// existian en la sesion.
// Adentro de una LIBRERIA (su contexto, io/Libraries.h) los nombres son los de SU archivo: su material vive con su
// prefijo. Desde el proyecto, un nombre con el prefijo de una libreria vinculada es un material de ella (la parte
// de una malla del proyecto a la que se le solto un material de una libreria).
static Material* MaterialByName(const std::string& name) {
    if (name.empty()) return NULL;
    const std::string& lib = W3dLibContext();
    if (!lib.empty()) {
        Material* m = W3dLibMaterial(lib + "/" + name);
        if (m) return m;
        // (uno de una libreria de SU registro -una de adentro-, soltado en su malla)
        if (name.find('/') != std::string::npos) {
            const std::string an = W3dLibsNameNested(lib, name);
            if (!an.empty() && (m = W3dLibMaterial(an)) != NULL) return m;
        }
    }
    std::map<std::string, Material*>::iterator it = gMatByName.find(name);
    if (it != gMatByName.end()) return it->second;
    Material* ml = W3dLibMaterial(name);
    if (!ml && name.find('/') != std::string::npos && !W3dLibsByName(name).empty()) ml = W3dLibMaterial(name);
    if (ml) return ml;
    return FindMaterialByName(name);
}

// ===========================================================================
//  LOS RECURSOS DE UNA LIBRERIA EXTERNA (io/Libraries.h: W3dLibsEnsure lo llama al montarla): sus materiales,
//  sus mallas y sus animsets, cada uno con el prefijo de la libreria. Nada se carga: se REGISTRAN (las mallas y
//  los animsets se leen recien cuando un objeto los usa; las texturas quedan dormidas hasta que una malla usa su
//  material), y nada de esto es del proyecto (no se lista en su biblioteca ni se guarda).
// ===========================================================================
static bool MeshTextToBin(const unsigned char* d, size_t n, const std::string& name, std::string& out);   // (mas abajo)
bool W3dLibRegisterResources(const std::string& lib, JVal* root) {
    if (lib.empty() || !root) return false;
    W3dLibContextGuard ctx(lib);
    W3dMeshMaterialByNameHook = MaterialByName;
    W3dMeshTextToBinHook = MeshTextToBin;
    LoadMaterials(root, gBaseProject, lib);
    int nm = 0, na = 0;
    JVal* lm = JChild(root, "meshes", 5);
    if (lm)
        for (size_t i = 0; i < lm->list.size(); i++) {
            JVal* e = lm->list[i];
            if (!e || e->type != 4) continue;
            const std::string name = JS(e, "name", ""), ent = JS(e, "entry", "");
            if (name.empty() || ent.empty()) continue;
            W3dMeshResourceNewLib(lib + "/" + name, JsonPath(ent, gBaseProject), lib);
            nm++;
        }
    W3dAnimSetsEnsureOps();
    JVal* la = JChild(root, "animsets", 5);
    if (la)
        for (size_t i = 0; i < la->list.size(); i++) {
            JVal* e = la->list[i];
            if (!e || e->type != 4) continue;
            const std::string name = JS(e, "name", ""), ent = JS(e, "entry", "");
            if (name.empty() || ent.empty()) continue;
            W3dAnimSetsLibAdd(lib + "/" + name, JsonPath(ent, gBaseProject));
            na++;
        }
    // sus FLIPBOOKS con nombre (el mismo prefijo). Van a la lista de la escena MARCADOS como de la libreria
    // (Flipbook::libreria): el guardado del proyecto nunca los escribe, sea cual sea su atlas
    JVal* jf = JChild(root, "flipbooks", 5);
    if (jf)
        for (size_t i = 0; i < jf->list.size(); i++) {
            JVal* je = jf->list[i];
            if (!je || je->type != 4) continue;
            const std::string n = lib + "/" + JS(je, "name", "Flipbook");
            Flipbook* f = FlipbookByName(n);
            if (f && !f->lib.empty()) continue;   // (ya registrado)
            // uno del PROYECTO con ese nombre es una copia CONGELADA que un guardado viejo filtro al nivel: pasa a
            // ser el de la libreria (con lo que la libreria dice HOY), en vez de taparlo
            if (!f) {
                f = new Flipbook();
                f->name = n;
                SceneFlipbooks.push_back(f);
            }
            f->lib = lib;
            f->ConfigureStrip(JsonPath(JS(je, "atlas", ""), gBaseProject), JI(je, "cols", 1), JI(je, "rows", 1),
                              JI(je, "frames", 1), JF(je, "fps", 30.0f));
            f->crossfade = JB(je, "crossfade", false);
        }
    w3dLogf("[librerias] '%s': %d malla(s) y %d animset(s) registrados", lib.c_str(), nm, na);
    return true;
}

// ===========================================================================
//  ANIMACIONES DE ESCENA (bloque raiz "animaciones") -> SceneAnimations
//
//  La contraparte de WriteAnimationsScene (io/SaveW3D.cpp), donde esta
//  el comentario largo de por que existe y por que vive en el project.json.
//  RETROCOMPAT: un .w3d SIN el bloque (o sea todos los guardados hasta hoy, que
//  es el bug que reporto el dueno) abre igual, con la escena "Scene" default.
//
//  TODA CANTIDAD LEIDA DEL ARCHIVO VA ACOTADA. El parser ya limito el tamano al
//  del archivo, pero las cotas de aca son las que evitan que un .w3d hecho a
//  mano (el formato se vende como editable) meta cien mil escenas de animacion
//  y deje el editor inusable sin un solo aviso.
// ===========================================================================
// (las ESCENAS de animacion: 1024 era poco -- un proyecto con mas perdia las que sobraban al abrir y el guardado
//  siguiente ya no las escribia. 65536 sigue debajo de los ids especiales del selector (Properties.cpp, ANIM_ID_*))
enum { kAnimMaxScenes = 65536, kAnimMaxObjects = 100000, kAnimMaxCurves = 4096 };

// BUSQUEDA POR NOMBRE RESPETANDO EL SCOPE. Los nombres son unicos POR ESCENA
// (W3dScopeNameOf), asi que buscar en todo el arbol podria devolver el widget
// homonimo de una UI en vez del objeto de la escena. La particion es LA MISMA
// que la de IndexScopeGlobal/IndexScopeUI (Objects.cpp): en el scope global
// cuenta todo lo que no esta dentro de ninguna UI, mas las RAICES UI.
// (las INSTANCIAS DE PREFAB tambien son frontera: lo que generan no es del scope de afuera, y el guardado
//  no escribe curvas de objetos generados)
static Object* AnimFindScopeGlobal(Object* node, const std::string& n, bool insideUI) {
    if (!node) return NULL;
    for (size_t i = 0; i < node->Childrens.size(); i++) {
        Object* h = node->Childrens[i];
        const bool isUI = (h->getType() == ObjectType::ui);
        if ((isUI || !insideUI) && h->name == n) return h;
        Object* r = AnimFindScopeGlobal(h, n, insideUI || W3dIsBoundaryScope(h));
        if (r) return r;
    }
    return NULL;
}
static Object* AnimFindScopeUI(Object* node, const std::string& n) {
    if (!node) return NULL;
    for (size_t i = 0; i < node->Childrens.size(); i++) {
        Object* h = node->Childrens[i];
        if (h->getType() == ObjectType::ui) continue;   // otro scope (el suyo)
        if (h->name == n) return h;
        if (W3dIsBoundaryScope(h)) continue;             // una instancia de prefab: lo suyo es de ella
        Object* r = AnimFindScopeUI(h, n);
        if (r) return r;
    }
    return NULL;
}

// ===========================================================================
//  LOS CLIPS DE OBJETO INLINE (la primera version de la fase de escenas, antes de los
//  clips de jerarquia compartidos): el nodo del objeto trae "clips": [{ "nombre",
//  "inicio", "fin", "fps", "objetos": [{ "objeto", "escena"?, "curvas" }] }] (la forma
//  de una animacion de escena; "objeto" vacio = el objeto mismo). Se siguen LEYENDO y
//  pasan a ser clips de jerarquia de la biblioteca de su objeto (el guardado los
//  escribe en su .w3da). Al leer el nodo sus objetos todavia no estan (los hijos vienen
//  despues), asi que se anotan y se convierten al final, con el arbol completo.
// ===========================================================================
namespace {
struct PendingClip {
    Object* owner;
    SceneAnimation* clip;                // sus curvas con obj = NULL hasta resolverlas
    std::vector<std::string> names;      // paralelo a clip->objects: "" = el dueno
    std::vector<std::string> scenes;     // la escena UI del nombre ("" = el scope global)
};
}
static std::vector<PendingClip> gClipsPend;

static void PendingClipsForget() {
    for (size_t i = 0; i < gClipsPend.size(); i++) delete gClipsPend[i].clip;
    gClipsPend.clear();
}
static void ReadClipsObject(JVal* j, Object* o) {
    JVal* jc = JChild(j, "clips", 5);
    if (!jc || !o) return;
    size_t n = jc->list.size();
    if (n > (size_t)kAnimMaxScenes) n = (size_t)kAnimMaxScenes;
    for (size_t i = 0; i < n; i++) {
        JVal* je = jc->list[i];
        if (!je || je->type != 4) continue;
        PendingClip p;
        p.owner = o;
        p.clip = new SceneAnimation(JS(je, "name", "Clip"));
        p.clip->startFrame = JI(je, "start", 1);
        p.clip->endFrame   = JI(je, "end", 250);
        p.clip->fps        = JI(je, "fps", 30);
        // (rango/fps SANEADOS, como los de una animacion de escena)
        if (p.clip->startFrame < 0) p.clip->startFrame = 0;
        if (p.clip->endFrame < p.clip->startFrame) p.clip->endFrame = p.clip->startFrame;
        if (p.clip->fps < 1) p.clip->fps = 1;
        if (p.clip->fps > 120) p.clip->fps = 120;
        JVal* jobjs = JChild(je, "objects", 5);
        size_t nObj = jobjs ? jobjs->list.size() : 0;
        if (nObj > (size_t)kAnimMaxObjects) nObj = (size_t)kAnimMaxObjects;
        for (size_t k = 0; k < nObj; k++) {
            JVal* jo = jobjs->list[k];
            if (!jo || jo->type != 4) continue;
            AnimationObject ao;
            ao.obj = NULL; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
            JVal* jcur = JChild(jo, "curves", 5);
            size_t nCur = jcur ? jcur->list.size() : 0;
            if (nCur > (size_t)kAnimMaxCurves) nCur = (size_t)kAnimMaxCurves;
            for (size_t c = 0; c < nCur; c++) {
                AnimProperty ap;
                if (!LoadCurveJson(jcur->list[c], ap) || ap.keyframes.empty()) continue;
                ap.SortKeyFrames();
                ao.Properties.push_back(ap);
            }
            if (ao.Properties.empty()) continue;
            ao.UpdateFirstLastFrame();
            p.clip->objects.push_back(ao);
            p.names.push_back(JS(jo, "object", ""));
            p.scenes.push_back(JS(jo, "scene", ""));
        }
        gClipsPend.push_back(p);
    }
}
// convierte los clips leidos en CLIPS DE JERARQUIA de su dueno (su biblioteca; una nueva con su nombre si no
// tiene): cada curva busca su objeto en el SUBARBOL del dueno y, si no esta ahi, en su scope (como una animacion
// de escena), y queda en la pista de su RUTA relativa al dueno (lo que no cuelga del dueno se descarta: un clip
// de jerarquia solo nombra su jerarquia). Despues elige el clip que se estaba editando al guardar
// ("animaciones"."activaClip" de 'raiz': una vista de ese clip sobre su raiz).
static void AttachPendingClips(JVal* root) {
    for (size_t i = 0; i < gClipsPend.size(); i++) {
        PendingClip& p = gClipsPend[i];
        SceneAnimation* c = p.clip;
        W3dClipHier* cj = W3dHierNewClip(p.owner, c->name);
        if (cj) {
            cj->start = c->startFrame; cj->end = c->endFrame; cj->fps = c->fps;
            for (size_t k = 0; k < c->objects.size(); k++) {
                Object* obj = NULL;
                const std::string& n = p.names[k];
                if (n.empty()) obj = p.owner;
                else {
                    obj = FindObjectByName(p.owner, n);   // su subarbol primero (la hoja de SU puerta)
                    if (!obj) {
                        if (p.scenes[k].empty()) obj = AnimFindScopeGlobal(SceneCollection, n, false);
                        else {
                            Object* ui = AnimFindScopeGlobal(SceneCollection, p.scenes[k], false);
                            if (ui && ui->getType() == ObjectType::ui) obj = AnimFindScopeUI(ui, n);
                        }
                    }
                }
                const std::string path = obj ? W3dHierPath(p.owner, obj) : std::string();
                if (path.empty()) {
                    w3dLogfW("[W3D] el clip '%s' de '%s' anima a '%s', que no es de su jerarquia: sus curvas se descartan",
                             c->name.c_str(), p.owner->name.c_str(), n.c_str());
                    continue;
                }
                W3dHierTrack pj;
                pj.path = path;
                pj.props = c->objects[k].Properties;
                cj->tracks.push_back(pj);
            }
            w3dLogf("[W3D] '%s': el clip de objeto '%s' pasa a ser un clip de jerarquia de su biblioteca '%s'",
                    p.owner->name.c_str(), cj->name.c_str(), p.owner->clipsHier ? p.owner->clipsHier->animset.c_str() : "?");
        }
        delete c;
        p.clip = NULL;
    }
    gClipsPend.clear();
    // el clip que se estaba editando al guardar
    JVal* ja = root ? JChild(root, "animations", 4) : NULL;
    JVal* jac = ja ? JChild(ja, "activeClip", 4) : NULL;
    if (jac) {
        Object* d = FindObjectByName(SceneCollection, JS(jac, "object", ""));
        W3dClipHier* c = d ? W3dHierClip(d, JS(jac, "clip", "")) : NULL;
        const int idx  = c ? W3dClipView(d, c) : -1;
        if (idx >= 0) { SetActiveScene(idx); AnimLoadActiveRange(); }
    }
}

// FLIPBOOKS CON NOMBRE de la escena (contraparte de WriteFlipbooks). Solo la config;
// las 8 curvas de UV se regeneran con ConfigureStrip->GenerateCells.
static void LoadFlipbooksScene(JVal* root) {
    SceneFlipbooksClear();                           // reemplaza la lista entera (escena nueva)
    JVal* jf = JChild(root, "flipbooks", 5);         // array (.w3d viejo: sin bloque, no pasa nada)
    if (!jf) return;
    for (size_t i = 0; i < jf->list.size(); i++) {
        JVal* je = jf->list[i];
        if (!je || je->type != 4) continue;
        Flipbook* f = new Flipbook();
        f->name = JS(je, "name", "Flipbook");
        f->ConfigureStrip(JsonPathField(JS(je, "atlas", ""), gBaseProject), JI(je, "cols", 1), JI(je, "rows", 1),
                          JI(je, "frames", 1), JF(je, "fps", 30.0f));   // grilla + regenera curvas
        f->crossfade = JB(je, "crossfade", false);
        SceneFlipbooks.push_back(f);
    }
}

static void LoadAnimationsScene(JVal* root) {
    JVal* ja = JChild(root, "animations", 4);
    if (!ja) { InitSceneAnimations(); return; }   // .w3d viejo: sin bloque, abre igual
    JVal* jes = JChild(ja, "scenes", 5);
    if (!jes || jes->list.empty()) { InitSceneAnimations(); return; }
    // se reemplaza la lista entera (RestartScene dejo la "Scene" vacia de arranque)
    for (size_t i = 0; i < SceneAnimations.size(); i++) delete SceneAnimations[i];
    SceneAnimations.clear();
    AnimationObjects.clear();
    ActiveSceneAnim = 0;

    size_t nEsc = jes->list.size();
    if (nEsc > (size_t)kAnimMaxScenes) {
        w3dLogfW("[W3D] el proyecto declara %d animaciones de escena: se cargan las primeras %d",
                 (int)nEsc, (int)kAnimMaxScenes);
        nEsc = (size_t)kAnimMaxScenes;
    }
    int lost = 0;             // curvas cuyo objeto no aparecio
    std::string firstLost;
    for (size_t i = 0; i < nEsc; i++) {
        JVal* je = jes->list[i];
        if (!je || je->type != 4) continue;
        SceneAnimation* esc = new SceneAnimation(JS(je, "name", "Scene"));
        esc->startFrame = JI(je, "start", 1);
        esc->endFrame   = JI(je, "end", 250);
        esc->fps        = JI(je, "fps", 30);
        // rango/fps SANEADOS: un archivo a mano puede traer fin < inicio o fps 0 y eso
        // deja el timeline inutilizable (y AnimTick en un loop de un solo frame)
        if (esc->startFrame < 0) esc->startFrame = 0;
        if (esc->endFrame < esc->startFrame) esc->endFrame = esc->startFrame;
        if (esc->fps < 1) esc->fps = 1;
        if (esc->fps > 120) esc->fps = 120;
        SceneAnimations.push_back(esc);
        // pista "CAMARA ACTIVA": [{frame, camara}] -> los cortes de la cinematica
        JVal* jcam = JChild(je, "activeCamera", 5);
        if (jcam)
            for (size_t q = 0; q < jcam->list.size(); q++) {
                JVal* jk = jcam->list[q];
                if (!jk || jk->type != 4) continue;
                const std::string cam = JS(jk, "camera", "");
                if (!cam.empty()) W3dCamKey((int)SceneAnimations.size() - 1, JI(jk, "frame", 0), cam);
            }

        // las TIRAS de esqueleto (un "audio" de antes no se lee: el audio de una escena es un objeto Sonido)
        { JVal* jq = JChild(je, "armatures", 5);
          if (jq) for (size_t q = 0; q < jq->list.size(); q++) {
              JVal* jk = jq->list[q]; if (!jk || jk->type != 4) continue;
              SceneAnimation::SkeletonTrack p; p.armature = JS(jk, "armature", ""); p.clip = JS(jk, "clip", ""); p.frame = JI(jk, "frame", 1);
              if (!p.armature.empty() && !p.clip.empty()) esc->skeletons.push_back(p);
          } }
        JVal* jobjs = JChild(je, "objects", 5);
        if (!jobjs) continue;
        size_t nObj = jobjs->list.size();
        if (nObj > (size_t)kAnimMaxObjects) {
            w3dLogfW("[W3D] la animacion '%s' declara %d objetos: se cargan los primeros %d",
                     esc->name.c_str(), (int)nObj, (int)kAnimMaxObjects);
            nObj = (size_t)kAnimMaxObjects;
        }
        for (size_t o = 0; o < nObj; o++) {
            JVal* jo = jobjs->list[o];
            if (!jo || jo->type != 4) continue;
            const std::string name = JS(jo, "object", "");
            if (name.empty()) continue;
            // SCOPE: "escena" = la UI adentro de la cual vive el objeto (ausente = global)
            const std::string scope = JS(jo, "scene", "");
            Object* obj = NULL;
            if (scope.empty()) {
                obj = AnimFindScopeGlobal(SceneCollection, name, false);
            } else {
                Object* ui = AnimFindScopeGlobal(SceneCollection, scope, false);
                if (ui && ui->getType() == ObjectType::ui) obj = AnimFindScopeUI(ui, name);
            }
            if (!obj) {
                // EL OBJETO NO ESTA (lo borraron o lo renombraron despues de guardar). La curva
                // NO se cuelga de cualquier lado: se descarta y se AVISA. Colgarla del primer
                // homonimo que apareciera seria animar un objeto que el usuario no eligio.
                if (lost == 0) firstLost = scope.empty() ? name : (scope + "/" + name);
                lost++;
                w3dLogfW("[W3D] la animacion '%s' referencia al objeto '%s'%s%s, que ya no esta: sus curvas se descartan",
                         esc->name.c_str(), name.c_str(),
                         scope.empty() ? "" : " de la escena ", scope.c_str());
                continue;
            }
            AnimationObject ao;
            ao.obj = obj; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
            JVal* jcur = JChild(jo, "curves", 5);
            if (jcur) {
                size_t nCur = jcur->list.size();
                if (nCur > (size_t)kAnimMaxCurves) {
                    w3dLogfW("[W3D] '%s' declara %d curvas: se cargan las primeras %d",
                             name.c_str(), (int)nCur, (int)kAnimMaxCurves);
                    nCur = (size_t)kAnimMaxCurves;
                }
                for (size_t c = 0; c < nCur; c++) {
                    AnimProperty ap;
                    if (!LoadCurveJson(jcur->list[c], ap)) continue;
                    if (ap.keyframes.empty()) continue;   // curva sin keys: no es dato
                    ap.SortKeyFrames();                    // el archivo puede venir desordenado
                    ao.Properties.push_back(ap);
                }
            }
            if (ao.Properties.empty()) continue;
            ao.UpdateFirstLastFrame();
            SceneAnimations.back()->objects.push_back(ao);
        }
    }
    if (SceneAnimations.empty()) { InitSceneAnimations(); return; }
    // LA ACTIVA: sus curvas NO viven en su SceneAnimation sino en el global
    // AnimationObjects (ver el invariante del swap en Animation.h). Se hace el
    // mismo swap que SetActiveScene, o el proyecto abriria con la escena que el
    // usuario estaba viendo aparentemente vacia.
    int active = JI(ja, "active", 0);
    if (active < 0 || active >= (int)SceneAnimations.size()) active = 0;
    ActiveSceneAnim = active;
    AnimationObjects.swap(SceneAnimations[active]->objects);
    // rango + fps PROPIOS de la animacion activa -> StartFrame/EndFrame/AnimFPS
    AnimLoadActiveRange();
    if (lost > 0)
        W3dAvisof(true, T("%d animation curve(s) were left without an object (the first one: '%s'): see whisk3d.log"),
                  lost, W3dShortName(firstLost).c_str());
}

// REGISTRO DE ANIMSETS: "animsets": [{ "nombre", "entrada", "carpeta" }] (ver animation/W3dAnimSet.h).
// Reemplaza el del proyecto anterior. La entrada va por JsonPath como toda referencia del .w3d.
static void LoadRegistryAnimSets(JVal* root, const std::string& base) {
    W3dAnimSetsEnsureOps();   // una lista de carga ("animset:") puede pedir uno antes que los armatures
    W3dAnimSetsRegistryClear();
    JVal* ja = root ? JChild(root, "animsets", 5) : NULL;
    if (!ja) return;
    for (size_t i = 0; i < ja->list.size(); i++) {
        JVal* e = ja->list[i]; if (!e || e->type != 4) continue;
        const std::string name = JS(e, "name", ""), ent = JS(e, "entry", "");
        if (name.empty() || ent.empty()) continue;
        W3dAnimSetsRegistryAdd(name, JsonPath(ent, base), W3dMeshFolderNormalize(JS(e, "folder", "")));
    }
}

// abre el proyecto desde el TEXTO json 'datos' (el .w3d plano entero, o el
// scene.json de un zip v2 viejo), con las rutas relativas resueltas contra
// 'base'. false + aviso si el json no parsea (el editor sigue vivo igual).
// ===========================================================================
//  EL REGISTRO DE MALLAS DEL PROYECTO (bloque raiz "mallas", objects/MeshResource.h):
//      "mallas": [{"nombre": "Arbol", "entrada": "meshes/arbol.w3db", "carpeta": "Vegetacion"}]
//  Solo se anotan: los arrays se leen recien cuando el primer objeto las usa (una sola vez
//  para todos). Una entrada sin usuarios (HUERFANA) queda en el registro y se conserva al
//  guardar. Va DESPUES de los materiales: la primera carga de un recurso resuelve el material
//  de cada parte por el nombre del archivo (MaterialByName).
// ===========================================================================
// ===========================================================================
//  UNA MALLA DE TRABAJO FUERA DE LA ESCENA (ver import_w3d.h): convertir una malla del registro
//  entre texto y binario sin que nadie la vea. El ctor de Object la cuelga de la escena, le
//  busca un nombre libre y la SELECCIONA (en Edit Mode eso deseleccionaria los vertices de la
//  malla en edicion): nada de eso puede pasar en medio de una carga o de un guardado. Se crea
//  con la seleccion congelada y sin buscar nombre, y se descuelga en el acto.
// ===========================================================================
Mesh* W3dTempNewMesh() {
    const std::vector<Object*> sel = ObjSelects;
    Object* act = ActiveObj;
    const unsigned int selSerial = W3dSelectionSerial;   // (nadie eligio nada: el recurso activo sigue)
    const int est = state;
    const bool nc = W3dNamesLoading;
    state = translation;        // DeselectAll sale sin tocar nada fuera de la navegacion
    W3dNamesLoading = true;     // sin recorrer la escena buscando un nombre libre
    Mesh* t = new Mesh(NULL, Vector3(0, 0, 0));
    W3dNamesLoading = nc;
    state = est;
    // sin padre, el ctor la cuelga de SceneCollection SIN anotarlo en Parent: se busca ahi
    Object* attached = t->Parent ? t->Parent : SceneCollection;
    if (attached) {
        std::vector<Object*>& ch = attached->Childrens;
        for (size_t i = 0; i < ch.size(); i++) if (ch[i] == t) { ch.erase(ch.begin() + (long)i); break; }
    }
    t->Parent = NULL;
    t->select = false;
    ObjSelects = sel;              // Select() la habia sumado y hecho activa
    ActiveObj = act;
    W3dSelectionSerial = selSerial;
    return t;
}
void W3dTempMeshDelete(Mesh* t) { delete t; }

// LA ENTRADA DE TEXTO (.w3dm) DE UNA MALLA DEL REGISTRO -> .w3db en memoria (el gancho
// W3dMeshTextToBinHook de MeshResource.h: el recurso vive en binario). El MISMO camino que la
// rama "malla" con un .w3dm suelto: el lector de texto y la derivacion (index buffer por parte,
// bordes conservando los puntos del archivo, capas al render, mapa de puntos); despues el
// escritor binario con el index buffer canonico. Los nombres de material de las partes viajan
// TAL CUAL venian en el texto (aunque el material no exista en esta escena).
static bool MeshTextToBin(const unsigned char* d, size_t n, const std::string& name, std::string& out) {
    out.clear();
    if (!d || !n) return false;
    Mesh* t = W3dTempNewMesh();
    W3dMeshInfo info;
    bool ok = W3dMeshRead((const char*)d, n, t, &info);
    for (size_t k = 0; k < info.notices.size(); k++)
        w3dLogfW("[W3D] malla '%s' (texto): %s", name.c_str(), info.notices[k].c_str());
    if (ok) {
        for (size_t k = 0; k < t->materialsGroup.size() && k < info.materials.size(); k++)
            t->materialsGroup[k].material = MaterialByName(info.materials[k]);
        t->RegroupMeshParts();
        t->ComputeBorders(true, false);   // CONSERVA los puntos del archivo (no re-suelda por posicion)
        t->ApplyLayersToRender();
        WeightPaintEnsureMap(t);
        // el index buffer que RegroupMeshParts acaba de armar YA es el canonico (los dos salen
        // de BuildIndicesByPart sobre las mismas caras): se escribe tal cual, sin volver a
        // triangular ni a pasar Forsyth
        std::vector<std::string> notices;
        ok = W3dMeshBinWrite(t, NULL, out, &notices);
        for (size_t k = 0; k < notices.size(); k++)
            w3dLogfW("[W3D] malla '%s' (texto -> binario): %s", name.c_str(), notices[k].c_str());
        std::string b2;
        if (ok && W3dMeshBinRename(out, name, b2)) out.swap(b2);
        if (ok && info.materials.size() == t->materialsGroup.size() &&
            W3dMeshBinChangeMaterials(out, info.materials, b2)) out.swap(b2);
    }
    W3dTempMeshDelete(t);
    if (!ok) out.clear();
    return ok && !out.empty();
}

static void LoadRegistryMeshes(JVal* root) {
    W3dMeshMaterialByNameHook = MaterialByName;
    W3dMeshTextToBinHook = MeshTextToBin;   // entradas del registro en texto ("formatoMallas": "texto")
    JVal* list = JChild(root, "meshes", 5);
    if (!list) return;
    for (size_t i = 0; i < list->list.size(); i++) {
        JVal* e = list->list[i];
        if (!e || e->type != 4) continue;
        const std::string name = JS(e, "name", "");
        const std::string ent  = JS(e, "entry", "");
        if (name.empty() || ent.empty()) {
            w3dLogfW("[W3D] registro de mallas: una entrada sin nombre o sin archivo (se ignora)");
            continue;
        }
        if (W3dMeshResourceByName(name)) {
            w3dLogfW("[W3D] registro de mallas: '%s' esta repetida (queda la primera)", name.c_str());
            continue;
        }
        MeshResource* r = W3dMeshNewResource(name, ent, W3dMeshFolderNormalize(JS(e, "folder", "")));
        (void)r;
    }
}

static bool OpenSceneJson(const char* data, size_t n, const std::string& base) {
    double tMed = LoadNow();   // medicion por fase (0 = nadie mide)
    gBaseProject = base;
    PendingClipsForget();     // (una carga anterior que no llego a engancharlos)
    JParser parser(data, n);
    JVal* root = parser.Value();
    if (g_w3dLoadClock) g_w3dLoadPhases.jsonMs += LoadNow() - tMed;
    w3dLogMemory("JSON del proyecto leido");
    if (parser.error || root->type != 4) {
        w3dLogfE("[W3D] el JSON del proyecto no parsea: %s (se abre vacio)", w3dPath.c_str());
        delete root;
        return false;
    }
    // fps: canonico en la raiz (v3); el zip v2 lo traia dentro de "escena"
    ApplyFps(JI(root, "fps", 0));
    ApplyFpsCap(JI(root, "fpsCap", 0));
    // PALETAS del proyecto (v3), ANTES de los objetos: asi las paletas del
    // .w3d ganan el merge contra las bakeadas en los .w3dui (primera gana).
    // Un .w3d viejo sin el campo: se adoptan las de las UIs al cargarlas.
    {
        JVal* jps = JChild(root, "palettes", 5);
        if (jps && !jps->list.empty()) {
            std::vector<Palette> ps;
            for (size_t i = 0; i < jps->list.size() && i < 8; i++) {
                JVal* jp = jps->list[i];
                if (!jp || jp->type != 4) continue;
                Palette pa;
                pa.name = JS(jp, "name", "Palette");
                JVal* jc = JChild(jp, "colors", 5);
                if (jc)
                    for (size_t c = 0; c < jc->list.size() && c < 32; c++) {
                        JVal* e = jc->list[c];
                        if (!e || e->type != 4) continue;
                        PaletteColor pc;
                        pc.name = JS(e, "name", "Color");
                        pc.rgba[0] = pc.rgba[1] = pc.rgba[2] = pc.rgba[3] = 1.0f;
                        JColor(e, "color", pc.rgba);
                        pa.colors.push_back(pc);
                    }
                ps.push_back(pa);
            }
            W3dPalettesAdopt(ps);
        }
    }
    // MATERIALES del proyecto, ANTES de los objetos: cada .w3dm los referencia por nombre.
    // Un .w3d viejo (mallas en GLB) no trae el bloque y sus materiales siguen viniendo de
    // adentro del .glb, como siempre.
    LoadMaterials(root, base);
    LoadRegistryMeshes(root);   // las mallas con nombre (despues de los materiales: sus partes los nombran)
    LoadFlipbooksScene(root);   // ANTES de los objetos: un objeto puede referenciar un flipbook por nombre
    LoadRegistryAnimSets(root, base);  // ANTES de los objetos: cada armature nombra su animset
#ifndef W3D_NO_EDITOR
    W3dResourcesViewReadJson(root);      // las carpetas de la biblioteca (cosmeticas; solo el editor)
#endif
    // las LIBRERIAS externas vinculadas (io/Libraries.h): solo su registro, ANTES de los objetos (un proxy o un
    // recurso con prefijo las nombran). Cada una se monta y registra sus recursos recien cuando algo la usa; el
    // juego compilado las trae empaquetadas (no abre ningun .w3d)
    W3dLibsReadJson(root, !gLoadGame);
#ifndef W3D_NO_EDITOR
    { extern void W3dExtLibsInvalidate(); W3dExtLibsInvalidate(); }   // (lo que el outliner leyo de las anteriores)
#endif
    // la VISTA PREVIA DEL STREAMING (opcion del proyecto, io/Streaming.h): ANTES de los objetos, porque decide si las
    // instancias diferidas se generan al abrir (sin ella el editor las genera todas). Solo el editor la usa
    if (!gLoadGame) g_w3dStreamingPreviewView = JB(root, "streamingPreview", false);
    // las ESCENAS 3D y los PREFABS del proyecto (W3dRoots.h): la raiz actual pasa a ser la de la
    // escena del bloque "escena"; el resto se carga desde su entrada cuando se la pida
    W3dRootsReadJson(root);
    // el JUEGO arranca en su escena INICIAL: si no es la del bloque, el bloque ni se arma
    const bool skipBlock = gLoadGame && W3dInitialRootIdx() != W3dRootBlock();
    JVal* esc = skipBlock ? NULL : JChild(root, "scene", 4);
    if (skipBlock) {
        w3dLogf("[W3D] el juego arranca en la escena '%s': el bloque \"escena\" no se arma",
                W3dInitialRoot().c_str());
    } else if (esc) {
        ApplyFps(JI(esc, "fps", 0));
        ApplyFpsCap(JI(esc, "fpsCap", 0));
        JVal* objs = JChild(esc, "objects", 5);
        if (objs) {
            extern void ProgressUpdate(float);
            tMed = LoadNow();
            for (size_t i = 0; i < objs->list.size(); i++) {
                // construir los objetos = 30%->95% de la barra (los imports de modelo de
                // adentro muestran ademas su propia barra). Throttle interno a ~1.5%.
                ProgressUpdate(0.30f + 0.65f * (float)i / (float)objs->list.size());
                JsonObject(objs->list[i], SceneCollection, base);
            }
            if (g_w3dLoadClock) g_w3dLoadPhases.objectsMs += LoadNow() - tMed;
        }
    } else {
        w3dLogfW("[W3D] el proyecto no trae \"scene\": abre sin objetos");
    }
    // ya existen TODOS los objetos: resolver las referencias modArmature (malla -> armature por nombre)
    ResolvePendingModArmature();
    // las texturas de los materiales que USA alguna malla despiertan (la base a la cola diferida);
    // las de un material HUERFANO quedan dormidas hasta que una malla lo use
    TexturesWakeUsed();
    // ...y las CURVAS DE ANIMACION de los objetos, que tambien los referencian por nombre
    tMed = LoadNow();
    if (!skipBlock) LoadAnimationsScene(root);   // (las de la inicial vienen con ella)
    if (!skipBlock) AttachPendingClips(root);    // los clips de objeto, despues de las animaciones de escena
    else PendingClipsForget();
    if (g_w3dLoadClock) g_w3dLoadPhases.animSceneMs += LoadNow() - tMed;
    // "fullscreen" solo si esta presente (no pisar la config del editor con un default)
    if (JChild(root, "fullscreen", 3)) ApplyFullscreen(JB(root, "fullscreen", false));
    // estado de reproduccion al GUARDAR (v3, opcional): guardado en pausa -> abre en
    // pausa. Ausente queda -1 y el pie de OpenW3D hace el auto-play de siempre.
    if (JChild(root, "playing", 3))
        g_projPlaying = JB(root, "playing", true) ? 1 : 0;
    // MULTI-ESCENA: escena inicial + modo (opcionales). Si el .w3d trae varios nodos "ui",
    // esta es la que arranca; el resto quedan cargadas y se activan con cambiarEscena().
    // Ausentes: escenaInicial "" (arranca la primera) y modo apagado (retrocompat 1 escena).
    ApplyInitialScene(JS(root, "initialScene", ""));
    ApplySceneMode(JB(root, "sceneMode", false));
    // FORMATO DE LAS MALLAS al guardar (opcion del proyecto, ver import_w3d.h). Ausente =
    // binario (.w3db); "texto" = .w3dm para quien quiere diffs legibles.
    g_w3dFormatMeshesProject = (JS(root, "meshFormat", "binary") == "text") ? W3D_MESHES_TEXT
                                                                                   : W3D_MESHES_BINARY;
    // EL ORIGEN de los assets importados (ver W3dContainer.h): de que archivo de disco salio cada entrada
    W3dAssetOriginsClear();
    if (JVal* jo = JChild(root, "origins", 4))
        for (std::map<std::string, JVal*>::const_iterator it = jo->obj.begin(); it != jo->obj.end(); ++it)
            if (it->second && it->second->type == 2) W3dAssetOriginAnnotate(it->first, it->second->str);
    // PIXELADO GLOBAL (ver la rama de texto arriba y w3dGraphics.h). Ausente =
    // false: nada de lo que ya andaba cambia de aspecto.
    w3dEngine::SetPixelatedGlobal(JB(root, "pixelated", false));
    // MIPMAPPING (ver la rama de texto): ausente = default del EDITOR (cfg.mipmaps),
    // presente = manda el proyecto. Antes de la cola diferida, asi gobierna la subida.
    w3dEngine::SetMipmapsGlobal(cfg.mipmaps);
    { JVal* jm = JChild(root, "mipmaps", 3); if (jm) w3dEngine::SetMipmapsGlobal(jm->b); }
    // TEXTURAS DE 16 BITS (ausente = solo las exactas). Antes de la cola diferida: gobierna esta carga
    { JVal* j16 = JChild(root, "textures16", 3); w3dEngine::SetTextures16Always(j16 && j16->b); }
    // EL TRAZADO DE RAYOS del proyecto (ausente = el de fabrica). Es del editor (el render final): el juego no lo trae
#ifndef W3D_NO_EDITOR
    {
        g_rt = W3dRTOptions();
        JVal* t = JChild(root, "raytraced", 4);
        if (t) {
            g_rt.on = JB(t, "on", g_rt.on); g_rt.rays = JI(t, "rays", g_rt.rays); g_rt.samples = JI(t, "samples", g_rt.samples);
            g_rt.passes = JI(t, "passes", g_rt.passes); g_rt.denoiser = JI(t, "denoiser", g_rt.denoiser);
            g_rt.gpu = JB(t, "gpu", g_rt.gpu); g_rt.onlyFinal = JB(t, "finalOnly", g_rt.onlyFinal); g_rt.scale = JI(t, "scale", g_rt.scale);
            g_rt.layers = JI(t, "layers", g_rt.layers); g_rt.upscaler = JI(t, "upscaler", g_rt.upscaler);
            g_rt.seedVariable = JB(t, "variableSeed", g_rt.seedVariable); if (g_rt.layers < 1) g_rt.layers = 1; if (g_rt.layers > 64) g_rt.layers = 64;
            g_rt.blur = JB(t, "blur", g_rt.blur); g_rt.shutter = JF(t, "shutter", g_rt.shutter);
            g_rt.shutterPos = JI(t, "shutterPos", g_rt.shutterPos);
            if (g_rt.shutter < 0.01f) g_rt.shutter = 0.01f; if (g_rt.shutter > 1.0f) g_rt.shutter = 1.0f;
            if (g_rt.shutterPos < 0 || g_rt.shutterPos > 2) g_rt.shutterPos = 0;
            g_rt.bounces = JI(t, "bounces", g_rt.bounces); if (g_rt.bounces < 0) g_rt.bounces = 0; if (g_rt.bounces > 8) g_rt.bounces = 8;
        }
        RTInvalidate();
    }
#endif
    // EL POSTPROCESADO (render/PostProcess.h): sin el bloque, apagado y sin efectos
    {
        // (los proyectos de ANTES tenian UN postprocesado global, el bloque "postproceso": pasa a ser un objeto
        //  Postproceso al FINAL de la escena, que es donde se aplicaba)
        JVal* t = JChild(root, "postProcess", 4);
        if (t && SceneCollection) {
            W3dPostOptions old;
            W3dPostRead(t, old);
            old.on = JB(t, "on", false);
            old.active = JI(t, "active", old.active);
            if (old.active >= (int)old.effects.size()) old.active = (int)old.effects.size() - 1;
            if (old.on || !old.effects.empty()) {
                PostProcess* pp = new PostProcess(SceneCollection);
                pp->opt = old;
            }
        }
    }
    { JVal* j16 = JChild(root, "textures16Mobile", 3); w3dEngine::SetTextures16Mobile(j16 && j16->b); }
    // TEXTURAS A LA MITAD EN EL N95 (ausente = 0, ninguna): un lado mayor a este se sube a la mitad. Idem: antes de la cola
    w3dEngine::SetTexturesHalfFrom(JI(root, "halfTexturesFrom", 0));
    // MALLAS SOLO EN GPU (ausente = false): las que solo dibujan rutinas sueltan su RAM al quedar en VBO (MeshResource.h)
    W3dMeshesOnlyGPUActivate(JB(root, "gpuOnlyMeshes", false));
    // icono del juego (opcional): ruta EXTERNA relativa al .w3d. La usa la tarjeta
    // Juego y Compilar juego (genera los tamanos chicos al compilar).
    ApplyIcon(JsonPath(JS(root, "icon", ""), base));
    // config de la tarjeta Juego (opcional): modo ventana, orientacion, flags,
    // assets y plataforma del juego COMPILADO. Ausente = defaults del editor.
    ApplyCompile(JChild(root, "compile", 4));
    // RENDER (opcional): la tarjeta Render. (Su "inicio"/"fin"/"fps" de los .w3d viejos -un rango propio de la secuencia-
    // se ignoran: el render usa el rango y los fps de la escena / animacion activa)
    {
        JVal* rn = JChild(root, "render", 4);
        g_shadows = W3dShadowsOpt();   // (sin la tarjeta en el archivo: las de fabrica)
        // el resto de la tarjeta Render (g_render): lo que no esta, el valor de fabrica. La carpeta, si existe aca
        const W3dRenderOpt d;
        W3dRenderOpt r;
        if (rn) {
            const std::string folder = JS(rn, "folder", "");
            if (!folder.empty() && w3dFileSystem::IsDir(folder)) r.folder = folder;
            r.name = JS(rn, "name", d.name.c_str());
            r.w = JF(rn, "width", d.w); r.h = JF(rn, "height", d.h);
            r.format  = JI(rn, "format", d.format); r.quality = JF(rn, "quality", d.quality);
            r.zbuffer = JB(rn, "zbuffer", d.zbuffer); r.normal = JB(rn, "normal", d.normal); r.alpha = JB(rn, "alpha", d.alpha);
            r.vectors = JB(rn, "vectors", d.vectors); r.blur = JB(rn, "blur", d.blur);
            r.shutter = JF(rn, "shutter", d.shutter);
            r.final = JB(rn, "final", d.final); r.finalAlpha = JB(rn, "finalAlpha", d.finalAlpha); r.post = JB(rn, "post", d.post);
            r.sound = JB(rn, "sound", d.sound); r.audioFormat = JI(rn, "audio", d.audioFormat);
            r.audioKbps = JF(rn, "audioKbps", d.audioKbps);
            r.videoCodec = JI(rn, "videoCodec", d.videoCodec); r.videoKbps = JF(rn, "videoKbps", d.videoKbps);
            r.antialias = JI(rn, "antialias", d.antialias);
            // EL FONDO: color, color o textura 360, rotacion y las texturas de fondo (en orden: la pista animable las
            // nombra por su lugar; una que no carga deja su lugar vacio)
            { Vector3 f(0, 0, 0); JVec3(rn, "background", f); g_renderBg[0] = f.x; g_renderBg[1] = f.y; g_renderBg[2] = f.z; g_renderBg[3] = 1.0f; }
            { Vector3 f(0, 0, 0); JVec3(rn, "backgroundRot", f); g_renderBgRot[0] = f.x; g_renderBgRot[1] = f.y; g_renderBgRot[2] = f.z; }
            g_renderBgMode = JI(rn, "backgroundMode", 0) == 1 ? 1 : 0;
            g_renderLightBackground = JF(rn, "backgroundLight", 1.0f);   // (la luz del fondo: 1 = 100%)
            if (g_renderLightBackground < 0.0f) g_renderLightBackground = 0.0f;
            // LAS SOMBRAS EN TIEMPO REAL (lo que no esta: de fabrica)
            { const W3dShadowsOpt sd; W3dShadowsOpt so;
              so.on = JB(rn, "shadows", sd.on);
              so.distance = JF(rn, "shadowDistance", sd.distance);
              so.cascades = JI(rn, "shadowCascades", sd.cascades);
              { Vector3 ct(sd.cuts[0], sd.cuts[1], sd.cuts[2]); JVec3(rn, "shadowSplits", ct);
                so.cuts[0] = ct.x; so.cuts[1] = ct.y; so.cuts[2] = ct.z; }
              so.border = JF(rn, "shadowBorder", sd.border);
              so.blur       = JF(rn, "shadowBlur", sd.blur);
              so.resolution = JI(rn, "shadowResolution", sd.resolution);
              so.smooth = JB(rn, "softShadows", sd.smooth);
              so.intensity = JF(rn, "shadowIntensity", sd.intensity);
              so.bias = JF(rn, "shadowBias", sd.bias);
              { Vector3 cc(sd.color[0], sd.color[1], sd.color[2]); JVec3(rn, "shadowColor", cc);
                so.color[0] = cc.x; so.color[1] = cc.y; so.color[2] = cc.z; so.color[3] = 1.0f; }
              if (so.cascades < 1) so.cascades = 1; if (so.cascades > 4) so.cascades = 4;
              g_shadows = so; }
            JVal* ft = JChild(rn, "backgroundTextures", 5);
            if (ft) {
                for (size_t i = 0; i < ft->list.size(); i++) {
                    Texture* t = NULL;
                    if (ft->list[i] && ft->list[i]->type == 2 && !ft->list[i]->str.empty())
                        t = TextureTake(JsonPath(ft->list[i]->str, base));
                    g_renderBgTexs.push_back(t);   // (con la referencia de TextureTake: la suelta W3dBackgroundClear)
                }
                const int sel = JI(rn, "backgroundTexture", 0);
                g_renderBgTex = (sel >= 1 && sel <= (int)g_renderBgTexs.size()) ? g_renderBgTexs[(size_t)(sel - 1)] : NULL;
            }
        }
        g_render = r;
        g_renderGen++;
    }
    // MIX (opcional): el modo Mix del timeline + su rango + las capas de escena
    {
        JVal* mx = JChild(root, "mix", 4);
        g_animMix = mx ? JB(mx, "active", false) : false;
        g_mixStart = mx ? JF(mx, "start", 1.0f) : 1.0f;
        g_mixEnd   = mx ? JF(mx, "end", 250.0f) : 250.0f;
        ReadLayers(mx ? JChild(mx, "scenes", 5) : NULL, g_mixScenes);
        g_mixActiveScene = -1;
    }
    // SESION (opcional): frame actual + seleccion. Solo se LEE aca; se aplica en el
    // pie de OpenW3D, con los nombres ya definitivos (ver g_sessionFrame arriba).
    {
        JVal* session = JChild(root, "session", 4);
        if (session) {
            // el -1 significa "no habia bloque": un frame negativo del archivo se acota a 0 aca
            // y no se disfraza de ausente (la cota de arriba la pone SessionApply)
            g_sessionFrame = JI(session, "frame", 0);
            if (g_sessionFrame < 0) {
                w3dLogfW("[W3D] sesion: 'frame' vale %d (negativo) -> se acota a 0", g_sessionFrame);
                g_sessionFrame = 0;
            }
            g_activeSession = JS(session, "active", "");
            {   // la animacion de la linea de tiempo (ausente = la de escena)
                const std::string t = JS(session, "animType", "");
                g_sessionAnimType = t == "armature" ? 1 : t == "object" ? 3 : t == "armature2d" ? 4 : t == "flipbook" ? 5 :
                                t == "scene" ? 10 : 0;   // (10: una animacion de escena de un JUEGO, que abre en "Juego")
                g_sessionAnimObject = JS(session, "animObject", "");
                g_sessionAnimClip = JI(session, "animClip", -1);
            }
            // la raiz que se estaba editando (ausente = la escena del bloque)
            g_sessionRoot = JS(session, "root", "");
            g_sessionRootType = W3dRootTypeByKey(JS(session, "rootType", "scene"), W3D_ROOT_SCENE);
            JVal* sel = JChild(session, "selection", 5);
            if (sel)
                for (size_t i = 0; i < sel->list.size(); i++)
                    if (sel->list[i] && sel->list[i]->type == 2)   // 2 = string
                        g_sessionSelection.push_back(sel->list[i]->str);
        }
    }
    ApplyLayoutText(JS(root, "layout", "2d"));
    delete root;
    return true;
}

// ============================================================================
//  ANEXAR UN .w3d EN RUNTIME (el streaming del modo juego)
//
//  Pedido del demo GTA: importarW3D("assets/claude/claude.w3d") desde Lua trae
//  un personaje/vehiculo ADENTRO de la escena que ya esta corriendo: objetos
//  (con jerarquia), materiales y SUS animaciones de escena. A diferencia de
//  OpenSceneJson NO limpia nada ni toca el estado del proyecto (fps, layout,
//  sesion, escena inicial, animacion activa): solo AGREGA.
//
//  Limites, a proposito y documentados:
//   - SOLO acepta JSON v3 plano (el contenedor v4/zip habria que montarlo
//     ademas del proyecto ya montado: pendiente).
//   - los nombres del archivo se conservan TAL CUAL (W3dNamesLoading):
//     importar dos veces el mismo archivo duplica nombres y `find()` de lua
//     devuelve el primero. Evitarlo es responsabilidad del que importa.
//   - no corre los post-pasos de targets/constraints del abrir completo (los
//     assets de juego tipo personaje/vehiculo no los usan).
// ============================================================================

// las animaciones de escena del archivo anexado: mismo parseo que
// LoadAnimationsScene pero SIN limpiar las existentes y SIN tocar la
// activa (en el juego se reproducen por nombre con animEscena() de lua).
static void AppendAnimationsScene(JVal* root) {
    JVal* ja = JChild(root, "animations", 4);
    if (!ja) return;
    JVal* jes = JChild(ja, "scenes", 5);
    if (!jes || jes->list.empty()) return;
    InitSceneAnimations();
    size_t nEsc = jes->list.size();
    if (nEsc > (size_t)kAnimMaxScenes) nEsc = (size_t)kAnimMaxScenes;
    for (size_t i = 0; i < nEsc; i++) {
        JVal* je = jes->list[i];
        if (!je || je->type != 4) continue;
        SceneAnimation* esc = new SceneAnimation(JS(je, "name", "Scene"));
        esc->startFrame = JI(je, "start", 1);
        esc->endFrame   = JI(je, "end", 250);
        esc->fps        = JI(je, "fps", 30);
        if (esc->startFrame < 0) esc->startFrame = 0;
        if (esc->endFrame < esc->startFrame) esc->endFrame = esc->startFrame;
        if (esc->fps < 1) esc->fps = 1;
        if (esc->fps > 120) esc->fps = 120;
        SceneAnimations.push_back(esc);
        JVal* jobjs = JChild(je, "objects", 5);
        if (!jobjs) continue;
        size_t nObj = jobjs->list.size();
        if (nObj > (size_t)kAnimMaxObjects) nObj = (size_t)kAnimMaxObjects;
        for (size_t o = 0; o < nObj; o++) {
            JVal* jo = jobjs->list[o];
            if (!jo || jo->type != 4) continue;
            const std::string name = JS(jo, "object", "");
            if (name.empty()) continue;
            // los objetos recien anexados ya cuelgan del arbol: scope global
            Object* obj = AnimFindScopeGlobal(SceneCollection, name, false);
            if (!obj) {
                w3dLogfW("[W3D] anexo: la animacion '%s' referencia a '%s', que no aparecio: se descarta",
                         esc->name.c_str(), name.c_str());
                continue;
            }
            AnimationObject ao;
            ao.obj = obj; ao.FirstKeyFrame = 0; ao.LastKeyFrame = 0;
            JVal* jcur = JChild(jo, "curves", 5);
            if (jcur) {
                size_t nCur = jcur->list.size();
                if (nCur > (size_t)kAnimMaxCurves) nCur = (size_t)kAnimMaxCurves;
                for (size_t c = 0; c < nCur; c++) {
                    AnimProperty ap;
                    if (!LoadCurveJson(jcur->list[c], ap)) continue;
                    if (ap.keyframes.empty()) continue;
                    ap.SortKeyFrames();
                    ao.Properties.push_back(ap);
                }
            }
            if (ao.Properties.empty()) continue;
            ao.UpdateFirstLastFrame();
            SceneAnimations.back()->objects.push_back(ao);
        }
    }
}

// Los ctores de Object/Mesh dejan seleccionado lo que crean (comportamiento de
// editor). OpenW3D lo limpia con DeselectAll(); el anexo NO puede hacer
// eso (pisaria la seleccion del usuario en plena sesion), asi que deselecciona
// SOLO el subarbol recien importado.
static void DeselectSubtree(Object* o) {
    if (!o) return;
    if (o->select) o->Deselect();
    if (ActiveObj == o) ActiveObj = NULL;
    for (size_t i = 0; i < o->Childrens.size(); i++)
        DeselectSubtree(o->Childrens[i]);
}

Object* W3dImportW3DAppendix(const std::string& path) {
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(path, data) || data.empty()) {
        w3dLogfE("[W3D] importarW3D: no pude leer %s", path.c_str());
        return NULL;
    }
    size_t i = 0;
    while (i < data.size() && (data[i] == ' ' || data[i] == '\t' ||
                                data[i] == '\r' || data[i] == '\n')) i++;
    if (i >= data.size() || data[i] != '{') {
        w3dLogfE("[W3D] importarW3D: %s no es un .w3d JSON v3 plano (unico formato anexable por ahora)",
                 path.c_str());
        return NULL;
    }
    JParser parser((const char*)&data[0] + i, data.size() - i);
    JVal* root = parser.Value();
    if (parser.error || root->type != 4) {
        w3dLogfE("[W3D] importarW3D: el JSON de %s no parsea", path.c_str());
        delete root;
        return NULL;
    }
    // las rutas internas (meshes/, textures/) cuelgan de la carpeta DEL ARCHIVO
    size_t bar = path.find_last_of("/\\");
    const std::string base = (bar == std::string::npos) ? std::string(".") : path.substr(0, bar);
    LoadMaterials(root, base);
    Object* first = NULL;
    JVal* esc = JChild(root, "scene", 4);
    JVal* objs = esc ? JChild(esc, "objects", 5) : NULL;
    if (objs && SceneCollection) {
        const bool prev = W3dNamesLoading;
        W3dNamesLoading = true;    // conservar los nombres tal como vienen
        const size_t nPrev = SceneCollection->Childrens.size();
        for (size_t k = 0; k < objs->list.size(); k++)
            JsonObject(objs->list[k], SceneCollection, base);
        W3dNamesLoading = prev;
        if (SceneCollection->Childrens.size() > nPrev)
            first = SceneCollection->Childrens[nPrev];
        for (size_t k = nPrev; k < SceneCollection->Childrens.size(); k++)
            DeselectSubtree(SceneCollection->Childrens[k]);
        // si la partida esta andando (importarW3D lo llamo un script), dar de
        // alta los cuerpos rigidos de lo que acaba de entrar
        { extern bool SimActive();
          if (SimActive())
              for (size_t k = nPrev; k < SceneCollection->Childrens.size(); k++)
                  W3dRigidEnsure(SceneCollection->Childrens[k]); }
    }
    ResolvePendingModArmature();
    TexturesWakeUsed();   // las de los materiales que usa lo que acaba de entrar
    AppendAnimationsScene(root);
    AttachPendingClips(NULL);   // los clips de los objetos anexados (sin tocar cual esta elegida)
    delete root;
    w3dLogf("[W3D] importarW3D: %s anexado (raiz '%s', %d animaciones de escena en total)",
            path.c_str(), first ? first->name.c_str() : "sin objetos",
            (int)SceneAnimations.size());
    return first;
}


// ---------------------------------------------------------------------------
//  PEGAR desde el PORTAPAPELES DE OBJETOS (io/SaveW3D.cpp: W3dClipboardCopy escribio la carpeta, quizas otro
//  Whisk3D): las mallas entran al registro con un nombre LIBRE (las del objeto se re-apuntan a ese nombre), los
//  materiales por la puerta de siempre (el homonimo identico se reusa) y los objetos con el lector de siempre, con
//  nombres libres (pegar dos veces da "Cubo.001", "Cubo.002"). 'nuevos' = los objetos de arriba que entraron
// ---------------------------------------------------------------------------
#ifndef W3D_NO_EDITOR   // (pegar es del editor: el juego no tiene portapapeles de objetos)
static void RenameMeshesJson(JVal* j, const std::map<std::string, std::string>& row) {
    if (!j) return;
    if (j->type == 4) {
        std::map<std::string, JVal*>::iterator m = j->obj.find("mesh");
        if (m != j->obj.end() && m->second && m->second->type == 2) {
            std::map<std::string, std::string>::const_iterator r = row.find(m->second->str);
            if (r != row.end()) m->second->str = r->second;
        }
        for (std::map<std::string, JVal*>::iterator it = j->obj.begin(); it != j->obj.end(); ++it) RenameMeshesJson(it->second, row);
    } else if (j->type == 5) {
        for (size_t i = 0; i < j->list.size(); i++) RenameMeshesJson(j->list[i], row);
    }
}
bool W3dClipboardPaste(const std::string& folder, std::vector<Object*>& newValue) {
    newValue.clear();
    if (!SceneCollection) return false;
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(folder + "/clipboard.json", data) || data.empty()) return false;
    JParser parser((const char*)&data[0], data.size());
    JVal* root = parser.Value();
    if (parser.error || !root || root->type != 4) { delete root; return false; }
    // las MALLAS: un recurso nuevo por cada una, con nombre libre
    std::map<std::string, std::string> row;
    JVal* jm = JChild(root, "meshes", 5);
    if (jm) for (size_t i = 0; i < jm->list.size(); i++) {
        JVal* e = jm->list[i]; if (!e || e->type != 4) continue;
        const std::string name = JS(e, "name", "Mesh"), arch = JS(e, "file", "");
        std::vector<unsigned char> b;
        if (arch.empty() || !w3dFileSystem::ReadFileBytes(folder + "/" + arch, b) || b.empty()) continue;
        MeshResource* r = W3dMeshNewResource(name, std::string(), std::string());
        if (!r) continue;
        W3dMeshResourceSetBytes(r, std::string((const char*)&b[0], b.size()));
        row[name] = r->name;
    }
    RenameMeshesJson(JChild(root, "objects", 5), row);
    LoadMaterials(root, folder);
    JVal* objs = JChild(root, "objects", 5);
    if (objs) {
        const size_t nPrev = SceneCollection->Childrens.size();
        for (size_t k = 0; k < objs->list.size(); k++) JsonObject(objs->list[k], SceneCollection, folder);
        for (size_t k = nPrev; k < SceneCollection->Childrens.size(); k++) newValue.push_back(SceneCollection->Childrens[k]);
        // NOMBRES LIBRES: lo pegado no le pisa el nombre a nadie ("Cubo" -> "Cubo.001"), con sus hijos
        struct Free { static void In(Object* o) { if (!o) return; o->name = W3dFreeNameFor(&o->name, o->name);
            for (size_t i = 0; i < o->Childrens.size(); i++) In(o->Childrens[i]); } };
        for (size_t k = 0; k < newValue.size(); k++) Free::In(newValue[k]);
    }
    ResolvePendingModArmature();
    TexturesWakeUsed();
    AttachPendingClips(NULL);
    delete root;
    return !newValue.empty();
}
#endif // W3D_NO_EDITOR (pegar)

// ---------------------------------------------------------------------------
//  MigrateOldConstraints: el segundo tiempo de la migracion (ver OldPendCons).
//
//  Corre al final de OpenW3D, con el arbol COMPLETO (por eso es un segundo tiempo:
//  el objeto apuntado puede venir despues en el archivo) y JUSTO ANTES de
//  W3dNamesRepairScene.
//
//  *** POR QUE ANTES DE LA REPARACION DE NOMBRES (y no despues, como estaba) ***
//  Un nodo constraint viejo puede llamarse IGUAL que un objeto real, y en el formato
//  viejo eso era inofensivo: nadie buscaba a un nodo constraint por nombre y no habia
//  reparacion de duplicados. Hoy el choque si importa. Si la reparacion corre con los
//  nodos todavia en la escena, ve dos "X" y renombra a uno de los dos. Cuando el nodo
//  gana el preorden (viene antes en el archivo), la que se lleva el ".001" es LA MALLA
//  DEL USUARIO -- y el nodo se borra unas lineas mas abajo, asi que ese ".001" queda
//  para siempre en la escena y en el archivo guardado, sin ningun "X" al lado
//  que lo explique, y los object("X") de lua dejan de encontrarla. Corriendo esta
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
static bool MigConstraintNameExists(const std::string& n, void* ctx) {
    Object* ob = (Object*)ctx;
    for (size_t i = 0; i < ob->constraints.size(); i++)
        if (ob->constraints[i] && ob->constraints[i]->name == n) return true;
    return false;
}
// el nodo viejo sale de la escena (solo se llama cuando NO tiene hijos, asi que la
// promocion de hijos de ~Object no tiene nada que mover y no se corre geometria)
static void ReleaseOldNode(Object* o) {
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
static bool MigNodeSeVa(const Object* o) {
    for (size_t i = 0; i < gOldPendCons.size(); i++)
        if (gOldPendCons[i].node == o && o->Childrens.empty()) return true;
    return false;
}
// la MISMA busqueda por nombre que FindObjectByName (preorden, la raiz incluida) pero
// salteando los nodos que se van. Saltea SOLO al nodo, no a su subarbol: un hijo suyo es
// geometria del usuario y puede ser un destino perfectamente valido.
static Object* MigFindDestination(Object* node, const std::string& name) {
    if (!node) return NULL;
    if (node->name == name && !MigNodeSeVa(node)) return node;
    for (size_t i = 0; i < node->Childrens.size(); i++) {
        Object* r = MigFindDestination(node->Childrens[i], name);
        if (r) return r;
    }
    return NULL;
}
static void MigrateOldConstraints() {
    if (gOldPendCons.empty()) return;
    int placed = 0, noTarget = 0, withChildren = 0, aOtherNode = 0;
    // los NOMBRES de los nodos que se van, juntados ANTES de tocar nada. Van por nombre y no
    // por puntero porque se consultan cuando el nodo apuntado ya puede estar borrado: asi el
    // aviso de "apuntaba a otro constraint viejo" sale igual venga el target antes o despues
    // en el archivo (con el puntero, el que venia antes ya no estaba y el aviso mentia).
    std::vector<std::string> seVan;
    for (size_t i = 0; i < gOldPendCons.size(); i++)
        if (gOldPendCons[i].node && gOldPendCons[i].node->Childrens.empty())
            seVan.push_back(gOldPendCons[i].node->name);
    for (size_t i = 0; i < gOldPendCons.size(); i++) {
        OldPendCons& p = gOldPendCons[i];
        if (!p.node) continue;
        const std::string nodeName = p.node->name;

        Object* dest = (p.target.empty() || !SceneCollection)
                           ? NULL : MigFindDestination(SceneCollection, p.target);
        // el nodo NO puede ser su propio destino. Con MigFindDestination esto ya no puede pasar
        // por el preorden (un nodo que se va no se devuelve nunca), pero se deja: si el nodo
        // TIENE hijos sobrevive, es un destino valido para los demas... y para si mismo seria
        // un constraint que se escribe encima, o sea lo que el objeto viejo hacia = nada.
        if (dest == p.node) dest = NULL;
        if (!dest) {
            // POR QUE no hay destino: si el nombre lo tiene OTRO nodo constraint viejo (de los
            // que se van), el aviso generico -"no esta en la escena"- seria falso: el objeto
            // esta, es que no puede recibir nada. Imposible en un archivo real (el objeto viejo
            // apuntaba a geometria), pero si no se dijera el constraint se perderia MUDO.
            bool otherNode = false;
            if (!p.target.empty() && p.target != nodeName)   // apuntarse a si mismo no es "otro"
                for (size_t k = 0; k < seVan.size(); k++)
                    if (seVan[k] == p.target) { otherNode = true; break; }
            if (otherNode) {
                aOtherNode++;
                w3dLogfW("[W3D] '%s' era un constraint viejo y su target '%s' es OTRO constraint viejo "
                         "que sale de la escena: no hay a quien migrarlo, no se migra nada",
                         nodeName.c_str(), p.target.c_str());
                W3dAvisof(true, T("'%s' pointed to another constraint ('%s'): it could not be migrated"),
                          W3dShortName(nodeName).c_str(), W3dShortName(p.target).c_str());
            } else {
                // el constraint viejo arrancaba con "if (!target) return": SIN target no hacia
                // absolutamente nada. No se inventa un constraint apuntando a la nada; se avisa
                // y listo (el nodo corre la misma suerte que los demas, mas abajo).
                noTarget++;
                w3dLogfW("[W3D] '%s' era un constraint viejo y su target '%s' no esta en la escena: "
                         "no hacia nada, no se migra nada",
                         nodeName.c_str(), p.target.c_str());
            }
        } else {
            // *** LA INVERSION: las props del nodo van SOBRE EL OBJETO APUNTADO ***
            // POSICION PRIMERO y rotacion despues, que es el orden en el que las aplicaba
            // el RenderObject viejo (y el stack se evalua en orden).
            if (p.copyX || p.copyY || p.copyZ) {
                W3dConstraint* c = new W3dConstraint(W3dConstraintType::CopyLocation);
                // la fuente vieja era la camara del viewport (viewPosGlobal), no un objeto
                c->sourceType = W3dConstraintSource::View;
                c->axisX = p.copyX; c->axisY = p.copyY; c->axisZ = p.copyZ;
                c->name = W3dUniqueName(c->name, "Constraint", MigConstraintNameExists, dest);
                dest->constraints.push_back(c);
                placed++;
            }
            if (p.horizontal || p.pitch) {
                W3dConstraint* c = new W3dConstraint(W3dConstraintType::Billboard);
                c->bbYaw = p.horizontal; c->bbPitch = p.pitch;   // las 4 combinaciones migran exacto
                c->Normalize();
                c->name = W3dUniqueName(c->name, "Constraint", MigConstraintNameExists, dest);
                dest->constraints.push_back(c);
                placed++;
            }
            if (!dest->constraints.empty())
                dest->activeConstraint = (int)dest->constraints.size() - 1;
        }

        // ---- que pasa con el NODO viejo ----
        // La transform propia del nodo es dato MUERTO para el efecto (RenderObject nunca la
        // usaba). Lo unico que puede importar son sus HIJOS:
        //  - sin hijos (los 7 nodos reales de los .w3d que lo destaparon): desaparece.
        //  - con hijos: se queda como Empty, con el MISMO nombre y la MISMA pos/rot/escala.
        //    Es la opcion de CERO MATEMATICA. Borrarlo los haria subir por ~Object con
        //    "pos += pos del nodo", que solo conserva la posicion de mundo si el nodo tenia
        //    rotacion identidad y escala 1: si no, mueve geometria del usuario EN SILENCIO.
        if (p.node->Childrens.empty()) {
            ReleaseOldNode(p.node);
        } else {
            withChildren++;
            w3dLogfW("[W3D] '%s' era un constraint viejo y tiene %d hijo(s): queda como Empty "
                     "en la misma posicion (sus propiedades ya pasaron al objeto apuntado)",
                     nodeName.c_str(), (int)p.node->Childrens.size());
            W3dAvisof(false, T("'%s' was a constraint: it is now an Empty, its %d child(ren) stayed where they were"),
                      W3dShortName(nodeName).c_str(), (int)p.node->Childrens.size());
        }
        p.node = NULL;
    }
    w3dLogf("[W3D] constraints viejos migrados: %d nodo(s) -> %d constraint(s) sobre el objeto "
            "apuntado (%d sin target, %d apuntando a otro constraint viejo, %d quedaron como "
            "Empty por tener hijos)",
            (int)gOldPendCons.size(), placed, noTarget, aOtherNode, withChildren);
    if (noTarget > 0)
        W3dAvisof(true, T("%d old constraint(s) without a target: they did nothing, nothing was migrated"), noTarget);
    gOldPendCons.clear();
}

// ============================================================================
//  EL CONTENIDO DE UNA RAIZ (escena 3D o prefab, ver W3dRoots.h) desde su entrada:
//  la carga PEREZOSA del editor y el cambiarEscena() del juego. SceneCollection ES en
//  este momento esa raiz (vacia), con su propio contexto: lo que se arma aca es suyo.
//  Mismo lector y mismos post-pasos que abrir el proyecto (sin tocar lo que es del
//  proyecto: materiales, mallas, animsets, flipbooks y paletas ya estan cargados).
//    escena del bloque  -> project.json: "escena".objetos + "animaciones" + "mix"
//    escena (.w3de)     -> "objetos" + "animaciones" + "mix"
//    prefab (.w3dp)     -> "raiz" (+ "sueltos") + "animaciones" + "mix"
// ============================================================================
bool W3dRootReadContent(const W3dRootRow& row) {
    if (!SceneCollection) return false;
    const std::string path = row.block ? std::string("project.json") : row.entry;
    JVal* root = NULL;
    std::vector<unsigned char> data;
    if (!path.empty()) {
        if (!w3dFileSystem::ReadFileBytes(path, data) || data.empty()) {
            w3dLogfE("[W3D] no pude leer '%s' (%s '%s'): queda vacia", path.c_str(),
                     W3dRootTypeKey(row.type), row.name.c_str());
            return false;
        }
        JParser parser((const char*)&data[0], data.size());
        root = parser.Value();
        if (parser.error || !root || root->type != 4) {
            w3dLogfE("[W3D] '%s' no parsea (%s '%s'): queda vacia", path.c_str(),
                     W3dRootTypeKey(row.type), row.name.c_str());
            delete root;
            return false;
        }
    }
    // las rutas de adentro son NOMBRES DE ENTRADA (igual que al abrir el contenedor)
    const bool nc = W3dNamesLoading, v4 = gProjectV4, refs = g_w3dRefsEntries;
    W3dNamesLoading = true;
    gProjectV4 = true;
    g_w3dRefsEntries = true;
    gPendModArm.clear(); gPendModTgt.clear(); gPendModGen.clear();
    gOldPendCons.clear();
    PendingClipsForget();
    const std::string base = gBaseProject;   // (la misma del proyecto: las "ext:" relativas resuelven igual)
    if (root) {
        JVal* objs = NULL;
        if (row.block) {
            JVal* esc = JChild(root, "scene", 4);
            objs = esc ? JChild(esc, "objects", 5) : NULL;
        } else if (row.type == W3D_ROOT_PREFAB) {
            JVal* jr = JChild(root, "root", 4);
            // (una instancia de ESTE prefab adentro de si mismo no se genera: seria infinito)
            W3dPrefabStackEnter(row.name);
            if (jr) JsonObject(jr, SceneCollection, base);
            W3dPrefabStackExit();
            objs = JChild(root, "loose", 5);   // lo que el editor dejo suelto al primer nivel
        } else {
            objs = JChild(root, "objects", 5);
        }
        if (objs)
            for (size_t i = 0; i < objs->list.size(); i++) JsonObject(objs->list[i], SceneCollection, base);
    }
    // un prefab tiene SIEMPRE su objeto raiz (uno recien creado todavia no se guardo)
    if (row.type == W3D_ROOT_PREFAB && SceneCollection->Childrens.empty()) {
        Empty* e = new Empty(NULL, Vector3(0, 0, 0));
        e->name = row.name;
    }
    ResolvePendingModArmature();
    TexturesWakeUsed();
    if (root) {
        LoadAnimationsScene(root);   // las animaciones de escena de ESTA raiz
        AttachPendingClips(root);    // ...y los clips de sus objetos
        JVal* mx = JChild(root, "mix", 4); // ...y sus capas del Mix de escenas (el modo y el rango son del proyecto)
        ReadLayers(mx ? JChild(mx, "scenes", 5) : NULL, g_mixScenes);
        g_mixActiveScene = -1;
    } else InitSceneAnimations();
    delete root;
    W3dNamesLoading = nc;
    gProjectV4 = v4;
    g_w3dRefsEntries = refs;
    // los post-pasos del abrir, sobre ESTA raiz
    MigrateOldConstraints();
#ifndef W3D_NO_EDITOR
    W3dNamesRepairScene(true);
    W3dResourcesViewInvalidate();         // el outliner por recursos: hay usuarios nuevos
#endif
    SearchLoop();
    SceneCollection->ReloadAll();
    W3dConstraintsResolveNames(SceneCollection);
#ifdef W3D_NO_EDITOR
    LoadAllPendingTextures();     // el juego no arranca un nivel con texturas grises
#endif
    return true;
}

#ifndef W3D_NO_EDITOR
// ============================================================================
//  LA COPIA DE UNA RAIZ ("Convert to Game", io/EditorRoots.h): arma en SceneCollection (la raiz NUEVA, que en este
//  momento es la activa y esta vacia) los objetos que escribio W3dRootSerializeDry (io/SaveW3D.cpp), con el
//  MISMO lector y los MISMOS post-pasos que la carga de una raiz (W3dRootReadContent): nombres crudos (el espacio de
//  nombres de los objetos es por raiz: la copia se llama igual), rutas como estan en memoria, las mallas y los
//  materiales por el nombre de su recurso (compartidos). SIN animaciones de escena: queda la "Scene" vacia de
//  siempre. 'origenes' recibe cada objeto creado con su "_origen" (el serial del original), para lo que el JSON en
//  seco no lleva (W3dPrefabFixHook y lo demas, en RaicesEditor). false = no parsea.
// ============================================================================
bool W3dRootReadCopyDry(const std::string& json, std::vector<std::pair<Object*, unsigned> >* origins) {
    if (!SceneCollection || json.empty()) return false;
    JParser parser(json.data(), json.size());
    JVal* root = parser.Value();
    if (parser.error || !root || root->type != 4) {
        w3dLogfE("[W3D] la copia de la raiz no parsea: queda vacia");
        delete root;
        return false;
    }
    const bool nc = W3dNamesLoading, v4 = gProjectV4, refs = g_w3dRefsEntries;
    W3dNamesLoading = true;
    gProjectV4 = true;
    g_w3dRefsEntries = true;
    gPendModArm.clear(); gPendModTgt.clear(); gPendModGen.clear();
    gOldPendCons.clear();
    PendingClipsForget();
    std::vector<std::pair<Object*, unsigned> >* orig = gOrigins;
    gOrigins = origins;
    JVal* objs = JChild(root, "objects", 5);
    if (objs)
        for (size_t i = 0; i < objs->list.size(); i++) JsonObject(objs->list[i], SceneCollection, gBaseProject);
    gOrigins = orig;
    ResolvePendingModArmature();
    TexturesWakeUsed();
    // (los clips INLINE de un nodo son de las animaciones de escena del formato viejo: la copia no las lleva)
    PendingClipsForget();
    InitSceneAnimations();
    delete root;
    W3dNamesLoading = nc;
    gProjectV4 = v4;
    g_w3dRefsEntries = refs;
    // los post-pasos de la carga de una raiz, sobre ESTA
    MigrateOldConstraints();
    W3dNamesRepairScene(true);
    W3dResourcesViewInvalidate();
    SearchLoop();
    SceneCollection->ReloadAll();
    W3dConstraintsResolveNames(SceneCollection);
    return true;
}
#endif

// ============================================================================
//  LOS HIJOS DE UNA INSTANCIA DE PREFAB (io/Prefabs.h): el objeto raiz del prefab ('j', de su
//  definicion) armado con el MISMO lector bajo el nodo de la instancia. Nombres CRUDOS (la
//  instancia es frontera de scope) y rutas de entrada, como la carga de una raiz. Se resuelve
//  aca lo pendiente de ESTE subarbol (modArmature, targets de modificadores) por scope: si la
//  generacion ocurre en medio de una carga (una escena con instancias), lo pendiente de la carga
//  de afuera se aparta y vuelve intacto.
// ============================================================================
Object* W3dPrefabBuildChildren(Object* parent, JVal* j, std::vector<std::pair<Object*, unsigned> >* origins) {
    if (!parent || !j || j->type != 4) return NULL;
    std::vector<PendModArm> arm; arm.swap(gPendModArm);
    std::vector<PendModTgt> tgt; tgt.swap(gPendModTgt);
    std::vector<Mesh*> gen; gen.swap(gPendModGen);
    std::vector<PendingClip> clips; clips.swap(gClipsPend);
    const bool nc = W3dNamesLoading, v4 = gProjectV4, refs = g_w3dRefsEntries, genPrefab = gGeneratingPrefab;
    W3dNamesLoading = true;
    gProjectV4 = true;
    g_w3dRefsEntries = true;
    gGeneratingPrefab = true;   // (las vertex anims de lo generado comparten sus frames entre instancias)
    std::vector<std::pair<Object*, unsigned> >* orig = gOrigins;
    gOrigins = origins;
    const size_t prev = parent->Childrens.size();
    JsonObject(j, parent, gBaseProject);
    gOrigins = orig;
    gGeneratingPrefab = genPrefab;
    ResolvePendingModArmature();
    // (los clips de objeto INLINE son del formato de la primera version de las escenas: una definicion de
    //  prefab no los trae; si un archivo escrito a mano los tuviera, no hay a que raiz adjuntarlos)
    PendingClipsForget();
    W3dNamesLoading = nc;
    gProjectV4 = v4;
    g_w3dRefsEntries = refs;
    gPendModArm.swap(arm); gPendModTgt.swap(tgt); gPendModGen.swap(gen); gClipsPend.swap(clips);
    TexturesWakeUsed();   // las de los materiales que usa lo que acaba de entrar
    return parent->Childrens.size() > prev ? parent->Childrens.back() : NULL;
}

// ============================================================================
//  LOS RECURSOS DE UN SUBARBOL (io/Prefabs.h: W3dPrefabResourcesJson): lo que el STREAMING (io/Streaming.h) pide
//  ASYNC antes de generar una instancia -las mallas, los animsets (los clips de un esqueleto y la biblioteca de
//  clips de jerarquia) y las fuentes de los scripts-, resuelto con las MISMAS reglas que JsonObject (el prefijo de
//  la libreria del contexto, las rutas "lib:", las entradas del contenedor). No crea ni carga nada.
// ============================================================================
static void ResourceAdd(std::vector<W3dLoadsItem>& out, int type, const std::string& id) {
    if (id.empty()) return;
    for (size_t i = 0; i < out.size(); i++) if (out[i].type == type && out[i].id == id) return;
    W3dLoadsItem it; it.type = type; it.id = id;
    out.push_back(it);
}
static void ResourcesJsonRec(JVal* j, std::vector<W3dLoadsItem>& out, std::vector<MeshResource*>* meshes,
                            std::vector<std::string>& nested) {
    if (!j || j->type != 4) return;
    const std::string type = JS(j, "type", "");
    // una INSTANCIA o un PROXY anidado: se genera con la de afuera (sus recursos tambien, por su CLAVE), salvo que
    // sea DIFERIDO: a ese lo pide su propio streaming. El guardado no escribe hijos de una instancia
    if (type == "prefab" || type == "proxy") {
        if (JS(j, "load", "always") == "distance") return;
        std::string key;
        if (type == "prefab") {   // (la misma clave que InstancePrefabReadFields + la rama "prefab" de JsonObject)
            key = JS(j, "prefab", "");
            if (!W3dLibContext().empty() && !key.empty()) key = W3dLibsKey(W3dLibContext(), W3D_LIB_PREFAB, key);
        } else {                  // (la de ProxyW3dReadFields)
            std::string lib = JS(j, "lib", "");
            if (!W3dLibContext().empty()) lib = W3dLibsNested(W3dLibContext(), lib);
            key = W3dLibsKey(lib, JS(j, "elementType", "prefab") == "scene" ? W3D_LIB_SCENE : W3D_LIB_PREFAB,
                                 JS(j, "element", ""));
        }
        if (!key.empty()) nested.push_back(key);
        return;
    }
    // la MALLA del objeto (un recurso del registro del proyecto o de una libreria)
    const std::string nm = ResourceNameJson(JS(j, "mesh", ""));
    if (!nm.empty()) {
        MeshResource* r = W3dMeshResourceByName(nm);
        if (r) {
            const size_t prev = out.size();
            ResourceAdd(out, W3DREC_MESH, r->IdStore());
            if (meshes && out.size() > prev) meshes->push_back(r);
        }
    }
    // los CLIPS de un esqueleto ("anims": el nombre de su animset; la lista inline de los .w3d viejos no es un recurso)
    // y su BIBLIOTECA de clips de jerarquia: los dos son animsets del registro, por su entrada
    const std::string an = ResourceNameJson(JS(j, "anims", ""));
    if (!an.empty()) ResourceAdd(out, W3DREC_ANIMSET, W3dAnimSetsEntryOf(an));
    const std::string cj = ResourceNameJson(JS(j, "hierarchyClips", ""));
    if (!cj.empty()) ResourceAdd(out, W3DREC_ANIMSET, W3dAnimSetsEntryOf(cj));
    // las FUENTES de sus scripts (la misma ruta que JsonScripts: es la clave con la que RunFile las busca)
    JVal* js = JChild(j, "scripts", 5);
    if (js)
        for (size_t i = 0; i < js->list.size(); i++)
            if (js->list[i] && js->list[i]->type == 4) {
                const std::string a = JS(js->list[i], "file", "");
                if (!a.empty()) ResourceAdd(out, W3DREC_SCRIPT, JsonPath(a, gBaseProject));
            }
    JVal* h = JChild(j, "children", 5);
    if (h) for (size_t i = 0; i < h->list.size(); i++) ResourcesJsonRec(h->list[i], out, meshes, nested);
}
void W3dPrefabResourcesJson(JVal* j, std::vector<W3dLoadsItem>& out, std::vector<MeshResource*>* meshes,
                           std::vector<std::string>& nested) {
    // (las rutas se resuelven como al GENERAR: W3dPrefabBuildChildren lee con el formato v4)
    const bool v4 = gProjectV4, refs = g_w3dRefsEntries;
    gProjectV4 = true;
    g_w3dRefsEntries = true;
    ResourcesJsonRec(j, out, meshes, nested);
    gProjectV4 = v4;
    g_w3dRefsEntries = refs;
}

// ============================================================================
//  W3dProjectLoadScene3D — LA ESCENA 3D DEL JUEGO COMPILADO.
//
//  Es la puerta de entrada que usa el runtime (w3drun) y el UNICO codigo propio
//  del juego en todo este archivo: por dentro llama al MISMO OpenSceneJson que
//  usa el editor, asi que el .deb/APK arma exactamente el mismo arbol de escena
//  que el Play — mallas, materiales, animaciones, camaras, curvas/rieles,
//  culling, LOD, visibilidad por celdas, particulas y constraints.
//
//  'datos' son los bytes del project.json que "Compilar juego" dejo al lado del
//  binario. Las rutas de adentro se tratan como NOMBRES DE ENTRADA (el staging
//  del compilador es un espejo exacto del contenedor .w3d), asi que
//  "meshes/pueblo.w3dm" resuelve contra la carpeta del juego sin traducir nada.
//
//  NO hace el pie del editor (layout, sesion, seleccion, auto-play): hace el que
//  necesita un juego — resolver targets, recargar el arbol, ligar los constraints
//  por nombre y dejar las texturas YA cargadas (el juego no puede arrancar con la
//  cola diferida a medias, se veria gris el primer segundo).
// ============================================================================
bool W3dProjectLoadScene3D(const void* data, size_t n) {
    if (!data || n == 0) return false;
    if (!SceneCollection) SceneCollection = new Object(NULL, "Scene");
    // los nombres entran CRUDOS (igual que en el editor): el archivo ya viene con
    // los nombres definitivos del proyecto guardado y las refs de los scripts
    // resuelven contra ESOS.
    W3dNamesLoading = true;
    gPendModArm.clear(); gPendModTgt.clear(); gPendModGen.clear();
    gOldPendCons.clear();
    gDirProject.clear();
    gProjectV4 = true;              // las rutas del json son nombres de entrada
    g_w3dRefsEntries = true;        // ...y las de los .w3dui tambien
    gLoadGame = true;               // arranca en su escena INICIAL (ver W3dRoots.h)
    // ---- DETECCION DE FORMATO, LA MISMA QUE HACE EL EDITOR AL ABRIR ----------
    // El project.json que deja "Compilar juego" es UNA COPIA del .w3d, y un .w3d
    // NO es siempre JSON: el formato TEXTO viejo (`Whisk3D { Escena {...} }`, con
    // comentarios // al principio) sigue abriendo en el editor y sigue siendo el
    // formato de proyectos reales. Aca se parseaba SIEMPRE como JSON, asi que un
    // proyecto de texto compilaba bien, instalaba bien... y el juego abria con la
    // escena 3D VACIA: pantalla NEGRA con el HUD encima. Ahora se mira el primer
    // byte util, igual que OpenW3D (json = '{', texto = 'W'/comentario).
    const char* txt = (const char*)data;
    size_t start = 0;
    while (start < n && (txt[start] == ' ' || txt[start] == '\t' ||
                       txt[start] == '\r' || txt[start] == '\n')) start++;
    bool isJson = (start < n && txt[start] == '{');
    bool ok;
    if (isJson) {
        ok = OpenSceneJson(txt + start, n - start, std::string("."));
        gLoadGame = false;
        // la escena INICIAL no es la del bloque: se arma desde su entrada (el bloque no se armo)
        const int initial = W3dInitialRootIdx();
        if (ok && initial >= 0 && initial != W3dActiveRoot()) W3dRootLoadInActive(initial);
    } else {
        // TEXTO viejo: el MISMO pipeline del editor (Tokenize + ParseNode +
        // BuildScene), sin el Layout -- un juego no tiene viewports.
        std::vector<std::string> tokens = Tokenize(std::string(txt, n));
        size_t it = 0;
        Node* project = ParseNode(tokens, it);
        ok = (project && project->type == "Whisk3D");
        if (!ok) {
            w3dLogfE("[juego] la escena 3D no es JSON ni texto Whisk3D{} (vino '%s')",
                     project ? project->type.c_str() : "(nada)");
        } else {
            Node* scene = Find(project, "Scene");
            if (scene) BuildScene(scene);
            else        w3dLogfW("[juego] el proyecto de texto no trae 'Escena': sin objetos");
        }
        FreeNode(project);
    }
    g_w3dRefsEntries = false;
    gProjectV4 = false;
    gLoadGame  = false;
    W3dNamesLoading = false;
    // MIGRACION del objeto Constraint VIEJO (el nodo suelto que apunta a su
    // objetivo): sus propiedades pasan al objeto apuntado. El editor la corre
    // siempre al abrir; el juego NO la corria, asi que un proyecto con nodos
    // constraint viejos -- todos los .w3d de texto -- perdia sus constraints en
    // el binario. En el demo eso se ve clarito: el domo del CIELO se engancha a
    // la vista con uno de estos, y sin migrar se quedaba plantado en el origen
    // (arrancabas viendo cielo y a los pocos metros el fondo quedaba NEGRO).
    MigrateOldConstraints();
    // targets de los modificadores/objetos, y el arbol recargado
    SearchLoop();
    if (SceneCollection) SceneCollection->ReloadAll();
    // la FUENTE de cada constraint viaja por NOMBRE: se liga con el arbol completo
    // (la pasada vive en el Core justamente porque el juego necesita esto).
    if (SceneCollection) W3dConstraintsResolveNames(SceneCollection);
    // las texturas se encolan DIFERIDAS al importar: el juego las quiere todas
    // decodificadas antes del primer frame.
    LoadAllPendingTextures();
    w3dLogf("[W3D] escena 3D del juego cargada: %d objeto(s) raiz",
            SceneCollection ? (int)SceneCollection->Childrens.size() : 0);
    return ok;
}

#ifndef W3D_NO_EDITOR
// ---------------------------------------------------------------------------
//  REINICIAR la escena (cerrar el proyecto actual) para abrir otro EN CALIENTE
// ---------------------------------------------------------------------------
void RestartScene() {
    // un RENOMBRADO en curso escribe en un nombre del proyecto que se va (un material, una malla): se descarta antes
    // de soltarlo (sino el Enter siguiente escribia en memoria liberada)
    { extern bool RenameActive(); extern void RenameCancel(); if (RenameActive()) RenameCancel(); }
    // parar el juego/simulacion y soltar los scripts
    extern bool SimActive(); extern void SimStop();
    if (SimActive()) SimStop();
    // la TARJETA RENDER es del proyecto: el que viene trae la suya (o los valores de fabrica)
    { g_render = W3dRenderOpt(); g_renderGen++; }
    g_w3dExternalMissing.clear();   // (lo que faltaba es del proyecto de antes)
    // el STREAMING suelta lo que sus instancias retienen del almacen (antes de que el cierre olvide los recursos) y la
    // vista previa vuelve al default del proyecto nuevo
    W3dStreamingCloseProject();
    PlayAnimation = false;
    AnimIsGame = false;
    // la ANIMACION ACTIVA (escena/armature/vertex) apunta a objetos que estamos por
    // BORRAR: soltarla ANTES o queda colgando (ActiveAnimMesh/ActiveAnimArm dangling ->
    // Properties::UpdateTabs crasheaba al abrir un proyecto con una vertex anim
    // activa). Volver a modo ESCENA sin objetivo.
    { extern int ActiveAnimKind; extern Mesh* ActiveAnimMesh; extern Armature* ActiveAnimArm;
      ActiveAnimKind = 0; ActiveAnimMesh = NULL; ActiveAnimArm = NULL;
#ifndef W3D_NO_EDITOR
      { extern bool g_animPickedSceneInGame; g_animPickedSceneInGame = false; }
#endif
    }
    // salir de Edit Mode: abrir un proyecto en Edit Mode dejaba g_editMesh COLGANDO al
    // borrar la escena (mismo crash que ActiveAnimMesh). Volver a Modo Objeto sin malla.
    { extern int InteractionMode; extern Object* g_editMesh;
      InteractionMode = ObjectMode; g_editMesh = NULL; }
    // idem el tick de las mallas como recurso (io/ProjectMeshes.h): su malla en edicion y la
    // activa que se miraba son del arbol que se va a borrar (sin esto el proximo guardado
    // desreferenciaba la malla liberada si ningun viewport 3D corria el tick antes)
    { extern void W3dMeshesEditorForget(); W3dMeshesEditorForget(); }
    // el OUTLINER POR RECURSOS: el recurso activo (y la textura que tomo para su vista previa,
    // ANTES de que se liberen las texturas), las carpetas y lo purgado son del proyecto
    W3dResourcesViewCloseProject();
    { extern void W3dExtLibsCloseProject(); W3dExtLibsCloseProject(); }
    // los paneles de propiedades sueltan el objeto activo (y la malla de su lista de partes): se van
    { extern void PropsForgetScene(); PropsForgetScene(); }
    W3dScriptUnloadAll();
    // el CONTENEDOR del proyecto que se cierra: se suelta su FILE* y su indice
    // (las rutas en memoria que apuntaban a sus entradas mueren con la escena)
    W3dContainerUnmount();
    W3dExternalRefClear();
    g_w3dUINotLoaded.clear();   // idem el freno de las escenas UI que no cargaron
    // el formato de las mallas es DEL PROYECTO: el nuevo arranca en el default (binario);
    // el que abre lo fija con su "formatoMallas". (El forzado del harness no se toca.)
    g_w3dFormatMeshesProject = W3D_MESHES_BINARY;
    // el icono del juego es DEL PROYECTO: al cerrar uno no debe heredarse al
    // siguiente (el que abre lo vuelve a setear si su .w3d lo trae)
    g_projIcon.clear();
    // idem las PALETAS del proyecto: el que abre trae las suyas (del .w3d o
    // adoptadas de sus .w3dui)
    W3dPalettesClear();
    // nada seleccionado ni activo
    DeselectAll();
    ActiveObj = NULL;
    // el undo guarda punteros a objetos que van a morir
    extern void UndoClear();
    UndoClear();
    // Y LAS CURVAS DE OBJETO TAMBIEN (fallo [E] de la ronda 7): una AnimationObject referencia a su objeto POR
    // PUNTERO, y aca se borra la escena entera. Esto limpiaba... nada: ni AnimationObjects ni las listas de las
    // OTRAS animaciones de escena. Al abrir otro proyecto quedaban curvas apuntando a objetos LIBERADOS y el
    // primer cambio de frame las desreferenciaba en ApplyAnimationObjects (ObjectMode.cpp) -> use-after-free;
    // ademas el primer objeto nuevo que cayera en una direccion reciclada nacia ya animado.
    // Se limpian TODAS las escenas, no solo la activa: sus curvas son la misma lista swapeada (Animation.cpp).
    InitSceneAnimations();
    AnimationObjects.clear();
    for (size_t i = 0; i < SceneAnimations.size(); i++)
        if (SceneAnimations[i]) SceneAnimations[i]->objects.clear();
    // idem las BASES del MIX DE ESCENAS (Animation.cpp): la transform de cada objeto que el mix toco,
    // guardada POR PUNTERO. Quedaban apuntando a los objetos borrados y salir del Mix (W3dMixScenesRelease)
    // les escribia encima. Se olvidan sin mover nada: esos objetos se van a borrar.
    W3dMixScenesForget();
    // ningun clip de jerarquia sonando (cada uno retiene su biblioteca: se sueltan antes de borrar los arboles)
    W3dAnimObjectsReset();
    // las OTRAS escenas 3D y los prefabs del proyecto (las raices que no son la activa) se liberan
    // enteras y el registro se vacia: la activa sigue siendo SceneCollection y se vacia abajo
    W3dRootsCloseProject();
    // borrar TODO el arbol, cada hijo del tope CON SU SUBARBOL (W3dFreeSubtree).
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
            W3dFreeSubtree(o);
        }
    CollectionActive = SceneCollection;
    // los FLIPBOOKS CON NOMBRE son del proyecto (y los de sus librerias): el arbol que los usaba ya no existe.
    // Sin esto un proyecto NUEVO (que no pasa por LoadFlipbooksScene) heredaba los del anterior y los guardaba
    SceneFlipbooksClear();

    // el FONDO del render (color, textura 360, rotacion): es del proyecto. Suelta sus texturas ANTES de liberar las de
    // la escena (sus referencias las retenian)
    W3dBackgroundClear();
    // ---- TEXTURAS: liberar de verdad (reporte del dueno) ----------------------
    // Hasta hoy RestartScene() no tocaba `Textures` ni una vez: abrir un
    // proyecto tras otro dejaba TODAS las texturas del anterior en la GPU y en
    // el heap para el resto de la vida del proceso. Va DESPUES de destruir el
    // arbol (ninguna malla puede seguir bindeando lo que se libera) y en este
    // orden:
    //   1. tirar la cola diferida: sus entradas apuntan a materiales del
    //      proyecto que se cierra (y como `Materials` no se poda, el chequeo de
    //      "material vivo" decia que si SIEMPRE y terminaba cargando texturas
    //      de un proyecto muerto encima del nuevo);
    //   2. liberar las texturas no fijadas, dejando en NULL los Texture* de
    //      todos los materiales (TexturesFreeScene lo hace adentro).
    // el PIXELADO es DEL PROYECTO: el que abre trae el suyo (o ninguno)
    w3dEngine::SetPixelatedGlobal(false);
    // PREFABS: la Biblioteca es DEL PROYECTO (y el hook de instanciar con ella)
    W3dPrefabsClear();
    W3dInstantiatePrefabHook = 0;
    // ...y las DEFINICIONES cacheadas de los prefabs del proyecto (io/Prefabs.h)
    W3dPrefabsForgetAll();
    // LISTAS DE CARGA: son del proyecto (las registro Curve::LoadLoadList).
    // Y sus referencias TRANSITORIAS mueren en bloque: cerrar la escena es el
    // "volver al menu" del modelo de streaming (deja las paginas en 0).
    W3dResourcesPurgeTransient();
    W3dLoadsForgetAll();
    // LAS MALLAS CON NOMBRE son del proyecto: el arbol ya no existe (nadie las usa) y las
    // referencias transitorias se acaban de purgar -> se borra el registro entero
    W3dMeshesRegistryClear();
    // "mallasSoloGPU" es DEL PROYECTO: el que abre la fija con su project.json (despues de borrar el registro: apagarla
    // antes releeria cada malla suelta de un contenedor que ya se desmonto)
    W3dMeshesOnlyGPUActivate(false);
    // ANIMSETS: el registro es del proyecto; los recursos ya se soltaron con los armatures
    W3dAnimSetsCloseProject();
    { extern void ForgetPendingTextures(); ForgetPendingTextures(); }
    TexEditClearAll();   // las texturas en edicion (pixeles en memoria) se van con la escena
    // ---- LOS CACHES DEL PROYECTO del lado del juego: sonidos, imagenes 2D y fuentes ----
    // Se llenan en la PRECARGA de OpenW3D (y en el primer uso) con clave = la ruta tal
    // como la escribe el proyecto ("sounds/x.wav", "textures/hud.png" en un contenedor).
    // No se vaciaban nunca: el audio decodificado del proyecto cerrado quedaba en el heap
    // (en un proyecto grande, ~67 MB vivos despues de cerrarlo) y abrir OTRO proyecto con
    // entradas del mismo nombre sonaba/mostraba lo del anterior. El arbol ya no existe:
    // nadie los usa. Van ANTES de TexturesFreeScene (las prestadas son de ese cache).
    { extern void W3dSoundsFree(); W3dSoundsFree(); }
    { extern void Font2DFreeAll(); Font2DFreeAll(); }
    { extern void Texture2DFreeAll(); Texture2DFreeAll(); }
    { const int n = TexturesFreeScene();
      if (n > 0) w3dLogf("[W3D] cierre de proyecto: %d textura(s) liberada(s)", n); }

    // ---- MATERIALES: podarlos de verdad (bug anotado y sin arreglar hasta hoy) --
    // `Materials` es un registro GLOBAL DE PROCESO y aca no se tocaba: abrir un
    // proyecto tras otro dejaba vivos TODOS los materiales del anterior, el ctor
    // de Material los veia como homonimos y uniquificaba, asi que el proyecto
    // reabierto volvia entero con sufijo ".001" (y guardar dejaba el sufijo en el
    // disco). Va DESPUES de destruir el arbol Y DESPUES de TexturesFreeScene,
    // que recorre `Materials` para soltar sus Texture*.
    { const int n = MaterialsFreeScene();
      if (n > 0) w3dLogf("[W3D] cierre de proyecto: %d material(es) liberado(s)", n); }

    // los punteros de viewport activo apuntan al layout VIEJO: soltarlos (el
    // primer render del layout nuevo los re-engancha)
    Viewport3DActive = NULL;
    g_redraw = true;
}

// la apertura REAL (destruye la escena y arma el layout nuevo). Solo debe correr
// con el stack limpio: desde el inicio del frame (main) o desde el modo --script.
void OpenProjectNow(const std::string& path) {
    // BARRA DE PROGRESO: abrir un .w3d bloquea el frame (montar el contenedor, descomprimir,
    // parsear el JSON y construir los objetos). En el N95 eso tarda MUCHO y la pantalla queda
    // congelada -> parece trabado. La barra bombea el render (swap) desde adentro, como el
    // import de modelos: el usuario ve que avanza (y si se traba, en que % quedo).
    extern void ProgressStart(const std::string&); extern void ProgressEnd();
    ProgressStart(T("Opening project..."));
    { extern void HomePopupClose(); HomePopupClose(); }   // (la pantalla de inicio, si seguia abierta)
    RestartScene();
    // EL LAYOUT ANTERIOR SE LIBERA. Antes quedaba huerfano ("el viejo queda"): ~0,2 MB de heap
    // por apertura, fijos (las tarjetas del panel de propiedades, las barras...). El proyecto trae
    // su layout (o va el default): ninguno de sus viewports se reusa. Se toma la raiz COMPLETA
    // (con un viewport maximizado, rootViewport es solo esa hoja) y se libera recien con el layout
    // NUEVO armado: mientras OpenW3D corre, los punteros globales siguen apuntando a viewports
    // vivos, como siempre. Los que apuntan al viejo los sueltan los ganchos de ~ViewportBase
    // (LayoutTree.h); los de "activo" se re-anclan al nuevo.
    ViewportBase* layoutOld = LayoutCompleteRoot();
    rootViewport = NULL;
    OpenW3D(path);
    // el layout nuevo nunca paso por Resize: darle el tamano ACTUAL de la ventana
    // (sin esto los viewports quedan en 0x0 y el editor parece TRABADO)
    extern int MenuScreenW, MenuScreenH;
    if (rootViewport && MenuScreenW > 0 && MenuScreenH > 0)
        rootViewport->Resize(MenuScreenW, MenuScreenH);
    // el arbol es NUEVO: (1) limpiar el estado "maximizado" (el g_savedRoot viejo es parte del arbol
    // que se libera; sino el menu pide Minimizar y minimizar instala el arbol muerto), (2) liberar el
    // viejo y re-anclar los activos al nuevo (el teclado va a viewPortActive: NULL = crash en la primera
    // tecla), y (3) reenganchar gRoot/paneles del layout (sino en Symbian la rotacion redimensiona el
    // arbol viejo y no re-acomoda nada).
    { extern void LayoutResetMaximized(); LayoutResetMaximized();
      if (layoutOld != rootViewport) ViewportDeleteTree(layoutOld);
      LayoutAnchorActive();
      extern void (*LayoutChangedTree)(); if (LayoutChangedTree) LayoutChangedTree(); }
    extern void ProjectSyncFields();
    ProjectSyncFields();
    // el tipo de las escenas sin tipo guardado se deduce AHORA y queda fijo (antes de la foto: no es un cambio)
    { extern void W3dRootsSetTypesDeduced(); W3dRootsSetTypesDeduced(); }
    // lo que se abrio esta GUARDADO: la foto de lo no guardado (los '*', el cartel al cerrar)
    { extern void W3dChangesSnapshot(); W3dChangesSnapshot(); }
    // MODO JUEGO (.sisx bundleado): full-screen del 3D -> el juego se ve solo, sin el chrome
    // del editor. El auto-play (game mode + Play) ya lo hizo OpenW3D si el proyecto tiene scripts.
    { extern bool g_gameMode;   // definido mas abajo en este archivo
      if (g_gameMode) { extern void LayoutMaximize3DForGame(); LayoutMaximize3DForGame(); } }
    ProgressEnd();
    g_redraw = true;
}

// abrir un proyecto DESDE LA TARJETA Archivo: NO se abre aca mismo (el boton que
// disparo esto vive en el layout que vamos a destruir; destruirlo con su stack
// activo TRABABA el editor). Se anota y el main lo abre al inicio del proximo frame.
std::string g_projOpenPending;
// MODO JUEGO: lo prende el arranque (Whisk3D.cpp) cuando el .sisx trae un proyecto bundleado.
// Hace que al abrir el proyecto se full-screene el viewport 3D (el juego se ve solo).
bool g_gameMode = false;
void OpenProjectFrom(const std::string& path) {
    // con algo SIN GUARDAR se pregunta primero ("Se perderan los cambios en:"); el cartel abre el proyecto si el usuario sigue
    { extern bool W3dChangesAsk(int, const std::string&); if (W3dChangesAsk(1 /*W3D_CHANGES_OPEN*/, path)) return; }
    g_projOpenPending = path;
    g_redraw = true;
}

// ===========================================================================
//  NUEVO PROYECTO (tarjeta Archivo y la pantalla de inicio): la misma escena y el mismo layout con los que arranca el
//  editor sin archivo (W3dCreateDefaultScene + LayoutDefault), sin ruta. Como abrir, va DIFERIDO al inicio del
//  proximo frame: el boton que lo pide vive en el layout que se reemplaza. Con algo SIN GUARDAR pregunta primero (el
//  cartel "Se perderan los cambios en:", como abrir otro proyecto): guardar y seguir, seguir sin guardar o cancelar.
// ===========================================================================
bool g_newPendingProj = false;
// el LAYOUT con el que arranca el proyecto nuevo: "" = el de fabrica (LayoutDefault); si no, uno guardado
// (config/W3dLayouts.h). Lo elige la lista de "Nuevo proyecto" (tarjeta Archivo) y la de "Nuevo archivo" (inicio)
static std::string g_newProjLayout;
void NewProjectNow();
// lo pedido por un boton (abrir / nuevo), con el stack limpio: el main al inicio de cada frame (y el harness)
void ProcessPendingProject() {
    if (!g_projOpenPending.empty()) {
        const std::string r = g_projOpenPending;
        g_projOpenPending.clear();
        OpenProjectNow(r);
    }
    if (g_newPendingProj) { g_newPendingProj = false; NewProjectNow(); }
#ifndef W3D_NO_EDITOR
    { extern void LayoutProcessPending(); LayoutProcessPending(); }   // un layout elegido en el menu (LayoutInput.cpp)
#endif
}
static void NewPendingProject() {
    g_newPendingProj = true;
    g_redraw = true;
}
void NewProjectWithLayout(const std::string& layout) {
    extern void (*W3dChangesNewHook)();
    extern bool W3dChangesAsk(int, const std::string&);
    g_newProjLayout = layout;
    W3dChangesNewHook = NewPendingProject;   // (lo que hace el cartel si el usuario sigue)
    if (W3dChangesAsk(2 /*W3D_CHANGES_NEW*/, std::string())) return;
    NewPendingProject();
}
void NewProjectFrom() { NewProjectWithLayout(std::string()); }
void NewProjectNow() {
    { extern void HomePopupClose(); HomePopupClose(); }   // (la pantalla de inicio, si seguia abierta)
    RestartScene();
    ViewportBase* layoutOld = LayoutCompleteRoot();
    rootViewport = NULL;
    w3dPath.clear();
    W3dCompileReset();   // la tarjeta Juego vuelve a los valores de fabrica (abrir los toma del archivo)
    W3dAssetOriginsClear();   // (los origenes de los assets eran del proyecto de antes)
    { extern void W3dCreateDefaultScene(); W3dCreateDefaultScene(); }
    extern int MenuScreenW, MenuScreenH;
    Viewport3D* vp3d = NULL;
    rootViewport = NULL;
#ifndef W3D_NO_EDITOR
    if (!g_newProjLayout.empty()) {   // uno guardado: como elegirlo en el menu Layout (LayoutInput.cpp, LayoutInstall)
        extern std::string W3dLayoutText(const std::string&);
        rootViewport = W3dLayoutFromText(W3dLayoutText(g_newProjLayout));
        if (!rootViewport) { extern void Notify(const std::string&, bool);
                             Notify(std::string(T("Could not load the layout")) + ": " + g_newProjLayout, true); }
        else { extern Viewport3D* LayoutFirst3DOf(ViewportBase*); vp3d = LayoutFirst3DOf(rootViewport); }
    }
#endif
    g_newProjLayout.clear();
    if (!rootViewport) rootViewport = LayoutDefault(MenuScreenW, MenuScreenH, &vp3d);
    if (rootViewport) { rootViewport->x = 0; rootViewport->y = 0; }
    if (rootViewport && MenuScreenW > 0 && MenuScreenH > 0) rootViewport->Resize(MenuScreenW, MenuScreenH);
    // (lo mismo que OpenProjectNow con el arbol nuevo: soltar el viejo y re-anclar los activos)
    { extern void LayoutResetMaximized(); LayoutResetMaximized();
      if (layoutOld != rootViewport) ViewportDeleteTree(layoutOld);
      LayoutAnchorActive();
      viewPortActive = vp3d ? (ViewportBase*)vp3d : rootViewport;   // (un layout sin Vista 3D: el arbol entero)
      extern void (*LayoutChangedTree)(); if (LayoutChangedTree) LayoutChangedTree(); }
    extern void ProjectSyncFields();
    ProjectSyncFields();
    { extern void W3dRootsSetTypesDeduced(); W3dRootsSetTypesDeduced(); }
    { extern void W3dChangesSnapshot(); W3dChangesSnapshot(); }   // recien hecho: nada sin guardar
    g_redraw = true;
}

// ===========================================================================
//  ENTRADA UNICA de lectura: OpenW3D(ruta) detecta el formato POR CONTENIDO
//  y rutea. Los tres formatos convergen en las mismas estructuras y en los
//  mismos aplicadores de campos (una sola fuente de verdad por campo):
//    '{'        -> JSON PLANO nuevo (v3: el .w3d ES el json)
//    'PK'       -> zip v2 viejo (scene.json adentro; se extrae como antes)
//    'Whisk3D'  -> texto viejo (Whisk3D { Escena { ... } Layout { ... } })
//  Cualquier problema (archivo inexistente, json roto, formato desconocido,
//  .w3dui faltante) deja el editor VIVO: aviso claro en el log (visible en el
//  viewport Console) + default sensato. NUNCA crash ni abortar todo.
// ===========================================================================

// nombre de archivo pelado sin extension ("a/b/consola.w3d" -> "consola")
static std::string NameNoExt(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    std::string b = (s == std::string::npos) ? r : r.substr(s + 1);
    size_t p = b.find_last_of('.');
    return (p == std::string::npos) ? b : b.substr(0, p);
}

// ---------------------------------------------------------------------------
//  camino CONTENEDOR v4: el .w3d es un ZIP con project.json adentro.
//  NO SE EXTRAE NADA: se indexa, se MONTA como origen virtual de
//  w3dFileSystem::ReadFileBytes y las rutas del JSON son nombres de entrada.
//  Por eso texturas, fuentes, lua, .w3dui y GLB cargan de adentro sin tocar sus
//  call sites, y el editor nunca trabaja sobre una copia extraida.
//
//  "mimetype" y "README.txt" (las entradas de servicio del estilo OpenDocument)
//  NO se leen NUNCA: son para el que abre el zip con un descompresor. El que
//  manda para decidir si esto es un proyecto es project.json, asi que un .w3d
//  guardado ANTES del mimetype abre igual y se migra en el primer guardado.
// ---------------------------------------------------------------------------
static bool OpenW3DContainer(const std::string& path) {
    extern void ProgressUpdate(float);
    const double tMed = LoadNow();   // medicion: montaje + lectura de project.json
    if (!W3dContainerMount(path)) {
        w3dLogfE("[W3D] no pude montar el contenedor %s (se abre vacio)", path.c_str());
        return false;
    }
    ProgressUpdate(0.12f);   // contenedor montado (indice del zip descomprimido)
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes("project.json", data) || data.empty()) {
        w3dLogfE("[W3D] el contenedor %s no trae project.json legible (se abre vacio)", path.c_str());
        return false;
    }
    ProgressUpdate(0.22f);   // project.json leido y descomprimido
    if (g_w3dLoadClock) g_w3dLoadPhases.mountMs += LoadNow() - tMed;
    gProjectV4 = true;
    g_w3dRefsEntries = true;          // los .w3dui tambien traen nombres de entrada
    g_w3dRefExtMark  = W3dExternalRefMark;
    bool ok = OpenSceneJson((const char*)&data[0], data.size(), std::string());
    g_w3dRefsEntries = false;
    g_w3dRefExtMark  = NULL;
    return ok;
}

// camino ZIP v2 viejo: extraer a userdata (los .glb/.bin referenciados
// necesitan vivir en disco) y abrir el scene.json de adentro.
static bool OpenW3DZip(const std::string& path) {
    std::string base = w3dFileSystem::GetUserDataDir() + "/w3d_abierto/" + NameNoExt(path);
    if (!W3dZipExtract(path, base)) {
        w3dLogfE("[W3D] no pude extraer el zip: %s (se abre vacio)", path.c_str());
        return false;
    }
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(base + "/scene.json", data) || data.empty()) {
        w3dLogfE("[W3D] el zip no trae escena.json: %s (se abre vacio)", path.c_str());
        return false;
    }
    return OpenSceneJson((const char*)&data[0], data.size(), base);
}

// camino TEXTO viejo: Whisk3D { Escena {...} Layout {...} }
static bool OpenW3DText(const std::string& src) {
    double tMed = LoadNow();   // medicion por fase (0 = nadie mide): el parse del texto = "json"
    std::vector<std::string> tokens = Tokenize(src);
    size_t i = 0;
    Node* project = ParseNode(tokens, i);
    if (g_w3dLoadClock) g_w3dLoadPhases.jsonMs += LoadNow() - tMed;
    bool ok = project && project->type == "Whisk3D";
    if (!ok) {
        w3dLogfE("[W3D] se esperaba un root Whisk3D{}, vino '%s' (se abre vacio)",
                 project ? project->type.c_str() : "(nada)");
    } else {
        Node* scene  = Find(project, "Scene");
        Node* layout = Find(project, "Layout");
        if (!scene) w3dLogfW("[W3D] el archivo no trae 'Escena': abre sin objetos");
        tMed = LoadNow();
        if (scene) BuildScene(scene);
        if (g_w3dLoadClock) g_w3dLoadPhases.objectsMs += LoadNow() - tMed;
        if (layout && !layout->children.empty())
            rootViewport = BuildLayout(layout->children[0]);
        else
            w3dLogfW("[W3D] el archivo no trae 'Layout': va el default");
    }
    FreeNode(project);
    return ok;
}


void OpenW3D(const std::string& path) {
    // los nombres entran CRUDOS mientras dura la carga (los .w3d ya guardados TIENEN
    // duplicados) y se reparan todos juntos al final, con el arbol completo y los
    // vinculos por nombre a la vista. Ademas evita el O(n^2) de uniquificar uno a uno.
    W3dNamesLoading = true;
    w3dPath = path;
    gDirProject = DirOfProject();
    g_w3dDirProject = gDirProject;   // base de las rutas "ext:" relativas (UI2DFormato)

    // REDIRECCION de cout/cerr AL LOG, tambien aca (no solo en el ConstructL de Symbian): si Carbide no
    // recompilo Whisk3DAppUi.cpp, el redirect del arranque no quedo instalado y los cout de la carga caian
    // a la TERMINAL blanca del N95. Abrir un proyecto SIEMPRE pasa por este archivo -> el redirect queda
    // puesto ANTES de leer nada, y persiste toda la sesion (juego incluido). Idempotente (solo el rdbuf).
    w3dRedirectCoutToLog();

    // INSTRUMENTACION de carga: resetear los acumuladores por fase y dejar un MARCADOR de build.
    // Si en el log NO aparece esta linea ni el resumen "[CARGA]" del pie, la build que corre es VIEJA
    // (Carbide no recompilo import_wobj.cpp/import_w3d.cpp) -> hay que forzar el recompilado.
    { extern unsigned long g_wobjParseMs, g_wobjBordersMs, g_wobjCount;
      g_wobjParseMs = g_wobjBordersMs = g_wobjCount = 0; }
    s_binaryMeshes = s_derivedMeshes = s_meshesRejectedBin = 0;
    w3dLogf("[BUILD] parser .obj = strtod (rapido), carga instrumentada");
    // el cache 2D pudo PRESTAR ids del cache 3D del proyecto anterior (atlas
    // unico): al abrir otro proyecto esos ids pueden liberarse -> purgarlas
    // ANTES de cargar (se re-prestan solas al primer uso).
    { extern void Texture2DPurgeBorrowed(); Texture2DPurgeBorrowed(); }
    gPendModArm.clear(); // referencias modArmature de una carga anterior (por si quedo algo)
    gPendModTgt.clear(); // idem targets de modificadores y mallas a regenerar
    gPendModGen.clear();
    gOldPendCons.clear(); // idem los nodos del objeto Constraint VIEJO (se migran al final)

    // campos DEL PROYECTO: arrancan limpios (el archivo los setea si los trae;
    // sin esto un proyecto sin escenaInicial heredaba la del proyecto anterior)
    ApplyInitialScene(std::string());
    ApplySceneMode(false);
    // idem la config de la tarjeta Juego: un .w3d sin el bloque "compilar" abre
    // con los defaults (no hereda el modo ventana del proyecto anterior)
    W3dCompileReset();
    // idem el estado de reproduccion guardado: ausente = -1 (auto-play de siempre)
    g_projPlaying = -1;
    // idem la SESION (frame + seleccion). El frame vuelve al 1: un .w3d sin el bloque
    // (los viejos) abre en el default del editor y NO se queda con el playhead donde lo
    // habia dejado el proyecto anterior. Si el archivo trae "sesion", SessionApply lo
    // pisa en el pie. La seleccion ya la limpia el DeselectAll del pie.
    SessionReset();
    CurrentFrame = 1;
    // idem el FPS: ApplyFps ignora un "fps" ausente/0, asi que sin este reset
    // un proyecto sin el campo HEREDABA el fps del proyecto anterior
    AnimFPS = 30;
    g_fpsCap = 60;   // idem el tope de render: sin el campo fpsCap se vuelve al default de plataforma

    // el proyecto ANTERIOR deja de estar montado (su FILE* se suelta) y sus refs
    // externas dejan de valer: las del que abre las trae SU archivo
    W3dContainerUnmount();
    W3dExternalRefClear();
    g_w3dUINotLoaded.clear();   // el freno de las escenas UI es DEL proyecto que se abre, no del anterior
    gProjectV4 = false;

    std::vector<unsigned char> data;
    // un ZIP no se levanta entero solo para detectarlo: W3dZipIs mira 4 bytes y
    // el contenedor despues se indexa por el directorio central (nunca completo)
    const double tMed = LoadNow();   // medicion: detectar + leer el archivo (el zip se mide al montar)
    bool isZip = W3dZipIs(path);
    bool read  = isZip || (w3dFileSystem::ReadFileBytes(path, data) && !data.empty());
    if (g_w3dLoadClock) g_w3dLoadPhases.mountMs += LoadNow() - tMed;
    const char* format = "unknown";
    bool ok = false;

    if (isZip) {
        if (W3dContainerIs(path)) {
            format = "contenedor v4";
            ok = OpenW3DContainer(path);
        } else {
            format = "zip v2";
            ok = OpenW3DZip(path);
        }
    } else if (!read) {
        w3dLogfE("[W3D] no pude leer %s (no existe o esta vacio): se abre vacio", path.c_str());
    } else {
        // deteccion POR CONTENIDO (no por extension): primer byte util
        size_t i = 0;
        while (i < data.size() && (data[i] == ' ' || data[i] == '\t' ||
                                    data[i] == '\r' || data[i] == '\n')) i++;
        bool isText = (data.size() - i >= 7 && memcmp(&data[i], "Whisk3D", 7) == 0) ||
                       (i < data.size() && (data[i] == '/' || data[i] == '#'));   // comentario inicial
        if (i < data.size() && data[i] == '{') {
            format = "json";
            ok = OpenSceneJson((const char*)&data[0] + i, data.size() - i, gDirProject);
        } else if (isText) {
            format = "texto viejo";
            ok = OpenW3DText(std::string((const char*)&data[0], data.size()));
        } else {
            w3dLogfE("[W3D] %s no parece un .w3d (ni JSON, ni zip, ni texto Whisk3D): se abre vacio",
                     path.c_str());
        }
    }

    // ---- pie COMUN a los tres formatos (tambien si algo fallo: el editor sigue) ----
    if (!CollectionActive) CollectionActive = SceneCollection;
    bool layoutOfProject = (rootViewport != NULL);
    if (!rootViewport) {
        // fallback: el .w3d no trae bloque Layout -> EL layout por defecto (por tamano de pantalla,
        // el MISMO que el arranque sin archivo). MenuScreenW/H estan seteados en PC y en Symbian.
        extern int MenuScreenW, MenuScreenH;
        rootViewport = LayoutDefault(MenuScreenW, MenuScreenH, NULL);
    }

    // nada seleccionado al abrir (los constructores van seleccionando lo
    // ultimo creado; un proyecto recien abierto arranca LIMPIO)
    DeselectAll();
    ActiveObj = NULL;

    // una raiz de tipo JUEGO (la del bloque: el registro lo dice, o en un proyecto viejo sale de su
    // contenido -tiene scripts-, ver W3dRoots.h) arranca con el selector del timeline en "Juego" y
    // en PLAY (asi el doble click abre jugando y las vertex animations corren de una). PERO si el
    // .w3d v3 trae "reproduciendo": false (se GUARDO en pausa), se respeta y abre en pausa.
    // Ausente (-1, archivos viejos) = auto-play de siempre. Una ESCENA abre en su animacion.
    if (W3dRootActiveType() == W3D_ROOT_GAME) {
        W3dRootModeByType(true);
        PlayAnimation = (g_projPlaying != 0);
        g_redraw = true;
    }
    // LA RED (config/W3dNetPermissions.h): si sus scripts juegan en red, lo decidido para este archivo o la pregunta
    // (una sola vez; si abria jugando, arranca al responder)
    { extern void NetOnOpenProject(const std::string&); NetOnOpenProject(path); }

    W3dNamesLoading = false;

    // LAS MALLAS QUE ABRIERON SUELTAS (sin recurso del registro: una "geometria" por objeto, un
    // archivo viejo) se juntan por contenido al guardar; las nuevas de la sesion no
    // (Mesh::dedupByContent, io/ProjectMeshes.h)
    { extern void W3dMeshesMarkFileLoose(); W3dMeshesMarkFileLoose(); }
    // TODA MALLA ES UN RECURSO: las que abrieron sueltas (un archivo viejo) tienen el suyo YA, no recien al
    // guardar (las iguales comparten uno: mil arboles -> una malla)
    { extern int W3dMeshesEnsureResources(); W3dMeshesEnsureResources(); }

    // MIGRACION del objeto Constraint viejo: sus propiedades pasan al objeto que APUNTABA (la
    // inversion de semantica) y el nodo desaparece si no tiene hijos. Va PRIMERA, antes de la
    // reparacion de nombres, porque los nodos que se van no tienen que contar como duplicados
    // de un objeto real: si contaran, la que se lleva el ".001" (para siempre) puede ser la
    // malla del usuario. El motivo completo esta arriba, en el bloque de MigrateOldConstraints.
    MigrateOldConstraints();

    // NOMBRES UNICOS: reparar los duplicados que venian en el archivo. Va ANTES del
    // ReloadAll (que resuelve targetName/RailName por nombre) para que los punteros se
    // resuelvan contra los nombres YA definitivos. Avisa de forma visible: renombrar
    // puede romper scripts lua que buscan por nombre.
    W3dNamesRepairScene(true);

    // recargar los targets de todos los objetos modificadores
    SearchLoop();
    if (SceneCollection) SceneCollection->ReloadAll();

    // ...y la FUENTE de cada constraint, que tambien viaja por nombre. Va DESPUES de
    // W3dNamesRepairScene (los nombres recien ahi son los definitivos: resolver antes
    // ligaria contra un nombre que la reparacion todavia va a mover) y despues del
    // ReloadAll, o sea en el mismo lugar y por el mismo motivo que targetName / RailName.
    // La pasada vive en el CORE (Objects.cpp) porque el runtime de un juego carga el mismo
    // archivo y necesita exactamente esto.
    // (los constraints que acaba de crear la migracion NO dependen de esta pasada: su fuente
    // es la VISTA, sin nombre que resolver. Por eso puede ir antes y esta no la alcanza.)
    if (SceneCollection) W3dConstraintsResolveNames(SceneCollection);

    // la RAIZ que el usuario estaba editando (otra escena 3D o un prefab): se abre (carga
    // perezosa) ANTES de la seleccion, que es de esa raiz
    if (!g_sessionRoot.empty()) {
        const int r = W3dRootFind(g_sessionRootType, g_sessionRoot);
        std::string reason;
        extern bool W3dActivateRoot(int idx, std::string* reason);
        if (r < 0) w3dLogfW("[W3D] sesion: la raiz '%s' ya no esta en el proyecto", g_sessionRoot.c_str());
        else if (!W3dActivateRoot(r, &reason))
            w3dLogfW("[W3D] sesion: no pude abrir '%s' (%s)", g_sessionRoot.c_str(), reason.c_str());
        // (el cambio de raiz para la reproduccion: un JUEGO reabierto arranca como el del bloque)
        else if (W3dRootActiveType() == W3D_ROOT_GAME) PlayAnimation = (g_projPlaying != 0);
    }

    // LAS ESCENAS QUE QUEDARON ABIERTAS (varias a la vez en el editor): se cargan ahora, con la activa ya puesta
    W3dOpenRootsLoad();

    // SESION: el frame y la seleccion que el usuario tenia en ESTE proyecto. Va DESPUES
    // de W3dNamesRepairScene (los nombres recien ahi son los definitivos) y despues
    // del DeselectAll de arriba, que es el que deja limpio al que no trae bloque.
    SessionApply();

    // RESUMEN corto de la apertura (queda en el log; se ve en el viewport Console)
    if (ok) {
        int uis = 0;
        int objs = SceneCollection ? (int)SceneCollection->Childrens.size() : 0;
        if (SceneCollection)
            for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
                if (SceneCollection->Childrens[i]->getType() == ObjectType::ui) uis++;
        w3dLogf("[W3D] abierto %s (%s): %d objeto(s), %d escena(s) UI, escenaInicial '%s', "
                "layout %s, icono '%s'",
                path.c_str(), format, objs, uis, W3dInitialScene().c_str(),
                layoutOfProject ? "del proyecto" : "default", g_projIcon.c_str());
        w3dLogMemory("proyecto abierto");
    }

    // REPARTO del tiempo de carga por fase (para saber que optimizar en el N95): parse+build de mallas
    // vs ComputeBorders (estructura de EDICION). La PRESENCIA de esta linea prueba que corre la build nueva.
    { extern unsigned long g_wobjParseMs, g_wobjBordersMs, g_wobjCount;
      w3dLogf("[CARGA] mallas .obj=%lu  parse+build=%lu ms  bordes(edicion)=%lu ms",
              g_wobjCount, g_wobjParseMs, g_wobjBordersMs); }
    // idem las mallas del contenedor: .w3db (listas para memoria, sin derivar) contra .w3dm de texto
    // (derivadas al abrir), y los .w3db que este build no puede usar (en el N95: mas de 65535 vertices)
    w3dLogf("[CARGA] mallas .w3db=%d (listas, sin derivar)  .w3dm=%d (derivadas al abrir)  .w3db rechazadas=%d",
            s_binaryMeshes, s_derivedMeshes, s_meshesRejectedBin);

    // PRECARGA de texturas 2D (emisores de particulas + imagenes del HUD): sin
    // esto cada PNG se decodificaba en su PRIMER uso, EN PLENO GAMEPLAY (la
    // primera salpicadura, el primer pickup). Ahora el decode va aca, a la
    // carga del proyecto (queja del dueno: "horrible que se ponga a decodificar
    // imagenes durante el gameplay").
    if (SceneCollection) {
        extern void W3dParticlesPreloadTextures(Object*);
        extern void UI2D_PreloadTextures(Object*);
        extern void W3dSoundsPreload();
        extern void LoadPendingTextures();
        extern double W3dNowMs();
        double _pre0 = W3dNowMs();
        const double tMedPre = LoadNow();   // medicion por fase (0 = nadie mide)
        // PRIMERO el 3D (la cola diferida sube el atlas unico): asi el puente
        // 2D->3D de Texture2DGet encuentra el id y el HUD/particulas/fuente
        // COMPARTEN esa textura en vez de decodificar una copia propia.
        LoadPendingTextures();
        W3dParticlesPreloadTextures(SceneCollection);
        UI2D_PreloadTextures(SceneCollection);
        W3dSoundsPreload();
        w3dLogf("[CARGA] precarga de texturas (3D + 2D) + WAVs: %d ms",
                (int)(W3dNowMs() - _pre0));
        if (g_w3dLoadClock) g_w3dLoadPhases.preloadMs += LoadNow() - tMedPre;
    }
}

// compat: los llamadores viejos (constructor con w3dPath ya seteado) siguen andando
void OpenW3D() { OpenW3D(w3dPath); }
#endif // W3D_NO_EDITOR (abrir un proyecto EN CALIENTE: es cosa del editor)
static void W3dPostRead(JVal* j, W3dPostOptions& o) {
    o = W3dPostOptions();
    o.on = JB(j, "on", true);
    o.active = JI(j, "selected", -1);
    JVal* l = JChild(j, "effects", 5);
    if (l) for (size_t i = 0; i < l->list.size(); i++) {
        JVal* je = l->list[i];
        if (!je || je->type != 4) continue;
        const int type = JI(je, "type", -1);
        if (type < 0 || type >= W3dPostTypes) continue;   // (un efecto de una version mas nueva: se saltea)
        W3dPostEffect e; W3dPostDefaults(e, type);
        e.active = JB(je, "active", true); e.opt = JB(je, "opt", e.opt);
        JVal* v = JChild(je, "v", 5);
        if (v) for (size_t k = 0; k < 8 && k < JRowLen(v); k++) e.v[k] = JRowNum(v, k, e.v[k]);
        JVal* col = JChild(je, "col", 5);
        if (col) for (size_t k = 0; k < 3 && k < JRowLen(col); k++) e.col[k] = JRowNum(col, k, e.col[k]);
        o.effects.push_back(e);
    }
    if (o.active >= (int)o.effects.size()) o.active = (int)o.effects.size() - 1;
    if (o.active < 0 && !o.effects.empty()) o.active = 0;
}
