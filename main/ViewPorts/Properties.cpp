#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "Properties.h"
#include "Undo.h" // Ctrl+Z: capturar rename
#include "edit/MeshEdit.h" // Nuevo/Borrar/MoverMeshPart (funciones libres del editor)
#include "WhiskUI/glesdraw.h"
#include "WhiskUI/PopupMenu.h"
#include "PopUp/ColorPicker.h"
#include "PopUp/FileBrowser.h" // explorador para elegir la carpeta de export
#include "PopUp/ProgressPopup.h" // barra "Rendering..." durante el render (clave en N95)
#include "ViewPorts/LayoutInput.h" // Notificar (toasts de exito/error)
#include "objects/Camera.h"   // selector de target de la camara
#include "objects/Instance.h" // selector de target de instance/array/mirror
#include "edit/Modifier.h"    // ModifierType (ids del menu Add del stack de modificadores)
#include "importers/import_obj.h" // ExportOBJ (boton Wavefront.obj de la tarjeta Export)
#include "render/OpcionesRender.h" // g_redraw (scroll de la lista con la rueda)
#include "ViewPorts/ViewPort3D.h"  // Viewport3D::RenderAPNG + Viewport3DActive (render a PNG)
#include <cstdio>
#include <string>
#ifdef W3D_SYMBIAN
extern int W3dPantallaAlto; // flip de Y (glesdraw.cpp)
#endif

Properties* PropsActivo = NULL;

void DibujarTitulo(Object* obj, int maxPixels){
    w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::blanco)][0], ListaColores[static_cast<int>(ColorID::blanco)][1],
              ListaColores[static_cast<int>(ColorID::blanco)][2], 1.0f);

    //icono de la coleccion
    W3dDrawStrip4(IconMesh, IconsUV[IconoDeObjeto(obj)]->uvs);

    //texto render
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    RenderBitmapText(obj->name, textAlign::left, maxPixels);
    w3dEngine::PopMatrix();
    w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
}

void RebindMaterialMeshPart(); // (definida mas abajo)

// nombre clasico del programa: Material, Material.001, Material.002...
static std::string NombreMaterialLibre(){
    if (!BuscarMaterialPorNombre("Material")) return "Material";
    char buf[32];
    for (int n = 1; n < 1000; n++){
        sprintf(buf, "Material.%03d", n);
        if (!BuscarMaterialPorNombre(buf)) return std::string(buf);
    }
    return "Material.999";
}

// el desplegable del selector de materiales (se reconstruye al abrir)
static PopupMenu* MenuMateriales = NULL;

// opcion elegida: 0 = New Material, 1 = Default Material, 2+ = existentes
static void AccionMaterialElegido(int id){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    if (mesh->materialsGroup.empty()) return;
    PropListMeshParts* lista =
        static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;

    UndoCapturarMaterial(mesh, idx); // Ctrl+Z: guarda el Material* previo del mesh part

    if (id == 0) {
        // nombre clasico del programa: Material, Material.001, ...
        mesh->materialsGroup[idx].material = new Material(NombreMaterialLibre());
    } else if (id == 1) {
        // el material por defecto (fuera de la lista global Materials:
        // no aparece entre los "existentes")
        if (!MaterialDefecto) MaterialDefecto = new Material("Default Material", true);
        mesh->materialsGroup[idx].material = MaterialDefecto;
    } else if (id >= 2 && id - 2 < (int)Materials.size()) {
        mesh->materialsGroup[idx].material = Materials[id - 2];
    }
    RebindMaterialMeshPart();
}

// ===== Mesh Parts: New / Assign / Select / Deselect / Delete (botones de la tarjeta) =====
extern bool LayoutToggleEditMode(); // LayoutInput.cpp

// el contenido cambio (material/modo/rename distinto): recalcular la tarjeta y el scroll el proximo frame
static bool PropertiesLayoutDirty = false;

static Mesh* MaterialMesh(){
    return (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? static_cast<Mesh*>(ObjActivo) : NULL;
}
static int MeshPartActivoIdx(Mesh* m){
    if (!PropsActivo || !m || m->materialsGroup.empty()) return -1;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)m->materialsGroup.size()) idx = 0;
    return idx;
}
// material que muestra el panel (el del mesh part activo) -> para el Ctrl+Z de modificacion de material
static Material* MaterialActivoUI(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m);
    if (!m || idx < 0 || idx >= (int)m->materialsGroup.size()) return NULL;
    return m->materialsGroup[idx].material;
}
static void SelEnListaMeshPart(int idx){
    if (!PropsActivo) return;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    lista->selectIndex = idx; lista->AjustarVentana();
}
// el selector del stack de modificadores SIGUE al modificador activo (tras add/remove/move -> no se pierde la
// seleccion visual: el modificador movido queda resaltado, como pidio Dante).
static void SelEnListaModificador(){
    if (!PropsActivo || !PropsActivo->propListModifiers) return;
    Mesh* m = MaterialMesh(); if (!m) return;
    PropsActivo->propListModifiers->selectIndex = m->modificadorActivo;
    PropsActivo->propListModifiers->AjustarVentana();
    m->GenerarMallaModificada(); // el stack cambio (add/remove/move) -> regenerar la malla generada
    g_redraw = true;
}
static void AccionNuevoMeshPart(){
    Mesh* m = MaterialMesh(); if (!m) return;
    UndoCapturarMallaGeo(m); // Ctrl+Z: snapshot pre-nuevo-mesh-part (materialsGroup)
    SelEnListaMeshPart(NuevoMeshPart(m)); // crea vacio + lo deja activo
    RebindMaterialMeshPart(); g_redraw = true;
}
static void AccionBorrarMeshPart(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0) return;
    UndoCapturarMallaGeo(m); // Ctrl+Z: snapshot pre-borrar-mesh-part (faces3d.mat + materialsGroup)
    BorrarMeshPart(m, idx); // huerfanas -> anterior; siempre queda >=1
    int n = (int)m->materialsGroup.size();
    SelEnListaMeshPart(idx >= n ? n - 1 : idx);
    RebindMaterialMeshPart(); g_redraw = true;
}
static void AccionAssignMeshPart(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0) return;
    m->AsignarFacesAMeshPart(idx); // las caras seleccionadas (edit) pasan a este mesh part
    g_redraw = true;
}
static void AccionSelectMeshPart(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0) return;
    if (InteractionMode != EditMode) LayoutToggleEditMode(); // entrar a Edit para VER la seleccion
    UndoCapturarSeleccionEdit(m); // Ctrl+Z: seleccionar las caras del mesh part cambia la seleccion edit
    m->SeleccionarMeshPart(idx, true);
    g_redraw = true;
}
static void AccionDeselectMeshPart(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0) return;
    UndoCapturarSeleccionEdit(m); // Ctrl+Z: deseleccionar las caras del mesh part cambia la seleccion edit
    m->SeleccionarMeshPart(idx, false);
    g_redraw = true;
}
// reordenar el mesh part activo (el ORDEN = orden de dibujado: solidos primero, transparentes al final).
static void AccionMeshPartUp(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx <= 0) return; // el primero no sube
    UndoCapturarMallaGeo(m); // Ctrl+Z: reordenar toca faces3d.mat + materialsGroup
    MoverMeshPart(m, idx, -1);
    SelEnListaMeshPart(idx - 1); // el mesh part MOVIDO queda seleccionado
    RebindMaterialMeshPart(); g_redraw = true;
}
static void AccionMeshPartDown(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m);
    if (idx < 0 || idx >= (int)m->materialsGroup.size() - 1) return; // el ultimo no baja
    UndoCapturarMallaGeo(m);
    MoverMeshPart(m, idx, +1);
    SelEnListaMeshPart(idx + 1);
    RebindMaterialMeshPart(); g_redraw = true;
}

// ===== nombres UNICOS (no se pueden duplicar): devuelve 'n', o n.001/.002... evitando 'excl' (el propio
// nombre, para que renombrar al mismo valor no choque). Cada scope junta los punteros a sus nombres. =====
static std::string UniqueNombre(const std::string& n, std::string* excl, const std::vector<std::string*>& nombres){
    std::string cand = n; int suf = 0;
    for (;;){
        bool choca = false;
        for (size_t i = 0; i < nombres.size(); i++)
            if (nombres[i] != excl && *nombres[i] == cand){ choca = true; break; }
        if (!choca) return cand;
        ++suf; char b[16]; sprintf(b, ".%03d", suf); cand = n + b;
    }
}
static std::string UniqMaterial(const std::string& n, std::string* excl){
    std::vector<std::string*> v; for (size_t i = 0; i < Materials.size(); i++) v.push_back(&Materials[i]->name);
    return UniqueNombre(n, excl, v);
}
static std::string UniqUVMap(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->uvMaps.size(); i++) v.push_back(&m->uvMaps[i]->nombre); }
    return UniqueNombre(n, excl, v);
}
static std::string UniqColor(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)ObjActivo;
        for (size_t i = 0; i < m->colorLayers.size(); i++) v.push_back(&m->colorLayers[i]->nombre); }
    return UniqueNombre(n, excl, v);
}
static void RecolectarNombresObj(Object* o, std::vector<std::string*>& v){
    v.push_back(&o->name);
    for (size_t i = 0; i < o->Childrens.size(); i++) RecolectarNombresObj(o->Childrens[i], v);
}
static std::string UniqObjeto(const std::string& n, std::string* excl){
    std::vector<std::string*> v;
    if (SceneCollection) for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
        RecolectarNombresObj(SceneCollection->Childrens[i], v);
    return UniqueNombre(n, excl, v);
}

// ===== Rename: el BOTON se vuelve un INPUT in-place (no un campo abajo). Al ACEPTAR escribe el nombre
// (uniquificado segun el scope); cancelar lo descarta. El input lo rutea controles.cpp a g_textFieldActivo. =====
static std::string* g_renameTarget = NULL; // NULL = no hay rename en curso
static TextField    g_renameField;         // el texto que se edita (se dibuja DENTRO del boton)
static Button*      g_renameBoton = NULL;  // el boton que se volvio input
static std::string (*g_renameUniq)(const std::string&, std::string*) = NULL; // uniquificador del scope (o NULL)

bool RenameActivo(){ return g_renameTarget != NULL; }

static void RenameLimpiar(){
    if (g_renameBoton) g_renameBoton->editField = NULL; // el boton vuelve a ser boton
    g_renameTarget = NULL;
    g_renameBoton = NULL;
    g_renameUniq = NULL;
    g_textFieldActivo = NULL;
}
void RenameCommit(){ // ACEPTAR: escribe el texto (uniquificado) en el nombre destino
    if (g_renameTarget) {
        UndoCapturarRename(g_renameTarget); // Ctrl+Z: guarda el nombre PREVIO antes de escribir
        *g_renameTarget = g_renameUniq ? g_renameUniq(g_renameField.text, g_renameTarget) : g_renameField.text;
    }
    RenameLimpiar();
    RebindMaterialMeshPart(); // refresca el texto mostrado (boton de material / lista de parts)
    g_redraw = true;
}
void RenameCancel(){ RenameLimpiar(); g_redraw = true; } // CANCELAR: no escribe nada

// 'boton' se vuelve input (con TODO seleccionado). 'uniq' = uniquificador del scope (NULL = sin chequeo).
static void RenameIniciar(Button* boton, std::string* destino, std::string (*uniq)(const std::string&, std::string*) = NULL){
    if (!boton || !destino) return;
    g_renameTarget = destino;
    g_renameBoton = boton;
    g_renameUniq = uniq;
    g_renameField.SetText(*destino);
    g_renameField.SelectAll();   // TODO seleccionado: la 1ra tecla reemplaza
    boton->editField = &g_renameField; // el boton se dibuja como input
    g_textFieldActivo = &g_renameField;
}
static void AccionRenameMeshPart(){ // las PARTES si pueden repetir nombre (no uniquifica)
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0 || !PropsActivo) return;
    if (!PropsActivo->propRowDelRen || PropsActivo->propRowDelRen->botones.size() < 2) return;
    RenameIniciar(PropsActivo->propRowDelRen->botones[1], &m->materialsGroup[idx].name); // [1] = Rename
}
static void AccionRenameMaterial(){
    Mesh* m = MaterialMesh(); int idx = MeshPartActivoIdx(m); if (idx < 0 || !PropsActivo) return;
    Material* mat = m->materialsGroup[idx].material;
    if (!mat || mat == MaterialDefecto) return; // el material POR DEFECTO no se renombra (es global)
    if (!PropsActivo->propBtnRenameMat) return;
    RenameIniciar(PropsActivo->propBtnRenameMat->button, &mat->name, UniqMaterial); // GLOBAL unico
}
static void AccionRenameUVMap(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || !PropsActivo || !PropsActivo->propBtnRenameUV) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->uvMapActivo < 0 || m->uvMapActivo >= (int)m->uvMaps.size()) return;
    RenameIniciar(PropsActivo->propBtnRenameUV->button, &m->uvMaps[m->uvMapActivo]->nombre, UniqUVMap);
}
static void AccionRenameColor(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh || !PropsActivo || !PropsActivo->propBtnRenameColor) return;
    Mesh* m = (Mesh*)ObjActivo;
    if (m->colorActivo < 0 || m->colorActivo >= (int)m->colorLayers.size()) return;
    RenameIniciar(PropsActivo->propBtnRenameColor->button, &m->colorLayers[m->colorActivo]->nombre, UniqColor);
}
static void AccionRenameObjeto(){
    if (!ObjActivo || !PropsActivo || !PropsActivo->propBtnRenameObj) return;
    RenameIniciar(PropsActivo->propBtnRenameObj->button, &ObjActivo->name, UniqObjeto); // todos los objetos
}

// nombre corto de una textura (el archivo, sin la ruta)
static std::string NombreDeTextura(Texture* t){
    if (!t) return std::string("No Texture");
    std::string n = t->path;
    size_t pos = n.find_last_of("/\\");
    if (pos != std::string::npos) n = n.substr(pos + 1);
    return n.empty() ? std::string("Texture") : n;
}

// el desplegable del selector de texturas del mesh part
static PopupMenu* MenuTexturas = NULL;
static PopupMenu* MenuTexturasNormal = NULL; // selector de la textura de NORMAL MAP (mat->normalTexture)

// "Load Texture": cada plataforma lo cablea (PC: abre el browser compartido en
// main.cpp). Carga la imagen y la asigna a 'mat' (async).
void (*DialogoCargarTextura)(Material* mat) = NULL;
// "Load Texture" del normal map: usa el MISMO DialogoCargarTextura (browser COMPARTIDO, anda en los 4 OS) pero
// con este flag prendido -> el callback de carga de cada plataforma asigna a mat->normalTexture en vez de
// mat->texture. (Antes habia un DialogoCargarNormalMap aparte SOLO cableado en PC -> en Symbian no abria nada.)
bool gCargarTexturaComoNormal = false;

// 0 = No Texture; 1 = Load Texture (dialogo); 2+ = Textures[5 + id - 2]
// (las primeras 5 son de la UI: font/origen/cursor/linea/lampara)
static void AccionTexturaElegida(int id){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    if (mesh->materialsGroup.empty()) return;
    PropListMeshParts* lista =
        static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;
    Material* mat = mesh->materialsGroup[idx].material;
    if (!mat) return;

    if (id == 0) {
        mat->texture = NULL; // No Texture
    } else if (id == 1) {
        // Load Texture: el browser COMPARTIDO carga la imagen y la asigna a
        // 'mat' (async: el rebind lo hace el callback al elegir el archivo)
        if (DialogoCargarTextura) { gCargarTexturaComoNormal = false; DialogoCargarTextura(mat); return; }
    } else if (5 + id - 2 < (int)Textures.size()) {
        mat->texture = Textures[5 + id - 2];
        mat->textureOn = true;
    }
    RebindMaterialMeshPart();
}

// ESTANDAR de los desplegables de Properties: abre 'menu' JUSTO debajo de 'boton', tocando su borde inferior
// (sin gap; el borde superior del menu se funde con el del boton, como los menus de la barra). Un solo lugar ->
// todos los dropdowns quedan iguales y bien pegados (antes cada accion lo calculaba a mano con un gap de mas).
static void AbrirMenuBajoBoton(PopupMenu* menu, Button* boton){
    if (!menu || !boton) return;
    menu->Abrir(boton->sx, boton->sy + boton->height - GlobalScale, MenuPantallaW, MenuPantallaH);
    MenuAbierto = menu;
}

static void AccionMenuTexturas(){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    if (!MenuTexturas) {
        MenuTexturas = new PopupMenu();
        MenuTexturas->action = AccionTexturaElegida;
    }
    MenuTexturas->Limpiar(); // las texturas cargadas van cambiando
    MenuTexturas->Agregar("No Texture", 0, IconType::notifError); // la "cruz" de error = sin textura
    MenuTexturas->Agregar("Load Texture", 1, IconType::archive);
    for (size_t i = 5; i < Textures.size(); i++) {
        MenuTexturas->Agregar(NombreDeTextura(Textures[i]),
                              2 + (int)(i - 5), IconType::textura);
    }
    AbrirMenuBajoBoton(MenuTexturas, PropsActivo->propBtnTextura->button);
}

// === NORMAL MAP: selector de textura (mirror del de la textura base, pero -> mat->normalTexture) ===
static void AccionNormalTexElegida(int id){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    if (mesh->materialsGroup.empty()) return;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;
    Material* mat = mesh->materialsGroup[idx].material;
    if (!mat) return;
    if (id == 0) { mat->normalTexture = NULL; }                                   // sin normal map
    else if (id == 1) { if (DialogoCargarTextura) { gCargarTexturaComoNormal = true; DialogoCargarTextura(mat); return; } } // cargar archivo (MISMO browser que la textura base, flag -> normalTexture)
    else if (5 + id - 2 < (int)Textures.size()) { mat->normalTexture = Textures[5 + id - 2]; } // una ya cargada
    RebindMaterialMeshPart();
}

static void AccionMenuTexturasNormal(){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    if (!MenuTexturasNormal) {
        MenuTexturasNormal = new PopupMenu();
        MenuTexturasNormal->action = AccionNormalTexElegida;
    }
    MenuTexturasNormal->Limpiar();
    MenuTexturasNormal->Agregar("No Normal Map", 0, IconType::notifError);
    MenuTexturasNormal->Agregar("Load Texture", 1, IconType::archive);
    for (size_t i = 5; i < Textures.size(); i++) {
        MenuTexturasNormal->Agregar(NombreDeTextura(Textures[i]), 2 + (int)(i - 5), IconType::textura);
    }
    AbrirMenuBajoBoton(MenuTexturasNormal, PropsActivo->propBtnNormalTex->button);
}

// === REFLECTION: el MODO del reflejo (Matcap HW / Sphere Map / Equirect) en un desplegable (reemplaza el viejo
// checkbox "Chrome 360"). Los tags (hardware)/(software) son para el N95 (donde importa el perf): el Matcap es por
// matriz de textura = HW en los 4 OS; el Sphere Map exacto es HW en PC (texgen) pero SW en el N95; el Equirect es SW.
static PopupMenu* MenuReflectMode = NULL;
static const char* NombreReflectMode(int m){
    return (m == 1) ? "Sphere Map (software)"
         : (m == 2) ? "Equirectangular (software)"
         :            "Matcap (hardware)";
}
static void AccionReflectModeElegido(int id){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    if (mesh->materialsGroup.empty()) return;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;
    Material* mat = mesh->materialsGroup[idx].material;
    if (!mat) return;
    if (id >= 0 && id <= 2) mat->reflectMode = id;
    RebindMaterialMeshPart();
}
static void AccionMenuReflectMode(){
    if (!PropsActivo) return;
    if (!MenuReflectMode) { MenuReflectMode = new PopupMenu(); MenuReflectMode->action = AccionReflectModeElegido; }
    MenuReflectMode->Limpiar();
    MenuReflectMode->Agregar(NombreReflectMode(0), 0, IconType::material);
    MenuReflectMode->Agregar(NombreReflectMode(1), 1, IconType::material);
    MenuReflectMode->Agregar(NombreReflectMode(2), 2, IconType::material);
    AbrirMenuBajoBoton(MenuReflectMode, PropsActivo->propBtnReflectMode->button);
}

// GL Light de la luz activa: el PropFloat edita un espejo float y al cambiar reasigna el LightID (0..7).
static float g_lightGLIdx = 0.0f;
static void OnLightGLChange(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::light) return;
    int idx = (int)(g_lightGLIdx + 0.5f); if (idx < 0) idx = 0; if (idx > 7) idx = 7;
    static_cast<Light*>(ObjActivo)->SetLightID(GL_LIGHT0 + (GLenum)idx);
}

// click en el selector: abre el desplegable (new / default / existentes)
static void AccionMenuMateriales(){
    if (!PropsActivo) return;
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    if (!MenuMateriales) {
        MenuMateriales = new PopupMenu();
        MenuMateriales->action = AccionMaterialElegido;
    }
    MenuMateriales->Limpiar(); // la lista de materiales va cambiando
    MenuMateriales->Agregar("New Material", 0, IconType::material);
    MenuMateriales->Agregar("Default Material", 1, IconType::material);
    for (size_t i = 0; i < Materials.size(); i++) {
        MenuMateriales->Agregar(Materials[i]->name, 2 + (int)i, IconType::material);
    }
    AbrirMenuBajoBoton(MenuMateriales, PropsActivo->propBtnNewMaterial->button);
}

// ====================================================================
// STACK de MODIFICADORES: menu "Add" (los 5 tipos) + acciones Add/Remove/Move. El stack vive en el Mesh
// (editor); aca solo la UI. Por ahora NO se genera ninguna malla: solo se gestiona la lista y su orden.
// ====================================================================
static PopupMenu* MenuAddModifier = NULL;

static void AccionAddModifierElegido(int id){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    ((Mesh*)ObjActivo)->AgregarModificador(id); // id = ModifierType (Screw/Mirror/Array/SubSurf/Boolean)
    SelEnListaModificador();                     // el nuevo queda seleccionado en el selector
    PropertiesLayoutDirty = true;                // re-layout (aparecen Remove / Move / la 2da tarjeta)
}
static void AccionMenuAddModifier(){
    if (!PropsActivo || !ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    if (!MenuAddModifier){
        MenuAddModifier = new PopupMenu();
        MenuAddModifier->action = AccionAddModifierElegido;
        MenuAddModifier->Agregar("Screw",               ModifierType::Screw);
        MenuAddModifier->Agregar("Mirror",              ModifierType::Mirror);
        MenuAddModifier->Agregar("Array",               ModifierType::Array);
        MenuAddModifier->Agregar("Subdivision Surface", ModifierType::SubdivisionSurface);
        MenuAddModifier->Agregar("Boolean",             ModifierType::Boolean);
    }
    if (PropsActivo->propRowMod && !PropsActivo->propRowMod->botones.empty()){
        AbrirMenuBajoBoton(MenuAddModifier, PropsActivo->propRowMod->botones[0]); // el boton "Add"
    }
}
static void AccionRemoveModifier(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    ((Mesh*)ObjActivo)->QuitarModificadorActivo();
    SelEnListaModificador();
    PropertiesLayoutDirty = true;
}
static void AccionModifierUp(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    ((Mesh*)ObjActivo)->MoverModificador(-1); // sube en el stack (el orden importa)
    SelEnListaModificador();                  // mantiene seleccionado el modificador MOVIDO
    PropertiesLayoutDirty = true;
}
static void AccionModifierDown(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::mesh) return;
    ((Mesh*)ObjActivo)->MoverModificador(+1); // baja en el stack
    SelEnListaModificador();
    PropertiesLayoutDirty = true;
}


// ====================================================================
// selector de MODO de rotacion (XYZ Euler / Quaternion / Axis Angle)
// ====================================================================
static PopupMenu* MenuRotMode = NULL;

static void AccionRotModeElegido(int id){
    if (!ObjActivo) return;
    ObjActivo->rotMode = id;            // 0=XYZ Euler, 1=Quaternion, 2=Axis Angle
    ObjActivo->ActualizarDisplayRot();  // pasa el display al nuevo modo
    if (PropsActivo) PropsActivo->target = NULL; // fuerza el re-bind (RefreshTarget)
    PropertiesLayoutDirty = true;       // aparece/desaparece el campo W
}

// click en el selector: abre el desplegable con los 3 modos
static void AccionMenuRotMode(){
    if (!PropsActivo || !ObjActivo) return;
    if (!MenuRotMode){
        MenuRotMode = new PopupMenu();
        MenuRotMode->action = AccionRotModeElegido;
    }
    MenuRotMode->Limpiar();
    MenuRotMode->Agregar("XYZ Euler", RotEulerXYZ);
    MenuRotMode->Agregar("Quaternion (WXYZ)", RotQuaternion);
    MenuRotMode->Agregar("Axis Angle", RotAxisAngle);
    AbrirMenuBajoBoton(MenuRotMode, PropsActivo->propRotMode->button);
}

// ====================================================================
// selector de TARGET (objeto linkeado) para camara e instance/array/mirror
// (ambos tipos heredan de Target). Un desplegable con los objetos de la escena.
// ====================================================================
static PopupMenu* MenuTarget = NULL;
static std::vector<Object*> gTargetCandidatos; // id - 1 -> objeto

// devuelve la parte Target* de ObjActivo si es camara o instance (sino NULL)
static Target* ObjComoTarget(Object* o){
    if (!o) return NULL;
    if (o->getType() == ObjectType::camera)   return static_cast<Camera*>(o);
    if (o->getType() == ObjectType::instance) return static_cast<Instance*>(o);
    return NULL;
}

// junta los objetos de la escena que pueden ser target (no el activo, no las
// colecciones, no a si mismo para evitar recursion)
static void RecolectarTargets(Object* nodo){
    if (!nodo) return;
    for (size_t i = 0; i < nodo->Childrens.size(); i++){
        Object* c = nodo->Childrens[i];
        if (c != ObjActivo && c->getType() != ObjectType::collection)
            gTargetCandidatos.push_back(c);
        RecolectarTargets(c);
    }
}

static void AccionTargetElegido(int id){
    Target* tgt = ObjComoTarget(ObjActivo);
    if (!tgt) return;
    if (id == 0){ tgt->target = NULL; tgt->targetName = ""; return; } // None
    int idx = id - 1;
    if (idx >= 0 && idx < (int)gTargetCandidatos.size()){
        Object* o = gTargetCandidatos[idx];
        tgt->target = o;
        tgt->targetName = o->name;
    }
}

static void AccionMenuTarget(){
    if (!PropsActivo) return;
    if (!ObjComoTarget(ObjActivo)) return;
    if (!MenuTarget){ MenuTarget = new PopupMenu(); MenuTarget->action = AccionTargetElegido; }
    MenuTarget->Limpiar();
    MenuTarget->Agregar("None", 0);
    gTargetCandidatos.clear();
    RecolectarTargets(SceneCollection);
    for (size_t i = 0; i < gTargetCandidatos.size(); i++)
        MenuTarget->Agregar(gTargetCandidatos[i]->name, 1 + (int)i,
                            (int)IconoDeObjeto(gTargetCandidatos[i]));
    bool esCam = ObjActivo->getType() == ObjectType::camera;
    Button* b = (esCam ? PropsActivo->propBtnCamTarget
                       : PropsActivo->propBtnInstTarget)->button;
    MenuTarget->Abrir(b->sx, b->sy + b->height - GlobalScale,
                      MenuPantallaW, MenuPantallaH);
    MenuAbierto = MenuTarget;
}

// ===== props del modificador MIRROR (tarjeta de abajo): helper + acciones (param change / target / apply) =====
static Modifier* ModActivoUI(){
    if (!ObjActivo || ObjActivo->getType()!=ObjectType::mesh) return NULL;
    Mesh* m=(Mesh*)ObjActivo;
    if (m->modificadorActivo<0 || m->modificadorActivo>=(int)m->modificadores.size()) return NULL;
    return m->modificadores[m->modificadorActivo];
}
// un param del modificador cambio (checkbox/float/target) -> REGENERAR la malla generada + redibujar
static void AccionModParamChanged(){
    if (ObjActivo && ObjActivo->getType()==ObjectType::mesh) ((Mesh*)ObjActivo)->GenerarMallaModificada();
    g_redraw = true;
}
// menu "Mirror Object": elegir CUALQUIER objeto de la escena como target del mirror (reusa RecolectarTargets)
static PopupMenu* MenuModTarget = NULL;
static void AccionModTargetElegido(int id){
    Modifier* mod = ModActivoUI(); if (!mod) return;
    int idx = id - 1; // 0 = None
    mod->target = (idx>=0 && idx<(int)gTargetCandidatos.size()) ? gTargetCandidatos[idx] : NULL;
    AccionModParamChanged();
}
static void AccionMenuModTarget(){
    if (!PropsActivo || !PropsActivo->propMirTarget) return;
    if (!MenuModTarget){ MenuModTarget=new PopupMenu(); MenuModTarget->action=AccionModTargetElegido; }
    MenuModTarget->Limpiar();
    MenuModTarget->Agregar("None", 0);
    gTargetCandidatos.clear(); RecolectarTargets(SceneCollection);
    for (size_t i=0;i<gTargetCandidatos.size();i++)
        MenuModTarget->Agregar(gTargetCandidatos[i]->name, 1+(int)i, (int)IconoDeObjeto(gTargetCandidatos[i]));
    AbrirMenuBajoBoton(MenuModTarget, PropsActivo->propMirTarget->button);
}
// menu "Axis" del Screw: dropdown X/Y/Z (como el modo de rotacion; nada de pestaña rara)
static PopupMenu* MenuScrewAxis = NULL;
static void AccionScrewAxisElegido(int id){
    Modifier* mod = ModActivoUI(); if (!mod) return;
    mod->screwAxis = id; // 0=X, 1=Y, 2=Z
    AccionModParamChanged();
}
static void AccionMenuScrewAxis(){
    if (!PropsActivo || !PropsActivo->propScrewAxis) return;
    if (!MenuScrewAxis){ MenuScrewAxis = new PopupMenu(); MenuScrewAxis->action = AccionScrewAxisElegido; }
    MenuScrewAxis->Limpiar();
    MenuScrewAxis->Agregar("X", 0); MenuScrewAxis->Agregar("Y", 1); MenuScrewAxis->Agregar("Z", 2);
    AbrirMenuBajoBoton(MenuScrewAxis, PropsActivo->propScrewAxis->button);
}
// "Apply Modifier": hornea la malla generada en la editable + saca el modificador del stack
static void AccionAplicarModificador(){
    if (!ObjActivo || ObjActivo->getType()!=ObjectType::mesh) return;
    ((Mesh*)ObjActivo)->AplicarModificadorActivo();
    PropertiesLayoutDirty = true; g_redraw = true;
    Notificar("Modifier applied", false);
}

// boton "Wavefront.obj" de la tarjeta Export: exporta al path del campo editable
static void AccionExportObj(){
    if (!PropsActivo || !PropsActivo->propExportName) return;
    std::string path = PropsActivo->propExportName->field.text;
    if (path.empty()) { Notificar("No export path set", true); return; }
#ifdef W3D_SYMBIAN
    // N95: carpeta FIJA E:/whisk3d/models/ (creada en AppInit). Toma solo el nombre del campo.
    size_t sl = path.find_last_of("/\\");
    std::string nombre = (sl == std::string::npos) ? path : path.substr(sl + 1);
    if (nombre.empty()) nombre = "model.obj";
    path = std::string("E:/whisk3d/models/") + nombre;
#endif
    bool ok = ExportOBJ(path, PropsActivo->exportSelectedOnly, PropsActivo->exportApplyModifiers, PropsActivo->exportApplyTransforms);
    if (ok) Notificar("OBJ saved successfully!", false);
    else    Notificar("Error: could not save the OBJ", true);
}

// el explorador (modo guardar) devolvio una CARPETA (o un .obj a sobrescribir): se
// arma el path completo carpeta + nombre actual y se pone en el campo File.
static void ExportFolderElegido(const std::string& elegido){
    if (!PropsActivo || !PropsActivo->propExportName) return;
    std::string actual = PropsActivo->propExportName->field.text;
    // nombre = basename del campo actual (o Cube.obj)
    size_t s = actual.find_last_of("/\\");
    std::string nombre = (s == std::string::npos) ? actual : actual.substr(s + 1);
    if (nombre.empty()) nombre = "export.obj";
    // si el usuario eligio un .obj existente -> usar ese path tal cual (sobrescribir)
    bool esObj = (elegido.size() >= 4 && elegido.substr(elegido.size() - 4) == ".obj");
    std::string full = esObj ? elegido : (elegido + "/" + nombre);
    PropsActivo->propExportName->field.SetText(full);
}

// boton de la carpeta: abre el explorador para elegir DONDE guardar el .obj
static void AccionBrowseExport(){
    AbrirFileBrowser("Exportar a...", "Select Folder", ".obj", ExportFolderElegido, true);
}

// arma "base[_tag]_0001.png" desde el campo Output, el pase (tag) y el frame de animacion actual.
// el _0001 es CurrentFrame (para secuencias mas adelante).
static std::string RenderFileNamePNG(const std::string& out, const char* tag){
    std::string base = out;
    size_t dot   = base.find_last_of('.');
    size_t slash = base.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        base = base.substr(0, dot); // saca la extension del campo Output
#ifdef W3D_SYMBIAN
    // N95: carpeta FIJA E:/whisk3d/render/ (prolijo + sabes donde queda). Toma solo el nombre del campo.
    size_t sl = base.find_last_of("/\\");
    std::string nombre = (sl == std::string::npos) ? base : base.substr(sl + 1);
    if (nombre.empty()) nombre = "render";
    base = std::string("E:/whisk3d/render/") + nombre;
#else
    if (base.empty()) base = "render";
#endif
    if (tag && tag[0]) { base += "_"; base += tag; }
    int frame = 0;
#ifndef W3D_SYMBIAN
    extern int CurrentFrame; // frame de animacion actual (Animation.cpp; en N95 no se linkea todavia)
    frame = CurrentFrame;
#endif
    char suf[24];
    snprintf(suf, sizeof(suf), "_%04d.png", frame);
    base += suf;
    return base;
}

// boton "Render Image": guarda el pase beauty (siempre) + los pases tildados (zbuffer/normal)
// como PNG a la resolucion pedida. El render por tiles permite tamanos mayores que la ventana.
// regenera la malla modificada de TODAS las mallas de la escena (para aplicar el cambio de nivel viewport<->render)
static void RegenerarModsEscena(Object* nodo){
    if (!nodo) return;
    for (size_t i=0; i<nodo->Childrens.size(); i++){ Object* o = nodo->Childrens[i]; if (!o) continue;
        if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; if (!m->modificadores.empty()) m->GenerarMallaModificada(); }
        RegenerarModsEscena(o); }
}

static void AccionRenderImage(){
    if (!PropsActivo) return;
    Viewport3D* vp = Viewport3DActive;
    if (!vp) { Notificar("No active 3D viewport", true); return; }
    std::string out = PropsActivo->propRenderOutput ? PropsActivo->propRenderOutput->field.text
                                                    : std::string("render.png");
    int w = (int)(PropsActivo->renderW + 0.5f); if (w < 1) w = 1;
    int h = (int)(PropsActivo->renderH + 0.5f); if (h < 1) h = 1;

    // barra de progreso 0..100%: total = PASES x TILES (frames = 1 en Render Image; la animacion sera x frames).
    bool doZ = PropsActivo->renderZbuffer, doN = PropsActivo->renderNormal, doA = PropsActivo->renderAlpha;
    int nPases = 1 + (doZ?1:0) + (doN?1:0) + (doA?1:0); // beauty siempre + los tildados
    int tpp    = vp->TilesNecesarios(w, h);
    int total  = nPases * tpp;

    // Subdivision (y cualquier modificador con nivel de render): regenerar con el nivel de RENDER antes de renderizar
    extern bool g_modRenderMode;
    g_modRenderMode = true; RegenerarModsEscena(SceneCollection);

    ProgresoIniciar("Rendering...");
    int fallos = 0, base = 0;
    if (!vp->RenderAPNG(w, h, RenderType::Rendered, RenderFileNamePNG(out, "").c_str(), base, total)) fallos++;
    base += tpp;
    if (doZ){ if (!vp->RenderAPNG(w, h, RenderType::ZBuffer,    RenderFileNamePNG(out, "zbuffer").c_str(), base, total)) fallos++; base += tpp; }
    if (doN){ if (!vp->RenderAPNG(w, h, RenderType::NormalView, RenderFileNamePNG(out, "normal").c_str(),  base, total)) fallos++; base += tpp; }
    if (doA){ if (!vp->RenderAPNG(w, h, RenderType::Alpha,      RenderFileNamePNG(out, "alpha").c_str(),   base, total)) fallos++; base += tpp; }
    ProgresoFin();

    g_modRenderMode = false; RegenerarModsEscena(SceneCollection); // volver al nivel de VIEWPORT

    if (fallos == 0) Notificar("Render saved!", false);
    else             Notificar("Error: could not save the render", true);
}

// === pestaña VERTICES: helpers + acciones (UV Maps + capas de color) ===
static Mesh* VerticesMesh() {
    return (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
}
// (la SELECCION de la capa activa la hace la lista PropListMeshParts; aca solo Add + el toggle)
// PropertiesLayoutDirty = recalcula alturas + la SCROLLBAR (sino no se podia scrollear al item nuevo)
static void AccionVertAddUVMap()  { Mesh* m = VerticesMesh(); if (m) { DuplicarUVMapActivo(m); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertAddColor()  { Mesh* m = VerticesMesh(); if (m) { DuplicarColorLayerActivo(m); PropertiesLayoutDirty = true; g_redraw = true; } }
static void AccionVertColorMode() {   // toggle Per-Vertex / Per-Corner de la capa de color activa
    Mesh* m = VerticesMesh(); if (!m) return;
    if (m->colorActivo >= 0 && m->colorActivo < (int)m->colorLayers.size()) {
        ColorLayer* cl = m->colorLayers[m->colorActivo];
        cl->porVertice = !cl->porVertice;
        m->AplicarCapasAlRender(); g_redraw = true;
    }
}

void Properties::ConstruirGrupos(){
    propTransform = new GroupPropertie("Transform");

    propTransform->properties.push_back(new PropFloat("Location X"));
    propTransform->properties.push_back(new PropFloat("Y"));
    propTransform->properties.push_back(new PropFloat("Z"));

    propTransform->properties.push_back(new PropGap(""));

    // selector de modo de rotacion (dropdown) + campos W/X/Y/Z. Que campos se
    // muestran (W solo en Quaternion/Axis) y a que apuntan lo hace RefreshTarget.
    propRotMode = new PropButton("Mode");                            // [4]
    propRotMode->button->desplegable = true;
    propRotMode->action = AccionMenuRotMode;
    propTransform->properties.push_back(propRotMode);
    propTransform->properties.push_back(new PropFloat("Rotation W")); // [5] (condicional)
    propTransform->properties.push_back(new PropFloat("Rotation X")); // [6]
    propTransform->properties.push_back(new PropFloat("Y"));          // [7]
    propTransform->properties.push_back(new PropFloat("Z"));          // [8]
    // editar W/X/Y/Z (flechas o arrastre) reconstruye el quaternion real
    for (int r = 5; r <= 8; r++)
        static_cast<PropFloat*>(propTransform->properties[r])->onChange = SincronizarRotacionActiva;

    propTransform->properties.push_back(new PropGap(""));

    propTransform->properties.push_back(new PropFloat("Scale X"));
    propTransform->properties.push_back(new PropFloat("Y"));
    propTransform->properties.push_back(new PropFloat("Z"));

    // "Rename" del OBJETO (abajo de todo): aplica a CUALQUIER objeto; nombre unico. Se vuelve input al apretar.
    propBtnRenameObj = new PropButton("Rename", -1);
    propBtnRenameObj->action = AccionRenameObjeto;
    propTransform->properties.push_back(propBtnRenameObj);

    GroupProperties.push_back(propTransform);

    // ===== Tarjeta "Mesh Parts": selector (lista) + gestion de la PARTE (sin material) =====
    propMeshParts = new GroupPropertie("Mesh Parts");
    propMeshParts->anchoValores = 0.30f;
    propMeshParts->properties.push_back(new PropListMeshParts("Mesh Parts")); // [0] selector (lo lee Rebind)
    PropButton* pbNewPart = new PropButton("New Mesh Part", IconType::mesh);
    pbNewPart->action = AccionNuevoMeshPart;      propMeshParts->properties.push_back(pbNewPart);
    // fila: Assign | Select | Deselect (sin icono, 33% c/u, auto-ancho con gap)
    propRowPartOps = new PropButtonRow();
    propRowPartOps->Agregar("Assign",   AccionAssignMeshPart);
    propRowPartOps->Agregar("Select",   AccionSelectMeshPart);
    propRowPartOps->Agregar("Deselect", AccionDeselectMeshPart);
    propMeshParts->properties.push_back(propRowPartOps);
    // fila: Delete | Rename (sin icono, 50% c/u). Delete se oculta si hay 1 sola parte (no borrable).
    propRowDelRen = new PropButtonRow();
    propRowDelRen->Agregar("Delete", AccionBorrarMeshPart);
    propRowDelRen->Agregar("Rename", AccionRenameMeshPart); // el boton Rename se vuelve input al apretarlo
    propMeshParts->properties.push_back(propRowDelRen);
    // fila: Move Up | Move Down (oculta si hay 1 sola parte). El ORDEN del mesh part = ORDEN DE DIBUJADO
    // (dibujar los solidos primero y los transparentes al final).
    propRowPartMove = new PropButtonRow();
    propRowPartMove->Agregar("Move Up",   AccionMeshPartUp);
    propRowPartMove->Agregar("Move Down", AccionMeshPartDown);
    propMeshParts->properties.push_back(propRowPartMove);
    GroupProperties.push_back(propMeshParts);

    // ===== Tarjeta APARTE "Material". Orden (pedido Dante): New Material + Rename Material (las opciones
    // del material), LINEA separadora, y abajo la textura + sus opciones. Los props se guardan en arrays
    // (propMatChk/propMatCol/propMatShin) -> Rebind los setea por nombre, NO por indice (reordenar = libre).
    propMaterial = new GroupPropertie("Material");
    propMaterial->anchoValores = 0.30f;
    propBtnNewMaterial = new PropButton("New Material", IconType::material);
    propBtnNewMaterial->button->desplegable = true;
    propBtnNewMaterial->action = AccionMenuMateriales;
    propMaterial->properties.push_back(propBtnNewMaterial);
    propBtnRenameMat = new PropButton("Rename Material", -1); // ANTES de la textura, SIN icono (pedido Dante)
    propBtnRenameMat->action = AccionRenameMaterial; // se vuelve input al apretarlo; oculto si es el default
    propMaterial->properties.push_back(propBtnRenameMat);
    // aviso cuando el mesh part usa el material POR DEFECTO (oculto si tiene uno propio). 1 label WRAP: se
    // adapta al ancho (salto de linea en los espacios) -> se lee entero aunque se achique el panel.
    propMsgDefault = new PropLabel("The default material can not be edited. Create a new material.", true /*wrap*/);
    propMaterial->properties.push_back(propMsgDefault);
    // LINEA: separa las opciones del material (arriba) de la textura + sus opciones (abajo). Se OCULTA con el
    // material por defecto (sino queda una linea huerfana molesta debajo del aviso).
    propSepMat = new PropSeparator();
    propMaterial->properties.push_back(propSepMat);
    propBtnTextura = new PropButton("No Texture", IconType::textura);
    propBtnTextura->button->desplegable = true;
    propBtnTextura->action = AccionMenuTexturas;
    propMaterial->properties.push_back(propBtnTextura);
    // COLORES + Shininess ARRIBA (pedido Dante); los checkboxes ABAJO. Rebind los setea por member
    // (propMatCol/propMatShin/propMatChk), asi que el orden de push NO importa para el bind.
    const char* nombresCol[3] = { "Base Color","Specular","Emission" };
    for (int i = 0; i < 3; i++) { propMatCol[i] = new PropColor(nombresCol[i]); propMaterial->properties.push_back(propMatCol[i]); }
    propMatShin = new PropFloat("Shininess");
    propMatShin->SetRango(0.0f, 255.0f);
    propMatShin->stepFino = 1.0f; propMatShin->stepGrueso = 10.0f; propMatShin->dragStep = 1.0f;
    propMatShin->entero = true;   // es un entero (Dante: "que sea entero"), no float
    propMatShin->acelera = true;  // izq/der arranca en 1 y acelera (Dante: "empieza lento y despues acelera")
    propMaterial->properties.push_back(propMatShin);
    // [8]="Reflection" (antes "Chrome"); [9] quedo SIN uso (lo reemplazo el dropdown de modo -> oculto en Rebind);
    // [10]="Normal Mapping". Se CONSTRUYEN [0..10] pero se PUSHEAN en orden VISUAL (ver abajo).
    const char* nombresChk[11] = { "Filtering","Transparent","Vertex Color","Lighting","Repeat","Culling","Depth Test","Smooth Shading","Reflection","(reflect mode)","Normal Mapping" };
    for (int i = 0; i < 11; i++) {
        propMatChk[i] = new PropBool(nombresChk[i]);
        // onChange = re-Rebind: togglear CUALQUIER checkbox de material re-arma la tarjeta -> los checkboxes/filas
        // que dependen de otro (Reflection muestra su dropdown, Normal Mapping su selector) aparecen/desaparecen al
        // instante. Va EN EL CHECKBOX (no en el handler de click) -> dispara IGUAL por click (PC) o teclado (Symbian).
        propMatChk[i]->onChange = RebindMaterialMeshPart;
    }
    // selector de la TEXTURA del normal map (aparece si Normal Mapping ON) y dropdown del MODO de Reflection (aparece
    // si Reflection ON; reemplaza el viejo checkbox "Chrome 360"). Se construyen aca, se pushean en el orden de abajo.
    propBtnNormalTex = new PropButton("No Normal Map", IconType::textura);
    propBtnNormalTex->button->desplegable = true;
    propBtnNormalTex->action = AccionMenuTexturasNormal;
    propBtnReflectMode = new PropButton("Matcap (hardware)", IconType::material);
    propBtnReflectMode->button->desplegable = true;
    propBtnReflectMode->action = AccionMenuReflectMode;
    // ORDEN VISUAL (Dante: "Normal Mapping ARRIBA de Reflection -> al activar normal map y desaparecer el reflejo
    // queda mas natural"): Filtering..Smooth Shading, luego NORMAL MAPPING + su selector, luego REFLECTION + su dropdown.
    for (int i = 0; i < 7; i++) propMaterial->properties.push_back(propMatChk[i]); // [0..6] comunes (Smooth Shading [7] se quito: el shading es de la malla, no del material)
    propMaterial->properties.push_back(propMatChk[10]); // Normal Mapping
    propMaterial->properties.push_back(propBtnNormalTex);
    propMaterial->properties.push_back(propMatChk[8]);  // Reflection
    propMaterial->properties.push_back(propMatChk[9]);  // (oculto: reemplazado por el dropdown)
    propMaterial->properties.push_back(propBtnReflectMode);
    GroupProperties.push_back(propMaterial);

    // pestania de LUZ: TODAS las propiedades editables de la luz de OpenGL (pedido Dante). Se ve solo si el
    // objeto activo es una luz. OpenGL = UN tipo de luz configurable: Direccional / Puntual / Spot (ver Light.h).
    propLight = new GroupPropertie("Light");
    propLightDir = new PropBool("Directional");                 // w=0 (sol) vs puntual/spot
    propLight->properties.push_back(propLightDir);
    propLightGL = new PropFloat("GL Light");                    // numero de GL light (0..7), entero editable
    propLightGL->SetRango(0.0f, 7.0f); propLightGL->entero = true;
    propLightGL->stepFino = 1.0f; propLightGL->stepGrueso = 1.0f; propLightGL->dragStep = 1.0f;
    propLightGL->onChange = OnLightGLChange;
    propLight->properties.push_back(propLightGL);
    propLightDiffuse = new PropColor("Diffuse");  propLight->properties.push_back(propLightDiffuse);
    propLightAmbient = new PropColor("Ambient");  propLight->properties.push_back(propLightAmbient);
    propLightSpecular = new PropColor("Specular"); propLight->properties.push_back(propLightSpecular);
    // atenuacion 1/(C + L*d + Q*d^2) (afecta a la puntual/spot)
    propLightAttC = new PropFloat("Att Constant"); propLightAttC->SetRango(0.0f, 5.0f);
    propLightAttC->stepFino = 0.02f; propLightAttC->stepGrueso = 0.1f; propLightAttC->dragStep = 0.01f;
    propLight->properties.push_back(propLightAttC);
    propLightAttL = new PropFloat("Att Linear"); propLightAttL->SetRango(0.0f, 2.0f);
    propLightAttL->stepFino = 0.01f; propLightAttL->stepGrueso = 0.05f; propLightAttL->dragStep = 0.005f;
    propLight->properties.push_back(propLightAttL);
    propLightAttQ = new PropFloat("Att Quadratic"); propLightAttQ->SetRango(0.0f, 1.0f);
    propLightAttQ->stepFino = 0.005f; propLightAttQ->stepGrueso = 0.02f; propLightAttQ->dragStep = 0.002f;
    propLight->properties.push_back(propLightAttQ);
    // spotlight: cono (grados) + concentracion del haz
    propLightSpotCut = new PropFloat("Spot Cutoff"); propLightSpotCut->SetRango(1.0f, 180.0f); propLightSpotCut->entero = true;
    propLightSpotCut->stepFino = 1.0f; propLightSpotCut->stepGrueso = 5.0f; propLightSpotCut->dragStep = 1.0f;
    propLight->properties.push_back(propLightSpotCut);
    propLightSpotExp = new PropFloat("Spot Exponent"); propLightSpotExp->SetRango(0.0f, 128.0f); propLightSpotExp->entero = true;
    propLightSpotExp->stepFino = 1.0f; propLightSpotExp->stepGrueso = 8.0f; propLightSpotExp->dragStep = 1.0f;
    propLight->properties.push_back(propLightSpotExp);
    GroupProperties.push_back(propLight);

    // pestania de CAMARA: el target (look-at)
    propCamera = new GroupPropertie("Camera");
    propBtnCamTarget = new PropButton("Target", IconType::object);
    propBtnCamTarget->button->desplegable = true;
    propBtnCamTarget->action = AccionMenuTarget;
    propCamera->properties.push_back(propBtnCamTarget);
    GroupProperties.push_back(propCamera);

    // pestania de los objetos especiales (Duplicate Linked / Array / Mirror):
    // el objeto al que apuntan (target)
    propInstance = new GroupPropertie("Linked");
    propBtnInstTarget = new PropButton("Target", IconType::object);
    propBtnInstTarget->button->desplegable = true;
    propBtnInstTarget->action = AccionMenuTarget;
    propInstance->properties.push_back(propBtnInstTarget);
    GroupProperties.push_back(propInstance);

    // ===== pestania RENDER: tarjeta "Render" (arriba) + tarjeta "Export" (abajo) =====
    propRender = new GroupPropertie("Render");
    propRender->anchoValores = 0.62f; // columna de valor ANCHA (paths)
    propRenderOutput = new PropText("Output", "render.png");
    propRender->properties.push_back(propRenderOutput);
    // resolucion editable (default 640x480). Puede ser MAYOR que la ventana: se rinde por tiles.
    renderW = 640.0f; renderH = 480.0f;
    propRenderW = new PropFloat("Width");
    propRenderW->SetRango(1.0f, 8192.0f); propRenderW->entero = true;
    propRenderW->stepFino = 1.0f; propRenderW->stepGrueso = 16.0f; propRenderW->dragStep = 1.0f;
    propRenderW->value = &renderW;
    propRender->properties.push_back(propRenderW);
    propRenderH = new PropFloat("Height");
    propRenderH->SetRango(1.0f, 8192.0f); propRenderH->entero = true;
    propRenderH->stepFino = 1.0f; propRenderH->stepGrueso = 16.0f; propRenderH->dragStep = 1.0f;
    propRenderH->value = &renderH;
    propRender->properties.push_back(propRenderH);
    // pases EXTRA a exportar (el beauty/render siempre se guarda). Nombre: base_zbuffer_0001.png, etc.
    renderZbuffer = false; renderNormal = false; renderAlpha = false;
    propRenderZbuffer = new PropBool("ZBuffer"); propRenderZbuffer->value = &renderZbuffer;
    propRender->properties.push_back(propRenderZbuffer);
    propRenderNormal = new PropBool("Normal"); propRenderNormal->value = &renderNormal;
    propRender->properties.push_back(propRenderNormal);
    propRenderAlpha = new PropBool("Alpha"); propRenderAlpha->value = &renderAlpha;
    propRender->properties.push_back(propRenderAlpha);
    // color de FONDO del render (global g_renderBg, solo para el pase Rendered). Se edita con el color picker.
    propRenderBg = new PropColor("Background");
    propRenderBg->value = g_renderBg; // el array global decae a puntero (igual que los colores de material/luz)
    propRender->properties.push_back(propRenderBg);
    // boton con action real (antes era no-op)
    PropButton* pbRenderImg = new PropButton("Render Image");
    pbRenderImg->action = AccionRenderImage;
    propRender->properties.push_back(pbRenderImg);
    propRender->properties.push_back(new PropButton("Render Animation")); // TODO: loop StartFrame..EndFrame
    GroupProperties.push_back(propRender);

    propExport = new GroupPropertie("Export");
    propExport->anchoValores = 0.62f;
    PropBool* pbSel = new PropBool("Selected only");
    pbSel->value = &exportSelectedOnly;
    propExport->properties.push_back(pbSel);
    PropBool* pbMods = new PropBool("Apply Modifiers"); // ON = exporta la malla generada por los modificadores
    pbMods->value = &exportApplyModifiers;
    propExport->properties.push_back(pbMods);
    PropBool* pbXf = new PropBool("Apply Transforms"); // ON = hornea el transform del objeto en el .obj (mundo)
    pbXf->value = &exportApplyTransforms;
    propExport->properties.push_back(pbXf);
    propExportName = new PropText("File", "cube.obj");
    propExport->properties.push_back(propExportName);
    PropButton* pbBrowse = new PropButton("Browse folder...", IconType::carpeta);
    pbBrowse->action = AccionBrowseExport; // abre el explorador -> setea el path en File
    propExport->properties.push_back(pbBrowse);
    PropButton* pbExp = new PropButton("Wavefront.obj", IconType::mesh);
    pbExp->action = AccionExportObj;
    propExport->properties.push_back(pbExp);
    GroupProperties.push_back(propExport);

    // pestaña VERTICES (icono mesh): 3 TARJETAS. Las listas REUSAN PropListMeshParts (con scroll,
    // resize, etc., el MISMO componente que el selector de mesh part) en modo 1 (uvmaps) / 2 (colors).
    propUVMaps = new GroupPropertie("UV Maps");
    propListUV = new PropListMeshParts("UV Maps"); propListUV->modo = 1;
    propUVMaps->properties.push_back(propListUV);
    PropButton* pbAddUV = new PropButton("Add UV Map", IconType::mesh);
    pbAddUV->action = AccionVertAddUVMap;
    propUVMaps->properties.push_back(pbAddUV);
    propBtnRenameUV = new PropButton("Rename", -1); // renombra la UV map activa (nombre unico por malla)
    propBtnRenameUV->action = AccionRenameUVMap;
    propUVMaps->properties.push_back(propBtnRenameUV);
    GroupProperties.push_back(propUVMaps);

    propColorLayers = new GroupPropertie("Color");
    propListColor = new PropListMeshParts("Color"); propListColor->modo = 2;
    propColorLayers->properties.push_back(propListColor);
    PropButton* pbAddCol = new PropButton("Add Color Layer", IconType::mesh);
    pbAddCol->action = AccionVertAddColor;
    propColorLayers->properties.push_back(pbAddCol);
    propBtnColorMode = new PropButton("Color Mode", IconType::mesh);
    propBtnColorMode->action = AccionVertColorMode;
    propColorLayers->properties.push_back(propBtnColorMode);
    propBtnRenameColor = new PropButton("Rename", -1); // renombra la capa de color activa (nombre unico)
    propBtnRenameColor->action = AccionRenameColor;
    propColorLayers->properties.push_back(propBtnRenameColor);
    GroupProperties.push_back(propColorLayers);

    propVertexAnim = new GroupPropertie("Vertex Animation");
    propVertexAnim->properties.push_back(new PropLabel("(coming soon)")); // placeholder
    GroupProperties.push_back(propVertexAnim);

    // ===== pestania "Modifiers" (mesh): selector del stack + Add/Remove + Move Up/Down =====
    propModifiers = new GroupPropertie("Modifiers");
    propModifiers->anchoValores = 0.30f;
    propListModifiers = new PropListMeshParts("Modifiers");
    propListModifiers->modo = 3;                              // 3 = stack de modificadores (mesh->modificadores)
    propModifiers->properties.push_back(propListModifiers);   // [0] selector (el mismo componente de UV/parts)
    // fila: Add (desplegable: abre el menu de tipos) | Remove (oculto si no hay modificadores)
    propRowMod = new PropButtonRow();
    Button* bAddMod = propRowMod->Agregar("Add", AccionMenuAddModifier);
    bAddMod->desplegable = true;
    propRowMod->Agregar("Remove", AccionRemoveModifier);
    propModifiers->properties.push_back(propRowMod);
    // fila: Move Up | Move Down (toda la fila oculta si hay < 2 -> el orden solo importa con 2+)
    propRowModMove = new PropButtonRow();
    propRowModMove->Agregar("Move Up",   AccionModifierUp);
    propRowModMove->Agregar("Move Down", AccionModifierDown);
    propModifiers->properties.push_back(propRowModMove);
    GroupProperties.push_back(propModifiers);

    // ===== 2da tarjeta: props del modificador SELECCIONADO. Por ahora las del MIRROR (bindeadas al modificador
    // activo en ActualizarPestanias); otros tipos muestran "(no properties yet)". Solo visible con un modificador. =====
    propModifierProps = new GroupPropertie("Modifier");
    propModifierProps->anchoValores = 0.55f;
    // Visibilidad (TODOS los modificadores): en el viewport (OFF = nunca se calcula) y en Edit Mode (OFF = edicion
    // rapida en N95, se recalcula al salir). onChange regenera la malla.
    propModVerViewport = new PropBool("Display in Viewport"); propModVerViewport->onChange = AccionModParamChanged;
    propModifierProps->properties.push_back(propModVerViewport);
    propModVerEdit = new PropBool("Display in Edit Mode"); propModVerEdit->onChange = AccionModParamChanged;
    propModifierProps->properties.push_back(propModVerEdit);
    propModVacio = new PropLabel("(no properties yet)");
    propModifierProps->properties.push_back(propModVacio);
    // Mirror: ejes X/Y/Z
    propMirX = new PropBool("Mirror X"); propMirX->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirX);
    propMirY = new PropBool("Mirror Y"); propMirY->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirY);
    propMirZ = new PropBool("Mirror Z"); propMirZ->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirZ);
    // Mirror Object (target: cualquier objeto; su posicion+rotacion define el plano)
    propMirTarget = new PropButton("Mirror Object", IconType::object);
    propMirTarget->button->desplegable = true; propMirTarget->action = AccionMenuModTarget;
    propModifierProps->properties.push_back(propMirTarget);
    // Merge (soldar los verts del plano) + distancia
    propMirMerge = new PropBool("Merge"); propMirMerge->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirMerge);
    propMirDist = new PropFloat("Merge Distance", "m"); propMirDist->onChange = AccionModParamChanged;
    propMirDist->SetRango(0.0f, 1.0f); propMirDist->stepFino = 0.0001f; propMirDist->dragStep = 0.0005f;
    propModifierProps->properties.push_back(propMirDist);
    // Clipping (edit-time): clampea los verts al plano al moverlos y, una vez pegados, los deja pegados (arranca ON)
    propMirClip = new PropBool("Clipping"); propMirClip->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propMirClip);
    // Subdivision Surface: modo Simple (OFF = Catmull-Clark, suaviza) + niveles viewport/render (enteros 0..6)
    propSubSimple = new PropBool("Simple"); propSubSimple->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propSubSimple);
    propSubLevel = new PropFloat("Levels Viewport"); propSubLevel->onChange = AccionModParamChanged;
    propSubLevel->SetRango(0.0f, 6.0f); propSubLevel->entero = true; propModifierProps->properties.push_back(propSubLevel);
    propSubRender = new PropFloat("Render"); propSubRender->onChange = AccionModParamChanged;
    propSubRender->SetRango(0.0f, 6.0f); propSubRender->entero = true; propModifierProps->properties.push_back(propSubRender);
    // Screw: angle (grados), screw (subida por el eje), steps viewport/render, eje (dropdown), stretch U/V (UV)
    propScrewAngle = new PropFloat("Angle", "\xc2\xb0"); propScrewAngle->onChange = AccionModParamChanged;
    propScrewAngle->SetRango(-3600.0f, 3600.0f); propModifierProps->properties.push_back(propScrewAngle);
    propScrewHeight = new PropFloat("Screw", "m"); propScrewHeight->onChange = AccionModParamChanged;
    propScrewHeight->SetRango(-1000.0f, 1000.0f); propModifierProps->properties.push_back(propScrewHeight);
    propScrewAxis = new PropButton("Axis"); propScrewAxis->button->desplegable = true; propScrewAxis->action = AccionMenuScrewAxis;
    propModifierProps->properties.push_back(propScrewAxis);
    propScrewSteps = new PropFloat("Steps Viewport"); propScrewSteps->onChange = AccionModParamChanged;
    propScrewSteps->SetRango(2.0f, 512.0f); propScrewSteps->entero = true; propModifierProps->properties.push_back(propScrewSteps);
    propScrewRender = new PropFloat("Render"); propScrewRender->onChange = AccionModParamChanged;
    propScrewRender->SetRango(2.0f, 512.0f); propScrewRender->entero = true; propModifierProps->properties.push_back(propScrewRender);
    propScrewStretchU = new PropBool("Stretch U"); propScrewStretchU->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewStretchU);
    propScrewStretchV = new PropBool("Stretch V"); propScrewStretchV->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewStretchV);
    propScrewSmooth = new PropBool("Smooth"); propScrewSmooth->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewSmooth);
    propScrewMerge = new PropBool("Merge"); propScrewMerge->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewMerge);
    propScrewFlip = new PropBool("Flip Normals"); propScrewFlip->onChange = AccionModParamChanged; propModifierProps->properties.push_back(propScrewFlip);
    // Apply Modifier (cualquier modificador): hornea la malla generada en la editable
    propBtnApplyMod = new PropButton("Apply Modifier");
    propBtnApplyMod->action = AccionAplicarModificador;
    propModifierProps->properties.push_back(propBtnApplyMod);
    GroupProperties.push_back(propModifierProps);
}


// rebindea las propiedades del material al MESH PART seleccionado en la
// lista (antes siempre era el [0], y "Texture" apuntaba a transparent)

// arrastre del borde inferior de la lista de mesh parts (cambia filasMax)
static bool gListaResize = false;
static int gListaResizeY0 = 0;
static int gListaFilas0 = 3;

// arrastre de un PropFloat con el mouse: click + mover horizontal acumula el
// delta 'dx' en el valor (como en Blender). NULL = no se esta arrastrando.
static PropFloat* gFloatDrag = NULL;

// el rebind global opera sobre el panel con el que se interactuo
void RebindMaterialMeshPart(){
    if (PropsActivo) PropsActivo->Rebind();
}

void Properties::Rebind(){
    if (!propMeshParts || !propMaterial || !ObjActivo) return;
    if (ObjActivo->getType() != ObjectType::mesh) return;
    PropListMeshParts* lista = static_cast<PropListMeshParts*>(propMeshParts->properties[0]);
    Mesh* mesh = lista->mesh;
    if (!mesh || mesh->materialsGroup.empty()) return;
    int idx = lista->selectIndex;
    if (idx < 0 || idx >= (int)mesh->materialsGroup.size()) idx = 0;
    Material* material = mesh->materialsGroup[idx].material;

    // Tarjeta "Material" (propMaterial): [0] New Material, [1..3] aviso default, [4] textura,
    // [5] Filtering, [6..12] checkboxes, [13..15] colores, [16] Shininess. El material POR DEFECTO
    // no se edita: se ocultan sus filas (value=NULL) y se muestra el aviso.
    bool esDefault = (!material || material == MaterialDefecto);
    if (propMsgDefault) propMsgDefault->oculto = !esDefault; // aviso: SOLO con el material por defecto
    if (propSepMat)     propSepMat->oculto     = esDefault;  // separador: OCULTO con el default (sino linea huerfana)
    if (propBtnRenameMat) propBtnRenameMat->oculto = esDefault; // el material por defecto NO se renombra
    if (propBtnTextura) {
        propBtnTextura->oculto = esDefault;
        propBtnTextura->button->text =
            NombreDeTextura(material ? material->texture : NULL);
    }
    // "Delete" (botones[0] de la fila Delete|Rename) solo si hay >1 parte. Aca (Rebind se llama tras
    // Add/Delete via RebindMaterialMeshPart) se actualiza apenas cambia la cantidad de partes.
    if (propRowDelRen && !propRowDelRen->botones.empty())
        propRowDelRen->botones[0]->visible = (mesh->materialsGroup.size() > 1);
    // "Move Up/Down": toda la fila solo si hay >1 parte (reordenar = orden de dibujado)
    if (propRowPartMove && propRowPartMove->botones.size() >= 2) {
        bool m2 = (mesh->materialsGroup.size() > 1);
        propRowPartMove->botones[0]->visible = m2;
        propRowPartMove->botones[1]->visible = m2;
    }
    // props del material por MEMBER (index-independiente: reordenar la tarjeta no rompe nada)
    propMatChk[0]->value = (!esDefault && material->texture) ? &material->filtrado : NULL; // Filtering
    propMatChk[1]->value = esDefault ? NULL : &material->transparent;
    propMatChk[2]->value = esDefault ? NULL : &material->vertexColor;
    propMatChk[3]->value = esDefault ? NULL : &material->lighting;
    propMatChk[4]->value = esDefault ? NULL : &material->repeat;
    propMatChk[5]->value = esDefault ? NULL : &material->culling;
    propMatChk[6]->value = esDefault ? NULL : &material->depth_test;
    propMatChk[7]->value = NULL; // Smooth Shading se quito del material (el shading lo dan las normales de la malla)
    // "Reflection" (chrome) se OCULTA cuando hay Normal Mapping (excluyentes: mismo combiner).
    propMatChk[8]->value = (esDefault || material->normalMap) ? NULL : &material->chrome; // Reflection on/off
    propMatChk[9]->value = NULL; // viejo "Chrome 360": SIEMPRE oculto -> lo reemplaza el dropdown propBtnReflectMode
    propMatChk[10]->value = esDefault ? NULL : &material->normalMap; // NORMAL MAPPING (DOT3)
    // dropdown del MODO de Reflection: visible SOLO si Reflection esta tildado (y sin normal map); muestra el modo.
    if (propBtnReflectMode) {
        propBtnReflectMode->oculto = (esDefault || material->normalMap || !material->chrome);
        propBtnReflectMode->button->text = material ? NombreReflectMode(material->reflectMode)
                                                    : std::string("Matcap (hardware)");
    }
    // selector de la textura del normal map: visible SOLO si Normal Mapping esta tildado; muestra su nombre
    if (propBtnNormalTex) {
        propBtnNormalTex->oculto = (esDefault || !material->normalMap);
        // GUARD material!=NULL (CRASH N95): el cubo de escena fresca (W3dNewSceneInit) tiene material==NULL
        // -> esDefault, pero ESTA linea derefenciaba material-> sin chequear (todo el resto del rebind SI guardea
        //    material NULL, ej. linea de propBtnTextura). En PC no se veia porque autocargaba una escena con material real.
        propBtnNormalTex->button->text = (material && material->normalTexture)
            ? NombreDeTextura(material->normalTexture) : std::string("No Normal Map");
    }
    propMatCol[0]->value = esDefault ? NULL : material->diffuse;
    propMatCol[1]->value = esDefault ? NULL : material->specular;
    propMatCol[2]->value = esDefault ? NULL : material->emission;
    propMatShin->value   = esDefault ? NULL : &material->shininess;

    // el selector muestra el material actual del mesh part
    if (propBtnNewMaterial) {
        propBtnNewMaterial->button->text =
            material ? material->name : std::string("Default Material");
    }
    PropertiesLayoutDirty = true; // el alto de la tarjeta pudo cambiar
}

void Properties::RefreshPropMeshParts(){
    if (ObjActivo->getType() != ObjectType::mesh){
        propMeshParts->visible = false;
        if (propMaterial) propMaterial->visible = false;
        static_cast<PropListMeshParts*>(propMeshParts->properties[0])->mesh = NULL;
        return;
    }

    propMeshParts->visible = true;
    if (propMaterial) propMaterial->visible = true;
    Mesh* mesh = static_cast<Mesh*>(ObjActivo);
    static_cast<PropListMeshParts*>(propMeshParts->properties[0])->mesh = mesh;
    static_cast<PropListMeshParts*>(propMeshParts->properties[0])->selectIndex = 0;

    if (mesh->materialsGroup.empty()) return;

    // "Delete" (botones[0] de la fila Delete|Rename) solo si hay MAS de 1 parte (no se borra la unica)
    if (propRowDelRen && !propRowDelRen->botones.empty())
        propRowDelRen->botones[0]->visible = (mesh->materialsGroup.size() > 1);
    // "Move Up/Down": la fila entera solo si hay >1 parte (el orden = orden de dibujado)
    if (propRowPartMove && propRowPartMove->botones.size() >= 2) {
        bool m2 = (mesh->materialsGroup.size() > 1);
        propRowPartMove->botones[0]->visible = m2;
        propRowPartMove->botones[1]->visible = m2;
    }

    Rebind();
    return;

    // (codigo viejo inalcanzable abajo: se poda en la proxima pasada)
    MaterialGroup& mg = mesh->materialsGroup[0];
    if (!mg.material) return;
    Material* material = mg.material;

    static_cast<PropBool*>(propMeshParts->properties[1])->value = &material->transparent;
    static_cast<PropBool*>(propMeshParts->properties[2])->value = &material->transparent;
    static_cast<PropBool*>(propMeshParts->properties[3])->value = &material->vertexColor;
    static_cast<PropBool*>(propMeshParts->properties[4])->value = &material->lighting;
    static_cast<PropBool*>(propMeshParts->properties[5])->value = &material->repeat;
    static_cast<PropBool*>(propMeshParts->properties[6])->value = &material->culling;
    static_cast<PropBool*>(propMeshParts->properties[7])->value = &material->depth_test;

    static_cast<PropColor*>(propMeshParts->properties[8])->value = material->diffuse;
    static_cast<PropColor*>(propMeshParts->properties[9])->value = material->specular;
    static_cast<PropColor*>(propMeshParts->properties[10])->value = material->emission;

    /*GLfloat diffuse[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat specular[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};*/

    /*
    bool culling = true;
    bool uv8bit = false;
    int interpolacion = 0;
    Texture* texture = NULL;
    GLfloat diffuse[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat specular[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    std::string name = "";*/
};

// el PropColor que tiene el picker abierto (borde verde en su fila)
static PropColor* gColorAbierto = NULL;
// posicion en pantalla de la fila seleccionada por TECLADO (para abrir el ColorPicker con OK/Enter)
static int gColorSelSx = 0, gColorSelSy = 0;

void Properties::RefreshTargetProperties(){
    // al cerrarse el picker, la fila del color pierde el borde verde
    if (gColorAbierto && !PopUpActive) {
        gColorAbierto->editando = false;
        gColorAbierto = NULL;
        // FIX: el picker se cerro -> DESBLOQUEAR la nav de propiedades. EnterPropertieSelect dejaba
        // editando=true + ViewPortClickDown=true (el picker es modal) y nadie los reseteaba al cerrar ->
        // las propiedades quedaban "trabadas" (el color seguia activo). Pedido Dante.
        if (PropsActivo) PropsActivo->editando = false;
        ViewPortClickDown = false;
    }
    // (El band-aid per-frame de "Chrome 360" se SACO: ahora cada checkbox de material lleva onChange=RebindMaterial
    //  -> togglear Chrome/Normal Mapping re-arma la tarjeta al instante por CUALQUIER camino (click PC o teclado
    //  Symbian). Ver ConstruirGrupos. Las visibilidades dependientes ya no necesitan un parche por-frame por-prop.)
    if (PropertiesLayoutDirty) {
        PropertiesLayoutDirty = false;
        Resize(width, height); // tambien recalcula la scrollbar
    }
    if (!ObjActivo) {
        if (target) {
            target = NULL;
            // soltar el mesh de la lista de partes: quedaba un puntero
            // al mesh BORRADO y el proximo click/resize crasheaba
            if (propMeshParts && !propMeshParts->properties.empty()) {
                static_cast<PropListMeshParts*>(
                    propMeshParts->properties[0])->mesh = NULL;
                propMeshParts->visible = false;
            }
            Resize(width, height);
        }
        return;
    }
    if (ObjActivo == target) return;
    target = ObjActivo;

    //posicion
    static_cast<PropFloat*>(propTransform->properties[0])->value = &ObjActivo->pos.x;
    static_cast<PropFloat*>(propTransform->properties[1])->value = &ObjActivo->pos.z;
    static_cast<PropFloat*>(propTransform->properties[2])->value = &ObjActivo->pos.y;

    //rotacion: el MODO decide que campos se muestran y a que apuntan
    ObjActivo->ActualizarDisplayRot(); // display fresco desde el quaternion
    int rm = ObjActivo->rotMode;
    propRotMode->button->text = (rm == RotQuaternion) ? "Quaternion (WXYZ)"
                              : (rm == RotAxisAngle)  ? "Axis Angle" : "XYZ Euler";
    PropFloat* pw = static_cast<PropFloat*>(propTransform->properties[5]); // W
    PropFloat* px = static_cast<PropFloat*>(propTransform->properties[6]); // X
    PropFloat* py = static_cast<PropFloat*>(propTransform->properties[7]); // Y
    PropFloat* pz = static_cast<PropFloat*>(propTransform->properties[8]); // Z
    if (rm == RotQuaternion){
        pw->name = "Rotation W"; pw->value = &ObjActivo->rot.w; pw->unit = "";
        px->name = "X"; px->value = &ObjActivo->rot.x; px->unit = "";
        py->value = &ObjActivo->rot.y; py->unit = "";
        pz->value = &ObjActivo->rot.z; pz->unit = "";
    } else if (rm == RotAxisAngle){
        pw->name = "Rotation W"; pw->value = &ObjActivo->rotAngle; pw->unit = "°";
        px->name = "X"; px->value = &ObjActivo->rotAxis.x; px->unit = "";
        py->value = &ObjActivo->rotAxis.y; py->unit = "";
        pz->value = &ObjActivo->rotAxis.z; pz->unit = "";
    } else { // XYZ Euler
        pw->value = NULL; // oculto (Resize devuelve 0; el teclado lo saltea)
        px->name = "Rotation X"; px->value = &ObjActivo->rotEuler.x; px->unit = "°";
        py->value = &ObjActivo->rotEuler.y; py->unit = "°";
        pz->value = &ObjActivo->rotEuler.z; pz->unit = "°";
    }

    //escala (indices corridos +2 por el Mode y el W)
    static_cast<PropFloat*>(propTransform->properties[10])->value = &ObjActivo->scale.x;
    static_cast<PropFloat*>(propTransform->properties[11])->value = &ObjActivo->scale.y;
    static_cast<PropFloat*>(propTransform->properties[12])->value = &ObjActivo->scale.z;

    //Mesh Parts
    RefreshPropMeshParts();

    // LUZ: bindea TODAS las propiedades por member (NULL si el activo no es luz -> no editable). Guard contra
    // punteros sin construir (propLightDir == el primero de los nuevos) -> nunca deref de basura.
    if (propLight && propLightDir){
        bool esLuz = ObjActivo->getType() == ObjectType::light;
        Light* l = esLuz ? static_cast<Light*>(ObjActivo) : NULL;
        propLightDir->value      = l ? &l->direccional   : NULL;
        if (l) g_lightGLIdx = (float)(l->LightID - GL_LIGHT0);
        propLightGL->value       = l ? &g_lightGLIdx     : NULL;
        propLightDiffuse->value  = l ? l->diffuse        : NULL;
        propLightAmbient->value  = l ? l->ambient        : NULL;
        propLightSpecular->value = l ? l->specular       : NULL;
        propLightAttC->value     = l ? &l->attConstant   : NULL;
        propLightAttL->value     = l ? &l->attLinear     : NULL;
        propLightAttQ->value     = l ? &l->attQuadratic  : NULL;
        propLightSpotCut->value  = l ? &l->spotCutoff    : NULL;
        propLightSpotExp->value  = l ? &l->spotExponent  : NULL;
    }

    Resize(width, height);
}

// Constructor
Properties::Properties() : ViewportBase() {
    // (eran inicializadores de clase: C++03)
    target = NULL;
    maxPixelsTitle = 1920;
    selectIndex = 0;
    editando = false;
    propTransform = NULL;
    propMeshParts = NULL;
    propLight = NULL;
    propCamera = NULL;
    propInstance = NULL;
    propBtnCamTarget = NULL;
    propBtnInstTarget = NULL;
    propBtnNewMaterial = NULL;
    propBtnTextura = NULL;
    propBtnNormalTex = NULL; // (faltaba: normal map UI)
    propBtnReflectMode = NULL; // dropdown del modo de Reflection
    // luz: punteros nuevos a NULL (si no se inicializan quedan BASURA y el rebind crashea antes de ConstruirGrupos)
    propLightDir = NULL; propLightGL = NULL; propLightDiffuse = NULL; propLightAmbient = NULL; propLightSpecular = NULL;
    propLightAttC = NULL; propLightAttL = NULL; propLightAttQ = NULL; propLightSpotCut = NULL; propLightSpotExp = NULL;
    propUVMaps = NULL; propColorLayers = NULL; propVertexAnim = NULL; propModifiers = NULL;
    propListModifiers = NULL; propRowMod = NULL; propRowModMove = NULL; propModifierProps = NULL;
    propModVerViewport = NULL; propModVerEdit = NULL;
    propModVacio = NULL; propMirX = NULL; propMirY = NULL; propMirZ = NULL; propMirTarget = NULL;
    propMirMerge = NULL; propMirDist = NULL; propMirClip = NULL; propBtnApplyMod = NULL;
    propSubSimple = NULL; propSubLevel = NULL; propSubRender = NULL;
    propScrewAngle = NULL; propScrewHeight = NULL; propScrewSteps = NULL; propScrewRender = NULL;
    propScrewAxis = NULL; propScrewStretchU = NULL; propScrewStretchV = NULL;
    propScrewSmooth = NULL; propScrewMerge = NULL; propScrewFlip = NULL;
    propListUV = NULL; propListColor = NULL; propBtnColorMode = NULL;
    propRotMode = NULL;
    propMsgDefault = NULL; propSepMat = NULL;
    propMaterial = NULL; propBtnRenameMat = NULL;
    propBtnRenameUV = NULL; propBtnRenameColor = NULL; propBtnRenameObj = NULL;
    propRowPartOps = NULL; propRowDelRen = NULL; propRowPartMove = NULL;
    for (int i = 0; i < 10; i++) propMatChk[i] = NULL;
    for (int i = 0; i < 3; i++) propMatCol[i] = NULL;
    propMatShin = NULL;
    pestaniaActiva = 1;      // arranca en "Objeto" (transforms); 0 = Render
    exportSelectedOnly = false;
    exportApplyModifiers = true;  // por defecto ON (como Blender)
    exportApplyTransforms = true; // por defecto ON
    exportLastObj = NULL;
    focoEnTabs = false;
    ConstruirGrupos(); // grupos PROPIOS: panel independiente
    BarCrear();
    // pestania 0: "Render" (icono camara: export); 1: "Objeto" (transforms);
    // 2: contextual (Mesh/Light/Camera/Instance, solo segun el objeto activo)
    Tab* tRender = new Tab("", IconType::camera);
    BarTabs.push_back(tRender);
    Tab* tObj = new Tab("", IconType::object);
    tObj->activa = true;
    BarTabs.push_back(tObj);
    Tab* tMesh = new Tab("", IconType::material);
    BarTabs.push_back(tMesh);
    // pestania 3: "Vertices" (icono mesh): UV Maps + capas de color (SOLO meshes)
    Tab* tVerts = new Tab("", IconType::mesh);
    BarTabs.push_back(tVerts);
    // pestania 4: "Modifiers" (icono llave 95,1): tarjeta Modifiers (SOLO meshes)
    Tab* tMods = new Tab("", IconType::modificador);
    BarTabs.push_back(tMods);
}

// segun el objeto activo y la pestania elegida: que tab se ve, cual esta
// activa, y que grupo de propiedades se muestra
void Properties::ActualizarPestanias(){
    // la 1ra pestania ("Objeto") siempre esta (transforms). La 2da depende del
    // tipo del objeto activo: Mesh -> mesh parts (icono material); Light ->
    // color (icono luz). (Camara / objetos especiales: a futuro.)
    // pestanias: 0 = Render (export), 1 = Objeto (transforms), 2 = contextual
    int tipo = ObjActivo ? (int)ObjActivo->getType() : -1;
    bool esMesh = (tipo == (int)ObjectType::mesh);
    bool esLuz  = (tipo == (int)ObjectType::light);
    bool esCam  = (tipo == (int)ObjectType::camera);
    bool esInst = (tipo == (int)ObjectType::instance);
    bool hayTab3 = esMesh || esLuz || esCam || esInst; // 3ra pestania (contextual)

    if (BarTabs.size() >= 3){
        BarTabs[2]->visible = hayTab3;
        int icono = (int)IconType::material;          // mesh
        if (esLuz)       icono = (int)IconType::light;
        else if (esCam)  icono = (int)IconType::camera;
        else if (esInst) icono = (int)IconoDeObjeto(ObjActivo); // instance/array/mirror
        BarTabs[2]->icon = icono;
    }
    if (BarTabs.size() >= 4) BarTabs[3]->visible = esMesh; // pestaña Vertices: SOLO meshes
    if (BarTabs.size() >= 5) BarTabs[4]->visible = esMesh; // pestaña Modifiers: SOLO meshes
    if (pestaniaActiva == 2 && !hayTab3) pestaniaActiva = 1;
    if (pestaniaActiva == 3 && !esMesh)  pestaniaActiva = 1;
    if (pestaniaActiva == 4 && !esMesh)  pestaniaActiva = 1;
    for (size_t i = 0; i < BarTabs.size(); i++){
        BarTabs[i]->activa = ((int)i == pestaniaActiva);
        BarTabs[i]->foco   = (focoEnTabs && (int)i == pestaniaActiva);
    }

    // nombre del export por defecto = nombre del objeto activo (al cambiar de objeto)
    if (propExportName && ObjActivo && ObjActivo != exportLastObj){
        // Symbian: por defecto guardar en la RAIZ de E: (memoria, accesible por el usuario).
        // En PC el path arranca relativo (el usuario navega con "Browse folder...").
#ifdef W3D_SYMBIAN
        propExportName->field.SetText(std::string("E:\\") + ObjActivo->name + ".obj");
#else
        propExportName->field.SetText(ObjActivo->name + ".obj");
#endif
        exportLastObj = ObjActivo;
    }

    // mostrar SOLO los grupos de la pestania activa
    if (propRender)    propRender->visible    = (pestaniaActiva == 0);
    if (propExport)    propExport->visible    = (pestaniaActiva == 0);
    if (propTransform) propTransform->visible = (pestaniaActiva == 1);
    if (propMeshParts) propMeshParts->visible = (pestaniaActiva == 2 && esMesh);
    if (propMaterial)  propMaterial->visible  = (pestaniaActiva == 2 && esMesh);
    if (propLight)     propLight->visible     = (pestaniaActiva == 2 && esLuz);
    if (propCamera)    propCamera->visible    = (pestaniaActiva == 2 && esCam);
    if (propInstance)  propInstance->visible  = (pestaniaActiva == 2 && esInst);
    bool vertTab = (pestaniaActiva == 3 && esMesh);
    if (propUVMaps)      propUVMaps->visible      = vertTab;
    if (propColorLayers) propColorLayers->visible = vertTab;
    if (propVertexAnim)  propVertexAnim->visible  = vertTab;
    // pestaña Modifiers: card del stack + una 2da card con las props del modificador seleccionado (vacia).
    bool modsTab = (pestaniaActiva == 4 && esMesh);
    if (propModifiers) propModifiers->visible = modsTab;
    if (modsTab) {
        Mesh* mm = (Mesh*)ObjActivo;
        if (propListModifiers) propListModifiers->mesh = mm;   // el selector sigue a la malla activa
        int nm = (int)mm->modificadores.size();
        if (propRowMod && propRowMod->botones.size() >= 2)
            propRowMod->botones[1]->visible = (nm >= 1);        // Remove: solo si hay 1+ (Dante)
        if (propRowModMove && propRowModMove->botones.size() >= 2) {
            bool hay2 = (nm >= 2);                              // Move Up/Down: solo si hay 2+ (el orden importa con 2)
            propRowModMove->botones[0]->visible = hay2;
            propRowModMove->botones[1]->visible = hay2;
        }
        bool haySel = (nm > 0 && mm->modificadorActivo >= 0 && mm->modificadorActivo < nm);
        Modifier* mod = haySel ? mm->modificadores[mm->modificadorActivo] : NULL;
        bool esMirror = (mod && mod->tipo == ModifierType::Mirror);
        bool esSub    = (mod && mod->tipo == ModifierType::SubdivisionSurface);
        bool esScrew  = (mod && mod->tipo == ModifierType::Screw);
        if (propModifierProps) {
            propModifierProps->visible = haySel;               // 2da tarjeta: solo con un modificador seleccionado
            if (mod) propModifierProps->name = mod->nombre;    // titulo = su nombre
        }
        // props del MIRROR: bindeadas al modificador activo (value=NULL las OCULTA -> solo se ven en un Mirror).
        // display toggles: para CUALQUIER modificador seleccionado (no solo Mirror)
        if (propModVerViewport) propModVerViewport->value = haySel ? &mod->mostrarViewport : NULL;
        if (propModVerEdit)     propModVerEdit->value     = haySel ? &mod->mostrarEdit : NULL;
        if (propModVacio) propModVacio->oculto = (esMirror || esSub || esScrew); // "(no properties yet)" solo tipos sin params
        if (propSubSimple) propSubSimple->value = esSub ? &mod->subSimple    : NULL;
        if (propSubLevel)  propSubLevel->value  = esSub ? &mod->subLevel      : NULL;
        if (propSubRender) propSubRender->value = esSub ? &mod->subRenderLevel: NULL;
        // Screw
        if (propScrewAngle)   propScrewAngle->value   = esScrew ? &mod->screwAngle       : NULL;
        if (propScrewHeight)  propScrewHeight->value  = esScrew ? &mod->screwHeight       : NULL;
        if (propScrewSteps)   propScrewSteps->value   = esScrew ? &mod->screwSteps        : NULL;
        if (propScrewRender)  propScrewRender->value  = esScrew ? &mod->screwRenderSteps  : NULL;
        if (propScrewStretchU)propScrewStretchU->value= esScrew ? &mod->screwStretchU     : NULL;
        if (propScrewStretchV)propScrewStretchV->value= esScrew ? &mod->screwStretchV     : NULL;
        if (propScrewSmooth)  propScrewSmooth->value  = esScrew ? &mod->screwSmooth        : NULL;
        if (propScrewMerge)   propScrewMerge->value   = esScrew ? &mod->screwMerge         : NULL;
        if (propScrewFlip)    propScrewFlip->value    = esScrew ? &mod->screwFlip          : NULL;
        if (propScrewAxis){ propScrewAxis->oculto = !esScrew;
            if (esScrew) propScrewAxis->button->text = (mod->screwAxis==0)?"X":(mod->screwAxis==1)?"Y":"Z"; }
        if (propMirX) propMirX->value = esMirror ? &mod->ejeX : NULL;
        if (propMirY) propMirY->value = esMirror ? &mod->ejeY : NULL;
        if (propMirZ) propMirZ->value = esMirror ? &mod->ejeZ : NULL;
        if (propMirMerge) propMirMerge->value = esMirror ? &mod->merge : NULL;
        if (propMirDist)  propMirDist->value  = esMirror ? &mod->mergeDist : NULL;
        if (propMirClip)  propMirClip->value  = esMirror ? &mod->clipping : NULL;
        if (propMirTarget) { propMirTarget->oculto = !esMirror;
            if (esMirror) propMirTarget->button->text = mod->target ? mod->target->name : std::string("None"); }
        if (propBtnApplyMod) propBtnApplyMod->oculto = !haySel; // Apply: con cualquier modificador seleccionado
    } else if (propModifierProps) propModifierProps->visible = false;

    // pestaña Vertices activa: las listas siguen a la malla activa (modo 1=uvmaps, 2=colors) + el toggle
    if (vertTab) {
        Mesh* mv = (Mesh*)ObjActivo;
        if (mv->uvMaps.empty() || mv->colorLayers.empty()) mv->PoblarCapas(); // crea la 1ra si falta
        if (propListUV)    propListUV->mesh    = mv;
        if (propListColor) propListColor->mesh = mv;
        if (propBtnColorMode && mv->colorActivo >= 0 && mv->colorActivo < (int)mv->colorLayers.size())
            propBtnColorMode->button->text =
                mv->colorLayers[mv->colorActivo]->porVertice ? "Per-Vertex" : "Per-Corner";
    }

    // el boton de target muestra el objeto apuntado (se actualiza cada frame
    // para reflejar el cambio al elegirlo del desplegable)
    if (esCam || esInst){
        Target* tgt = ObjComoTarget(ObjActivo);
        PropButton* btn = esCam ? propBtnCamTarget : propBtnInstTarget;
        if (tgt && btn)
            btn->button->text = tgt->target ? tgt->target->name : std::string("None");
    }
}

void Properties::ClickTab(int mx, int my){
    for (size_t i = 0; i < BarTabs.size(); i++){
        if (BarTabs[i]->visible && BarTabs[i]->Contains(mx, my)){
            g_textFieldActivo = NULL; // cambiar de pestania des-enfoca el texto
            pestaniaActiva = (int)i;
            focoEnTabs = false; // con mouse la activa va blanca (no verde)
            ActualizarPestanias();
            Resize(width, height); // re-layout (scroll del nuevo grupo)
            return;
        }
    }
}

void Properties::Resize(int newW, int newH){
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);
    ActualizarPestanias(); // visibilidad de grupos antes de medir el contenido

    if (!ObjActivo) {
        // sin objeto: sin contenido ni scrollbar (antes quedaba la barra
        // con el tamano viejo)
        PosY = 0;
        ResizeScrollbar(newW, newH, 0, 0, BarTopOffset());
        return;
    }

    // la barra de scroll solo necesita su ancho (4px) + un respiro
    int WidthCard = width - bordersGS - gapGS
        - (scrollY ? GlobalScale*8 : 0); // la reserva de la barra
    // (incluso GRANDE) solo cuando la barra existe
    int heightCard = borderGS + borderGS + borderGS + (RenglonHeightGS + gapGS)*10;
    maxPixelsTitle = WidthCard - IconSizeGS - gapGS;

    for (size_t i=0; i < GroupProperties.size(); i++){
        GroupProperties[i]->Resize(WidthCard, heightCard);
    }

    // alto REAL del contenido (antes era -2000 hardcodeado: el scroll
    // vertical se calculaba mal, tambien en PC)
    int contenidoH = borderGS + RenglonHeightGS + gapGS; // titulo
    for (size_t i=0; i < GroupProperties.size(); i++){
        if (GroupProperties[i]->visible){
            // mismo paso que el render (y que ClickEn/CentrarSeleccion)
            contenidoH += GroupProperties[i]->height + borderGS
                          + (GroupProperties[i]->open ? GlobalScale : 0);
        }
    }
    contenidoH += marginGS; // respiro abajo (la barra va por topOffset)
    ResizeScrollbar(newW, newH, 0, -contenidoH, BarTopOffset());
}

void Properties::Render(){
    if (!leftMouseDown) UndoMaterialModCommit(); // Ctrl+Z: al soltar el mouse, pushea el cambio de material (si difiere)
    RefreshTargetProperties();
    ActualizarPestanias(); // que grupo mostrar segun la pestania (Objeto/Mesh)

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // Limpiar pantalla
    w3dEngine::Enable(w3dEngine::ScissorTest);
    const int glY = W3dPantallaAlto - y - height; // arbol arriba-izq -> GL
    w3dEngine::Scissor(x, glY, width, height); // igual a tu viewport
    w3dEngine::ClearColor(
        ListaColores[static_cast<int>(ColorID::background)][0],
        ListaColores[static_cast<int>(ColorID::background)][1],
        ListaColores[static_cast<int>(ColorID::background)][2],
        ListaColores[static_cast<int>(ColorID::background)][3]
    );

    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

    w3dEngine::Viewport(x, glY, width, height); // x, y, ancho, alto
    w3dEngine::Ortho(0, width, height, 0, -1, 1);

    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::ColorMaterial);

    w3dEngine::BindTexture(Textures[0]->iID);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
#ifndef W3D_SYMBIAN
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif

    if (ObjActivo){
        // los GRUPOS son globales y otro panel de propiedades pudo
        // haberlos acomodado con OTRO ancho: relayout con el propio
        // antes de dibujar (mitigacion hasta hacerlos por-instancia)
        {
            int WidthCard = width - bordersGS - gapGS
        - (scrollY ? GlobalScale*8 : 0); // la reserva de la barra
    // (incluso GRANDE) solo cuando la barra existe
            int heightCard = borderGS * 3 + (RenglonHeightGS + gapGS) * 10;
            for (size_t i = 0; i < GroupProperties.size(); i++){
                GroupProperties[i]->Resize(WidthCard, heightCard);
            }
        }
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(PosX + borderGS, PosY + borderGS + BarTopOffset(), 0);

        DibujarTitulo(ObjActivo, maxPixelsTitle);

        //render de los grupos de propiedades, con CULLING: el grupo que
        //queda completo fuera del viewport no se dibuja (solo se avanza
        //el cursor lo mismo que avanzaria su Render)
        int yLocal = PosY + borderGS + BarTopOffset() + RenglonHeightGS + gapGS;
        for (size_t i=0; i < GroupProperties.size(); i++){
            GroupPropertie* g = GroupProperties[i];
            if (!g->visible) continue;
            int paso = g->height + borderGS + (g->open ? GlobalScale : 0);
            if (yLocal + paso < 0 || yLocal > height) {
                w3dEngine::Translatef(0, (GLfloat)paso, 0); // fuera: solo avanzar
            } else {
                g->Render();
            }
            yLocal += paso;
        }

        //si es lampara
        /*if (ObjActivo->getType() == ObjectType::light){
            Light* light = static_cast<Light*>(ObjActivo);

            w3dEngine::Translatef(0, RenglonHeightGS + marginGS, 0);
            DibujarPropiedadFloat(" Ambient R ", light->ambient[0]);

            w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         G ", light->ambient[1]);

            w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         B ", light->ambient[2]);

            w3dEngine::Translatef(0, RenglonHeightGS + marginGS, 0);
            DibujarPropiedadFloat(" Diffuse R ", light->diffuse[0]);

            w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         G ", light->diffuse[1]);

            w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         B ", light->diffuse[2]);

            w3dEngine::Translatef(0, RenglonHeightGS + marginGS, 0);
            DibujarPropiedadFloat("Specular R ", light->specular[0]);

            w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         G ", light->specular[1]);

            w3dEngine::Translatef(0, RenglonHeightGS + gapGS, 0);
            DibujarPropiedadFloat("         B ", light->specular[2]);
        }

        //si es camara
        if (ObjActivo->getType() == ObjectType::camera){
            Camera* camera = static_cast<Camera*>(ObjActivo);

            w3dEngine::Translatef(0, RenglonHeightGS + marginGS, 0);
            if (camera->Riel){
                DibujarPropiedadFloat("Tiene Riel! ", 0.0f);

                w3dEngine::Translatef(0, RenglonHeightGS + marginGS, 0);
                DibujarPropiedadFloat("Offset Riel: ", camera->offsetRiel);
            }
            else {
                DibujarPropiedadFloat("No tiene Riel ", 0.0f);
            }
        }

        //si es camara
        if (ObjActivo->getType() == ObjectType::mesh){
            Mesh* mesh = static_cast<Mesh*>(ObjActivo);

            w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::blanco)][0],
                    ListaColores[static_cast<int>(ColorID::blanco)][1],
                    ListaColores[static_cast<int>(ColorID::blanco)][2], 1.0f);

            w3dEngine::Translatef(0, RenglonHeightGS + marginGS, 0);
            CardTitulo(
                IconsUV[static_cast<size_t>(IconType::arrow)]->uvs,
                "Materiales"
            );

            w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::grisUI)][0],
                    ListaColores[static_cast<int>(ColorID::grisUI)][1],
                    ListaColores[static_cast<int>(ColorID::grisUI)][2], 1.0f);

            for (size_t i = 0; i < mesh->materialsGroup.size(); ++i){
                Material* mat = mesh->materialsGroup[i].material;
                w3dEngine::Translatef(0, RenglonHeightGS + marginGS, 0);
                DibujarPropiedadInt(mat->name, mesh->materialsGroup.size());
            }
        }*/

        w3dEngine::PopMatrix();
    }

    //w3dEngine::Disable(w3dEngine::ScissorTest);
    RenderBar();
    DibujarBordes(this);
    DibujarScrollbar(this);
    w3dEngine::Disable(w3dEngine::ScissorTest);
}

void Properties::CambiarTab(int dir){
    // DINAMICO: avanza al SIGUIENTE tab VISIBLE en la direccion 'dir', saltando los ocultos y envolviendo.
    // (Antes usaba n=hayTab3?3:2 -> nunca llegaba a Vertices/Modifiers por teclado: solo con el mouse.)
    int n = (int)BarTabs.size();
    if (n <= 0) return;
    for (int k = 0; k < n; k++){
        pestaniaActiva = (pestaniaActiva + dir + n) % n;
        if (BarTabs[pestaniaActiva]->visible) break; // primer tab visible en esa direccion
    }
    LimpiarSeleccionGrupos();   // la pestaña nueva entra sin nada resaltado
    ActualizarPestanias();      // visibilidad de los grupos de la nueva pestaña
    Resize(width, height);      // RECALCULA el scroll (MaxPosY) del nuevo contenido
}

// pone el foco en el primer grupo VISIBLE de la pestaña actual (al bajar de las
// pestañas a las propiedades). Sin esto el foco quedaba en un grupo invisible
// (ej: transforms cuando estas en Materiales) y la navegacion se rompia.
void Properties::EntrarPrimerGrupoVisible(){
    for (size_t i = 0; i < GroupProperties.size(); i++){
        if (GroupProperties[i]->visible){
            selectIndex = (int)i;
            GroupProperties[i]->selectIndex = -1; // cabecera del grupo
            CentrarSeleccion();
            return;
        }
    }
}

// arriba estando en las pestañas: wrap a la ULTIMA propiedad del ULTIMO grupo visible (simetrico a bajar
// desde la ultima opcion -> pestañas). Pedido Dante.
void Properties::EntrarUltimoGrupoVisible(){
    for (int i = (int)GroupProperties.size() - 1; i >= 0; i--){
        if (GroupProperties[i]->visible){
            selectIndex = i;
            GroupProperties[i]->selectLastIndexProperty(); // ultima propiedad seleccionable del grupo
            CentrarSeleccion();
            return;
        }
    }
}

// nada resaltado en las propiedades (mientras el foco esta en las pestañas)
void Properties::LimpiarSeleccionGrupos(){
    for (size_t i = 0; i < GroupProperties.size(); i++)
        GroupProperties[i]->selectIndex = -2;
}

// la propiedad seleccionada por teclado (NULL si es una cabecera o nada)
static PropertieBase* PropFilaSeleccionada(std::vector<GroupPropertie*>& gps, int selectIndex){
    if (selectIndex < 0 || selectIndex >= (int)gps.size()) return NULL;
    GroupPropertie* g = gps[selectIndex];
    if (g->selectIndex < 0 || g->selectIndex >= (int)g->properties.size()) return NULL;
    return g->properties[g->selectIndex];
}

void Properties::button_left(){
    PropsActivo = this; // este panel pasa a ser el activo
    if (focoEnTabs){ CambiarTab(-1); return; } // en las pestañas: cambiar de pestaña
    if (!editando){
        // si la fila seleccionada es una FILA DE BOTONES, mover entre ellos (NO colapsar la tarjeta)
        PropertieBase* p = PropFilaSeleccionada(GroupProperties, selectIndex);
        if (p && p->GetType() == PropertyType::ButtonRow) { p->button_left(); return; }
        SetOpenGroup(false);
    }
    else {
        GroupProperties[selectIndex]->button_left();
    }
}

void Properties::button_right(){
    PropsActivo = this; // este panel pasa a ser el activo
    if (focoEnTabs){ CambiarTab(+1); return; }
    if (!editando){
        PropertieBase* p = PropFilaSeleccionada(GroupProperties, selectIndex);
        if (p && p->GetType() == PropertyType::ButtonRow) { p->button_right(); return; }
        SetOpenGroup(true);
    }
    else {
        GroupProperties[selectIndex]->button_right();
    }
}

#ifndef W3D_SYMBIAN
void Properties::mouse_button_up(SDL_Event &e){
    gFloatDrag = NULL; // soltar el mouse termina el arrastre de un PropFloat
    if (!editando) ViewPortClickDown = false;
}
#endif

#ifndef W3D_SYMBIAN
void Properties::event_mouse_wheel(SDL_Event &e){
    if (editando) return;
    // si el mouse esta sobre una LISTA (mesh parts / selector), la rueda la scrollea A
    // ELLA (antes solo scrolleaba el panel entero -> el componente "obligaba" al estilo
    // Symbian de Enter+flechas). Reusa el hover ya trackeado (PropHoverGroup/Fila).
    if (PropHoverGroup && PropHoverFila >= 0 && PropHoverFila < (int)PropHoverGroup->properties.size()) {
        PropertieBase* prop = PropHoverGroup->properties[PropHoverFila];
        if (prop->GetType() == PropertyType::List) {
            PropListMeshParts* lst = static_cast<PropListMeshParts*>(prop);
            int n = lst->ListaCount(); // parts / uv maps / colors segun el modo
            int vis = n < lst->filasMax ? n : lst->filasMax;
            if (n > vis) {
                lst->scrollFila -= (e.wheel.y > 0 ? 1 : -1); // rueda arriba = subir
                if (lst->scrollFila > n - vis) lst->scrollFila = n - vis;
                if (lst->scrollFila < 0) lst->scrollFila = 0;
                g_redraw = true;
                return; // consumido por la lista: NO scrollea el panel
            }
        }
    }
    MouseWheel = true;
    ScrollY(e.wheel.y*12*GlobalScale);
    MouseWheel = false;
}
#endif

// apaga el hover de TODOS los botones de fila (no solo los conocidos: si no, el
// hover de los nuevos -Render/Export- quedaba pegado al salir el mouse)
void Properties::ResetButtonHovers(){
    for (size_t i = 0; i < GroupProperties.size(); i++)
        for (size_t j = 0; j < GroupProperties[i]->properties.size(); j++)
            if (GroupProperties[i]->properties[j]->GetType() == PropertyType::Button)
                ((PropButton*)GroupProperties[i]->properties[j])->button->hover = false;
}

void Properties::ClearHover(){
    ResetButtonHovers();
    PropHoverGroup = NULL;
    PropHoverFila = -1;
}

void Properties::FindMouseOver(int mx, int my){
    PropsActivo = this; // este panel pasa a ser el activo
    // hover de FILAS (texto blanco / borde del checkbox) y de los
    // botones de fila; mismo recorrido que ClickEn
    ResetButtonHovers(); // apaga TODOS los botones (luego se prende el de la fila)
    PropHoverGroup = NULL;
    PropHoverFila = -1;
    if (mouseOverScrollY) return; // el "scrollbar area" esta reservada
    if (!ObjActivo || !Contains(mx, my)) return;
    int yCursor = y + BarTopOffset() + PosY + borderGS + RenglonHeightGS + gapGS;
    for (size_t i = 0; i < GroupProperties.size(); i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        int hCabecera = borderGS + RenglonHeightGS + gapGS;
        if (g->open) {
            int yFila = yCursor + hCabecera;
            for (size_t j = 0; j < g->properties.size(); j++) {
                PropertieBase* prop = g->properties[j];
                int hFila = prop->Resize(g->width);
                if (hFila > 0 && prop->GetType() != PropertyType::Gap &&
                    prop->Seleccionable() &&
                    my >= yFila && my < yFila + hFila) {
                    PropHoverGroup = g;
                    PropHoverFila = (int)j;
                    if (prop->GetType() == PropertyType::Button) {
                        int izq = x + PosX + borderGS + borderGS;
                        Button* b2 = ((PropButton*)prop)->button;
                        b2->hover = (mx >= izq && mx < izq + b2->width);
                    }
                    return;
                }
                yFila += hFila;
            }
        }
        yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
}

void Properties::event_mouse_motion(int mx, int my) {
    // arrastre de un PropFloat (posicion/rotacion/escala/shininess): mover el
    // mouse en horizontal cambia el valor. Va ANTES del check de 'editando'.
    if (gFloatDrag) {
        if (!leftMouseDown) { gFloatDrag = NULL; return; }
        ViewPortClickDown = true; // mantiene el viewport activo durante el arrastre
        // 'dx' GLOBAL = delta por evento que YA neutraliza la teletransportacion
        // del cursor (CheckWarpMouseInViewport pone dx=0 al wrappear). Por eso
        // acumulamos el delta en vez de usar la X absoluta (que saltaba).
        gFloatDrag->Set(*gFloatDrag->value + dx * gFloatDrag->dragStep);
        return;
    }

    if (editando) return;

    if (gListaResize) {
        if (!leftMouseDown) {
            gListaResize = false;
        } else if (propMeshParts && !propMeshParts->properties.empty()) {
            // arrastrar el borde inferior: 1..10 filas visibles
            PropListMeshParts* lista =
                (PropListMeshParts*)propMeshParts->properties[0];
            int filas = gListaFilas0 +
                        (my - gListaResizeY0) / (RenglonHeightGS + gapGS);
            if (filas < 1) filas = 1;
            if (filas > 10) filas = 10;
            if (filas != lista->filasMax) {
                lista->filasMax = filas;
                lista->AjustarVentana();
                Resize(width, height);
            }
        }
        return;
    }

    if (middleMouseDown || leftMouseDown) {
        ViewPortClickDown = true;

        ScrollX(dx);
        ScrollY(dy);
        return;
    }
    //si no se esta haciendo click. entonces miras si el mouse esta encima de algo
    else {
        FindMouseOver(mx, my);
    }
}

#ifndef W3D_SYMBIAN
void Properties::event_key_down(SDL_Event &e){
    #if SDL_MAJOR_VERSION == 2
        SDL_Keycode key = e.key.keysym.sym; //SDL2
    #elif SDL_MAJOR_VERSION == 3
        SDL_Keycode key = e.key.key; // SDL3
    #endif
    if (e.key.repeat == 0) {
        switch (key) {
            case SDLK_LEFT:
                button_left();
                break;
            case SDLK_RIGHT:
                button_right();
                break;
            case SDLK_UP:
                button_up();
                break;
            case SDLK_DOWN:
                button_down();
                break;
            case SDLK_RETURN:
                EnterPropertieSelect();
                break;
            case SDLK_ESCAPE:
                Cancel();
                break;
        };
    }
    else {
        // Evento repetido por mantener apretada
        switch (key) {
            case SDLK_LEFT:
                button_left();
                break;
            case SDLK_RIGHT:
                button_right();
                break;
            case SDLK_UP:
                button_up();
                break;
            case SDLK_DOWN:
                button_down();
                break;
        }
    }
}
#endif

// rect en pantalla del boton de la fila seleccionada (para abrir su desplegable
// alineado por teclado). Igual recorrido que CentrarSeleccion + igual cuenta de
// sx que ClickEn (x + PosX + 2*borderGS) y sy = y + BarTop + PosY + offset.
void Properties::SetRectFilaSeleccionada(){
    if (selectIndex < 0 || selectIndex >= (int)GroupProperties.size()) return;
    GroupPropertie* gsel = GroupProperties[selectIndex];
    if (!gsel->open || gsel->selectIndex < 0 ||
        gsel->selectIndex >= (int)gsel->properties.size()) return;
    PropertieBase* prop = gsel->properties[gsel->selectIndex];
    // Button (desplegable alineado) y Color (abrir el picker con teclado) necesitan la posicion
    if (prop->GetType() != PropertyType::Button && prop->GetType() != PropertyType::Color) return;
    int yFila = borderGS + RenglonHeightGS + gapGS; // titulo
    for (int i = 0; i < (int)GroupProperties.size() && i <= selectIndex; i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        if (i == selectIndex) {
            yFila += borderGS + RenglonHeightGS + gapGS; // cabecera del grupo
            for (int j = 0; j < gsel->selectIndex; j++)
                yFila += g->properties[j]->Resize(g->width);
            break;
        }
        yFila += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
    int sxFila = x + PosX + borderGS + borderGS;
    int syFila = y + BarTopOffset() + PosY + yFila;
    if (prop->GetType() == PropertyType::Button) {
        PropButton* pb = (PropButton*)prop;
        pb->button->sx = sxFila;
        pb->button->sy = syFila;
    } else { // Color: guardo la posicion para abrir el ColorPicker desde EnterPropertieSelect
        gColorSelSx = sxFila; gColorSelSy = syFila;
    }
}

void Properties::EnterPropertieSelect(){
    PropsActivo = this; // este panel pasa a ser el activo
    SetRectFilaSeleccionada(); // desplegable alineado al botón / pos de la fila (nav por teclado)
    editando = GroupProperties[selectIndex]->EnterPropertieSelect();
    ViewPortClickDown = editando;
    // OK/Enter sobre un COLOR: abrir el ColorPicker (igual que el click del mouse) -> sin esto el
    // selector de color SOLO se podia abrir con el mouse (Dante). El picker es modal: se lleva el teclado.
    GroupPropertie* g = GroupProperties[selectIndex];
    if (g->selectIndex >= 0 && g->selectIndex < (int)g->properties.size() &&
        g->properties[g->selectIndex]->GetType() == PropertyType::Color) {
        PropColor* pc = (PropColor*)g->properties[g->selectIndex];
        if (pc->value) {
            for (int q = 0; q < 4; q++) pc->originalValue[q] = pc->value[q]; // para Cancel
            pc->editando = true;
            if (!colorPicker) colorPicker = new ColorPicker();
            colorPicker->Abrir(pc->value, gColorSelSx, gColorSelSy);
            gColorAbierto = pc;
            editando = true; ViewPortClickDown = true;
        }
    }
}

void Properties::Cancel(){
    PropsActivo = this; // este panel pasa a ser el activo
    editando = GroupProperties[selectIndex]->Cancel();
    ViewPortClickDown = editando;
};

void Properties::SetOpenGroup(bool open){
    GroupProperties[selectIndex]->open = open;
    if (!open){
        GroupProperties[selectIndex]->selectIndex = -1;
    }
    Resize(width, height);
}

// centra la opcion seleccionada en el viewport (con topes arriba/abajo)
void Properties::CentrarSeleccion(){
    // y de la fila seleccionada (sin PosY): mismo recorrido que ClickEn,
    // con las alturas reales de cada fila (Resize)
    int yFila = borderGS + RenglonHeightGS + gapGS; // titulo
    for (int i = 0; i < (int)GroupProperties.size() && i <= selectIndex; i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        if (i == selectIndex) {
            if (g->open && g->selectIndex >= 0) {
                yFila += borderGS + RenglonHeightGS + gapGS; // cabecera
                for (int j = 0; j < (int)g->properties.size() && j < g->selectIndex; j++) {
                    yFila += g->properties[j]->Resize(g->width);
                }
            }
            break;
        }
        yFila += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
    // centrado en el area VISIBLE (la de abajo de la barra de botones);
    // los topes hacen que en los extremos quede pegado arriba/abajo
    PosY = -(yFila - (height - BarTopOffset()) / 2);
    if (PosY > 0) PosY = 0;
    if (PosY < MaxPosY) PosY = MaxPosY;
}

void Properties::button_up(){
    PropsActivo = this; // este panel pasa a ser el activo
    if (focoEnTabs){ // en las pestañas: ARRIBA hace wrap a la ultima propiedad (simetrico a bajar desde la ultima)
        focoEnTabs = false;
        EntrarUltimoGrupoVisible();
        return;
    }
    if (!editando){
        PrevSelect();                       // en el tope setea focoEnTabs
        if (!focoEnTabs) CentrarSeleccion();
    }
    else {
        GroupProperties[selectIndex]->button_up();
    }
}

void Properties::button_down(){
    PropsActivo = this; // este panel pasa a ser el activo
    if (focoEnTabs){ // bajar = entrar a las propiedades de la pestaña
        focoEnTabs = false;
        EntrarPrimerGrupoVisible(); // al 1er grupo VISIBLE (no a uno oculto)
        return;
    }
    if (!editando){
        NextSelect();
        CentrarSeleccion();
    }
    else {
        GroupProperties[selectIndex]->button_down();
    }
}

void Properties::NextSelect(){
    if (GroupProperties[selectIndex]->NextSelect()){
        // saltar grupos INVISIBLES (una camara no tiene mesh parts:
        // se "navegaban" opciones que no existian)
        for (size_t v = 0; v < GroupProperties.size(); v++){
            selectIndex++;
            if (selectIndex >= static_cast<int>(GroupProperties.size())){
                selectIndex = 0;
            }
            if (GroupProperties[selectIndex]->visible) break;
        }
        GroupProperties[selectIndex]->selectIndex = -1;
    }
}

void Properties::PrevSelect(){
    if (GroupProperties[selectIndex]->PrevSelect()){
        // TOPE: si es el primer grupo VISIBLE de la pestaña (no necesariamente
        // el indice 0: en Materiales el visible es otro), salir a las PESTAÑAS.
        bool hayVisibleAntes = false;
        for (int i = 0; i < selectIndex; i++)
            if (GroupProperties[i]->visible){ hayVisibleAntes = true; break; }
        if (!hayVisibleAntes){ LimpiarSeleccionGrupos(); focoEnTabs = true; return; }
        // saltar grupos INVISIBLES (idem NextSelect)
        for (size_t v = 0; v < GroupProperties.size(); v++){
            selectIndex--;
            if (selectIndex < 0){
                selectIndex = static_cast<int>(GroupProperties.size()) - 1;
            }
            if (GroupProperties[selectIndex]->visible) break;
        }

        if (GroupProperties[selectIndex]->open){
            GroupProperties[selectIndex]->selectLastIndexProperty();
        }
        else {
            GroupProperties[selectIndex]->selectIndex = -1;
        }
    }
}

#ifndef W3D_SYMBIAN
void Properties::event_key_up(SDL_Event &e){
}
#endif

void Properties::ClickEn(int mx, int my) {
    PropsActivo = this; // este panel pasa a ser el activo
    g_textFieldActivo = NULL; // cualquier click des-enfoca; abajo se re-enfoca si es texto
    (void)mx; // el arrastre usa el delta global 'dx', no la X del click
    if (editando) {
        // un click mientras se edita ACEPTA el cambio
        EnterPropertieSelect();
        return;
    }
    if (!ObjActivo) return; // sin objeto no hay filas (y sin crashes)
    // mismo recorrido que el render: el titulo avanza RenglonHeightGS+gapGS
    // (no marginGS) y cada fila mide lo que devuelve su Resize (PropGap es
    // gapGS, checkbox sin valor es 0): antes el mapeo quedaba corrido y el
    // click en "Vertex Color" tocaba "Transparent"
    int yCursor = y + BarTopOffset() + PosY + borderGS + RenglonHeightGS + gapGS;
    for (size_t i = 0; i < GroupProperties.size(); i++) {
        GroupPropertie* g = GroupProperties[i];
        if (!g->visible) continue;
        int hCabecera = borderGS + RenglonHeightGS + gapGS;
        if (my >= yCursor && my < yCursor + hCabecera) {
            // -1 = "cabecera ACTIVA" (se pinta accent): los otros grupos
            // van a -2 o quedaban todos verdes al plegar/desplegar
            for (size_t k = 0; k < GroupProperties.size(); k++)
                GroupProperties[k]->selectIndex = -2;
            selectIndex = (int)i;
            g->selectIndex = -1; // el cursor queda en esta cabecera
            g->open = !g->open; // plegar/desplegar el grupo
            Resize(width, height);
            return;
        }
        if (g->open) {
            int yFila = yCursor + hCabecera;
            for (size_t j = 0; j < g->properties.size(); j++) {
                PropertieBase* prop = g->properties[j];
                int hFila = prop->Resize(g->width); // = alto del render
                if (hFila > 0 && prop->GetType() != PropertyType::Gap &&
                    prop->Seleccionable() &&
                    my >= yFila && my < yFila + hFila) {
                    for (size_t k = 0; k < GroupProperties.size(); k++)
                        GroupProperties[k]->selectIndex = -2; // -1 = cabecera
                    selectIndex = (int)i;
                    g->selectIndex = (int)j;
                    // Ctrl+Z de MODIFICACION de material: si se toca un checkbox o el shininess de la tarjeta
                    // Material, snapshotear el material ANTES (se commitea al soltar el mouse, en Render).
                    if (g == propMaterial && (prop->GetType() == PropertyType::Bool || prop->GetType() == PropertyType::Float))
                        UndoMaterialModIniciar(MaterialActivoUI());
                    if (prop->GetType() == PropertyType::Bool) {
                        prop->EditPropertie(); // checkbox: toggle directo (+ su onChange: los de material re-Rebindean)
                    }
                    else if (prop->GetType() == PropertyType::Color) {
                        PropColor* pc = (PropColor*)prop;
                        if (pc->value) {
                            // selector de color (popup); la fila queda
                            // con BORDE VERDE mientras se edita
                            if (!colorPicker) colorPicker = new ColorPicker();
                            colorPicker->Abrir(pc->value,
                                x + PosX + borderGS + borderGS, yFila);
                            pc->editando = true;
                            gColorAbierto = pc;
                        }
                    }
                    else if (prop->GetType() == PropertyType::Button) {
                        // rect absoluto (los desplegables abren debajo)
                        PropButton* pb = (PropButton*)prop;
                        pb->button->sx = x + PosX + borderGS + borderGS;
                        pb->button->sy = yFila;
                        prop->EditPropertie(); // accion del boton
                    }
                    else if (prop->GetType() == PropertyType::ButtonRow) {
                        // hit-test la CELDA por X (los botones se reparten el ancho en partes iguales)
                        PropButtonRow* row = (PropButtonRow*)prop;
                        int leftX = x + PosX + borderGS + borderGS; // borde izq del cuerpo (igual que el Button)
                        int cw = row->AnchoCelda(g->width);
                        int cx = leftX;
                        for (size_t b = 0; b < row->botones.size(); b++) {
                            if (!row->botones[b]->visible) continue;
                            if (mx >= cx && mx < cx + cw) {
                                // rect ABSOLUTO del boton (igual que el PropButton de arriba): asi un boton de la
                                // fila que abre un DESPLEGABLE (ej. "Add" de modificadores) lo abre JUSTO debajo suyo
                                // y no en una esquina (su sx/sy no se seteaban en el click de la fila -> quedaban stale).
                                row->botones[b]->sx = cx;
                                row->botones[b]->sy = yFila;
                                row->Disparar((int)b);
                                break;
                            }
                            cx += cw + gapGS;
                        }
                    }
                    else if (prop->GetType() == PropertyType::List) {
                        PropListMeshParts* lista = (PropListMeshParts*)prop;
                        if (my >= yFila + hFila - gapGS - borderGS) {
                            // agarre del BORDE INFERIOR: arrastrar cambia
                            // el alto de la lista (1..10 filas)
                            gListaResize = true;
                            gListaResizeY0 = my;
                            gListaFilas0 = lista->filasMax;
                            return;
                        }
                        // item clickeado (la ventana arranca en scrollFila)
                        int item = lista->scrollFila +
                                   (my - yFila - borderGS) / (RenglonHeightGS + gapGS);
                        int n = lista->ListaCount(); // parts / uv maps / colors segun el modo
                        if (item >= n) item = n - 1;
                        if (item >= 0 && n > 0) {
                            lista->ListaSeleccionar(item); // setea el activo + re-bind/re-bake
                            lista->AjustarVentana();
                        }
                    }
                    else if (prop->GetType() == PropertyType::Float) {
                        // arranca el arrastre del valor con el mouse (Enter +
                        // flechas siguen andando igual)
                        PropFloat* pf = (PropFloat*)prop;
                        if (pf->value) gFloatDrag = pf;
                    }
                    else if (prop->GetType() == PropertyType::Text) {
                        prop->EditPropertie(); // ENFOCA la caja: el texto entra por SDL_TEXTINPUT
                    }
                    return;
                }
                yFila += hFila;
            }
        }
        // paso al proximo grupo: igual que el net-translate del render
        yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
    }
}

void Properties::key_down_return(){
    PropsActivo = this; // este panel pasa a ser el activo
    // entra/acepta la edicion de la propiedad seleccionada (estaba vacio,
    // tambien en PC)
    EnterPropertieSelect();
}

Properties::~Properties() {
    // si este panel era el ACTIVO, limpiar el puntero global: sino queda colgando
    // (dangling) y cualquier lectura de PropsActivo crashea (ej. al reemplazar este
    // panel por un UV Editor, que lee la parte activa via PropsActivo).
    if (PropsActivo == this) PropsActivo = NULL;
}