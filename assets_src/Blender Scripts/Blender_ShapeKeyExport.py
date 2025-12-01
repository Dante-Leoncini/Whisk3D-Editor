import bpy
import bmesh

# Obtener el objeto seleccionado
obj = bpy.context.active_object

if obj and obj.type == 'MESH':
    # Obtener la ruta para guardar el archivo
    filepath = bpy.path.abspath("//vertices_keyframes.txt")
    
    # Obtener la escena activa
    scene = bpy.context.scene
    start_frame = scene.frame_start
    end_frame = scene.frame_end

    # Abrir el archivo correctamente para escritura
    try:
        with open(filepath, "w") as output_file:
            output_file.write("# Este archivo es para importar animaciones de keyframes en Whisk3D y fue creado por Dante Leoncini\n")
            output_file.write("new_animation_ShapeKey\n")
            
            for frame in range(start_frame, end_frame + 1):
                # Configurar la escena al fotograma actual
                scene.frame_set(frame)

                # Obtener el gráfico de dependencias
                depsgraph = bpy.context.evaluated_depsgraph_get()
                eval_obj = obj.evaluated_get(depsgraph)

                # Crear un bmesh para trabajar con los datos evaluados
                bm = bmesh.new()
                bm.from_object(eval_obj, depsgraph)

                # Asegurarse de que las normales están actualizadas
                bm.normal_update()

                # Escribir el encabezado del keyframe
                output_file.write("ShapeKey\n")  

                for vert in bm.verts:
                    # Convertir la posición del vértice al espacio mundial
                    world_pos = obj.matrix_world @ vert.co
                    # Convertir la normal del vértice al espacio mundial
                    world_normal = obj.matrix_world.to_3x3() @ vert.normal

                    # Escribir en el archivo en formato: akf x y z nx ny nz
                    output_file.write(f"akf {world_pos.y} {world_pos.z} {world_pos.x} {world_normal.y} {world_normal.z} {world_normal.x}\n")

                # Liberar el bmesh
                bm.free()

        print(f"Datos guardados en: {filepath}")
    except IOError as e:
        print(f"Error al guardar el archivo: {e}")

else:
    print("Selecciona un objeto de tipo 'MESH'")