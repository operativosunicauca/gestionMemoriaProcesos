/**
 * @file
 * @ingroup kernel_code
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License.
 * @brief Punto de entrada del kernel
 * @details
 * El kernel se carga en la dirección de memoria 0x100000 (1 MB) y empieza su
 * ejecución en la etiqueta start.
 * Este kernel cumple con la especificación multiboot, por lo cual debe
 * contener un encabezado (estructura de datos) que le indica al cargador
 * de arranque los parametros necesarios para que cargue el kernel.
 * Este kernel es diferente a los anteriores: Se almacena en una imagen de disco
 * duro, que se encuentra formateada con el sistema de archivos ext2 (linux).
 * Para cargarlo, se recurre a  un cargador de arranque (bootloader) llamado
 * GRUB.
 * GRUB es un bootloader que cumple con la Especificacion Multiboot. Puede
 * cargar cualquier kernel que cumpla con esta especificación.
 * @see http://www.gnu.org/software/grub/manual/multiboot/multiboot.html Especificación Multiboot
*/

/** @verbatim */

.intel_syntax noprefix /* Usar sintaxis Intel, si prefijo para los registros */
/* Seccion llamada 'boot' para almacenarla al principio del archivo ejecutable
de salida (Ver archivo link.ld). La especificacion Multiboot exige que este
encabezado se encuentre en los primeros 8192 bytes del archivo ejecutable. */
.section .boot
.code32				/* 32 bits - Modo protegido */

.globl start	/* Punto de entrada del kernel */

/* Definir la constante ASM para que del archivo multiboot.h solo se
incluyan las constantes*/

#define ASM 1
#include <multiboot.h>

start:
  jmp entry_point

.align 4
.globl multiboot_header
multiboot_header:		/* Encabezado multiboot */
.long MULTIBOOT_HEADER_MAGIC
.long MULTIBOOT_HEADER_FLAGS
.long MULTIBOOT_CHECKSUM

.long multiboot_header /* Header address dirección en la cual se encuentra
						  el encabezado multiboot */

/* kernel_start, data_end, kernel_end son calculados por el linker. Ver
   archivo link.ld*/

.long kernel_start  /* Load address  = la dirección fisica del comienzo
					   del segmento de texto */
.long data_end   /* Load end address = la dirección fisica del final del
					   segmento de datos  */
.long kernel_end   /* BSS end address = dirección fisica del final del segmento
						BSS. El cargador inicializa esta area a cero */
.long entry_point /* Entry address  = dirección fisica a la cual el cargador
					deberia saltar para comenzar la ejecucion del sistema
					operativo */

entry_point: /* Punto de inicio de la ejecucion del kernel*/

  cli

  mov esp, 0x9FC00	/* Tope de la pila en 0x9FC00*/

  /* Reset EFLAGS*/
  push 0
  popf

  /* Enviar los parametros al kernel */
  push ebx /* dirección fisica de memoria en la cual se encuentra
  			   la estructura de informacion multiboot. Esta puede
  			   ser utilizada por el kernel para obtener la informacion
  			   suministrada por el cargador. */
  push eax /* Número magico del cargador de arranque = 0x2BADB002.
  			   En el kernel se puede validar si se recibio este valor. En caso
  			   afirmativo, el kernel fue cargado por un cargador que cumple
  			   con la especificacion multiboot. */

  call cmain /* Pasar el control a la rutina 'cmain' en elarchivo kernel.c */

  add sp, 8

  /* La función cmain() retorna a este punto. Se debe entrar en un ciclo
  infinito, para que el procesador no siga ejecutando instrucciones al finalizar
  la ejecución del kernel. */

loop:	hlt
	jmp loop /* Ciclo infinito */


/**
@endverbatim
*/
