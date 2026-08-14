// ============================================================================
//  w3dskelstubs.cpp — reloj de ms del CORE para Symbian.
// ----------------------------------------------------------------------------
//  ANTES este archivo tenia STUBS no-op del esqueleto/skinning porque
//  SkeletalAnimation.cpp no se compilaba en el N95. Ya no: SkeletalAnimation.cpp
//  esta en el .mmp y se compila DE VERDAD (esta en dialecto C++03), asi que el
//  esqueleto deforma y anima en el telefono. Aca queda SOLO lo que NO viene de
//  ese .cpp: w3dGetTicks (en PC lo define main.cpp con SDL_GetTicks; aca con el
//  reloj de Symbian). Declarado en animation/Animation.h.
// ============================================================================
#include <e32std.h> // User::NTickCount (reloj de ms de Symbian)

// reloj de ms del CORE (lo usa Animation.cpp para el playback). ~ms (misma convencion que w3dlayout/w3dmouse).
unsigned int w3dGetTicks() { return User::NTickCount(); }
