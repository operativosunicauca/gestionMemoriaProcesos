/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene la implementación de las rutinas para la gestión del modo
 * protegido IA-32 y de la Tabla Global de Descriptores (GDT)
 */

#include <pm.h>
#include <stdio.h>

/** @brief Tabla Global de Descriptores (GDT). Es un arreglo de descriptores
 * de segmento. Según el manual de Intel, esta tabla debe estar alineada a un
 * límite de 8 bytes para un óptimo desempeño. */
gdt_descriptor gdt[MAX_GDT_ENTRIES] __attribute((aligned(8)));

/** @brief Variable que almacena la siguiente entrada disponible en la GDT */
int current_gdt_entry = 0;

/** @brief Variable que almacena el selector del descriptor de segmento de
 * código del kernel */
unsigned short kernel_code_selector;

/** @brief Referencia al Descriptor de segmento de codigo del kernel dentro de
 * la GDT*/
gdt_descriptor * kernel_code_descriptor;

/** @brief  Variable que almacena el selector del descriptor de segmento de
 * datos para el kernel */
unsigned short kernel_data_selector;

/** @brief Referencia al Descriptor de segmento de datos del kernel */
gdt_descriptor * kernel_data_descriptor;

/** @brief Apuntador a la GDT usado por la instrucción lgdt  para cargar
 * la GDT*/
gdt_ptr gdt_pointer;

/**
 * @brief Función que permite obtener el selector en la GDT a partir de un
 * apuntador a un descriptor de segmento
 * @param desc Referencia al descriptor de segmento del cual se desea obtener
 * el selector
 * @return Selector que referencia al descriptor dentro de la GDT.
 */
unsigned short get_gdt_selector(gdt_descriptor * desc) {
	unsigned short offset = (unsigned short)((char*)desc - (char*)gdt);
	if (offset < 0) {return 0;}
	if (offset > sizeof(gdt_descriptor) * MAX_GDT_ENTRIES) {return 0;}
	if (offset % sizeof(gdt_descriptor)) {
		return 0;
	}
	return offset;
}


/**
 * @brief Función que permite obtener el descriptor de segmento en la GDT a
 * partir de un selector
 * @param selector Selector que permite obtener un descriptor de segmento
 * de la GDT
 * @return Referencia al descriptor de segmento dentro de la GDT
 */
gdt_descriptor * get_gdt_descriptor(unsigned short selector) {
	if (selector < 0) {return 0;}
	if (selector > sizeof(gdt_descriptor) * MAX_GDT_ENTRIES) {return 0;}
	if (selector % sizeof(gdt_descriptor)) {
		return 0;
	}
	return &gdt[selector>>3];
}

/**
 * @brief Esta rutina permite obtener un descriptor de segmento
 *  disponible en la GDT.
 *  @return Referencia al próximo descriptor de segmento dentro de la GDT que se
 *  encuentra disponible, nulo en caso que no exista una entrada disponible
 *  dentro de la GDT.
 */
gdt_descriptor * allocate_gdt_descriptor(void) {
	unsigned short next_gdt_entry;


	next_gdt_entry = current_gdt_entry;

	do {
			if(next_gdt_entry != 0  /* Valida? */
					&& gdt[next_gdt_entry].low == 0 /* Entrada vacia? */
					&& 	gdt[next_gdt_entry].high == 0){
				/*Entrada valida!*/
				/* Marcar la entrada como 'Presente' para evitar
				 * que una llamada concurrente encuentre la misma entrada */
				gdt[next_gdt_entry].high |= 1 << 15;
				current_gdt_entry = (current_gdt_entry + 1) % MAX_GDT_ENTRIES;
				return &gdt[next_gdt_entry];
			}
			next_gdt_entry = (next_gdt_entry + 1) % MAX_GDT_ENTRIES;
	}while (next_gdt_entry > current_gdt_entry);
	return 0;
}

/**
 * @brief Buscar un selector disponible dentro de la GDT
 * @return Selector que apunta al descriptor de segmento disponible encontrado
 * dentro de la GDT
 */
unsigned short allocate_gdt_selector(void) {
	gdt_descriptor * desc = allocate_gdt_descriptor();
	if (desc == 0) {return 0;}
	return get_gdt_selector(desc);
}

/**
 * @brief Esta rutina permite liberar un descriptor de segmento en la GDT.
 * @param desc Apuntador al descriptor que se desa liberar
 * */
void free_gdt_descriptor(gdt_descriptor *desc) {
	unsigned short selector = get_gdt_selector(desc);
	unsigned int index;

	if (selector == 0) {return;}
		index = selector >> 3;
		gdt[index].low = 0;
		gdt[index].high = 0;
		current_gdt_entry = index;
}


/**
 * @brief Permite configurar un descriptor de segmento dentro de la GDT
 * @param selector Selector que referencia al descriptor de segmento
 * dentro de la GDT
 * @param base Dirección lineal del inicio del segmento en memoria
 * @param limit Tamaño del segmento
 * @param type Tipo de segmento
 * @param dpl Nivel de privilegios del segmento
 * @param code_or_data 1 = Segmento de código o datos, 0 = segmento del
 * sistema
 * @param opsize Tamaño de operandos: 0 = 16 bits, 1 = 32 bits
 */
void setup_gdt_descriptor(unsigned short selector , unsigned int base,
		unsigned int limit, char type, char dpl, int code_or_data, char opsize)
{

	gdt_descriptor * desc;

	desc = get_gdt_descriptor(selector);

	if (desc == 0) {return;}

	desc->low = (	/* Base 0..15 */
					((base & 0x0000FFFF) << 16) |
					/* limit 0..15 */
					(limit & 0x0000FFFF));

	desc->high = (  /* Base 24..31 */
					(base & 0xFF000000) |
					/* C = (G=1, D/B=?, L=0, AVL=0) */
					(limit & 0x008F0000) |
					((opsize & 0x1) << 22) |
					/* P = 1, S = code_or_data */
					(0x8000 | ((code_or_data & 1) << 12)) |
					((dpl & 0x03) << 13) |
					(type & 0x0F) << 8 |
					((base & 0x00FF0000) >> 16)
					);
}

/**
 * @brief Esta función se encarga de cargar la GDT.
 * @details
 * Esta función se encarga de configurar la Tabla Global de Descriptores (GDT)
 * que usará el kernel. La GDT es un arreglo de descriptores de segmento, cada
 * uno de los cuales contiene la información de los segmentos en memoria.
 * Esta rutina realiza los siguientes pasos:
 * */
void setup_gdt(void) {

	int i;

	/** - Buscar el espacio en la GDT para el descriptor de segmento de código
	 * del kernel */
	kernel_code_selector = allocate_gdt_selector();

	/** - Verificar el offset dentro de la GDT. Debe ser igual a la constante
	 * @ref KERNEL_CODE_SELECTOR Definida en pm.h */
	if (kernel_code_selector != KERNEL_CODE_SELECTOR){
		printf("Kernel code selector must be %x\n", KERNEL_CODE_SELECTOR);
		for (;;);
	}

	/** - Buscar espacio en la GDT para el descriptor de segmento de datos
	 * del kernel */
	kernel_data_selector = allocate_gdt_selector();

	/** - Verificar el offset dentro de la GDT. Debe ser igual a la constante
	 * @ref KERNEL_DATA_SELECTOR definida en pm.h */
	if (kernel_data_selector != KERNEL_DATA_SELECTOR){
		printf("Kernel data selector must be %x\n", KERNEL_DATA_SELECTOR);
		for (;;);
	}

	/** - Definir el segmento de código del kernel como un segmento plano
	 * (base = 0, límite = 4 GB, nivel de privilegios 0). Para describir este
    * segmento se usará la segunda entrada de la GDT. El manual de Intel
    * especifica que la primera entrada de la GDT siempre debe ser nula.*/
	setup_gdt_descriptor(kernel_code_selector,0, 0xFFFFFFFF,
			CODE_SEGMENT, 0, 1, 1);

	/** - Definir el segmento de datos del kernel como un segmento plano
    * (base = 0, límite = 4 GB, nivel de privilegios 0). Para describir este
    * segmento se usará la tercera entrada de la GDT. */
	setup_gdt_descriptor(kernel_data_selector,0, 0xFFFFFFFF,
			DATA_SEGMENT, 0, 1, 1);

	/** La instrucción LGDT recibe un apuntador que tiene dos atributos:
	 * Tamaño del GDT - 1 y dirección lineal de la GDT en memoria. */
	gdt_pointer.limit = sizeof(gdt_descriptor)*MAX_GDT_ENTRIES - 1;

	/* dirección lineal de 32 bits en donde se encuentra la GDT. */
	gdt_pointer.base = (unsigned int) &gdt;

	/** Ejecutar el siguiente código en ensamblador, para cargar la GDT y pasar
	 * "de nuevo" a modo protegido. */
	/** @verbatim */
	inline_assembly(".intel_syntax noprefix\n\t\
                            cli                            \n\t\
                            lgdt [%0]                      \n\t\
                            mov edx, cr0                   \n\t\
                            or edx, 1                      \n\t\
                            mov cr0, edx                   \n\t\
                            push %2                        \n\t\
                            push 0  /* eflags */           \n\t\
                            push %1                        \n\t\
                            push OFFSET 1f                 \n\t\
                            iret                           \n\t\
                 1:                                        \n\t\
                            pop ecx                        \n\t\
                            mov ds, cx                     \n\t\
                            mov es, cx                     \n\t\
                            mov fs, cx                     \n\t\
                            mov gs, cx                     \n\t\
                            mov ss, cx                     \n\t\
                            .att_syntax prefix\n\t"
                            :
                            : "am"(&gdt_pointer),
                            "b"(kernel_code_selector & 0x0000FFFF),
                            "c"(kernel_data_selector & 0x0000FFFF)
                            : "dx");

	/** @endverbatim*/

	/** Este código en lenguaje ensamblador realiza las siguientes acciones:
	 *  -# Deshabilitar las interrupciones
	 *  -# Ejecutar la instrucción lgdt, pasando como parámetro un apuntador
	 *   a la GDT configurada.
	 *  -# Activar el bit PE (Protection Enable) en el registro CR0.
	 *  -# Simular un retorno de interrupción, para que el registro de segmento
	 *  CS contenga el valor del selector que referencia al descriptor de
	 *  segmento de código configurado en la GDT.
	 *  -# Actualizar los registros de segmento DS, ES, FS, GS y SS para que
	 *  contengan el valor del selector que referencia al descriptor de
	 *  segmento de datos configurado en la GDT.
	 *
	 *  Observe que básicamente se está pasando de nuevo a modo protegido, esta
	 *  vez usando la GDT configurada en esta función.
	 */

}
