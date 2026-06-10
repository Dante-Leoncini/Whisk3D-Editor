/*
 * ==============================================================================
 *  Name        : whisklog.cpp
 *  Part of     : OpenGLEx / Whisk3D
 *
 *  Implementacion del log de diagnostico (ver whisklog.h).
 * ==============================================================================
 */

#include "whisklog.h"

#if WHISK3D_DEV_LOG

#include <f32file.h>
#include "fscompat.h" // cerrar RFs sin importar efsrv@390 (Symbian^3)

_LIT(KWhiskLogPath, "e:\\whisk3d.log");

// AppendFormatList panickea si el texto no entra en el buffer; este handler
// hace que simplemente quede truncado.
class TWhiskLogOverflow : public TDes16Overflow
    {
    public:
        void Overflow(TDes16& /*aDes*/) {}
    };

static void WhiskLogWrite(const TDesC& aMsg, TBool aTruncateFile)
    {
    RFs fs;
    if (fs.Connect() != KErrNone)
        {
        return; // sin tarjeta E: u otro problema: no hay log, pero no molesta
        }

    RFile f;
    TInt err;
    if (aTruncateFile)
        {
        err = f.Replace(fs, KWhiskLogPath, EFileWrite | EFileShareAny);
        }
    else
        {
        err = f.Open(fs, KWhiskLogPath, EFileWrite | EFileShareAny);
        if (err == KErrNotFound)
            {
            err = f.Create(fs, KWhiskLogPath, EFileWrite | EFileShareAny);
            }
        }

    if (err == KErrNone)
        {
        TInt pos = 0;
        f.Seek(ESeekEnd, pos);

        // timestamp en ms + mensaje pasado a 8 bits + CRLF
        TBuf8<240> line;
        line.AppendNum((TUint)User::NTickCount());
        line.Append(' ');
        TBuf8<200> msg8;
        msg8.Copy(aMsg.Left(195));
        line.Append(msg8);
        line.Append(_L8("\r\n"));

        f.Write(line);
        f.Flush(); // a disco YA, por si lo proximo que pasa es el cuelgue
        f.Close();
        }

    FsCloseCompat(fs);
    }

void WhiskLogReset()
    {
    WhiskLogWrite(_L("=== Whisk3D DEV LOG: inicio de sesion ==="), ETrue);
    }

void WhiskLog(const TDesC& aMsg)
    {
    WhiskLogWrite(aMsg, EFalse);
    }

void WhiskLogFmt(TRefByValue<const TDesC> aFmt, ...)
    {
    VA_LIST list;
    VA_START(list, aFmt);
    TBuf<195> buf;
    TWhiskLogOverflow overflow;
    buf.AppendFormatList(aFmt, list, &overflow);
    VA_END(list);
    WhiskLogWrite(buf, EFalse);
    }

#endif // WHISK3D_DEV_LOG
