import bpy
import os
from math import *
import math

os.system('clear')

#nombres
scene = bpy.context.scene
objName = "Dado"
MultiplicadorPos = 5000

# Obtener la ruta de la carpeta que contiene el archivo .blend actual
dirname = bpy.path.abspath("//")
#dirname = os.getcwd() #ruta relativa

Archivo=open(dirname+"/animation_"+objName+".txt","w") #abrir archivo
print(dirname+"/animation_"+objName+".txt")
Archivo.write("static const int animacionRot["+str(scene.frame_end)+"][3] = {"+'\n') #guardar inicio

#frame de inicio
bpy.context.scene.frame_set(scene.frame_start)
#print("Cantidad de frames: "+str(scene.frame_end - scene.frame_start + 1))
print("Cantidad de frames: "+str(scene.frame_end) )

#calcula la posicion y le da formato
def CalcPos(pos):
  return str(math.trunc(pos*MultiplicadorPos))

def CalcRot(rot):
  return str(math.trunc(degrees(rot)))

if not bpy.data.objects.get(objName):
    print("no se encontro "+objName)
else:
    obj = bpy.data.objects.get(objName);
    TotalFrames = scene.frame_end-scene.frame_start+1
    for x in range(TotalFrames):
        frame = bpy.data.scenes["Scene"].frame_current
        #print(obj.location)
        #print(obj.rotation_euler)
        #print(degrees(obj.rotation_euler.x))
        
        endString = ","
        if (scene.frame_end == frame):
            endString = ""        
        else:
            endString = ","
        
        #graba el frame
        Archivo.write("    {"+
                      CalcRot(obj.rotation_euler.x)+", "+
                      CalcRot(obj.rotation_euler.y)+", "+
                      CalcRot(obj.rotation_euler.z)+'}'+endString+'\n') #guardar
        
        #pasa al siguiente    
        bpy.context.scene.frame_set(frame+1)
    Archivo.write("};\n") #guardar inicio     
    Archivo.write("animacionPos = ["+'\n') #guardar inicio   
    bpy.context.scene.frame_set(scene.frame_start)
        
    for x in range(TotalFrames):
        frame = bpy.data.scenes["Scene"].frame_current
        #print(obj.location)
        #print(obj.rotation_euler)
        #print(degrees(obj.rotation_euler.x))
        
        #graba el frame
        endString = ","
        if (scene.frame_end == frame):
            endString = ""        
        else:
            endString = ","
            
        Archivo.write("    {"+
                      CalcPos(obj.location.x)+", "+
                      CalcPos(obj.location.y)+", "+
                      CalcPos(obj.location.z)+'}'+endString+'\n') #guardar        
        
        #pasa al siguiente    
        bpy.context.scene.frame_set(frame+1)
    Archivo.write("};") #guardar inicio
    print("Fin de Bucle")
    
Archivo.close() #cierra y termina
print("Archivo Creado")