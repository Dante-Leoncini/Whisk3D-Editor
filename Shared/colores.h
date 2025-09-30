enum {
	background,
	blanco,
	accent,
	accentDark,
	negro,
	gris,
	naranjaFace,
	headerColor,
	negroTransparente,
	grisUI
};

//array de colores
GLfloat ListaColores[11][4] = {
	{ 0.22, 0.28, 0.25, 1.0  },   //fondo
	{ 1.0, 1.0, 1.0, 1.0     },   //blanco
	{ 0.91, 0.50, 0.98, 1.0  },   //acento (violeta) 
	{ 0.48, 0.30, 1.0, 1.0  },   //acento oscuro	
	{ 0.0, 0.0, 0.0, 1.0     },   //negro
	{ 0.12, 0.12, 0.12, 1.0  },   //gris
	{ 0.94, 0.59, 0.17, 0.25f},   //naranja transparente
	{ 0.22, 0.22, 0.22, 1.0  },    //cabezera de la barra de herramientas
	{ 0.0, 0.0, 0.0, 0.25f     },   //negroTransparente
	{ 0.757, 0.757, 0.757, 1.0     },   //azul de los iconos seleccionados
};