void Confirmar(){

}

void ClickDerecha(){
    rotX+= 0.5;
	redibujar = true;  
}

void ClickIzquierda(){
    rotX-= 0.5;  
	redibujar = true;  
}

void ClickArriba(){
    rotY+= 0.5;
	redibujar = true;  
}

void ClickAbajo(){
    rotY-= 0.5;  
	redibujar = true;  
}

void ClickW(){
    posY+= 1.0;
	redibujar = true;  
}

void ClickS(){
    posY-= 1.0;  
	redibujar = true;  
}

void ClickA(){
    posX+= 1.0;
	redibujar = true;  
}

void ClickD(){
    posX-= 1.0;  
	redibujar = true;  
}

void ClickE(){
    posZ-= 1.0;
	redibujar = true;  
}

void ClickQ(){
    posZ+= 1.0;  
	redibujar = true;  
}