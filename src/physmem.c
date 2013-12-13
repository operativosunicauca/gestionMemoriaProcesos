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

/**@brief Lista enlazada para gestionar la memoria ocupada y disponible.
 *@details Esta variable almacena el apuntador a una memory_list, la cual
 * ayudara al kernel a gestionar la memoria disponible y usada. */
memory_list *kernel_list;

/* Referencia a la variable global kernel_keap */
/** @brief Variable global para el heap. Sobre este heap actua kmalloc. */
static heap_t * kernel_heap;

/** @brief Variable global del kernel que almacena el inicio del
 * heap del kernel */
unsigned int kernel_heap_start;

 /** @brief Siguiente unidad disponible en la lista enlazada. */
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

/* @brief Mínima unidad donde empieza la memoria disponible. */
 int min;

/* @brief Máxima unidad donde termina la memoria disponible. */
 int max;

/**
 * @brief Esta rutina inicializa la lista enlazada para la gestión de memoria,
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
	 * de mayor tamano, marcada como disponible, cuya dirección base sea
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
				  * hasta ahora.
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

		/* Se calcula el número de unidades libres al iniciar la gestión de memoria. */
		free_units = memory_length  / MEMORY_UNIT_SIZE;

		/* Se crea una lista para la gestión de memoria disponible. */
		kernel_list = create_memory_list((char*)memory_start, memory_length);

		/* Establecer la dirección de memoria a partir de la cual se puede liberar memoria */
		allowed_free_start = memory_start;
		next_free_unit = allowed_free_start / MEMORY_UNIT_SIZE;

		/* Se establece el total de unidades libres y la unidad de memoria base. */
		total_units = free_units;
		base_unit = next_free_unit;

		 printf("Available memory at: 0x%x units: %d Total memory: %d\n",
				memory_start, total_units, memory_length);
		 printf("base unit %d\n", base_unit);

		 /* Se establece la unidad mínima y la unidad máxima de memoria permitida. */
		 min = base_unit;
		 max = (base_unit + total_units) - 1;
	}
 }

/**
 @brief Busca una unidad libre dentro de la lista enlazada de la memoria.
 * @return Dirección de inicio de la unidad en memoria.
 */
  char * allocate_unit(void) {

	 /* Si no existen unidades libres, da un aviso y retorna cero (0).*/
	 if (free_units == 0) {
		 printf("Warning! out of memory!\n");
		 return 0;
	 }

	 /*Puntero a un nodo de memoria.*/
	 memory_node *ptr;

	 /*Recorre la lista de memoria en busca de un nodo libre.
	  *Inicia desde la cabeza de la lista de nodos de memoria
	  * y termina cuando ha encontrado dicho nodo o llega al final de la lista.
	  */
	 for( ptr = kernel_list->mem_head; ptr != 0; ptr = ptr->next)
	 {
		 	/*Si encuentra un nodo libre.*/
		 	if( ptr->state == 'L' ) {

		 		/*Si el nodo tiene una sola unidad en la region, establece su estado en usado.*/
	 			if(ptr->units == 1) {
	 				ptr->state = 'U';
	 			}
	 			else {
	 				/*Si el nodo tiene mas de una unidad libre, se crea un nuevo nodo.
	 				 *Su estado se establece en Libre, el inicio del nodo es el inicio del nodo
	 				 *actual más uno y las unidades son las unidades del nodo actual menos 1.
	 				 *Como se usa la primera unidad de la region libre, el nuevo nodo
	 				 *representa el nuevo espacio libre que queda.
	 				 */
	 				memory_node *new_node = create_memory_node('L', (ptr->start+1), (ptr->units-1));

	 				/* Establece como usada la primera unidad del nodo actual. */
	 				ptr->state = 'U';
	 				ptr->units = 1;

	 				/*Enlaza el nodo 'anterior' del nuevo nodo al nodo actual,
	 				 *con el objetivo de colocar el nuevo nodo a la 'derecha' del actual.*/
	 				new_node->previous = ptr;

	 				/*Si el nodo actual es la cola de la lista, el 'siguiente' de nodo actual
	 				 *se enlaza al nuevo nodo y a su vez, este último se coloca como la cola
	 				 *de la lista.*/
	 				if(ptr->next == 0){
	 					ptr->next = new_node;
	 					kernel_list->mem_tail = new_node;
	 				}
	 				/*Sino, se enlaza el nodo actual, el nodo siguiente a éste y el
	 				 *nuevo nodo, de tal manera que el nuevo nodo quede entre el actual y el
	 				 *siguiente del actual.*/
	 				else {
	 					new_node->next = ptr->next;
	 					memory_node *n = ptr->next;
	 					n->previous = new_node;
	 					ptr->next = new_node;
	 				}
	 			}
	 			/* Se disminuye el número de unidades libres en memoria y se retorna la dirección
	 			 * de inicio de la unidad en memoria.*/
	 			free_units--;
	 			return (ptr->start * MEMORY_UNIT_SIZE);
	 		}
	 }

	 /* Si se recorrió toda la lista enlazada y no se encontró una unidad de memoria libre, se
	  * informa lo sucedido y se retorna cero (0). */
	 printf("Warning! out of memory!\n");
 	 return 0;
  }

  /** @brief Busca una región de memoria contigua libre dentro de la lista enlazada
   * de memoria.
   * @param length Tamaño de la región de memoria a asignar.
   * @return Dirección de inicio de la región en memoria.
   */
  char * allocate_unit_region(unsigned int length) {

	/*Variable de unidad*/
    unsigned int unit;
    /*Número de unidades*/
    unsigned int unit_count;

    /* Se calcula el número de unidades a asignar dividiendo el tamaño (en bytes)
     * entre el tamaño de cada unidad. Se debe tener encuenta que unit_count
     * almacena la parte entera de la division. */
	unit_count = (length / MEMORY_UNIT_SIZE);

	/* Si la división anterior no es exacta, se necesita una unidad más que representa
	 * la parte decimal restante, por cual se aumenta unit_count en uno (1). */
	if (length % MEMORY_UNIT_SIZE > 0) {
		unit_count++;
	}

	/*Si no existen unidades libres, da un aviso y retorna cero (0).*/
	if (free_units < unit_count) {
		printf("Warning! out of memory!\n");
		return 0;
	}

	/*Se crea un puntero a un nodo de memoria para recorrer la lista de memoria.*/
	memory_node *ptr;

	/* Se crea un puntero a un nodo de memoria y se reserva espacio en memoria para su uso.*/
	memory_node *new_node = (memory_node *) kmalloc( sizeof(memory_node) );

	/*Se crea un nodo de memoria auxiliar.*/
	memory_node *aux_node;

	/* Recorre la lista de gestión de memoria en busca de un nodo
	 * que tenga al menos el numero de unidades solicitadas.
	 * Inicia desde la cabeza de la lista de nodos de memoria
	 * y termina cuando ha encontrado dicho nodo o cuando llega al final de la lista. */
	for( ptr = kernel_list->mem_head; ptr != 0; ptr = ptr->next) {
		/*Si se encuentra un nodo libre.*/
		if( ptr->state == 'L') {
			/*Si las unidades del nodo actual son mayores que las unidades a asignar. */
			if( ptr->units > unit_count ) {
				/* Se cambia el estado del nodo actual a usado. */
				ptr->state = 'U';

				/* new_node representa el nuevo nodo que se ubicará a la 'derecha' del nodo actual
				 * y contendrá las unidades libres restantes a partir de las unidades asignadas.
				 */
				new_node->state = 'L';
				new_node->start = ptr->start + unit_count;
				new_node->units = ptr->units - unit_count;

				/*Se cambian las unidades del nodo actual por las unidades solicitadas (unit_count).*/
				ptr->units = unit_count;

				/*El nodo auxiliar contiene la referencia del nodo siguiente al nodo actual.*/
				aux_node = ptr->next;

				/* Si el nodo auxiliar es nulo indica que el nodo actual (ptr) es la cola de la lista. */
				if(aux_node == 0) {
					/* Por lo tanto el nuevo nodo será la cola de la lista de memoria. */
					kernel_list->mem_tail = new_node;
				}

				/* El nuevo nodo que se creó queda en medio del nodo actual
				 * y del siguiente nodo al actual (si existe). Sino
				 * el nuevo nodo quedara como cola de la lista de memoria. */
				aux_node->previous = new_node;
				new_node->previous = ptr;
				new_node->next = aux_node;
				ptr->next = new_node;

				/* Se disminuye el número de unidades libres en memoria y se retorna la dirección
				 * de inicio de la región en memoria. */
				free_units -= ptr->units;
				return ptr->start * MEMORY_UNIT_SIZE;
			}
			/* Si no se encuentra una unidad de memoria mayor a las unidades solicitadas. */
			else{
				/* Se chequea que el número de unidades del nodo actual sea igual a las unidades
				 * solicitadas. De ser así se cambia el estado del nodo actual por usado. */
				if(ptr->units == unit_count)
				{
					ptr->state = 'U';

					/* Se disminuye el número de unidades libres en memoria y se retorna la dirección
					 * de inicio de la región en memoria. */
					free_units -= ptr->units;
					return ptr->start * MEMORY_UNIT_SIZE;
				}
			}
		}
	}
	/* Si se recorrió toda la lista enlazada y no se encontró una región de memoria libre, se
	 * informa lo sucedido y se retorna cero (0). */
	printf("Warning! out of memory!\n");
	return 0;
  }

/**
 * @brief Permite liberar una unidad de memoria.
 * @param addr Dirección de memoria dentro del área a liberar.
 */
void free_unit(char * addr) {

	 /*Variable inicio*/
	 unsigned int start;
	 /*Variable de unidad*/
	 unsigned int unit;

	 /* Se redondea la dirección de memoria de inicio y se almacena en 'start'. */
	 start = round_down_to_memory_unit((unsigned int)addr);

	 /* Si la dirección de inicio (start) es menor que la mínima permitida para liberar,
	 * no se libera memoria.*/
	 if (start < allowed_free_start) {return;}

	 /* La unidad a liberar será la división entre la dirección inicio y el tamaño de
	  * unidad de memoria */
	 unit = start / MEMORY_UNIT_SIZE;

	 /* Si se pide liberar una unidad que está por fuera del límite, se muestra un
	  * mensaje de advertencia y no se libera memoria. */
	 if ( unit > max ) {
		 printf(" Warning address no exist !!! \n");
		 return;
	 }

	 /* Si todas las unidades de memoria están libres, no se libera memoria. */
	 if( free_units == (max - min) + 1 ){ return; }

	 /*Puntero a un nodo de memoria para recorrer la lista de memoria.*/
	 memory_node *ptr;
	 /*Puntero a un nodo de memoria auxiliar, representa un nodo izquierdo.*/
	 memory_node *new_nodel;
	 /*Puntero a un nodo de memoria auxiliar, representa un nodo derecho.*/
	 memory_node *new_noder;
	 /*Puntero a un nodo de memoria auxiliar.*/
	 memory_node *new_naux;

	 /* Recorre y busca la unidad en la lista para la gestión de memoria y la marca
	 * como Libre cuando la encuentra.
	 * Inicia desde la cabeza de la lista de nodos de memoria
	 * y termina cuando ha encontrado dicho nodo o cuando llega al final de la lista.*/
	 for( ptr = kernel_list->mem_head; ptr != 0; ptr = ptr->next) {

		 /* Se obtiene el nodo anterior y el siguiente al nodo actual para facilitar
		  * la configuración de enlaces en la lista.*/
		 new_nodel = ptr->previous;
		 new_noder = ptr->next;

		 /*	CASO 1.
		  * Si el estado del nodo es usado y el inicio del nodo actual coincide
		  * con la unidad a liberar.*/
		 if(ptr->start == unit && ptr->state == 'U'){

			 /* CASO 1a.
			  * Si el número de unidades del nodo actual equivale a 1 */
			 if(ptr->units == 1){

				 /* Se cambia el estado del nodo actual a Libre. */
				 ptr->state = 'L';

				 /* Se chequea si el nodo derecho o izquierdo del nodo actual están libres
				  * para fusionar dichos nodos con el actual. */

				 if( new_nodel->state == 'L'){
					 if( new_noder->state == 'L'){
						 /* Si el nodo derecho e izquierdo son libres, se suma a las
						  * unidades del nodo izquierdo, las unidades del nodo actual
						  * y del derecho.*/
						 new_nodel->units += ptr->units + new_noder->units;

						 /* Si el nodo siguiente al actual es la cola de la lista, el nodo anterior
						  * al actual pasa a ser la nueva cola de la lista. */
						 if(new_noder->next == 0){
							 kernel_list->mem_tail = new_nodel;
						 }
						 /* Sino, se enlaza el nodo izquierdo con el resto de la lista
						  * de memoria excluyendo el nodo actual y el derecho. */
						 else {
							 new_nodel->next = new_noder->next;
							 new_naux = new_noder->next;
							 new_naux->previous = new_nodel;
						 }

						 /* Se desenlaza el nodo derecho y se libera memoria del
						  * montículo del kernel.*/
						 new_noder->previous = 0;
						 new_noder->next = 0;
						 kfree(new_noder);
					 }
					 else{
						 /* Si el nodo izquierdo es libre y el nodo derecho es usado,
						  * se suma las unidades del nodo actual al izquierdo.
						  * Se enlaza el nodo izquierdo con el nodo derecho. */
						 new_nodel->units += ptr->units;
						 new_noder->previous = new_nodel;
						 new_nodel->next = new_noder;
					 }
					 /* Se desenlaza el nodo actual de la lista y se libera memoria del
					  * montículo del kernel. */
					 ptr->next = 0;
					 ptr->previous = 0;
					 kfree(ptr);
				 }

				 /* En este punto, el nodo izquierdo está usado, se chequea si el nodo derecho
				  * está libre con el fin de fusionar dicho nodo con el actual. */
				 else{
					 if( new_noder->state == 'L'){

						 /* Se actulizan las unidades del nodo actual sumando sus unidades con
						  * las del nodo derecho. */
						 ptr->units += new_noder->units;

						 /* Se establece el nodo derecho como el nodo siguiente al actual. */
						 new_noder = ptr->next;

						 /* Si el nodo derecho es la cola de la lista, el nodo actual
						  * pasa a ser la nueva cola de la lista. */
						 if(kernel_list->mem_tail == new_noder ){
							 kernel_list->mem_tail = ptr;
						 }
						 /* Sino, se enlaza el nodo actual con el resto de la lista
						  * de memoria excluyendo el nodo derecho. */
						 else{
							 ptr->next = new_noder->next;
							 new_naux = new_noder->next;
							 new_naux->previous = ptr;
						 }
						 /* Se desenlaza el nodo derecho de la lista y se libera memoria del
						  * montículo del kernel. */
						 new_noder->next = 0;
						 new_noder->previous = 0;
						 kfree(new_noder);
					 }
				 }
				 /* Marcar la unidad recién liberada como la proxima unidad para asignar. */
			     next_free_unit = unit;
				 /* Se aumenta el número de unidades libres de memoria. */
				 free_units ++;
				 break;
			 }

			 /* CASO 1b.
			  * Si el número de unidades del nodo actual es mayor que 1. */
			 else
			 {
				 /* Si el nodo izquierdo es Libre, se resta una unidad al nodo actual y se le
				  * adiciona al nodo izquierdo, actualizando los valores que sean necesarios. */
				 if( new_nodel->state == 'L' ){
					new_nodel->units += 1;
				 	ptr->units -= 1;
				 	ptr->start += 1;
				 }

				 /* En este punto, el nodo izquierdo puede estar usado o nulo.
				  * Se crea un nuevo nodo y se enlaza a la derecha del actual, liberando
				  * una unidad de memoria y modificando los valores de los nodos que sean
				  * necesarios. */
				 else{

					 /* Se crea un nuevo nodo que representa la región de memoria restante luego
					  * de haber liberado una unidad. */
					 memory_node * new_ptr = create_memory_node('U', ptr->start + 1,ptr->units -1);

					 /* Este nuevo nodo se enlaza por la parte izquierda al actual. */
					 new_ptr->previous = ptr;

					 /* Si el nodo actual es la cola de la lista, el nuevo nodo
					  * pasa a ser la nueva cola de la lista. */
					 if(ptr->next == 0){
						 kernel_list->mem_tail = new_ptr;
					 }

					 /* El nuevo nodo se enlaza por la derecha con el resto de la lista
					  * (si existe) que hace referencia al lado derecho del nodo actual. */
					 new_ptr->next = ptr->next;
					 new_noder->previous = new_ptr;

					 /* Se libera una unidad de memoria en el nodo actual y se enlaza este último
					  * por la derecha con el nuevo nodo (resto de la lista). */
					 ptr->state = 'L';
					 ptr->units = 1;
					 ptr->next = new_ptr;
				 }
				 /* Marcar la unidad recién liberada como la proxima unidad para asignar. */
			     next_free_unit = unit;
				 /* Se aumenta el número de unidades libres de memoria. */
				 free_units ++;
				 break;
			 }
		}
		/* CASO 2.
		 * Si el inicio del nodo actual es mayor que la unidad requerida a liberar y además
		 * el estado del nodo izquierdo es Usado. */
		else if(ptr->start > unit){
			if(new_nodel->state == 'U') {

				/* Caso especial. Se da cuando la unidad a liberar coincide con la última unidad de la
				 * región del nodo izquierdo y además el nodo actual es libre. Cuando se libera dicha unidad
				 * se obtiene una región resultante libre por lo que se modifica el nodo actual
				 * para que contenga dicha región. */
				if(ptr->start == (unit + 1) && ptr->state == 'L'){
					new_nodel->units -= 1;
					ptr->units += 1;
					ptr->start -= 1;
					/* Marcar la unidad recién liberada como la proxima unidad para asignar. */
				    next_free_unit = unit;
				    /* Se aumenta el número de unidades libres de memoria. */
					free_units ++;
					break;
				}

				/* Se crea un 'nodo_primero' que será el nodo que contiene la unidad de memoria a liberar. */
				memory_node *new_st = create_memory_node('L',unit,1);

				/* Se crea un 'nodo_segundo' que será un nodo que contiene la región de memoria
				 * usada restante el cual se ubicará a la izquierda del nodo actual. */
				memory_node *new_nd = create_memory_node('U',unit+1,ptr->start-(unit+1));

				/* Se enlazan los nodos 'nodo_primero', 'nodo_segundo', 'nodo_izquierdo' y 'nodo_actual'.
				 * De esta manera queda en este orden:
				 * 'nodo_izquierdo'-'nodo_primero'-'nodo_segundo'-'nodo_actual'. */
				new_st->previous = new_nodel;
				new_nd->previous = new_st;
				new_st->next = new_nd;
				new_nd->next = ptr;
				ptr->previous = new_nd;
				new_nodel->next = new_st;

				/* Se actualiza el número de unidades del nodo izquierdo. */
				new_nodel->units = unit - new_nodel->start;
			}
			/* Marcar la unidad recién liberada como la proxima unidad para asignar. */
			next_free_unit = unit;
			/* Se aumenta el número de unidades libres de memoria. */
			free_units ++;
			break;
		}
		/* CASO 3.
		 * Si el nodo actual está libre y es la cola de la lista. */
		else if( kernel_list->mem_tail == ptr && ptr->state != 'L'){

			/* Se crea un 'nodo_primero' que será un nodo que contiene la región de memoria
			 * usada restante por el lado izquierdo a la unidad que se va a liberar. */
			memory_node *new_st = create_memory_node('U',ptr->start, unit - ptr->start);

			/* Se crea un 'nodo_segundo' que será un nodo que contiene la unidad liberada. */
			memory_node *new_nd = create_memory_node('L',unit,1);

			/* Se 'limpia' el nodo anterior al 'nodo_primero' y se establece a 'nodo_segundo'
			 * como su nodo siguiente. */
			new_st->previous = 0;
			new_st->next = new_nd;

			/* Se resta al nodo actual el número de unidades del 'nodo_primero'. */
			ptr->units -= new_st->units;

			/* Se completa el enlace doble entre 'nodo_primero' y 'nodo_segundo' y se 'limpia'
			 * el nodo siguiente a este último. */
			new_nd->previous = new_st;
			new_nd->next = 0;

			/* Se resta una unidad al nodo actual, y se establece el inicio de este nodo.
			 * Este nodo contiene la región de memoria usada restante por el lado derecho a la
			 * unidad que se va a liberar. */
			ptr->units--;
			ptr->start = unit + 1;

			/* Si las unidades del nodo actual son menores o iguales a cero, se desenlaza este
			 * nodo y se establece como cola de la lista a 'nodo_segundo'. */
			if(ptr->units <= 0){
				kernel_list->mem_tail = new_nd;
				ptr->next = 0;
				ptr->previous = 0;
				kfree(ptr);
			}
			/* Sino, se enlazan los nodos 'nodo_segundo' y el actual.
			 * De esta manera queda en este orden: 'nodo_primero'-'nodo_segundo'-'nodo_actual'. */
			else{
				ptr->previous = new_nd;
				new_nd->next = ptr;
			}
			/* Si el nodo actual no es la cabeza de la lista se enlaza por la izquierda el resto de
			 * los elementos de la lista con el 'nodo_primero'. */
			if(kernel_list->mem_head != ptr){
				new_nodel->next = new_st;
				new_st->previous = new_nodel;
			}
			/* Sino, el 'nodo_primero' será la nueva cabeza de la lista. */
			else{
				kernel_list->mem_head = new_st;
			}
			/* Marcar la unidad recién liberada como la proxima unidad para asignar. */
		    next_free_unit = unit;
			/* Se aumenta el número de unidades libres de memoria. */
			free_units ++;
			break;
		}
	 }
}

/**
 * @brief Permite liberar una región de memoria.
 * @param start_addr Dirección de memoria del inicio de la región a liberar.
 * @param length Tamaño de la región a liberar.
 */
void free_region(char * start_addr, unsigned int length) {

	/* Dirección de inicio de la región a liberar. */
	 unsigned int start;
	 /* Dirección de fin de la región a liberar. */
	 unsigned int end;

	 /* Se redondea la dirección de memoria de inicio y se almacena en 'start'.*/
	 start = round_down_to_memory_unit((unsigned int)start_addr);

	 /* Si la dirección de inicio (start) es menor que la mínima permitida para liberar,
     * no se libera memoria.*/
	 if (start < allowed_free_start) {return;}

	 /* La última dirección de la región a liberar es la suma entre la dirección de inicio
	  * y el tamaño de la región a liberar. */
	 end = start + length;

	 /* Se recorre la región de memoria a liberar y se avanza de acuerdo al tamaño
	  * de la unidad de memoria. */
	 for (; start < end; start += MEMORY_UNIT_SIZE) {
		 /* Se libera cada una de las unidades de memoria de la región a liberar.
		  * Cada una inicia en la dirección 'start'. */
		 free_unit((char*)start);
	 }

	 /* Almacena el inicio de la región liberada para una próxima asignación. */
	 next_free_unit = (unsigned int)start_addr / MEMORY_UNIT_SIZE;
 }

/**
 * @brief Solicita asignacion de memoria dentro del heap.
 * @param size Tamaño requerido.
 * @return Puntero a la base de la region de memoria asignada, 0 si
 *  		 no es posible asignar memoria.
 */
void * kmalloc(unsigned int size) {
	return alloc_from_heap(kernel_heap, size);
}

/**
 * @brief Solicita liberar una region de memoria dentro del heap.
 * @param ptr Puntero a la base de la region de memoria a liberar.
 */
void  kfree(void * ptr) {
	memreg_header_t * header;
	header = (memreg_header_t *)((unsigned int) ptr - MEMREG_HEADER_SIZE);
	free_from_heap(kernel_heap, header);
}

/* @brief Recorre la lista de gestión de memoria al tiempo que imprime la información
 * de cada nodo.
 * */
void print_list(){
	memory_node *ptr = (memory_node *) kmalloc( sizeof(memory_node) );
	printf(" Kernel memory_list !!!\n");
	int i;
	for( i = 0, ptr = kernel_list->mem_head; ptr != 0; i++, ptr = ptr->next){
		printf("\tnodo %d\t state %c\t start %d\t units %d\n", i, ptr->state, ptr->start, ptr->units);
	}
	kfree(ptr);
}

/* @brief Recorre de derecha a izquierda la lista de gestión de memoria al tiempo que imprime
 * la información de cada nodo.
 * */
void print_list_right_letf(){
	memory_node *ptr = (memory_node *) kmalloc( sizeof(memory_node) );
	printf(" Kernel memory_list !!!\n");
	int i;
	for( i = 0, ptr = kernel_list->mem_tail; ptr != 0; i++, ptr = ptr->previous){
		printf("\tnodo %d\t state %c\t start %d\t units %d\n", i, ptr->state, ptr->start, ptr->units);
	}
	kfree(ptr);
}

/* @brief Permite crear un nodo que será usado en una lista de gestión de memoria.
 * @param state Define el estado del nodo, 'L' si el nodo está disponible y 'U' si está usado.
 * @param start Define la unidad donde inicia la región de memoria representada por el nodo.
 * @param units Define el número de unidades de la región de memoria representada por el nodo.
 * @return Puntero a un nodo que será usado en una lista de gestión de memoria.
 * */
memory_node* create_memory_node(char state, int start, int units){
	memory_node *ret;
	ret = (memory_node*) kmalloc( sizeof(memory_node) );
	ret->state = state;
	ret->start = start;
	ret->units = units;
	ret->previous = 0;
	ret->next = 0;

	return ret;
}

/* @brief Permite crear una lista para la gestión de memoria con un nodo inicial que representa
 * la totalidad del espacio libre.
 * @param start_addr Dirección a partir de la cual la memoria está disponible.
 * @param length Tamaño en bytes de la región de memoria disponible.
 * @return Puntero a una lista para la gestión de memoria.
 * */
memory_list *create_memory_list(char * start_addr, unsigned int length){
	memory_list *mem_list;
	memory_node *mem_node;
	mem_node = create_memory_node('L', (unsigned int)start_addr / MEMORY_UNIT_SIZE, length / MEMORY_UNIT_SIZE);
	mem_list = (memory_list *) kmalloc( sizeof(memory_list) );
	mem_list->mem_head = mem_node;
	mem_list->mem_tail = mem_node;

	return mem_list;
}
