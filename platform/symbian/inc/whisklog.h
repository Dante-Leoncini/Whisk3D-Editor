/*
 * ==============================================================================
 *  Name        : whisklog.h
 *  Part of     : OpenGLEx / Whisk3D
 *
 *  Modo DEV: log de diagnostico en E:\whisk3d.log (tarjeta de memoria).
 *  Cada linea se abre/escribe/cierra en el momento, con Flush incluido: si la
 *  app se cuelga o muere, el log queda completo hasta el ultimo instante.
 *
 *  Poner WHISK3D_DEV_LOG en 0 para builds de release: las llamadas WLOG/WLOGF
 *  quedan vacias y no se escribe nada.
 *
 *  Uso:
 *    WLOG("texto fijo");
 *    WLOGF(_L("archivo '%S' err=%d"), &nombreDes, err);   // %S necesita &
 * ==============================================================================
 */

#ifndef __WHISKLOG_H__
#define __WHISKLOG_H__

#include <e32std.h>

#define WHISK3D_DEV_LOG 1

#if WHISK3D_DEV_LOG

void WhiskLogReset();                              // borra el log (al iniciar la app)
void WhiskLog(const TDesC& aMsg);                  // una linea con timestamp (ms)
void WhiskLogFmt(TRefByValue<const TDesC> aFmt, ...); // idem con formato

#define WLOG(t)  WhiskLog(_L(t))
#define WLOGF    WhiskLogFmt

#else

inline void WhiskLogReset() {}
inline void WhiskLogFmt(TRefByValue<const TDesC>, ...) {}
#define WLOG(t)
#define WLOGF    WhiskLogFmt

#endif // WHISK3D_DEV_LOG

#endif // __WHISKLOG_H__
