/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Contiene las definiciones globales requeridas para
 * el manejo de interrupciones en la arquitectura IA-32
 */

#ifndef IDT_H_
#define IDT_H_

/** @brief N�mero de entradas en la IDT: 256 en la arquitectura IA-32. */
#define MAX_IDT_ENTRIES 256

/** @brief Constante para el tipo de descriptor 'interrupt_gate' */
#define INTERRUPT_GATE_TYPE 0x0E

/** @brief Valor de el registro EFLAGS, con el bit IF = 1. El bit 1 siempre debe
 * ser 1. */
#define IF_ENABLE 0x202


/** @brief Definici�n de la estructura de datos para un descriptor de
 * interrupci�n */
struct idt_descriptor {
	/** Bits menos significativos del desplazamiento dentro del segmento de
	 * c�digo en el cual se encuentra la rutina de manejo de interrupci�n */
	unsigned short offset_low  : 16;
	/** Selector del segmento de c�digo en el cual se encuentra la rutina de
	 * manejo de interrupci�n */
	unsigned short selector : 16;
	/** Tipo del descriptor */
	unsigned short type : 16;
	/** Bits m�s significativos del desplazamiento dentro del segmento de
	 * c�digo en el cual se encuentra la rutina de manejo de interrupci�n */
	unsigned int offset_high: 16;
}__attribute__((packed));

/** @brief Definici�n del tipo de datos para el descriptor de segmento  */
typedef struct idt_descriptor idt_descriptor;

/** @brief Estructura de datos para el registro IDTR (puntero a la IDT) */
struct idt_pointer_t {
	/** @brief Tama�o de la IDT */
	unsigned short limit;
	/** @brief direcci�n lineal del inicio de la IDT */
	unsigned int base;
} __attribute__ ((packed));

/** @brief Definici�n del tipo de datos para el apuntador a la GDT */
typedef struct idt_pointer_t idt_ptr;

/** @brief Referencia a la tabla de rutinas de servicio de interrupcion.
 *  Esta tabla se encuentra definida en el archivo isr.S. */
extern unsigned int isr_table[];

/** @brief Referencia a la tabla de descriptores de interrupcion */
extern idt_descriptor idt[];

/** @brief Estructura que define el estado del procesador al recibir una
 * interrupci�n o una excepci�n.
 * @details Al recibir una interrupci�n, el procesador autom�ticamente almacena
 * en la pila el valor de CS, EIP y EFLAGS. Si la interrupci�n ocurri� en un
 * nivel de privilegios diferente de cero, antes de almacenar CS, EIP y EFLAGS
 * se almacena el valor de SS y ESP.
 * El control lo recibe el c�digo del archivo isr.S, en el cual almacena
 * (en orden inverso) el estado del procesador contenido en esta estructura.
 */
typedef struct interrupt_state {
	/** @brief Valor del selector GS (Tope de la pila) */
	unsigned int gs;
	/** @brief Valor del selector FS */
	unsigned int fs;
	/** @brief Valor del selector ES */
	unsigned int es;
	/** @brief Valor del selector DS */
	unsigned int ds;
	/** @brief Valor del registro EDI */
	unsigned int edi;
	/** @brief Valor del registro  ESI */
	unsigned int esi;
	/** @brief Valor del registro  EBP */
	unsigned int ebp;
	/** @brief Valor del registro  ESP */
	unsigned int esp;
	/** @brief Valor del registro  EBX */
	unsigned int ebx;
	/** @brief Valor del registro  EDX */
	unsigned int edx;
	/** @brief Valor del registro  ECX */
	unsigned int ecx;
	/** @brief Valor del registro  EAX */
	unsigned int eax;
	/** @brief N�mero de la interrupci�n (o excepci�n) */
	unsigned int number;
	/** @brief C�digo de error. Cero para las excepciones que no generan
	 * c�digo de error y para las interrupciones. */
	unsigned int error_code;
	/** @brief Valor de EIP en el momento en que ocurri� la interrupci�n
	 * (almacenado autom�ticamente por el procesador) */
	unsigned int old_eip;
	/** @brief Valor de CS en el momento en que ocurri� la interrupci�n
	* (almacenado autom�ticamente por el procesador) */
	unsigned int old_cs;
	/** @brief Valor de EFLAGS en el momento en que ocurri� la interrupci�n
	* (almacenado autom�ticamente por el procesador) */
	unsigned int old_eflags;
	/** @brief Valor de ESP en el momento en que ocurri� la interrupci�n
	* (almacenado autom�ticamente por el procesador). S�lo se almacena cuando
	* la interrupci�n o excepci�n ocurri� cuado una tarea de privilegio
	* mayor a cero se estaba ejecutando. */
	unsigned int old_esp;
	/** @brief Valor de SS en el momento en que ocurri� la interrupci�n
	* (almacenado autom�ticamente por el procesador). S�lo se almacena cuando
	* la interrupci�n o excepci�n ocurri� cuado una tarea de privilegio
	* mayor a cero se estaba ejecutando. */
	unsigned int old_ss;
} interrupt_state;

/** @brief Definici�n de tipo para las rutinas de manejo de interrupcion */
typedef void (*interrupt_handler)(interrupt_state *);

/** @brief Constante para definir un manejador de interrupcion vacio*/
#define NULL_INTERRUPT_HANDLER (interrupt_handler)0

/**
 * @brief Esta rutina permite crear un descriptor de idt de 32 bits.
 * @param selector  Selector del GDT a partir del cual se puede determinar
 * 					el segmento de codigo dentro del cual se encuentra
 * 					la rutina de manejo de interrupcion.
 * @param offset	Desplazamiento en el cual se encuentra la rutina de manejo
 * 					de interrupcion dentro del segmento especificado por el
 * 					selector.
 * @param dpl		Nivel de privilegios del descriptor
 * @param type		Tipo de descriptor
 * @returns idt_descriptor : Descriptor de idt creado.
 * */
static __inline__ idt_descriptor idt_descriptor_32(unsigned int selector,
		unsigned int offset, unsigned char dpl, unsigned char type) {

	  return (idt_descriptor) {
		  /* offset 0..15*/
		  (unsigned short)(offset & 0x0000FFFF),
		  /* selector */
		  (unsigned short) (selector & 0x0000FFFF),
		  /* tipo de descriptor */
		  (unsigned short)(((1<<7) | ((dpl & 0x03) << 5) | (type & 0x0F)) << 8),
		  /* offset 16..31*/
		  ((offset >> 16) & 0x0000FFFF)
	  };
}

/**
 * @brief Esta rutina se encarga de cargar la IDT.
 * */
void setup_idt(void);

#endif /* IDT_H_ */
