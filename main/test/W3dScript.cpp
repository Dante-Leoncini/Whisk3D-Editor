#include "test/W3dScript.h"
#include "objects/Objects.h"   // ObjActivo, g_editMesh, EditSelectMode, Sel*, DeseleccionarTodo
#include "objects/Mesh.h"      // NewMesh, MeshType, Mesh
#include "edit/Modifier.h"     // Modifier (params del Mirror en el harness)
#include "edit/MeshEdit.h"     // Nuevo/MoverMeshPart (funciones libres del editor)
#include "objects/EditMesh.h"  // EditMesh
#include "ViewPorts/LayoutInput.h" // LayoutToggleEditMode/ExtrudeFaces, EditXform*
#include "importers/import_obj.h"   // ExportOBJ
#include "objects/ObjectMode.h" // Eliminar (test del borrado + su undo)
#include "objects/Light.h"      // Light::Create + Lights (test del borrado de luces)
#include "Undo.h"               // UndoCapturarSeleccionEdit / UndoDeshacer (test del Ctrl+Z)
#include "variables.h"         // InteractionMode, enum { ObjectMode, EditMode, ... }
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>

// ============================================================================
//  Helpers
// ============================================================================

// malla "activa" del script: la de edicion si estamos en Edit Mode, sino el
// objeto activo si es una malla.
static Mesh* ScriptActiveMesh() {
    if (g_editMesh) return (Mesh*)g_editMesh;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh) return (Mesh*)ObjActivo;
    return NULL;
}

static Material* ScriptActiveMaterial() {
    Mesh* m = ScriptActiveMesh();
    if (!m || m->materialsGroup.empty()) return NULL;
    return m->materialsGroup[0].material;
}

static bool MeshTypeFromName(const std::string& s, MeshType::Enum& out) {
    if      (s == "cube")     out = MeshType::cube;
    else if (s == "plane")    out = MeshType::plane;
    else if (s == "circle")   out = MeshType::circle;
    else if (s == "uvsphere" || s == "sphere") out = MeshType::UVsphere;
    else if (s == "cone")     out = MeshType::cone;
    else if (s == "cylinder") out = MeshType::cylinder;
    else if (s == "vertex" || s == "vertice") out = MeshType::vertice;
    else return false;
    return true;
}

// ============================================================================
//  Un comando
// ============================================================================
bool W3dRunCommand(const std::string& linea, std::string& err) {
    std::istringstream ss(linea);
    std::string cmd; ss >> cmd;
    if (cmd.empty()) return true;

    // ---- add <cube|plane|circle|uvsphere|cone|cylinder|vertex> ----
    if (cmd == "add") {
        std::string prim; ss >> prim;
        MeshType::Enum tipo;
        if (!MeshTypeFromName(prim, tipo)) { err = "primitiva desconocida: '" + prim + "'"; return false; }
        Object* o = NewMesh(MeshType(tipo), NULL, false);
        if (!o) { err = "NewMesh devolvio NULL"; return false; }
        DeseleccionarTodo();
        o->Seleccionar();
        return true;
    }

    // ---- objpos <x> <y> <z> : setea la posicion del objeto ACTIVO (transform, para testear Apply Transforms) ----
    if (cmd == "objpos") {
        if (!ObjActivo) { err = "no hay objeto activo"; return false; }
        float x=0,y=0,z=0; ss>>x>>y>>z; ObjActivo->pos = Vector3(x,y,z); return true;
    }

    // ---- mode <object|edit> ----
    if (cmd == "mode") {
        std::string md; ss >> md;
        if (md == "edit") {
            if (InteractionMode != EditMode) LayoutToggleEditMode();
            if (InteractionMode != EditMode) { err = "no se pudo entrar a Edit Mode (hay una malla activa?)"; return false; }
        } else if (md == "object") {
            if (InteractionMode == EditMode) LayoutToggleEditMode();
            if (InteractionMode == EditMode) { err = "no se pudo salir de Edit Mode"; return false; }
        } else { err = "modo desconocido: '" + md + "' (object|edit)"; return false; }
        return true;
    }

    // ---- selmode <vertex|edge|face> ----
    if (cmd == "selmode") {
        std::string sm; ss >> sm;
        if      (sm == "vertex" || sm == "vert") EditSelectMode = SelVertex;
        else if (sm == "edge")                   EditSelectMode = SelEdge;
        else if (sm == "face")                   EditSelectMode = SelFace;
        else { err = "selmode desconocido: '" + sm + "' (vertex|edge|face)"; return false; }
        return true;
    }

    // ---- select <all|none|face N|vert N|edge N> ----
    if (cmd == "select") {
        std::string what; ss >> what;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        m->EnsureEdit();
        EditMesh* e = m->edit;
        if (!e) { err = "la malla no tiene edit mesh"; return false; }
        UndoCapturarSeleccionEdit(m); // Ctrl+Z: guarda la seleccion previa (igual que el input real)
        if (what == "all")  { e->SeleccionarTodo(true);  return true; }
        if (what == "none") { e->SeleccionarTodo(false); return true; }
        int idx = -1; ss >> idx;
        char b[96];
        if (what == "face") {
            if (idx < 0 || idx >= e->NumFaces()) { sprintf(b, "cara %d fuera de rango (0..%d)", idx, e->NumFaces()-1); err = b; return false; }
            e->TogglearFace(idx, true);
        } else if (what == "vert" || what == "vertex") {
            if (idx < 0 || idx >= e->NumVerts()) { sprintf(b, "vert %d fuera de rango (0..%d)", idx, e->NumVerts()-1); err = b; return false; }
            e->TogglearVert(idx, true);
        } else if (what == "edge") {
            if (idx < 0 || idx >= e->NumEdges()) { sprintf(b, "edge %d fuera de rango (0..%d)", idx, e->NumEdges()-1); err = b; return false; }
            e->TogglearEdge(idx, true);
        } else { err = "select desconocido: '" + what + "' (all|none|face N|vert N|edge N)"; return false; }
        return true;
    }

    // ---- loop <edge|ring|face> <edgeIdx> (loop select desde un borde; reemplaza la seleccion) ----
    //   edge = Edge Loop (sigue la linea, ej. circulo del cilindro)
    //   ring = Edge Ring (travesanos perpendiculares)
    //   face = Face Loop (anillo de caras)
    if (cmd == "loop") {
        std::string what; int idx = -1; ss >> what >> idx;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        m->EnsureEdit(); EditMesh* e = m->edit;
        if (!e) { err = "la malla no tiene edit mesh"; return false; }
        char b[96];
        if (idx < 0 || idx >= e->NumEdges()) { sprintf(b, "edge %d fuera de rango (0..%d)", idx, e->NumEdges()-1); err = b; return false; }
        UndoCapturarSeleccionEdit(m); // Ctrl+Z: guarda la seleccion previa
        if      (what == "edge") { if (EditSelectMode == SelVertex) e->SeleccionarLoopEdgeVerts(idx, true); else e->SeleccionarLoopEdge(idx, true); }
        else if (what == "ring") e->SeleccionarRingEdge(idx, true);
        else if (what == "face") e->SeleccionarLoopFace(idx, true);
        else { err = "loop desconocido: '" + what + "' (edge|ring|face) <edgeIdx>"; return false; }
        return true;
    }

    // ---- path <toIdx> [fill] (Pick Shortest Path desde el sub-elemento ACTIVO hasta toIdx) ----
    //   sin 'fill' = un caminito; con 'fill' = rellena la region (todos los caminos minimos)
    if (cmd == "path") {
        int idx = -1; std::string opt; ss >> idx >> opt;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        m->EnsureEdit(); EditMesh* e = m->edit;
        if (!e) { err = "la malla no tiene edit mesh"; return false; }
        UndoCapturarSeleccionEdit(m); // Ctrl+Z: guarda la seleccion previa
        e->SeleccionarShortestPath(idx, opt == "fill");
        return true;
    }

    // ---- undo / redo (deshace / rehace el ultimo comando del stack de Ctrl+Z / Ctrl+Y) ----
    if (cmd == "undo") { UndoDeshacer(); return true; }
    if (cmd == "redo") { UndoRehacer(); return true; }

    // ---- addlight (crea una luz en la escena, seleccionada) ----
    if (cmd == "addlight") {
        Light* l = Light::Create(NULL, 1, 2, 2);
        if (!l) { err = "Light::Create devolvio NULL"; return false; }
        DeseleccionarTodo(); l->Seleccionar();
        return true;
    }

    // ---- selectall / deselectall (seleccion de OBJETOS top-level, object mode) ----
    if (cmd == "selectall" || cmd == "deselectall") {
        DeseleccionarTodo();
        if (cmd == "selectall" && SceneCollection)
            for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) SceneCollection->Childrens[i]->Seleccionar();
        return true;
    }

    // ---- matcolor <r> <g> <b> (cambia el diffuse via UndoCapturarColor, como el ColorPicker) ----
    if (cmd == "matcolor") {
        float r=0,g=0,b=0; ss >> r >> g >> b;
        Material* mat = ScriptActiveMaterial();
        if (!mat) { err = "no hay material activo"; return false; }
        GLfloat orig[4]; for (int i=0;i<4;i++) orig[i]=mat->diffuse[i];
        mat->diffuse[0]=r; mat->diffuse[1]=g; mat->diffuse[2]=b;
        UndoCapturarColor(mat->diffuse, orig);
        return true;
    }
    // ---- matchrome (togglea el checkbox 'chrome' via el pending de modificacion de material) ----
    if (cmd == "matchrome") {
        Material* mat = ScriptActiveMaterial();
        if (!mat) { err = "no hay material activo"; return false; }
        UndoMaterialModIniciar(mat);
        mat->chrome = !mat->chrome;
        UndoMaterialModCommit();
        return true;
    }

    // ---- delete (borra los objetos seleccionados; object mode) -> testea el undo de borrado ----
    if (cmd == "delete") {
        if (InteractionMode != ObjectMode) { err = "delete necesita Object Mode"; return false; }
        Eliminar(false);
        return true;
    }

    // ---- editdelete : borra la seleccion en Edit Mode segun el selmode actual (Vertices/Edges/Faces) ----
    if (cmd == "editdelete") {
        if (InteractionMode != EditMode) { err = "editdelete necesita Edit Mode"; return false; }
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        m->BorrarSeleccionEdit(EditSelectMode);
        return true;
    }

    // ---- move <amt> (mueve la seleccion 'amt' en el eje X; usa el MISMO path G/R/S -> testea EditMoveUndo) ----
    if (cmd == "move") {
        float amt = 0.0f; ss >> amt;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if (InteractionMode != EditMode) { err = "move necesita Edit Mode"; return false; }
        if (!EditXformStart(translacion, X)) { err = "el move no arranco"; return false; }
        if (!EditXformActivo()) { err = "el move no arranco (sin seleccion)"; return false; }
        EditXformNumValor(amt);  // aplica el valor exacto en X
        EditXformConfirmar();    // confirma -> pushea el EditMoveUndo pendiente
        return true;
    }

    // ---- movechain <a> <b> (DOS moves en X SIN confirmar entre medio -> testea el encadenamiento G->R->S:
    //       el 2do EditXformStart debe confirmar+pushear el 1ro, dejando 2 pasos de undo) ----
    if (cmd == "movechain") {
        float a = 0.0f, b = 0.0f; ss >> a >> b;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if (InteractionMode != EditMode) { err = "movechain necesita Edit Mode"; return false; }
        if (!EditXformStart(translacion, X)) { err = "el 1er move no arranco"; return false; }
        EditXformNumValor(a);                 // move 1 (NO confirma)
        if (!EditXformStart(translacion, X)) { err = "el 2do move no arranco"; return false; } // debe confirmar el 1ro
        EditXformNumValor(b);                 // move 2
        EditXformConfirmar();                 // confirma el 2do
        return true;
    }

    // ---- objpos <x> <y> <z> : posicion LOCAL del objeto activo (setup del Join) ----
    if (cmd == "objpos") {
        if (!ObjActivo) { err = "no hay objeto activo"; return false; }
        float x=0,y=0,z=0; ss >> x >> y >> z;
        ObjActivo->pos = Vector3(x,y,z);
        return true;
    }
    // ---- objscale <s> : escala UNIFORME del objeto activo ----
    if (cmd == "objscale") {
        if (!ObjActivo) { err = "no hay objeto activo"; return false; }
        float s=1.0f; ss >> s;
        ObjActivo->scale = Vector3(s,s,s);
        return true;
    }
    // ---- objrot <gradosY> : rota el objeto activo alrededor de Y ----
    if (cmd == "objrot") {
        if (!ObjActivo) { err = "no hay objeto activo"; return false; }
        float deg=0.0f; ss >> deg;
        ObjActivo->rot = Quaternion::FromAxisAngle(Vector3(0,1,0), deg);
        return true;
    }
    // ---- active <n> : ObjActivo = hijo n de la escena (top-level), sin tocar la seleccion ----
    if (cmd == "active") {
        int n=-1; ss >> n;
        if (!SceneCollection || n<0 || n>=(int)SceneCollection->Childrens.size()) { err="indice de objeto fuera de rango"; return false; }
        ObjActivo = SceneCollection->Childrens[n];
        return true;
    }
    // ---- objname <nombre> : renombra el objeto activo (para distinguirlos en el test) ----
    if (cmd == "objname") {
        if (!ObjActivo) { err = "no hay objeto activo"; return false; }
        std::string nm; ss >> nm;
        ObjActivo->name = nm; return true;
    }
    // ---- activename <nombre> : ObjActivo = objeto con ese nombre (busca en el arbol) ----
    if (cmd == "activename") {
        std::string nm; ss >> nm;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o) { err = "objeto no encontrado: " + nm; return false; }
        ObjActivo = o; return true;
    }
    // ---- selname <nombre> : agrega ese objeto a la seleccion ----
    if (cmd == "selname") {
        std::string nm; ss >> nm;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o) { err = "objeto no encontrado: " + nm; return false; }
        o->Seleccionar(); return true;
    }
    // ---- parent <hijo> <padre> : emparenta (conserva lo LOCAL) -> testea el Join con jerarquias anidadas ----
    if (cmd == "parent") {
        std::string c, p; ss >> c >> p;
        Object* co = SceneCollection ? FindObjectByName(SceneCollection, c) : NULL;
        Object* po = SceneCollection ? FindObjectByName(SceneCollection, p) : NULL;
        if (!co || !po) { err = "parent: objeto no encontrado"; return false; }
        ReparentSimple(co, po); return true;
    }
    // ---- scene : lista los hijos top-level (nombre/tipo/hijos) ----
    if (cmd == "scene") {
        if (!SceneCollection) { err = "sin escena"; return false; }
        for (size_t i=0;i<SceneCollection->Childrens.size();i++){
            Object* o=SceneCollection->Childrens[i];
            printf("      [scene] [%d] '%s' type=%d children=%d\n", (int)i, o->name.c_str(), (int)o->getType(), (int)o->Childrens.size());
        }
        return true;
    }
    // ---- join : une las mallas seleccionadas en el objeto activo (Ctrl+J) ----
    if (cmd == "join") { JoinObjetos(); return true; }
    // ---- apply <location|rotation|scale|all> : hornea el transform en la malla (Ctrl+A) ----
    if (cmd == "apply") {
        std::string w; ss >> w;
        int what = (w=="location")?0:(w=="rotation")?1:(w=="scale")?2:(w=="all")?3:-1;
        if (what<0) { err = "apply desconocido: '"+w+"' (location|rotation|scale|all)"; return false; }
        AplicarTransform(what); return true;
    }
    // ---- objinfo : imprime pos/rot/scale del objeto activo + su bbox LOCAL (vertex[] sin transform) ----
    if (cmd == "objinfo") {
        if (!ObjActivo) { err = "no hay objeto activo"; return false; }
        Object* o = ObjActivo;
        char b[240];
        Mesh* m = (o->getType()==ObjectType::mesh) ? (Mesh*)o : NULL;
        float mn[3]={0,0,0}, mx[3]={0,0,0};
        if (m && m->vertex && m->vertexSize>0){
            for (int k=0;k<3;k++){ mn[k]=1e30f; mx[k]=-1e30f; }
            for (int i=0;i<m->vertexSize;i++){ float p[3]={m->vertex[i*3],m->vertex[i*3+1],m->vertex[i*3+2]};
                for (int k=0;k<3;k++){ if(p[k]<mn[k])mn[k]=p[k]; if(p[k]>mx[k])mx[k]=p[k]; } }
        }
        sprintf(b, "[objinfo] pos(%.2f,%.2f,%.2f) rotEuler(%.1f,%.1f,%.1f) scale(%.2f,%.2f,%.2f) localbbox x[%.2f..%.2f]",
                o->pos.x,o->pos.y,o->pos.z, o->rotEuler.x,o->rotEuler.y,o->rotEuler.z,
                o->scale.x,o->scale.y,o->scale.z, mn[0],mx[0]);
        printf("      %s\n", b);
        return true;
    }
    // ---- modadd <screw|mirror|array|subsurf|boolean> : agrega un modificador al stack de la malla activa ----
    if (cmd == "modadd") {
        Mesh* m = ScriptActiveMesh(); if (!m) { err = "no hay malla activa"; return false; }
        std::string t; ss >> t;
        int tipo = (t=="screw")?0:(t=="mirror")?1:(t=="array")?2:(t=="subsurf")?3:(t=="boolean")?4:-1;
        if (tipo < 0) { err = "tipo de modificador desconocido: '" + t + "'"; return false; }
        m->AgregarModificador(tipo); m->GenerarMallaModificada(); return true;
    }
    // ---- modtarget <n> : setea el target del Mirror activo = hijo n de la escena ----
    if (cmd == "modtarget") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        if (m->modificadorActivo<0||m->modificadorActivo>=(int)m->modificadores.size()){err="sin modificador activo";return false;}
        int n=-1; ss>>n;
        if (!SceneCollection||n<0||n>=(int)SceneCollection->Childrens.size()){err="target fuera de rango";return false;}
        m->modificadores[m->modificadorActivo]->target = SceneCollection->Childrens[n];
        m->GenerarMallaModificada(); return true;
    }
    // ---- genbbox : bbox LOCAL de la malla GENERADA (para verificar la reflexion del mirror) ----
    if (cmd == "genbbox") {
        Mesh* m = ScriptActiveMesh(); if(!m||!m->genValido||!m->genVertex){err="sin malla generada";return false;}
        float mn[3]={1e30f,1e30f,1e30f},mx[3]={-1e30f,-1e30f,-1e30f};
        for(int i=0;i<m->genVertexSize;i++){ float p[3]={m->genVertex[i*3],m->genVertex[i*3+1],m->genVertex[i*3+2]};
            for(int k=0;k<3;k++){if(p[k]<mn[k])mn[k]=p[k];if(p[k]>mx[k])mx[k]=p[k];} }
        printf("      [genbbox] x[%.2f..%.2f] y[%.2f..%.2f] z[%.2f..%.2f]\n",mn[0],mx[0],mn[1],mx[1],mn[2],mx[2]);
        return true;
    }
    // ---- modapply : hornea la malla generada del modificador activo en la editable (Apply Modifier) ----
    if (cmd == "modapply") { Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;} m->AplicarModificadorActivo(); return true; }
    // ---- editinfo : imprime la malla EDITABLE (verts + faces3d) con histograma de lados (tris/quads/ngons) ----
    if (cmd == "editinfo") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        int tri=0,quad=0,ngon=0; for(size_t f=0;f<m->faces3d.size();f++){ int s=(int)m->faces3d[f].idx.size(); if(s==3)tri++; else if(s==4)quad++; else if(s>4)ngon++; }
        printf("      [edit] verts=%d faces3d=%d (tris=%d quads=%d ngons=%d) render tris=%d looseE=%d looseV=%d\n",
               m->vertexSize, (int)m->faces3d.size(), tri, quad, ngon, m->facesSize/3,
               (int)(m->looseEdges.size()/2), (int)m->looseVerts.size());
        return true;
    }
    // ---- editbbox : bounding box de la malla EDITABLE (vertex[]) ----
    if (cmd == "editbbox") {
        Mesh* m = ScriptActiveMesh(); if(!m||!m->vertex||m->vertexSize<=0){err="sin malla editable";return false;}
        float mn[3]={1e30f,1e30f,1e30f},mx[3]={-1e30f,-1e30f,-1e30f};
        for(int i=0;i<m->vertexSize;i++) for(int k=0;k<3;k++){ float p=m->vertex[i*3+k]; if(p<mn[k])mn[k]=p; if(p>mx[k])mx[k]=p; }
        printf("      [editbbox] x[%.3f..%.3f] y[%.3f..%.3f] z[%.3f..%.3f]\n",mn[0],mx[0],mn[1],mx[1],mn[2],mx[2]);
        return true;
    }
    // ---- triangulate : triangula las caras SELECCIONADAS (Edit Mode) de la malla activa ----
    if (cmd == "triangulate") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        if (InteractionMode != EditMode) { err="triangulate necesita Edit Mode"; return false; }
        if (!m->TriangularSeleccionEdit()) { err="no se triangulo nada (sin caras >3 lados seleccionadas)"; return false; }
        return true;
    }
    // ---- geninfo : imprime la malla GENERADA por los modificadores (genValido + verts/tris) ----
    if (cmd == "geninfo") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        printf("      [gen] valido=%d genVerts=%d genTris=%d (render verts=%d tris=%d)\n",
               (int)m->genValido, m->genVertexSize, m->genFacesSize/3, m->vertexSize, m->facesSize/3);
        return true;
    }
    // ---- modeje <x|y|z> <0|1> / modmerge <0|1> [dist] : cambia params del modificador activo (Mirror) + regenera ----
    if (cmd == "modeje" || cmd == "modmerge") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        if (m->modificadorActivo<0 || m->modificadorActivo>=(int)m->modificadores.size()){err="sin modificador activo";return false;}
        Modifier* mod = m->modificadores[m->modificadorActivo];
        if (cmd == "modeje"){ std::string e; int on=1; ss>>e>>on; if(e=="x")mod->ejeX=on; else if(e=="y")mod->ejeY=on; else if(e=="z")mod->ejeZ=on; }
        else { int on=1; float dist=-1; ss>>on>>dist; mod->merge=on; if(dist>=0) mod->mergeDist=dist; }
        m->GenerarMallaModificada(); return true;
    }
    // ---- modclip <0|1> : activa/desactiva el clipping del modificador activo (Mirror) ----
    if (cmd == "modclip") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        if (m->modificadorActivo<0 || m->modificadorActivo>=(int)m->modificadores.size()){err="sin modificador activo";return false;}
        int on=1; ss>>on; m->modificadores[m->modificadorActivo]->clipping=on; return true;
    }
    // ---- modsub <level> <simple 0|1> : setea el nivel viewport + modo (Simple/Catmull) del subsurf activo + regenera ----
    if (cmd == "modsub") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        if (m->modificadorActivo<0 || m->modificadorActivo>=(int)m->modificadores.size()){err="sin modificador activo";return false;}
        Modifier* mod = m->modificadores[m->modificadorActivo];
        int lvl=1, simple=0; ss>>lvl>>simple; mod->subLevel=lvl; mod->subSimple=(simple!=0);
        m->GenerarMallaModificada(); return true;
    }
    // ---- modscrew <angle> <height> <steps> <axis 0|1|2> : setea el screw activo + regenera ----
    if (cmd == "modscrew") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        if (m->modificadorActivo<0 || m->modificadorActivo>=(int)m->modificadores.size()){err="sin modificador activo";return false;}
        Modifier* mod = m->modificadores[m->modificadorActivo];
        float ang=360, hei=0, st=16; int ax=1; ss>>ang>>hei>>st>>ax;
        mod->screwAngle=ang; mod->screwHeight=hei; mod->screwSteps=st; mod->screwAxis=ax;
        int sm, mg, fl; // opcionales: smooth / merge / flip (solo se aplican si vienen)
        if (ss>>sm) mod->screwSmooth=(sm!=0);
        if (ss>>mg) mod->screwMerge=(mg!=0);
        if (ss>>fl) mod->screwFlip=(fl!=0);
        m->GenerarMallaModificada(); return true;
    }
    // ---- rendermode <0|1> : setea g_modRenderMode (Subdivision usa subRenderLevel) + regenera la malla activa ----
    if (cmd == "rendermode") {
        extern bool g_modRenderMode; Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        int on=0; ss>>on; g_modRenderMode=(on!=0); m->GenerarMallaModificada(); return true;
    }
    // ---- modmostrar <viewport|edit> <0|1> : toggle de visibilidad del modificador activo + regenera ----
    if (cmd == "modmostrar") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        if (m->modificadorActivo<0 || m->modificadorActivo>=(int)m->modificadores.size()){err="sin modificador activo";return false;}
        Modifier* mod = m->modificadores[m->modificadorActivo];
        std::string q; int on=1; ss>>q>>on;
        if (q=="viewport") mod->mostrarViewport=on; else if (q=="edit") mod->mostrarEdit=on; else {err="viewport|edit";return false;}
        m->GenerarMallaModificada(); return true;
    }
    // ---- modremove : borra el modificador activo ----
    if (cmd == "modremove") { Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;} m->QuitarModificadorActivo(); return true; }
    // ---- modmove <up|down> : reordena el modificador activo (el orden importa) ----
    if (cmd == "modmove") { Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;} std::string d; ss>>d; m->MoverModificador(d=="up"?-1:1); return true; }
    // ---- partnew : agrega un mesh part vacio ----
    if (cmd == "partnew") { Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;} NuevoMeshPart(m); return true; }
    // ---- partassign <idx> : asigna las caras SELECCIONADAS (edit) al mesh part idx ----
    if (cmd == "partassign") { Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;} int idx=-1; ss>>idx; m->AsignarFacesAMeshPart(idx); return true; }
    // ---- partmove <idx> <up|down> : reordena el mesh part idx (orden de dibujado) ----
    if (cmd == "partmove") { Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;} int idx=-1; std::string d; ss>>idx>>d; MoverMeshPart(m, idx, d=="up"?-1:1); return true; }
    // ---- partcount : imprime la cantidad de mesh parts + los rangos de dibujado (start/count por parte) ----
    if (cmd == "partcount") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        std::string s = "[parts] n="; { char b[32]; sprintf(b,"%d |",(int)m->materialsGroup.size()); s+=b; }
        for (size_t g=0;g<m->materialsGroup.size();g++){ char b[48]; sprintf(b," [%d:start%d cnt%d]",(int)g,m->materialsGroup[g].startDrawn,m->materialsGroup[g].indicesDrawnCount); s+=b; }
        printf("      %s\n", s.c_str());
        return true;
    }
    // ---- modlist : imprime el stack (nombres en orden + el activo) ----
    if (cmd == "modlist") {
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        std::string s = "[mods] activo="; { char b[16]; sprintf(b,"%d",m->modificadorActivo); s+=b; } s += " |";
        for (int i=0;i<(int)m->modificadores.size();i++) s += " " + m->NombreModificador(i);
        printf("      %s\n", s.c_str());
        return true;
    }
    // ---- worldbbox : bbox en MUNDO de la malla activa (GetWorldMatrix*verts). Prueba la matematica del Join:
    //       tras el join, el bbox-mundo del activo = UNION de los bbox-mundo previos (la geo queda visualmente igual) ----
    if (cmd == "worldbbox") {
        Mesh* m = ScriptActiveMesh();
        if (!m || !m->vertex || m->vertexSize<=0) { err="no hay malla activa con geometria"; return false; }
        Matrix4 W; ObjActivo->GetWorldMatrix(W);
        float mn[3]={1e30f,1e30f,1e30f}, mx[3]={-1e30f,-1e30f,-1e30f};
        for (int i=0;i<m->vertexSize;i++){
            Vector3 w = W * Vector3(m->vertex[i*3], m->vertex[i*3+1], m->vertex[i*3+2]);
            float p[3]={w.x,w.y,w.z};
            for (int k=0;k<3;k++){ if(p[k]<mn[k])mn[k]=p[k]; if(p[k]>mx[k])mx[k]=p[k]; }
        }
        printf("      [worldbbox] x[%.2f..%.2f] y[%.2f..%.2f] z[%.2f..%.2f] verts=%d\n",
               mn[0],mx[0],mn[1],mx[1],mn[2],mx[2], m->vertexSize);
        return true;
    }

    // ---- extrude <dist> (caras/aristas/verts seleccionados, a lo largo de la normal) ----
    if (cmd == "extrude") {
        float dist = 0.0f; ss >> dist;
        LayoutExtrudeFaces();
        if (!EditXformActivo()) { err = "el extrude no arranco (hay algo seleccionado? estas en Edit Mode?)"; return false; }
        EditXformNumValor(dist);
        EditXformConfirmar();
        return true;
    }

    // ---- merge <center|cursor|collapse|bydistance> (suelda la seleccion; bydistance usa g_mergeDist) ----
    if (cmd == "merge") {
        extern float g_mergeDist;
        std::string what; ss >> what;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        int modo = 3;
        if      (what == "center")   modo = 0;
        else if (what == "cursor")   modo = 1;
        else if (what == "collapse") modo = 2;
        else if (what == "bydistance" || what == "distance") modo = 3;
        else { err = "merge desconocido: '" + what + "' (center|cursor|collapse|bydistance)"; return false; }
        MergeVertsEdit(m, modo, g_mergeDist, Vector3(0,0,0));
        return true;
    }

    // ---- shade <smooth|flat> ----
    if (cmd == "shade") {
        std::string s; ss >> s;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if      (s == "smooth") { m->meshSmooth = true;  m->RecalcularNormales(); } // promedia por posicion
        else if (s == "flat")   { m->meshSmooth = false; }                          // GenerarRender hace por-cara
        else { err = "shade desconocido: '" + s + "' (smooth|flat)"; return false; }
        m->GenerarRender();
        return true;
    }

    // ---- shadesel <smooth|flat> : Shade POR CARA (menu Face) sobre la seleccion (ShadeEdit); NO afecta toda la malla ----
    if (cmd == "shadesel") {
        std::string s; ss >> s;
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        if (InteractionMode != EditMode) { err="shadesel necesita Edit Mode"; return false; }
        bool sm = (s=="smooth"); if (s!="smooth" && s!="flat"){ err="shadesel: smooth|flat"; return false; }
        if (!m->ShadeEdit(sm)) { err="shadesel no afecto nada (sin caras seleccionadas)"; return false; }
        return true;
    }

    // ---- export <ruta.obj> [applyMods 0|1] [applyXform 0|1] : def ambos ON ----
    if (cmd == "export") {
        std::string path; ss >> path;
        if (path.empty()) { err = "falta la ruta del export"; return false; }
        int aMods=1, aXf=1; { int t; if (ss>>t) aMods=t; if (ss>>t) aXf=t; } // opcionales (si faltan, quedan 1=ON)
        if (!ExportOBJ(path, false, aMods!=0, aXf!=0)) { err = "ExportOBJ fallo (ruta: " + path + ")"; return false; }
        return true;
    }

    // ---- import <ruta.obj> ----
    if (cmd == "import") {
        std::string path; ss >> path;
        if (path.empty()) { err = "falta la ruta del import"; return false; }
        if (!ImportOBJ(path, false)) { err = "ImportOBJ fallo (ruta: " + path + ")"; return false; }
        return true;
    }

    // ---- duplicate (Shift+D en Edit Mode: duplica la seleccion -> caras/bordes indirectos + verts) ----
    if (cmd == "duplicate") {
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if (!m->DuplicarSeleccionEdit()) { err = "DuplicarSeleccionEdit fallo (hay seleccion en Edit Mode?)"; return false; }
        return true;
    }

    // ---- rip (V en Edit Mode: separa la malla a lo largo de la seleccion) ----
    if (cmd == "rip") {
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if (!m->RipSeleccionEdit()) { err = "RipSeleccionEdit fallo (la seleccion no separa la malla?)"; return false; }
        return true;
    }

    // ---- delloop (Delete > Edge Loops: disuelve el edge loop seleccionado) ----
    if (cmd == "delloop") {
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if (!m->BorrarEdgeLoopEdit()) { err = "BorrarEdgeLoopEdit fallo (la seleccion no es un loop disolvible?)"; return false; }
        return true;
    }

    // ---- expect <verts|faces|edges|tris> <N> (assert sobre la malla activa) ----
    if (cmd == "expect") {
        std::string what; ss >> what;
        // material del mesh activo: matr = diffuse rojo (float); matchrome = checkbox chrome (0/1)
        if (what == "matr" || what == "matchrome") {
            Material* mat = ScriptActiveMaterial();
            if (!mat) { err = "no hay material activo"; return false; }
            char b[96];
            if (what == "matr") {
                float val=0; ss >> val; float d=mat->diffuse[0]-val; if (d<0) d=-d;
                if (d>0.01f) { sprintf(b,"esperaba matr=%.3f, hay %.3f", val, mat->diffuse[0]); err=b; return false; }
            } else {
                int val=-1; ss >> val; int got=mat->chrome?1:0;
                if (got!=val) { sprintf(b,"esperaba matchrome=%d, hay %d", val, got); err=b; return false; }
            }
            return true;
        }
        // scene-global (NO necesitan malla activa): objects = hijos top-level de la escena; lights = global Lights
        if (what == "objects" || what == "lights") {
            int n = -1; ss >> n;
            int got = (what == "objects") ? (SceneCollection ? (int)SceneCollection->Childrens.size() : 0)
                                          : (int)Lights.size();
            if (got != n) { char b[96]; sprintf(b, "esperaba %s=%d, hay %d", what.c_str(), n, got); err = b; return false; }
            return true;
        }
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if (what == "vx" || what == "vy" || what == "vz") { // posicion (float) de un vertice del render
            int idx = -1; float val = 0.0f; ss >> idx >> val;
            if (!m->vertex || idx < 0 || idx >= m->vertexSize) { err = "expect vx/vy/vz: vert fuera de rango"; return false; }
            int comp = (what == "vx") ? 0 : (what == "vy") ? 1 : 2;
            float gv = m->vertex[idx*3 + comp];
            float d = gv - val; if (d < 0) d = -d;
            if (d > 0.01f) { char b[128]; sprintf(b, "esperaba %s[%d]=%.3f, hay %.3f", what.c_str(), idx, val, gv); err = b; return false; }
            return true;
        }
        if (what == "minx" || what == "maxx") { // min/max de la coordenada X de la malla editable (para clipping)
            float val = 0.0f; ss >> val;
            if (!m->vertex || m->vertexSize<=0) { err = "expect minx/maxx: sin malla"; return false; }
            float ex = m->vertex[0]; for (int i=1;i<m->vertexSize;i++){ float x=m->vertex[i*3]; if (what=="minx"? x<ex : x>ex) ex=x; }
            float d = ex - val; if (d < 0) d = -d;
            if (d > 0.01f) { char b[128]; sprintf(b, "esperaba %s=%.3f, hay %.3f", what.c_str(), val, ex); err = b; return false; }
            return true;
        }
        int n = -1; ss >> n;
        int got = -1;
        if (what == "verts" || what == "faces" || what == "edges") {
            m->EnsureEdit();
            EditMesh* e = m->edit;
            if (!e) { err = "la malla no tiene edit mesh"; return false; }
            if      (what == "verts") got = e->NumVerts();
            else if (what == "faces") got = e->NumFaces();
            else                      got = e->NumEdges();
        } else if (what == "tris") {
            got = m->facesSize / 3;
        } else if (what == "rverts") {
            got = m->vertexSize; // verts del RENDER (suben/bajan al split/merge por shading)
        } else if (what == "sel") {
            m->EnsureEdit(); EditMesh* e = m->edit;
            if (!e) { err = "la malla no tiene edit mesh"; return false; }
            int c = 0; // seleccionados en el sub-modo activo
            if      (EditSelectMode == SelFace) { for (size_t i=0;i<e->faceSel.size();i++) if (e->faceSel[i]) c++; }
            else if (EditSelectMode == SelEdge) { for (size_t i=0;i<e->edgeSel.size();i++) if (e->edgeSel[i]) c++; }
            else                                { for (size_t i=0;i<e->vertSel.size();i++) if (e->vertSel[i]) c++; }
            got = c;
        } else if (what == "seams") {
            got = (int)m->seamEdges.size(); // costuras UV marcadas (por posicion)
        } else if (what == "quads" || what == "ngons" || what == "tris3d") {
            int c=0; for(size_t f=0;f<m->faces3d.size();f++){ int s=(int)m->faces3d[f].idx.size();
                if((what=="quads"&&s==4)||(what=="ngons"&&s>4)||(what=="tris3d"&&s==3)) c++; }
            got = c; // caras LOGICAS de faces3d por cantidad de lados (verifica que Apply NO triangule los quads)
        } else { err = "expect desconocido: '" + what + "' (verts|faces|edges|tris|rverts|sel|seams|quads|ngons|tris3d)"; return false; }
        if (got != n) { char b[128]; sprintf(b, "esperaba %s=%d, hay %d", what.c_str(), n, got); err = b; return false; }
        return true;
    }

    // ---- sharp <mark|clear> (bordes seleccionados; en malla smooth quedan flat) ----
    if (cmd == "sharp") {
        std::string s; ss >> s;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if      (s == "mark")  m->MarcarSharpEdit(true);
        else if (s == "clear") m->MarcarSharpEdit(false);
        else { err = "sharp desconocido: '" + s + "' (mark|clear)"; return false; }
        return true;
    }

    // ---- seam <mark|clear> (costuras UV de los bordes seleccionados; se ven magenta) ----
    if (cmd == "seam") {
        std::string s; ss >> s;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if      (s == "mark")  m->MarcarSeamEdit(true);
        else if (s == "clear") m->MarcarSeamEdit(false);
        else { err = "seam desconocido: '" + s + "' (mark|clear)"; return false; }
        return true;
    }

    // ---- project <cube|cylinder|sphere> (proyecta UV sobre las caras seleccionadas) ----
    if (cmd == "project") {
        std::string s; ss >> s;
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        if      (s == "cube")     m->ProyectarUVCaras(0);
        else if (s == "cylinder") m->ProyectarUVCaras(1);
        else if (s == "sphere")   m->ProyectarUVCaras(2);
        else { err = "project desconocido: '" + s + "' (cube|cylinder|sphere)"; return false; }
        return true;
    }

    // ---- print (loguea los contadores de la malla activa: diagnostico) ----
    if (cmd == "print") {
        Mesh* m = ScriptActiveMesh();
        if (!m) { err = "no hay malla activa"; return false; }
        m->EnsureEdit();
        int ev = m->edit ? m->edit->NumVerts() : -1;
        int ef = m->edit ? m->edit->NumFaces() : -1;
        int ee = m->edit ? m->edit->NumEdges() : -1;
        int n0x=0,n0y=0,n0z=0; if (m->normals && m->vertexSize>0){ n0x=m->normals[0]; n0y=m->normals[1]; n0z=m->normals[2]; }
        int selC = 0; const char* selN = "vert"; // seleccionados en el sub-modo activo
        if (m->edit){
            if      (EditSelectMode == SelFace){ selN="face"; for (size_t i=0;i<m->edit->faceSel.size();i++) if (m->edit->faceSel[i]) selC++; }
            else if (EditSelectMode == SelEdge){ selN="edge"; for (size_t i=0;i<m->edit->edgeSel.size();i++) if (m->edit->edgeSel[i]) selC++; }
            else                               {              for (size_t i=0;i<m->edit->vertSel.size();i++) if (m->edit->vertSel[i]) selC++; }
        }
        printf("      [malla] editVerts=%d faces=%d edges=%d | render verts=%d tris=%d | smooth=%d sharp=%d | sel(%s)=%d\n",
               ev, ef, ee, m->vertexSize, m->facesSize/3, (int)m->meshSmooth, (int)m->sharpEdges.size(), selN, selC);
        return true;
    }

    // ---- log <mensaje libre> ----
    if (cmd == "log") {
        std::string resto; std::getline(ss, resto);
        printf("      %s\n", resto.c_str());
        return true;
    }

    err = "comando desconocido: '" + cmd + "'";
    return false;
}

// ============================================================================
//  Runner: corre el archivo entero, corta al primer fallo
// ============================================================================
bool W3dRunScript(const std::string& path) {
    std::ifstream f(path.c_str());
    if (!f) { printf("FALLO: no se pudo abrir el script '%s'\n", path.c_str()); return false; }
    printf("=== W3dScript: %s ===\n", path.c_str()); fflush(stdout);
    std::string linea; int nLinea = 0, nOk = 0;
    while (std::getline(f, linea)) {
        nLinea++;
        size_t a = linea.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) continue;          // linea vacia
        std::string t = linea.substr(a);
        if (t[0] == '#') continue;                      // comentario
        size_t b = t.find_last_not_of(" \t\r\n");       // saca el CR de Windows
        t = t.substr(0, b + 1);
        std::string err;
        printf("...   [%2d] %s\n", nLinea, t.c_str()); fflush(stdout); // ANTES (por si crashea)
        if (W3dRunCommand(t, err)) {
            printf("OK    [%2d] %s\n", nLinea, t.c_str()); fflush(stdout);
            nOk++;
        } else {
            printf("FALLO [%2d] %s\n           -> %s\n", nLinea, t.c_str(), err.c_str()); fflush(stdout);
            return false;                               // corta al primer fallo
        }
    }
    printf("=== TEST OK: %d comandos ===\n", nOk); fflush(stdout);
    return true;
}
