/*
 * ==============================================================================
 *  Name        : hidmonitor.h
 *  Part of     : OpenGLEx / Whisk3D
 *
 *  Soporte de mouse/teclado bluetooth (servidor HID de Hinkka), adaptado del
 *  port de OpenRCT2 (que a su vez salio del Half-Life/Xash3D). hidsrv.dll se
 *  carga en RUNTIME -- NO se linkea -- asi que si no esta instalada (o no hay
 *  mouse emparejado) NewL() simplemente deja y la app sigue con las teclas
 *  del telefono. Por eso el llamado va siempre adentro de un TRAPD.
 * ==============================================================================
 */

#ifndef __WHISK3D_HIDMONITOR_H__
#define __WHISK3D_HIDMONITOR_H__

#include <e32base.h>
#include "bthidclient.h"

// Recibe los eventos HID ya clasificados (lo implementa CWhisk3D).
class MHidObserver
    {
    public:
        virtual void HidMouseMove(TInt aDx, TInt aDy) = 0;
        virtual void HidMouseButton(TInt aButton, TBool aDown) = 0; // TMouseButtonID
        virtual void HidMouseWheel(TInt aDelta) = 0;                // + = arriba
        virtual void HidKey(TInt aScanCode, TBool aDown) = 0;       // scancode USB HID
    };

class CHidMonitor : public CActive
    {
    public:
        static CHidMonitor* NewL(MHidObserver& aObs); // deja si falta hidsrv.dll
        ~CHidMonitor();

    private:
        CHidMonitor(MHidObserver& aObs);
        void ConstructL();

        // de CActive
        void RunL();
        void DoCancel();

    private:
        MHidObserver& iObs;
        MHIDSrvClient* iClient;
        RLibrary iLib;
    };

#endif // __WHISK3D_HIDMONITOR_H__
