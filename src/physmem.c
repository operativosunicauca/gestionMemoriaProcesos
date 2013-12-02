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

	//printf("Mods end: %u\n", mods_end);

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

		printf("Memory start at %u = %x\n", memory_start, memory_start);

		/* Marcar la región de memoria como disponible */
		free_region((char*)memory_start, memory_length);

		/* Establecer la dirección de memoria a partir
		 * de la cual se puede liberar memoria */
		allowed_free_start = memory_start;
		next_free_unit = allowed_free_start / MEMORY_UNIT_SIZE;

		total_units = free_units;
		base_unit = next_free_unit;

		 printf("Available memory at: 0x%x units: %d Total memory: %d\n",
				memory_start, total_units, memory_length);
	}
 }

/**
 @brief Busca una unidad libre dentro del mapa de bits de memoria.
 * @return Dirección de inicio de la unidad en memoria.
 */
  char * allocate_unit(void) {
	 unsigned int unit;
	 unsigned int entry;
	 unsigned int offset;


	// printf ("%d ", free_units);
	 /* Si no existen unidades libres, retornar*/
	 if (free_units == 0) {
		 //printf("Warning! out of memory!\n");
		 return 0;
	 }

	/* TODO: Implementar la logica para recorrer la lista de
	 * unidades de memoria y buscar un espacio libre.
	 * Es posible que se deba partir un nodo libre en dos.
	 * Se debe retornar un apuntador char * al inicio de la
	 * region libre. */

 	 return 0;
  }


  /** @brief Busca una región de memoria contigua libre dentro del mapa de bits
   * de memoria.
   * @param length Tamaño de la región de memoria a asignar.
   * @return Dirección de inicio de la región en memoria.
   */
  char * allocate_unit_region(unsigned int length) {
	unsigned int unit;
	unsigned int unit_count;
	unsigned int i;
	int result;

	unit_count = (length / MEMORY_UNIT_SIZE);

	if (length % MEMORY_UNIT_SIZE > 0) {
		unit_count++;
	}

	//printf("\tAllocating %d units\n", unit_count);

	if (free_units < unit_count) {
		 //printf("Warning! out of memory!\n");
		 return 0;
	}

	/* TODO: Iterar por la lista para encontra un nodo
	 * que tenga al menos el numero de unidades solicitados.
	 * Marcar el nodo como usado, y crear un nuevo nodo en
	 * el cual queda el resto de unidades disponibles. */

	  return 0;
  }

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Dirección de memoria dentro del área a liberar.
 */
void free_unit(char * addr) {
	 unsigned int start;
	 unsigned int entry;
	 int offset;
	 unsigned int unit;

	 start = round_down_to_memory_unit((unsigned int)addr);

	 if (start < allowed_free_start) {return;}

	 unit = start / MEMORY_UNIT_SIZE;

	 /* TODO: Buscar la unidad en la lista de unidades, y marcarla como
	  * disponible.
	  * Es posible que se requiera fusionar nodos en la lista!*/

	 /* Marcar la unidad recien liberada como la proxima unidad
	  * para asignar */
	 next_free_unit = unit;

	 /* Aumentar en 1 el numero de unidades libres */
	 free_units ++;

 }

/**
 * @brief Permite liberar una región de memoria.
 * @param start_addr Dirección de memoria del inicio de la región a liberar
 * @param length Tamaño de la región a liberar
 */
void free_region(char * start_addr, unsigned int length) {
	 unsigned int start;
	 unsigned int end;

	 start = round_down_to_memory_unit((unsigned int)start_addr);

	 if (start < allowed_free_start) {return;}

	 end = start + length;

	 for (; start < end; start += MEMORY_UNIT_SIZE) {
		 free_unit((char*)start);
	 }

	 /* Almacenar el inicio de la región liberada para una próxima asignación */
	 next_free_unit = (unsigned int)start_addr / MEMORY_UNIT_SIZE;
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
