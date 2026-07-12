// ============================================================================
//  w3dskelstubs.cpp — STUBS no-op del ESQUELETO / SKINNING para Symbian (N95).
// ----------------------------------------------------------------------------
//  SkeletalAnimation.cpp usa C++11 (nullptr / auto / range-for) y NO compila en
//  la toolchain vieja del N95. El N95 no importa FBX ni maneja esqueletos, asi
//  que estas funciones nunca se EJECUTAN alli; solo hacen falta para que LINKEE
//  lo que el editor referencia (SkinearMesh en el render de Mesh, EvaluarPose
//  del armature, la gestion de clips del panel de propiedades y los helpers de
//  pose-mode del LayoutInput).
//
//  Cuando el esqueleto se pase a C++03 (o el target Symbian sea uno que lo
//  soporte), se saca este archivo del .mmp y se compila SkeletalAnimation.cpp
//  de verdad. La lista de abajo = las funciones LIBRES de SkeletalAnimation.h
//  que referencian los .cpp del .mmp (Animation.cpp SI se compila: los globales
//  de playback y CurrentFrame salen de ahi, no aca).
// ============================================================================
#include <e32std.h>                       // User::NTickCount (reloj de ms de Symbian)
#include "animation/SkeletalAnimation.h" // firmas EXACTAS (incluye Matrix4/Vector3; forward Armature/Mesh)

// reloj de ms del CORE (lo usa Animation.cpp para el playback). En PC lo define main.cpp con SDL_GetTicks;
// aca con el reloj de Symbian (~ms, misma convencion que w3dlayout/w3dmouse). Declarado en animation/Animation.h.
unsigned int w3dGetTicks() { return User::NTickCount(); }

void SkinearMesh(Mesh*) {}
void EvaluarPoseEsqueleto(Armature*, int) {}
void CrearAnimacion(Armature*) {}
void InsertarKeyframeEsqueleto(Armature*) {}
void BorrarAnimacionActiva(Armature*) {}
void MoverAnimacionActiva(Armature*, int) {}
Matrix4 SkelNodeToYupMat() { Matrix4 m; m.Identity(); return m; }
Matrix4 SkelMatRotEuler(const Vector3&, int) { Matrix4 m; m.Identity(); return m; }
Matrix4 SkelBoneWorldNode(Armature*, int) { Matrix4 m; m.Identity(); return m; }
Vector3 SkelMatrizAEulerFBX(const Matrix4&, int) { return Vector3(0.0f, 0.0f, 0.0f); }
