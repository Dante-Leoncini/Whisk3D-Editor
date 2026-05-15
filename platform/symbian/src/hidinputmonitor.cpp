#include "hidinputmonitor.h"
#include <bthidevent.h>
#include <bthidclient.h>

#include <e32debug.h>   // RDebug::Print
#include <e32cons.h>    // Console

static CConsoleBase* gConsole = NULL;

// ===================== CONSTRUCTOR =====================

CHIDEventMonitor* CHIDEventMonitor::NewL()
{
    CHIDEventMonitor* self = new (ELeave) CHIDEventMonitor;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
}

CHIDEventMonitor::CHIDEventMonitor()
    : CActive(CActive::EPriorityIdle)
{
}

void CHIDEventMonitor::ConstructL(){
    // Crear consola
    if (!gConsole)
    {
        gConsole = Console::NewL(_L("HID Debug"), TSize(KConsFullScreen, KConsFullScreen));
    }

    gConsole->Printf(_L("HID: ConstructL\n"));
    RDebug::Print(_L("HID: ConstructL"));

    // Cargar DLL
    User::LeaveIfError(iHidLibrary.Load(_L("hidsrv.dll")));
    gConsole->Printf(_L("DLL OK\n"));

    // Obtener entry
    TLibraryFunction entry = iHidLibrary.Lookup(1);
    if (!entry){
        gConsole->Printf(_L("entry NULL\n"));
        User::Leave(KErrNotFound);
    }

    gConsole->Printf(_L("entry OK\n"));

    // Crear cliente
    iHIDClient = (MHIDSrvClient*) entry();

    TInt err = iHIDClient->Connect();
    gConsole->Printf(_L("Connect=%d\n"), err);

    User::LeaveIfError(err);

    // Activar Active Object
    CActiveScheduler::Add(this);
    iHIDClient->EventReady(&iStatus);
    SetActive();

    gConsole->Printf(_L("READY\n"));
}

// ===================== DESTRUCTOR =====================

CHIDEventMonitor::~CHIDEventMonitor()
{
    Cancel();

    if (iHIDClient)
        iHIDClient->Close();

    iHidLibrary.Close();

    if (gConsole)
    {
        gConsole->Printf(_L("HID: END\n"));
    }
}

// ===================== EVENTOS =====================

void CHIDEventMonitor::RunL()
{
    THIDEvent event;
    iHIDClient->GetEvent(event);

    gConsole->Printf(_L("Event type=%d\n"), event.Type());

    if (event.Type() == THIDEvent::EMouseEvent)
    {
        TMouseEvent* mouse = event.Mouse();

        gConsole->Printf(_L("Mouse type=%d val=%d\n"),
            mouse->Type(),
            mouse->Value());

        if (mouse->Type() == EEventButtonDown &&
            mouse->Value() == EMouseButtonLeft)
        {
            gConsole->Printf(_L("CLICK LEFT\n"));
        }

        if (mouse->Type() == EEventRelativeXY)
        {
            gConsole->Printf(_L("MOVE dx=%d dy=%d\n"),
                mouse->Position().iX,
                mouse->Position().iY);
        }
    }

    // volver a escuchar
    iHIDClient->EventReady(&iStatus);
    SetActive();
}

void CHIDEventMonitor::DoCancel()
{
    if (iHIDClient)
        iHIDClient->EventReadyCancel();
}
