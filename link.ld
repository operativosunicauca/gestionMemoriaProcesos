/** 
   @file
   @author Erwin Meza <emezav@gmail.com> 
   @brief Archivo  para realizar el enlace (linking) del kernel.
   Este archivo toma los archivos de objeto y organiza el archivo
   final. 
*/
/** @verbatim */
/* 
   Se supone que el kernel se carga en la dirección de memoria 0x100000.
*/

ENTRY(start);
OUTPUT_FORMAT(elf32-i386);
phys = 0x100000;
virt = 0x100000;
SECTIONS {
   . = phys;
   kernel_start = .;	/* dirección en la cual se carga el kernel */
   .text phys : AT(virt) {
     code_start = .;	/* Inicio de la seccion de codigo */
     *(.boot)	/* Incluir primero el codigo ejecutable de start.S */
     *(.text)
     *(.rodata)
     . = ALIGN(4096);
     code_end = .;
   } = 0x90909090
   .data  : AT (virt + (data_start - code_start)) {
       data_start = .;
       *(.data)
     . = ALIGN(4096);
       data_end = .;
   } = 0x00000000
   .bss : AT (phys + (bss_start - code_start)) {
       bss_start = .;
       *(.bss)
       *(COMMON)
     . = ALIGN(4096);
       bss_end = .;
   } = 0x00000000
   
   kernel_end = .;      
}

/** @endverbatim */