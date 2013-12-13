/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene la implementación de las rutinas relacionadas
 * con las gestión de memoria física. La memoria se gestiona en unidades
 * de 4096 bytes.
 */

#include <kmm.h>
#include <physmem.h>
#include <multiboot.h>
#include <stdio.h>
#include <stdlib.h>

memory_list *kernel_list;
node *kernel_node;

/* Referencia a la variable global kernel_keap */
/** @brief Variable global para el heap. Sobre este heap actua kmalloc. */
static heap_t * kernel_heap;

/** @brief Variable global del kernel que almacena el inicio del
 * heap del kernel */
unsigned int kernel_heap_start;

/** @brief Siguiente unidad disponible en el mapa de bits */
unsigned int next_free_unit;

/** @brief Numero de marcos libres en la memoria */
int free_units;

/** @brief Numero total de unidades en la memoria */
int total_units;

/** @brief Marco inicial de las unidades  disponibles en memoria */
unsigned int base_unit;

/** @brief Variable global del kernel que almacena el inicio de la región
 * de memoria disponible */
unsigned int memory_start;
/** @brief Variable global del kernel que almacena el tamaño en bytes de
 * la memoria disponible */
unsigned int memory_length;

/** @brief Mínima dirección de memoria permitida para liberar */
unsigned int allowed_free_start;

/**
 * @brief Esta rutina inicializa el mapa de bits de memoria,
 * a partir de la informacion obtenida del GRUB.
 */
void setup_memory(void){

	extern multiboot_header_t multiboot_header;
	extern unsigned int multiboot_info_location;

	/* Variables temporales para hallar la region de memoria disponible */
	unsigned int tmp_start;
	unsigned int tmp_length;
	unsigned int tmp_end;
	int mod_count;
	multiboot_info_t * info = (multiboot_info_t *)multiboot_info_location;

	int i;

	unsigned int mods_end; /* Almacena la dirección de memoria final
	del ultimo modulo cargado, o 0 si no se cargaron modulos. */

	printf("Inicio del kernel: %x\n", multiboot_header.kernel_start);
	printf("Fin del segmento de datos: %x\n", multiboot_header.data_end);
	printf("Fin del segmento BSS: %x\n", multiboot_header.bss_end);
	printf("Punto de entrada del kernel: %x\n", multiboot_header.entry_point);

	/* si flags[3] = 1, se especificaron módulos que deben ser cargados junto
	 * con el kernel*/

	mods_end = 0;

	if (test_bit(info->flags, 3)) {
		mod_info_t * mod_info;
		/*
		printf("Modules available!. Start: %u Count: %u\n", info->mods_addr,
				info->mods_count);
		 */
		for (mod_info = (mod_info_t*)info->mods_addr, mod_count=0;
				mod_count <info->mods_count;
				mod_count++, mod_info++) {
			/*
			printf("[%d] start: %u end: %u cmdline: %s \n", mod_count,
					mod_info->mod_start, mod_info->mod_end,
					mod_info->string);*/
			if (mod_info->mod_end > mods_end) {
				/* Los modulos se redondean a limites de 4 KB, redondear
				 * la dirección final del modulo a un limite de 4096 */
				mods_end = mod_info->mod_end + (mod_info->mod_end % 4096);
			}
		}
	}

	/* si flags[6] = 1, los campos mmap_length y mmap_addr son validos */

	/* Revisar las regiones de memoria, y extraer la region de memoria
	 * de mayor tamano, maracada como disponible, cuya dirección base sea
	 * mayor o igual a la posicion del kernel en memoria.
	 */

	memory_start = 0;
	memory_length = 0;

	free_units = 0;
	total_units = 0;

	/* Suponer que el inicio de la memoria disponible se encuentra
	 * al finalizar el kernel + el fin del HEAP del kernel*/
	allowed_free_start = round_up_to_memory_unit(multiboot_header.bss_end);

	/** Existe un mapa de memoria válido creado por GRUB? */
	if (test_bit(info->flags, 6)) {
		memory_map_t *mmap;

		/*printf ("mmap_addr = 0x%x, mmap_length = 0x%x\n",
			   (unsigned) info->mmap_addr, (unsigned) info->mmap_length);*/
		for (mmap = (memory_map_t *) info->mmap_addr;
				(unsigned int) mmap < info->mmap_addr + info->mmap_length;
				mmap = (memory_map_t *) ((unsigned int) mmap
						+ mmap->entry_size
						+ sizeof (mmap->entry_size))) {
			printf (" size = 0x%x, base_addr = 0x%x%x,"
					" length = 0x%x%x, type = 0x%x\n",
					mmap->entry_size,
					mmap->base_addr_high,
					mmap->base_addr_low,
					mmap->length_high,
					mmap->length_low,
					mmap->type);

			/** Verificar si la región de memoria cumple con las condiciones
			 * para ser considerada "memoria disponible".
			 *
			 * Importante: Si se supone un procesador de 32 bits, los valores
			 * de la parte alta de base y length (base_addr_high y
			 * length_high) son cero. Por esta razon se pueden ignorar y solo
			 * se usan los 32 bits menos significativos de base y length.
			 *
			 * Para que una region de memoria sea considerada "memoria
			 * disponible", debe cumplir con las siguientes condiciones:
			 *
			 * - Estar ubicada en una posicion de memoria mayor o igual que
			 * 	1 MB.
			 * - Tener su atributo 'type' en 1 = memoria disponible.
			 * */
			/* La region esta marcada como disponible y su dirección base
			 * esta por encima de la posicion del kernel en memoria ?*/
			if (mmap->type == 1 &&
					mmap->base_addr_low >= multiboot_header.kernel_start) {
				tmp_start = mmap->base_addr_low;
				tmp_length = mmap->length_low;

				/* Verificar si el kernel se encuentra en esta region */
				if (multiboot_header.bss_end >= tmp_start &&
						multiboot_header.bss_end <= tmp_start + tmp_length) {
					/* El kernel se encuentra en esta region. Tomar el inicio
					 * de la memoria disponible en la posicion en la cual
					 * finaliza el kernel
					 */
					tmp_start = multiboot_header.bss_end;

					/* Ahora verificar si ser cargaron modulos junto con el
					 * kernel. Estos modulos se cargan en regiones continuas
					 * al kernel.
					 * Si es asi, la nueva posicion inicial de la memoria
					 * disponible es la posicion en la cual terminan los modulos
					 * */
					if (mods_end > 0 &&
							mods_end >= tmp_start &&
							mods_end <= tmp_start + tmp_length) {
						//printf("Adding module space...\n");
						tmp_start = mods_end;
					}
					/* Restar al espacio disponible.*/
					tmp_length -= tmp_start - mmap->base_addr_low;
					if (tmp_length > memory_length) {
						memory_start = tmp_start;
						memory_length = tmp_length; /* Tomar el espacio */
					}
				}else {
					/* El kernel no se encuentra en esta region, verificar si
					 * su tamano es mayor que la region mas grande encontrada
					 * hasta ahora
					 */
					if (tmp_length > memory_length) {
						memory_start = tmp_start;
						memory_length = tmp_length; /* Tomar el espacio */
					}
				}
			}
		} //endfor
	}

	/* Inicializar en cero el heap del kernel */
	kernel_heap = 0;

	printf("Memory start at %u = %x\n", memory_start, memory_start);

	/* Existe una región de memoria disponible? */
	if (memory_start > 0 && memory_length > 0) {

		/* Se encontro memoria disponible? configurar el heap del kernel
		 * justo al inicio de la memoria disponible!
		 */
		kernel_heap_start = memory_start;

		/* Avanzar la direccion de memoria disponible, tanto como
		 * ocupa el heap del kernel */
		memory_start = memory_start + KERNEL_HEAP_SIZE;

		/* Restar el tamanio del heap a la cantidad de memoria disponible */
		memory_length = memory_length - KERNEL_HEAP_SIZE;

		/* Configurar el heap del kernel */
		kernel_heap = setup_heap((void*)kernel_heap_start, KERNEL_HEAP_SIZE);

		printf("Kernel heap at: 0x%x Size: %d KB\n", kernel_heap->base,
				kernel_heap->limit / 1024);

		/* Antes de retornar, establecer la minima dirección de memoria
		 * permitida para liberar*/

		//printf("Free units before setting up memory: %d\n", free_units);

		tmp_start = memory_start;
		/* Calcular la dirección en la cual finaliza la memoria disponible */
		tmp_end = tmp_start + memory_length;

		/* Redondear el inicio y el fin de la región de memoria disponible a
		 * unidades de memoria */

		tmp_start = round_up_to_memory_unit(tmp_start);
		tmp_end = round_down_to_memory_unit(tmp_end);

		/* Calcular el tamaño de la región de memoria disponible, redondeada
		 * a límites de unidades de memoria */
		tmp_length = tmp_end - tmp_start;

		/* Actualizar las variables globales del kernel */
		memory_start = tmp_start;
		memory_length = tmp_length;

		printf("Memory start at %u = %x\n", (memory_start), memory_start);

		/* Crear la memory_list con un unico nodo disponible*/
		kernel_list = create_memory_list();
		inicializar_memoria_disponible(kernel_list,(memory_start), memory_length);


		/* Establecer la dirección de memoria a partir
		 * de la cual se puede liberar memoria */
		allowed_free_start = memory_start;
		next_free_unit = allowed_free_start / MEMORY_UNIT_SIZE;

		total_units = free_units;
		//base_unit = next_free_unit;

		printf("Available memory at: 0x%x units: %d Total memory: %d\n",
				memory_start, total_units, memory_length);

	}
}

/**
 @brief Busca una unidad libre dentro del mapa de bits de memoria.
 * @return Dirección de inicio de la unidad en memoria.
 */
void allocate_unit() {

	/* Si no existen unidades libres, retornar*/
	if (free_units == 0) {
		printf("Warning! out of memory!\n");
		return;
	}
	/*En este metodo asignar_unidades(kernel_list,1) se implementa
	 * la logica para recorrer la memory_list de unidades de memoria
	 *  y buscar un espacio libre.
	 *  Internamente de serr necesario se parte un nodo libre en dos.*/
		asignar_unidades(kernel_list,1);
	return;
}

/** @brief Busca una región de memoria contigua libre dentro del mapa de bits
 * de memoria.
 * @param length Tamaño de la región de memoria a asignar.
 * @return Dirección de inicio de la región en memoria.
 */
void allocate_unit_region(unsigned int length) {
	unsigned int unit;
	unsigned int unit_count;
	unsigned int i;
	int result;

	unit_count = (length / MEMORY_UNIT_SIZE);

	if (length % MEMORY_UNIT_SIZE > 0) {
		unit_count++;
	}

	if (free_units < unit_count) {
		//printf("Warning! out of memory!\n");
		return;
	}

	/*En este segmento se hace llamado a la función
	 * asignar_unidades(kernel_list,length); con el fin de iterar
	 * por la memory_list y encontrar un nodo que tenga al menos
	 * el número de nodos solicitado, de ser necesario, marcar el
	 * nodo como usado, y crear un nuevo nodo en el cual queda
	 * el resto de unidades disponibles.*/
	if(length > 1){
			asignar_unidades(kernel_list,length);
	}
	else{
			allocate_unit();
	}
	return;
}

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Dirección de memoria dentro del área a liberar.
 */
void free_unit(unsigned int start_dir) {
	unsigned int start;
	unsigned int unit;

	start = round_down_to_memory_unit(start_dir);
	if (start < allowed_free_start) {return;}
	start = start / 4096;
	/* Se busca la unidad en la memory_list deunidades y se marca
	 * como libre 'L'.
	 * Se invoca el metodo unirNodosLibres(kernel_list,nodo_libre);
	 * ya que de ser necesario se fusionarán nodos del amemory_list.
	 */
	node_iterator ptr;
	for (ptr = head(kernel_list); ptr != 0; ptr= next(ptr)) {
		if(ptr->state == 'U' && ptr->start == start){
			if(ptr->length == 1){
				ptr->state = 'L';
				free_units = free_units + 1;
				unirNodosLibres(kernel_list,ptr);
			}
			else{
				node *n = create_node('L',ptr->start,1);
				ptr->start = ptr->start + 1;
				ptr->length = ptr->length - 1;
				free_units = free_units + 1;
				if(ptr->previous != 0){
					ptr->previous->next = n;
					n->previous = ptr->previous;
					n->next = ptr;
					ptr->previous = n;
					unirNodosLibres(kernel_list,ptr->previous);
				}
				else{
					push_front(kernel_list,n);
					unirNodosLibres(kernel_list,ptr->previous);
				}
			}
		}
	}
}
/**
 * @brief Permite liberar una región de memoria. Primero declaramos 2 unsigned int (start y end)
 * a statr le asignamos la dirección de memoria donde empieza la región de memoria que vamos a liberar,
 *   luego verificamos que la región empiece dentro de ona posición permitida. A end se le asignará el número
 *   donde termina esta región y después en un ciclo for que va desde start hasta end se irá liberando
 *   cada unidad de memoria.
 * @param start_addr Dirección de memoria del inicio de la región a liberar
 * @param length Tamaño de la región a liberar
 */
void free_region(unsigned int start_addr, unsigned int length){
	unsigned int start;
	unsigned int end;

	start = round_down_to_memory_unit((unsigned int)start_addr);

	if (start < allowed_free_start) {return;}

	end = start + (length*MEMORY_UNIT_SIZE);

	for (; start < end; start += MEMORY_UNIT_SIZE) {
		free_unit(start);
	}
}

/**
 * @brief Solicita asignacion de memoria dentro del heap.
 * @param size Tamaño requerido
 * @return Puntero a la base de la region de memoria asignada, 0 si
 *  		 no es posible asignar memoria.
 */
void * kmalloc(unsigned int size) {
	return alloc_from_heap(kernel_heap, size);
}

/**
 * @brief Solicita liberar una region de memoria dentro del heap
 * @param ptr Puntero a la base de la region de memoria a liberar
 */
void  kfree(void * ptr) {
	memreg_header_t * header;

	header = (memreg_header_t *)((unsigned int) ptr - MEMREG_HEADER_SIZE);

	free_from_heap(kernel_heap, header);
}

/*-----------------se implementa todo lo referente a la memory_lista-------------------*/

static __inline__ memory_list * create_memory_list() {
	memory_list *ret;
	ret = (memory_list *) kmalloc(sizeof(memory_list));

	ret->head = 0;
	ret->tail = 0;
	ret->count = 0;
	return ret;
}

static __inline__ void  inicializar_memoria_disponible(memory_list *kernel_list,unsigned int start, unsigned int length) {

	node *n;
	n = create_node('L',start / MEMORY_UNIT_SIZE,length / MEMORY_UNIT_SIZE);
	push_front(kernel_list,n);
	free_units = n->length;
}

static __inline__ node *
create_node(char state,unsigned int start,unsigned int length) {
	node * ret;

	ret = (node *)kmalloc(sizeof(node));
	ret->state = state;
	ret->start = start;
	ret->length = length;
	ret->next = 0;
	ret->previous = 0;
	return ret;
}

static __inline__ void *
push_front(memory_list *l, node * unit) {
	if (l == 0) {
		return 0;
	}

	if (l->head == 0) { /*Primer elemento en la memory_lista  */
		l->head = unit;
		l->tail = unit;
		l->count = 0;
	}else {
		unit->next = l->head;
		l->head->previous = unit;
		l->head = unit;
	}
	//printf("---->ESTADO: %c, INICIO: 0x%u, TAMAÑO: 0x%u\n",unit->state, unit->start, unit->length);

	l->count++;

	return 0;
}

static __inline__ void *
push_back(memory_list *l, node * unit) {
	node * n;

	if (l ==0) {return 0;}

	n = create_node(unit->state ,unit->start,unit->length);

	if (l->tail == 0) { /*Primer elemento en la memory_lista */
		l->head = n;
		l->tail = n;
		l->count = 0;
	}else {
		n->previous = l->tail;
		l->tail->next = n;
		l->tail = n;
	}
	l->count++;

	return 0;
}

static __inline__ void *
pop_front(memory_list *l) {
	node *ret;

	if (l == 0) {return (void*)0;}

	ret = l->head;

	if (l->head == 0) {return (void*)0;}

	l->head = l->head->next;

	if (l->head == 0) {
		l->tail = 0;
	}else {
		l->head->previous = 0;
		kfree(ret);
	}

	l->count--;

	return (void*)0;
}

static __inline__ void *
pop_back(memory_list *l) {
	node *ret;

	if (l ==0) {return 0;}

	ret = l->tail;

	if (ret == 0) {return ret;}

	l->tail = l->tail->previous;

	if (l->tail == 0) { /* Un solo elemento? */
		l->head = 0;
	}else {
		l->tail->next = 0;
		kfree(ret);
	}

	l->count--;

	return (void*)0;
}

static __inline__ node_iterator head(memory_list *l) {
	if (l ==0) {return 0;}
	return l->head;
}

static __inline__ node_iterator next(node_iterator it) {
	if (it == 0) {return 0;}
	return it->next;
}

/*la prueba simple de memory_listas*/
void main(){

	printf("<<<<<<KERNEL LIST>>>>>>\n");
	node_iterator ptr;
	for (ptr = head(kernel_list); ptr != 0; ptr= next(ptr)) {
		printf("---->ESTADO: %c, UNIDAD INICIO: %u, TAMANO: %u\n",ptr->state, ptr->start, ptr->length);
	}
}

void asignar_unidades(memory_list *klist,unsigned int nUnits)
{
	node_iterator ptr;
	node *n;
	for (ptr = head(klist); ptr != 0; ptr= next(ptr)) {
		if(ptr->state == 'L'){
			if(ptr->length == nUnits){
				ptr->state = 'U';
				free_units -= nUnits;
				return;
			}
			else if(ptr->length > nUnits){
				node *new_node = create_node('U',ptr->start,nUnits);
				node *old_node = create_node(ptr->state,ptr->start+nUnits,ptr->length - nUnits);
				free_units -= nUnits;
				if(ptr->previous==0){
					if(ptr->next==0){
						pop_front(klist);
						push_back(klist,new_node);
						push_back(klist,old_node);
					}
					else{
						pop_front(klist);
						push_front(klist,old_node);
						push_front(klist,new_node);
					}
					return;
				}
				else{
					free_units -= nUnits;
					if(ptr->next==0){
							pop_back(kernel_list);
							push_back(kernel_list,new_node);
							push_back(kernel_list,old_node);
					}
					else{

						n = ptr->next;
						new_node->next = old_node;
						old_node->previous = new_node;
						old_node->next = n;
						n->previous = old_node;
						ptr->previous->next = new_node;
						new_node->previous = ptr->previous;
						ptr->previous = 0;
						ptr->next = 0;
						kfree(ptr);
					}
					return;
				}
			}
		}
	}
}

void unirNodosLibres(memory_list *klist , node* posicionActual )
{

	node *infoActual = posicionActual;
	if(infoActual->state=='L'){ //Solo si esta si es un nodo q se posiciona como libre..
		node *nodoAnterior = posicionActual->previous;
		node *nodoSiguiente = posicionActual->next;
		if (nodoAnterior == 0 && nodoSiguiente == 0) {
			return; //porq es el unico en la lista, no hay nada q hacer
		}
		if( (nodoAnterior->state== 'L') && ((nodoSiguiente)->state== 'L') ){//si el nodo anterior y el siguiente tienen estado L


			nodoAnterior->length = (nodoAnterior->length)+(posicionActual->length) +(nodoSiguiente->length );
			if((posicionActual->next)->next != 0){
				nodoAnterior->next = (posicionActual->next)->next; //...U-U-L-L'-L-U
				(nodoSiguiente->next)->previous = posicionActual->previous;//...U-U-L''-?..
				posicionActual->previous = 0;
				posicionActual->next = 0;
				nodoSiguiente->previous = 0;
				nodoSiguiente->next = 0;
				kfree(nodoSiguiente);
				kfree(posicionActual);
			}
			else{
				pop_back(klist);
				pop_back(klist);
			}
			return;
		}

		if(nodoAnterior!=0 && (nodoAnterior->state== 'L')){
			//  U-U-L-L'-?-U ...
			if(nodoSiguiente!=0){

				// si es algo como U-U-L-L'-U ... Donde L' es el q se va a liberar
				//entonces dejamos el q nodo de la posicionActual y quitamos el otro
				//...U-U-L''-U...
				nodoAnterior->next = nodoSiguiente;
				((nodoSiguiente)->previous) = nodoAnterior;

				nodoAnterior->length = ((nodoAnterior->length)+(posicionActual->length) );

				nodoAnterior->next = nodoSiguiente;
				((nodoSiguiente)->previous) = nodoAnterior;
				posicionActual->previous = 0;
				posicionActual->next = 0;
				kfree(posicionActual);

				return;
			}
			else{
				nodoAnterior->length = ((nodoAnterior->length)+(posicionActual->length));
				pop_back(klist);
				return;
			}
		}

		if(nodoAnterior==0 ){
				//  -L'-?-? ...
			if(nodoSiguiente != 0){
				if((nodoSiguiente)->state== 'L' ){
					// -L'-L-?- ...
	           		if((nodoSiguiente->next)!=0){

						posicionActual->length = ((posicionActual->length)+(nodoSiguiente->length) );
						((nodoSiguiente->next))->previous = posicionActual;
						posicionActual->next = nodoSiguiente->next;
						nodoSiguiente->next = 0;
						nodoSiguiente->previous = 0;
						kfree(nodoSiguiente);
					}else{
						posicionActual->length = ((posicionActual->length)+(nodoSiguiente->length) );
						pop_back(klist);
					}
	           		return;
				}
			}else{
					// -L'-
					return;
			}

		}
		if(nodoAnterior!=0 && nodoAnterior->state!= 'L'){

			if((nodoSiguiente)->state== 'L'){
				if(nodoSiguiente->next != 0 ){

								posicionActual->length = ((posicionActual->length)+(nodoSiguiente->length) );
								posicionActual->next = nodoSiguiente->next;
								nodoSiguiente->next->previous = posicionActual;
								nodoSiguiente->previous = 0;
								nodoSiguiente->next = 0;
								kfree(nodoSiguiente);
				}
				else{
					posicionActual->length = ((posicionActual->length)+(nodoSiguiente->length) );
					pop_back(klist);
				}
			}

		}

	}
}
