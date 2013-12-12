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
 * @brief Estructura que representa un nodo de una lista para la gestión de memoria.
 */
typedef struct {
	/*Define el estado del nodo, 'L' si el nodo esta disponible y 'U' si esta usado.*/
	char state;
	/* Define la unidad donde inicia la región de memoria representada por el nodo. */
	int start;
	/* Define el número de unidades de la región de memoria representada por el nodo. */
	int units;

	/* Apuntadores al nodo anterior y al nodo siguiente del actual. */
	struct memory_node *previous;
	struct memory_node *next;
} memory_node;

/*
 * @brief Estructura que representa una lista de nodos para la gestión de memoria disponible.
 * @details Contiene un apuntador a la cabeza y otro a la cola de la lista.
 * */
typedef struct {
	memory_node *mem_head;
	memory_node *mem_tail;
} memory_list;

/* @brief Permite crear una lista para la gestión de memoria con un nodo inicial que representa
 * la totalidad del espacio libre.
 * @param start_addr Dirección a partir de la cual la memoria está disponible.
 * @param length Tamaño en bytes de la región de memoria disponible.
 * @return Puntero a una lista para la gestión de memoria.
 * */
memory_list *create_memory_list(char * start_addr, unsigned int length);

/* @brief Permite crear un nodo que será usado en una lista de gestión de memoria.
 * @param state Define el estado del nodo, 'L' si el nodo está disponible y 'U' si está usado.
 * @param start Define la unidad donde inicia la región de memoria representada por el nodo.
 * @param units Define el número de unidades de la región de memoria representada por el nodo.
 * @return Puntero a un nodo que será usado en una lista de gestión de memoria.
 * */
memory_node* create_memory_node(char state, int start, int units);

/* @brief Recorre la lista de gestión de memoria al tiempo que imprime la información
 * de cada nodo.
 * */
void print_list();

/* @brief Recorre de derecha a izquierda la lista de gestión de memoria al tiempo que imprime
 * la información de cada nodo.
 * */
void print_list_right_letf();

#endif /* PHYSMEM_H_ */
