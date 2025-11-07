int GlobalScale = 1;
int margin = 10;
int padding = 10;
int border = 4;
int gap = 5;
int RenglonHeight = 12;
int LetterWidth = 6;
int LetterHeight = 11;

//precalculos con el global scale para ahorrar multiplicaciones
int marginGS = margin;
int paddingGS = padding;
int gapGS = gap;
int borderGS = border;
int RenglonHeightGS = RenglonHeight;
int LetterWidthGS = LetterWidth;
int LetterHeightGS = LetterHeight;

//mouse, se usa en controles... pero tambien necesario en la UI y viewport. por eso esta aca
int dx = 0;
int dy = 0;
//mismo problema para teclas
bool LAltPressed = false;
bool LShiftPressed = false;

#include "./sprites.h"
#include "./object2D.h"