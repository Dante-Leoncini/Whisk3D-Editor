enum ColorID {
	background,
	blanco,
	accent,
	accentDark,
	negro,
	gris,
	naranjaFace,
	headerColor,
	negroTransparente,
	grisUI,
	rojoEje,

	LineaPiso,
	LineaPisoRoja,
	LineaPisoVerde,
	ColorTransformX,
	ColorTransformY,
	ColorTransformZ
};

//array de colores
GLfloat ListaColores[17][4] = {
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
	{ 0.757, 0.757, 0.757, 1.0     },   //azul de los iconos seleccionados

	{ 0.22, 0.22, 0.22, 1.0 },  //LineaPiso
	{ 0.56, 0.23, 0.28, 1.0 },  //LineaPisoRoja
	{ 0.38, 0.53, 0.15, 1.0 },  //LineaPisoVerde

	{ 0.88, 0.48, 0.54, 1.0 },  //ColorTransformX
	{ 0.65, 0.81, 0.38, 1.0 },  //ColorTransformY
	{ 0.46, 0.67, 0.89, 1.0 },  //ColorTransformZ
};