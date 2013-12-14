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

/** @brief Localizacion del mapa de bits de memoria.
 *
 * En esta direccion es donde esta localizada la estructura de datos
 * del mapa de bits*/
#define MMAP_LOCATION 0x500

 /** @brief Tamaño de la unidad de asignación de memoria
  *
  * El valor por default es 4096 es decir 4KB*/
#define MEMORY_UNIT_SIZE 4096

/** @brief Número de bytes que tiene una entrada en el mapa de bits
 *
 * Equivale a que el numero de bytes por cada unidad de memoria*/
#define BYTES_PER_ENTRY sizeof(unsigned int)

/** @brief Número de bits que tiene una entrada en el mapa de bits
 *
 * Equivale a que el numero de bits por cada unidad de memoria
 * en este caso, son 32bits */
#define BITS_PER_ENTRY (8 * BYTES_PER_ENTRY)

/** @brief Número de unidades en la memoria disponible
 *
 * MEMORY_UNITS calcula el numero de unidades que estan disponible
 * para ello hace la operacion de el tamaño total de memoria
 * disponible dividido por MEMORY_UNIT_SIZE*/
#define MEMORY_UNITS (memory_length / MEMORY_UNIT_SIZE)

/** @brief Entrada en el mapa de bits correspondiente a una dirección
 *
 * Definiendo la la direccion con el parametro de entrada addr, la direccion
 * en el mapa de bits que ocupa*/
#define bitmap_entry(addr) \
	( addr /  MEMORY_UNIT_SIZE) / ( BITS_PER_ENTRY )

/** @brief Desplazamiento en bits dentro de la entrada en el mapa de bits */
#define bitmap_offset(addr) \
	(addr / MEMORY_UNIT_SIZE) % ( BITS_PER_ENTRY )

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

/**
 * @brief Esta rutina inicializa el mapa de bits de memoria,
 * a partir de la informacion obtenida del GRUB.
 *
 * Basicamente lo que se hace es configurar el mapa de bits
 * de la memoria, para ello la secuencia de pasos que realiza
 * dicha configuracion es:
 * @verbatim
   1.Cargar una estructura de tipo multiboot_info_t con la
	   informacion del multiboot que es la informacion que el GRUB
	   almacena luego de cargar el KERNEL.
   2.Limpia el mapa de bits, haciendo memory_bitmap[i] = 0,
   	   donde i=1,2,3.... (memory_bitmap_length-1)
   3.Verifica si el GRUB cargo la informacion solicitada en la
	   estructura del multiboot. En el proyecto actual, se omite
	   esta parte.
   4.Se establece, la minima direccion de memoria permitida para
	   liberar, a esta se le asignara la direccion lineal de donde
	   termina el KERNEL.
   5.Verifica si la direccion del mapa de bits son validos, para ello
	   verifica que el bit info->flags[6] sea 1, lo que sifnifica que  mmap_length
	   y mmap_addr son validos .
   6.Verifica que la region de memoria cumple con las condiciones
	   para ser considerada como Memoria Disponible.
	   Para ello, se debe considerar que:
	   ->La primera unidad de memoria este ubicada en la posición de
	   memoria mayor o igual que 1 MB.
	   ->nmap->type que es el tipo de area de memoria si es 1.
	   6,1.Lo que significa disponible, de lo contrario es que ya esta reservada.
		   Si la region esta marcada como disponible y la posicion esta por
		   encima de la direccion  de la posicion del KERNEL en memora,
		   se procede a verificar si el KERNEL se encuentre en esa region.
		   Entonces se toma el inicio de la memoria disponible en la posicion
		   en la cual finaliza el KERNEL.
		   Seguido a esto, se verifica si se cargaron los modulos con el KERNEL
		   En caso de que hayan modulos que se carguen con el KERNEL, entonces
		   la direccion de posicion inicial cambian ahora y empezadia desde luego
		   de los modulos cargado; de lo contrario no se afecta.
		   y por ultimo se resta el espacio que ahora dispone.

	   6,2.Si la region ya esta reservada, y/o que la primera unidad de memoria
	   	   es menor que 1 MB, lo que significa que el KERNEL no se encuentra en
	   	   esta region, entonces el tamaño es mayor que la region encontrada.

	7.Si existe una region de memoria disponible, es decir si la direccion donde
		inicia la primera unidad de memoria es valida, y el tamaño disponible es
		valido.
		Se procede a establecer en que direccion de memoria se debe ubicar para
		la asignacion de memoria permitida a liberar, para esto, se calcula la
		direccion en la cual finaliza la memoria disponibe, hace unos ajustes de
		redondeo, se calcula el tamaño de region disponible con las unidades ya
		redondeadas, y se procede a establecer las variables globales del KERNEL.
	8.Se marca la direccion de memoria como disponibley la direccion de memoria
		de la cual se puede liberar memoria.


 @endverbatim
 */
void setup_memory(void);

/**
 @brief Busca una unidad libre dentro del mapa de bits de memoria.
 * @return Dirección de inicio de la unidad en memoria.
 * El proceso que realiza esta funcion es,
 * @verbatim
 	 1.Verificar si no existen unidades libres, de ser asi, el valor de
 	 	 retorno es 0.
 	 2.Luego si existen unidades libres, entonces, se inicia una iteracion
 	 	 en busqueda de una unidad de memoria libre por el mapa de bits.
 	 3.De no existir una unidad libre dentro del mapa de bits, entonces
 	 	 devuelve 0;
 	 	 En caso de que si, devuelve la direccion de inicio lineal en la
 	 	 de donde se encuentra libre la unidad encontrada, para ello lo
 	 	 hace multiplicando las unidades libres de memoria por el tamaño
 	 	 de asignacion de una unidad de memoria;
 @endverbatim

 */
char * allocate_unit(void);

/** @brief Busca una región de memoria contigua libre dentro del mapa de bits
 * de memoria.
 * @param length Tamaño de la región de memoria a asignar.
 * @return Dirección de inicio de la región en memoria.
 *
 * El proceso que realiza esta funcion es,
 * @verbatim
 	 1.Verificar si no existen unidades libres, de ser asi, el valor de
 	 	 retorno es 0
 	 2.Luego si existen unidades libres, entonces, se inicia una iteracion
 	 	 en busqueda de una unidad de memoria disponible.
 	 3.De no existir una unidad libre dentro del mapa de bits, entonces
 	 	 devuelve 0; en caso de que si, devuelve la direccion de inicio
 	 	 de la region de memoria multiplicando las unidades libres
 	 	 de memoria por el tamaño de asignacin de una unidad de memoria.
 @endverbatim
 */
char * allocate_unit_region(unsigned int length);

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Dirección de memoria dentro del área a liberar.
 *
 * Para permitir la liberacion de una unidad de memoria, se define la secuencia
 * de pasos como:
 * @verbatim
	1.Si la direccion que se recibe es menor a la unidad minima permitida de
		asignacion de memoria, significa que se quiere liberar memoria que no
		esta disponible, ya que esta usada por el KERNEL o los modulos que se
		cargan con el KERNEL, en este caso termina sin realizar la peticion.
	2.Se convierte a una direccion lineal para ponerla en el mapa de bits como
		disponible.
	3.Se marca esa unidad liberada, como la proxima unidad para asignar, y aumenta
		el numero de unidades libres.
 @endverbatim
 */
void free_unit(char *addr);

/**
 * @brief Permite liberar una región de memoria.
 * @param start_addr Dirección de memoria del inicio de la región a liberar
 * @param length Tamaño de la región a liberar
 *
 * Para permitir la liberacion de una region de memoria, se siguen estos pasos;
 * @verbatim
	1.Si la direccion que se recibe es menor a la unidad minima permitida de
		asignacion de memoria, significa que se quiere liberar memoria que no
		esta disponible, ya que esta usada por el KERNEL o los modulos que se
		cargan con el KERNEL, en este caso termina sin realizar la peticion.
	2.Se convierte a una direccion lineal para poner en el mapa de bits como
		disponible mediante un ciclo permitiendo liberar toda la region de memoria
		en unidades de memoria disponibles.
	3.Se marca esa unidad liberada, como la proxima unidad para asignar, y aumenta
		el numero de unidades libres.
 @endverbatim
 */
void free_region(char *start_addr, unsigned int length);

#endif /* PHYSMEM_H_ */
