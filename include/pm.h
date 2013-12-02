/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene las definiciones relacionadas con el Modo Protegido IA-32.
 */

#ifndef PM_H_
#define PM_H_

/** @brief Número m{aximo de entradas de la GDT */
#define MAX_GDT_ENTRIES 1024

/** @brief Tipo de segmento de código */
#define CODE_SEGMENT 0xA
/** @brief Tipo de segmento de datos*/
#define DATA_SEGMENT 0x2

/** @brief Desplazamiento en bytes dentro de la GDT a partir del cual se
 * encuentra el descriptor de segmento de código del kernel. Se debe tener en
 * cuenta que cada descriptor de segmento ocupa 8 bytes. */
#define KERNEL_CODE_SELECTOR 0x08

/** @brief Desplazamiento en bytes dentro de la GDT a partir del cual se
 * encuentra el descriptor de segmento de datos del kernel. Se debe tener en
 * cuenta que cada descriptor de segmento ocupa 8 bytes. */
#define KERNEL_DATA_SELECTOR 0x10

/** @brief Nivel de privilegios 0*/
#define RING0_DPL 0
/** @brief Nivel de privilegios 1*/
#define RING1_DPL 1
/** @brief Nivel de privilegios 2*/
#define RING2_DPL 2
/** @brief Nivel de privilegios 3*/
#define RING3_DPL 3

/* Dado que este archivo puede ser incluido desde codigo en Assembler, incluir
 * solo las constantes definidas anteriormente. */

#ifndef ASM

/** @brief Estructura de datos para un descriptor de segmento
 * @details
De acuerdo con el manual de Intel, un descriptor de segmento en modo
protegido de 32 bits es una estructura de datos que ocupa 8 bytes
(64 bits), con el siguiente formato (Ver Cap. 3, Seccion 3.4.5, en
Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume 3A:
System Programming Guide, Part 1)

@verbatim
31                               16     12 11    8 7              0
 ------------------------------------------------------------------
|                | |D| |A|        | |   | |       |                |
|  Base 24..31   |G|/|L|V| Limite |P|DPL|S| Tipo  |   Base 16..23  |
|                | |B| |L| 16..19 | |   | |       |                |
 ------------------------------------------------------------------  MSB
 31                               16 15                           0
 ------------------------------------------------------------------
|                                 |                                |
|        Base 0..15               |          Limite 0..15          |
|                                 |                                |
 ------------------------------------------------------------------  LSB

@endverbatim

La distribución de los bits dentro del descriptor se explica a continuación:
- Los bits Base 0..31 permiten definir la base del segmento en el espacio
lineal de 32 bits.
- Los bits Limite 0..19 permiten definir el tamaño del segmento. Este límite
también se relaciona con el bit G (Granularidad) de la siguiente forma:
   - Si G es 0, el tamaño del segmento en bytes es igual al valor
     almacenado en Limite
   - Si G = 1, el tamaño del segmento es el valor almacenado en Limite
     multiplicado por 4096.
- El bit D/B funciona diferente para para segmentos de codigo y datos. Para modo
protegido de 32 bits este bit debe tener valor de 1. Consulte el manual
de Intel para más detalles.
- El bit L debe ser 0 para modo protegido de 32 bits.
- El bit AVL puede ser tener como valor 1 o 0. Por defecto se toma 0.
- El bit P es 1 si el segmento está presente, 0 en caso contrario.
- El bit DPL define el nivel de privilegios del descriptor. Por tener 2 bits
  puede almacenar tres valores: 0, 1 y 2. 0 es el mayor privilegio.
- El bit S para descriptores de segmento de codigo o datos debe ser 1.
- Los bits correspondientes a Tipo definen el tipo de segmento. Vea la sección
3.5.1 del manual de Intel mencionado.
   - Para segmentos de código, Tipo tiene el valor binario de 1010 = 0xA
   - Para segmentos de datos, Tipo tiene el valor binario de 0010 = 0x2
*/
/** @brief Estructura de datos para un descriptor de segmento */
struct gdt_descriptor {
	/** @brief  Bits menos significativos del descriptor. Agrupan
	 * Limite 0..15 y Base 0..15 del descriptor*/
	 unsigned int low : 32;
	 /** bits más significativos del descriptor. Agrupan Base 16..23, Tipo,
	  * S, DPL, P, Límite, AVL, L, D/B, G y Base 24..31*/
	 unsigned int high: 32;
}__attribute__((packed));

/** @brief  Tipo de datos para el descriptor de segmento  */
typedef struct gdt_descriptor gdt_descriptor;

/** @brief Estructura de datos para el registro GDTR (puntero a la GDT) */
struct gdt_pointer_t {
	unsigned short limit; /* Tamanio de la GDT */
	unsigned int base; /* dirección lineal del inicio de la GDT */
} __attribute__ ((packed));

/** @brief Tipo de datos para el apuntador a la GDT */
typedef struct gdt_pointer_t gdt_ptr;

/** @brief Tabla Global de Descriptores (GDT). Es un arreglo de descriptores
 * de segmento. Según el manual de Intel, esta tabla debe estar alineada a un
 * límite de 8 bytes para un óptimo desempeño. */
extern gdt_descriptor gdt[];

/**
 * @brief Función que permite obtener el selector en la GDT a partir de un
 * apuntador a un descriptor de segmento
 * @param desc Referencia al descriptor de segmento del cual se desea obtener
 * el selector
 * @return Selector que referencia al descriptor dentro de la GDT.
 */
unsigned short get_gdt_selector(gdt_descriptor * desc);

/**
 * @brief Función que permite obtener el descriptor de segmento en la GDT a
 * partir de un selector
 * @param selector Selector que permite obtener un descriptor de segmento
 * de la GDT
 * @return Referencia al descriptor de segmento dentro de la GDT
 */
gdt_descriptor * get_gdt_descriptor(unsigned short selector);

/**
 * @brief Esta rutina permite obtener un descriptor de segmento
 *  disponible en la GDT.
 */
gdt_descriptor * allocate_gdt_descriptor(void);

/**
 * @brief Esta rutina permite obtener un descriptor de segmento
 *  disponible en la GDT.
 *  @return Referencia al próximo descriptor de segmento dentro de la GDT que se
 *  encuentra disponible, nulo en caso que no exista una entrada disponible
 *  dentro de la GDT.
 */
unsigned short allocate_gdt_selector(void);

/**
 * @brief Esta rutina permite liberar un descriptor de segmento en la GDT.
 * @param desc Apuntador al descriptor que se desa liberar
 * */
void free_gdt_descriptor(gdt_descriptor *);

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
void setup_gdt_descriptor(unsigned short , unsigned int,
		unsigned int, char, char, int, char);

/**
 * @brief Esta función se encarga de cargar la GDT.
 * @details
 * Esta función se encarga de configurar la Tabla Global de Descriptores (GDT)
 * que usará el kernel. La GDT es un arreglo de descriptores de segmento, cada
 * uno de los cuales contiene la información de los segmentos en memoria.
 * Esta rutina realiza los siguientes pasos:
 * */
void setup_gdt(void);

#endif

#endif /* PM_H_ */
