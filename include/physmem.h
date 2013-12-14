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

/*-------------Se crea todo lo referente a la memory_lista -------------------*/

/**
 * @brief En esta parte creamos una estructura de datos node. Que va a representar cada nodo de nuestra lista enlazada.
 * Tiene un apuntador previous y un apuntador nex para ue sea doblemente enlazada.
 * Tiene un estado (state) que va a representar L o U (Libre o Usado).
 * Además tiene un start que me indica en que parte de la memoria inicia esta región ya sea libre u ocupada y
 * tiene un lenght que me indica el tamaño de esta región que representa el nodo.
 *
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
 * @brief Creamos una estructura de datos memory_list que será la lista enlazada con la que haremos el la gestión de la memoria.
 * Tiene dos nodos, la cabeza (head) y la cola (tail) con los que vamos a referenciar todos los nodos que pertenecen a la lista.
 * También tiene un contador (count) que va a llevar el número de nodos que componen la lista.
 */
typedef struct memory_list {
       node *head;
       node *tail;
       int count;
} memory_list;

/**
 * @brief construye una nueva lista.
*/
static __inline__ memory_list * create_memory_list();

/**
 * @brief Agrega los nodos que representan la memoria disponible en la lista.
 * @param La lista enlazada
 * @param El inicio de la memoria libre.
 * @param El tamanio que tendrá la memoria libre.
*/
static __inline__ void inicializar_memoria_disponible(memory_list *,unsigned int,unsigned int);

/**
 * @brief Crea un nuevo nodo.
 * @param El estado del nodo L o U
 * @param Dirección de memoria que representa el Start.
 * @param El tamaño que tendrá esta memoria libre o usada.
 * @return Devuelve el nodo que fue creado.
 */

static __inline__ node * create_node(char,unsigned int ,unsigned int);

/**
 * @brief Agrega un nodo en el frente de la lista.
 * @param La lista de la memoria.
 * @param El nodo que será agregado en la lista.
 */

static __inline__ void * push_front(memory_list *, node *);

/**
 * @brief Agrega un nodo al final de la lista.
 * @param El nodo que será agregado en la lista..
 */

static __inline__ void * push_back(memory_list *, node * );

/**
 * @brief Quita el nodo que se encuentra al inicio de la lista.
 */

static __inline__ void * pop_front(memory_list *);


/**
 * @brief Quita el nodo que se encuenta el final de la lista.
 */

static __inline__ void * pop_back(memory_list *);

/**
 * @brief Con esta función obtengo el nodo del inicio de la lista.
 * @param La lista de la memoria.
 * @return el nodo que se encuenta en el inicio de la lista
 */

static __inline__ node_iterator head(memory_list *);

/**
 * @brief Con esta función obtengo el siguiente a un nodo dado.
 * @param Nodo del cual quiero saber cual es su siguiente.
 * @return El nodo Siguiente.
 */

static __inline__ node_iterator next(node_iterator);

/**
 * @brief Declaración de la función main implementada en physmem.c.
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
 * @brief Declaración de la función asignar_unidades implementada en physmem.c.
 */
void asignar_unidades(memory_list*,unsigned int);

/**
 * @brief Declaración de la función unirNodosLibres implementada en physmem.c.
 */
void unirNodosLibres(memory_list *,node *);

#endif /* PHYSMEM_H_ */
