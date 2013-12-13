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
void allocate_unit();

/** @brief Busca una región de memoria contigua libre dentro del mapa de bits
 * de memoria.
 * @param length Tamaño de la región de memoria a asignar.
 * @return Dirección de inicio de la región en memoria.
 */
void allocate_unit_region(unsigned int length);

/**
 * @brief Permite liberar una región de memoria.
 * @param start_addr Dirección de memoria del inicio de la región a liberar
 * @param length Tamaño de la región a liberar
 */
void free_region(unsigned int start_addr, unsigned int length);

/**
 * @brief Estructura que permite crear unidades o regiones
 */
typedef struct node {
	char state;
	unsigned int start;
	unsigned int length;
       struct node *next;
       struct node *previous;
}node;


typedef node * node_iterator;

/**
 * @brief Estructura que maneja las unidades o regiones.
 */
typedef struct memory_list {
       node *head;
       node *tail;
       int count;
} memory_list;

/**
 * @brief Permite crear la memory_list vacia.
 */
static __inline__ memory_list * create_memory_list();

/**
 * @brief Permite crear la memory_list con el numero de undades disponobles para asignar.
 * @param start_addr Dirección de memoria del inicio de la región libre
 * @param length Tamaño region o numero de unidades libres.
 */
static __inline__ void inicializar_memoria_disponible(memory_list *,unsigned int,unsigned int);

/**
 * @brief Permite crear una unidad o region.
 * @param state estado de la unidad o region libre:'L' o usada: 'U'.
 * @param start Direccion de la unidad o region trabajado.
 * @param length Tamaño de la unidad o region trabajado.
 */
static __inline__ node * create_node(char,unsigned int ,unsigned int);

/**
 * @brief Permite asignar al frente una unidad o región nueva a la memory_list.
 * @param memory_list lista de memoria.
 * @param nodo Unidad o región asignada.
 */
static __inline__ void * push_front(memory_list *, node *);

/**
 * @brief Permite asignar al final una unidad o región nueva a la memory_list.
 * @param memory_list lista de memoria.
 * @param nodo Unidad o región asignada.
 */
static __inline__ void * push_back(memory_list *, node * );

/**
 * @brief Permite actualizar la memory_list removiendo al frente
 * una unidad o region que ha sido fusionada.
 * @param memory_list lista de memoria.
 */
static __inline__ void * pop_front(memory_list *);

/**
 * @brief Permite actualizar la memory_list removiendo al final
 * una unidad o region que ha sido fusionada.
 * @param memory_list lista de memoria.
 */
static __inline__ void * pop_back(memory_list *);

/**
 * @brief Permite Extraer la cabeza de la lista para cualquier caso que
 * requiera ser iterada.
 * @param memory_list lista de memoria.
 */
static __inline__ node_iterator head(memory_list *);

/**
 * @brief Permite permite recorrer la memory_list
 * a partir de una unidad o region.
 * @param memory_list lista de memoria.
 */
static __inline__ node_iterator next(node_iterator);

/**
 * @brief Permite imprimir los datos de la memory_list.
 */
void main();

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Dirección de memoria dentro del área a liberar.
 */
void free_unit( unsigned int);

/**
 * @brief Permite liberar una región de memoria.
 * @param start_addr Dirección de memoria del inicio de la región a liberar
 * @param length Tamaño de la región a liberar
 */
void free_region(unsigned int start_addr, unsigned int length);

/**
 * @brief Permite asignar unidades en la memory_list.
 * @param memory_list lista de memoria.
 * @param length Tamaño de unidades a asignar.
 */
void asignar_unidades(memory_list*,unsigned int);

/**
 * @brief Permite fusionar nodos libres y adyacentes.
 * @param memory_list lista de memoria.
 * @param nodo unidad o región que se acaba de liberar.
 */
void unirNodosLibres(memory_list *,node *);

#endif /* PHYSMEM_H_ */
