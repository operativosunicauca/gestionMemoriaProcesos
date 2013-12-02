/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 *
 * @brief Contiene algunas constantes necesarias para  el kernel
 * relacionadas con la especificaci�n Multiboot
 * @see http://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 */

#ifndef MULTIBOOT_H_
#define MULTIBOOT_H_

/** @brief Direcci�n f�sica del kernel en memoria */
#define KERNADDR 0x100000

/* Estas FLAGS se pasan a GRUB. Ver especificacion Multiboot. */
/** @brief Alinear los m�dulos cargados a l�mites de p�gina */
#define MULTIBOOT_PAGE_ALIGN 1<<0
/** @brief Proporcionar al kernel informaci�n de la memoria disponible */
#define MULTIBOOT_MEMORY_INFO 1<<1
/** @brief Proporcionar al kernel informaci�n de los modos de video  */
#define MULTIBOOT_VIDEO_INFO 1<<2
/** @brief Solicitar a Grub que use las direcciones proporcionados en el
 * encabezado multiboot */
#define MULTIBOOT_AOUT_KLUDGE 1<<16
/** @brief N�mero m�gico de la especificac�n multiboot */
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
/** @brief Constante que incluye las flags que se pasar�n a GRUB */
#define MULTIBOOT_HEADER_FLAGS MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | \
							   MULTIBOOT_AOUT_KLUDGE
/** @brief Constante de suma de chequeo*/
#define MULTIBOOT_CHECKSUM -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

/** @brief N�mero m�gico que el cargador de arranque almacena en el registro
 * EAX para indicar que es compatible con la especificaci�n Multiboot */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* No incluir de aqui en adelante si se incluye este archivo desde codigo
 * en ensamblador */
#ifndef ASM

/** @brief Definici�n del tipo de datos del Encabezado Multiboot */
typedef struct multiboot_header_struct {
	/** @brief N�mero m�gico */
	unsigned int magic;
	/** @brief Flags pasadas a GRUB para solicitar informaci�n / servicios. */
	unsigned int flags;
	/** @brief N�mero de chequeo. Este valor debe ser igual a la suma de
	 * magic y flags. */
	unsigned int checksum;
	/** @brief Desplazamiento (Direcci�n) en el cual se encuentra el encabezado
	 * dentro del archivo del kernel */
	unsigned int header_addr;
	/** @brief Direcci�n en la cual se debe cargar el kernel */
	unsigned int kernel_start;
	/** @brief Direcci�n del final de la secci�n de datos del kernel */
	unsigned int data_end;
	/** @brief Direcci�n del final de la secci�n BSS del kernel  */
	unsigned int bss_end;
	/** @brief Direcci�n en la cual se debe pasar el control al kernel */
	unsigned int entry_point;
} multiboot_header_t;

/** @brief Tabla de s�mbolos usadas en el formato a.out*/
typedef struct aout_symbol_table
{
	unsigned int tabsize;
	unsigned int strsize;
	unsigned int addr;
	unsigned int reserved;
} aout_symbol_table_t;

/** @brief Tabla de encabezados de secci�n del kernel en el formato elf */
typedef struct elf_section_header_table
{
	unsigned int num;
	unsigned int size;
	unsigned int addr;
	unsigned int shndx;
}elf_section_header_table_t;

/** @brief Estructura de datos que almacena la informaci�n de una
 * regi�n de memoria dentro del mapa de memoria  proporcionado por GRUB. */
typedef struct memory_map
{
 /** @brief Campo no usado */
 unsigned int entry_size;
 /** @brief 32 bits menos significativos de la base de la regi�n de memoria */
 unsigned int base_addr_low;
 /** @brief 32 bits m�s significativos de la base de la regi�n de memoria */
 unsigned int base_addr_high;
 /** @brief 32 bits menos significativos del tama�o de la regi�n de memoria */
 unsigned int length_low;
 /** @brief 32 bits m�s significativos del tama�o de la regi�n de memoria */
 unsigned int length_high;
 /** @brief Tipo de �rea de memoria 1 = disponible, 2 = reservada */
 unsigned int type;
} memory_map_t;

/** @brief Estructura de datos que almacena la informaci�n de un m�dulo
 * cargado por GRUB. */
typedef struct mod_info  {
	/** @brief Direcci�n en la cual se carg� el m�dulo */
	unsigned int mod_start;
	/** @brief Tama�o en bytes del m�dulo cargado */
	unsigned int mod_end;
	/** @brief 32 Comando usado para cargar el m�dulo  */
	char * string;
	unsigned int always0;
}mod_info_t;

/** @brief Estructura de informaci�n Multiboot. Al cargar el kernel,
 * GRUB almacena en el registro EBX un apuntador a la direcci�n de memoria
 * en la que se encuentra esta estructura. */
typedef struct {
	/** @brief Versi�n e informaci�n de Multiboot. El kernel deber� comprobar
	 * sus bits para verificar si GRUB le pas� la informaci�n solicitada. */
	unsigned int flags;
	/** @brief Presente si flags[0] = 1 Memoria baja reportada por la BIOS*/
	unsigned int mem_lower;
	/** @brief Presente si flags[0] = 1 Memoria alta reportada por la BIOS*/
	unsigned int mem_upper;
	/** @brief Presente si flags[1] = 1 Dispositivo desde el cual se carg�
	 * el kernel. */
	unsigned int boot_device;
	/** @brief Presente si flags[2] = 1 L�nea de comandos usada para cargar
	 * el kernel */
	unsigned int cmdline;
	/** @brief Presente si flags[3] = 1 N�mero de m�dulos cargados junto
	 * con el kernel */
	unsigned int mods_count;
	/** @brief Presente si flags[3] = 1 Direcci�n de memoria en la cual se
	 * encuentra la informaci�n de los m�dulos cargados por el kernel. */
	unsigned int mods_addr;

	/** @brief Presente si flags[4] = 1 o flags[5] = 1. Informaci�n de s�mbolos
	 * a.out o de secciones ELF del kernel cargado. */
	union {
		aout_symbol_table_t aout_symbol_table;
		elf_section_header_table_t elf_section_table;
	}syms;

	/** @brief Presente si flags[6] = 1. Tama�o del mapa de memoria creado
	 * por GRUB*/
	unsigned int mmap_length;
	/** @brief Presente si flags[6] = 1. Direcci�n f�sica de la ubicaci�n del
	 * mapa de memoria creado por GRUB. */
	unsigned int mmap_addr;

	/** @brief Presente si flags[7] = 1. Especifica el tama�o total de la
	 * estructura que describe los drives reportados por la BIOS.
	 */
	unsigned int drives_length;/* Presente si flags[7] = 1 */
	/** @brief Presente si flags[7] = 1. Especifica la direcci�n de memoria en
	 * la que se encuentra la estructura que describe los drivers reportados
	 * por la BIOS.
	 */
	unsigned int drives_addr;	/* Presente si flags[7] = 1 */
	/** @brief Presente si flags[8] = 1. Especifica la direcci�n de la tabla
	 * de configuraci�n de la BIOS. */
	unsigned int config_table;
	/** @brief Presente si flags[9] = 1. Contiene la direcci�n de memoria
	 * en la cual se encuentra una cadena de caracteres con el nombre del
	 * cargador de arranque. */
	unsigned int boot_loader_name;

	/** @brief Presente si flags[10] = 1. Especifica la localizaci�n en la
	 * memoria de la tabla APM. */
	unsigned int apm_table;

	/** @brief Presente si flags[11] = 1. Contiene la informaci�n de control
	 * retornada por la funci�n vbe 00
	 */
	unsigned int vbe_control_info;
	/** @brief Presente si flags[11] = 1. Contiene la informaci�n de modo
	 * retornada por la funci�n vbe 00
	 */
	unsigned int vbe_mode_info;

	/** @brief Presente si flags[11] = 1. */
	unsigned int vbe_mode;

	/** @brief Presente si flags[11] = 1. */
	unsigned short vbe_interface_seg;
	/** @brief Presente si flags[11] = 1. */
	unsigned short vbe_interface_off;
	/** @brief Presente si flags[11] = 1. */
	unsigned short vbe_interface_len;

}multiboot_info_t;

#endif

#endif
