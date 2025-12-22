#ifndef UI_H
#define UI_H

extern int GlobalScale;
extern int margin;
extern int padding;
extern int border;
extern int gap;
extern int RenglonHeight;
extern int LetterWidth;
extern int LetterHeight;
extern int paddingViewport;
extern int MinViewportHeight;
extern int MinViewportWidth;
extern int CharacterWidth;

// precalculos con el global scale
extern int marginGS;
extern int paddingGS;
extern int gapGS;
extern int borderGS;
extern int bordersGS;
extern int RenglonHeightGS;
extern int LetterWidthGS;
extern int LetterHeightGS;
extern int paddingViewportGS;
extern int MinViewportHeightGS;
extern int MinViewportWidthGS;
extern int CharacterWidthGS;

// mouse
extern int dx;
extern int dy;

// teclas
extern bool LAltPressed;
extern bool LShiftPressed;

#endif