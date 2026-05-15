#ifndef __HIDINPUTMONITOR_H__
#define __HIDINPUTMONITOR_H__

#include <e32base.h>
#include <bthidclient.h>

class CHIDEventMonitor : public CActive
    {
    public:
        virtual void RunL();
        virtual void DoCancel();
        static CHIDEventMonitor* NewL();
        ~CHIDEventMonitor();

    protected: 
        CHIDEventMonitor();
        void ConstructL();       
    private:         
        MHIDSrvClient* iHIDClient;
        RLibrary iHidLibrary;
    };
    
#endif
