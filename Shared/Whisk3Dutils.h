template <typename T>
void AddPointerToList(T*** list, int& count) {
    // 1️⃣ Crear nueva lista de tamaño (count + 1)
    T** newList = new T*[count + 1];

    // 2️⃣ Copiar los punteros existentes
    for (int i = 0; i < count; ++i) {
        newList[i] = (*list)[i];
    }

    // 3️⃣ Dejar el último espacio vacío (nullptr)
    newList[count] = nullptr;

    // 4️⃣ Liberar la lista anterior (NO los objetos apuntados)
    delete[] *list;

    // 5️⃣ Actualizar el puntero original
    *list = newList;

    // 6️⃣ Aumentar el contador
    ++count;
}