import bpy
import os
from math import *
import math

os.system('clear')

#nombres
scene = bpy.context.scene
MultiplicadorPos = 5000

# Obtener la ruta de la carpeta que contiene el archivo .blend actual
dirname = bpy.path.abspath("//")
blend_name = bpy.path.basename(bpy.data.filepath).split(".")[0]  # Nombre del proyecto sin extensión

# Crear el archivo de salida
Archivo = open(dirname + "/animation_" + blend_name + ".txt", "w")  # abrir archivo
print("Archivo creado: " + dirname + "/animation_" + blend_name + ".txt")

#frame de inicio
bpy.context.scene.frame_set(scene.frame_start)
#print("Cantidad de frames: "+str(scene.frame_end - scene.frame_start + 1))
print("Cantidad de frames: "+str(scene.frame_end) )

#calcula la posicion y le da formato
def CalcPos(pos):
    return str(math.trunc(pos*MultiplicadorPos))

def CalcRot(rot):
    return str(math.trunc(degrees(rot)))

def CalcScale(scale):
    return str(math.trunc(scale * 1000))

# Exportar animaciones de los objetos seleccionados
selected_objects = bpy.context.selected_objects

if not selected_objects:
    print("No se encontraron objetos seleccionados.")
else:
    for obj in selected_objects:
        Archivo.write("obj " + obj.name + "\n")
        TotalFrames = scene.frame_end-scene.frame_start+1
        Archivo.write("anim Animacion_1 "+str(TotalFrames)+"\n")
        
        Archivo.write("rotacion "+str(TotalFrames)+"\n")
        Archivo.write("#frame, interpolacion, rotacion X, Y, Z\n")
        Archivo.write("#interpolaciones, c constante, l lineal\n")
        for x in range(TotalFrames):
            frame = bpy.data.scenes["Scene"].frame_current        
            
            #graba el frame
            Archivo.write("r "+str(frame)+" "+
                CalcRot(obj.rotation_euler.x)+" "+
                CalcRot(obj.rotation_euler.y)+" "+
                CalcRot(obj.rotation_euler.z)+'\n')
            
            #pasa al siguiente    
            bpy.context.scene.frame_set(frame+1)

        # Volver al frame inicial
        bpy.context.scene.frame_set(scene.frame_start)  

        Archivo.write("locacion "+str(TotalFrames)+"\n")
        for x in range(TotalFrames):
            frame = bpy.data.scenes["Scene"].frame_current        
                
            Archivo.write("l "+str(frame)+" "+
                        CalcPos(obj.location.x)+" "+
                        CalcPos(obj.location.y)+" "+
                        CalcPos(obj.location.z)+'\n')      
            
            #pasa al siguiente    
            bpy.context.scene.frame_set(frame+1)

        # Volver al frame inicial
        bpy.context.scene.frame_set(scene.frame_start)

        # Exportar escala
        Archivo.write("escala " + str(TotalFrames) + "\n")
        Archivo.write("#frame, escala X, Y, Z\n")
        for x in range(TotalFrames):
            frame = bpy.data.scenes["Scene"].frame_current
            Archivo.write("s " + str(frame) + " " +
                            CalcScale(obj.scale.x) + " " +
                            CalcScale(obj.scale.y) + " " +
                            CalcScale(obj.scale.z) + '\n')

            # Pasa al siguiente frame
            bpy.context.scene.frame_set(frame + 1)
        print("Fin de Bucle")
    
Archivo.close() #cierra y termina
print("Archivo Creado")