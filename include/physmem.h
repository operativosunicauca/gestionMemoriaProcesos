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
void allocate_unit();

/** @brief Busca una regi�n de memoria contigua libre dentro del mapa de bits
 * de memoria.
 * @param length Tama�o de la regi�n de memoria a asignar.
 * @return Direcci�n de inicio de la regi�n en memoria.
 */
void allocate_unit_region(unsigned int length);

/**
 * @brief Permite liberar una regi�n de memoria.
 * @param start_addr Direcci�n de memoria del inicio de la regi�n a liberar
 * @param length Tama�o de la regi�n a liberar
 */
void free_region(unsigned int start_addr, unsigned int length);

/*-------------Se crea todo lo referente a la memory_lista -------------------*/

/**
 * @brief En esta parte creamos una estructura de datos node. Que va a representar cada nodo de nuestra lista enlazada.
 * Tiene un apuntador previous y un apuntador nex para ue sea doblemente enlazada.
 * Tiene un estado (state) que va a representar L o U (Libre o Usado).
 * Adem�s tiene un start que me indica en que parte de la memoria inicia esta regi�n ya sea libre u ocupada y
 * tiene un lenght que me indica el tama�o de esta regi�n que representa el nodo.
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
 * @brief Creamos una estructura de datos memory_list que ser� la lista enlazada con la que haremos el la gesti�n de la memoria.
 * Tiene dos nodos, la cabeza (head) y la cola (tail) con los que vamos a referenciar todos los nodos que pertenecen a la lista.
 * Tambi�n tiene un contador (count) que va a llevar el n�mero de nodos que componen la lista.
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
 * @param El tamanio que tendr� la memoria libre.
*/
static __inline__ void inicializar_memoria_disponible(memory_list *,unsigned int,unsigned int);

/**
 * @brief Crea un nuevo nodo.
 * @param El estado del nodo L o U
 * @param Direcci�n de memoria que representa el Start.
 * @param El tama�o que tendr� esta memoria libre o usada.
 * @return Devuelve el nodo que fue creado.
 */

static __inline__ node * create_node(char,unsigned int ,unsigned int);

/**
 * @brief Agrega un nodo en el frente de la lista.
 * @param La lista de la memoria.
 * @param El nodo que ser� agregado en la lista.
 */

static __inline__ void * push_front(memory_list *, node *);

/**
 * @brief Agrega un nodo al final de la lista.
 * @param El nodo que ser� agregado en la lista..
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
 * @brief Con esta funci�n obtengo el nodo del inicio de la lista.
 * @param La lista de la memoria.
 * @return el nodo que se encuenta en el inicio de la lista
 */

static __inline__ node_iterator head(memory_list *);

/**
 * @brief Con esta funci�n obtengo el siguiente a un nodo dado.
 * @param Nodo del cual quiero saber cual es su siguiente.
 * @return El nodo Siguiente.
 */

static __inline__ node_iterator next(node_iterator);

/**
 * @brief Declaraci�n de la funci�n main implementada en physmem.c.
 */
void main();

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Direcci�n de memoria dentro del �rea a liberar.
 */
void free_unit( unsigned int);

/**
 * @brief Permite liberar una regi�n de memoria.
 * @param start_addr Direcci�n de memoria del inicio de la regi�n a liberar
 * @param length Tama�o de la regi�n a liberar
 */
void free_region(unsigned int start_addr, unsigned int length);

/**
 * @brief Declaraci�n de la funci�n asignar_unidades implementada en physmem.c.
 */
void asignar_unidades(memory_list*,unsigned int);

/**
 * @brief Declaraci�n de la funci�n unirNodosLibres implementada en physmem.c.
 */
void unirNodosLibres(memory_list *,node *);

#endif /* PHYSMEM_H_ */
