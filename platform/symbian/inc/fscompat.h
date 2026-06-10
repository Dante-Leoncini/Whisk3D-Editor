/*
 * ==============================================================================
 *  Name        : fscompat.h
 *  Part of     : OpenGLEx / Whisk3D
 *
 *  RFs::Close() paso a ser un export de efsrv.dll en el SDK de Symbian^3
 *  (ordinal 390); antes era inline. Ese ordinal NO existe en el efsrv de los
 *  telefonos S60 viejos: si el exe lo importa, el loader no puede cargar el
 *  binario y el telefono muestra "funcion no admitida" al abrir la app.
 *  (El Half-Life no linkea efsrv, por eso nunca le paso.)
 *
 *  Cerrando la sesion via RSessionBase::Close (export de euser de toda la
 *  vida) el import a efsrv@390 desaparece y la app carga en cualquier
 *  firmware. Usar FsCloseCompat(fs) en lugar de fs.Close().
 * ==============================================================================
 */

#ifndef __FSCOMPAT_H__
#define __FSCOMPAT_H__

#include <e32base.h>
#include <f32file.h>

static inline void FsCloseCompat(RFs& aFs)
    {
    ((RSessionBase&)aFs).Close();
    }

// CleanupClosePushL(fs) tambien instancia una llamada a RFs::Close() (via la
// plantilla CleanupClose<RFs>), o sea que vuelve a meter el import efsrv@390
// aunque no haya ningun fs.Close() directo. Usar CleanupCloseFsPushL en su
// lugar: misma semantica (cierra al hacer PopAndDestroy), sin el import.
static void FsCleanupCompatFn(TAny* aPtr)
    {
    FsCloseCompat(*static_cast<RFs*>(aPtr));
    }

static inline void CleanupCloseFsPushL(RFs& aFs)
    {
    CleanupStack::PushL(TCleanupItem(FsCleanupCompatFn, &aFs));
    }

#endif
