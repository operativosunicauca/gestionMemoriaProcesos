/**
 * @file
 * @ingroup user_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 *
 * @brief Este archivo implementa las primitivas para la gestion de memoria.
 */

#include <kmm.h>

/** @brief Función para comparar dos regiones de memoria */
int compare_memreg_header_t(memreg_header_t * a, memreg_header_t *b) {
    return b->limit - a->limit;
}

/** @brief Función para comparar una región de memoria con un escalar */
int equals_memreg_header_t(memreg_header_t * a, void *b) {
    return (unsigned int)b - (unsigned int)a->base;
}

/** @brief Implementación de las primitivas para gestionar listas de tipo
 * memreg_header_t*/
IMPLEMENT_GENERIC_LIST_TYPE(memreg_header_t, memreg_header);

/**
 * @brief Esta rutina se encarga de inicializar el area de memoria
 * para asignacion dinamica, y las estructuras de datos requeridas para su
 * gestion.
 * @param ptr Puntero al inicio de la memoria disponible para asignacion
 * dinámica
 * @param limit Tamaño de la memoria disponible
 * @return Apuntador al heap inicializado.
 */
heap_t * setup_heap(void * ptr, unsigned int limit) {

	int i;
	
	unsigned int base;

	heap_t * heap;

	base = (unsigned int)ptr;
	
	heap = create_heap(base, limit);

	return heap;
}

/**
 * @brief Permite crear una region de memoria para asignacion dinamica.
 * Importante: Esta rutina asume que la region de memoria que se desea
 * crear esta disponible. Si no se utiliza adecuadamente, puede danar regiones
 * de memoria creadas anteriormente.
 * @param base Dirección lineal de inicio del heap
 * @param limit Tamaño de la región de memoria para asignación dinámica.
 * @return Apuntador al nuevo heap
 */
heap_t *  create_heap(unsigned int base, unsigned int limit) {

	/** @note Esta rutina asume que la region de memoria especificada es
	 * válida, y que no está sobreescribiendo otro heap.
	 * Al crear el heap se destruye la información que existe en el
	 * inicio de la region de memoria especificada. */
	 int i;

	unsigned int offset;

	heap_t * heap;

	/* Validacion inicial: Hay suficiente espacio para crear un heap?
	 * */

	heap = 0;

	if (limit < HEAP_MIN_SIZE) {
		return 0;
	}


	//printf("Creating new heap at %u\n", base);

	/* Crear la estructura heap_t al inicio de la region */
	heap = (heap_t *)base;

	/* Debido a que la estructura heap_t contiene las estructuras
	 * list_memreg_header_t * de las regiones libres y usadas, tambien se deben
	 * crear estas sub-estructuras en memoria. */

	/* Ubicar la lista de regiones libres inmediatamente despues
	 * del heap */
	heap->free = (list_memreg_header *)(base + sizeof(heap_t));

	/* Ubicar la lista de regiones usadas inmediatamente despues
		 * de la lista de regiones libres */
	/*
    heap->used = (list_memreg_header_t *)(base + sizeof(heap_t) +
											sizeof(list_memreg_header_t));
	*/
	
	/* Desplazamiento de memoria en la cual comienza el area de datos del heap:
	 * se debe sumar el tamaño de la estructura heap_t, y el tamaño de las
	 * dos estructuras para las listas de regiones libres y usadas. */

	offset = sizeof(heap_t) + /* Tamanio de la estructura heap_t */
			 sizeof(list_memreg_header) + /* Espacio para la lista de
											 regiones libres */
			 sizeof(list_memreg_header); /* Espacio para la lista de
											  regiones usadas */

	/* Establecer los parametros iniciales del heap */

	/* Inicio del espacio disponible */
	heap->base = base + offset;
	heap->limit = limit - offset; /* Tamanio a asignar */
	heap->top = heap->base; /* Inicializar el tope del heap */

	init_list_memreg_header(heap->free); /* Inicializar la lista de libres */
 //init_list_memreg_header(heap->used); /* Inicializar la lista de usadas */

	return heap;
}

/**
 * @brief Función privada para expandir el heap en un tamaño especificado.
 * @param heap Heap que se desea expandir
 * @param limit  Tamanio a expandir.
 * @return Puntero al encabezado de la nueva region de memoria, 0 si no es
 *  posible expandir el heap. Esta region de memoria ya se encuentra insertada
 *  dentro de la lista de regiones disponibles.
 */
memreg_header_t * expand_heap(heap_t * heap, unsigned int limit) {

	memreg_header_t * header;
	memreg_footer_t * footer;

	unsigned int space_available;

	if (heap == 0) {
		//printf("Heap not created yet!\n");
		return 0;
	}

	/* Verificar el tamano minimo asignado */
	if (limit < MEMREG_GRANULARITY) {
		limit = MEMREG_GRANULARITY;
	}

	space_available = heap->base + heap->limit - heap->top;

	/* Verificar si existe suficiente espacio para la region solicitada */
	if (space_available < MEMREG_HEADER_SIZE + limit + MEMREG_FOOTER_SIZE) {
		/*
		printf("No space available. Requested: %d available: %d\n",
					limit,
					space_available
				);
		*/
		return 0;
	}

	/* Crear la nueva region de memoria e insertarla en
	 * la lista de regiones disponibles */
	header = (memreg_header_t * )(heap->top);
	header->base = (unsigned int)header + MEMREG_HEADER_SIZE;
	header->limit = limit;
	header->used = 0;
	header->next_memreg_header = 0;
	header->prev_memreg_header = 0;

	footer = (memreg_footer_t *)(header->base + header->limit);
	footer->base = header->base;
	footer->header = header;

	/* Expandir el heap */
	//printf("Top of heap expanded from %u", heap->top);
	heap->top  = (unsigned int)footer+ MEMREG_FOOTER_SIZE;
	//printf(" to %u\n", heap->top);


	/* Insertar la region en la lista de regiones disponibles */
	push_front_memreg_header(heap->free, header);
	return header;
}

/**
 * @brief Solicita la asignacion de memoria dentro de un heap.
 * Primero se busca dentro de las regiones libres, una que tenga
 * un tamaño cercano al buscado. Si no existe una region, expande el heap.
 * Si existe suficiente espacio adicional al buscado dentro de la region, esta
 * region se divide en dos: una region asignada, que se retorna, y una
 * region libre con el espacio sobrante.
 * @param heap Heap del cual se desea obtener el espacio
 * @param size Cantidad de memoria a asignar
 * @return Apuntador a la region de memoria asignada. 0 si no se puede
 * asignar memoria.
 */
void * alloc_from_heap(heap_t * heap, unsigned int size) {

	/* Region candidata a asignar */
	memreg_header_t * candidate = 0;
	
	/* Apuntador al pie original de la region candidata */
	memreg_footer_t * candidate_footer;

	/* Tamanio original de la region de memoria candidata a asignar */
	unsigned int candidate_limit;

	/* Apuntadores al nuevo encabezado y pie, si se requiere partir
	 * una region de memoria libre */

	/* Apuntador al nuevo encabezado */
	memreg_header_t * new_header;
	/* Apuntador al nuevo pie */
	memreg_footer_t * new_footer;

	unsigned int requested_size = 0;

	/* Verificar que el tamaño solicitado sea mayor o igual que el minimo
	 * tamaño que se puede asignar */

	if (size < MEMREG_GRANULARITY) {
		size = MEMREG_GRANULARITY;
	}

	/* Ahora buscar una region marcada como libre, que tenga
	 * un tamano mayor o igual a la cantidad  solicitada.
	 */

	candidate = heap->free->head;
	while (candidate != 0 && candidate->limit < size ) {
		candidate = candidate->next_memreg_header;
	}

	/* No existe una region candidata, tratar de expandir  el heap */
	if (candidate == 0) {
		/*Expandir la pila para que contenga la nueva region candidata.*/
		candidate = expand_heap(heap, size);
		//printf("Trying to expand the heap in %d bytes..\n", size);
	}

	/* No existe region candidata luego de expandir el heap? */
	if (candidate == 0) {
		//printf("Unable to expand the heap in %d bytes!\n", size);
		return 0; /* No se puede asignar memoria! */
	}

	/* La region candidata se encuentra en la lista de regiones disponibles,
	 * primero extraerla de la lista */
	remove_memreg_header(heap->free, candidate);

	//printf("Candidate region for allocating %u bytes: ", size);
	//print_memory_region(candidate);

	/* Marcar la region como usada */
	candidate->used = 1;

	/* La region candidata se debe partir? */
	if (candidate->limit - size >=
		MEMREG_FOOTER_SIZE + MEMREG_MIN_SIZE) {

		/*
		printf("Attempting to split existing memory region [%u]\n",
				(unsigned int)candidate);
		*/

		/* Almacenar el tamaño original de la region candidata */
		candidate_limit = candidate->limit;

		/* Recuperar el pie original, para actualizarlo mas adelante */
		candidate_footer = (memreg_footer_t *)(candidate->base +
								candidate_limit);

		/* Y actualizar el nuevo tamaño de la region candidata */
		candidate->limit = size;

		/* Validacion: Verificar si el footer original es valido. */

		if (candidate_footer->header != candidate
				|| candidate_footer->base != candidate->base) {
			//printf("Error reading old footer!\n");
			return 0;
		}

		/* Crear el nuevo footer para la region de memoria asignada,
		 * usando el tamano solicitado */
		new_footer = (memreg_footer_t *)(candidate->base +  size);
		new_footer->base = candidate->base;
		new_footer->header = candidate;

		//printf("New footer at %u\n", new_footer);


		/*
		 printf("Splitting region ending at %u. Top=%u. Is this the last region?\n",
				(unsigned int)candidate_footer +
				MEMREG_FOOTER_SIZE, heap->top);
		*/

		/* Crear el nuevo header, para la region de memoria restante.
		 * Este header se ubica inmediatamente despues de new_footer */
		new_header = (memreg_header_t * )(candidate->base + size +
				MEMREG_FOOTER_SIZE);
		new_header->base = (unsigned int)new_header + MEMREG_HEADER_SIZE;
		/* El tamaño de esta region libre luego de asignar size :
	     * tamaño original - size - sizeof(new_footer) - sizeof(new_header) */
		new_header->limit = candidate_limit - size - MEMREG_FOOTER_SIZE
							- MEMREG_HEADER_SIZE;

		new_header->used = 0;
		new_header->next_memreg_header = 0;
		new_header->prev_memreg_header = 0;

	/* Validacion : verificar si los calculos para el limite son correctos.
	 * El pie original de la region deberia estar ubicado en
	 * new_header->base + new_header->limit, es decir justo al final de la
	 * nueva region de memoria */
		if (new_header->base + new_header->limit !=
				(unsigned int)candidate_footer) {
			//printf("ERROR! original footer is out of place!. Whatever...");
		}


		/* Actualizar el footer original para que apunte al nuevo header */
		candidate_footer->header = new_header;
					candidate_footer->base = new_header->base;

		/* Validacion: Si la region candidata no es la ultima region
		   definida en el heap, agregar el espacio restante luego
		   de dividirla como una nueva region disponible dentro del heap */
		if (heap->top != (unsigned int)candidate_footer + MEMREG_FOOTER_SIZE){
			//printf("No!\n");
			/* Insertar la region en la lista de regiones disponibles */
			push_front_memreg_header(heap->free, new_header);
		}else {
			//printf("YES!");
			/* La region candidata era la ultima en el heap, Verificar si
			 * es necesario contraer.
			 * Solo se contrae el heap cuando el numero de regiones marcadas
			 * como libres es mayor a FREE_MEMREGS_LIMIT
			 *  */
			/* Ya existen suficientes regiones disponibles? */
			//printf("Have to contract the heap?\n");
			if (heap->free->count > FREE_MEMREGS_LIMIT) {
				//printf("Yes!\n");
				/* Liberar esta region, contrayendo el heap */
				//printf("Contracting the heap from %u ", heap->top);
				heap->top = (unsigned int)new_footer + MEMREG_FOOTER_SIZE;

				/*Borrar el header que se creo */
				new_header->base = 0;
				new_header->limit = 0;
				new_header->used = 0;

				/* Borrar el footer original*/
				candidate_footer->header = 0;
				candidate_footer->base = 0;
				//printf("to %u\n", heap->top);
			}else { //Agregar la region restante a la lista de disponibles
				/* Insertar la region en la lista de regiones disponibles */
				push_front_memreg_header(heap->free, new_header);
			}
		}
	}

	/* Insertar la region asignada en la lista de regiones usadas */
	//push_front_memreg_header(heap->used, candidate);

	return (void *)(candidate->base);

}

/**
 * @brief Libera una region de memoria dentro de un heap.
 * @param heap Heap de la cual se desea liberar la región
 * @param header Apuntador al encabezado de la region de memoria a liberar
 */
void free_from_heap(heap_t * heap, memreg_header_t *header) {

	memreg_header_t * tmp_header;
	memreg_footer_t * tmp_footer;
	memreg_footer_t * footer;

	int joined_left;

	unsigned int header_address;

	if (!memreg_is_valid(heap, header)) {
		return;
	}

	header_address = (unsigned int)header;

	footer = (memreg_footer_t *)(header->base + header->limit);
	if (footer == 0 || footer->header != header
			|| footer->base != header->base) {
		/*printf("Error! Coult not find region footer at %u!\n",
				header->base + header->limit);*/
		return;
	}
	//printf( "Removing region: \n");
	//print_memory_region(header);

	//remove_memreg_header(heap->used, header);

	header->used = 0;


	/* Verificar si esta region se puede fusionar con una region libre
		* que se encuentre inmediatamente antes */

	joined_left = 0;
	if ((unsigned int)header > heap->base) { //Si no es la primera

		/* Apuntar al footer de la region anterior */
		tmp_footer = (memreg_footer_t *)(header_address - MEMREG_FOOTER_SIZE);

		if (tmp_footer  != 0 &&
			tmp_footer->base != 0 &&
			tmp_footer->header != 0) {
			/* Apuntar al header anterior y validar */
			tmp_header = (memreg_header_t *)(tmp_footer->base
											- MEMREG_HEADER_SIZE);
			/* Verificar si la region anterior existe y esta libre*/
			if (tmp_footer->header == tmp_header && tmp_header->used == 0) {
				/* Adicionar esta region a la region anterior */
				tmp_header->limit += MEMREG_FOOTER_SIZE +
									 MEMREG_HEADER_SIZE +
									 header->limit;


				/* Actualizar footer, ahora apunta a tmp_header */
				footer->header = tmp_header;
				footer->base = tmp_header->base;

				/* Borrar datos de tmp_footer */
				tmp_footer->header = 0;
				tmp_footer->base = 0;

				/* Borrar datos de header */
				header->base = 0;
				header->limit = 0;
				header->used = 0;

				/* Finalmente apuntar header a la region anterior*/
				header = tmp_header;

				/* Recordar que la region se fusiono con la anterior */
				joined_left = 1;
			}
		}
	}

	/* Verificar si la region se puede fusionar con una region libre
	* que se encuentre inmediatamente despues */
	if ((unsigned int)footer + MEMREG_FOOTER_SIZE < heap->top) {
		/* Apuntar al header de la region siguiente */
		tmp_header = (memreg_header_t *) ((unsigned int)footer +
										  MEMREG_FOOTER_SIZE);

		/* Existe una region valida y esta libre ? */
		if (memreg_is_valid(heap, tmp_header) && tmp_header->used == 0) {


			/* Quitar la siguiente region de la lista de regiones libres */
			remove_memreg_header(heap->free, tmp_header);

			/* Hallar el pie de la region siguiente */
			tmp_footer = (memreg_footer_t *)(tmp_header->base +
											tmp_header->limit);

			/* Fusionar con la region siguiente */
			header->limit += MEMREG_FOOTER_SIZE +
							 MEMREG_HEADER_SIZE +
							 tmp_header->limit;

			/* Actualizar tmp_footer, ahora apunta a header  */
			tmp_footer->header = header;
			tmp_footer->base = header->base;


			/*Borrar datos de footer */
			footer->header = 0;
			footer->base = 0;

			/* Borrar datos de tmp_header */
			tmp_header->base = 0;
			tmp_header->limit = 0;
			tmp_header->used = 0;

			/* Si la siguiente region era la ultima, contraer el heap */

			if (heap->top == (unsigned int)footer + MEMREG_FOOTER_SIZE) {
				//printf("%u was the last memory region!\n", footer);
				heap->top = (unsigned int)tmp_footer + MEMREG_FOOTER_SIZE;
			}
		}
	}

	/* Si la region no se fusiono con sus region anterior, agregarla a la
	 * lista de regiones libres */
	if (joined_left == 0) {
		push_front_memreg_header(heap->free, header);
	}
}

/**
 * @brief Determina si una region de memoria dada es valida dentro de un
 * heap.
 * @param heap Apuntador al heap en el cual se encuentra la region
 * @param header Apuntador al encabezado de la region a validar
 * @return: 1 = region valida, 0 = region no valida
 */
int memreg_is_valid(heap_t * heap, memreg_header_t *header) {

	memreg_footer_t *footer;
	unsigned int header_address;



	if (heap == 0 || header ==0 || header->limit == 0) {return 0;}

	header_address = (unsigned int)header;

	if (header_address < heap->base ||
		header_address > (heap->top - MEMREG_FOOTER_SIZE - MEMREG_GRANULARITY)){
		return 0; /* Header no valido! */
	}

	footer = (memreg_footer_t *)(header->base + header->limit);

	return (header->base == (unsigned int)header + MEMREG_HEADER_SIZE &&
			footer != 0 &&
			footer->header == header &&
			footer->base == header->base);

}

/**
 * @brief Imprime la informacion de una region de memoria
 * @param header Apuntador al encabezado de la región de memoria
 */
void print_memory_region(memreg_header_t * header) {
	memreg_footer_t * footer;

	/* Primero verificar si la region es valida */
	footer = (memreg_footer_t *)(header->base + header->limit);

	if (footer->header != header || footer->base != header->base) {
		//printf("Region at [%u] not valid!\n");
	}

	/*
	printf("[%u]: (%u, %u, %u, %u, %u) {%u}: (-->%u, %u)\n",
			header,
			(unsigned int)header->base,
			header->limit,
			header->used,
			(unsigned int)header->next,
			(unsigned int)header->prev,
			(header->base + header->limit),
			footer->header,
			footer->base);
	*/
}

/**
 * @brief Imprime el estado de un heap, incluyendo las regiones
 * que se encuentren definidas.
 * @param heap Heap a imprimir
 */
void print_heap(heap_t *heap) {

	memreg_header_t * it;

	/*
	printf("Heap at: %u top=%u limit=%u \n", heap, heap->top, heap->limit);
	printf("Free regions:\n");
printf("[head]: (base, limit, used, next, previous) {foot}: (->header, base)\n");
*/
	for (it = heap->free->head; it != 0;
    it=(memreg_header_t *)it->next_memreg_header) {
		print_memory_region(it);
	}
	/*
	printf("Used regions:\n");
printf("[head]: (base, limit, used, next, previous) {foot}: (->header, base)\n");
	for (it = heap->used->head; it != 0; it=(memreg_header_t *)it->next) {
		print_memory_region(it);
	}
	*/
}

