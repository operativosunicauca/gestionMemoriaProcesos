/**
 * @file
 * @ingroup kernel_code
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 *
 * @brief Contiene la definicion de las primitivas para la gestion de
 * memoria.
 */

#ifndef MM_H_
#define MM_H_

#include <generic_linked_list.h>

/** @brief Limite inferior de regiones de memoria libres. Por encima de este
 * valor la pila se contrae cuando se divide la region libre que se
 * encuentra en el tope del heap. */
#define FREE_MEMREGS_LIMIT 64

/** @brief Minimo numero de bytes permitido para una region de memoria . Debe
 * ser potencia de 2 */
#define MEMREG_GRANULARITY 4 

/** @brief Numero de bytes preferidos para solicitar. Debe ser potencia de 2, y
multiplo de MEMREG_GRANULARITY  */
#define AVG_ALLOC_SIZE 0x100 

/* Estructuras de datos para gestionar la memoria disponible */
struct memreg_header;

/** @brief Encabezado de una region de memoria */
typedef struct memreg_header {
	/** @brief Inicio del �rea de datos */
	unsigned int base;
	/** @brief Tama�o de la region de memoria */
	unsigned int limit;
	/** @brief 1 = region asignada, 0 = region libre */
	int used;
	DEFINE_GENERIC_LIST_LINKS(memreg_header); /*Links genericos */
}memreg_header_t;

/** @brief Definici�n de las primitivas para gestionar listas de tipo
 * memreg_header_t*/
DEFINE_GENERIC_LIST_TYPE(memreg_header_t, memreg_header);

/**  @brief Funci�n para comparar dos regiones de memoria */
int compare_memreg_header_t(memreg_header_t * , memreg_header_t *);

/** @brief Pie de una region de memoria*/
typedef struct memreg_footer {
	/** @brief Inicio del area de datos */
	unsigned int base;
	/** @brief Apuntador al encabezado */
	memreg_header_t * header;
}memreg_footer_t;

/** @brief Tipo para lista enlazada de encabezados. */
typedef struct memreg_header_list {
	/** @brief Cabeza de la lista */
	memreg_header_t * head;
	/** @brief Cola de la lista */
	memreg_header_t * tail;
	/** @brief N�mero de elementos en la lista */
	int count;
} memreg_header_list_t;

/** @brief Tipo para la gesti�n de memoria din�mica
 * Para gestionar la memoria  se requiere conocer la direcci�n de inicio de la
 * region para asignacion dinamica, su tama�o, y las listas de regiones
 * libres y asignadas.
 */
typedef struct heap {
	/** @brief Inicio de la region de memoria para asignacion dinamica */
	unsigned int base;
	/** @brief Posici�n actual del heap */
	unsigned int top;
	/** @brief Tamanio maximo de la region de memoria para asignacion dinamica*/
	unsigned int limit;
	/** @brief Lista de regiones de memoria libres */
	list_memreg_header * free;
	/** @brief Lista de regiones de memoria usadas */
	list_memreg_header * used;
}heap_t;


/* Macros para tamanos */

/** @brief Tama�o del encabezado de region */
#define MEMREG_HEADER_SIZE sizeof(memreg_header_t)

/** @brief Tama�o del pie de la region */
#define MEMREG_FOOTER_SIZE sizeof(memreg_footer_t)

/** @brief Tama�o minimo de una region de memoria */
#define MEMREG_MIN_SIZE MEMREG_HEADER_SIZE + MEMREG_GRANULARITY \
		+ MEMREG_FOOTER_SIZE

/** @brief Tama�o de una regi�n de memoria, incluyendo el encabezado y el
 * pie */
#define MEMREG_SIZE(size) \
	MEMREG_HEADER_SIZE + size + MEMREG_HEADER_SIZE

/** @brief Tama�o minimo del heap */
#define HEAP_MIN_SIZE sizeof(heap_t) + MEMREG_MIN_SIZE

/**
 * @brief Esta rutina se encarga de inicializar el area de memoria
 * para asignacion dinamica, y las estructuras de datos requeridas para su
 * gestion.
 * @param ptr Puntero al inicio de la memoria disponible para asignacion
 * din�mica
 * @param limit Tama�o de la memoria disponible
 * @return Apuntador al heap inicializado.
 */
heap_t * setup_heap(void * ptr, unsigned int limit);

/**
 * @brief Permite crear una region de memoria para asignacion dinamica.
 * Importante: Esta rutina asume que la region de memoria que se desea
 * crear esta disponible. Si no se utiliza adecuadamente, puede danar regiones
 * de memoria creadas anteriormente.
 * @param base Direcci�n lineal de inicio del heap
 * @param limit Tama�o de la regi�n de memoria para asignaci�n din�mica.
 * @return Apuntador al nuevo heap
 */
heap_t *  create_heap(unsigned int base, unsigned int limit);

/**
 * @brief Funci�n privada para expandir el heap en un tama�o especificado.
 * @param heap Heap que se desea expandir
 * @param limit  Tamanio a expandir.
 * @return Puntero al encabezado de la nueva region de memoria, 0 si no es
 *  posible expandir el heap. Esta region de memoria ya se encuentra insertada
 *  dentro de la lista de regiones disponibles.
 */
memreg_header_t * expand_heap(heap_t * heap, unsigned int limit);

/**
 * @brief Solicita la asignacion de memoria dentro de un heap.
 * Primero se busca dentro de las regiones libres, una que tenga
 * un tama�o cercano al buscado. Si no existe una region, expande el heap.
 * Si existe suficiente espacio adicional al buscado dentro de la region, esta
 * region se divide en dos: una region asignada, que se retorna, y una
 * region libre con el espacio sobrante.
 * @param heap Heap del cual se desea obtener el espacio
 * @param size Cantidad de memoria a asignar
 * @return Apuntador a la region de memoria asignada. 0 si no se puede
 * asignar memoria.
 */
void * alloc_from_heap(heap_t * heap, unsigned int size);

/**
 * @brief Libera una region de memoria dentro de un heap.
 * @param heap Heap de la cual se desea liberar la regi�n
 * @param header Apuntador al encabezado de la region de memoria a liberar
 */
void free_from_heap(heap_t * heap, memreg_header_t *header);

/**
 * @brief Determina si una region de memoria dada es valida dentro de un
 * heap.
 * @param heap Apuntador al heap en el cual se encuentra la region
 * @param header Apuntador al encabezado de la region a validar
 * @return: 1 = region valida, 0 = region no valida
 */
int memreg_is_valid(heap_t * heap, memreg_header_t *header);

/**
 * @brief Imprime la informacion de una region de memoria
 * @param header Apuntador al encabezado de la regi�n de memoria
 */
void print_memory_region(memreg_header_t * header);

/**
 * @brief Imprime el estado de un heap, incluyendo las regiones
 * que se encuentren definidas.
 * @param heap Heap a imprimir
 */
void print_heap(heap_t *heap);

#endif /* MM_H_ */
