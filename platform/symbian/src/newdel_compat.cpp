/*
 * ==============================================================================
 *  Name        : newdel_compat.cpp
 *  Part of     : OpenGLEx / Whisk3D
 *
 *  Compatibilidad con S60 3rd/5th edition: el SDK de Symbian^3 mueve los
 *  operator new/delete globales de C++ a scppnwdl.dll, una DLL que NO existe
 *  en el firmware del N95 (S60 3rd FP1). Si el exe la importa, el loader no
 *  puede cargarlo y el telefono muestra "funcion no admitida" al abrir la app.
 *
 *  Definiendo los operadores aca (con la misma semantica que tenian en el
 *  euser viejo: Alloc/Free, NULL si no hay memoria, sin excepciones), el
 *  linker usa estos en vez de los stubs de scppnwdl.lib y el import
 *  desaparece. El new (ELeave) de las clases CBase no pasa por aca (ese vive
 *  en euser y no cambio). El Half-Life no necesita esto porque usa STDCPP:
 *  sus new/delete salen de libstdcpp.dll (Open C++), instalada aparte.
 * ==============================================================================
 */

#ifndef __WINS__

#include <e32std.h>

void* operator new(unsigned int aSize)
    {
    return User::Alloc(aSize);
    }

void* operator new[](unsigned int aSize)
    {
    return User::Alloc(aSize);
    }

void operator delete(void* aPtr)
    {
    User::Free(aPtr);
    }

void operator delete[](void* aPtr)
    {
    User::Free(aPtr);
    }

#endif // !__WINS__
