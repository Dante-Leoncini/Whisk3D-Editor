#include "Dialogs.h"
#include <whisk3D.rsg>
#include <aknquerydialog.h>

TBool CDialogs::Alert(HBufC* aNoteBuf){
    TBool retVal(EFalse);
	CAknQueryDialog* query = CAknQueryDialog::NewL();
	if (query->ExecuteLD(R_ACCEPT_INVITATION_DLG, *aNoteBuf)) {
		retVal = ETrue;
	}
    return retVal;
}
