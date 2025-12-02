int GlobalScale = 1;
int margin = 10;
int padding = 10;
int border = 4;
int gap = 5;
int RenglonHeight = 12;
int LetterWidth = 6;
int LetterHeight = 11;
int paddingViewport = 3;
int MinViewportHeight = 30;
int MinViewportWidth = 30;

//precalculos con el global scale para ahorrar multiplicaciones
int marginGS = margin;
int paddingGS = padding;
int gapGS = gap;
int borderGS = border;
int RenglonHeightGS = RenglonHeight;
int LetterWidthGS = LetterWidth;
int LetterHeightGS = LetterHeight;
int paddingViewportGS = paddingViewport;
int MinViewportHeightGS = MinViewportHeight;
int MinViewportWidthGS = MinViewportWidth;

//mouse, se usa en controles... pero tambien necesario en la UI y viewport. por eso esta aca
int dx = 0;
int dy = 0;
//mismo problema para teclas
bool LAltPressed = false;
bool LShiftPressed = false;