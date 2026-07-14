#include "test/W3dScript.h"
#include <cmath>               // sqrt (skindump)
#include "objects/Objects.h"   // ObjActivo, g_editMesh, EditSelectMode, Sel*, DeseleccionarTodo
#include "objects/Mesh.h"      // NewMesh, MeshType, Mesh
#include "edit/Modifier.h"     // Modifier (params del Mirror en el harness)
#include "edit/MeshEdit.h"     // Nuevo/MoverMeshPart (funciones libres del editor)
#include "objects/EditMesh.h"  // EditMesh
#include "ViewPorts/LayoutInput.h" // LayoutToggleEditMode/ExtrudeFaces, EditXform*
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
        AnimProperty& ap = tr.PropertyDe(pr);
        keyFrame kf; kf.frame=frame; kf.valueX=vx; kf.valueY=vy; kf.valueZ=vz; kf.Interpolation=0;
        ap.keyframes.push_back(kf); ap.SortKeyFrames();
        return true;
    }
    // ---- animkeys <bone> <pos|rot|scale> <from> <to> : vuelca los keyframes de esa propiedad en el rango ----
    if (cmd == "animkeys") {
        Armature* a = (ObjActivo && ObjActivo->getType()==ObjectType::armature) ? (Armature*)ObjActivo : NULL;
        if (!a || a->animActiva<0) { err="animkeys: sin clip"; return false; }
        int bone=0, from=0, to=999999; std::string prop; ss>>bone>>prop>>from>>to;
        int pr = (prop=="pos")?AnimPosition : (prop=="rot")?AnimRotation : AnimScale;
        SkeletalAnimation* an = a->animations[a->animActiva];
        for (size_t t=0;t<an->tracks.size();t++) if (an->tracks[t].bone==bone)
            for (size_t p=0;p<an->tracks[t].Propertys.size();p++) if (an->tracks[t].Propertys[p].Property==pr){
                AnimProperty& ap=an->tracks[t].Propertys[p];
                for (size_t k=0;k<ap.keyframes.size();k++){ int f=ap.keyframes[k].frame; if(f<from||f>to) continue;
                    printf("      [key] bone=%d %s f=%d (%.2f,%.2f,%.2f)\n", bone, prop.c_str(), f,
                           ap.keyframes[k].valueX, ap.keyframes[k].valueY, ap.keyframes[k].valueZ); }
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
                printf("      [mesh] '%s' verts=%d tris=%d meshParts=%d\n", m->name.c_str(), m->vertexSize, m->facesSize/3, (int)m->materialsGroup.size());
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
