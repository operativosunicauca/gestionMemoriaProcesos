/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 *
 * @brief Código de inicialización del kernel en C. Este código recibe el
 * control de start.S y continúa con la ejecución.
 *
 * @details
 * Luego de configurar la GDT, la IDT, las interrupciones, las excepciones y
 * las IRQ, este código configura el mapa de bits que permitirá gestionar la
 * memoria física en unidades de 4096 bytes. Este mapa de bits se referencia
 * con la variable @ref memory_bitmap (physmem.c), la cual apunta a la
 * dirección MMAP_LOCATION.
 *
 * El mapa de bits de memoria ocupa exactamente 128 KB, y permite gestionar
 * hasta 4 GB de memoria física.
 *
 */

#include <pm.h>
#include <multiboot.h>
#include <stdio.h>
#include <stdlib.h>
#include <idt.h>
#include <physmem.h>

/** @brief Variable global del kernel que almacena la localizacion de la
 * estructura multiboot */
unsigned int multiboot_info_location;

/**
 * @brief Función principal del kernel. Esta rutina recibe el control del
 * codigo en ensamblador de start.S.
 *
 * 	@param magic  Número mágico pasado por GRUB al código de start,S
 *  @param multiboot_info Apuntador a la estructura de información multiboot
 */
void cmain(unsigned int magic, void * multiboot_info) {
	unsigned int i;
	unsigned int allocations;

	char * addr;

	/* Referencia a la estructura de datos multiboot_header en start.S */
	extern multiboot_header_t multiboot_header;

	multiboot_info_location = (unsigned int) multiboot_info;

	cls();

	/* Configurar y cargar la GDT definida en pm.c*/
	setup_gdt();

	/* Configurar y cargar la IDT definida en idt.c */
	setup_idt();

	/* Configurar las excepciones definidas en IA-32 */
	setup_exceptions();

	/* Configurar las IRQ */
	setup_irq();

	/* Configurar el mapa de bits de memoria del kernel */
	setup_memory();

	printf("Kernel started\n");

	/* Probar la gestion de unidades de memoria */

	/* Reservar una unidad  */
	addr = allocate_unit();
	printf("Allocated address: %x = %d\n", addr, addr);

	addr = allocate_unit_region(0xFFFF);

	printf("Allocated region: %x\n", addr);

	addr = allocate_unit();

	printf("Allocated address: %x\n", addr);

	printf("Last allocated address: %x, %u\n",addr, addr);

	inline_assembly("sti");

	printf("Kernel finished\n");

}
