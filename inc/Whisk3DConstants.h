#ifndef __AKNEXNOTECONSTANTS_H__
#define __AKNEXNOTECONSTANTS_H__

// CONSTANTS

// All apllications constants are here.
const TInt KWhisk3DGray = 0xaaaaaa;

const TInt KWhisk3DComponentCount = 1;
const TInt KWhisk3DMessageBufLength = 256;
const TInt KWhisk3DRandomNumberRange = 20;
const TInt KWhisk3DProgressbarFinalValue = 200;
const TInt KWhisk3DInitializeTime = 0;
const TInt KWhisk3DInitializeResourceId = 0;
const TInt KWhisk3DTextBufLength = 256;

// Define show time of wait note and permanent note.
// Unit is microsecond. 
const TInt KMaxPermanentNoteShowTime( 6 * 1000000 );

const TUid KViewId = { 1 }; // UID of view
const TInt KWhisk3DTitleBufLength = 256;

const TInt KPermNoteCallBackPriority( CActive::EPriorityStandard );
const TInt KWaitNoteCallBackPriority( CActive::EPriorityStandard );

#endif  //__AKNEXNOTECONSTANTS_H__

// End of File
