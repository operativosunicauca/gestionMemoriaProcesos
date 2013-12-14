/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene la implementacion de las rutinas relacionadas
 * con las gestion de memoria fisica. La memoria se gestiona en unidades
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

/** @brief Variable global del kernel que almacena el inicio de la regi�n
 * de memoria disponible */
unsigned int memory_start;
/** @brief Variable global del kernel que almacena el tama�o en bytes de
 * la memoria disponible */
unsigned int memory_length;

/** @brief M�nima direcci�n de memoria permitida para liberar */
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

	unsigned int mods_end; /* Almacena la direcci�n de memoria final
	del ultimo modulo cargado, o 0 si no se cargaron modulos. */

	printf("Inicio del kernel: %x\n", multiboot_header.kernel_start);
	printf("Fin del segmento de datos: %x\n", multiboot_header.data_end);
	printf("Fin del segmento BSS: %x\n", multiboot_header.bss_end);
	printf("Punto de entrada del kernel: %x\n", multiboot_header.entry_point);

	/* si flags[3] = 1, se especificaron m�dulos que deben ser cargados junto
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
				 * la direcci�n final del modulo a un limite de 4096 */
				mods_end = mod_info->mod_end + (mod_info->mod_end % 4096);
			}
		}
	}

	//printf("Mods end: %u\n", mods_end);

	/* si flags[6] = 1, los campos mmap_length y mmap_addr son validos */

	/* Revisar las regiones de memoria, y extraer la region de memoria
	 * de mayor tamano, maracada como disponible, cuya direcci�n base sea
	 * mayor o igual a la posicion del kernel en memoria.
	 */

	memory_start = 0;
	memory_length = 0;

	free_units = 0;
	total_units = 0;

	/* Suponer que el inicio de la memoria disponible se encuentra
	 * al finalizar el kernel + el fin del HEAP del kernel*/
	allowed_free_start = round_up_to_memory_unit(multiboot_header.bss_end);

	/** Existe un mapa de memoria v�lido creado por GRUB? */
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

			/** Verificar si la regi�n de memoria cumple con las condiciones
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
			/* La region esta marcada como disponible y su direcci�n base
			 * esta por encima de la posicion del kernel en memoria ?*/
			if (mmap->type == 1 &&
					mmap->base_addr_low >= multiboot_header.kernel_start) {
				tmp_start = mmap->base_addr_low;
				tmp_length = mmap->length_low;

				/* Verificar si el kernel se encuentra en esta region */
				if (multiboot_header.bss_end >= tmp_start &&
						multiboot_header.bss_end <= tmp_start + tmp_length) {
					//printf("Kernel is on this region!. Base: %u\n", tmp_start);
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

	/* Existe una regi�n de memoria disponible? */
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

		/* Antes de retornar, establecer la minima direcci�n de memoria
		 * permitida para liberar*/

		//printf("Free units before setting up memory: %d\n", free_units);

		tmp_start = memory_start;
		/* Calcular la direcci�n en la cual finaliza la memoria disponible */
		tmp_end = tmp_start + memory_length;

		/* Redondear el inicio y el fin de la regi�n de memoria disponible a
		 * unidades de memoria */

		tmp_start = round_up_to_memory_unit(tmp_start);
		tmp_end = round_down_to_memory_unit(tmp_end);

		/* Calcular el tama�o de la regi�n de memoria disponible, redondeada
		 * a l�mites de unidades de memoria */
		tmp_length = tmp_end - tmp_start;

		/* Actualizar las variables globales del kernel */
		memory_start = tmp_start;
		memory_length = tmp_length;

		printf("Memory start at %u = %x\n", (memory_start), memory_start);

		/* Marcar la regi�n de memoria como disponible */
		//free_region_original((char*)memory_start, memory_length);

//----------------------------------------------------------------------------------
		kernel_list = create_memory_list();
		inicializar_memoria_disponible(kernel_list,(memory_start), memory_length);


		/* Establecer la direcci�n de memoria a partir
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
 * @return Direcci�n de inicio de la unidad en memoria.
 */
void * allocate_unit() {

	/* Si no existen unidades libres, retornar*/
	if (free_units == 0) {
		//printf("Warning! out of memory!\n");
		return (void *)("Warning! out of memory!");
	}
	/*En este metodo asignar_unidades(kernel_list,1) se implementa
		 * la logica para recorrer la memory_list de unidades de memoria
		 *  y buscar un espacio libre.
		 *  Internamente de serr necesario se parte un nodo libre en dos.*/
	return asignar_unidades(kernel_list,1);
}

/** @brief Busca una regi�n de memoria contigua libre dentro del mapa de bits
 * de memoria.
 * @param length Tama�o de la regi�n de memoria a asignar.
 * @return Direcci�n de inicio de la regi�n en memoria.
 */
void * allocate_unit_region(unsigned int length) {
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
		return (void *)("Warning! out of memory!");
	}

	/*En este segmento se hace llamado a la función
		 * asignar_unidades(kernel_list,length); con el fin de iterar
		 * por la memory_list y encontrar un nodo que tenga al menos
		 * el número de nodos solicitado, de ser necesario, marcar el
		 * nodo como usado, y crear un nuevo nodo en el cual queda
		 * el resto de unidades disponibles.*/
	if(length > 1){
			return asignar_unidades(kernel_list,length);
	}
	else{
			return allocate_unit();
	}
}

/**
 * @brief Permite liberar una unidad de memoria. Recibiendo la dirección de la
 * unidad que vamos a liberar. Se crea una variable start que será la unidad que vamos a liberar.
 * Recorremos mediante un for y utilizando un node_iterator ptr
 * la lista del kernel(kernel_list).Si el estado de ptr es usado y ptr empieza en start entonces se verifica si el tamaño del nodo
 * ptr es igual a uno, en caso de ser así liberamos la memoria, aumentamos las unidades que están libres
 * y enlazamos los nodos que quedan libres; llamando la función UnirNodosLibres.
 *
 *En caso contrario, que tamaño de ptr sea diferente de uno, se crea un nodo auxiliar que iniciara donde comienza ptr
 * ptr el inicio se le aumentara en uno y el tamaño se le restara en uno, se aumentara las unidades libres, luego tenemos
 * que verificar si el nodo anterior de ptr es o no nulo, para enlazar el n en la lista, dado el caso que la unidad a liberar
 * sea la última de la lista o no.
 *
 * En cada caso se deberán unir los nodos libres.

 * @param star_dir-> recibe la dirección que vamos a liberar
 * addr Direcci�n de memoria dentro del �rea a liberar.
 */
void free_unit(unsigned int start_dir) {
	unsigned int start;
	unsigned int unit;

	start = round_down_to_memory_unit(start_dir);
	start = start / 4096;
	//if (start < allowed_free_start) {return;}
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
 * @param start_addr Direccion de memoria del inicio de la región a liberar
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

	/* Almacenar el inicio de la regi�n liberada para una pr�xima asignaci�n */
	//next_free_unit = (unsigned int)start_addr / MEMORY_UNIT_SIZE;
}

/**
 * @brief Solicita asignacion de memoria dentro del heap.
 * @param size Tama�o requerido
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
	//printf("---->ESTADO: %c, INICIO: 0x%u, TAMA�O: 0x%u\n",n->state, n->start, n->length);
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
	//node * n;

	if (l == 0) {
		return 0;
	}

	//n = create_node(unit->state ,unit->start,unit->length);

	if (l->head == 0) { /*Primer elemento en la memory_lista  */
		l->head = unit;
		l->tail = unit;
		l->count = 0;
	}else {
		unit->next = l->head;
		l->head->previous = unit;
		l->head = unit;
	}
	//printf("---->ESTADO: %c, INICIO: 0x%u, TAMA�O: 0x%u\n",unit->state, unit->start, unit->length);

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


/**
 * @brief La prueba simple de memory_list mediante un ciclo for.
 */
void main(){

	printf("<<<<<<KERNEL LIST>>>>>>\n");
	node_iterator ptr;
	for (ptr = head(kernel_list); ptr != 0; ptr= next(ptr)) {
		printf("---->ESTADO: %c, UNIDAD INICIO: %u, TAMANO: %u\n",ptr->state, ptr->start, ptr->length);
	}
}

/**
* @brief Al asignar memoria a un proceso nos llega la lista y
  * la cantidad de unidades  que se necesitan reservar.

  * Se recorre la lista con un apuntador que inicia en la cabecera
  * y termina al final o donde  se pueda asignar a la memoria.

  * Para esto se verifica si al nodo al que apunta se encuentra libre, si esto
  * ocurre el tamaño del nodo deberá corresponder a las unidades requeridas
  * para reservar; y se cambia el estado del nodo a usado.

  * Por el contrario si el tamaño del nodo es mayor a las unidades requeridas,
  * se nombra Ocupado y se enlaza a la lista.

  * Se crea un nuevo nodo que lleva la memoria libre y empieza después del
  * espacio requerido.

  * Los nodos se vinculan a la lista tomando en cuenta que el nodo que se
  * encuentra libre fue el primer nodo de la lista, un nodo distinto al inicial
  * o el ultimo nodo de la lista

  @parametros Memory_list -> La lista que indica las unidades libres de memoria
   * n_units-> Un entero sin signo, corresponde al tamaño de memoria que quiero
   * solicitar
  @return retorna la funcion void recurrido con los cambios realizados
*/

void * asignar_unidades(memory_list *klist,unsigned int nUnits)
{
	node_iterator ptr;
	node *n;
	for (ptr = head(klist); ptr != 0; ptr= next(ptr)) {
		if(ptr->state == 'L'){
			if(ptr->length == nUnits){
				ptr->state = 'U';
				free_units -= nUnits;
				return (void *)ptr->start;
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
					return (void *)new_node->start;
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
					return (void *)new_node->start;
				}
			}
		}
	}
	return (void *)("Warning! out of memory!");
}
/**
@brief  Esta función recibe la vista enlazada y un nodo donde se encuentra la
  * posición libre. En el primer caso la lista que recibe contiene un elemento
  * el cual es el nodo libre, por lo tanto se deja de esta manera.

  * En el siguiente caso se verifica si el nodoAnterior y nodoSiguiente se
  * encuentran libres, es decir si esta libre a la izquierda o a la derecha.

  * En este caso quitamos el nodo de la posiciónActual y el de la derecha.
  * Después se verifica si el nodoSiguiente del actual, es diferente de nulo;
  * entonces se procede a quitar el nodoActual y nodosSguiente, para obtener uno
  * solo con el tamaño libre.

  * Ahora con este nuevo nodo se verifica si el nodoAnterior es diferente de  nulo
  * o se encuentra libre.

  * Luego se procede a unir estos nuevos nodos y el anterior
  * será el nuevo Nodo libre con el tamaño de ambos.

  * Teniendo encuentra si el nodoSiguiente es o no nulo, con lo que se hará
  * los respectivos pasos.

  * Por lo contrario, si el nodoAnterior es igual a nulo entramos a ver si
  * el siguiente es diferente de nulo. En este caso, si el nodoSiguiente fue
  * diferente de nulo y es libre; además si el siguiente del siguiente fue
  * diferente de nulo hacemos la asignación del tamaño al nodo posiciónActual
  * uniéndolo con el nodoSiguiente y libera el nodoSiguiente.
  *
  * En cambio si el nodoSguiente al siguiente es nulo le asigna al nodo posicionActual
  * su tamaño mas el tamaño del nodoSiguiente y lo coloca al final de la lista;
  * En el caso que el nodoSiguiente fuera igual a nulo, ya retornaríamos la lista
  *
  * Luego, si nodoAnterior es diferente de cero, y el estado del nodoAnterior es libre
  * entramos a preguntar si el estado del nodo siguiente es libre y si es asi verificamos
  * si el nodoSiguiente del siguiente es nulo. En caso, de ser así el tamaño de la posición
  * actual será igual al tamaño de la posicionActual más el tamaño del nodoSiguiente.
  *
  * Luego desenlazamos el nodoSiguiente y lo liberamos. En el caso de ser nulo el nodoSiguente al nodoSiguiente,
  * simplemente le adicionamos el tamaño de nodoSiguiente al nodo posicionActual y lo ponemos al final de la lista.

@param klist-> Lista enlazada
*posicionActual-> Posicion libre

@return retorna la función void recurrido con los cambios realizados
*/

/**
@overwrite Ejemplo de 12 Posibles casos que se analizan para liberar unidades de
 *la lista
 *DONDE:
 *  _ : Esta vacio
 *  *: Indica que se acaba de liberar
 *  U: Indica la existencia de mas nodos
 *  L: Indica que el nodo se encuentra Libre
 *     _ -L*- _
 *     _ -L*- L
 *     U -L*- L
 *     U -L-  L*
 *     L* -L- U
 *     L -L*- U
 *     U -L*-L- U
 *     U -L-L*- U
 *     U -L-L*- L
 *     U -L-L*- L- U
 *     L -L*-L- U
 @end_overwrite

*/

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
