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
 *  en euser y no cambio).
 *
 *  SIN MEMORIA: devolver NULL no avisa a nadie. Casi todo el codigo C++ (un
 *  std::vector que crece, el new de un objeto) sigue de largo y escribe en
 *  NULL: la app se cierra sin dejar rastro. Por eso el pedido que falla queda
 *  en el log, con el estado de la memoria (w3dMemoryKB no reserva nada).
 * ==============================================================================
 */

#ifndef __WINS__

#include <e32std.h>
#include "w3dlog.h"

// (si el log mismo pidiera memoria y fallara, no se vuelve a entrar)
static TBool sNotifyingNoMemory = EFalse;

static void NotifyNoMemory(unsigned int aSize)
    {
    if (sNotifyingNoMemory)
        {
        return;
        }
    sNotifyingNoMemory = ETrue;
    long used = -1, taken = -1, free = -1;
    w3dMemoryKB(used, taken, free);
    w3dLogfE("[mem] SIN MEMORIA: fallo un pedido de %u bytes (heap %ld KB en uso, %ld KB tomados; RAM libre %ld KB)",
             aSize, used, taken, free);
    sNotifyingNoMemory = EFalse;
    }

static void* Request(unsigned int aSize)
    {
    void* p = User::Alloc(aSize);
    if (!p)
        {
        NotifyNoMemory(aSize);
        }
    return p;
    }

void* operator new(unsigned int aSize)
    {
    return Request(aSize);
    }

void* operator new[](unsigned int aSize)
    {
    return Request(aSize);
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
