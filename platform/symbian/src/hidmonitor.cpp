/*
 * ==============================================================================
 *  Name        : hidmonitor.cpp
 *  Part of     : OpenGLEx / Whisk3D
 *
 *  Ver hidmonitor.h. Adaptado del port de OpenRCT2 (y este del Half-Life).
 * ==============================================================================
 */

#include "hidmonitor.h"
#include "w3dlog.h"

CHidMonitor::CHidMonitor(MHidObserver& aObs)
    : CActive(CActive::EPriorityStandard), iObs(aObs), iClient(NULL)
    {
    }

CHidMonitor* CHidMonitor::NewL(MHidObserver& aObs)
    {
    CHidMonitor* self = new (ELeave) CHidMonitor(aObs);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

void CHidMonitor::ConstructL()
    {
    User::LeaveIfError(iLib.Load(_L("hidsrv.dll")));
    TLibraryFunction entry = iLib.Lookup(1); // GetHIDClientAPI
    if (entry == NULL)
        {
        User::Leave(KErrNotFound);
        }
    iClient = reinterpret_cast<MHIDSrvClient*>(entry());
    if (iClient == NULL)
        {
        User::Leave(KErrNotFound);
        }
    User::LeaveIfError(iClient->Connect());

    CActiveScheduler::Add(this);
    iClient->EventReady(&iStatus);
    SetActive();
    w3dLog("HidMonitor: conectado a hidsrv, escuchando eventos");
    }

CHidMonitor::~CHidMonitor()
    {
    Cancel();
    if (iClient != NULL)
        {
        iClient->Close();
        }
    delete iClient;
    iLib.Close();
    }

void CHidMonitor::DoCancel()
    {
    if (iClient != NULL)
        {
        iClient->EventReadyCancel();
        }
    }

void CHidMonitor::RunL()
    {
    THIDEvent ev;
    iClient->GetEvent(ev);

    switch (ev.Type())
        {
        case THIDEvent::EMouseEvent:
            {
            TMouseEvent* m = ev.Mouse();
            switch (m->Type())
                {
                case EEventRelativeXY:
                    iObs.HidMouseMove(m->iPosition.iX, m->iPosition.iY);
                    break;
                case EEventButtonDown:
                    iObs.HidMouseButton(m->iValue, ETrue);
                    break;
                case EEventButtonUp:
                    // El driver BT manda UPs ESPURIOS con iValue=0 (Null) cuando
                    // se MANTIENE un boton apretado SIN mover el mouse (al mover
                    // no los manda). El codigo viejo los pasaba como UP real y el
                    // observador soltaba el boton -> cortaba el drag/scroll y la
                    // vista 3D, y al mantener+mover hacia multi-disparo (clicks
                    // fantasma que prendian/apagaban la UI). Fix (como el port de
                    // Q3): soltar SOLO un boton con iValue valido; ignorar los
                    // iValue=0 espurios. El UP real de soltar trae Left/Right/Middle.
                    if (m->iValue == EMouseButtonLeft ||
                        m->iValue == EMouseButtonRight ||
                        m->iValue == EMouseButtonMiddle) {
                        iObs.HidMouseButton(m->iValue, EFalse);
                    }
                    break;
                case EEventRelativeWheel:
                    iObs.HidMouseWheel(m->iValue);
                    break;
                default:
                    break;
                }
            break;
            }

        case THIDEvent::EKeyEvent:
            {
            THIDKeyEvent* key = ev.Key();
            iObs.HidKey(key->ScanCode(), key->Type() == EEventHIDKeyDown);
            break;
            }

        default:
            break;
        }

    iClient->EventReady(&iStatus);
    SetActive();
    }
