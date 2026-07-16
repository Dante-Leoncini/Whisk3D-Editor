#include "test/W3dScript.h"
#include <cmath>               // sqrt (skindump)
#include "objects/Objects.h"   // ObjActivo, g_editMesh, EditSelectMode, Sel*, DeseleccionarTodo
#include "objects/Mesh.h"      // NewMesh, MeshType, Mesh
#include "edit/Modifier.h"     // Modifier (params del Mirror en el harness)
#include "edit/MeshEdit.h"     // Nuevo/MoverMeshPart (funciones libres del editor)
#include "objects/EditMesh.h"  // EditMesh
#include "ViewPorts/LayoutInput.h" // LayoutToggleEditMode/ExtrudeFaces, EditXform*
#include "ViewPorts/Timeline.h"
#include "WhiskUI/Propieties/GroupPropertie.h" // icontest: la tarjeta "Keyframe" y su icono
#include "ViewPorts/Properties.h"
#include "ViewPorts/ViewPort3D.h"   // animmenu: la barra del 3D (BR_Animation / BarRolBtn)
bool LayoutAbrirMenuDeBarra(ViewportBase* vp, int mx, int my); // LayoutInput.cpp (animmenu: el camino real del click)           // icontest: crear el panel arma las tarjetas    // dopedump: arma las filas del dope sheet
#include "importers/import_obj.h"   // ExportOBJ
#include "importers/import_fbx.h"  // ImportFBX
#include "objects/ObjectMode.h" // Eliminar (test del borrado + su undo)
#include "objects/Light.h"      // Light::Create + Lights (test del borrado de luces)
#include "objects/Armature.h"   // test de la pestania Animation (clips del esqueleto)
#include "animation/SkeletalAnimation.h" // CrearAnimacion/BorrarAnimacionActiva/MoverAnimacionActiva
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
// distancia punto-segmento (metrica del skincheck: vertices vs hueso)
static float ScriptDistSeg(const Vector3& p, const Vector3& A, const Vector3& B){
    Vector3 d = B - A; float L2 = d.LengthSq();
    float t = (L2 > 1e-9f) ? (p - A).Dot(d) / L2 : 0.0f;
    if (t < 0) t = 0; if (t > 1) t = 1;
    return (p - (A + d * t)).Length();
}

static Mesh* ScriptActiveMesh() {
    if (g_editMesh) return (Mesh*)g_editMesh;
    if (ObjActivo && ObjActivo->getType() == ObjectType::mesh) return (Mesh*)ObjActivo;
    return NULL;
}

// vuelca la escena en arbol (recursivo). Para mallas muestra el # de grupos de vertices (huesos del rig).
static void ScriptDumpEscena(Object* o, int nivel) {
    if (!o) return;
    char sangria[64]; int n = nivel*2; if (n > 62) n = 62;
    for (int i=0;i<n;i++) sangria[i]=' '; sangria[n]='\0';
    Mesh* m = (o->getType()==ObjectType::mesh) ? (Mesh*)o : NULL;
    if (m && !m->vertexGroups.empty()) {
        std::string muestra;
        for (size_t g=0; g<m->vertexGroups.size() && g<8; g++){ if(g) muestra+=","; muestra+=m->vertexGroups[g]->nombre; }
        printf("      [scene] %s'%s' type=%d children=%d vgroups=%d (%s...)\n", sangria, o->name.c_str(),
               (int)o->getType(), (int)o->Childrens.size(), (int)m->vertexGroups.size(), muestra.c_str());
    }
    else
        printf("      [scene] %s'%s' type=%d children=%d\n", sangria, o->name.c_str(), (int)o->getType(), (int)o->Childrens.size());
    for (size_t i=0;i<o->Childrens.size();i++) ScriptDumpEscena(o->Childrens[i], nivel+1);
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

    // ---- addarmature : crea un Armature (2 huesos en cadena) y lo deja activo ----
    if (cmd == "addarmature") {
        Armature* a = new Armature(NULL, Vector3(0,0,0));
        W3dBone r; r.name="root";  r.parent=-1; r.head=Vector3(0,0,0); r.tail=Vector3(0,1,0);
        W3dBone c; c.name="child"; c.parent=0;  c.head=Vector3(0,1,0); c.tail=Vector3(0,2,0);
        a->bones.push_back(r); a->bones.push_back(c);
        DeseleccionarTodo();
        a->Seleccionar();
        return true;
    }
    // ---- animkey <bone> <pos|rot|scale> <frame> <x> <y> <z> : setea un keyframe en el clip activo ----
    if (cmd == "animkey") {
        Armature* a = (ObjActivo && ObjActivo->getType()==ObjectType::armature) ? (Armature*)ObjActivo : NULL;
        if (!a || a->animActiva<0 || a->animActiva>=(int)a->animations.size()) { err="animkey: sin clip activo"; return false; }
        int bone=0, frame=0; std::string prop; float vx=0,vy=0,vz=0; ss>>bone>>prop>>frame>>vx>>vy>>vz;
        int pr = (prop=="pos")?AnimPosition : (prop=="rot")?AnimRotation : (prop=="scale")?AnimScale : -1;
        if (pr<0) { err="animkey: prop debe ser pos|rot|scale"; return false; }
        BoneTrack& tr = a->animations[a->animActiva]->TrackDe(bone);
        // cada componente es una CURVA propia -> se setea en las 3 (una por bloque: PropertyDe puede reallocar)
        { AnimProperty& ap = tr.PropertyDe(pr, AnimX); SetKeyCurva(ap, frame, vx); }
        { AnimProperty& ap = tr.PropertyDe(pr, AnimY); SetKeyCurva(ap, frame, vy); }
        { AnimProperty& ap = tr.PropertyDe(pr, AnimZ); SetKeyCurva(ap, frame, vz); }
        return true;
    }
    // ---- animkeys <bone> <pos|rot|scale> <from> <to> : vuelca los keyframes de esa propiedad, POR COMPONENTE ----
    if (cmd == "animkeys") {
        Armature* a = (ObjActivo && ObjActivo->getType()==ObjectType::armature) ? (Armature*)ObjActivo : NULL;
        if (!a || a->animActiva<0) { err="animkeys: sin clip"; return false; }
        int bone=0, from=0, to=999999; std::string prop; ss>>bone>>prop>>from>>to;
        int pr = (prop=="pos")?AnimPosition : (prop=="rot")?AnimRotation : AnimScale;
        SkeletalAnimation* an = a->animations[a->animActiva];
        const char* cn[3] = {"X","Y","Z"};
        for (size_t t=0;t<an->tracks.size();t++) if (an->tracks[t].bone==bone)
            for (size_t p=0;p<an->tracks[t].Propertys.size();p++) if (an->tracks[t].Propertys[p].Property==pr){
                AnimProperty& ap=an->tracks[t].Propertys[p];
                int c = ap.component; if (c<0||c>2) c=0;
                for (size_t k=0;k<ap.keyframes.size();k++){ int f=ap.keyframes[k].frame; if(f<from||f>to) continue;
                    printf("      [key] bone=%d %s.%s f=%d = %.2f\n", bone, prop.c_str(), cn[c], f, ap.keyframes[k].value); }
            }
        return true;
    }
    // ---- skindump <frame> : skinnea el mesh activo y vuelca el desplazamiento (verifica que no explote) ----
    if (cmd == "skindump") {
        Mesh* m = (ObjActivo && ObjActivo->getType()==ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (!m || !m->skinArmature) { err="skindump: mesh sin skinArmature"; return false; }
        int f=1; ss>>f; CurrentFrame=f; m->lastSkinFrame=-999999;
        SkinearMesh(m);
        if (!m->skinVertex) { err="skindump: sin skinVertex"; return false; }
        int nv=m->vertexSize; double maxd=0, sum=0;
        for (int i=0;i<nv;i++){ double dx=m->skinVertex[i*3]-m->vertex[i*3], dy=m->skinVertex[i*3+1]-m->vertex[i*3+1], dz=m->skinVertex[i*3+2]-m->vertex[i*3+2];
            double d=dx*dx+dy*dy+dz*dz; if(d>maxd)maxd=d; sum+=d; }
        printf("      [skin] f=%d nv=%d maxDelta=%.2f avgDelta=%.2f v0=(%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f)\n",
               f, nv, (float)(maxd>0?sqrt(maxd):0), (float)(nv?sqrt(sum/nv):0), // sqrt: __builtin_sqrt no existe en MSVC
               m->vertex[0],m->vertex[1],m->vertex[2], m->skinVertex[0],m->skinVertex[1],m->skinVertex[2]);
        return true;
    }
    // ---- xforms : world matrix de la malla activa y de su esqueleto (para cazar el OFFSET malla<->armature) ----
    if (cmd == "xforms") {
        Mesh* m = (ObjActivo && ObjActivo->getType()==ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (!m || !m->skinArmature) { err="xforms: mesh sin skinArmature"; return false; }
        Matrix4 MW, AW; m->GetWorldMatrix(MW); m->skinArmature->GetWorldMatrix(AW);
        printf("      [xf] mesh world T=(%.3f,%.3f,%.3f) diag=(%.3f,%.3f,%.3f)\n", MW.m[12],MW.m[13],MW.m[14], MW.m[0],MW.m[5],MW.m[10]);
        printf("      [xf] arm  world T=(%.3f,%.3f,%.3f) diag=(%.3f,%.3f,%.3f)\n", AW.m[12],AW.m[13],AW.m[14], AW.m[0],AW.m[5],AW.m[10]);
        printf("      [xf] mesh pos=(%.3f,%.3f,%.3f) scale=(%.3f,%.3f,%.3f)\n", m->pos.x,m->pos.y,m->pos.z, m->scale.x,m->scale.y,m->scale.z);
        printf("      [xf] arm  pos=(%.3f,%.3f,%.3f) scale=(%.3f,%.3f,%.3f)\n", m->skinArmature->pos.x,m->skinArmature->pos.y,m->skinArmature->pos.z, m->skinArmature->scale.x,m->skinArmature->scale.y,m->skinArmature->scale.z);
        return true;
    }
    // ---- bindcheck : compara el bind del FK-rest (NodeToYup del FK con Lcl de rest) contra el TransformLink real
    //      del FBX (b.head/b.bind). Si difieren, la malla (skinneada al TransformLink) queda OFFSET de los huesos FK. ----
    if (cmd == "bindcheck") {
        Armature* a = (ObjActivo && ObjActivo->getType()==ObjectType::armature) ? (Armature*)ObjActivo : NULL;
        if (!a) { err="bindcheck: sin armature activo"; return false; }
        int save=a->animActiva; a->animActiva=-1; a->lastPoseFrame=-999999;
        EvaluarPoseEsqueleto(a, 1); // rest FK -> poseHead = NodeToYup(FK-rest.origin)
        a->animActiva=save; a->lastPoseFrame=-999999;
        double sum=0; float worst=0; int worstB=-1, n=0;
        for (size_t i=0;i<a->bones.size();i++){
            if (!a->bones[i].hasSkin) continue;
            Vector3 tl(a->bones[i].bind.m[12], a->bones[i].bind.m[13], a->bones[i].bind.m[14]); // TransformLink origin
            float d=(a->bones[i].poseHead - tl).Length(); sum+=d; n++;
            if (d>worst){ worst=d; worstB=(int)i; }
        }
        printf("      [bind] bones=%d  |FK-rest - TransformLink| avg=%.3f worst=%.3f (%s)\n",
               n, n?sum/n:0.0, worst, worstB>=0?a->bones[worstB].name.c_str():"-");
        // pivots/offsets FBX: si son != 0, el FK debe aplicarlos (sino no coincide con el TransformLink)
        { int nPiv=0; for (size_t i=0;i<a->bones.size();i++){ W3dBone& b=a->bones[i];
            if (b.rotPivot.Length()>0.01f||b.rotOffset.Length()>0.01f||b.sclPivot.Length()>0.01f||b.postRot.Length()>0.01f) nPiv++; }
          printf("      [bind] huesos con pivot/offset/postRot != 0: %d de %d\n", nPiv, (int)a->bones.size());
          if (worstB>=0){ W3dBone& b=a->bones[worstB];
            printf("      [bind]   %s rotPivot=(%.2f,%.2f,%.2f) rotOffset=(%.2f,%.2f,%.2f) postRot=(%.2f,%.2f,%.2f) preRot=(%.2f,%.2f,%.2f)\n",
                b.name.c_str(), b.rotPivot.x,b.rotPivot.y,b.rotPivot.z, b.rotOffset.x,b.rotOffset.y,b.rotOffset.z,
                b.postRot.x,b.postRot.y,b.postRot.z, b.preRot.x,b.preRot.y,b.preRot.z); } }
        if (worstB>=0){ Vector3 tl(a->bones[worstB].bind.m[12],a->bones[worstB].bind.m[13],a->bones[worstB].bind.m[14]);
            printf("      [bind]   %s FK-rest=(%.2f,%.2f,%.2f) TransformLink=(%.2f,%.2f,%.2f)\n", a->bones[worstB].name.c_str(),
                   a->bones[worstB].poseHead.x,a->bones[worstB].poseHead.y,a->bones[worstB].poseHead.z, tl.x,tl.y,tl.z); }
        return true;
    }
    // ---- armdump : auto-encuentra el armature, evalua la pose REST y dumpea por hueso: parent, restR, origen del
    //      FK-rest (poseHead, espacio nodo) y origen del TransformLink (bind real). Ver si el FK-rest esta DISPARADO. ----
    if (cmd == "armdump") {
        Armature* a=NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::armature && !a) a=(Armature*)o;
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if (!a) { err="armdump: sin armature"; return false; }
        int save=a->animActiva; a->animActiva=-1; a->lastPoseFrame=-999999;
        EvaluarPoseEsqueleto(a, 1); // rest FK -> poseHead = origen del FK-rest (espacio nodo)
        a->animActiva=save; a->lastPoseFrame=-999999;
        int N=(int)a->bones.size();
        // bounding de los origenes FK-rest y de los TransformLink (para ver dispersion = "disparo")
        Vector3 fmn(1e9f,1e9f,1e9f), fmx(-1e9f,-1e9f,-1e9f), tmn(1e9f,1e9f,1e9f), tmx(-1e9f,-1e9f,-1e9f);
        for (int i=0;i<N;i++){ W3dBone& b=a->bones[i]; if(!b.hasSkin) continue;
            Vector3 fk=b.poseHead, tl(b.bind.m[12],b.bind.m[13],b.bind.m[14]);
            if(fk.x<fmn.x)fmn.x=fk.x; if(fk.y<fmn.y)fmn.y=fk.y; if(fk.z<fmn.z)fmn.z=fk.z;
            if(fk.x>fmx.x)fmx.x=fk.x; if(fk.y>fmx.y)fmx.y=fk.y; if(fk.z>fmx.z)fmx.z=fk.z;
            if(tl.x<tmn.x)tmn.x=tl.x; if(tl.y<tmn.y)tmn.y=tl.y; if(tl.z<tmn.z)tmn.z=tl.z;
            if(tl.x>tmx.x)tmx.x=tl.x; if(tl.y>tmx.y)tmx.y=tl.y; if(tl.z>tmx.z)tmx.z=tl.z; }
        printf("      [armdump] %d huesos. armPos=(%.2f,%.2f,%.2f) armScale=%.4f skinUsaBind=%d\n",
               N, a->pos.x,a->pos.y,a->pos.z, a->scale.x, a->skinUsaBind?1:0);
        printf("      [armdump] FK-rest bbox=(%.1f,%.1f,%.1f)..(%.1f,%.1f,%.1f)  TL bbox=(%.1f,%.1f,%.1f)..(%.1f,%.1f,%.1f)\n",
               fmn.x,fmn.y,fmn.z,fmx.x,fmx.y,fmx.z, tmn.x,tmn.y,tmn.z,tmx.x,tmx.y,tmx.z);
        int lim = N<12?N:12;
        for (int i=0;i<lim;i++){ W3dBone& b=a->bones[i];
            Vector3 tl(b.bind.m[12],b.bind.m[13],b.bind.m[14]);
            printf("      [armdump] %2d p=%2d restR=(%.0f,%.0f,%.0f) restT=(%.1f,%.1f,%.1f) FK=(%.1f,%.1f,%.1f) TL=(%.1f,%.1f,%.1f) '%s'\n",
                i, b.parent, b.restR.x,b.restR.y,b.restR.z, b.restT.x,b.restT.y,b.restT.z,
                b.poseHead.x,b.poseHead.y,b.poseHead.z, tl.x,tl.y,tl.z, b.name.c_str()); }
        // escala de las columnas de tlNode / restLcl de un hueso medio -> ver escala bakeada (rompe el delta)
        if (N > 3){ W3dBone& b=a->bones[3];
            float ts0=Vector3(b.tlNode.m[0],b.tlNode.m[1],b.tlNode.m[2]).Length(), ts1=Vector3(b.tlNode.m[4],b.tlNode.m[5],b.tlNode.m[6]).Length(), ts2=Vector3(b.tlNode.m[8],b.tlNode.m[9],b.tlNode.m[10]).Length();
            printf("      [armdump] reconstruirFK=%d  tlNode[3] escCols=(%.3f,%.3f,%.3f)  restS[3]=(%.3f,%.3f,%.3f)\n",
                a->skinReconstruirFK?1:0, ts0,ts1,ts2, b.restS.x,b.restS.y,b.restS.z);
        }
        // FRAME ANIMADO: evaluar con el clip a un frame medio -> ver si el FK se DISPARA al animar (bbox gigante)
        if (save >= 0){
            a->lastPoseFrame=-999999; EvaluarPoseEsqueleto(a, 8);
            Vector3 amn(1e9f,1e9f,1e9f), amx(-1e9f,-1e9f,-1e9f); float worst=0; int worstB=-1;
            for (int i=0;i<N;i++){ W3dBone& b=a->bones[i]; if(!b.hasSkin) continue; Vector3 fk=b.poseHead;
                if(fk.x<amn.x)amn.x=fk.x; if(fk.y<amn.y)amn.y=fk.y; if(fk.z<amn.z)amn.z=fk.z;
                if(fk.x>amx.x)amx.x=fk.x; if(fk.y>amx.y)amx.y=fk.y; if(fk.z>amx.z)amx.z=fk.z;
                if(fk.Length()>worst){worst=fk.Length();worstB=i;} }
            printf("      [armdump] FRAME 8 (animado) FK bbox=(%.0f,%.0f,%.0f)..(%.0f,%.0f,%.0f)  peor=%.0f (%s)\n",
                   amn.x,amn.y,amn.z,amx.x,amx.y,amx.z, worst, (worstB>=0)?a->bones[worstB].name.c_str():"-");
            a->lastPoseFrame=-999999;
        }
        // DIAG: bbox de la malla skinneada (local) + clusterTransform de un hueso -> ver el mismatch malla<->bind
        { Mesh* m=NULL; std::vector<Object*> st2; if(SceneCollection) st2.push_back(SceneCollection);
          while(!st2.empty()){ Object* o=st2.back(); st2.pop_back();
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature && !m) m=(Mesh*)o;
            for(size_t i=0;i<o->Childrens.size();i++) st2.push_back(o->Childrens[i]); }
          if (m && m->vertex){ Vector3 vmn(1e9f,1e9f,1e9f), vmx(-1e9f,-1e9f,-1e9f);
            for (int i=0;i<m->vertexSize;i++){ float x=m->vertex[i*3],y=m->vertex[i*3+1],z=m->vertex[i*3+2];
              if(x<vmn.x)vmn.x=x;if(y<vmn.y)vmn.y=y;if(z<vmn.z)vmn.z=z; if(x>vmx.x)vmx.x=x;if(y>vmx.y)vmx.y=y;if(z>vmx.z)vmx.z=z; }
            printf("      [armdump] MESH '%s' vertBbox=(%.1f,%.1f,%.1f)..(%.1f,%.1f,%.1f) diag=%.1f\n",
              m->name.c_str(), vmn.x,vmn.y,vmn.z,vmx.x,vmx.y,vmx.z, (vmx-vmn).Length()); }
          if (N>3){ W3dBone& b=a->bones[3]; Matrix4& ct=b.clusterTransform;
            float cs0=Vector3(ct.m[0],ct.m[1],ct.m[2]).Length(), cs1=Vector3(ct.m[4],ct.m[5],ct.m[6]).Length(), cs2=Vector3(ct.m[8],ct.m[9],ct.m[10]).Length();
            printf("      [armdump] clusterTransform[3] t=(%.1f,%.1f,%.1f) escCols=(%.3f,%.3f,%.3f)\n", ct.m[12],ct.m[13],ct.m[14], cs0,cs1,cs2); }
        }
        return true;
    }
    // ---- skelexport <substr> <frame> <file> : activa el clip, evalua el frame y vuelca por hueso: idx parent
    //      headX headY headZ tailX tailY tailZ |poseT-restT|. Para dibujar el ESQUELETO (stick figure) y ver si la
    //      pose es coherente (persona) o basura, y cuantos huesos TRASLADAN (Tdelta del root motion). ----
    if (cmd == "skelexport") {
        std::string sub, path; int frame=1; ss>>sub>>frame>>path;
        if (path.empty()){ err="skelexport: uso: skelexport <substr> <frame> <file>"; return false; }
        extern int ActiveAnimKind; extern Armature* ActiveAnimArm;
        Armature* a=NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::armature && !a) a=(Armature*)o;
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if (!a){ err="skelexport: sin armature"; return false; }
        int clip=-1; for (size_t i=0;i<a->animations.size();i++) if (sub.empty()||a->animations[i]->name.find(sub)!=std::string::npos){ clip=(int)i; break; }
        if (clip<0){ err="skelexport: clip no encontrado"; return false; }
        ActiveAnimKind=1; ActiveAnimArm=a; a->animActiva=clip;
        CurrentFrame=frame; a->lastPoseFrame=-999999; EvaluarPoseEsqueleto(a, frame);
        FILE* fp=fopen(path.c_str(),"w"); if(!fp){ err="skelexport: no pude abrir "+path; return false; }
        int nT=0;
        for (size_t i=0;i<a->bones.size();i++){ W3dBone& b=a->bones[i];
            float dt=(b.poseT-b.restT).Length(); if (dt>0.5f) nT++;
            fprintf(fp,"%d %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", (int)i, b.parent,
                b.poseHead.x,b.poseHead.y,b.poseHead.z, b.poseTail.x,b.poseTail.y,b.poseTail.z, dt); }
        fclose(fp);
        printf("      [skelexport] clip='%s' f=%d bones=%d huesosQueTrasladan(dt>0.5)=%d -> %s\n",
               a->animations[clip]->name.c_str(), frame, (int)a->bones.size(), nT, path.c_str());
        return true;
    }
    // ---- animtrace <substr> : reproduce el clip SECUENCIALMENTE (2 vueltas, como el play) y por frame vuelca el bbox
    //      del esqueleto + la deformacion max de la malla. Detecta: (a) acumulacion (vuelta2 != vuelta1), (b) explosion
    //      de la deformacion (maxDelta gigante = "super deforme"), (c) sin root motion (bbox centro fijo). ----
    if (cmd == "animtrace") {
        std::string sub; std::getline(ss, sub);
        { size_t p=sub.find_first_not_of(" \t"); sub = (p==std::string::npos)?"":sub.substr(p); }
        extern int ActiveAnimKind; extern Armature* ActiveAnimArm;
        Mesh* m=NULL; Armature* a=NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature && !m){ m=(Mesh*)o; a=m->skinArmature; }
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if (!m || !a) { err="animtrace: sin malla skinneada"; return false; }
        int clip=-1; for (size_t i=0;i<a->animations.size();i++) if (sub.empty()||a->animations[i]->name.find(sub)!=std::string::npos){ clip=(int)i; break; }
        if (clip<0){ err="animtrace: clip no encontrado"; return false; }
        ActiveAnimKind=1; ActiveAnimArm=a; a->animActiva=clip;
        SkeletalAnimation* an=a->animations[clip];
        int f0=an->startFrame, f1=an->endFrame; if (f1<=f0) f1=f0+12;
        printf("      [animtrace] clip[%d]='%s' start=%d end=%d fps=%d\n", clip, an->name.c_str(), f0, f1, an->FrameRate);
        for (int vuelta=0; vuelta<2; vuelta++){
            for (int f=f0; f<=f1; f++){
                CurrentFrame=f; a->lastPoseFrame=-999999; m->lastSkinFrame=-999999; m->skinPoseSerial=0; a->poseSerial++;
                SkinearMesh(m);
                // bbox del esqueleto (poseHead) + peor largo
                Vector3 mn(1e9f,1e9f,1e9f), mx(-1e9f,-1e9f,-1e9f); float worst=0; int wb=-1;
                for (size_t i=0;i<a->bones.size();i++){ if(!a->bones[i].hasSkin) continue; Vector3 h=a->bones[i].poseHead;
                    if(h.x<mn.x)mn.x=h.x;if(h.y<mn.y)mn.y=h.y;if(h.z<mn.z)mn.z=h.z; if(h.x>mx.x)mx.x=h.x;if(h.y>mx.y)mx.y=h.y;if(h.z>mx.z)mx.z=h.z;
                    if(h.Length()>worst){worst=h.Length();wb=(int)i;} }
                // deformacion de la malla: max|skinVertex - vertex|
                double maxd=0; if (m->skinVertex) for (int i=0;i<m->vertexSize;i++){
                    double dx=m->skinVertex[i*3]-m->vertex[i*3],dy=m->skinVertex[i*3+1]-m->vertex[i*3+1],dz=m->skinVertex[i*3+2]-m->vertex[i*3+2];
                    double d=dx*dx+dy*dy+dz*dz; if(d>maxd)maxd=d; }
                if (f==f0 || f==(f0+f1)/2 || f==f1)
                    printf("      [animtrace] v%d f=%d skelBbox=(%.0f,%.0f,%.0f)..(%.0f,%.0f,%.0f) peorHueso=%.0f(%s) maxDeform=%.1f\n",
                        vuelta, f, mn.x,mn.y,mn.z,mx.x,mx.y,mx.z, worst, wb>=0?a->bones[wb].name.c_str():"-", (float)sqrt(maxd));
            }
        }
        return true;
    }
    // ---- rootmotion <substr> : busca el clip cuyo nombre contiene <substr>, lo activa (gating) y por varios frames
    //      vuelca los 3 huesos con mayor |poseT-restT| (= donde vive el ROOT MOTION del Lcl Translation) + su poseHead.
    //      Sirve para ver si la traslacion animada existe en el Lcl y a que ESCALA (para el Tdelta del biped). ----
    if (cmd == "rootmotion") {
        std::string sub; std::getline(ss, sub);
        { size_t p=sub.find_first_not_of(" \t"); sub = (p==std::string::npos)?"":sub.substr(p); }
        Armature* a=NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::armature && !a) a=(Armature*)o;
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if (!a) { err="rootmotion: sin armature"; return false; }
        int clipIdx=-1;
        for (size_t i=0;i<a->animations.size();i++){ if (sub.empty() || a->animations[i]->name.find(sub)!=std::string::npos){ clipIdx=(int)i; break; } }
        if (clipIdx<0) { err="rootmotion: clip no encontrado ('"+sub+"')"; return false; }
        extern int ActiveAnimKind; extern Armature* ActiveAnimArm;
        ActiveAnimKind=1; ActiveAnimArm=a; a->animActiva=clipIdx;
        SkeletalAnimation* an=a->animations[clipIdx];
        printf("      [rootm] clip[%d]='%s' start=%d end=%d fps=%d figureScale=%.3f\n",
               clipIdx, an->name.c_str(), an->startFrame, an->endFrame, an->FrameRate, a->figureScale);
        int f0 = an->startFrame, f1 = an->endFrame; if (f1<=f0) f1=f0+12;
        int frames[5]; for (int k=0;k<5;k++) frames[k]=f0 + (f1-f0)*k/4;
        for (int k=0;k<5;k++){ int f=frames[k]; a->lastPoseFrame=-999999; EvaluarPoseEsqueleto(a, f);
            // top-3 huesos por |poseT-restT|
            int b0=-1,b1=-1,b2=-1; float d0=-1,d1=-1,d2=-1;
            for (size_t i=0;i<a->bones.size();i++){ float d=(a->bones[i].poseT - a->bones[i].restT).Length();
                if (d>d0){ d2=d1;b2=b1; d1=d0;b1=b0; d0=d;b0=(int)i; }
                else if (d>d1){ d2=d1;b2=b1; d1=d;b1=(int)i; }
                else if (d>d2){ d2=d;b2=(int)i; } }
            printf("      [rootm] f=%d:\n", f);
            int bs[3]={b0,b1,b2};
            for (int j=0;j<3;j++){ int i=bs[j]; if(i<0) continue; W3dBone& b=a->bones[i];
                printf("      [rootm]   #%d '%s' |dT|=%.2f restT=(%.2f,%.2f,%.2f) poseT=(%.2f,%.2f,%.2f) poseHead=(%.1f,%.1f,%.1f)\n",
                    i, b.name.c_str(), (b.poseT-b.restT).Length(), b.restT.x,b.restT.y,b.restT.z,
                    b.poseT.x,b.poseT.y,b.poseT.z, b.poseHead.x,b.poseHead.y,b.poseHead.z); }
        }
        return true;
    }
    // ---- posekey : posa el hueso 5 (poseR=Y45), inserta keyframe en frame 10, re-evalua y verifica el roundtrip ----
    if (cmd == "posekey") {
        Armature* a = (ObjActivo && ObjActivo->getType()==ObjectType::armature) ? (Armature*)ObjActivo : NULL;
        if (!a || a->bones.size() < 6) { err="posekey: sin armature (o <6 huesos)"; return false; }
        a->boneActivo = 5; a->bones[5].select = true;
        CurrentFrame = 10; a->lastPoseFrame = -999999; EvaluarPoseEsqueleto(a, 10); // pose base del frame
        a->bones[5].poseR = a->bones[5].poseR + Vector3(0, 45, 0); // posar: +45 en Y
        a->poseDirty = true;
        InsertarKeyframeEsqueleto(a); // guarda la pose en la curva
        // re-evaluar DESDE la curva en frame 10 -> debe reproducir el poseR guardado
        a->lastPoseFrame = -999999; a->poseDirty = false;
        Vector3 antes = a->bones[5].poseR;
        EvaluarPoseEsqueleto(a, 10);
        printf("      [posekey] bone5 poseR guardado=(%.1f,%.1f,%.1f) reevaluado=(%.1f,%.1f,%.1f) clips=%d\n",
               antes.x,antes.y,antes.z, a->bones[5].poseR.x,a->bones[5].poseR.y,a->bones[5].poseR.z, (int)a->animations.size());
        return true;
    }
    // ---- rigidcheck <frame> : mide si cada PIEZA (vertex group) se deforma RIGIDA (preserva forma). Un skinA con
    //      escala/shear mal distorsiona la pieza aunque su CENTRO quede en el hueso (el drama de LISA). ratio~1 = rigido. ----
    if (cmd == "rigidcheck") {
        Mesh* m = (ObjActivo && ObjActivo->getType()==ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (!m || !m->skinArmature) { err="rigidcheck: mesh sin skinArmature"; return false; }
        int f=20; ss>>f; CurrentFrame=f; m->lastSkinFrame=-999999; SkinearMesh(m);
        if (!m->skinVertex) { err="rigidcheck: sin skinVertex"; return false; }
        int nv=m->vertexSize;
        std::map<int,int> rep; for (int ri=0; ri<nv; ri++) if (!rep.count(m->vertCtrlPoint[ri])) rep[m->vertCtrlPoint[ri]]=ri;
        double sumR=0; int nPairs=0; double worst=0; int worstG=-1;
        for (size_t g=0; g<m->vertexGroups.size(); g++){ VertexGroup* vg=m->vertexGroups[g];
            // pares de verts DENTRO de la pieza: dist skinneada / dist bind (deberia ser ~1 si es rigido)
            std::vector<int> ris; for (size_t j=0;j<vg->verts.size();j++){ std::map<int,int>::iterator it=rep.find(vg->verts[j]); if (it!=rep.end()) ris.push_back(it->second); }
            double gR=0; int gN=0;
            for (size_t a=0;a<ris.size() && a<40;a++) for (size_t b=a+1;b<ris.size() && b<40;b++){
                int ra=ris[a], rb=ris[b];
                double bx=m->vertex[ra*3]-m->vertex[rb*3], by=m->vertex[ra*3+1]-m->vertex[rb*3+1], bz=m->vertex[ra*3+2]-m->vertex[rb*3+2];
                double sx=m->skinVertex[ra*3]-m->skinVertex[rb*3], sy=m->skinVertex[ra*3+1]-m->skinVertex[rb*3+1], sz=m->skinVertex[ra*3+2]-m->skinVertex[rb*3+2];
                double db=sqrt(bx*bx+by*by+bz*bz), ds=sqrt(sx*sx+sy*sy+sz*sz);
                if (db>1e-3){ double r=ds/db; sumR+=r; nPairs++; gR+=r; gN++; } }
            if (gN>0){ double avg=gR/gN; double dev=avg>1?avg-1:1-avg; if (dev>worst){ worst=dev; worstG=(int)g; } }
        }
        printf("      [rigid] f=%d pares=%d ratioAvg=%.3f (1.0=rigido perfecto) peorGrupo='%s' devRatio=%.3f\n",
               f, nPairs, nPairs?sumR/nPairs:0.0, worstG>=0?m->vertexGroups[worstG]->nombre.c_str():"-", worst);
        return true;
    }
    // ---- matdump : escala de skinA / skinMatrix / bind / clusterTransform para ubicar la escala 0.01 de LISA ----
    if (cmd == "matdump") {
        Armature* a = (ObjActivo && ObjActivo->getType()==ObjectType::armature) ? (Armature*)ObjActivo : NULL;
        if (!a || a->bones.size()<6) { err="matdump: sin armature"; return false; }
        a->lastPoseFrame=-999999; EvaluarPoseEsqueleto(a, 20);
        for (int bi=3; bi<6 && bi<(int)a->bones.size(); bi++){ W3dBone& b=a->bones[bi];
            // escala aproximada = largo de la columna X (col-major m[0..2])
            #define COLLEN(M,c) sqrt((double)M.m[(c)*4]*M.m[(c)*4]+(double)M.m[(c)*4+1]*M.m[(c)*4+1]+(double)M.m[(c)*4+2]*M.m[(c)*4+2])
            printf("      [mat] %s bind|X|=%.3f cluster|X|=%.3f skinA|X|=%.3f skinMatrix|X|=%.3f\n",
                   b.name.c_str(), COLLEN(b.bind,0), COLLEN(b.clusterTransform,0), COLLEN(b.skinA,0), COLLEN(b.skinMatrix,0));
            // worldFK = skinMatrix * inv(skinA)
            Matrix4 wf = b.skinMatrix * b.skinA.Inverse();
            printf("      [mat]   worldFK|X|=%.3f restS=(%.2f,%.2f,%.2f) poseS=(%.2f,%.2f,%.2f) restT=(%.0f,%.0f,%.0f)\n",
                   COLLEN(wf,0), b.restS.x,b.restS.y,b.restS.z, b.poseS.x,b.poseS.y,b.poseS.z, b.restT.x,b.restT.y,b.restT.z);
        }
        return true;
    }
    // ---- eulertest : roundtrip euler->matriz->euler (verifica SkelMatRotEuler / SkelMatrizAEulerFBX del transform de pose) ----
    if (cmd == "eulertest") {
        Vector3 casos[4] = { Vector3(30,-45,60), Vector3(10,20,30), Vector3(-80,15,-25), Vector3(0,89,0) };
        for (int i=0;i<4;i++){ Vector3 e=casos[i]; Matrix4 M=SkelMatRotEuler(e,0); Vector3 o=SkelMatrizAEulerFBX(M,0);
            // re-armar desde la salida y comparar las MATRICES (el euler puede diferir por equivalencia +-360/gimbal)
            Matrix4 M2=SkelMatRotEuler(o,0); float d=0; for(int k=0;k<16;k++){ float df=M.m[k]-M2.m[k]; d+=df*df; }
            printf("      [euler] in=(%.1f,%.1f,%.1f) out=(%.1f,%.1f,%.1f) matDiff=%.4f\n", e.x,e.y,e.z, o.x,o.y,o.z, d); }
        return true;
    }
    // ---- skinformula <0|1> : elige la formula de skinning (A/B testing headless) ----
    if (cmd == "skinformula") { int n=1; ss>>n; g_skinFormula = n; return true; }
    // ---- skincheck <frame> : POR HUESO, distancia del centroide (ponderado) de sus vertices skinneados al
    //      segmento del hueso animado. Si el skinning es correcto, cada grupo queda PEGADO a su hueso.
    //      'rest' = misma medida en bind (verts originales vs hueso en FK-rest): baseline del archivo. ----
    if (cmd == "skincheck") {
        Mesh* m = (ObjActivo && ObjActivo->getType()==ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (!m || !m->skinArmature) { err="skincheck: mesh sin skinArmature"; return false; }
        Armature* a = m->skinArmature;
        int f=1; ss>>f;
        int nv = m->vertexSize;
        if ((int)m->vertCtrlPoint.size() < nv) { err="skincheck: sin vertCtrlPoint"; return false; }
        // representante render-vert de cada control-point (posiciones duplicadas por corner son identicas)
        std::map<int,int> rep;
        for (int ri=0; ri<nv; ri++) if (!rep.count(m->vertCtrlPoint[ri])) rep[m->vertCtrlPoint[ri]] = ri;
        // pose BIND -> baseline. Si el skinning usa el TransformLink real (banana), el bind es b.head/b.tail
        // (donde fue skinneada la malla), NO la Lcl-rest del FK. Sino (LISA) el baseline es el FK-rest.
        int animSave = a->animActiva; a->animActiva = -1; a->lastPoseFrame = -999999;
        EvaluarPoseEsqueleto(a, f);
        std::vector<Vector3> rH(a->bones.size()), rT(a->bones.size());
        for (size_t b=0;b<a->bones.size();b++){
            if (a->skinUsaBind){ rH[b]=a->bones[b].head; rT[b]=a->bones[b].tail; }   // bind real (TransformLink)
            else               { rH[b]=a->bones[b].poseHead; rT[b]=a->bones[b].poseTail; } // FK-rest (LISA)
        }
        // pose ANIMADA + skin
        a->animActiva = animSave; a->lastPoseFrame = -999999; m->lastSkinFrame = -999999;
        CurrentFrame = f;
        SkinearMesh(m);
        if (!m->skinVertex) { err="skincheck: sin skinVertex"; return false; }
        // por vertex group (= hueso): centroides ponderados, raw y skinneado (NodeToYup = (x,z,-y) como el display)
        double sumErr=0, sumRest=0; int nb=0; float worstE=-1; std::string worstN;
        for (size_t g=0; g<m->vertexGroups.size(); g++){
            VertexGroup* vg = m->vertexGroups[g];
            int bi=-1; for (size_t b=0;b<a->bones.size();b++) if (a->bones[b].name==vg->nombre){ bi=(int)b; break; }
            if (bi<0) continue;
            Vector3 accS(0,0,0), accR(0,0,0); float wsum=0;
            for (size_t j=0;j<vg->verts.size() && j<vg->pesos.size();j++){
                std::map<int,int>::iterator it = rep.find(vg->verts[j]); if (it==rep.end()) continue;
                int ri = it->second; float w = vg->pesos[j];
                accS += Vector3(m->skinVertex[ri*3],m->skinVertex[ri*3+1],m->skinVertex[ri*3+2])*w;
                accR += Vector3(m->vertex[ri*3],   m->vertex[ri*3+1],   m->vertex[ri*3+2])*w;
                wsum += w;
            }
            if (wsum < 0.01f) continue;
            accS = accS*(1.0f/wsum); accR = accR*(1.0f/wsum);
            Vector3 pS(accS.x, accS.z, -accS.y), pR(accR.x, accR.z, -accR.y); // NodeToYup
            float eA = ScriptDistSeg(pS, a->bones[bi].poseHead, a->bones[bi].poseTail);
            float eR = ScriptDistSeg(pR, rH[bi], rT[bi]);
            sumErr+=eA; sumRest+=eR; nb++;
            if (eA > worstE){ worstE = eA; worstN = vg->nombre; }
            if (eA > 15.0f) printf("      [skchk] %-16s w=%.0f rest=%.1f anim=%.1f\n", vg->nombre.c_str(), wsum, eR, eA);
        }
        printf("      [skchk] f=%d formula=%d bones=%d restAvg=%.2f animAvg=%.2f worst=%.1f (%s)\n",
               f, g_skinFormula, nb, nb?sumRest/nb:0.0, nb?sumErr/nb:0.0, worstE, worstN.c_str());
        // ---- caza de VERTICES SUELTOS: sin peso (quedan en bind -> spikes) o lejos de TODO hueso ----
        { std::vector<float> wTot(nv, 0.0f); int cpMax=-1, gvMax=-1;
          for (int ri=0; ri<nv; ri++) if (m->vertCtrlPoint[ri] > cpMax) cpMax = m->vertCtrlPoint[ri];
          std::map<int,float> cpWt;
          for (size_t g=0; g<m->vertexGroups.size(); g++){ VertexGroup* vg=m->vertexGroups[g];
              for (size_t j=0;j<vg->verts.size() && j<vg->pesos.size();j++){
                  if (vg->verts[j] > gvMax) gvMax = vg->verts[j];
                  cpWt[vg->verts[j]] += vg->pesos[j]; } }
          int sinPeso=0; for (int ri=0; ri<nv; ri++){ std::map<int,float>::iterator it=cpWt.find(m->vertCtrlPoint[ri]);
              wTot[ri] = (it!=cpWt.end())?it->second:0.0f; if (wTot[ri] < 0.01f) sinPeso++; }
          int voladores=0; float peor=0; int peorRi=-1;
          for (int ri=0; ri<nv; ri++){
              Vector3 p(m->skinVertex[ri*3], m->skinVertex[ri*3+2], -m->skinVertex[ri*3+1]); // NodeToYup
              float mind=1e9f;
              for (size_t b=0;b<a->bones.size();b++){ float d=ScriptDistSeg(p, a->bones[b].poseHead, a->bones[b].poseTail); if (d<mind) mind=d; }
              if (mind > 40.0f){ voladores++; if (mind>peor){ peor=mind; peorRi=ri; } }
          }
          printf("      [skchk] nv=%d sinPeso=%d voladores(>40)=%d peor=%.1f cpMax=%d grpVertMax=%d\n",
                 nv, sinPeso, voladores, peor, cpMax, gvMax);
          // ---- MULTI-HUESO: cp -> lista de (hueso,peso). Aisla los verts influidos por >=2 huesos y mide
          //      su distancia a SUS PROPIOS huesos (no al mas cercano). Es lo que Dante ve romperse (rodillas/codos). ----
          { std::map<int, std::vector<std::pair<int,float> > > cpB;
            for (size_t g=0; g<m->vertexGroups.size(); g++){ VertexGroup* vg=m->vertexGroups[g];
                int bi=-1; for (size_t b=0;b<a->bones.size();b++) if (a->bones[b].name==vg->nombre){ bi=(int)b; break; }
                if (bi<0) continue;
                for (size_t j=0;j<vg->verts.size() && j<vg->pesos.size();j++)
                    if (vg->pesos[j] > 0.001f) cpB[vg->verts[j]].push_back(std::make_pair(bi, vg->pesos[j])); }
            int nMulti=0, nMultiMal=0; float peorM=0; int peorMri=-1; double sumDevM=0;
            for (int ri=0; ri<nv; ri++){
                std::map<int, std::vector<std::pair<int,float> > >::iterator it=cpB.find(m->vertCtrlPoint[ri]);
                if (it==cpB.end() || it->second.size() < 2) continue; // solo multi-hueso
                nMulti++;
                // hueso PRIMARIO (mayor peso) del vert
                int bi=it->second[0].first; float wmax=it->second[0].second;
                for (size_t k=1;k<it->second.size();k++) if (it->second[k].second>wmax){ wmax=it->second[k].second; bi=it->second[k].first; }
                Vector3 pS(m->skinVertex[ri*3], m->skinVertex[ri*3+2], -m->skinVertex[ri*3+1]); // NodeToYup skin
                Vector3 pR(m->vertex[ri*3],    m->vertex[ri*3+2],    -m->vertex[ri*3+1]);        // NodeToYup bind
                // DEV = cuanto cambia su distancia al hueso primario (rest vs anim). Un vert bien skinneado la conserva.
                float dev = ScriptDistSeg(pS, a->bones[bi].poseHead, a->bones[bi].poseTail)
                          - ScriptDistSeg(pR, rH[bi], rT[bi]);
                if (dev<0) dev=-dev; sumDevM += dev;
                if (dev > 10.0f){ nMultiMal++; if (dev>peorM){ peorM=dev; peorMri=ri; } }
            }
            printf("      [skchk] MULTI-hueso=%d devAvg=%.2f rotos(dev>10)=%d peor=%.1f\n",
                   nMulti, nMulti?sumDevM/nMulti:0.0, nMultiMal, peorM);
            if (peorMri>=0){ int cp=m->vertCtrlPoint[peorMri];
                printf("      [skchk]   peorMulti ri=%d cp=%d skin=(%.1f,%.1f,%.1f)\n", peorMri, cp,
                       m->skinVertex[peorMri*3],m->skinVertex[peorMri*3+1],m->skinVertex[peorMri*3+2]);
                std::map<int, std::vector<std::pair<int,float> > >::iterator it=cpB.find(cp);
                if (it!=cpB.end()) for (size_t k=0;k<it->second.size();k++)
                    printf("      [skchk]     hueso '%s' w=%.3f\n", a->bones[it->second[k].first].name.c_str(), it->second[k].second); }
          }
          // DISPERSION RIGIDA: un vert dominado (>90%) por UN hueso conserva su distancia al hueso al animar
          // (la rotacion preserva distancias). dev = |dist(skin, huesoAnim) - dist(raw, huesoRest)|. Si es alta,
          // ese vert recibe la matriz EQUIVOCADA (mapeo de pesos roto) aunque el centroide del grupo este bien.
          { std::map<int, std::pair<int,float> > cpBest; // cp -> (huesoDominante, wMax)
            for (size_t g=0; g<m->vertexGroups.size(); g++){ VertexGroup* vg=m->vertexGroups[g];
                int bi=-1; for (size_t b=0;b<a->bones.size();b++) if (a->bones[b].name==vg->nombre){ bi=(int)b; break; }
                if (bi<0) continue;
                for (size_t j=0;j<vg->verts.size() && j<vg->pesos.size();j++){
                    std::map<int,std::pair<int,float> >::iterator it=cpBest.find(vg->verts[j]);
                    if (it==cpBest.end() || vg->pesos[j] > it->second.second) cpBest[vg->verts[j]] = std::make_pair(bi, vg->pesos[j]); } }
            int nDom=0, nMal=0; double sumDev=0; float worstDev=0; int worstB=-1;
            for (int ri=0; ri<nv; ri++){
                std::map<int,std::pair<int,float> >::iterator it=cpBest.find(m->vertCtrlPoint[ri]);
                if (it==cpBest.end() || wTot[ri]<0.01f || it->second.second/wTot[ri] < 0.9f) continue;
                int bi = it->second.first; nDom++;
                Vector3 pS(m->skinVertex[ri*3], m->skinVertex[ri*3+2], -m->skinVertex[ri*3+1]);
                Vector3 pR(m->vertex[ri*3],    m->vertex[ri*3+2],    -m->vertex[ri*3+1]);
                float dev = ScriptDistSeg(pS, a->bones[bi].poseHead, a->bones[bi].poseTail)
                          - ScriptDistSeg(pR, rH[bi], rT[bi]);
                if (dev < 0) dev = -dev;
                sumDev += dev; if (dev > 10.0f) nMal++;
                if (dev > worstDev){ worstDev = dev; worstB = bi; }
            }
            printf("      [skchk] dominados=%d devAvg=%.2f devMal(>10)=%d worstDev=%.1f (%s)\n",
                   nDom, nDom?sumDev/nDom:0.0, nMal, worstDev, worstB>=0?a->bones[worstB].name.c_str():"-");
          }
          if (peorRi>=0){ int cp=m->vertCtrlPoint[peorRi];
              printf("      [skchk] peorVert ri=%d cp=%d wTot=%.2f raw=(%.1f,%.1f,%.1f) skin=(%.1f,%.1f,%.1f)\n",
                     peorRi, cp, wTot[peorRi], m->vertex[peorRi*3],m->vertex[peorRi*3+1],m->vertex[peorRi*3+2],
                     m->skinVertex[peorRi*3],m->skinVertex[peorRi*3+1],m->skinVertex[peorRi*3+2]);
              // que grupos pesan ese cp
              for (size_t g=0; g<m->vertexGroups.size(); g++){ VertexGroup* vg=m->vertexGroups[g];
                  for (size_t j=0;j<vg->verts.size() && j<vg->pesos.size();j++)
                      if (vg->verts[j]==cp) printf("      [skchk]   grupo '%s' w=%.3f\n", vg->nombre.c_str(), vg->pesos[j]); }
          }
        }
        return true;
    }
    // ---- animpose <frame> : evalua la pose por FK y vuelca poseHead/poseTail de cada hueso ----
    if (cmd == "animpose") {
        Armature* a = (ObjActivo && ObjActivo->getType()==ObjectType::armature) ? (Armature*)ObjActivo : NULL;
        if (!a) { err="animpose: sin armature activo"; return false; }
        int frame=1; ss>>frame;
        EvaluarPoseEsqueleto(a, frame);
        for (size_t i=0;i<a->bones.size();i++)
            printf("      [pose] %s f=%d head=(%.2f,%.2f,%.2f) tail=(%.2f,%.2f,%.2f)\n",
                   a->bones[i].name.c_str(), frame,
                   a->bones[i].poseHead.x,a->bones[i].poseHead.y,a->bones[i].poseHead.z,
                   a->bones[i].poseTail.x,a->bones[i].poseTail.y,a->bones[i].poseTail.z);
        return true;
    }
    // ---- anim <add|del|up|down|count|list|rename <txt>> : gestion de clips del armature activo ----
    if (cmd == "anim") {
        Armature* a = (ObjActivo && ObjActivo->getType()==ObjectType::armature) ? (Armature*)ObjActivo : NULL;
        if (!a) { err = "no hay armature activo"; return false; }
        std::string sub; ss >> sub;
        if (sub == "add")       CrearAnimacion(a);
        else if (sub == "dup")  DuplicarAnimacionActiva(a);
        else if (sub == "del")  BorrarAnimacionActiva(a);
        else if (sub == "up")   MoverAnimacionActiva(a, -1);
        else if (sub == "down") MoverAnimacionActiva(a, +1);
        else if (sub == "count") printf("      [anim] count=%d active=%d\n", (int)a->animations.size(), a->animActiva);
        else if (sub == "list") { for (size_t i=0;i<a->animations.size();i++){ SkeletalAnimation* an=a->animations[i];
                                      int nkf=0; for(size_t t=0;t<an->tracks.size();t++) for(size_t p=0;p<an->tracks[t].Propertys.size();p++) nkf+=(int)an->tracks[t].Propertys[p].keyframes.size();
                                      printf("      [anim] %d: '%s' fps=%d start=%d end=%d tracks=%d keys=%d\n",
                                             (int)i, an->name.c_str(), an->FrameRate, an->startFrame, an->endFrame, (int)an->tracks.size(), nkf); } }
        else if (sub == "rename") { std::string nm; std::getline(ss, nm);
                                    size_t p=nm.find_first_not_of(" \t"); if(p!=std::string::npos) nm=nm.substr(p);
                                    if (a->animActiva>=0 && a->animActiva<(int)a->animations.size()) a->animations[a->animActiva]->name = nm; }
        else { err = "subcomando anim desconocido: '"+sub+"'"; return false; }
        return true;
    }
    // ---- importfbx <ruta> : importa un FBX (para testear el import de animaciones headless) ----
    if (cmd == "importfbx") {
        std::string ruta; std::getline(ss, ruta);
        size_t p = ruta.find_first_not_of(" \t"); if (p!=std::string::npos) ruta = ruta.substr(p);
        if (ruta.empty()) { err = "importfbx: falta la ruta"; return false; }
        bool ok = ImportFBX(ruta);
        printf("      [importfbx] %s -> %s\n", ruta.c_str(), ok?"OK":"FALLO");
        return ok;
    }
    // ---- importgltf <ruta> : importa un glTF/GLB (para testear el import headless) ----
    if (cmd == "importgltf") {
        std::string ruta; std::getline(ss, ruta);
        size_t p = ruta.find_first_not_of(" \t"); if (p!=std::string::npos) ruta = ruta.substr(p);
        if (ruta.empty()) { err = "importgltf: falta la ruta"; return false; }
        extern bool ImportGLTF(const std::string&);
        bool ok = ImportGLTF(ruta);
        printf("      [importgltf] %s -> %s\n", ruta.c_str(), ok?"OK":"FALLO");
        return ok;
    }
    // ---- exportgltf/exportglb <ruta> : exporta la escena a glTF (texto) o GLB (binario) para round-trip headless ----
    if (cmd == "exportgltf" || cmd == "exportglb") {
        std::string ruta; std::getline(ss, ruta);
        size_t p = ruta.find_first_not_of(" \t"); if (p!=std::string::npos) ruta = ruta.substr(p);
        if (ruta.empty()) { err = cmd + ": falta la ruta"; return false; }
        extern bool ExportGLTF(const std::string&, bool, bool);
        bool ok = ExportGLTF(ruta, false, cmd == "exportglb");
        printf("      [%s] %s -> %s\n", cmd.c_str(), ruta.c_str(), ok?"OK":"FALLO");
        return ok;
    }
    // ---- armdeltest : borra el armature con una malla skinneada -> verifica que skinArmature queda NULL (no cuelga),
    //      que skinnear no crashea, y que el undo lo restaura. (bug: borrar esqueleto y la malla sigue animando/crash). ----
    if (cmd == "armdeltest") {
        Armature* a=NULL; Mesh* m=NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::armature && !a) a=(Armature*)o;
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature && !m) m=(Mesh*)o;
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if (!a || !m) { err="armdeltest: falta armature o mesh skinneado"; return false; }
        printf("      [armdel] antes: skinArmature=%p armature=%p\n", (void*)m->skinArmature, (void*)a);
        if (InteractionMode != ObjectMode) {}
        DeseleccionarTodo(); ObjActivo=(Object*)a; a->Seleccionar();
        Eliminar(false); // borra el armature seleccionado (lo detacha al undo + limpia refs)
        printf("      [armdel] tras borrar: skinArmature=%p (0 = OK, no cuelga)\n", (void*)m->skinArmature);
        CurrentFrame=5; m->lastSkinFrame=-999999; SkinearMesh(m); // NO debe crashear (skinArmature NULL -> no deforma)
        printf("      [armdel] SkinearMesh tras borrar: OK (sin crash)\n");
        UndoDeshacer(); // undo -> re-inserta el armature y restaura skinArmature
        printf("      [armdel] tras undo: skinArmature=%p (debe volver a %p)\n", (void*)m->skinArmature, (void*)a);
        m->lastSkinFrame=-999999; SkinearMesh(m);
        printf("      [armdel] SkinearMesh tras undo: OK\n");
        return true;
    }
    // ---- bordercheck : diagnostica el contorno skinneado. Cuenta aristas cuyos 2 extremos caen en DISTINTO vertex
    //      group (hueso) -> posRep las soldo cruzando piezas y el contorno estira mal (el drama de los pies de LISA). ----
    if (cmd == "bordercheck") {
        Mesh* found=NULL;
        { std::vector<Object*> stack; if (SceneCollection) stack.push_back(SceneCollection);
          while(!stack.empty() && !found){ Object* o=stack.back(); stack.pop_back();
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature) found=(Mesh*)o;
            for (size_t i=0;i<o->Childrens.size();i++) stack.push_back(o->Childrens[i]); } }
        if (!found) { err="bordercheck: no hay malla con skinArmature"; return false; }
        Mesh* m=found;
        int nv=m->vertexSize;
        // control-point -> grupo (hueso) dominante. Un render-vert -> su control-point -> grupo.
        std::map<int,int> cpGrupo; // control-point -> indice de grupo
        for (size_t g=0; g<m->vertexGroups.size(); g++)
            for (size_t j=0;j<m->vertexGroups[g]->verts.size();j++) cpGrupo[m->vertexGroups[g]->verts[j]] = (int)g;
        int cross=0, sinGrupo=0, total=0;
        for (size_t e=0; e+1<m->edges.size(); e+=2){
            int a=m->edges[e], b=m->edges[e+1];
            if (a<0||a>=nv||b<0||b>=nv) continue;
            int ca=(a<(int)m->vertCtrlPoint.size())?m->vertCtrlPoint[a]:-1;
            int cb=(b<(int)m->vertCtrlPoint.size())?m->vertCtrlPoint[b]:-1;
            std::map<int,int>::iterator ia=cpGrupo.find(ca), ib=cpGrupo.find(cb);
            total++;
            if (ia==cpGrupo.end()||ib==cpGrupo.end()){ sinGrupo++; continue; }
            if (ia->second != ib->second) cross++;
        }
        printf("      [border] edges=%d  cruzan-2-huesos=%d  con-extremo-sin-grupo=%d  (posRep suelda por posicion de bind)\n",
               total, cross, sinGrupo);
        return true;
    }
    // ---- skinbench <frames> : mide el costo de SkinearMesh (forzando recompute cada frame) sobre la 1er malla skinneada. ----
    // ---- skincache <on> [skip] : activa/desactiva el cache de vertex-animation en la 1er malla skinneada (para medir). ----
    // ---- meshinfo : lista las mallas de la escena con sus MESH PARTS (materiales) -> verificar el import multi-material. ----
    if (cmd == "meshinfo") {
        std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
        int nm=0;
        while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; nm++;
                printf("      [mesh] '%s' verts=%d faces3d=%d tris(idx/3)=%d meshParts=%d\n", m->name.c_str(), m->vertexSize, (int)m->faces3d.size(), m->facesSize/3, (int)m->materialsGroup.size());
                for (size_t g=0; g<m->materialsGroup.size(); g++){ MaterialGroup& mg=m->materialsGroup[g];
                    printf("         part[%d] mat='%s' tris=%d\n", (int)g, mg.material?mg.material->name.c_str():"(none)", mg.indicesDrawnCount/3); }
            }
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); }
        printf("      [meshinfo] %d malla(s)\n", nm);
        return true;
    }
    if (cmd == "skincache") {
        int on=1, skip=0; ss>>on; ss>>skip;
        Mesh* found=NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()&&!found){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature) found=(Mesh*)o;
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if (!found){ err="skincache: no hay malla con skinArmature"; return false; }
        found->skinCacheOn = (on!=0); if(skip<0)skip=0; found->skinCacheSkip = skip; found->LiberarSkinCache();
        printf("      [skincache] on=%d skip=%d\n", on, skip);
        return true;
    }
    if (cmd == "skinbench") {
        int nf=200; ss>>nf; if(nf<1)nf=1;
        Mesh* found=NULL;
        { std::vector<Object*> stack; if (SceneCollection) stack.push_back(SceneCollection);
          while(!stack.empty() && !found){ Object* o=stack.back(); stack.pop_back();
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature) found=(Mesh*)o;
            for (size_t i=0;i<o->Childrens.size();i++) stack.push_back(o->Childrens[i]); } }
        if (!found) { err="skinbench: no hay malla con skinArmature"; return false; }
        Armature* a=found->skinArmature;
        // DIAGNOSTICO de dedup: skinnea 1 vez (arma el CSR) y reporta render-verts vs control-points UNICOS + influencias.
        // El ratio render/ctrl mide cuanto se ahorraria skinneando por control-point (verts que comparten CP = mismo bind).
        { CurrentFrame=1; a->lastPoseFrame=-999999; found->lastSkinFrame=-999999; SkinearMesh(found);
          int nvd=found->vertexSize, nCtrl=0;
          for (int i=0;i<nvd && i<(int)found->vertCtrlPoint.size(); i++) if (found->vertCtrlPoint[i]+1>nCtrl) nCtrl=found->vertCtrlPoint[i]+1;
          std::vector<char> seen(nCtrl>0?nCtrl:1,0); int cpUsados=0;
          for (int i=0;i<nvd && i<(int)found->vertCtrlPoint.size(); i++){ int c=found->vertCtrlPoint[i]; if(c>=0&&c<nCtrl&&!seen[c]){seen[c]=1;cpUsados++;} }
          int totInf=(int)found->skinCpBone.size(), cpsPeso=0;
          for (int c=0;c+1<(int)found->skinCpOff.size(); c++) if (found->skinCpOff[c+1]>found->skinCpOff[c]) cpsPeso++;
          printf("      [skinbench-diag] nv(render)=%d  ctrlPoints(unicos)=%d  ratio=%.2fx  influencias(por-CP)=%d (avg %.2f/CP)  cpsConPeso=%d\n",
                 nvd, cpUsados, cpUsados>0?(float)nvd/(float)cpUsados:0.0f, totInf, cpsPeso>0?(float)totInf/cpsPeso:0.0f, cpsPeso); }
        clock_t t0=clock();
        for (int f=0; f<nf; f++){
            CurrentFrame = 1 + (f % 30);            // varia el frame -> fuerza re-FK + re-skin
            a->lastPoseFrame=-999999; found->lastSkinFrame=-999999;
            SkinearMesh(found);
        }
        clock_t t1=clock();
        double ms = 1000.0*(double)(t1-t0)/CLOCKS_PER_SEC;
        printf("      [skinbench] malla='%s' nv=%d bones=%d frames=%d -> %.2f ms total, %.3f ms/frame\n",
               found->name.c_str(), found->vertexSize, (int)a->bones.size(), nf, ms, ms/nf);
        return true;
    }
    // ---- vgsimplify : "1 hueso por vertice" sobre la 1er malla con vertex groups (destructivo; para medir el skin). ----
    if (cmd == "vgsimplify") {
        Mesh* found=NULL;
        { std::vector<Object*> stack; if (SceneCollection) stack.push_back(SceneCollection);
          while(!stack.empty() && !found){ Object* o=stack.back(); stack.pop_back();
            if (o->getType()==ObjectType::mesh && !((Mesh*)o)->vertexGroups.empty()) found=(Mesh*)o;
            for (size_t i=0;i<o->Childrens.size();i++) stack.push_back(o->Childrens[i]); } }
        if (!found) { err="vgsimplify: no hay malla con vertex groups"; return false; }
        extern void OptimizarVertexGroups1Hueso(Mesh*);
        OptimizarVertexGroups1Hueso(found);
        printf("      [vgsimplify] malla='%s' -> 1 hueso/vertice\n", found->name.c_str());
        return true;
    }
    // ---- skinexport <frame> <archivo> : busca la 1er malla con skinArmature, la skinnea en <frame> y vuelca
    //      TODOS los vertices skinneados (x y z por linea) a <archivo>. Para Procrustes vs Blender (ground truth). ----
    if (cmd == "skinexport") {
        int f=1; std::string path; ss>>f>>path;
        if (path.empty()) { err="skinexport: falta el archivo"; return false; }
        // walk de la escena buscando la malla con skinArmature
        Mesh* found=NULL;
        { std::vector<Object*> stack; if (SceneCollection) stack.push_back(SceneCollection);
          while(!stack.empty() && !found){ Object* o=stack.back(); stack.pop_back();
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature) found=(Mesh*)o;
            for (size_t i=0;i<o->Childrens.size();i++) stack.push_back(o->Childrens[i]); } }
        if (!found) { err="skinexport: no hay malla con skinArmature"; return false; }
        ObjActivo = found; // dejar activa para comandos siguientes (rigidcheck, skincheck)
        CurrentFrame=f; found->lastSkinFrame=-999999; SkinearMesh(found);
        if (!found->skinVertex) { err="skinexport: sin skinVertex"; return false; }
        int nv=found->vertexSize;
        // exportar POR CONTROL-POINT (orden FBX 0..nCtrl-1) = mismo orden que Blender.data.vertices -> correspondencia directa.
        int nCtrl=0; for (int i=0;i<nv && i<(int)found->vertCtrlPoint.size(); i++) if (found->vertCtrlPoint[i]+1>nCtrl) nCtrl=found->vertCtrlPoint[i]+1;
        std::vector<int> rep(nCtrl,-1);
        for (int i=0;i<nv && i<(int)found->vertCtrlPoint.size(); i++){ int c=found->vertCtrlPoint[i]; if (c>=0 && c<nCtrl && rep[c]<0) rep[c]=i; }
        FILE* fp=fopen(path.c_str(),"w"); if(!fp){ err="skinexport: no pude abrir "+path; return false; }
        bool bind = (f==-999); // frame sentinela -999 = exportar el BIND (m->vertex) en vez del skinneado
        for (int c=0;c<nCtrl;c++){ int i=rep[c]; if (i<0){ fprintf(fp,"nan nan nan\n"); continue; }
            const float* src = bind ? found->vertex : found->skinVertex;
            fprintf(fp,"%.5f %.5f %.5f\n", src[i*3], src[i*3+1], src[i*3+2]); }
        fclose(fp);
        printf("      [skinexport] f=%d nCtrl=%d (nv=%d) -> %s (malla '%s')\n", f, nCtrl, nv, path.c_str(), found->name.c_str());
        return true;
    }
    // ---- matexport <frame> <archivo> : vuelca por hueso skinMatrix/skinA/worldFK (row-major, 16 floats) al frame. ----
    if (cmd == "matexport") {
        int f=1; std::string path; ss>>f>>path;
        if (path.empty()) { err="matexport: falta el archivo"; return false; }
        Armature* a=NULL;
        { std::vector<Object*> stack; if (SceneCollection) stack.push_back(SceneCollection);
          while(!stack.empty() && !a){ Object* o=stack.back(); stack.pop_back();
            if (o->getType()==ObjectType::armature) a=(Armature*)o;
            for (size_t i=0;i<o->Childrens.size();i++) stack.push_back(o->Childrens[i]); } }
        if (!a) { err="matexport: no hay armature"; return false; }
        CurrentFrame=f; a->lastPoseFrame=-999999; EvaluarPoseEsqueleto(a, f);
        FILE* fp=fopen(path.c_str(),"w"); if(!fp){ err="matexport: no pude abrir "+path; return false; }
        for (size_t i=0;i<a->bones.size();i++){ W3dBone& b=a->bones[i];
            fprintf(fp,"BONE %s\n", b.name.c_str());
            #define ROWMAJ(M) do{ for(int r=0;r<4;r++)for(int c=0;c<4;c++) fprintf(fp,"%.6f ", (M).m[c*4+r]); fprintf(fp,"\n"); }while(0)
            fprintf(fp,"skinMatrix "); ROWMAJ(b.skinMatrix);
            fprintf(fp,"skinA "); ROWMAJ(b.skinA);
            fprintf(fp,"skinInvBind "); ROWMAJ(b.skinInvBind);
            fprintf(fp,"clusterTransform "); ROWMAJ(b.clusterTransform);
            fprintf(fp,"bind "); ROWMAJ(b.bind);
        }
        fclose(fp);
        printf("      [matexport] f=%d bones=%d -> %s\n", f, (int)a->bones.size(), path.c_str());
        return true;
    }
    // ---- timeline <play|pause|rev|start|end|goto <n>|range <s> <e>|tick|state> : motor de playback global ----
    if (cmd == "timeline") {
        std::string sub; ss >> sub;
        if (sub == "play")       { PlayAnimation = true; AnimPlayDir = 1; }
        else if (sub == "rev")   { PlayAnimation = true; AnimPlayDir = -1; }
        else if (sub == "pause") { PlayAnimation = false; }
        else if (sub == "start") { CurrentFrame = StartFrame; }
        else if (sub == "end")   { CurrentFrame = EndFrame; }
        else if (sub == "goto")  { int n=0; ss>>n; CurrentFrame = n; }
        else if (sub == "range") { int s=0,e=0; ss>>s>>e; StartFrame=s; EndFrame=e; }
        else if (sub == "tick")  { int n=1; ss>>n; if(n<1)n=1; for(int i=0;i<n;i++) AnimTick(); }
        else if (sub == "state") { printf("      [timeline] cur=%d start=%d end=%d play=%d dir=%d\n",
                                          CurrentFrame, StartFrame, EndFrame, PlayAnimation?1:0, AnimPlayDir); }
        else { err = "subcomando timeline desconocido: '"+sub+"'"; return false; }
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

    // ---- selectall / deselectall (seleccion RECURSIVA de objetos, object mode) ----
    if (cmd == "selectall" || cmd == "deselectall") {
        DeseleccionarTodo();
        if (cmd == "selectall" && SceneCollection) {
            std::vector<Object*> st; for (size_t i=0;i<SceneCollection->Childrens.size();i++) st.push_back(SceneCollection->Childrens[i]);
            for (size_t k=0;k<st.size();k++){ st[k]->Seleccionar(); for (size_t i=0;i<st[k]->Childrens.size();i++) st.push_back(st[k]->Childrens[i]); }
        }
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

    // ---- objkeytest : keyframea la 1ra malla en frame 1 (pos 0) y 20 (pos 10), interpola en frame 10 (~5) ----
    if (cmd == "objkeytest") {
        Object* o = NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !o){ Object* n=st.back(); st.pop_back();
            if (n->getType()==ObjectType::mesh) o=n;
            for(size_t i=0;i<n->Childrens.size();i++) st.push_back(n->Childrens[i]); } }
        if (!o) { err="objkeytest: sin malla"; return false; }
        DeseleccionarTodo(); ObjActivo=o; o->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(o);
        CurrentFrame=1;  o->pos=Vector3(0,0,0);   InsertarKeyframeObjeto();
        CurrentFrame=20; o->pos=Vector3(10,0,0);  InsertarKeyframeObjeto();
        CurrentFrame=10; o->pos=Vector3(999,999,999); // ensuciar -> el playback debe corregirlo a la interpolacion
        AplicarAnimacionObjetos();
        printf("      [objkeytest] frame10 pos=(%.2f,%.2f,%.2f)  (esperado ~4.7,0,0)\n", o->pos.x,o->pos.y,o->pos.z);
        return true;
    }
    // ---- delete [col] (borra los seleccionados; 'col' incluye colecciones, como el outliner) -> testea undo de borrado ----
    if (cmd == "delete") {
        if (InteractionMode != ObjectMode) { err = "delete necesita Object Mode"; return false; }
        std::string sub; ss >> sub;
        Eliminar(sub == "col");
        return true;
    }
    // ---- colactive : reporta si CollectionActive esta viva en la escena (para el bug de borrar la coleccion default) ----
    if (cmd == "colactive") {
        bool viva = false;
        if (CollectionActive && SceneCollection) {
            std::vector<Object*> st; st.push_back(SceneCollection);
            while (!st.empty() && !viva){ Object* o=st.back(); st.pop_back();
                if (o == CollectionActive) viva = true;
                for (size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); }
        }
        printf("      [colactive] CollectionActive=%p viva=%d (=Scene:%d)\n",
               (void*)CollectionActive, viva?1:0, CollectionActive==SceneCollection?1:0);
        return true;
    }

    // ---- loopcut <editEdge> <cuts> <factor> : loop cut / subdivide sobre la arista de EDICION indicada ----
    if (cmd == "loopcut") {
        if (InteractionMode != EditMode) { err = "loopcut necesita Edit Mode"; return false; }
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        m->EnsureEdit(); if(!m->edit){err="sin edit mesh";return false;}
        int eg=0, cuts=1; float factor=0.0f; ss>>eg>>cuts>>factor;
        bool ok = m->LoopCutEdit(eg, cuts, factor);
        printf("      [loopcut] edge=%d cuts=%d factor=%.2f -> %s\n", eg, cuts, factor, ok?"ok":"no-op");
        return true;
    }

    // ---- editcenter <x> <y> <z> : mueve la seleccion (Edit Mode) para que su CENTRO local caiga en (x,y,z) ----
    if (cmd == "editcenter") {
        if (InteractionMode != EditMode) { err = "editcenter necesita Edit Mode"; return false; }
        Mesh* m = ScriptActiveMesh(); if(!m){err="no hay malla activa";return false;}
        m->EnsureEdit(); if(!m->edit){err="sin edit mesh";return false;}
        float tx=0,ty=0,tz=0; ss>>tx>>ty>>tz;
        float cx,cy,cz; if(!m->edit->CentroSeleccion(cx,cy,cz)){err="nada seleccionado";return false;}
        MoverSeleccionEditLocal(m, Vector3(tx-cx, ty-cy, tz-cz));
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
    // ---- selobj <nombre> : selecciona un objeto de la escena por nombre (activo + select). Para tests. ----
    if (cmd == "selobj") {
        std::string n; ss >> n;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
        if (!o) { err = "selobj: objeto no encontrado"; return false; }
        DeseleccionarTodo(); o->Seleccionar(); ObjActivo = o; return true;
    }
    // ---- scene : lista los hijos top-level (nombre/tipo/hijos) ----
    if (cmd == "scene") {
        if (!SceneCollection) { err = "sin escena"; return false; }
        ScriptDumpEscena(SceneCollection, 0);
        return true;
    }
    // ---- join : une las mallas seleccionadas en el objeto activo (Ctrl+J) ----
    if (cmd == "join") { JoinObjetos(); return true; }
    // ---- undo : Ctrl+Z (deshace el ultimo comando) ----
    if (cmd == "undo") { UndoDeshacer(); return true; }
    // ---- outliner : dump del arbol de objetos (nombre + tipo + padre) para ver la jerarquia tras un join/delete. ----
    if (cmd == "outliner") {
        struct L { static void rec(Object* o, int d){ if(!o) return;
            for(size_t i=0;i<o->Childrens.size();i++){ Object* c=o->Childrens[i]; std::string ind(d*2,' ');
                const char* t = c->getType()==ObjectType::mesh?"mesh":c->getType()==ObjectType::armature?"armature":c->getType()==ObjectType::collection?"coll":"obj";
                printf("      [outliner] %s%s (%s) parent=%s\n", ind.c_str(), c->name.c_str(), t, c->Parent?c->Parent->name.c_str():"NULL");
                rec(c, d+1); } } };
        L::rec(SceneCollection, 0);
        return true;
    }
    // ---- realjoin <A> <B> : camino COMPLETO de Ctrl+J (JoinObjetos con estado+seleccion). Verifica que el join real
    //      preserva el skinning end-to-end. ----
    if (cmd == "realjoin") {
        std::string na, nb; ss >> na >> nb;
        Object* a = SceneCollection ? FindObjectByName(SceneCollection, na) : NULL;
        Object* b = SceneCollection ? FindObjectByName(SceneCollection, nb) : NULL;
        if (!a || !b || a->getType()!=ObjectType::mesh || b->getType()!=ObjectType::mesh) { err="realjoin: mallas no encontradas"; return false; }
        estado = editNavegacion; InteractionMode = ObjectMode;
        DeseleccionarTodo(); ObjActivo=a; a->Seleccionar(); b->Seleccionar();
        ObjSelects.clear(); ObjSelects.push_back(a); ObjSelects.push_back(b);
        Mesh* ma=(Mesh*)a; int vA=ma->vertexSize, gA=(int)ma->vertexGroups.size();
        JoinObjetos();
        printf("      [realjoin] estado=%d IM=%d '%s' v %d->%d g %d->%d skin=%s\n", estado, InteractionMode,
            na.c_str(), vA, ma->vertexSize, gA, (int)ma->vertexGroups.size(), ma->skinArmature?"SI":"NO");
        ObjActivo=a; return true;
    }
    // ---- joinall : selecciona TODAS las mallas hijas del 1er armature (una activa) y las une (Ctrl+J). Reproduce el
    //      caso real de Dante (mallas dentro del armature). Reporta el resultado + deformacion. ----
    if (cmd == "joinall") {
        Armature* arm=NULL; { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !arm){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::armature) arm=(Armature*)o; for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(!arm){err="joinall: sin armature";return false;}
        std::vector<Mesh*> ms; { std::vector<Object*> st; st.push_back(arm);
          while(!st.empty()){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::mesh) ms.push_back((Mesh*)o); for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(ms.size()<2){err="joinall: <2 mallas en el armature";return false;}
        estado = editNavegacion; InteractionMode = ObjectMode;
        DeseleccionarTodo(); ObjSelects.clear();
        for(size_t i=0;i<ms.size();i++){ ms[i]->Seleccionar(); ObjSelects.push_back((Object*)ms[i]); }
        ObjActivo = (Object*)ms[0]; // activo = la primera
        printf("      [joinall] %d mallas del armature, activo='%s'\n", (int)ms.size(), ms[0]->name.c_str());
        JoinObjetos();
        Mesh* r=(Mesh*)ms[0]; ObjActivo=(Object*)r;
        printf("      [joinall] resultado '%s' v=%d g=%d skin=%s\n", r->name.c_str(), r->vertexSize, (int)r->vertexGroups.size(), r->skinArmature?"SI":"NO");
        return true;
    }
    // ---- joinbug : reproduce el ESTADO ROTO del outliner viejo (shift-range): TODAS las mallas del armature con
    //      select=true PERO solo la 1ra y la ultima en ObjSelects (las del medio quedaban fuera). Antes esas mallas
    //      del medio se BORRABAN sin unirse ("desaparecen todas"). Verifica que el JoinObjetos blindado NO pierde nada. ----
    if (cmd == "joinbug") {
        Armature* arm=NULL; { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !arm){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::armature) arm=(Armature*)o; for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(!arm){err="joinbug: sin armature";return false;}
        std::vector<Mesh*> ms; { std::vector<Object*> st; st.push_back(arm);
          while(!st.empty()){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::mesh) ms.push_back((Mesh*)o); for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(ms.size()<3){err="joinbug: <3 mallas en el armature";return false;}
        estado = editNavegacion; InteractionMode = ObjectMode;
        DeseleccionarTodo(); ObjSelects.clear();
        // ESTADO ROTO: todas select=true; SOLO 1ra y ultima en ObjSelects (como el shift-range viejo)
        for(size_t i=0;i<ms.size();i++) ms[i]->select = true;
        ObjSelects.push_back((Object*)ms[0]);
        ObjSelects.push_back((Object*)ms[ms.size()-1]);
        ObjActivo = (Object*)ms[ms.size()-1]; // activo = la ultima (clickeada)
        int antes=(int)ms.size();
        printf("      [joinbug] %d mallas, ObjSelects=%d (1ra+ultima), resto select=true fuera de ObjSelects\n", antes, (int)ObjSelects.size());
        JoinObjetos();
        // contar mallas que SIGUEN bajo el armature (ninguna deberia haberse perdido sin unirse)
        int quedan=0; { std::vector<Object*> st; st.push_back(arm);
          while(!st.empty()){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::mesh) quedan++; for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        printf("      [joinbug] tras join: %d mallas bajo el armature (antes %d). NINGUNA borrada-sin-unir = OK\n", quedan, antes);
        return true;
    }
    // ---- regen <name> : corre GenerarRender sobre la malla (para ver el conteo de verts dedupeado + si preserva skin). ----
    if (cmd == "regen") {
        std::string nm; ss >> nm; Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o || o->getType()!=ObjectType::mesh){ err="regen: malla no encontrada"; return false; }
        Mesh* m=(Mesh*)o; int v0=m->vertexSize, g0=(int)m->vertexGroups.size();
        m->LiberarCapas(false); m->PoblarCapas(); m->GenerarRender(); m->lastSkinFrame=-999999; ObjActivo=o;
        printf("      [regen] '%s' v %d->%d g %d->%d skin=%s\n", nm.c_str(), v0, m->vertexSize, g0, (int)m->vertexGroups.size(), m->skinArmature?"SI":"NO");
        return true;
    }
    // ---- jointest <A> <B> : anexa B a A (mismo camino que Ctrl+J: AnexarMallaTransformada + GenerarRender) y verifica
    //      el merge de mallas SKINNEADAS (vertex groups + vertCtrlPoint). Aisla el fix del join sin el plumbing de seleccion. ----
    if (cmd == "jointest") {
        std::string na, nb; ss >> na >> nb;
        Object* a = SceneCollection ? FindObjectByName(SceneCollection, na) : NULL;
        Object* b = SceneCollection ? FindObjectByName(SceneCollection, nb) : NULL;
        if (!a || !b || a->getType()!=ObjectType::mesh || b->getType()!=ObjectType::mesh) { err="jointest: mallas no encontradas"; return false; }
        Mesh* ma=(Mesh*)a; Mesh* mb=(Mesh*)b;
        int vA=ma->vertexSize, gA=(int)ma->vertexGroups.size(), cpA=(int)ma->vertCtrlPoint.size();
        int vB=mb->vertexSize, gB=(int)mb->vertexGroups.size();
        Matrix4 M; M.Identity();
        ma->AnexarMallaTransformada(mb, M);
        ma->LiberarCapas(false); ma->PoblarCapas(); ma->GenerarRender(); ma->lastSkinFrame = -999999;
        ObjActivo = a; // para skinbbox despues
        printf("      [jointest] '%s'(v=%d g=%d cp=%d) + '%s'(v=%d g=%d) -> v=%d g=%d cp=%d skin=%s\n",
            na.c_str(),vA,gA,cpA, nb.c_str(),vB,gB, ma->vertexSize, (int)ma->vertexGroups.size(), (int)ma->vertCtrlPoint.size(), ma->skinArmature?"SI":"NO");
        return true;
    }
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
    // ---- animlistsel <idx> : simula el click en la LISTA de animaciones (tab Armature) via el hook OnSeleccionarAnimClip
    //      y verifica que sincroniza la seleccion app-wide (ActiveAnimKind/ActiveAnimArm) que lee el timeline. ----
    if (cmd == "animlistsel") {
        int idx=1; ss>>idx;
        Armature* a=NULL; { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !a){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::armature) a=(Armature*)o; for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(!a){err="animlistsel: sin armature";return false;}
        extern int ActiveAnimKind; extern Armature* ActiveAnimArm;
        ActiveAnimKind=0; ActiveAnimArm=NULL; // estado inicial: escena activa (como si no estuviera seleccionado el clip)
        printf("      [animlistsel] hook=%s\n", OnSeleccionarAnimClip?"SET":"NULL");
        if (OnSeleccionarAnimClip) OnSeleccionarAnimClip(a, idx); // == click en la lista (PropList modo 5)
        printf("      [animlistsel] clip %d -> ActiveAnimKind=%d ActiveAnimArm==arm=%d animActiva=%d\n",
            idx, ActiveAnimKind, (ActiveAnimArm==a)?1:0, a->animActiva);
        return true;
    }
    // ---- posetest <1=grab|2=rotate|3=scale> <valor> : maneja el transform de POSE via valor numerico exacto sobre 2
    //      huesos y verifica que se transforman alrededor del PIVOTE compartido (median): rotate/scale preservan/escalan
    //      la distancia al pivote y los huesos SE MUEVEN; translate los desplaza igual. Verifica la matematica del overhaul. ----
    if (cmd == "posetest") {
        int mode=2; float val=45; ss>>mode>>val;
        Armature* a=NULL; { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !a){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::armature) a=(Armature*)o; for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(!a || a->bones.size()<3){err="posetest: sin armature o <3 huesos";return false;}
        for(size_t i=0;i<a->bones.size();i++){ a->bones[i].poseT=a->bones[i].restT; a->bones[i].poseR=a->bones[i].restR; a->bones[i].poseS=a->bones[i].restS; } // pose = rest (headless no evalua FK)
        estado = editNavegacion; InteractionMode = PoseMode; ObjActivo=(Object*)a;
        // elegir 2 huesos con cabezas separadas del baricentro (evitar la raiz en el origen)
        int b0=1, b1=(int)a->bones.size()-1;
        for(size_t i=0;i<a->bones.size();i++) a->bones[i].select=false;
        a->bones[b0].select=true; a->bones[b1].select=true; a->boneActivo=b0;
        Vector3 h0a=SkelBoneWorldNode(a,b0)*Vector3(0,0,0), h0b=SkelBoneWorldNode(a,b1)*Vector3(0,0,0);
        Vector3 piv=(h0a+h0b)*0.5f;
        float d0a=(h0a-piv).Length(), d0b=(h0b-piv).Length();
        extern void PoseXformStart(int); extern void PoseXformNumValor(float); extern void PoseXformConfirm();
        extern int axisSelect; extern int transformOrientation;
        PoseXformStart(mode);
        if(mode!=3){ axisSelect=(mode==2)?Z:X; transformOrientation=GlobalOrient; } // eje global determinista para el test
        PoseXformNumValor(val);
        PoseXformConfirm();
        Vector3 h1a=SkelBoneWorldNode(a,b0)*Vector3(0,0,0), h1b=SkelBoneWorldNode(a,b1)*Vector3(0,0,0);
        float d1a=(h1a-piv).Length(), d1b=(h1b-piv).Length();
        float mova=(h1a-h0a).Length(), movb=(h1b-h0b).Length();
        const char* mn=(mode==1)?"GRAB":(mode==2)?"ROTATE":"SCALE";
        printf("      [posetest] %s val=%.2f huesos b%d,b%d pivote=(%.2f,%.2f,%.2f)\n", mn, val, b0, b1, piv.x,piv.y,piv.z);
        printf("         b%d: dist_pivote %.3f->%.3f  movio=%.3f\n", b0, d0a, d1a, mova);
        printf("         b%d: dist_pivote %.3f->%.3f  movio=%.3f\n", b1, d0b, d1b, movb);
        bool ok=false;
        if(mode==2) ok = (fabsf(d1a-d0a)<0.01f*(d0a+1) && fabsf(d1b-d0b)<0.01f*(d0b+1) && mova>0.001f && movb>0.001f); // rotate: distancia preservada + se movio
        else if(mode==3) ok = (fabsf(d1a-val*d0a)<0.02f*(d0a+1) && fabsf(d1b-val*d0b)<0.02f*(d0b+1)); // scale: distancia * factor
        else ok = (mova>0.001f && movb>0.001f && fabsf(mova-movb)<0.01f*(mova+1)); // grab: los 2 se mueven IGUAL
        printf("         -> %s\n", ok?"OK (transform alrededor del pivote correcto)":"FALLO");
        return true;
    }
    // ---- dopedump : arma las filas del DOPE SHEET con la seleccion/animacion actual y las dumpea (verifica el
    //      filtrado por seleccion, el summary = union, y que el armature solo salga en Pose Mode con huesos elegidos). ----
    if (cmd == "dopedump") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->ConstruirDopeRows();
        printf("      [dopedump] panelW=%d filas=%d (0 filas = panel vacio -> timeline clasico)\n", tl->panelW, (int)tl->dopeRows.size());
        for (size_t i=0;i<tl->dopeRows.size();i++){
            Timeline::DopeRow& d = tl->dopeRows[i];
            const char* t = (d.tipo==0)?"SUMMARY":(d.tipo==1)?"objeto":(d.tipo==2)?"grupo":"canal";
            std::string ind(d.nivel*3, ' ');
            printf("         %s%-7s '%s' keys=%d @", ind.c_str(), t, d.nombre.c_str(), (int)d.keys.size());
            for (size_t k=0;k<d.keys.size() && k<10;k++) printf(" %d", d.keys[k]);
            printf("%s\n", d.keys.size()>10?" ...":"");
        }
        delete tl; return true;
    }
    // ---- dopexform <g|s> <expr> [center|cur] : selecciona TODOS los keyframes del dope sheet y les aplica el
    //      transform tipeando el valor tal cual lo haria el usuario ('g' 2 = mover 2 frames; 's' 2 = lo que
    //      duraba 2 frames pasa a durar 4). Verifica el redondeo a entero, el pivote y que NO se clampee. ----
    if (cmd == "dopexform") {
        std::string modoS, expr, pivS; ss>>modoS>>expr>>pivS;
        int modo = (modoS=="s"||modoS=="scale") ? Timeline::DOPE_ESC : Timeline::DOPE_MOV;
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->ConstruirDopeRows();
        tl->DopeSetPivot((pivS=="cur"||pivS=="curframe") ? 1 : 0);
        tl->DopeSelectAll();
        tl->DopeMoveStart(modo);
        if (!tl->DopeMoviendo()){ printf("      [dopexform] no arranco (no hay keyframes seleccionados)\n"); delete tl; return true; }
        for (size_t i=0;i<expr.size();i++) tl->DopeNumChar((int)(unsigned char)expr[i]);
        printf("      [dopexform] %s  (frame actual=%d)\n", tl->DopeTextoTransform().c_str(), CurrentFrame);
        tl->DopeMoveConfirm();
        tl->ConstruirDopeRows();
        for (size_t i=0;i<tl->dopeRows.size();i++){
            Timeline::DopeRow& d = tl->dopeRows[i];
            if (d.tipo!=0 && d.tipo!=3) continue;               // solo Summary y canales
            const char* t = (d.tipo==0)?"SUMMARY":"canal";
            printf("         %-7s '%s' keys=%d @", t, d.nombre.c_str(), (int)d.keys.size());
            for (size_t k=0;k<d.keys.size() && k<12;k++) printf(" %d", d.keys[k]);
            printf("%s\n", d.keys.size()>12?" ...":"");
        }
        delete tl; return true;
    }
    // ---- dopeframesel : View > Frame Selected sobre TODOS los keyframes. Reporta donde cae cada extremo en
    //      pixeles (el 1ro debe quedar contra el borde IZQUIERDO y el ultimo contra el DERECHO; si todos cayeron
    //      en un mismo frame, al MEDIO y sin tocar el zoom). ----
    if (cmd == "dopeframesel") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->ConstruirDopeRows();
        if (tl->dopeRows.empty()){ printf("      [dopeframesel] sin filas\n"); delete tl; return true; }
        tl->DopeSelectAll();
        float zoom0 = tl->pxPerFrame;
        tl->DopeFrameSelected();
        int mn, mx; if (!tl->DopeRangoSeleccion(mn, mx)){ printf("      [dopeframesel] sin seleccion\n"); delete tl; return true; }
        float xa = tl->FrameToX((float)mn), xb = tl->FrameToX((float)mx);
        int izq = tl->panelW, der = tl->width, medio = (izq + der)/2;
        printf("      [dopeframesel] keys sel=[%d..%d]  strip=[%d..%d] (medio=%d)  zoom %.2f -> %.2f\n",
               mn, mx, izq, der, medio, zoom0, tl->pxPerFrame);
        if (mn == mx)
            printf("         1 solo keyframe: x=%.1f (medio=%d, error=%.1fpx) zoom %s\n",
                   xa, medio, xa-(float)medio, (tl->pxPerFrame==zoom0)?"INTACTO OK":"CAMBIO!");
        else
            printf("         primero x=%.1f (borde izq=%d, margen=%.1fpx) | ultimo x=%.1f (borde der=%d, margen=%.1fpx)\n",
                   xa, izq, xa-(float)izq, xb, der, (float)der-xb);
        delete tl; return true;
    }
    // ---- curvatest : el evaluador de curvas. Arma una curva a mano y verifica los 3 modos de interpolacion
    //      (Constant/Linear/Bezier), que bezier PASE por los keyframes, que sea SUAVE (sin quiebre en el keyframe
    //      del medio, que es lo que lineal NO tiene) y que Eval(int) == EvalF(int). ----
    if (cmd == "curvatest") {
        AnimProperty ap; ap.Property = AnimPosition; ap.component = AnimX;
        SetKeyCurva(ap, 0, 0.0f); SetKeyCurva(ap, 10, 10.0f); SetKeyCurva(ap, 20, 0.0f);
        struct S {
            static void modo(AnimProperty& ap, int interp, const char* nom){
                for (size_t i=0;i<ap.keyframes.size();i++){ ap.keyframes[i].Interpolation = interp;
                    ap.keyframes[i].handleType = HAuto; }
                printf("      [curvatest] %-8s f=0,5,10,15,20 -> %.3f %.3f %.3f %.3f %.3f\n", nom,
                    ap.EvalF(0,0), ap.EvalF(5,0), ap.EvalF(10,0), ap.EvalF(15,0), ap.EvalF(20,0));
            }
        };
        S::modo(ap, KfConstant, "Constant"); S::modo(ap, KfLinear, "Linear"); S::modo(ap, KfBezier, "Bezier");
        // TODOS los modos tienen que dar el valor del keyframe EN el frame del keyframe. El escalon es el que se
        // puede escapar: retenia a.value tambien en el frame de b (llegaba un frame tarde) y el editor lo dibujaba
        // en b -> lo que veias no era lo que corria.
        for (int interp=0; interp<=2; interp++){
            for (size_t i=0;i<ap.keyframes.size();i++) ap.keyframes[i].Interpolation = interp;
            const char* nm = (interp==KfConstant)?"Constant":(interp==KfLinear)?"Linear":"Bezier";
            bool ok = true; float peor = 0.0f;
            for (size_t i=0;i<ap.keyframes.size();i++){
                float d = fabsf(ap.EvalF((float)ap.keyframes[i].frame, 0.0f) - ap.keyframes[i].value);
                if (d > peor) peor = d;
                if (d > 1e-4f) ok = false;
            }
            printf("         %-8s vale lo del keyframe EN cada keyframe: %s (peor=%.5f)\n", nm, ok?"OK":"NO!", peor);
        }
        // SUAVE en el keyframe del medio: la pendiente de antes y la de despues tienen que coincidir.
        // (con lineal ahi hay un pico: +1 y luego -1 -> quiebre de 2.0)
        float h = 0.01f;
        float dAntes  = (ap.EvalF(10,0) - ap.EvalF(10-h,0)) / h;
        float dDespues= (ap.EvalF(10+h,0) - ap.EvalF(10,0)) / h;
        printf("         bezier pendiente antes=%.3f despues=%.3f quiebre=%.4f -> %s\n",
               dAntes, dDespues, fabsf(dAntes-dDespues), (fabsf(dAntes-dDespues)<0.05f)?"SUAVE OK":"QUIEBRE!");
        for (size_t i=0;i<ap.keyframes.size();i++) ap.keyframes[i].Interpolation = KfLinear;
        float lAntes = (ap.EvalF(10,0)-ap.EvalF(10-h,0))/h, lDespues = (ap.EvalF(10+h,0)-ap.EvalF(10,0))/h;
        printf("         lineal pendiente antes=%.3f despues=%.3f quiebre=%.4f (tiene que QUEBRAR: es una recta)\n",
               lAntes, lDespues, fabsf(lAntes-lDespues));
        // Eval(int) tiene que ser identico a EvalF(int): la animacion no cambia por agregar el evaluador continuo
        float maxd = 0.0f;
        for (int interp=0; interp<=2; interp++){
            for (size_t i=0;i<ap.keyframes.size();i++) ap.keyframes[i].Interpolation = interp;
            for (int f=-2; f<=22; f++){ float d = fabsf(ap.Eval(f,0.0f) - ap.EvalF((float)f,0.0f)); if (d>maxd) maxd=d; }
        }
        printf("         Eval(int) vs EvalF(float) maxdiff=%.7f -> %s\n", maxd, (maxd<1e-6f)?"OK":"DIFIEREN!");
        // handles PLANOS explicitos (Free, dV=0 a los dos lados) = ease: arranca y llega frenado
        for (size_t i=0;i<ap.keyframes.size();i++){ keyFrame& k = ap.keyframes[i];
            k.Interpolation = KfBezier; k.handleType = HFree;
            k.inDF = -3.0f; k.inDV = 0.0f; k.outDF = 3.0f; k.outDV = 0.0f; }
        printf("         bezier handles PLANOS f=2,5,8 -> %.3f %.3f %.3f (ease: arranca y llega frenado)\n",
               ap.EvalF(2,0), ap.EvalF(5,0), ap.EvalF(8,0));
        return true;
    }
    // ---- curvaedit : el EDITOR de curvas. Verifica el switch de modo, que en curvas NO haya scrollbar, el mapeo
    //      valor<->Y (centro = cero) con zoom de 2 ejes independientes, el arrastre de un HANDLE (que curva el
    //      tramo) y el ROTAR de keyframes (que en dope sheet no existe y en curvas si). ----
    if (cmd == "curvaedit") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->ConstruirDopeRows();
        if (tl->dopeRows.empty()){ err="curvaedit: sin filas (importa algo animado y selecciona)"; delete tl; return false; }
        printf("      [curvaedit] modo=%s scrollbar=%s\n", tl->modo?"CURVAS":"DOPE", tl->scrollY?"si":"no");
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        printf("      [curvaedit] modo=%s scrollbar=%s (en curvas NO tiene que haber scrollbar)\n",
               tl->modo?"CURVAS":"DOPE", tl->scrollY?"si":"no");
        // eje vertical: el CENTRO es el CERO
        float yCero = tl->ValueToY(0.0f);
        printf("         valor 0 -> y=%.1f (centro del strip=%d) | ida y vuelta v=3.5 -> y=%.1f -> v=%.4f\n",
               yCero, tl->CentroVertical(), tl->ValueToY(3.5f), tl->YToValue(tl->ValueToY(3.5f)));
        // ZOOM DE 2 EJES: estirar el vertical NO toca el horizontal
        float px0 = tl->pxPerFrame, pu0 = tl->pxPerUnit;
        tl->ZoomVBy(2.0f);
        printf("         ZoomV x2: pxPerFrame %.2f -> %.2f (%s) | pxPerUnit %.2f -> %.2f\n",
               px0, tl->pxPerFrame, (tl->pxPerFrame==px0)?"INTACTO OK":"SE MOVIO!", pu0, tl->pxPerUnit);
        // ---- HANDLE: agarrar el de SALIDA de un keyframe y arrastrarlo -> el tramo pasa a BEZIER ----
        Timeline::DopeRow* canal = NULL;
        for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0 && tl->dopeRows[i].keys.size()>=3){ canal=&tl->dopeRows[i]; break; }
        if (canal){
            AnimProperty* ap = tl->CurvaDeFila(*canal);
            if (ap && ap->keyframes.size()>=3){
                const int idx = 1;                       // un keyframe del medio (tiene vecinos de los dos lados)
                int f0 = ap->keyframes[idx].frame;
                float val0 = ap->keyframes[idx].value;
                tl->DopeSelectNone();
                printf("         canal '%s' (eje %s) keyframe f=%d interp ANTES=%d\n", canal->nombre.c_str(),
                       (canal->compId==AnimX)?"X rojo":(canal->compId==AnimY)?"Y verde":"Z azul", f0, ap->keyframes[idx].Interpolation);
                // 1) CLICK sobre el keyframe -> lo selecciona (hit-test 2D real). Recien ahi aparecen los handles.
                int kx = (int)(tl->x + tl->FrameToX((float)f0)), ky = (int)(tl->y + tl->ValueToY(val0));
                bool tomo = tl->CurvaClickStrip(kx, ky);
                printf("         click en el keyframe (%d,%d) -> seleccionado: %s\n", kx, ky, tomo?"SI":"NO");
                // el handle SOLO existe si el tramo es BEZIER (una recta no tiene nada que ajustar)
                tl->SetInterpolacionSel(KfBezier);
                float v1 = ap->EvalF((float)f0 + 0.5f, 0.0f);   // valor a mitad de tramo ANTES de tocar el handle
                // 2) CLICK sobre el HANDLE de salida, en la MISMA posicion que usan el dibujo y el hit-test
                float phx, phy; tl->HandlePos(ap, (size_t)idx, true, phx, phy);
                int hx = (int)(tl->x + phx), hy = (int)(tl->y + phy);
                bool agarro = tl->CurvaClickStrip(hx, hy) && tl->HandleArrastrando();
                printf("         click en el handle de SALIDA (%d,%d) -> agarrado: %s, lado=%s\n", hx, hy,
                       agarro?"SI":"NO", tl->HandleEsSalida()?"SALIDA OK":"ENTRADA (agarro el equivocado!)");
                if (agarro){
                    int mx = (int)(tl->x + phx);
                    int my = (int)(tl->y + tl->ValueToY(val0 + 8.0f));      // tirar el handle hacia ARRIBA
                    tl->HandleApply(mx, my);
                    tl->HandleSoltar();
                    float v2 = ap->EvalF((float)f0 + 0.5f, 0.0f);
                    printf("         valor a mitad de tramo %.4f -> %.4f -> %s\n", v1, v2,
                           (fabsf(v2-v1)>1e-4f)?"LA CURVA CAMBIO OK":"no cambio!");
                    printf("         handle de salida = PUNTO (dF=%.3f, dV=%.3f) tipo=%d | el keyframe NO se movio: f=%d val=%.4f -> %s\n",
                           ap->keyframes[idx].outDF, ap->keyframes[idx].outDV, ap->keyframes[idx].handleType,
                           ap->keyframes[idx].frame, ap->keyframes[idx].value,
                           (ap->keyframes[idx].frame==f0 && ap->keyframes[idx].value==val0)?"OK":"SE MOVIO!");
                }
            }
        }
        // ---- ROTAR: solo en curvas. 180 grados alrededor del pivote ESPEJA en los dos ejes (tiempo y valor).
        //      El RANGO de frames no sirve para verificarlo (es simetrico: espejarlo da lo mismo) -> se miran
        //      keyframes CONCRETOS: cada uno tiene que terminar en 2*pivote - original. ----
        if (canal){
            AnimProperty* ap = tl->CurvaDeFila(*canal);
            if (ap && ap->keyframes.size()>=3){
                tl->DopeSelectAll();
                std::vector<keyFrame> antes = ap->keyframes;
                // Rotar 180 EFECTIVAMENTE mueve las curvas...
                tl->DopeMoveStart(Timeline::DOPE_ROT);
                if (!tl->DopeMoviendo()){ printf("         ROTAR no arranco\n"); delete tl; return true; }
                for (const char* p="180"; *p; ++p) tl->DopeNumChar(*p);
                printf("         %s\n", tl->DopeTextoTransform().c_str());
                tl->DopeMoveConfirm();
                float dmaxV = 0.0f; int movidos = 0;
                for (size_t i=0;i<antes.size() && i<ap->keyframes.size();i++){
                    float dv = fabsf(ap->keyframes[i].value - antes[i].value); if (dv>dmaxV) dmaxV=dv;
                    if (dv > 1e-5f) movidos++; }
                printf("         ROTAR 180 sobre '%s' (%d keyframes): cambiaron %d valores, maxdiff=%.4f -> %s\n",
                       canal->nombre.c_str(), (int)antes.size(), movidos, dmaxV,
                       (movidos>0)?"la rotacion TOCA el eje del valor OK":"NO se movio nada!");
                // ...y rotar 180 OTRA VEZ tiene que volver EXACTO al original. Es la verificacion buena: no depende
                // de saber donde cayo el pivote (que se calcula sobre TODA la seleccion, no sobre esta curva sola).
                tl->DopeSelectAll();
                tl->DopeMoveStart(Timeline::DOPE_ROT);
                for (const char* p="180"; *p; ++p) tl->DopeNumChar(*p);
                tl->DopeMoveConfirm();
                int fdif = 0; float vdif = 0.0f;
                for (size_t i=0;i<antes.size() && i<ap->keyframes.size();i++){
                    if (ap->keyframes[i].frame != antes[i].frame) fdif++;
                    float dv = fabsf(ap->keyframes[i].value - antes[i].value); if (dv>vdif) vdif=dv; }
                printf("         ROTAR 180 dos veces = identidad: frames distintos=%d valor maxdiff=%.6f -> %s\n",
                       fdif, vdif, (fdif==0 && vdif<1e-4f)?"VUELVE EXACTO OK":"NO VOLVIO!");
            }
        }
        delete tl; return true;
    }
    // ---- curvaperf [reps] : costo de ARMAR el trazo de las curvas (lo que se hacia pixel por pixel). Cuenta los
    //      vertices generados y los tramos que el culling descarta. No mide GL: mide el trabajo por frame, que es
    //      donde estaba el problema (un draw call y un escaneo de keyframes por CADA pixel de CADA curva). ----
    if (cmd == "curvaperf") {
        int reps = 60; ss >> reps; if (reps < 1) reps = 1;
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        if (tl->dopeRows.empty()){ err="curvaperf: sin filas"; delete tl; return false; }
        int canales = 0; for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0) canales++;
        unsigned int t0 = w3dGetTicks();
        long long verts = 0;
        for (int i=0;i<reps;i++) verts = tl->CurvaTrazoCosto();
        unsigned int t1 = w3dGetTicks();
        printf("      [curvaperf] %d canales, %d frames simulados en %ums (%.3f ms/frame)\n",
               canales, reps, t1-t0, (float)(t1-t0)/(float)reps);
        // ConstruirDopeRows se rearma en CADA Render: cuanto cuesta al lado del trazo?
        unsigned int t2 = w3dGetTicks();
        for (int i=0;i<reps;i++) tl->ConstruirDopeRows();
        unsigned int t3 = w3dGetTicks();
        printf("         ConstruirDopeRows (se rearma en cada redibujo): %ums / %d = %.3f ms/frame\n",
               t3-t2, reps, (float)(t3-t2)/(float)reps);
        printf("         vertices del trazo por frame = %lld  (antes: 1 draw call + 1 EvalF por PIXEL de cada curva)\n", verts);
        // ahora TODO fuera de la vista: el culling tiene que tirar casi todo
        tl->viewCenterV = 1.0e6f;    // la vista se va lejisimos de las curvas
        long long vFuera = tl->CurvaTrazoCosto();
        printf("         con la vista LEJOS de las curvas: vertices = %lld -> %s\n", vFuera,
               (vFuera < verts/10)?"el culling descarta lo que no se ve OK":"NO esta culleando!");

        // ---- lo IMPORTANTE del culling: una curva SI se puede llegar a ver aunque NINGUN keyframe se vea ----
        tl->viewCenterV = 0.0f; tl->pxPerUnit = 1.0f;   // se ven ~150 unidades para arriba y para abajo
        tl->viewStartF = -2.0f;
        { // A) los dos keyframes MUY fuera (uno arriba, otro abajo): la recta CRUZA la vista -> HAY que dibujarla
          AnimProperty cruza; cruza.Property=AnimPosition; cruza.component=AnimX;
          SetKeyCurva(cruza, 0, 1000.0f); SetKeyCurva(cruza, 10, -1000.0f);
          int v = tl->CurvaTrazo(&cruza, 1.0f);
          printf("         keyframes en +1000 y -1000 (los DOS fuera de pantalla, la recta cruza): vertices=%d -> %s\n",
                 v, (v>0)?"SE DIBUJA OK":"SE PERDIO! (culling de mas)");
          // B) los dos keyframes arriba y la recta tambien: no se ve nada -> NO se dibuja
          AnimProperty lejos; lejos.Property=AnimPosition; lejos.component=AnimX;
          SetKeyCurva(lejos, 0, 1000.0f); SetKeyCurva(lejos, 10, 1200.0f);
          int v2 = tl->CurvaTrazo(&lejos, 1.0f);
          printf("         keyframes en +1000 y +1200 (todo el tramo arriba de la vista): vertices=%d -> %s\n",
                 v2, (v2==0)?"culleada OK":"se dibuja al pedo!");
          // C) BEZIER con sobrepico: los keyframes se ven pero la curva se va MUY arriba -> el hull lo tiene que notar
          AnimProperty bez; bez.Property=AnimPosition; bez.component=AnimX;
          SetKeyCurva(bez, 0, 0.0f); SetKeyCurva(bez, 10, 0.0f);
          bez.keyframes[0].Interpolation = KfBezier; bez.keyframes[0].handleType = HFree;
          bez.keyframes[0].outDF = 3.0f; bez.keyframes[0].outDV = 1500.0f;   // handle enorme hacia arriba
          bez.keyframes[1].Interpolation = KfBezier; bez.keyframes[1].handleType = HFree;
          bez.keyframes[1].inDF = -3.0f; bez.keyframes[1].inDV = 1500.0f;
          int v3 = tl->CurvaTrazo(&bez, 1.0f);
          printf("         bezier con handles enormes (keyframes en 0 pero la curva se dispara): vertices=%d\n", v3);
        }
        delete tl; return true;
    }
    // ---- curvakey : menu Key. Duplicar (Shift+D: copia + agarra, y Esc tiene que BORRAR las copias) y los ejes
    //      X/Y durante el move de curvas (X = solo tiempo, Y = solo valor). ----
    if (cmd == "curvakey") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        Timeline::DopeRow* canal = NULL;
        for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0 && tl->dopeRows[i].keys.size()>=3){ canal=&tl->dopeRows[i]; break; }
        if (!canal){ err="curvakey: sin canal con keyframes"; delete tl; return false; }
        AnimProperty* ap = tl->CurvaDeFila(*canal);
        std::string ck = canal->claveFila;
        int n0 = (int)ap->keyframes.size();

        // ---- DUPLICAR + CANCELAR: tiene que quedar TODO como estaba (las copias se borran) ----
        tl->DopeSelectAll();
        tl->DopeDuplicarSeleccion();
        int nDup = (int)ap->keyframes.size();
        printf("      [curvakey] Shift+D: %d keyframes -> %d (duplico) | move activo=%s\n", n0, nDup, tl->DopeMoviendo()?"SI":"NO");
        tl->DopeMoveCancel();
        int nCanc = (int)ap->keyframes.size();
        printf("         Esc despues de duplicar: %d keyframes -> %s (las copias NO pueden quedar tiradas)\n",
               nCanc, (nCanc==n0)?"volvio al original OK":"QUEDARON COPIAS!");

        // ---- DUPLICAR + CONFIRMAR + mover con valor exacto ----
        tl->ConstruirDopeRows(); tl->DopeSelectAll();
        tl->DopeDuplicarSeleccion();
        if (tl->DopeMoviendo()){
            for (const char* p="10"; *p; ++p) tl->DopeNumChar(*p);   // correr las copias 10 frames
            printf("         %s\n", tl->DopeTextoTransform().c_str());
            tl->DopeMoveConfirm();
        }
        printf("         Shift+D + mover 10 + confirmar: %d keyframes (mas que %d = las copias quedaron)\n",
               (int)ap->keyframes.size(), n0);

        // ---- EJES X / Y durante el MOVE (curvas) ----
        tl->ConstruirDopeRows();
        for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].claveFila==ck){ canal=&tl->dopeRows[i]; break; }
        ap = tl->CurvaDeFila(*canal);
        struct S { static void probar(Timeline* tl, AnimProperty* ap, int eje, const char* nom){
            std::vector<keyFrame> antes = ap->keyframes;
            tl->DopeSelectAll();
            tl->DopeMoveStart(Timeline::DOPE_MOV);
            if (!tl->DopeMoviendo()){ printf("         %s: no arranco\n", nom); return; }
            tl->DopeCiclarEje(eje);
            for (const char* p="5"; *p; ++p) tl->DopeNumChar(*p);
            tl->DopeMoveConfirm();
            int fCambio=0; float vMax=0.0f;
            for (size_t i=0;i<antes.size() && i<ap->keyframes.size();i++){
                if (ap->keyframes[i].frame != antes[i].frame) fCambio++;
                float dv = fabsf(ap->keyframes[i].value - antes[i].value); if (dv>vMax) vMax=dv; }
            printf("         mover 5 con eje %s -> frames que cambiaron=%d, valor maxdiff=%.5f\n", nom, fCambio, vMax);
        } };
        // eje X (1) = solo TIEMPO: los frames se corren y el valor NO se toca
        S::probar(tl, ap, Timeline::DOPE_EJE_X, "X (solo tiempo)");
        // eje Y (2) = solo VALOR: el valor cambia y los frames NO
        S::probar(tl, ap, Timeline::DOPE_EJE_Y, "Y (solo valor)");
        delete tl; return true;
    }
    // ---- dopecubre : el ownerKey de una fila PADRE no puede cubrir al de otro hueso solo por ser prefijo de su
    //      nombre ("arm:rig/b3" es prefijo de "arm:rig/b30"). Al borrar, eso se llevaba puesta animacion ajena. ----
    if (cmd == "dopecubre") {
        struct C { const char* padre; const char* hijo; bool esperado; };
        const C casos[] = {
            { "arm:rig/b3",  "arm:rig/b3",   true  },  // el hueso cubre sus propios canales (mismo ownerKey)
            { "arm:rig/b3",  "arm:rig/b30",  false },  // <- el bug: el hueso 3 se llevaba al 30
            { "arm:rig/b3",  "arm:rig/b31",  false },
            { "arm:rig",     "arm:rig/b3",   true  },  // el armature cubre a sus huesos
            { "arm:rig",     "arm:rig2/b1",  false },  // ...pero no a OTRO armature con nombre parecido
            { "obj:Cube",    "obj:Cube",     true  },
            { "obj:Cube",    "obj:Cube.001", false },  // <- el mismo bug con objetos
            { "",            "arm:rig/b7",   true  },  // el Summary cubre todo
        };
        int fallas = 0;
        for (int i=0;i<8;i++){
            bool r = W3dScriptDopeCubre(casos[i].padre, casos[i].hijo);
            bool ok = (r == casos[i].esperado);
            if (!ok) fallas++;
            printf("      [dopecubre] '%s' cubre '%s' -> %s (esperado %s) %s\n",
                   casos[i].padre, casos[i].hijo, r?"si":"no", casos[i].esperado?"si":"no", ok?"":"<-- MAL");
        }
        printf("      [dopecubre] %d/8 OK\n", 8-fallas);
        if (fallas){ err="dopecubre: el match de ownerKey esta mal"; return false; }
        return true;
    }
    // ---- dopewrap : al escalar/rotar keyframes el cursor se ENVUELVE de un borde al otro del timeline para poder
    //      seguir sin quedarse sin pantalla. El transform NO puede enterarse del salto: usa un cursor VIRTUAL que
    //      acumula dx/dy (que CheckWarpMouseInViewport pone en 0 justo en el frame del salto).
    //      Se simula el recorrido del mouse SIN envolver y CON envolver: tienen que dar EXACTAMENTE lo mismo. ----
    if (cmd == "dopewrap") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        Timeline::DopeRow* canal = NULL;
        for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0 && tl->dopeRows[i].keys.size()>=3){ canal=&tl->dopeRows[i]; break; }
        if (!canal){ err="dopewrap: sin canal con keyframes"; delete tl; return false; }
        std::string ck = canal->claveFila;

        // recorre 'pasos' de a 'paso' px hacia la derecha. envolver=true -> el cursor real salta al borde opuesto
        // cada vez que se pasa (y ahi dx=0, que es lo que hace el warp de verdad).
        struct S {
            // la curva viva de la fila 'ck' (para poder guardarla y restaurarla entre corridas)
            static AnimProperty* curva(Timeline* tl, const std::string& ck){
                for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].claveFila==ck) return tl->CurvaDeFila(tl->dopeRows[i]);
                return NULL;
            }
            // TODAS las curvas: el pivote del escalado se calcula sobre la seleccion ENTERA (18 canales), asi que
            // restaurar solo la que se mira dejaria las otras movidas y el pivote saldria distinto.
            static void guardar(Timeline* tl, std::vector<std::vector<keyFrame> >& out){
                out.clear();
                for (size_t i=0;i<tl->dopeRows.size();i++){
                    AnimProperty* p = tl->CurvaDeFila(tl->dopeRows[i]);
                    if (p) out.push_back(p->keyframes);
                }
            }
            static void restaurar(Timeline* tl, const std::vector<std::vector<keyFrame> >& in){
                size_t k = 0;
                for (size_t i=0;i<tl->dopeRows.size() && k<in.size();i++){
                    AnimProperty* p = tl->CurvaDeFila(tl->dopeRows[i]);
                    if (p) p->keyframes = in[k++];
                }
            }
            static std::vector<int> correr(Timeline* tl, const std::string& ck, int modo, int pasos, int paso, bool envolver){
                tl->ConstruirDopeRows(); tl->DopeSelectAll();
                tl->lastMx = tl->x + 500; tl->lastMy = tl->y + 150;
                tl->DopeMoveStart(modo);
                int mx = tl->lastMx, my = tl->lastMy;
                int bordeDer = tl->x + tl->width - 2;
                tl->event_mouse_motion(mx, my);        // 1er motion: ignora el delta viejo (arranca en cero)
                for (int i=0;i<pasos;i++){
                    int nx = mx + paso;
                    bool salto = envolver && nx >= bordeDer;
                    if (salto){
                        // el warp de verdad: el cursor real vuelve al otro borde y dx queda en 0 ese frame
                        dx = 0; dy = 0;
                        mx = tl->x + 2;
                        tl->event_mouse_motion(mx, my);
                        // ...y el proximo motion ya lleva el delta normal
                        dx = paso; dy = 0; mx += paso;
                        tl->event_mouse_motion(mx, my);
                    } else {
                        dx = paso; dy = 0; mx = nx;
                        tl->event_mouse_motion(mx, my);
                    }
                }
                std::string txt = tl->DopeTextoTransform();
                tl->DopeMoveConfirm();
                tl->ConstruirDopeRows();
                std::vector<int> fr;
                for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].claveFila==ck){
                    fr = tl->dopeRows[i].keys; break; }
                printf("         %-9s %s -> %s\n", envolver?"CON warp":"sin warp", txt.c_str(), "");
                return fr;
            }
        };
        // ---- El transform NO puede arrastrar la distancia que hoveraste desde el ultimo click ----
        // lastMouseX/Y globales solo los refrescan el CLICK y CheckWarpMouseInViewport; hoverando no corre ninguno.
        // Si al apretar 'g' no se sincronizan, el primer dx que calcula CheckWarp es (donde estas - ULTIMO CLICK) y
        // el cursor virtual se lo come: el transform arranca corrido esa distancia ENTERA, y para siempre.
        {
            std::vector<std::vector<keyFrame> > pre; S::guardar(tl, pre);
            tl->ConstruirDopeRows(); tl->DopeSelectAll();
            lastMouseX = (float)(tl->x + 100);      // el ultimo CLICK quedo lejos...
            lastMouseY = (float)(tl->y + 150);
            tl->lastMx = tl->x + 600;               // ...y el mouse hoveo hasta aca (500px a la derecha)
            tl->lastMy = tl->y + 150;
            tl->DopeMoveStart(Timeline::DOPE_MOV);
            // dos motions SIN mover el mouse: si el arranque se contamina, el 2do se come los 500px
            dx = 0; dy = 0;
            tl->event_mouse_motion(tl->x + 600, tl->y + 150);
            dx = (int)((float)(tl->x + 600) - lastMouseX); dy = 0;   // lo que calcularia CheckWarp si no se sincroniza
            tl->event_mouse_motion(tl->x + 600, tl->y + 150);
            std::string txt = tl->DopeTextoTransform();
            tl->DopeMoveCancel();
            S::restaurar(tl, pre);
            printf("      [dopewrap] 'g' tras hoverear 500px desde el ultimo click, sin mover: %s\n", txt.c_str());
            printf("         tiene que decir 0 frames (si dice ~43 se comio el hover) -> %s\n",
                   (txt.find("Move: 0 frames") != std::string::npos) ? "OK" : "SE COMIO EL HOVER!");
            if (txt.find("Move: 0 frames") == std::string::npos){
                err="dopewrap: el transform arranca corrido por la distancia hovereada"; delete tl; return false;
            }
        }

        // OJO: cada corrida CONFIRMA y deja la curva movida -> hay que restaurarla, o la 2da arranca desde el
        // resultado de la 1ra y "difieren" por eso y no por el warp.
        AnimProperty* ap = S::curva(tl, ck);
        if (!ap){ err="dopewrap: no resolvio la curva"; delete tl; return false; }
        std::vector<std::vector<keyFrame> > base; S::guardar(tl, base);

        // el recorrido es MAS LARGO que el viewport a proposito: sin envolver te quedarias sin pantalla
        printf("      [dopewrap] MOVER: 40 pasos de 30px (1200px) en un timeline de %dpx de ancho\n", tl->width);
        std::vector<int> a = S::correr(tl, ck, Timeline::DOPE_MOV, 40, 30, false);
        S::restaurar(tl, base);                                  // volver al estado inicial (TODAS las curvas)
        std::vector<int> b = S::correr(tl, ck, Timeline::DOPE_MOV, 40, 30, true);
        S::restaurar(tl, base);
        bool ok1 = (a.size()==b.size() && !a.empty());
        for (size_t i=0;i<a.size() && ok1;i++) if (a[i]!=b[i]) ok1=false;
        printf("         primer/ultimo frame: sin warp [%d..%d] | CON warp [%d..%d] -> %s\n",
               a.empty()?0:a[0], a.empty()?0:a[a.size()-1], b.empty()?0:b[0], b.empty()?0:b[b.size()-1],
               ok1?"IDENTICO OK (el salto del cursor no se ve)":"DISTINTOS!");
        printf("      [dopewrap] ESCALAR: idem\n");
        std::vector<int> c = S::correr(tl, ck, Timeline::DOPE_ESC, 40, 30, false);
        S::restaurar(tl, base);
        std::vector<int> d = S::correr(tl, ck, Timeline::DOPE_ESC, 40, 30, true);
        S::restaurar(tl, base);
        bool ok2 = (c.size()==d.size() && !c.empty());
        for (size_t i=0;i<c.size() && ok2;i++) if (c[i]!=d[i]) ok2=false;
        printf("         primer/ultimo frame: sin warp [%d..%d] | CON warp [%d..%d] -> %s\n",
               c.empty()?0:c[0], c.empty()?0:c[c.size()-1], d.empty()?0:d[0], d.empty()?0:d[d.size()-1],
               ok2?"IDENTICO OK":"DISTINTOS!");
        delete tl;
        if (!ok1 || !ok2){ err="dopewrap: el warp cambia el resultado del transform"; return false; }
        return true;
    }
    // ---- handletest : los 5 tipos de handle y el transform de handles (r/s sobre UN solo keyframe). ----
    if (cmd == "handletest") {
        // curva en pico: 0 -> 10 -> 0. Es la que muestra la diferencia entre Automatic y Auto Clamped.
        AnimProperty ap; ap.Property = AnimPosition; ap.component = AnimX;
        SetKeyCurva(ap, 0, 0.0f); SetKeyCurva(ap, 10, 10.0f); SetKeyCurva(ap, 20, 0.0f);
        struct S { static void tipo(AnimProperty& ap, int t, const char* nom){
            for (size_t i=0;i<ap.keyframes.size();i++){ ap.keyframes[i].Interpolation = KfBezier;
                ap.keyframes[i].handleType = t; }
            float mx = -1e9f;
            for (int f=0; f<=20; f++){ float v = ap.EvalF((float)f,0); if (v>mx) mx=v; }
            printf("      [handletest] %-13s f=5,10,15 -> %6.3f %6.3f %6.3f   maximo=%.3f\n", nom,
                   ap.EvalF(5,0), ap.EvalF(10,0), ap.EvalF(15,0), mx);
        } };
        S::tipo(ap, HVector,      "Vector");
        S::tipo(ap, HAuto,        "Automatic");
        S::tipo(ap, HAutoClamped, "Auto Clamped");
        // VECTOR tiene que dar lo MISMO que lineal: los handles apuntan al vecino -> el tramo sale recto
        for (size_t i=0;i<ap.keyframes.size();i++) ap.keyframes[i].handleType = HVector;
        float vb = ap.EvalF(5,0);
        for (size_t i=0;i<ap.keyframes.size();i++) ap.keyframes[i].Interpolation = KfLinear;
        float vl = ap.EvalF(5,0);
        printf("         Vector=%.4f vs Linear=%.4f -> %s\n", vb, vl,
               (fabsf(vb-vl)<1e-3f)?"IGUAL OK (vector = recta)":"DIFIEREN!");
        // AUTO CLAMPED no puede pasarse del valor del keyframe (10); AUTOMATIC en este pico tampoco sobrepica,
        // asi que se prueba con un pico ASIMETRICO, que es donde Automatic si se pasa.
        // Pico ASIMETRICO y filoso: el keyframe de 10 tiene los dos vecinos abajo (es un maximo local) pero uno
        // muy cerca. Ahi Automatic apunta el handle hacia arriba y la curva SE PASA de 10; Auto Clamped lo aplana.
        // Se muestrea fino (0.05) porque el sobrepico cae ENTRE frames enteros.
        AnimProperty pk; pk.Property = AnimPosition; pk.component = AnimX;
        SetKeyCurva(pk, 0, 0.0f); SetKeyCurva(pk, 10, 10.0f); SetKeyCurva(pk, 11, 9.9f); SetKeyCurva(pk, 20, 0.0f);
        struct P { static float pico(AnimProperty& p, int t){
            for (size_t i=0;i<p.keyframes.size();i++){ p.keyframes[i].Interpolation = KfBezier; p.keyframes[i].handleType = t; }
            float mx=-1e9f; for (float f=0.0f; f<=20.0f; f+=0.05f){ float v=p.EvalF(f,0); if (v>mx) mx=v; } return mx;
        } };
        float pAuto = P::pico(pk, HAuto), pClamp = P::pico(pk, HAutoClamped);
        printf("         pico filoso (el keyframe mas alto vale 10): Automatic llega a %.4f (se pasa %.4f) | Auto Clamped a %.4f\n",
               pAuto, pAuto-10.0f, pClamp);
        printf("         -> Automatic SE PASA: %s | Auto Clamped NO se pasa: %s\n",
               (pAuto > 10.0f + 1e-3f)?"si (es lo esperado)":"no (el caso no lo dispara)",
               (pClamp <= 10.0f + 1e-3f)?"OK":"SE PASO!");

        // ---- ROTAR / ESCALAR los handles de UN solo keyframe ----
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        Timeline::DopeRow* canal = NULL;
        for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0 && tl->dopeRows[i].keys.size()>=3){ canal=&tl->dopeRows[i]; break; }
        if (canal){
            AnimProperty* c = tl->CurvaDeFila(*canal);
            const int idx = 1;
            int f0 = c->keyframes[idx].frame;
            float vf0 = c->keyframes[idx].value;
            // seleccionar SOLO ese keyframe (por el camino real: click) y ponerlo bezier
            // Hay 18 canales encimados cerca del valor 0: un click ahi elige el keyframe MAS CERCANO, que no tiene
            // por que ser el de esta curva. Se centra la vista en el valor del keyframe buscado y se estira el eje
            // vertical, asi los de las otras curvas quedan lejos y el click es inequivoco.
            tl->viewCenterV = vf0; tl->pxPerUnit = 20000.0f;
            tl->DopeSelectNone();
            bool sel1 = tl->CurvaClickStrip((int)(tl->x + tl->FrameToX((float)f0)), (int)(tl->y + tl->ValueToY(vf0)));
            tl->SetInterpolacionSel(KfBezier);
            tl->SetHandleTypeSel(HAligned);
            printf("         keyframe f=%d seleccionado=%s interp=%d tipoHandle=%d out=(%.4f,%.4f)\n",
                   f0, sel1?"si":"NO", c->keyframes[idx].Interpolation, c->keyframes[idx].handleType,
                   c->keyframes[idx].outDF, c->keyframes[idx].outDV);
            float oDF0 = c->keyframes[idx].outDF, oDV0 = c->keyframes[idx].outDV;
            float L0 = sqrtf(oDF0*oDF0*tl->pxPerFrame*tl->pxPerFrame + oDV0*oDV0*tl->pxPerUnit*tl->pxPerUnit);
            // ESCALAR x2: la DISTANCIA del handle se duplica
            tl->lastMx = tl->x + 500; tl->lastMy = tl->y + 150;
            tl->DopeMoveStart(Timeline::DOPE_ESC);
            for (const char* p="2"; *p; ++p) tl->DopeNumChar(*p);
            tl->DopeMoveConfirm();
            float oDF1 = c->keyframes[idx].outDF, oDV1 = c->keyframes[idx].outDV;
            float L1 = sqrtf(oDF1*oDF1*tl->pxPerFrame*tl->pxPerFrame + oDV1*oDV1*tl->pxPerUnit*tl->pxPerUnit);
            printf("         ESCALAR x2 el handle de un keyframe solo: largo %.2fpx -> %.2fpx (razon %.3f) -> %s\n",
                   L0, L1, L0>1e-6f?L1/L0:0.0f, (L0>1e-6f && fabsf(L1/L0 - 2.0f)<0.05f)?"OK":"MAL");
            printf("         el keyframe NO se movio: f=%d val=%.4f -> %s\n", c->keyframes[idx].frame, c->keyframes[idx].value,
                   (c->keyframes[idx].frame==f0 && fabsf(c->keyframes[idx].value-vf0)<1e-5f)?"OK":"SE MOVIO!");
            // ROTAR 180: el handle se da vuelta (pero el tiempo lo frena para que x siga siendo monotono)
            float aDF0 = c->keyframes[idx].inDF, aDV0 = c->keyframes[idx].inDV;
            tl->DopeMoveStart(Timeline::DOPE_ROT);
            for (const char* p="180"; *p; ++p) tl->DopeNumChar(*p);
            printf("         %s\n", tl->DopeTextoTransform().c_str());
            tl->DopeMoveConfirm();
            printf("         ROTAR 180 el handle de ENTRADA: dV %.3f -> %.3f -> %s\n", aDV0, c->keyframes[idx].inDV,
                   (fabsf(c->keyframes[idx].inDV + aDV0) < 0.01f*(1.0f+fabsf(aDV0)))?"se dio vuelta OK":"?");
        }
        delete tl; return true;
    }
    // ---- objbezier : REPRO del crash. Animacion de OBJETO (mover un cubo y keyframearlo), modo curvas, y ponerle
    //      Bezier a los keyframes (lo que hace 't' -> Bezier). La animacion de objeto NO pasa por el mismo camino
    //      que la de armature. ----
    if (cmd == "objbezier") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        printf("      [objbezier] filas del dope sheet = %d\n", (int)tl->dopeRows.size());
        if (tl->dopeRows.empty()){ err="objbezier: sin filas (corre objkeytest antes)"; delete tl; return false; }
        tl->DopeSelectAll();
        printf("      [objbezier] poniendo Bezier a todo lo seleccionado...\n");
        tl->SetInterpolacionSel(KfBezier);
        printf("      [objbezier] interpolacion aplicada OK\n");
        // evaluar la animacion (lo que hace el playback) y dibujar el trazo: aca es donde revienta si algo esta mal
        AplicarAnimacionObjetos();
        printf("      [objbezier] AplicarAnimacionObjetos OK\n");
        long long v = tl->CurvaTrazoCosto();
        printf("      [objbezier] trazo de las curvas OK (%lld vertices)\n", v);
        // y el handle type, que es lo otro que toca 'v'
        tl->SetHandleTypeSel(HAligned);
        AplicarAnimacionObjetos();
        printf("      [objbezier] Handle Type OK\n");

        // ---- EL CRASH DE VERDAD: por el camino REAL del menu. 't' abre Interpolation Mode SOLO (sin haber
        //      abierto nunca el menu Key), y al elegir un item Click() ya cierra el menu -> MenuAbierto = NULL.
        //      Si la accion compara MenuAbierto contra un menu que TAMBIEN es NULL, "NULL == NULL" da true y
        //      llama Cerrar() sobre NULL. Se simula igual que LayoutMenuDragSoltar: Click + action. ----
        struct M {
            static bool elegir(PopupMenu* m, int fila){        // click en la fila N, como lo calcula PopupMenu::Click
                int oy = m->titulo.empty() ? 0 : (RenglonHeightGS + gapGS);
                int my = m->y + borderGS + oy + fila*(RenglonHeightGS + gapGS) + 1;
                int mx = m->x + m->width/2;
                int id = m->Click(mx, my);
                if (id >= 0 && m->action) m->action(id);       // <-- aca crasheaba
                return id >= 0;
            }
        };
        tl->DopeSelectAll();
        tl->AbrirMenuInterp(100, 100);
        printf("      [objbezier] 't' abrio Interpolation Mode (menu Key nunca abierto)\n");
        if (!MenuAbierto){ err="objbezier: el menu no quedo abierto"; delete tl; return false; }
        bool ok = M::elegir(MenuAbierto, 2);                   // fila 2 = Bezier
        printf("      [objbezier] elegido 'Bezier' desde el menu -> %s (si llegaste aca, no crasheo)\n",
               ok?"OK":"no se eligio nada");
        // y ahora el de Handle Type ('v'), por las dudas
        tl->AbrirMenuHandle(100, 100);
        if (MenuAbierto) M::elegir(MenuAbierto, 4);            // fila 4 = Auto Clamped
        printf("      [objbezier] elegido 'Auto Clamped' desde el menu 'v' -> OK\n");
        AplicarAnimacionObjetos();
        delete tl; return true;
    }
    // ---- menudump : estructura de la barra y del menu Key. Verifica que no haya menus DUPLICADOS, que 'Key'
    //      este siempre, y que Handle Type viva adentro de Key. ----
    if (cmd == "menudump") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        for (int paso=0; paso<2; paso++){
            bool conSel = (paso==1);
            DeseleccionarTodo(); ObjSelects.clear(); ObjActivo=NULL;
            // el que TIENE animacion (el que keyframeo objkeytest), no cualquiera
            if (conSel && !AnimationObjects.empty() && AnimationObjects[0].obj){
                Object* o = AnimationObjects[0].obj;
                ObjActivo=o; o->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(o);
            }
            tl->modo = Timeline::TL_MODO_CURVAS;
            tl->ConstruirDopeRows();
            tl->SyncFields();          // la visibilidad de la barra sale de aca (Render los llama en este orden)
            printf("      [menudump] %s seleccion -> filas=%d | botones visibles de la barra:",
                   conSel?"CON":"SIN", (int)tl->dopeRows.size());
            for (size_t i=0;i<tl->BarButtons.size();i++){
                Button* b = tl->BarButtons[i];
                if (b->visible && !b->text.empty()) printf(" [%s]", b->text.c_str());
            }
            printf("\n");
        }
        // el menu Key y sus submenus
        tl->AbrirMenuKey(100, 100);
        PopupMenu* m = MenuAbierto;
        if (!m){ err="menudump: el menu Key no abrio"; delete tl; return false; }
        printf("      [menudump] menu 'Key':\n");
        for (size_t i=0;i<m->items.size();i++){
            MenuItem* it = m->items[i];
            printf("         %-20s %s\n", it->text.c_str(), it->submenu ? "(submenu)" : "");
            if (it->submenu) for (size_t j=0;j<it->submenu->items.size();j++)
                printf("              - %s\n", it->submenu->items[j]->text.c_str());
        }
        delete tl; return true;
    }
    // ---- ojotest : el OJO de las filas del dope sheet. Apagarlo tiene que sacar la curva del dibujo Y de la
    //      edicion (si no, seguirias moviendo lo que no ves). Y el de un PADRE baja a sus canales. ----
    if (cmd == "ojotest") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        if (tl->dopeRows.empty()){ err="ojotest: sin filas"; delete tl; return false; }
        tl->DopeSelectAll();
        long long v0 = tl->CurvaTrazoCosto();
        int sel0 = 0; for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0) sel0++;
        printf("      [ojotest] %d canales | trazo=%lld vertices | columna de ojos en x=%d (panel=%d)\n",
               sel0, v0, tl->DopeOjoX(), tl->panelW);
        // apagar el ojo de UN canal: click en su ojo (camino real)
        int fila = -1; for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0){ fila=(int)i; break; }
        std::string nom = tl->dopeRows[fila].nombre;
        int oy = tl->stripY + fila*tl->rowH + tl->PosY + tl->rowH/2;
        tl->DopeClickPanel(tl->x + tl->DopeOjoX() + 1, tl->y + oy);
        tl->ConstruirDopeRows();
        int ocultas = 0; for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].oculto) ocultas++;
        long long v1 = tl->CurvaTrazoCosto();
        tl->DopeSelectAll();
        printf("         apagado el ojo de '%s': filas ocultas=%d | trazo=%lld -> %s\n",
               nom.c_str(), ocultas, v1, (v1 < v0)?"la curva NO se dibuja OK":"SIGUE DIBUJANDOSE!");
        // ...y el ojo de una fila PADRE apaga a todos sus hijos
        int padre = -1; for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].tipo==0){ padre=(int)i; break; }
        if (padre >= 0){
            int py = tl->stripY + padre*tl->rowH + tl->PosY + tl->rowH/2;
            tl->DopeClickPanel(tl->x + tl->DopeOjoX() + 1, tl->y + py);
            tl->ConstruirDopeRows();
            int todas = 0, tot = (int)tl->dopeRows.size();
            for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].oculto) todas++;
            long long v2 = tl->CurvaTrazoCosto();
            tl->DopeSelectAll();
            printf("         apagado el ojo del Summary: ocultas=%d de %d | trazo=%lld -> %s\n",
                   todas, tot, v2, (todas==tot && v2==0)?"baja a TODOS los hijos OK":"NO bajo!");
        }
        delete tl; return true;
    }
    // ---- kfcard : la tarjeta "Keyframe" del panel de propiedades. Aparece SOLO con un keyframe elegido, y sus
    //      campos tienen que reflejar la curva VIVA (frame, valor, interpolacion, tipo y los dos handles). ----
    if (cmd == "kfcard") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        if (tl->dopeRows.empty()){ err="kfcard: sin filas"; delete tl; return false; }
        int idx = -1;
        printf("      [kfcard] sin keyframe elegido -> DopeKeyframeActivo=%s\n",
               DopeKeyframeActivo(&idx) ? "algo (MAL)" : "NULL (la tarjeta no se muestra) OK");
        // elegir uno por el camino real (click), separando las curvas para que no sea ambiguo
        Timeline::DopeRow* canal = NULL;
        for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0 && tl->dopeRows[i].keys.size()>=3){ canal=&tl->dopeRows[i]; break; }
        AnimProperty* c = tl->CurvaDeFila(*canal);
        int f0 = c->keyframes[1].frame; float v0 = c->keyframes[1].value;
        tl->viewCenterV = v0; tl->pxPerUnit = 20000.0f;
        tl->DopeSelectNone();
        tl->CurvaClickStrip((int)(tl->x + tl->FrameToX((float)f0)), (int)(tl->y + tl->ValueToY(v0)));
        AnimProperty* ap = DopeKeyframeActivo(&idx);
        printf("      [kfcard] click en un keyframe -> activo=%s canal='%s'\n",
               ap?"si":"NO", DopeKeyframeActivoCanal().c_str());
        if (!ap){ err="kfcard: el click no dejo keyframe activo"; delete tl; return false; }
        const keyFrame& k = ap->keyframes[idx];
        printf("         Frame X=%d | Value Y=%.4f | Interp=%d | HandleType=%d\n",
               k.frame, k.value, k.Interpolation, k.handleType);
        printf("         Left Handle=(%.3f, %.3f) | Right Handle=(%.3f, %.3f)\n",
               k.inDF, k.inDV, k.outDF, k.outDV);
        // MOVER el keyframe con 'g' no puede hacer desaparecer la tarjeta: el activo se identifica por
        // (curva, frame), asi que el transform lo tiene que SEGUIR.
        tl->lastMx = tl->x + 500; tl->lastMy = tl->y + 150;
        tl->DopeMoveStart(Timeline::DOPE_MOV);
        for (const char* q="5"; *q; ++q) tl->DopeNumChar(*q);
        tl->DopeMoveConfirm();
        int i2; AnimProperty* ap2 = DopeKeyframeActivo(&i2);
        printf("         tras mover el keyframe 5 frames con 'g' -> la tarjeta sigue: %s (frame ahora=%d, esperado=%d)\n",
               ap2 ? "SI OK" : "NO (se perdio el activo)", ap2 ? ap2->keyframes[i2].frame : -1, f0+5);
        if (!ap2 || ap2->keyframes[i2].frame != f0 + 5){
            err="kfcard: el keyframe activo no siguio al transform"; delete tl; return false; }
        // deseleccionar -> la tarjeta se tiene que ir
        tl->DopeSelectNone();
        printf("         deseleccionado -> DopeKeyframeActivo=%s\n",
               DopeKeyframeActivo(&idx) ? "algo (MAL: la tarjeta quedaria)" : "NULL OK");
        delete tl; return true;
    }
    // ---- icontest : el icono "keyframe" del atlas. Verifica que sus UV apunten al pixel que corresponde
    //      (95,10 de 10x10) y que la tarjeta "Keyframe" lo tenga puesto. Si el enum y la lista de UVs se
    //      desalinean, TODOS los iconos de ahi para abajo salen corridos y no se nota mirando uno solo. ----
    if (cmd == "icontest") {
        printf("      [icontest] ICON_TOTAL=%d | IconsUV cargados=%d -> %s\n",
               (int)ICON_TOTAL, (int)IconsUV.size(),
               (IconsUV.size() == ICON_TOTAL) ? "coinciden OK" : "DESALINEADOS!");
        if (IconsUV.size() != ICON_TOTAL){ err="icontest: el enum y la lista de UVs no tienen el mismo largo"; return false; }
        size_t ik = (size_t)IconType::keyframe;
        if (ik >= IconsUV.size()){ err="icontest: keyframe fuera de rango"; return false; }
        // Los UV son FRACCION del atlas. Se vuelven a pixeles usando el tamano real del archivo (font.png, que
        // es de donde CrearIconos los saca) y se comparan contra el pixel pedido. Asi el test falla tambien si
        // alguien mete un icono en el medio de la lista y desalinea todo lo de abajo.
        const int AW = 128, AH = 128;   // font.png
        const GLfloat* uv = IconsUV[ik]->uvs;
        int px = (int)(uv[0]*AW + 0.5f), py = (int)(uv[1]*AH + 0.5f);
        int pw = (int)((uv[6]-uv[0])*AW + 0.5f), ph = (int)((uv[7]-uv[1])*AH + 0.5f);
        printf("      [icontest] keyframe uv=(%.4f,%.4f)..(%.4f,%.4f) -> pixel (%d,%d) de %dx%d (esperado 95,10 de 10x10) -> %s\n",
               uv[0], uv[1], uv[6], uv[7], px, py, pw, ph,
               (px==95 && py==10 && pw==10 && ph==10) ? "OK" : "MAL");
        if (!(px==95 && py==10 && pw==10 && ph==10)){ err="icontest: el UV del keyframe no cae en 95,10"; return false; }
        // y que la tarjeta lo tenga. Las tarjetas las arma Properties::ConstruirGrupos, que corre al crear el
        // viewport: en headless no hay ninguno, asi que se crea uno aca.
        Properties* pr = new Properties();
        // OJO: las tarjetas van al vector MIEMBRO del panel (Properties::GroupProperties), que ENSOMBRECE al
        // global del mismo nombre declarado en GroupPropertie.h. El global queda vacio.
        GroupPropertie* g = NULL;
        for (size_t i=0;i<pr->GroupProperties.size();i++)
            if (pr->GroupProperties[i]->name.compare(0,8,"Keyframe")==0){ g = pr->GroupProperties[i]; break; }
        printf("      [icontest] Properties armo %d tarjetas\n", (int)pr->GroupProperties.size());
        printf("      [icontest] tarjeta 'Keyframe': %s | icono=%d (keyframe=%d) -> %s\n",
               g?"existe":"NO EXISTE", g?g->icono:-99, (int)IconType::keyframe,
               (g && g->icono == (int)IconType::keyframe) ? "OK" : "MAL");
        bool okCard = (g && g->icono == (int)IconType::keyframe);
        delete pr;
        if (!okCard){ err="icontest: la tarjeta Keyframe no tiene el icono"; return false; }
        return true;
    }
    // ---- handlewrap : arrastrar un HANDLE mas alla del borde. El cursor se envuelve (el warp corre porque el
    //      boton esta apretado) y el arrastre NO se tiene que enterar: arrastrando para ABAJO el handle tiene que
    //      SEGUIR BAJANDO, aunque el cursor reaparezca arriba. Se compara el recorrido sin warp contra el mismo
    //      recorrido CON warp: tienen que dar el mismo handle. ----
    if (cmd == "handlewrap") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        Timeline::DopeRow* canal = NULL;
        for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0 && tl->dopeRows[i].keys.size()>=3){ canal=&tl->dopeRows[i]; break; }
        if (!canal){ err="handlewrap: sin canal"; delete tl; return false; }
        AnimProperty* ap = tl->CurvaDeFila(*canal);
        const int idx = 1;
        int f0 = ap->keyframes[idx].frame; float v0 = ap->keyframes[idx].value;
        // separar las curvas para que el click sea inequivoco (18 canales encimados cerca del 0)
        tl->viewCenterV = v0; tl->pxPerUnit = 20000.0f;
        tl->DopeSelectNone();
        tl->CurvaClickStrip((int)(tl->x + tl->FrameToX((float)f0)), (int)(tl->y + tl->ValueToY(v0)));
        tl->SetInterpolacionSel(KfBezier);
        tl->SetHandleTypeSel(HFree);
        std::vector<keyFrame> base = ap->keyframes;

        // arrastra el handle 'pasos' de a 'paso' px hacia ABAJO. envolver=true -> el cursor real salta arriba al
        // pasarse del borde (y ahi dx/dy quedan en 0, que es lo que hace el warp de verdad).
        struct S {
            static void arrastrar(Timeline* tl, AnimProperty* ap, int idx, int pasos, int paso, bool envolver){
                int f0 = ap->keyframes[idx].frame; float v0 = ap->keyframes[idx].value;
                float hx, hy; tl->HandlePos(ap, (size_t)idx, true, hx, hy);
                leftMouseDown = true;
                tl->CurvaClickStrip((int)(tl->x + hx), (int)(tl->y + hy));   // agarrar el handle
                int mx = (int)(tl->x + hx), my = (int)(tl->y + hy);
                int bordeAbajo = tl->y + tl->height - 2;
                dx = 0; dy = 0;
                tl->event_mouse_motion(mx, my);          // 1er motion: ignora el delta viejo
                for (int i=0;i<pasos;i++){
                    int ny = my + paso;                  // ABAJO
                    if (envolver && ny >= bordeAbajo){
                        dx = 0; dy = 0;                  // el warp: el cursor real vuelve ARRIBA y dy=0 ese frame
                        my = tl->y + tl->stripY + 2;
                        tl->event_mouse_motion(mx, my);
                        dx = 0; dy = paso; my += paso;   // y el proximo motion ya lleva el delta normal
                        tl->event_mouse_motion(mx, my);
                    } else {
                        dx = 0; dy = paso; my = ny;
                        tl->event_mouse_motion(mx, my);
                    }
                }
                tl->HandleSoltar();
                leftMouseDown = false;
                (void)f0; (void)v0;
            }
        };
        printf("      [handlewrap] arrastrando el handle 40 pasos de 20px hacia ABAJO (800px) en un strip de %dpx\n",
               tl->height - tl->stripY);
        S::arrastrar(tl, ap, idx, 40, 20, false);
        float sinF = ap->keyframes[idx].outDF, sinV = ap->keyframes[idx].outDV;
        ap->keyframes = base;
        S::arrastrar(tl, ap, idx, 40, 20, true);
        float conF = ap->keyframes[idx].outDF, conV = ap->keyframes[idx].outDV;
        ap->keyframes = base;
        printf("         sin warp: handle=(%.4f, %.4f)\n", sinF, sinV);
        printf("         CON warp: handle=(%.4f, %.4f)\n", conF, conV);
        bool ok = fabsf(sinF-conF) < 1e-3f && fabsf(sinV-conV) < 1e-3f*(1.0f+fabsf(sinV));
        printf("         -> %s\n", ok ? "IDENTICO OK (el salto del cursor no se ve; sigue bajando)"
                                      : "DISTINTOS! (el handle salta con el cursor)");
        // y que efectivamente haya BAJADO (dV negativo: arrastramos para abajo, y el valor baja hacia abajo)
        printf("         el handle BAJO (dV<0): %s\n", (sinV < 0.0f) ? "si OK" : "NO (no siguio el arrastre)");
        delete tl;
        if (!ok){ err="handlewrap: el warp cambia el arrastre del handle"; return false; }
        return true;
    }
    // ---- autokeytest : AUTO KEY. Lo central: guardar SOLO los canales que CAMBIARON. Si solo se rota en X, tiene
    //      que quedar UN keyframe en "X Euler Rotation" y NADA en los otros 8 canales. ----
    if (cmd == "autokeytest") {
        Object* o = NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !o){ Object* n=st.back(); st.pop_back();
            if (n->getType()==ObjectType::mesh) o=n;
            for(size_t i=0;i<n->Childrens.size();i++) st.push_back(n->Childrens[i]); } }
        if (!o){ err="autokeytest: sin malla"; return false; }
        DeseleccionarTodo(); ObjActivo=o; o->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(o);
        EliminarAnimaciones(*o);   // arrancar sin animacion

        struct S {
            // cuantos keyframes tiene cada canal del objeto
            static void dump(Object* o, const char* titulo){
                const char* nP[3] = {"X Location","Y Location","Z Location"};
                const char* nR[3] = {"X Euler Rotation","Y Euler Rotation","Z Euler Rotation"};
                const char* nS[3] = {"X Scale","Y Scale","Z Scale"};
                const char* const* noms[3] = { nP, nR, nS };
                const int props[3] = { AnimPosition, AnimRotation, AnimScale };
                printf("         %s ->", titulo);
                bool algo = false;
                for (size_t i=0;i<AnimationObjects.size();i++){
                    if (AnimationObjects[i].obj != o) continue;
                    for (int p=0;p<3;p++) for (int c=0;c<3;c++){
                        for (size_t q=0;q<AnimationObjects[i].Propertys.size();q++){
                            AnimProperty& ap = AnimationObjects[i].Propertys[q];
                            if (ap.Property!=props[p] || ap.component!=((c==0)?AnimX:(c==1)?AnimY:AnimZ)) continue;
                            if (ap.keyframes.empty()) continue;
                            printf(" %s(%d)", noms[p][c], (int)ap.keyframes.size()); algo = true;
                        }
                    }
                }
                if (!algo) printf(" (nada)");
                printf("\n");
            }
            static int canales(Object* o){
                int n = 0;
                for (size_t i=0;i<AnimationObjects.size();i++){ if (AnimationObjects[i].obj != o) continue;
                    for (size_t q=0;q<AnimationObjects[i].Propertys.size();q++)
                        if (!AnimationObjects[i].Propertys[q].keyframes.empty()) n++; }
                return n;
            }
            // simula un transform: snapshot -> cambiar -> confirmar (que es lo que llama al auto key)
            static void xform(Object* o){
                estadoObjetos.clear();
                SaveState st; st.obj=o; st.pos=o->pos; st.rot=o->rot; st.scale=o->scale; st.worldPos=o->pos;
                estadoObjetos.push_back(st);
            }
        };

        // ---- AUTO KEY APAGADO: no tiene que guardar NADA ----
        AutoKeyOn = false;
        CurrentFrame = 1;
        S::xform(o);
        o->pos.x += 5.0f;
        AutoKeyObjetos();
        printf("      [autokeytest] auto key APAGADO, movi en X: canales con keyframes=%d -> %s\n",
               S::canales(o), (S::canales(o)==0) ? "no guardo nada OK" : "GUARDO CON AUTOKEY APAGADO!");
        if (S::canales(o) != 0){ err="autokeytest: guardo con auto key apagado"; return false; }

        // ---- AUTO KEY PRENDIDO: solo el canal que cambio ----
        AutoKeyOn = true;
        EliminarAnimaciones(*o);
        CurrentFrame = 1;
        S::xform(o);
        o->pos.x += 5.0f;                       // SOLO position X
        int n1 = AutoKeyObjetos();
        printf("      [autokeytest] movi SOLO en X: guardo %d canal(es)\n", n1);
        S::dump(o, "canales");
        bool ok1 = (n1 == 1 && S::canales(o) == 1);
        printf("         -> %s\n", ok1 ? "SOLO X Location OK" : "MAL (guardo canales que no cambiaron)");
        if (!ok1){ err="autokeytest: no guardo solo el canal que cambio"; return false; }

        // ---- otro frame, SOLO rotacion X: no puede tocar los canales de position ----
        CurrentFrame = 20;
        S::xform(o);
        // FromAxisAngle es ESTATICA y toma GRADOS (llamarla como metodo de instancia compila pero descarta el
        // resultado -> el quaternion quedaba identidad y no rotaba nada)
        { Quaternion qx = Quaternion::FromAxisAngle(Vector3(1,0,0), 30.0f); o->rot = qx * o->rot; o->ActualizarDisplayRot(); }
        int n2 = AutoKeyObjetos();
        printf("      [autokeytest] frame 20, rote SOLO en X: guardo %d canal(es)\n", n2);
        S::dump(o, "canales");
        // X Location sigue con 1 solo keyframe (el del frame 1): la rotacion no lo tiene que tocar
        int kfLocX = 0;
        for (size_t i=0;i<AnimationObjects.size();i++){ if (AnimationObjects[i].obj != o) continue;
            for (size_t q=0;q<AnimationObjects[i].Propertys.size();q++){
                AnimProperty& ap = AnimationObjects[i].Propertys[q];
                if (ap.Property==AnimPosition && ap.component==AnimX) kfLocX = (int)ap.keyframes.size(); } }
        printf("         X Location sigue con %d keyframe(s) (esperado 1: la rotacion no lo toca) -> %s\n",
               kfLocX, (kfLocX==1) ? "OK" : "MAL");
        if (kfLocX != 1){ err="autokeytest: la rotacion ensucio los canales de position"; return false; }

        // ---- transform que NO cambia nada: no guarda ----
        int antes = S::canales(o);
        CurrentFrame = 30;
        S::xform(o);
        int n3 = AutoKeyObjetos();     // sin tocar nada
        printf("      [autokeytest] confirmar SIN cambiar nada: guardo %d canal(es) -> %s\n",
               n3, (n3==0 && S::canales(o)==antes) ? "no ensucia OK" : "GUARDO DE GEDE!");
        if (n3 != 0){ err="autokeytest: guardo sin que cambiara nada"; return false; }
        AutoKeyOn = false;
        estadoObjetos.clear();
        return true;
    }
    // ---- autoframetest : AUTO FRAME (View > Auto frame, prendido por defecto). Clickear una fila del panel tiene
    //      que elegir TODA su curva y encuadrarla sola, sin apretar numpad '.'. Apagado, la vista no se mueve. ----
    if (cmd == "autoframetest") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        if (tl->dopeRows.empty()){ err="autoframetest: sin filas"; delete tl; return false; }
        // el menu View: Frame Selected + el checkbox Auto frame
        tl->AbrirMenuView(100, 100);
        PopupMenu* m = MenuAbierto;
        if (!m){ err="autoframetest: el menu View no abrio"; delete tl; return false; }
        printf("      [autoframetest] menu 'View':\n");
        bool hayCheck = false; bool* estado = NULL;
        for (size_t i=0;i<m->items.size();i++){
            printf("         %-16s %s\n", m->items[i]->text.c_str(),
                   m->items[i]->checkbox ? (*m->items[i]->checkbox ? "[x] checkbox" : "[ ] checkbox") : "");
            if (m->items[i]->checkbox){ hayCheck = true; estado = m->items[i]->checkbox; }
        }
        if (!hayCheck){ err="autoframetest: no hay checkbox en View"; delete tl; return false; }
        printf("      [autoframetest] Auto frame por defecto: %s -> %s\n",
               *estado ? "PRENDIDO" : "apagado", *estado ? "OK" : "MAL (tiene que venir prendido)");
        if (!*estado){ err="autoframetest: Auto frame no viene prendido"; delete tl; return false; }
        m->Cerrar();

        // buscar la fila de un CANAL y clickearla (el camino real)
        int fila = -1;
        for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0 && tl->dopeRows[i].keys.size()>=2){ fila=(int)i; break; }
        if (fila < 0){ err="autoframetest: sin canal"; delete tl; return false; }
        std::string nom = tl->dopeRows[fila].nombre;

        struct S {
            static void clickFila(Timeline* tl, int fila){
                int fy = tl->stripY + fila*tl->rowH + tl->PosY + tl->rowH/2;
                tl->DopeClickPanel(tl->x + tl->panelW/2, tl->y + fy);   // en el NOMBRE (no en la flecha ni el ojo)
            }
        };

        // ---- PRENDIDO: elige la curva Y encuadra ----
        tl->DopeSelectNone();
        tl->viewStartF = -500.0f; tl->pxPerFrame = 1.0f;   // vista lejos y desencuadrada
        float vs0 = tl->viewStartF, px0 = tl->pxPerFrame;
        S::clickFila(tl, fila);
        int sel = 0, mn, mx;
        bool hayRango = tl->DopeRangoSeleccion(mn, mx);
        for (size_t i=0;i<tl->dopeRows.size();i++){} // (la seleccion se cuenta por el rango)
        printf("      [autoframetest] click en '%s' con Auto frame PRENDIDO:\n", nom.c_str());
        printf("         curva elegida: %s (keyframes %d..%d)\n", hayRango?"si":"NO", hayRango?mn:0, hayRango?mx:0);
        printf("         vista: viewStartF %.1f -> %.1f | pxPerFrame %.2f -> %.2f -> %s\n",
               vs0, tl->viewStartF, px0, tl->pxPerFrame,
               (tl->viewStartF != vs0 || tl->pxPerFrame != px0) ? "ENCUADRO SOLO OK" : "no encuadro!");
        if (!hayRango){ err="autoframetest: el click no eligio la curva"; delete tl; return false; }
        bool encuadro = (tl->viewStartF != vs0 || tl->pxPerFrame != px0);
        if (!encuadro){ err="autoframetest: no encuadro con Auto frame prendido"; delete tl; return false; }
        // el 1er keyframe tiene que quedar DENTRO del strip (eso es "estar encuadrado")
        float xa = tl->FrameToX((float)mn), xb = tl->FrameToX((float)mx);
        printf("         1er keyframe en x=%.1f, ultimo en x=%.1f (strip=[%d..%d]) -> %s\n",
               xa, xb, tl->panelW, tl->width,
               (xa >= tl->panelW && xb <= tl->width) ? "los dos ENTRAN OK" : "quedaron fuera!");

        // ---- APAGADO: elige la curva pero NO mueve la vista ----
        *estado = false;
        tl->DopeSelectNone();
        tl->viewStartF = -500.0f; tl->pxPerFrame = 1.0f;
        vs0 = tl->viewStartF; px0 = tl->pxPerFrame;
        S::clickFila(tl, fila);
        bool hayRango2 = tl->DopeRangoSeleccion(mn, mx);
        printf("      [autoframetest] click con Auto frame APAGADO: curva elegida=%s | vista quieta=%s -> %s\n",
               hayRango2?"si":"no",
               (tl->viewStartF == vs0 && tl->pxPerFrame == px0) ? "si" : "NO",
               (hayRango2 && tl->viewStartF == vs0 && tl->pxPerFrame == px0) ? "OK" : "MAL");
        if (!(hayRango2 && tl->viewStartF == vs0 && tl->pxPerFrame == px0)){
            err="autoframetest: apagado no se porta bien"; delete tl; return false; }
        *estado = true;   // dejarlo como estaba (default)
        (void)sel;
        delete tl; return true;
    }
    // ---- menuhover : el menu del timeline tiene que portarse como el del viewport 3D: pasar el mouse por una
    //      opcion con submenu lo ABRE, y pasar a otra lo CIERRA. Y las flechas izq/der (Symbian) lo mismo. ----
    if (cmd == "menuhover") {
        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        tl->AbrirMenuKey(100, 100);
        PopupMenu* m = MenuAbierto;
        if (!m){ err="menuhover: el menu Key no abrio"; delete tl; return false; }
        // filas con submenu
        int fTrans = -1, fInterp = -1;
        for (size_t i=0;i<m->items.size();i++){
            if (!m->items[i]->submenu) continue;
            if (fTrans < 0) fTrans = (int)i; else if (fInterp < 0) fInterp = (int)i;
        }
        printf("      [menuhover] menu Key: titulo='%s' | fila con submenu: %d ('%s') y %d ('%s')\n",
               m->titulo.c_str(), fTrans, fTrans>=0?m->items[fTrans]->text.c_str():"",
               fInterp, fInterp>=0?m->items[fInterp]->text.c_str():"");
        if (fTrans < 0 || fInterp < 0){ err="menuhover: no hay 2 submenus"; delete tl; return false; }

        struct H { static void mover(PopupMenu* m, int fila){
            int oy = m->titulo.empty() ? 0 : (RenglonHeightGS + gapGS);
            int my = m->y + borderGS + oy + fila*(RenglonHeightGS + gapGS) + 1;
            m->MouseMove(m->x + m->width/2, my);
        } };

        // ---- HOVER: pasar por Transform lo abre ----
        H::mover(m, fTrans);
        bool abrio1 = (m->submenuAbierto == m->items[fTrans]->submenu && m->submenuAbierto->abierto);
        printf("      [menuhover] mouse sobre '%s' -> su submenu: %s\n",
               m->items[fTrans]->text.c_str(), abrio1 ? "ABIERTO OK" : "cerrado (no hay hover)");
        // ---- ...y pasar a la otra CIERRA la primera y abre la segunda ----
        H::mover(m, fInterp);
        bool abrio2 = (m->submenuAbierto == m->items[fInterp]->submenu && m->submenuAbierto->abierto);
        bool cerro1 = !m->items[fTrans]->submenu->abierto;
        printf("      [menuhover] mouse sobre '%s' -> su submenu: %s | el anterior: %s\n",
               m->items[fInterp]->text.c_str(), abrio2 ? "ABIERTO OK" : "cerrado",
               cerro1 ? "CERRADO OK" : "quedo abierto!");

        // ---- FLECHAS (Symbian): derecha entra al submenu, izquierda vuelve ----
        m->selectIndex = fTrans;
        m->SincronizarSubmenu();
        bool derecha = m->AbrirSubmenuActual();
        printf("      [menuhover] flecha DERECHA sobre '%s' -> entro al submenu: %s\n",
               m->items[fTrans]->text.c_str(), derecha ? "si OK" : "NO");
        // (la flecha IZQUIERDA la rutea LayoutTeclaUI, que cierra el submenu abierto; aca alcanza con comprobar
        //  que la DERECHA entra, que es la mitad que puede faltar)

        m->Cerrar();

        // ---- ACEPTAR / CANCELAR desde AFUERA (click der = cancelar, click izq / OK = aceptar) ----
        Timeline::DopeRow* canal = NULL;
        for (size_t i=0;i<tl->dopeRows.size();i++) if (tl->dopeRows[i].propId>=0 && tl->dopeRows[i].keys.size()>=2){ canal=&tl->dopeRows[i]; break; }
        if (canal){
            AnimProperty* ap = tl->CurvaDeFila(*canal);
            std::vector<keyFrame> base = ap->keyframes;
            int f0 = ap->keyframes[0].frame;
            tl->DopeSelectAll();
            tl->lastMx = tl->x + 500; tl->lastMy = tl->y + 150;
            tl->DopeMoveStart(Timeline::DOPE_MOV);
            printf("      [menuhover] transform de keyframes activo: %s (lo ve DopeXformActivo: %s)\n",
                   tl->DopeMoviendo()?"si":"no", DopeXformActivo()?"si OK":"NO");
            for (const char* q="7"; *q; ++q) tl->DopeNumChar(*q);
            DopeXformCancelar();     // <- lo que hace el click DERECHO (y el backspace de Symbian)
            printf("      [menuhover] CANCELAR desde afuera: activo=%s | 1er frame %d -> %d -> %s\n",
                   DopeXformActivo()?"si":"no", f0, ap->keyframes[0].frame,
                   (!DopeXformActivo() && ap->keyframes[0].frame==f0) ? "volvio atras OK" : "MAL");
            if (DopeXformActivo() || ap->keyframes[0].frame != f0){
                err="menuhover: cancelar desde afuera no anda"; delete tl; return false; }
            // ACEPTAR
            tl->DopeSelectAll();
            tl->DopeMoveStart(Timeline::DOPE_MOV);
            for (const char* q="7"; *q; ++q) tl->DopeNumChar(*q);
            DopeXformAceptar();      // <- lo que hace el click IZQUIERDO (y el OK de Symbian)
            printf("      [menuhover] ACEPTAR desde afuera: activo=%s | 1er frame %d -> %d -> %s\n",
                   DopeXformActivo()?"si":"no", f0, ap->keyframes[0].frame,
                   (!DopeXformActivo() && ap->keyframes[0].frame==f0+7) ? "quedo movido OK" : "MAL");
            if (DopeXformActivo() || ap->keyframes[0].frame != f0+7){
                err="menuhover: aceptar desde afuera no anda"; delete tl; return false; }
            ap->keyframes = base;
        }

        bool ok = abrio1 && abrio2 && cerro1;
        delete tl;
        if (!ok){ err="menuhover: el hover de submenus no anda en el timeline"; return false; }
        return true;
    }
    // ---- smarteuler : el ejemplo de Dante. 3 keyframes de rotacion: frame 10 = 0 grados, frame 20 = 405,
    //      frame 30 = 90. 405 es la MISMA rotacion que 45 (405 = 45 + 360), pero la curva interpola por el VALOR:
    //      con 405 el objeto da una vuelta entera de mas y vuelve. Smart Euler tiene que dejar 0 -> 45 -> 90.
    //      Lo CRITICO: la pose de cada keyframe NO puede cambiar (mod 360 tiene que dar lo mismo). ----
    if (cmd == "smarteuler") {
        Object* o = NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !o){ Object* n2=st.back(); st.pop_back();
            if (n2->getType()==ObjectType::mesh) o=n2;
            for(size_t i=0;i<n2->Childrens.size();i++) st.push_back(n2->Childrens[i]); } }
        if (!o){ err="smarteuler: sin malla"; return false; }
        DeseleccionarTodo(); ObjActivo=o; o->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(o);
        EliminarAnimaciones(*o);

        // el caso de Dante, en la curva Z Euler Rotation. (AnimObjDe es static en ObjectMode.cpp: aca se busca
        //  o se crea el AnimationObject a mano, que es lo mismo que hace.)
        AnimationObject* pao = NULL;
        for (size_t i=0;i<AnimationObjects.size();i++) if (AnimationObjects[i].obj==o){ pao=&AnimationObjects[i]; break; }
        if (!pao){ AnimationObject nuevo; nuevo.obj=o; nuevo.FirstKeyFrame=0; nuevo.LastKeyFrame=0;
                   AnimationObjects.push_back(nuevo); pao=&AnimationObjects[AnimationObjects.size()-1]; }
        AnimationObject& ao = *pao;
        AnimProperty& ap = PropertyDeLista(ao.Propertys, AnimRotation, AnimZ);
        SetKeyCurva(ap, 10,   0.0f);
        SetKeyCurva(ap, 20, 405.0f);
        SetKeyCurva(ap, 30,  90.0f);

        struct S { static void dump(AnimProperty& a, const char* t){
            printf("         %-8s ->", t);
            for (size_t i=0;i<a.keyframes.size();i++) printf("  f%d=%.1f", a.keyframes[i].frame, a.keyframes[i].value);
            printf("\n"); } };
        S::dump(ap, "antes");

        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        tl->DopeSelectAll();
        // el valor de la curva ANTES, muestreado fino: la POSE (mod 360) no puede cambiar en ningun frame
        std::vector<float> antes;
        for (int f=10; f<=30; f++) antes.push_back(ap.EvalF((float)f, 0.0f));

        int n = tl->SmartEulerSel();
        S::dump(ap, "despues");
        printf("      [smarteuler] corrigio %d keyframe(s)\n", n);

        bool ok = (fabsf(ap.keyframes[0].value - 0.0f) < 1e-3f &&
                   fabsf(ap.keyframes[1].value - 45.0f) < 1e-3f &&
                   fabsf(ap.keyframes[2].value - 90.0f) < 1e-3f);
        printf("      [smarteuler] esperado 0 -> 45 -> 90 : %s\n", ok ? "OK" : "MAL");

        // LA POSE de cada keyframe NO cambio (mod 360 da lo mismo)
        float peor = 0.0f;
        const float orig[3] = { 0.0f, 405.0f, 90.0f };
        for (int i=0;i<3;i++){
            float d = ap.keyframes[i].value - orig[i];
            float vueltas = floorf(d/360.0f + 0.5f);
            float resto = fabsf(d - vueltas*360.0f);
            if (resto > peor) peor = resto;
        }
        printf("      [smarteuler] la POSE de los keyframes no cambio (todo es multiplo exacto de 360): peor=%.6f -> %s\n",
               peor, (peor < 1e-3f) ? "OK" : "CAMBIO LA POSE!");

        // y el recorrido: antes daba una vuelta de mas
        float recorridoAntes = 0.0f, recorridoDespues = 0.0f;
        for (size_t i=1;i<antes.size();i++) recorridoAntes += fabsf(antes[i]-antes[i-1]);
        std::vector<float> desp;
        for (int f=10; f<=30; f++) desp.push_back(ap.EvalF((float)f, 0.0f));
        for (size_t i=1;i<desp.size();i++) recorridoDespues += fabsf(desp[i]-desp[i-1]);
        printf("      [smarteuler] grados recorridos por la curva: %.0f -> %.0f (se ahorro %.0f = %.2f vueltas) -> %s\n",
               recorridoAntes, recorridoDespues, recorridoAntes-recorridoDespues,
               (recorridoAntes-recorridoDespues)/360.0f,
               (recorridoDespues < recorridoAntes) ? "gira derecho OK" : "no mejoro!");

        // ---- una curva que YA esta bien no se toca ----
        AnimProperty& ap2 = PropertyDeLista(ao.Propertys, AnimRotation, AnimX);
        SetKeyCurva(ap2, 10, 0.0f); SetKeyCurva(ap2, 20, 45.0f); SetKeyCurva(ap2, 30, 90.0f);
        tl->ConstruirDopeRows(); tl->DopeSelectAll();
        int n2 = tl->SmartEulerSel();
        printf("      [smarteuler] sobre una curva que ya esta bien (0/45/90): corrigio %d -> %s\n",
               n2, (n2==0) ? "no la toca OK" : "la toco de gede!");

        // ---- POSITION no se toca: ahi +360 NO es lo mismo ----
        AnimProperty& ap3 = PropertyDeLista(ao.Propertys, AnimPosition, AnimX);
        SetKeyCurva(ap3, 10, 0.0f); SetKeyCurva(ap3, 20, 405.0f); SetKeyCurva(ap3, 30, 90.0f);
        tl->ConstruirDopeRows(); tl->DopeSelectAll();
        tl->SmartEulerSel();
        bool posIntacta = (fabsf(ap3.keyframes[1].value - 405.0f) < 1e-3f);
        printf("      [smarteuler] X Location con 405: quedo en %.1f -> %s\n",
               ap3.keyframes[1].value, posIntacta ? "NO se toca OK (en position 405 != 45)" : "LA ROMPIO!");

        delete tl;
        if (!ok || peor > 1e-3f || n2 != 0 || !posIntacta){ err="smarteuler: no se porta como corresponde"; return false; }
        return true;
    }
    // ---- trailtest : MOTION TRAIL. Solo POSICION. Con keyframes en el frame 1 y el 20, el trail tiene que tener
    //      UN punto por frame (20) pero solo DOS keyframes marcados: contando los tramos claro/oscuro se sabe
    //      cuantos frames hay entre uno y otro. ----
    if (cmd == "trailtest") {
        Object* o = NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !o){ Object* n2=st.back(); st.pop_back();
            if (n2->getType()==ObjectType::mesh) o=n2;
            for(size_t i=0;i<n2->Childrens.size();i++) st.push_back(n2->Childrens[i]); } }
        if (!o){ err="trailtest: sin malla"; return false; }
        DeseleccionarTodo(); ObjActivo=o; o->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(o);
        EliminarAnimaciones(*o);

        std::vector<Vector3> pts; std::vector<int> keys; int desde, hasta;
        // ---- sin animacion: no hay trail ----
        printf("      [trailtest] objeto SIN animacion -> trail: %s\n",
               MotionTrailDe(o, pts, keys, desde, hasta) ? "hay (MAL)" : "no hay OK");
        if (MotionTrailDe(o, pts, keys, desde, hasta)){ err="trailtest: dio trail sin animacion"; return false; }

        // ---- el caso de Dante: keyframes en el frame 1 y el 20 ----
        CurrentFrame = 1;  o->pos = Vector3(0,0,0);   InsertarKeyframeObjeto();
        CurrentFrame = 20; o->pos = Vector3(19,0,0);  InsertarKeyframeObjeto();
        if (!MotionTrailDe(o, pts, keys, desde, hasta)){ err="trailtest: no dio trail"; return false; }
        printf("      [trailtest] keyframes en 1 y 20 -> rango [%d..%d] | puntos del camino=%d | keyframes marcados=%d\n",
               desde, hasta, (int)pts.size(), (int)keys.size());
        bool okPts = ((int)pts.size() == 20);   // un punto por FRAME (1..20)
        bool okKf  = ((int)keys.size() == 2);   // pero SOLO 2 keyframes
        printf("         un punto por frame (20): %s | solo 2 keyframes marcados: %s\n",
               okPts?"OK":"MAL", okKf?"OK":"MAL");
        // los tramos: 19 (uno por par de frames) -> alternando claro/oscuro se cuentan los frames
        printf("         tramos de la linea = %d (alternan claro/oscuro: contarlos dice cuantos frames hay)\n",
               (int)pts.size()-1);
        // la curva es LINEAL entre 0 y 19 -> el punto del frame 10 tiene que estar en x=9
        printf("         frame 1 en x=%.2f | frame 10 en x=%.2f (esperado 9) | frame 20 en x=%.2f\n",
               pts[0].x, pts[9].x, pts[19].x);
        bool okInterp = (fabsf(pts[0].x - 0.0f) < 1e-3f && fabsf(pts[9].x - 9.0f) < 1e-3f &&
                         fabsf(pts[19].x - 19.0f) < 1e-3f);
        printf("         el camino sigue la curva: %s\n", okInterp?"OK":"MAL");

        // ---- SOLO POSICION: rotar/escalar no genera trail ----
        EliminarAnimaciones(*o);
        AnimationObject* pao = NULL;
        for (size_t i=0;i<AnimationObjects.size();i++) if (AnimationObjects[i].obj==o){ pao=&AnimationObjects[i]; break; }
        if (!pao){ AnimationObject nv; nv.obj=o; nv.FirstKeyFrame=0; nv.LastKeyFrame=0;
                   AnimationObjects.push_back(nv); pao=&AnimationObjects[AnimationObjects.size()-1]; }
        SetKeyCurva(PropertyDeLista(pao->Propertys, AnimRotation, AnimZ), 1, 0.0f);
        SetKeyCurva(PropertyDeLista(pao->Propertys, AnimRotation, AnimZ), 20, 90.0f);
        bool soloRot = MotionTrailDe(o, pts, keys, desde, hasta);
        printf("      [trailtest] objeto con SOLO rotacion animada -> trail: %s\n",
               soloRot ? "hay (MAL: el trail es de POSICION)" : "no hay OK");

        if (!okPts || !okKf || !okInterp || soloRot){ err="trailtest: el trail no se porta bien"; return false; }
        return true;
    }
    // ---- animmenu : el menu "Animation" de la barra del viewport 3D tiene que ABRIRSE al clickear su boton.
    //      Se va por el camino REAL (LayoutAbrirMenuDeBarra con las coords del boton), que es el que estaba roto:
    //      la rama del menu habia quedado ANIDADA adentro del Contains de "Object" -> pedia el cursor sobre los
    //      dos botones a la vez y no abria nunca. ----
    if (cmd == "animmenu") {
        Object* o = NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !o){ Object* n2=st.back(); st.pop_back();
            if (n2->getType()==ObjectType::mesh) o=n2;
            for(size_t i=0;i<n2->Childrens.size();i++) st.push_back(n2->Childrens[i]); } }
        if (!o){ err="animmenu: sin malla"; return false; }
        // El viewport se crea ACA: en headless no hay ninguno. Render() es GL puro, asi que no se llama; lo que se
        // testea es el RUTEO del click (que es lo que estaba roto: la rama del menu habia quedado anidada adentro
        // del Contains de "Object"). El boton se posiciona a mano, como lo hace la barra al dibujarse.
        Viewport3D* vp = new Viewport3D();
        vp->Resize(900, 600);
        InteractionMode = ObjectMode;
        ObjActivo=o; o->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(o);
        Button* bAnim = BarRolBtn(vp->BarButtons, BR_Animation);
        if (!bAnim){ err="animmenu: no existe el boton Animation en la barra"; delete vp; return false; }
        printf("      [animmenu] el boton 'Animation' existe en la barra (rol=%d)\n", bAnim->rol);
        bAnim->visible = true; bAnim->sx = 200; bAnim->sy = 10; bAnim->width = 80; bAnim->height = 20;
        bool okSin = true;

        if (MenuAbierto) MenuAbierto->Cerrar();
        // el camino REAL: click en el CENTRO del boton
        bool abrio = LayoutAbrirMenuDeBarra(vp, bAnim->sx + bAnim->width/2, bAnim->sy + bAnim->height/2);
        printf("      [animmenu] click en el boton -> LayoutAbrirMenuDeBarra=%s | MenuAbierto=%s\n",
               abrio?"true":"false",
               (MenuAbierto == MenuAnimation) ? "el menu Animation OK" : (MenuAbierto ? "OTRO menu!" : "NINGUNO"));
        bool okAbre = (abrio && MenuAbierto == MenuAnimation);
        if (okAbre){
            printf("      [animmenu] items:\n");
            for (size_t i=0;i<MenuAnimation->items.size();i++)
                printf("         %-16s %s\n", MenuAnimation->items[i]->text.c_str(),
                       MenuAnimation->items[i]->checkbox ? (*MenuAnimation->items[i]->checkbox?"[x]":"[ ]") : "");
        }
        if (MenuAbierto) MenuAbierto->Cerrar();

        // ---- y el boton "Object" sigue abriendo SU menu (el nuevo no lo tapo) ----
        Button* bObj = BarRolBtn(vp->BarButtons, BR_Object);
        bool okObj = false;
        if (bObj){
            bObj->visible = true; bObj->sx = 300; bObj->sy = 10; bObj->width = 60; bObj->height = 20;
            LayoutAbrirMenuDeBarra(vp, bObj->sx + bObj->width/2, bObj->sy + bObj->height/2);
            okObj = (MenuAbierto == MenuObject);
            printf("      [animmenu] el boton 'Object' sigue abriendo SU menu: %s\n", okObj ? "OK" : "SE ROMPIO!");
            if (MenuAbierto) MenuAbierto->Cerrar();
        }
        delete vp;
        if (!okSin || !okAbre || !okObj){ err="animmenu: el menu Animation no se porta bien"; return false; }
        return true;
    }
    // ---- trailparent : el motion trail de un objeto EMPARENTADO a un padre ANIMADO. El trail es el camino en
    //      MUNDO, asi que en cada frame hay que usar el world del padre EN ESE FRAME. Antes se usaba su world
    //      ACTUAL -> el camino del hijo salia pegado a donde el padre esta AHORA, no a donde estaba. ----
    if (cmd == "trailparent") {
        // dos mallas: la 1ra es el padre, la 2da el hijo
        std::vector<Object*> ms;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* n2=st.back(); st.pop_back();
            if (n2->getType()==ObjectType::mesh) ms.push_back(n2);
            for(size_t i=0;i<n2->Childrens.size();i++) st.push_back(n2->Childrens[i]); } }
        if (ms.size() < 2){ err="trailparent: hacen falta 2 mallas"; return false; }
        Object* padre = ms[0]; Object* hijo = ms[1];
        EliminarAnimaciones(*padre); EliminarAnimaciones(*hijo);

        // emparentar de verdad
        if (hijo->Parent){ std::vector<Object*>& hs = hijo->Parent->Childrens;
            for (size_t i=0;i<hs.size();i++) if (hs[i]==hijo){ hs.erase(hs.begin()+i); break; } }
        hijo->Parent = padre; padre->Childrens.push_back(hijo);
        padre->pos = Vector3(0,0,0); padre->rot = Quaternion(1,0,0,0); padre->scale = Vector3(1,1,1);
        hijo->rot = Quaternion(1,0,0,0); hijo->scale = Vector3(1,1,1);

        struct S { static void key(Object* o, int f, Vector3 p){
            DeseleccionarTodo(); ObjActivo=o; o->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(o);
            CurrentFrame=f; o->pos=p; InsertarKeyframeObjeto(); } };

        // el PADRE se mueve en X: 0 -> 100 (frames 1..10). El HIJO queda quieto en su local (5,0,0).
        S::key(padre, 1,  Vector3(0,0,0));
        S::key(padre, 10, Vector3(100,0,0));
        S::key(hijo,  1,  Vector3(5,0,0));
        S::key(hijo,  10, Vector3(5,0,0));

        DeseleccionarTodo(); ObjActivo=hijo; hijo->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(hijo);
        std::vector<Vector3> pts; std::vector<int> keys; int desde, hasta;
        if (!MotionTrailDe(hijo, pts, keys, desde, hasta)){ err="trailparent: sin trail"; return false; }
        printf("      [trailparent] padre animado 0->100 en X | hijo quieto en su local (5,0,0)\n");
        printf("         trail del hijo (MUNDO): f1 x=%.2f | f5 x=%.2f | f10 x=%.2f\n",
               pts[0].x, pts[4].x, pts[9].x);
        // el hijo en MUNDO tiene que ARRASTRARSE con el padre: 5 -> 105
        bool ok = (fabsf(pts[0].x - 5.0f) < 1e-2f && fabsf(pts[9].x - 105.0f) < 1e-2f);
        printf("         esperado f1=5 y f10=105 (el hijo se mueve CON el padre) -> %s\n",
               ok ? "OK" : "MAL (esta usando el world ACTUAL del padre, no el del frame)");
        // ...y el del medio tiene que seguir la curva del padre, no quedarse quieto
        printf("         f5 = %.2f (esperado ~49.4: la curva del padre en el frame 5, + 5) -> %s\n",
               pts[4].x, (pts[4].x > 5.5f && pts[4].x < 104.5f) ? "sigue al padre OK" : "NO lo sigue!");
        bool okMedio = (pts[4].x > 5.5f && pts[4].x < 104.5f);

        // limpiar: desemparentar
        for (size_t i=0;i<padre->Childrens.size();i++) if (padre->Childrens[i]==hijo){ padre->Childrens.erase(padre->Childrens.begin()+i); break; }
        hijo->Parent = NULL;
        if (!ok || !okMedio){ err="trailparent: el trail no respeta al padre animado"; return false; }
        return true;
    }
    // ---- trailhueso : MOTION TRAIL de un HUESO (Pose Mode). Reusa EvaluarPoseEsqueleto, que PISA la pose viva:
    //      lo critico es que al salir el esqueleto quede EXACTAMENTE como estaba. ----
    if (cmd == "trailhueso") {
        Armature* a = NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !a){ Object* n2=st.back(); st.pop_back();
            if (n2->getType()==ObjectType::armature) a=(Armature*)n2;
            for(size_t i=0;i<n2->Childrens.size();i++) st.push_back(n2->Childrens[i]); } }
        if (!a){ err="trailhueso: sin armature"; return false; }
        if (a->animActiva < 0){ err="trailhueso: sin clip activo (corre skinbbox antes)"; return false; }

        // un hueso que tenga curvas de posicion
        int bone = -1;
        SkeletalAnimation* clip = a->animations[a->animActiva];
        for (size_t t=0;t<clip->tracks.size() && bone<0;t++)
            for (size_t p2=0;p2<clip->tracks[t].Propertys.size();p2++)
                if (clip->tracks[t].Propertys[p2].Property==AnimPosition && !clip->tracks[t].Propertys[p2].keyframes.empty()){
                    bone = clip->tracks[t].bone; break; }
        if (bone < 0){ err="trailhueso: ningun hueso con curvas de posicion"; return false; }

        // ---- la pose VIVA antes: se posa un hueso a mano para que haya algo que perder ----
        CurrentFrame = 5;
        EvaluarPoseEsqueleto(a, 5);
        a->bones[bone].poseR = Vector3(11.0f, 22.0f, 33.0f);   // pose "a mano"
        a->poseDirty = true;
        std::vector<Vector3> T0(a->bones.size()), R0(a->bones.size()), S0(a->bones.size());
        for (size_t i=0;i<a->bones.size();i++){ T0[i]=a->bones[i].poseT; R0[i]=a->bones[i].poseR; S0[i]=a->bones[i].poseS; }
        int lpf0 = a->lastPoseFrame;

        std::vector<Vector3> pts; std::vector<int> keys; int desde, hasta;
        unsigned int t0 = w3dGetTicks();
        bool hay = MotionTrailHuesoNodo(a, bone, pts, keys, desde, hasta);
        unsigned int t1 = w3dGetTicks();
        printf("      [trailhueso] hueso %d ('%s') -> trail: %s | rango [%d..%d] | puntos=%d | keyframes=%d | %ums\n",
               bone, a->bones[bone].name.c_str(), hay?"si":"no", desde, hasta, (int)pts.size(), (int)keys.size(), t1-t0);
        if (!hay){ err="trailhueso: no dio trail"; return false; }
        bool okPts = ((int)pts.size() == hasta-desde+1);
        printf("         un punto por frame (%d): %s\n", hasta-desde+1, okPts?"OK":"MAL");
        // el camino tiene que MOVERSE (si diera todo el mismo punto, el FK no se estaria re-evaluando)
        float dmax = 0.0f;
        for (size_t i=1;i<pts.size();i++){ float d=(pts[i]-pts[0]).Length(); if (d>dmax) dmax=d; }
        printf("         el hueso RECORRE %.3f unidades -> %s\n", dmax,
               (dmax > 1e-4f) ? "el camino existe OK" : "todos los puntos iguales (no se evaluo)!");

        // ---- LO CRITICO: la pose viva quedo INTACTA ----
        float peor = 0.0f;
        for (size_t i=0;i<a->bones.size();i++){
            float d;
            d = (a->bones[i].poseT - T0[i]).Length(); if (d>peor) peor=d;
            d = (a->bones[i].poseR - R0[i]).Length(); if (d>peor) peor=d;
            d = (a->bones[i].poseS - S0[i]).Length(); if (d>peor) peor=d;
        }
        printf("         la POSE VIVA quedo intacta: peor diferencia=%.6f | lastPoseFrame %d -> %d -> %s\n",
               peor, lpf0, a->lastPoseFrame,
               (peor < 1e-4f && a->lastPoseFrame == lpf0) ? "OK (no pisa lo que estas editando)" : "LA PISO!");
        bool okPose = (peor < 1e-4f && a->lastPoseFrame == lpf0);
        if (!okPts || dmax <= 1e-4f || !okPose){ err="trailhueso: no se porta bien"; return false; }
        return true;
    }
    // ---- simpltest : borrar un keyframe de un BEZIER manteniendo la forma. Se mide el ERROR contra la curva
    //      ORIGINAL: el borrado que ajusta los handles tiene que quedar MUCHO mas cerca que el borrado crudo. ----
    if (cmd == "simpltest") {
        struct S {
            // curva bezier en S: 0 -> 5 -> 10 con handles automaticos
            static void armar(AnimProperty& a){
                a.keyframes.clear();
                SetKeyCurva(a, 0,  0.0f); SetKeyCurva(a, 10, 8.0f); SetKeyCurva(a, 20, 10.0f);
                for (size_t i=0;i<a.keyframes.size();i++){ a.keyframes[i].Interpolation = KfBezier;
                                                           a.keyframes[i].handleType = HAuto; }
            }
            static float error(AnimProperty& a, const std::vector<float>& orig, int f0, int f1){
                float peor = 0.0f; size_t k = 0;
                for (int f=f0; f<=f1; f++, k++){ float d = fabsf(a.EvalF((float)f,0) - orig[k]); if (d>peor) peor=d; }
                return peor;
            }
        };
        AnimProperty ap; ap.Property = AnimPosition; ap.component = AnimX;
        S::armar(ap);
        // la curva ORIGINAL, frame a frame
        std::vector<float> orig;
        for (int f=0; f<=20; f++) orig.push_back(ap.EvalF((float)f, 0.0f));
        printf("      [simpltest] bezier 0/10/20 con valores 0 / 8 / 10\n");
        printf("         original f5=%.3f f10=%.3f f15=%.3f\n", orig[5], orig[10], orig[15]);

        // ---- borrado CRUDO (el de antes): erase y listo ----
        AnimProperty crudo = ap;
        for (size_t i=0;i<crudo.keyframes.size();i++) if (crudo.keyframes[i].frame==10){ crudo.keyframes.erase(crudo.keyframes.begin()+i); break; }
        float eCrudo = S::error(crudo, orig, 0, 20);
        printf("         borrado CRUDO      -> f5=%.3f f15=%.3f | error maximo=%.4f\n",
               crudo.EvalF(5,0), crudo.EvalF(15,0), eCrudo);

        // ---- borrado que MANTIENE LA FORMA ----
        BorrarKeyframeManteniendoForma(ap, 10);
        float eFit = S::error(ap, orig, 0, 20);
        printf("         MANTENIENDO FORMA  -> f5=%.3f f15=%.3f | error maximo=%.4f\n",
               ap.EvalF(5,0), ap.EvalF(15,0), eFit);
        printf("         -> %.1fx mas parecido a la original -> %s\n",
               (eFit > 1e-6f) ? eCrudo/eFit : 999.0f, (eFit < eCrudo) ? "OK" : "NO MEJORO!");
        // los EXTREMOS no se tocan nunca
        bool okExt = (fabsf(ap.EvalF(0,0) - 0.0f) < 1e-3f && fabsf(ap.EvalF(20,0) - 10.0f) < 1e-3f);
        printf("         los extremos quedan intactos (0 y 10): %s\n", okExt?"OK":"MAL");
        printf("         keyframes: %d (el del medio se fue)\n", (int)ap.keyframes.size());
        // se exige una mejora REAL, no "por un pelo": si el ajuste no hace nada, el error queda igual al crudo
        if (eFit > eCrudo*0.7f || !okExt || ap.keyframes.size()!=2){ err="simpltest: la simplificacion no mantiene la forma"; return false; }

        // ---- un tramo LINEAL no se toca: borrar deja la recta (no hay forma que mantener) ----
        AnimProperty li; li.Property = AnimPosition; li.component = AnimX;
        SetKeyCurva(li, 0, 0.0f); SetKeyCurva(li, 10, 8.0f); SetKeyCurva(li, 20, 10.0f);
        BorrarKeyframeManteniendoForma(li, 10);
        printf("      [simpltest] curva LINEAL: keyframes=%d | f10=%.3f (esperado 5 = la recta 0->10) -> %s\n",
               (int)li.keyframes.size(), li.EvalF(10,0),
               (li.keyframes.size()==2 && fabsf(li.EvalF(10,0)-5.0f)<1e-3f) ? "OK (borrado comun)" : "MAL");
        if (li.keyframes.size()!=2 || fabsf(li.EvalF(10,0)-5.0f)>1e-3f){ err="simpltest: el lineal no se porta bien"; return false; }
        return true;
    }
    // ---- afcurva : el AUTO FRAME tiene que encuadrar LA CURVA, no solo los keyframes. Una bezier se PASA de los
    //      valores de sus keyframes (sobrepico), asi que midiendo solo los keyframes la curva sale RECORTADA. ----
    if (cmd == "afcurva") {
        Object* o = NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !o){ Object* n2=st.back(); st.pop_back();
            if (n2->getType()==ObjectType::mesh) o=n2;
            for(size_t i=0;i<n2->Childrens.size();i++) st.push_back(n2->Childrens[i]); } }
        if (!o){ err="afcurva: sin malla"; return false; }
        DeseleccionarTodo(); ObjActivo=o; o->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(o);
        EliminarAnimaciones(*o);

        AnimationObject* pao = NULL;
        for (size_t i=0;i<AnimationObjects.size();i++) if (AnimationObjects[i].obj==o){ pao=&AnimationObjects[i]; break; }
        if (!pao){ AnimationObject nv; nv.obj=o; nv.FirstKeyFrame=0; nv.LastKeyFrame=0;
                   AnimationObjects.push_back(nv); pao=&AnimationObjects[AnimationObjects.size()-1]; }
        // curva con SOBREPICO: keyframes en 0 y 10 los dos en valor 0, pero con handles enormes hacia arriba ->
        // la curva se dispara MUY por encima de 0 en el medio. Encuadrar por los keyframes daria un rango de
        // altura CERO y no se veria nada de la panza.
        AnimProperty& ap = PropertyDeLista(pao->Propertys, AnimPosition, AnimX);
        SetKeyCurva(ap, 0, 0.0f); SetKeyCurva(ap, 10, 0.0f);
        ap.keyframes[0].Interpolation = KfBezier; ap.keyframes[0].handleType = HFree;
        ap.keyframes[0].outDF = 3.0f; ap.keyframes[0].outDV = 40.0f;
        ap.keyframes[1].Interpolation = KfBezier; ap.keyframes[1].handleType = HFree;
        ap.keyframes[1].inDF = -3.0f; ap.keyframes[1].inDV = 40.0f;

        float pico = -1e30f, valle = 1e30f;
        for (int f=0; f<=10; f++){ float v = ap.EvalF((float)f,0); if (v>pico) pico=v; if (v<valle) valle=v; }
        printf("      [afcurva] keyframes: f0=0.0 f10=0.0 | pero LA CURVA llega a %.3f (sobrepico)\n", pico);

        Timeline* tl = new Timeline(); tl->Resize(900, 300);
        tl->modo = Timeline::TL_MODO_CURVAS;
        tl->ConstruirDopeRows();
        tl->DopeSelectAll();
        tl->CurvaFrameSelected();

        // donde cae el PICO de la curva en pantalla? Tiene que ENTRAR en el strip.
        float yPico  = tl->ValueToY(pico);
        float yValle = tl->ValueToY(valle);
        printf("      [afcurva] tras Auto frame: pico en y=%.1f | valle en y=%.1f | strip=[%d..%d]\n",
               yPico, yValle, tl->stripY, tl->height);
        bool entra = (yPico >= (float)tl->stripY && yPico <= (float)tl->height &&
                      yValle >= (float)tl->stripY && yValle <= (float)tl->height);
        printf("         la curva ENTERA entra en el strip: %s\n",
               entra ? "OK (encuadra la curva)" : "NO (sale recortada: esta midiendo solo los keyframes)");
        // ...y que no quede ridiculamente chica (que use el alto): el pico y el valle tienen que estar separados
        float usa = fabsf(yValle - yPico) / (float)(tl->height - tl->stripY);
        printf("         usa el %.0f%% del alto del strip\n", usa*100.0f);
        delete tl;
        if (!entra){ err="afcurva: el auto frame recorta la curva"; return false; }
        return true;
    }
    // ---- rot360 : se puede animar una vuelta ENTERA? El transform guarda la rotacion como QUATERNION, y el euler
    //      se DERIVA de el (ToEulerXYZ usa asin/atan2 -> rango canonico). Un giro de 360 vuelve al mismo
    //      quaternion, asi que el euler derivado vuelve a 0 y la vuelta se PIERDE. ----
    if (cmd == "rot360") {
        Object* o = NULL;
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !o){ Object* n2=st.back(); st.pop_back();
            if (n2->getType()==ObjectType::mesh) o=n2;
            for(size_t i=0;i<n2->Childrens.size();i++) st.push_back(n2->Childrens[i]); } }
        if (!o){ err="rot360: sin malla"; return false; }
        DeseleccionarTodo(); ObjActivo=o; o->Seleccionar(); ObjSelects.clear(); ObjSelects.push_back(o);
        EliminarAnimaciones(*o);
        o->rot = Quaternion(1,0,0,0); o->ActualizarDisplayRot();

        struct S { static float ez(Object* o){ o->ActualizarDisplayRot(); return o->rotEuler.z; } };

        printf("      [rot360] el euler que el editor DERIVA del quaternion:\n");
        const float angs[6] = { 0.0f, 90.0f, 180.0f, 270.0f, 360.0f, 405.0f };
        for (int i=0;i<6;i++){
            o->rot = Quaternion::FromAxisAngle(Vector3(0,0,1), angs[i]);
            printf("         rotado %6.0f grados -> rotEuler.z = %8.2f%s\n", angs[i], S::ez(o),
                   (fabsf(S::ez(o) - angs[i]) > 1.0f) ? "   <-- NO es lo que se pidio" : "");
        }

        // ---- AUTO KEY con una vuelta entera ----
        AutoKeyOn = true;
        EliminarAnimaciones(*o);
        o->rot = Quaternion(1,0,0,0); o->ActualizarDisplayRot();
        CurrentFrame = 1;
        estadoObjetos.clear();
        { SaveState st; st.obj=o; st.pos=o->pos; st.rot=o->rot; st.scale=o->scale; st.worldPos=o->pos;
          estadoObjetos.push_back(st); }
        o->rot = Quaternion::FromAxisAngle(Vector3(0,0,1), 360.0f);   // UNA VUELTA ENTERA
        o->ActualizarDisplayRot();
        int n = AutoKeyObjetos();
        printf("      [rot360] auto key tras rotar 360 grados: guardo %d canal(es) -> %s\n", n,
               (n == 0) ? "NADA (la vuelta se perdio)" : "algo");
        AutoKeyOn = false;
        estadoObjetos.clear();
        printf("      [rot360] -> el quaternion no distingue 0 de 360, y el euler se DERIVA de el.\n");
        printf("               Para animar vueltas hace falta que el euler sea el dato AUTORITATIVO.\n");
        return true;
    }
    // ---- normstats <name> : normales de la malla -> #verts, normales DISTINTAS y posiciones con MAS DE UNA normal
    //      (= verts "split" = bordes FILOSOS). Si un rebuild promedia las normales, los splits desaparecen. ----
    if (cmd == "normstats" || cmd == "normtest") {
        std::string nm; ss>>nm; Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if(!o || o->getType()!=ObjectType::mesh){ err=cmd+": malla no encontrada"; return false; }
        Mesh* m=(Mesh*)o;
        struct S { static void stats(Mesh* m, int& nv, int& distintas, int& splits, double& sum){
            nv = m->vertexSize; distintas=0; splits=0; sum=0;
            if(!m->normals||!m->vertex||m->vertexSize<=0) return;
            std::set<std::string> setN; std::map<std::string, std::set<std::string> > porPos;
            for(int i=0;i<m->vertexSize;i++){
                char nb[3]={(char)m->normals[i*3],(char)m->normals[i*3+1],(char)m->normals[i*3+2]};
                std::string kn(nb,3); setN.insert(kn);
                char pb[12]; memcpy(pb,&m->vertex[i*3],4); memcpy(pb+4,&m->vertex[i*3+1],4); memcpy(pb+8,&m->vertex[i*3+2],4);
                porPos[std::string(pb,12)].insert(kn);
                sum += (double)m->normals[i*3]*3 + (double)m->normals[i*3+1]*5 + (double)m->normals[i*3+2]*7; }
            distintas=(int)setN.size();
            for(std::map<std::string,std::set<std::string> >::iterator it=porPos.begin();it!=porPos.end();++it) if(it->second.size()>1) splits++;
        } };
        int nv0,d0,s0; double c0; S::stats(m,nv0,d0,s0,c0);
        printf("      [normstats] '%s' verts=%d normales_distintas=%d posiciones_con_split(bordes filosos)=%d checksum=%.0f\n", nm.c_str(), nv0,d0,s0,c0);
        if (cmd == "normtest"){
            m->GenerarRender(false); int nv1,d1,s1; double c1; S::stats(m,nv1,d1,s1,c1);
            printf("      [normtest] tras GenerarRender(FALSE=preservar): verts=%d distintas=%d splits=%d checksum=%.0f -> %s\n",
                nv1,d1,s1,c1, (s1==s0 && d1==d0)?"OK (normales PRESERVADAS)":"CAMBIARON!");
            m->GenerarRender(true);  int nv2,d2,s2; double c2; S::stats(m,nv2,d2,s2,c2);
            printf("      [normtest] tras GenerarRender(TRUE=recomputar):  verts=%d distintas=%d splits=%d checksum=%.0f -> %s\n",
                nv2,d2,s2,c2, (s2!=s0||d2!=d0)?"recomputo (los splits/normales cambian = lo que rompia el join)":"sin cambio");
        }
        return true;
    }
    // ---- matdist <name> : cuenta cuantas caras (faces3d) hay por indice de material -> verifica que reimport preserva
    //      el material POR-CARA (bug: ConvertToES1 dejaba todas en mat=0 -> colapso de meshparts al rebuildear/exportar). ----
    if (cmd == "matdist") {
        std::string nm; ss>>nm; Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if(!o || o->getType()!=ObjectType::mesh){ err="matdist: malla no encontrada"; return false; }
        Mesh* m=(Mesh*)o; std::vector<int> cnt(m->materialsGroup.size()+1, 0); int noCero=0;
        for(size_t f=0;f<m->faces3d.size();f++){ int mm=m->faces3d[f].mat; if(mm<0||mm>=(int)m->materialsGroup.size()) mm=(int)m->materialsGroup.size(); cnt[mm]++; }
        for(size_t g=0;g<m->materialsGroup.size();g++){ if(cnt[g]>0) noCero++; }
        printf("      [matdist] '%s' grupos=%d caras=%d | grupos con caras=%d\n", nm.c_str(), (int)m->materialsGroup.size(), (int)m->faces3d.size(), noCero);
        for(size_t g=0;g<m->materialsGroup.size();g++) printf("         mat[%d] '%s' caras=%d\n", (int)g, m->materialsGroup[g].name.c_str(), cnt[g]);
        return true;
    }
    // ---- fpstest : crea un clip SPARSE (2 keyframes: frame 1 y 20) en el 1er armature, exporta glTF, reimporta y dumpea
    //      los frames del clip reimportado -> verifica que el fps se preserva (bug: adivinaba fps=1 -> frame 20 -> frame 2). ----
    if (cmd == "fpstest") {
        Armature* a=NULL; { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !a){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::armature) a=(Armature*)o; for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(!a){err="fpstest: sin armature";return false;}
        CrearAnimacion(a); int ci=a->animActiva; SkeletalAnimation* clip=a->animations[ci]; clip->name="SparseTest"; clip->FrameRate=24;
        BoneTrack& tr=clip->TrackDe(0);
        // curvas INDEPENDIENTES por componente: X en los frames 1 y 20; Y solo en el 10 -> se prueba que cada canal
        // guarda SUS propios frames (con el modelo viejo X/Y/Z estaban obligados a compartirlos).
        { AnimProperty& ap=tr.PropertyDe(AnimRotation, AnimX); SetKeyCurva(ap, 1, 0.0f); SetKeyCurva(ap, 20, 45.0f); }
        { AnimProperty& ap=tr.PropertyDe(AnimRotation, AnimY); SetKeyCurva(ap, 10, 30.0f); }
        clip->startFrame=1; clip->endFrame=20;
        extern int ActiveAnimKind; extern Armature* ActiveAnimArm; ActiveAnimKind=1; ActiveAnimArm=a; a->animActiva=ci;
        printf("      [fpstest] clip SPARSE creado: 2 keyframes @ frames 1,20 fps=24 rango[1..20]\n");
        std::string path="C:/Users/dante/AppData/Local/Temp/claude/c--Symbian-Carbide-workspace-Whisk3Decosystem/dc888d7b-5375-4a95-a9de-8b709335a3d6/scratchpad/fpstest.gltf";
        extern bool ExportGLTF(const std::string&, bool, bool); extern bool ImportGLTF(const std::string&);
        if(!ExportGLTF(path, false, false)){ err="fpstest: fallo export"; return false; }
        if(!ImportGLTF(path)){ err="fpstest: fallo import"; return false; }
        // buscar el ULTIMO armature con un clip 'SparseTest'
        Armature* re=NULL; SkeletalAnimation* rc=NULL; { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::armature){ Armature* ar=(Armature*)o;
            for(size_t k=0;k<ar->animations.size();k++) if(ar->animations[k]->name.find("SparseTest")!=std::string::npos){ re=ar; rc=ar->animations[k]; } }
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(!rc){ err="fpstest: no se encontro el clip reimportado"; return false; }
        printf("      [fpstest] REIMPORTADO clip '%s' fps=%d rango[%d..%d]\n", rc->name.c_str(), rc->FrameRate, rc->startFrame, rc->endFrame);
        const char* cn[3] = {"X","Y","Z"};
        for(size_t t=0;t<rc->tracks.size();t++) for(size_t p=0;p<rc->tracks[t].Propertys.size();p++){ AnimProperty& ap=rc->tracks[t].Propertys[p];
            if(ap.Property!=AnimRotation) continue; int c=ap.component; if(c<0||c>2)c=0;
            printf("         Rot.%s bone=%d: %d keys @", cn[c], rc->tracks[t].bone, (int)ap.keyframes.size());
            for(size_t k=0;k<ap.keyframes.size();k++) printf(" %d(%.1f)", ap.keyframes[k].frame, ap.keyframes[k].value); printf("\n"); }
        return true;
    }
    // ---- animdensity : dumpea la densidad de KEYFRAMES del clip 0 del 1er armature (para saber si la animacion se guardo
    //      sparse -o horneada frame a frame-). Total de keys, rango, keys/property y densidad (keys / span de frames). ----
    if (cmd == "animdensity") {
        // junta TODOS los armatures y elige el 1ro CON clips (puede haber armatures auxiliares sin animacion)
        std::vector<Armature*> arms; { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::armature) arms.push_back((Armature*)o); for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(arms.empty()){err="animkeys: sin armature";return false;}
        Armature* a=NULL; for(size_t i=0;i<arms.size();i++){ printf("      [animkeys] armature '%s' clips=%d\n", arms[i]->name.c_str(), (int)arms[i]->animations.size()); if(!a && !arms[i]->animations.empty()) a=arms[i]; }
        if(!a){err="animkeys: ningun armature tiene clips";return false;}
        SkeletalAnimation* c=a->animations[0];
        long totalKeys=0; int props=0; int minF=0x7fffffff,maxF=0; int maxKeysProp=0;
        for(size_t t=0;t<c->tracks.size();t++) for(size_t p=0;p<c->tracks[t].Propertys.size();p++){
            int nk=(int)c->tracks[t].Propertys[p].keyframes.size(); totalKeys+=nk; props++;
            if(nk>maxKeysProp)maxKeysProp=nk;
            for(int k=0;k<nk;k++){ int f=c->tracks[t].Propertys[p].keyframes[k].frame; if(f<minF)minF=f; if(f>maxF)maxF=f; } }
        int span = (maxF>=minF)?(maxF-minF+1):0;
        printf("      [animkeys] clip '%s' rango[%d..%d] span=%d frames | huesos animados=%d properties=%d\n",
            c->name.c_str(), c->startFrame, c->endFrame, span, (int)c->tracks.size(), props);
        printf("      [animkeys] TOTAL keyframes=%ld | max keys en una property=%d | densidad=%.2f keys/frame por property (1.0 = horneado frame a frame)\n",
            totalKeys, maxKeysProp, span>0?((float)maxKeysProp/(float)span):0.0f);
        // muestra 1 hueso: sus properties y las primeras frames de cada una
        if(!c->tracks.empty()){ BoneTrack& tr=c->tracks[0]; int bi=tr.bone;
            printf("      [animkeys] ej hueso '%s':\n", (bi>=0&&bi<(int)a->bones.size())?a->bones[bi].name.c_str():"?");
            for(size_t p=0;p<tr.Propertys.size();p++){ AnimProperty& ap=tr.Propertys[p];
                const char* pn=(ap.Property==AnimPosition)?"Pos":(ap.Property==AnimRotation)?"Rot":"Scl";
                printf("         %s: %d keys @ frames", pn, (int)ap.keyframes.size());
                for(size_t k=0;k<ap.keyframes.size() && k<12;k++) printf(" %d", ap.keyframes[k].frame);
                printf("%s\n", ap.keyframes.size()>12?" ...":""); } }
        return true;
    }
    // ---- bonepose <substr> <frame> : activa el clip 0 y dumpea la pose ANIMADA (euler + world head/tail) del 1er hueso
    //      cuyo nombre contiene <substr>. Para comparar la rotacion de un hueso (ej brazo) baseline vs reimportado. ----
    if (cmd == "bonepose") {
        std::string sub; int frame=15; ss >> sub >> frame;
        Armature* a=NULL; { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty() && !a){ Object* o=st.back(); st.pop_back(); if(o->getType()==ObjectType::armature) a=(Armature*)o; for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(!a){err="bonepose: sin armature";return false;}
        int bi=-1; for(size_t b=0;b<a->bones.size();b++) if(a->bones[b].name.find(sub)!=std::string::npos){ bi=(int)b; break; }
        if(bi<0){err="bonepose: hueso '"+sub+"' no encontrado";return false;}
        extern int ActiveAnimKind; extern Armature* ActiveAnimArm;
        if(!a->animations.empty()){ ActiveAnimKind=1; ActiveAnimArm=a; a->animActiva=0; }
        a->poseDirty=false; a->lastPoseFrame=-999999; CurrentFrame=frame; EvaluarPoseEsqueleto(a, frame);
        W3dBone& b=a->bones[bi];
        printf("      [bonepose] '%s' p=%d restT=(%.2f,%.2f,%.2f) restR=(%.1f,%.1f,%.1f) restS=(%.3f,%.3f,%.3f) poseT=(%.2f,%.2f,%.2f) poseR=(%.1f,%.1f,%.1f) head=(%.2f,%.2f,%.2f)\n",
            b.name.c_str(), b.parent, b.restT.x,b.restT.y,b.restT.z, b.restR.x,b.restR.y,b.restR.z, b.restS.x,b.restS.y,b.restS.z,
            b.poseT.x,b.poseT.y,b.poseT.z, b.poseR.x,b.poseR.y,b.poseR.z, b.poseHead.x,b.poseHead.y,b.poseHead.z);
        return true;
    }
    // ---- meshskin : lista cada malla de la escena con su nombre, verts, si tiene skinArmature y cuantos vertex groups. ----
    if (cmd == "meshskin") {
        int n = 0; std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
        while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o;
                printf("      [meshskin] '%s' verts=%d skin=%s grupos=%d mats=%d\n", m->name.c_str(), m->vertexSize,
                    m->skinArmature?"SI":"NO", (int)m->vertexGroups.size(), (int)m->materialsGroup.size()); n++; }
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); }
        printf("      [meshskin] total mallas=%d\n", n);
        return true;
    }
    // ---- loadtex : procesa TODA la cola de texturas pendientes (headless normalmente no corre frames). Deja
    //      mat->texture/textureOn seteados para poder testear el export de texturas sin la GUI. ----
    if (cmd == "loadtex") {
        extern void CargarTexturasPendientes();
        for (int i = 0; i < 100000; i++) CargarTexturasPendientes(); // vacia la cola (idempotente al terminar)
        int conTex = 0; for (size_t i = 0; i < Materials.size(); i++) if (Materials[i]->textureOn && Materials[i]->texture) conTex++;
        printf("      [loadtex] materiales con textura=%d de %d\n", conTex, (int)Materials.size());
        return true;
    }
    // ---- skinbbox [frame] : activa el clip 0 en 'frame' y dumpea el bbox de la malla DEFORMADA (skinVertex). Sirve
    //      para comparar la animacion baseline vs re-importada (el export hornea la pose por-frame). ----
    if (cmd == "skinbbox") {
        int frame=20; ss>>frame;
        Mesh* m = (ObjActivo && ObjActivo->getType()==ObjectType::mesh && ((Mesh*)ObjActivo)->skinArmature) ? (Mesh*)ObjActivo : NULL; // preferir el activo
        if(!m){ std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature && !m) m=(Mesh*)o;
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(!m){err="skinbbox: no hay malla skinneada";return false;}
        Armature* a=m->skinArmature;
        extern int ActiveAnimKind; extern Armature* ActiveAnimArm;
        if(!a->animations.empty()){ActiveAnimKind=1;ActiveAnimArm=a;a->animActiva=0;}
        CurrentFrame=frame; m->lastSkinFrame=-999999; SkinearMesh(m);
        if(!m->skinVertex){err="skinbbox: sin skinVertex";return false;}
        float mn[3]={1e30f,1e30f,1e30f},mx[3]={-1e30f,-1e30f,-1e30f};
        for(int i=0;i<m->vertexSize;i++) for(int c=0;c<3;c++){ float v=m->skinVertex[i*3+c]; if(v<mn[c])mn[c]=v; if(v>mx[c])mx[c]=v; }
        SkeletalAnimation* clip = (!a->animations.empty()) ? a->animations[0] : NULL;
        printf("      [skinbbox] f=%d mesh='%s' deformado=(%.2f,%.2f,%.2f)..(%.2f,%.2f,%.2f)  clip[0] start=%d end=%d fps=%d\n",
            frame, m->name.c_str(), mn[0],mn[1],mn[2],mx[0],mx[1],mx[2],
            clip?clip->startFrame:-1, clip?clip->endFrame:-1, clip?clip->FrameRate:-1);
        return true;
    }
    // ---- applyarmtest <what> <frame> : pone un transform NO trivial (pos+rot+scale) en el armature y hace Apply
    //      (what: 0=location 1=rotation 2=scale 3=all) via Ctrl+A. Mide el bbox MUNDIAL (GetWorldMatrix*skinVertex) de
    //      la malla deformada ANTES, DESPUES del apply (no-op visual -> identico) y tras UNDO (restaura -> identico). ----
    if (cmd == "applyarmtest") {
        int what=2, frame=15; ss>>what>>frame;
        Mesh* m=NULL; { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature && !m) m=(Mesh*)o;
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if(!m){err="applyarmtest: no hay malla skinneada";return false;}
        Armature* a=m->skinArmature;
        extern int ActiveAnimKind; extern Armature* ActiveAnimArm;
        if(!a->animations.empty()){ActiveAnimKind=1;ActiveAnimArm=a;a->animActiva=0;}
        // transform NO trivial (simula el rig movido/rotado/achicado por el usuario)
        a->pos=Vector3(1.2f,0.5f,-0.8f); a->rotEuler=Vector3(20,35,-15); a->rot=Quaternion::FromEulerXYZ(20*3.14159265f/180,35*3.14159265f/180,-15*3.14159265f/180);
        a->scale=Vector3(0.37f,0.37f,0.37f); a->lastPoseFrame=-999999; a->poseSerial++; m->LiberarSkinCache(); m->lastSkinFrame=-999999;
        CurrentFrame=frame; SkinearMesh(m); if(!m->skinVertex){err="applyarmtest: sin skinVertex";return false;}
        struct BB { static void medir(Mesh* mm, float* b){ Matrix4 W; mm->GetWorldMatrix(W);
            for(int k=0;k<3;k++){b[k]=1e30f;b[k+3]=-1e30f;}
            for(int i=0;i<mm->vertexSize;i++){ Vector3 p=W*Vector3(mm->skinVertex[i*3],mm->skinVertex[i*3+1],mm->skinVertex[i*3+2]);
                float pv[3]={p.x,p.y,p.z}; for(int c=0;c<3;c++){ if(pv[c]<b[c])b[c]=pv[c]; if(pv[c]>b[c+3])b[c+3]=pv[c]; } } } };
        float b0[6]; BB::medir(m,b0);
        const char* wn=(what==0)?"Location":(what==1)?"Rotation":(what==2)?"Scale":"All";
        printf("      [applyarmtest] Apply %s | armT(%.2f,%.2f,%.2f) armR(%.0f,%.0f,%.0f) armS(%.3f,%.3f,%.3f)\n",
            wn, a->pos.x,a->pos.y,a->pos.z, a->rotEuler.x,a->rotEuler.y,a->rotEuler.z, a->scale.x,a->scale.y,a->scale.z);
        estado=editNavegacion; InteractionMode=ObjectMode;
        DeseleccionarTodo(); ObjSelects.clear(); a->Seleccionar(); ObjSelects.push_back((Object*)a); ObjActivo=(Object*)a;
        AplicarTransform(what);
        m->lastSkinFrame=-999999; SkinearMesh(m);
        float b1[6]; BB::medir(m,b1);
        float d1=0; for(int k=0;k<6;k++){ float d=fabsf(b1[k]-b0[k]); if(d>d1)d1=d; }
        UndoDeshacer();
        a->lastPoseFrame=-999999; m->LiberarSkinCache(); m->lastSkinFrame=-999999; SkinearMesh(m);
        float b2[6]; BB::medir(m,b2);
        float d2=0; for(int k=0;k<6;k++){ float d=fabsf(b2[k]-b0[k]); if(d>d2)d2=d; }
        printf("      [applyarmtest] tras apply armT(%.2f,%.2f,%.2f) armR(%.0f,%.0f,%.0f) armS(%.3f,%.3f,%.3f)\n         world ANTES =(%.2f,%.2f,%.2f)..(%.2f,%.2f,%.2f)\n         world APPLY =(%.2f,%.2f,%.2f)..(%.2f,%.2f,%.2f)  maxdiff=%.5f %s\n         world UNDO  =(%.2f,%.2f,%.2f)..(%.2f,%.2f,%.2f)  maxdiff=%.5f %s\n",
            a->pos.x,a->pos.y,a->pos.z, a->rotEuler.x,a->rotEuler.y,a->rotEuler.z, a->scale.x,a->scale.y,a->scale.z,
            b0[0],b0[1],b0[2],b0[3],b0[4],b0[5], b1[0],b1[1],b1[2],b1[3],b1[4],b1[5], d1, d1<0.05f?"OK":"DIFF!",
            b2[0],b2[1],b2[2],b2[3],b2[4],b2[5], d2, d2<0.05f?"OK":"DIFF!");
        return true;
    }
    // ---- applyarm [frame] : Apply del modificador Armature -> hornea la pose deformada del frame en la malla editable.
    //      Verifica que skinArmature queda NULL, el modificador se saca, y el bbox de vertex[] paso a la pose deformada. ----
    if (cmd == "applyarm") {
        int frame = 20; ss >> frame;
        Mesh* m = NULL; // primera malla skinneada de la escena
        { std::vector<Object*> st; if(SceneCollection) st.push_back(SceneCollection);
          while(!st.empty()){ Object* o=st.back(); st.pop_back();
            if (o->getType()==ObjectType::mesh && ((Mesh*)o)->skinArmature && !m) m=(Mesh*)o;
            for(size_t i=0;i<o->Childrens.size();i++) st.push_back(o->Childrens[i]); } }
        if (!m) { err="applyarm: no hay malla skinneada"; return false; }
        Armature* a = m->skinArmature;
        extern int ActiveAnimKind; extern Armature* ActiveAnimArm;
        if (!a->animations.empty()) { ActiveAnimKind=1; ActiveAnimArm=a; a->animActiva=0; } // activar el clip 0 para posar
        CurrentFrame = frame;
        int armMod=-1; for(size_t i=0;i<m->modificadores.size();i++) if(m->modificadores[i]->tipo==ModifierType::Armature) armMod=(int)i;
        if (armMod<0){ err="applyarm: la malla no tiene modificador Armature"; return false; }
        m->modificadorActivo = armMod;
        float b0[6]={1e30f,1e30f,1e30f,-1e30f,-1e30f,-1e30f};
        for(int i=0;i<m->vertexSize;i++) for(int c=0;c<3;c++){ float v=m->vertex[i*3+c]; if(v<b0[c])b0[c]=v; if(v>b0[c+3])b0[c+3]=v; }
        int nAntes=(int)m->modificadores.size();
        m->AplicarModificadorActivo();
        float b1[6]={1e30f,1e30f,1e30f,-1e30f,-1e30f,-1e30f};
        for(int i=0;i<m->vertexSize;i++) for(int c=0;c<3;c++){ float v=m->vertex[i*3+c]; if(v<b1[c])b1[c]=v; if(v>b1[c+3])b1[c+3]=v; }
        printf("      [applyarm] f=%d skinArmature=%p (0=OK) mods=%d->%d\n         bind   bbox=(%.1f,%.1f,%.1f)..(%.1f,%.1f,%.1f)\n         horneado bbox=(%.1f,%.1f,%.1f)..(%.1f,%.1f,%.1f)\n",
            frame, (void*)m->skinArmature, nAntes, (int)m->modificadores.size(),
            b0[0],b0[1],b0[2],b0[3],b0[4],b0[5], b1[0],b1[1],b1[2],b1[3],b1[4],b1[5]);
        return true;
    }
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

    // ---- automerge <on|off> [threshold] : Auto Merge (menu Mesh). Al confirmar un move suelda lo movido <= threshold ----
    if (cmd == "automerge") {
        extern bool g_autoMerge; extern float g_autoMergeThreshold;
        std::string on; ss >> on; g_autoMerge = (on == "on" || on == "1" || on == "true");
        float th; if (ss >> th) g_autoMergeThreshold = th;
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

    // ---- crearcara : crea cara/borde desde los verts seleccionados (tecla F). Para testear el anti-duplicado. ----
    if (cmd == "crearcara") {
        Mesh* m = ScriptActiveMesh(); if (!m) { err = "no hay malla activa"; return false; }
        bool ok = m->CrearCaraEdit();
        printf("      [crearcara] ok=%d faces3d=%d\n", (int)ok, (int)m->faces3d.size());
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

    // ---- import <ruta.obj|.fbx> ----
    if (cmd == "import") {
        std::string path; ss >> path;
        if (path.empty()) { err = "falta la ruta del import"; return false; }
        std::string ext = path.size() >= 4 ? path.substr(path.size() - 4) : std::string();
        for (size_t i = 0; i < ext.size(); i++) if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] += 32;
        bool ok = (ext == ".fbx") ? ImportFBX(path) : ImportOBJ(path, false);
        if (!ok) { err = "import fallo (ruta: " + path + ")"; return false; }
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
