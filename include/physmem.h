/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene las definiciones relacionadas con las gestión
 * de memoria del kernel.
 */

#ifndef PHYSMEM_H_
#define PHYSMEM_H_

 /** @brief Tamaño de la unidad de asignación de memoria  */
#define MEMORY_UNIT_SIZE 4096

/** @brief Número de unidades en la memoria disponible */
#define MEMORY_UNITS (memory_length / MEMORY_UNIT_SIZE)

/** @brief Función que redondea una dirección de memoria a la dirección
 *  más cercana por debajo que sea múltiplo de MEMORY_UNIT_SIZE */
static __inline__ unsigned int round_down_to_memory_unit(addr) {
	if (addr < 0) { return 0; }

	volatile remainder = addr % MEMORY_UNIT_SIZE;

    return addr - remainder;
}


/** @brief Función que redondea una dirección de memoria a la dirección
 *  más cercana por encima que sea múltiplo de MEMORY_UNIT_SIZE */
static __inline__ unsigned int round_up_to_memory_unit(addr) {

	if (addr < 0) { return 0; }

	volatile remainder = addr % MEMORY_UNIT_SIZE;

	if (remainder > 0) {
		return addr + MEMORY_UNIT_SIZE - remainder;
	}

	return addr;

}

/** @brief Tamanio en bytes del HEAP del kernel, 1 MB */
#define KERNEL_HEAP_SIZE 0x100000

/**
 * @brief Solicita asignacion de memoria dentro del heap.
 * @param size Tamaño requerido
 * @return Puntero a la base de la region de memoria asignada, 0 si
 *  		 no es posible asignar memoria.
 */
void * kmalloc(unsigned int size);



/**
 * @brief Solicita liberar una region de memoria dentro del heap
 * @param ptr Puntero a la base de la region de memoria a liberar
 */
void  kfree(void * ptr);

/**
 * @brief Esta rutina inicializa el mapa de bits de memoria,
 * a partir de la informacion obtenida del GRUB.
 */
void setup_memory(void);

/**
 @brief Busca una unidad libre dentro del mapa de bits de memoria.
 * @return Dirección de inicio de la unidad en memoria.
 */
char * allocate_unit(void);

/** @brief Busca una región de memoria contigua libre dentro del mapa de bits
 * de memoria.
 * @param length Tamaño de la región de memoria a asignar.
 * @return Dirección de inicio de la región en memoria.
 */
char * allocate_unit_region(unsigned int length);

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Dirección de memoria dentro del área a liberar.
 */
void free_unit(char *addr);

/**
 * @brief Permite liberar una región de memoria.
 * @param start_addr Dirección de memoria del inicio de la región a liberar
 * @param length Tamaño de la región a liberar
 */
void free_region(char *start_addr, unsigned int length);

/*
 * @brief Representa un nodo de memoria.
 */
typedef struct {
	/*Define el estado del nodo, 'L' si el nodo esta disponible y 'U' si esta usado.*/
	char state;
	/*Define el inicio de la region de memoria que representa el nodo actual.*/
	int start;
	/*Permite determinar la unidad de memoria en la que termina la region
	 * (Unidad Final = start + units ).*/
	int units;

	/*Apuntadores a los nodos anterior y siguiente del nodo actual.*/
	struct memory_node *previous;
	struct memory_node *next;
} memory_node;

/*
 *@brief Representa una lista de nodos de memoria.
 *@details Contiene un apuntador a la cabeza, otro a la cola de la lista
 *y un contador que representa el tamaño de los nodos de la lista.*/
typedef struct {
	memory_node *mem_head;
	memory_node *mem_tail;
	int count;
} memory_list;

static __inline__ memory_node* create_memory_node(char state, int start, int units){
	memory_node *ret;
	ret = (memory_node*) kmalloc( sizeof(memory_node) );
	ret->state = state;
	ret->start = start;
	ret->units = units;
	ret->previous = 0;
	ret->next = 0;

	return ret;
}

static __inline__ memory_list *create_memory_list(char * start_addr, unsigned int length){
	memory_list *mem_list;
	memory_node *mem_node;
	mem_node = create_memory_node('L', (unsigned int)start_addr / MEMORY_UNIT_SIZE, length / MEMORY_UNIT_SIZE);

	mem_list = (memory_list *) kmalloc( sizeof(memory_list) );
	mem_list->mem_head = mem_node;
	mem_list->mem_tail = mem_node;
	mem_list->count = 1;

	return mem_list;
}
/*
static __inline__ void agregar_nodo(memory_list *mem_list, memory_node *mem_node){

	if( mem_list == 0 ) { return; }
	if( mem_list->mem_tail == 0 ) {
		mem_list->mem_head = mem_node;
		mem_list->mem_tail = mem_node;
		mem_list->count = 0;
	}
	else{
		mem_node->previous = mem_list->mem_tail;
		mem_list->mem_tail->next = mem_node;
		mem_list->mem_tail = mem_node;
	}
	mem_list->count++;
}
*/


#endif /* PHYSMEM_H_ */

