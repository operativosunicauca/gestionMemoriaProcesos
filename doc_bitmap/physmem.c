/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene la implementación de las rutinas relacionadas
 * con las gestión de memoria física. La memoria se gestiona en unidades
 * de 4096 bytes.
 */

#include <physmem.h>
#include <multiboot.h>
#include <stdio.h>
#include <stdlib.h>

/** @brief Mapa de bits de memoria disponible
 *  @details Esta variable almacena el apuntador del inicio del mapa de bits
 *	que permite gestionar las unidades de memoria. En este caso, la direccion lineal
 *	en donde esta ubicado el bitmap es 0x500 e
 *	
 */
unsigned int * memory_bitmap = (unsigned int*) MMAP_LOCATION;

/** @brief Siguiente unidad disponible en el mapa de bits */
unsigned int next_free_unit;

/** @brief Numero de marcos libres en la memoria */
int free_units;

/** @brief Numero total de unidades en la memoria */
int total_units;

/** @brief Marco inicial de las unidades  disponibles en memoria */
unsigned int base_unit;

/** @brief Tamaño del mapa de bits en memoria.
 * @details
 * Para un espacio fisico de maximo 4 GB, se requiere un mapa de bits
 * de 128 KB. Si cada entrada ocupa 4 bytes, se requiere 32678 entradas.
 * En la operacion ~(0x0)/(MEMORY_UNIT_SIZE * BITS_PER_ENTRY) lo que hace es
 * averiguar que tamaño tiene el mapa de bits, en donde se divide el máximo
 *	numero que se puede guardar en un registro (2^32 -1 ) entre el tamaño de
 *	la unidad de memoria, lo que nos da el numero de unidades de memoria que
 *	tenemos en un espacio lineal, y a su vez se divide entre los bits que tiene
 *	cada entrada en la tabla, lo que da el total de elementos que tiene la tabla.
 */
unsigned int memory_bitmap_length = ~(0x0)
				/ (MEMORY_UNIT_SIZE * BITS_PER_ENTRY);

/** @brief Variable global del kernel que almacena el inicio de la región
 * de memoria disponible */
unsigned int memory_start;
/** @brief Variable global del kernel que almacena el tamaño en bytes de
 * la memoria disponible */
unsigned int memory_length;

/** @brief Mínima dirección de memoria permitida para liberar,  a esta se le 
 *	debera asignar la direccion lineal en donde termina el KERNEL.
 */

unsigned int allowed_free_start;

/**
 * @brief Esta rutina inicializa el mapa de bits de memoria,
 * a partir de la informacion obtenida del GRUB.
 */
void setup_memory(void) {

	extern multiboot_header_t multiboot_header;
	extern unsigned int multiboot_info_location;

	/* Variables temporales para hallar la region de memoria disponible */
	unsigned int tmp_start;
	unsigned int tmp_length;
	unsigned int tmp_end;
	int mod_count;
	multiboot_info_t * info = (multiboot_info_t *) multiboot_info_location;

	int i;

	unsigned int mods_end; /* Almacena la dirección de memoria final
	 del ultimo modulo cargado, o 0 si no se cargaron modulos. */

	/*printf("Bitmap array size: %d\n", memory_bitmap_length);*/

	for (i = 0; i < memory_bitmap_length; i++) {
		memory_bitmap[i] = 0;
	}

	/*
	 printf("Inicio del kernel: %x\n", multiboot_header.kernel_start);
	 printf("Fin del segmento de datos: %x\n", multiboot_header.data_end);
	 printf("Fin del segmento BSS: %x\n", multiboot_header.bss_end);
	 printf("Punto de entrada del kernel: %x\n", multiboot_header.entry_point);
	 */

	/* si flags[3] = 1, se especificaron módulos que deben ser cargados junto
	 * con el kernel*/

	mods_end = 0;
	//printf("\n1.info->flags :<%b>\n",info->flags);
	if (test_bit(info->flags, 3)) {
		printf("\n2.info->flags :<%b>\n", info->flags);
		mod_info_t * mod_info;
		/*
		 printf("Modules available!. Start: %u Count: %u\n", info->mods_addr,
		 info->mods_count);
		 */
		for (mod_info = (mod_info_t*) info->mods_addr, mod_count = 0;
				mod_count < info->mods_count; mod_count++, mod_info++) {
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

	/* Suponer que el inicio de la memoria disponible se encuentra
	 * al finalizar el kernel */
	allowed_free_start = round_up_to_memory_unit(multiboot_header.bss_end);

	/** Existe un mapa de memoria válido creado por GRUB? */
	if (test_bit(info->flags, 6)) {
		/** Si verifica que el bit en la posición 6 es 1, entonces se crea un mapa valido de memoria,
		 *   es decir que mmap_length y mmap_addr son validos
		 *
		 */
		memory_map_t *mmap;

		/*printf ("mmap_addr = 0x%x, mmap_length = 0x%x\n",
		 (unsigned) info->mmap_addr, (unsigned) info->mmap_length);*/
		for (mmap = (memory_map_t *) info->mmap_addr;
				(unsigned int) mmap < info->mmap_addr + info->mmap_length;
				mmap = (memory_map_t *) ((unsigned int) mmap + mmap->entry_size
						+ sizeof(mmap->entry_size))) {
			printf(" size = 0x%x, base_addr = 0x%x%x,"
					" length = 0x%x%x, type = 0x%x\n", mmap->entry_size,
					mmap->base_addr_high, mmap->base_addr_low,
					mmap->length_high, mmap->length_low, mmap->type);

			/* Verificar si la región de memoria cumple con las condiciones
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
			if (mmap->type == 1
					&& mmap->base_addr_low >= multiboot_header.kernel_start) {
				tmp_start = mmap->base_addr_low;
				tmp_length = mmap->length_low;

				/* Verificar si el kernel se encuentra en esta region */
				if (multiboot_header.bss_end >= tmp_start
						&& multiboot_header.bss_end <= tmp_start + tmp_length) {
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
					if (mods_end > 0 && mods_end >= tmp_start
							&& mods_end <= tmp_start + tmp_length) {
						//printf("Adding module space...\n");
						tmp_start = mods_end;
					}
					/* Restar al espacio disponible.*/
					tmp_length -= tmp_start - mmap->base_addr_low;
					if (tmp_length > memory_length) {
						memory_start = tmp_start;
						memory_length = tmp_length; /* Tomar el espacio */
					}
				} else {
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

	/* Existe una región de memoria disponible? */
	if (memory_start > 0 && memory_length > 0) {
		/* Antes de retornar, establecer la minima dirección de memoria
		 * permitida para liberar*/

		//printf("Free units before setting up memory: %d\n", free_units);

		tmp_start = memory_start;
		/* Calcular la dirección en la cual finaliza la memoria disponible */
		tmp_end = tmp_start + tmp_length;

		/* Redondear el inicio y el fin de la región de memoria disponible a
		 * unidades de memoria */
		tmp_end = round_down_to_memory_unit(tmp_end);
		tmp_start = round_up_to_memory_unit(tmp_start);

		/* Calcular el tamaño de la región de memoria disponible, redondeada
		 * a límites de unidades de memoria */
		tmp_length = tmp_end - tmp_start;

		/* Actualizar las variables globales del kernel */
		memory_start = tmp_start;
		memory_length = tmp_length;

		/* Marcar la región de memoria como disponible */
		free_region((char*) memory_start, memory_length);

		/* Establecer la dirección de memoria a partir
		 * de la cual se puede liberar memoria */
		allowed_free_start = memory_start;
		next_free_unit = allowed_free_start / MEMORY_UNIT_SIZE;

		total_units = free_units;
		base_unit = next_free_unit;

		/* printf("Available memory at: 0x%x units: %d Total memory: %d\n",
		 memory_start, total_units, memory_length);*/
	}
}

/** @brief Permite verificar si la unidad se encuentra disponible. */
static __inline__ int test_unit(unsigned int unit) {
	volatile entry = unit / BITS_PER_ENTRY;
	volatile offset = unit % BITS_PER_ENTRY;
	return (memory_bitmap[entry] & 0x1 << offset);
}

/** @brief  Permite marcar la unidad como ocupada. */
static __inline__ void clear_unit(unsigned int unit) {
	volatile entry = unit / BITS_PER_ENTRY;
	volatile offset = unit % BITS_PER_ENTRY;
	memory_bitmap[entry] &= ~(0x1 << offset);
}

/** @brief Permite marcar la unidad como libre. */
static __inline__ void set_unit(unsigned int unit) {
	volatile entry = unit / BITS_PER_ENTRY;
	volatile offset = unit % BITS_PER_ENTRY;
	memory_bitmap[entry] |= (0x1 << offset);
}

/**
 @brief Busca una unidad libre dentro del mapa de bits de memoria.
 * @return Dirección de inicio de la unidad en memoria.
 */
char * allocate_unit(void) {
	unsigned int unit; /** Define las unidades disponibles de memoria
	 que dispone*/
	unsigned int entry;
	unsigned int offset;

	// printf ("%d ", free_units);
	/* Si no existen unidades libres, retornar*/
	if (free_units == 0) {
		//printf("Warning! out of memory!\n");
		return 0;
	}

	/* Iterar por el mapa de bits*/
	unit = next_free_unit;
	do {
		if (test_unit(unit)) {
			entry = unit / BITS_PER_ENTRY;
			offset = unit % BITS_PER_ENTRY;
			memory_bitmap[entry] &= ~(0x1 << offset);

			/* printf("\tFree unit at %x offset %d %b %x\n",
			 unit * MEMORY_UNIT_SIZE,
			 offset, memory_bitmap[entry], &memory_bitmap[entry]);*/
			/* Avanzar en la posicion de busqueda de la proxima unidad
			 * disponible */
			next_free_unit++;
			if (next_free_unit > base_unit + total_units) {
				next_free_unit = base_unit;
			}
			/* Descontar la unidad tomada */

			free_units--;
			return (char*) (unit * MEMORY_UNIT_SIZE);
		}
		unit++;
		if (unit > base_unit + total_units) {
			unit = base_unit;
		}
	} while (unit != next_free_unit);

	return 0;
}

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

	/* Iterar por el mapa de bits*/
	unit = next_free_unit;

	/* Iterar por el mapa de bits*/
	unit = next_free_unit;
	do {
		if (test_unit(unit)
				&& (unit + unit_count) < (base_unit + total_units)) {

			result = 1;
			for (i = unit; i < unit + unit_count; i++) {
				result = (result && test_unit(i));
			}
			/* Marcar la unidad como libre */
			if (result) {
				for (i = unit; i < unit + unit_count; i++) {
					//printf("\tFree unit at %x\n", i *  MEMORY_UNIT_SIZE);
					/* Descontar la unidad tomada */
					free_units--;
					clear_unit(i);
				}

				/* Avanzar en la posicion de busqueda de la proxima unidad
				 * disponible */
				next_free_unit = unit + unit_count;
				if (next_free_unit > base_unit + total_units) {
					next_free_unit = base_unit;
				}

				return (char*) (unit * MEMORY_UNIT_SIZE);
			}
		}
		unit++;
		if (unit > base_unit + total_units) {
			unit = base_unit;
		}
	} while (unit != next_free_unit);

	return 0;
}

void free_unit(char * addr) {
	unsigned int start;
	unsigned int entry;
	int offset;
	unsigned int unit;

	start = round_down_to_memory_unit((unsigned int) addr);

	if (start < allowed_free_start) {
		return;
	}

	unit = start / MEMORY_UNIT_SIZE;

	set_unit(unit);

	/* Marcar la unidad recien liberada como la proxima unidad
	 * para asignar */
	next_free_unit = unit;

	/* Aumentar en 1 el numero de unidades libres */
	free_units++;

}

void free_region(char * start_addr, unsigned int length) {
	unsigned int start;
	unsigned int end;

	start = round_down_to_memory_unit((unsigned int) start_addr);

	if (start < allowed_free_start) {
		return;
	}

	end = start + length;

	for (; start < end; start += MEMORY_UNIT_SIZE) {
		free_unit((char*) start);
	}

	/* Almacenar el inicio de la región liberada para una próxima asignación */
	next_free_unit = (unsigned int) start_addr / MEMORY_UNIT_SIZE;
}
