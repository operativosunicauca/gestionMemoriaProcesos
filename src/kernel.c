/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 *
 * @brief C�digo de inicializaci�n del kernel en C. Este c�digo recibe el
 * control de start.S y contin�a con la ejecuci�n.
 *
 * @details
 * Luego de configurar la GDT, la IDT, las interrupciones, las excepciones y
 * las IRQ, este c�digo configura el mapa de bits que permitir� gestionar la
 * memoria f�sica en unidades de 4096 bytes. Este mapa de bits se referencia
 * con la variable @ref memory_bitmap (physmem.c), la cual apunta a la
 * direcci�n MMAP_LOCATION.
 *
 * El mapa de bits de memoria ocupa exactamente 128 KB, y permite gestionar
 * hasta 4 GB de memoria f�sica.
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
 * @brief Funci�n principal del kernel. Esta rutina recibe el control del
 * codigo en ensamblador de start.S.
 *
 * 	@param magic  N�mero m�gico pasado por GRUB al c�digo de start,S
 *  @param multiboot_info Apuntador a la estructura de informaci�n multiboot
 */
void cmain(unsigned int magic, void * multiboot_info) {
	unsigned int i;
	unsigned int allocations;

	char * addr;
	char * addr0;
	char * addr1;
	char * addr2;
	char * addr3;

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

	/* Configurar la lista enlazada para gesionar la memoria del kernel */
	setup_memory();

	printf("------- Kernel started -------\n");
	print_list();
	allocate_unit_region(15859*4096);
	print_list();
	free_unit(523*4096);
	print_list();

	free_unit(525*4096);
	print_list();

	//print_list();
	//print_list_right_letf();
	/* Probar la gestion de unidades de memoria */

	/* Reservar una region de 64 KB 16 Unidades*/
	//allocate_unit_region(0xFFFF);
	//print_list();


	/* Reservar una unidad  */
	//addr = allocate_unit();
	//printf("Allocated address: 0x%x = %d\n", addr, addr);

	//allocate_unit_region(5*4096);
	//addr0 = allocate_unit();
	//printf("Allocated address: 0x%x = %d\n", addr0, addr0);
	//addr1 = allocate_unit();
	//printf("Allocated address: 0x%x = %d\n", addr1, addr1);
	//addr2 = allocate_unit();
	//printf("Allocated address: 0x%x = %d\n", addr2, addr2);
	//addr3 = allocate_unit();
	//printf("Allocated address: 0x%x = %d\n", addr3, addr3);

	//print_list();

	//free_unit((char*)addr0);
	//free_unit((char*)addr1);
	//free_unit((char*)addr2);
	//print_list();

	//free_unit(523*4096);
	//print_list();
	//print_list_right_letf();
	//free_unit(524*4096);
	//print_list();
	//printf("free region");

	//free_region(525*4096, 2*4096);
	//print_list();
	//free_unit(526*4096);
	//print_list();

	//free_region(525*4096, 4*4096);
	//print_list();
	/*
	addr3 = allocate_unit();
		printf("0000000000000000000000Allocated address: 0x%x = %d\n", addr3, addr3);
		print_list();
	*/

	//addr = allocate_unit_region(0xFFFF);

	//printf("Allocated region: %x\n", addr);

	//addr = allocate_unit();

	//printf("Allocated address: %x\n", addr);

	//printf("Last allocated address: %x, %u\n",addr, addr);

	inline_assembly("sti");

	printf("------- Kernel finished -------\n");

}
