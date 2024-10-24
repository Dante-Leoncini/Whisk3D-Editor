#ifndef DIALOGS_H
#define DIALOGS_H

#include <e32base.h>
#include <aknmessagequerydialog.h>

class CDialogs : public CBase {
    public:
        static TBool Alert(HBufC* aNoteBuf);
};

#endif // DIALOGS_H
