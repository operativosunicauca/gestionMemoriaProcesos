/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene la implementación de las rutinas relacionadas
 * con las gestión de memoria física. La memoria se gestiona en unidades
 * de 4096 bytes.
 */

/**
 * @brief Commit de prueba
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

/* TODO comentar min-ima unidad en memoria*/
 int min;
/* TODO comentar max-ima unidad en memoria */
 int max;

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
		//free_region((char*)memory_start, memory_length);

		/* NUMERO TOTAL DE UNIDADES LIBRES AL INICIAR LA MEMORIA */
		free_units = memory_length  / MEMORY_UNIT_SIZE;

		/* Crear la lista del Kernel */
		kernel_list = create_memory_list((char*)memory_start, memory_length);

		//print_list();

		/* Establecer la dirección de memoria a partir
		 * de la cual se puede liberar memoria */
		allowed_free_start = memory_start;
		next_free_unit = allowed_free_start / MEMORY_UNIT_SIZE;

		total_units = free_units;
		base_unit = next_free_unit;

		 printf("Available memory at: 0x%x units: %d Total memory: %d\n",
				memory_start, total_units, memory_length);
		 printf("base unit %d\n", base_unit);
		 /* TODO cometar asignacion de min y max */
		 min = base_unit;
		 max = (base_unit + total_units) - 1;
	}
 }

/**
 @brief Busca una unidad libre dentro de la lista enlazada de la memoria.
 * @return Dirección de inicio de la unidad en memoria.
 */
  char * allocate_unit(void) {

	 /*Indica la unidad libre
	 unsigned int unit;
	 Variable de entrada
	 unsigned int entry;
	 Variable de desplazamiento
	 unsigned int offset;*/

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
		 	/*Si encuentra un nodo libre...*/
		 	if( ptr->state == 'L' ) {

		 		/*Si el nodo tiene una sola unidad en la region, establecer su estado en usado.*/
	 			if(ptr->units == 1) {
	 				ptr->state = 'U';
	 			}
	 			else {
	 				/*Si el nodo tiene mas de una unidad libre, se crea un nuevo nodo.
	 				 *Su estado se establece en Libre, el inicio del nodo es el inicio del nodo
	 				 *actual más uno y las unidades en las unidades del nodo actual menos 1.
	 				 *Como se usa la primera unidad de la region libre, el nuevo nodo
	 				 *representa el nuevo espacio libre que queda.
	 				 */

	 				memory_node * new_node = (memory_node*) kmalloc(sizeof(memory_node));

	 				//new_node->start = (ptr->start+1);
	 				//new_node->state = 'L';
	 				//new_node->units = ptr->units-1;

	 				//memory_node *new_node = create_memory_node('L', (ptr->start+1), (ptr->units-1));

	 				/*Establece como usada la primera unidad del nodo actual.*/
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
	 			/*Finalmente se retorna la dirección de inicio de la unidad en memoria.*/
	 			free_units--; /*TODO Comentar free_units*/
	 			return (ptr->start * MEMORY_UNIT_SIZE);
	 		}
	 }
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
    /*Variable iteradora*/
    unsigned int i;
    /*Variable de resultado*/
    int result;

    /* Se calcula el número de unidades a asignar dividiendo el tamaño (en bytes)
     * entre el tamaño de cada unidad. Se debe tener encuenta que unit_count
     * almacena la parte entera de la division.*/
	unit_count = (length / MEMORY_UNIT_SIZE);

	/* Si la división anterior no es exacta, se necesita una unidad más que representa
	 * la parte decimal restante, por cual se aumenta unit_count en uno (1)*/
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
	 * y termina cuando ha encontrado dicho nodo o cuando llega al final de la lista.*/
	for( ptr = kernel_list->mem_head; ptr != 0; ptr = ptr->next) {
		/*Si se encuentra un nodo libre.*/
		if( ptr->state == 'L') {
			/*Si las unidades del nodo actual son mayores que las unidades a asignar, */
			if( ptr->units > unit_count ) {
				/*Se cambia el estado del nodo actual a usado*/
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

				/* Si el nodo auxiliar es nulo indica que el nodo actual (ptr) es la cola de la lista
				 * de gestión de memoria.*/
				if(aux_node == 0) {
					/* Por lo tanto el nuevo nodo será la cola de la lista de memoria. */
					kernel_list->mem_tail = new_node;
				}

				/* El nodo 'Anterior' al auxiliar será ahora el nuevo nodo,
				 * el nodo 'Anterior' al nuevo será el nodo actual (ptr),
				 * el nodo 'Siguiente' al nuevo será el auxiliar nodo
				 * y por ultimo el nodo 'Siguiente' al actual será el nuevo nodo
				 * De esta manera el nuevo nodo que se creo queda en medio del nodo actual
				 * y el siguiente nodo al actual (si existe nodo siguiente al actual) sino
				 * el nuevo nodo quedara como cola de la lista de memoria*/
				aux_node->previous = new_node;
				new_node->previous = ptr;
				new_node->next = aux_node;
				ptr->next = new_node;

				/*Se cambia el valor de inicio del nodo actual por el mismo valor
				 * multiplicado por el tamaño de la unidad de memoria*/
				free_units -= ptr->units;
				return ptr->start * MEMORY_UNIT_SIZE;
			}
			/* Si no se encuentra una unidad de memoria mayor a las unidades solicitadas
			 * el nodo actual cambia su estado a usado 'U' y  */
			else{
				if(ptr->units == unit_count) /*TODO Comentar los de unidad = unit*/
				{
					ptr->state = 'U';
					free_units -= ptr->units; /*TODO Comentar!!!*/
					return ptr->start * MEMORY_UNIT_SIZE;
				}
			}
		}
	}
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

	 /*La variable 'start' se inicializa con la dirección de memoria ya redondeada */
	 start = round_down_to_memory_unit((unsigned int)addr);

	 /*Si el valor de inicio (start) es menor memoria permitida para liberar se retorna*/
	 if (start < allowed_free_start) {return;}

	 /*La unidad será la división entre el inicio y el tamaño de unidad de memoria */
	 unit = start / MEMORY_UNIT_SIZE;

	 /* TODO: Buscar la unidad en la lista de unidades, y marcarla como
	  * disponible.
	  * Es posible que se requiera fusionar nodos en la lista!*/
	 /* SET UNIT */
	 /* TODO comentar -> si se pide liberar una unidad inexistente */
	 if ( unit > max ) {
		 printf(" Warnig address no exist !!! \n");
		 return;
	 }
	 /*TODO Comeentaarr estoo*/
	 if( free_units == (max - min) + 1 ){ return; }

	 /*Se crea un puntero a un nodo de memoria para recorrer la lista de memoria.*/
	 memory_node *ptr;
	 /*Se crea un nodo de memoria auxiliar 'nodel'.*/
	 memory_node *new_nodel;
	 /*Se crea un nodo de memoria auxiliar 'noder'.*/
	 memory_node *new_noder;
	 /*Se crea un nodo de memoria auxiliar 'naux'.*/
	 memory_node *new_naux;

	 /* Recorre la lista de memoria buscando la unidad en las lista de unidades y la
	  * marca como disponible cuando la encuentre.
	  * Inicia desde la cabeza de la lista de nodos de memoria
	  * y termina cuando ha encontrado dicho nodo o cuando llega al final de la lista.*/
	 for( ptr = kernel_list->mem_head; ptr != 0; ptr = ptr->next) {
		 new_nodel = ptr->previous; // left (previous);
		 new_noder = ptr->next; // right (next);
		 if(ptr->start == unit && ptr->state == 'U'){
			 if(ptr->units == 1){
				 ptr->state = 'L';
				 /*TODO Comentar Mezclas de nodos*/

				 if( new_nodel->state == 'L'){
					 if( new_noder->state == 'L'){
						 // left and right LIBRES
						 new_nodel->units += ptr->units + new_noder->units;

						 /*TODO Faltaba controlar lo de la cola*/

						 if(new_noder->next == 0){ /*Si es la colaa*/
							 kernel_list->mem_tail = new_nodel;
						 }
						 else {
							 new_nodel->next = new_noder->next;
							 new_naux = new_noder->next;
							 new_naux->previous = new_nodel;
						 }

						 /*TODO Se libera memoria porq se quito el nodo de la derecha de ptr*/
						 new_noder->previous = 0;
						 new_noder->next = 0;
						 kfree(new_noder);
					 }
					 else{
						 // left LIBRE and right USADO
						 new_nodel->units += ptr->units;
						 new_noder->previous = new_nodel;
						 new_nodel->next = new_noder;
					 }
					 /*TODO Comentar liberacion de memoria, ya que el nodo ptr no va en la lista*/
					 ptr->next = 0;
					 ptr->previous = 0;
					 kfree(ptr);
				 }
				 else{
					 if( new_noder->state == 'L'){
					 	// left USADO and right LIBRE
						 ptr->units += new_noder->units;
						 new_noder = ptr->next;
						 if(kernel_list->mem_tail == new_noder ){ /*Si ptr->next es la colaa*/
							 kernel_list->mem_tail = ptr;
						 }
						 else{
							 ptr->next = new_noder->next;
							 new_naux = new_noder->next;
							 new_naux->previous = ptr;
						 }
						 /*TODO Se liberra memoria, porque se desenlazo new_noder del kernel_list*/
						 new_noder->next = 0;
						 new_noder->previous = 0;
						 kfree(new_noder);
					 }
				 }
				 free_units ++;/*TODO free_units*/
				 break;
			 }
			 else //ptr->units>1
			 {
				 if( new_nodel->state == 'L' ){
					new_nodel->units += 1;
				 	ptr->units -= 1;
				 	ptr->start += 1;
				 }
				 else{ /*new_nodel puede ser usado o nulo, Aqui aparte de esto se controla lo de la cabeza*/
					 memory_node *new_ptr = create_memory_node('U', ptr->start + 1,ptr->units -1);

					 new_ptr->previous = ptr;
					 if(ptr->next == 0){ /*Es decir, si ptr es la cola*/
						 kernel_list->mem_tail = new_ptr;
					 }
					 new_ptr->next = ptr->next;
					 new_noder->previous = new_ptr; /*Ojooo*/

					 ptr->state = 'L';
					 ptr->units = 1;
					 ptr->next = new_ptr;
				 }
				 free_units ++;
				 break;

			 }
	 } // end if
	else if(ptr->start > unit){
		if(new_nodel->state == 'U') {

			if(ptr->start == (unit + 1) && ptr->state == 'L'){ /*Condicional para proceder a mezclar nodos*/
				new_nodel->units -= 1;
				ptr->units += 1;
				ptr->start -= 1;
				free_units ++;
				break;
			}
			/* nuevo nodo usado */
			memory_node *nn = create_memory_node('U',unit+1,ptr->start-(unit+1));

			/* nuevo nodo libre*/
			memory_node *new_node = create_memory_node('L',unit,1);

			new_node->previous = new_nodel;

			/* nuevo usado */
			nn->previous = new_node;
			new_node->next = nn;
			nn->next = ptr;
			ptr->previous = nn;

			new_nodel->units = unit - new_nodel->start;
			new_nodel->next = new_node;
		}
		free_units ++;
		break;
	}
	else if( kernel_list->mem_tail == ptr && ptr->state != 'L'){/*Case special*/
		//printf("unit > (mem_tail->star)\n");
		// new node first
		memory_node *new_st = create_memory_node('U',ptr->start, unit - ptr->start);
		// new node second
		memory_node *new_nd = create_memory_node('L',unit,1);

		new_st->previous = 0; /*Ojoo*/
		new_st->next = new_nd;
		/* Se resta el número de unidades del primer nodo creado*/
		ptr->units -= new_st->units;

		new_nd->previous = new_st;
		new_nd->next = 0;
		/* Se resta la unidad del segundo nodo creado para liberar*/
		ptr->units--;
		ptr->start = unit + 1;

		if(ptr->units <= 0){
			kernel_list->mem_tail = new_nd;
			/*Falta liberar memoria*/
			ptr->next = 0;
			ptr->previous = 0;
			kfree(ptr);
		}
		else{
			ptr->previous = new_nd; //Liberar previous
			new_nd->next = ptr;
		}
		/**/
		if(kernel_list->mem_head != ptr){
			new_nodel->next = new_st;
			new_st->previous = new_nodel;
		}
		else{
			kernel_list->mem_head = new_st;
		}
		free_units ++;
		break;
	}

	 /* Marcar la unidad recien liberada como la proxima unidad
	  * para asignar */
	 next_free_unit = unit;


	 /* Aumentar en 1 el numero de unidades libres */
	 // printf("free_units %d\n", free_units);

 }
}

/**
 * @brief Permite liberar una región de memoria.
 * @param start_addr Dirección de memoria del inicio de la región a liberar.
 * @param length Tamaño de la región a liberar.
 */
void free_region(char * start_addr, unsigned int length) {

	 /* Unidad de inicio de la región a liberar. */
	 unsigned int start;
	 /* última unidad de la región a liberar. */
	 unsigned int end;

	 /* Se redondea la dirección de inicio */
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
/* TODO comentar print_list */
void print_list(){
	memory_node *ptr = (memory_node *) kmalloc( sizeof(memory_node) );
	printf(" Kernel memory_list !!!\n");
	int i;
	for( i = 0, ptr = kernel_list->mem_head; ptr != 0; i++, ptr = ptr->next){
		printf("\tnodo %d\t state %c\t start %d\t units %d\n", i, ptr->state, ptr->start, ptr->units);
	}
	kfree(ptr);
}
void print_list_right_letf(){
	memory_node *ptr = (memory_node *) kmalloc( sizeof(memory_node) );
	printf(" Kernel memory_list !!!\n");
	int i;
	for( i = 0, ptr = kernel_list->mem_tail; ptr != 0; i++, ptr = ptr->previous){
		printf("\tnodo %d\t state %c\t start %d\t units %d\n", i, ptr->state, ptr->start, ptr->units);
	}
	kfree(ptr);
}
/* TODO comentar create_memory_node */
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
/* TODO comentar create_memory_list*/
memory_list *create_memory_list(char * start_addr, unsigned int length){
	memory_list *mem_list;
	memory_node *mem_node;
	mem_node = create_memory_node('L', (unsigned int)start_addr / MEMORY_UNIT_SIZE, length / MEMORY_UNIT_SIZE);

	mem_list = (memory_list *) kmalloc( sizeof(memory_list) );
	mem_list->mem_head = mem_node;
	mem_list->mem_tail = mem_node;

	return mem_list;
}
