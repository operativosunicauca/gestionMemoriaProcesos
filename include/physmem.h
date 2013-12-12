/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene las definiciones relacionadas con las gesti�n
 * de memoria del kernel.
 */

#ifndef PHYSMEM_H_
#define PHYSMEM_H_

 /** @brief Tama�o de la unidad de asignaci�n de memoria  */
#define MEMORY_UNIT_SIZE 4096

/** @brief N�mero de unidades en la memoria disponible */
#define MEMORY_UNITS (memory_length / MEMORY_UNIT_SIZE)

/** @brief Funci�n que redondea una direcci�n de memoria a la direcci�n
 *  m�s cercana por debajo que sea m�ltiplo de MEMORY_UNIT_SIZE */
static __inline__ unsigned int round_down_to_memory_unit(addr) {
	if (addr < 0) { return 0; }

	volatile remainder = addr % MEMORY_UNIT_SIZE;

    return addr - remainder;
}

/** @brief Funci�n que redondea una direcci�n de memoria a la direcci�n
 *  m�s cercana por encima que sea m�ltiplo de MEMORY_UNIT_SIZE */
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
 * @param size Tama�o requerido
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
 * @return Direcci�n de inicio de la unidad en memoria.
 */
char * allocate_unit(void);

/** @brief Busca una regi�n de memoria contigua libre dentro del mapa de bits
 * de memoria.
 * @param length Tama�o de la regi�n de memoria a asignar.
 * @return Direcci�n de inicio de la regi�n en memoria.
 */
char * allocate_unit_region(unsigned int length);

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Direcci�n de memoria dentro del �rea a liberar.
 */
void free_unit(char *addr);

/**
 * @brief Permite liberar una regi�n de memoria.
 * @param start_addr Direcci�n de memoria del inicio de la regi�n a liberar
 * @param length Tama�o de la regi�n a liberar
 */
void free_region(char *start_addr, unsigned int length);

/*
 * @brief Estructura que representa un nodo de una lista para la gesti�n de memoria.
 */
typedef struct {
	/*Define el estado del nodo, 'L' si el nodo esta disponible y 'U' si esta usado.*/
	char state;
	/* Define la unidad donde inicia la regi�n de memoria representada por el nodo. */
	int start;
	/* Define el n�mero de unidades de la regi�n de memoria representada por el nodo. */
	int units;

	/* Apuntadores al nodo anterior y al nodo siguiente del actual. */
	struct memory_node *previous;
	struct memory_node *next;
} memory_node;

/*
 * @brief Estructura que representa una lista de nodos para la gesti�n de memoria disponible.
 * @details Contiene un apuntador a la cabeza y otro a la cola de la lista.
 * */
typedef struct {
	memory_node *mem_head;
	memory_node *mem_tail;
} memory_list;

/* @brief Permite crear una lista para la gesti�n de memoria con un nodo inicial que representa
 * la totalidad del espacio libre.
 * @param start_addr Direcci�n a partir de la cual la memoria est� disponible.
 * @param length Tama�o en bytes de la regi�n de memoria disponible.
 * @return Puntero a una lista para la gesti�n de memoria.
 * */
memory_list *create_memory_list(char * start_addr, unsigned int length);

/* @brief Permite crear un nodo que ser� usado en una lista de gesti�n de memoria.
 * @param state Define el estado del nodo, 'L' si el nodo est� disponible y 'U' si est� usado.
 * @param start Define la unidad donde inicia la regi�n de memoria representada por el nodo.
 * @param units Define el n�mero de unidades de la regi�n de memoria representada por el nodo.
 * @return Puntero a un nodo que ser� usado en una lista de gesti�n de memoria.
 * */
memory_node* create_memory_node(char state, int start, int units);

/* @brief Recorre la lista de gesti�n de memoria al tiempo que imprime la informaci�n
 * de cada nodo.
 * */
void print_list();

/* @brief Recorre de derecha a izquierda la lista de gesti�n de memoria al tiempo que imprime
 * la informaci�n de cada nodo.
 * */
void print_list_right_letf();

#endif /* PHYSMEM_H_ */
