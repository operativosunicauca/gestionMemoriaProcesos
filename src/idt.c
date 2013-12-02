/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 * @brief Este archivo implementa las primitivas para
 * el manejo de interrupciones en la arquitectura IA-32
 */

#include <idt.h>
#include <stdlib.h>
#include <stdio.h>
#include <asm.h>
#include <pm.h>

/** @brief Tabla de descriptores de interrupción (IDT) */
idt_descriptor idt[MAX_IDT_ENTRIES] __attribute__((aligned(8)));

/** @brief El apuntador al IDT que se utiliza en la instruccion lidt */
idt_ptr idt_pointer;

/**
 * @brief Arreglo que almacena las referencias a las rutinas de manejo de
 * interrupción.
 * La rutina install_interrupt_handler almacena las referencias a las rutinas
 * en este arreglo. */
interrupt_handler interrupt_handlers[MAX_IDT_ENTRIES];

/**
 * @brief Esta rutina permite determinar si dos selectores
 * se encuentran en el mismo nivel de privilegios.
 * @param sel1	Selector 1
 * @param sel2	Selector 2
 * @return int 1 = los selectores tienen igual DPL, 0 = los selectores tienen
 *  diferente DPL.
 */
static __inline__ int same_dpl(unsigned short sel1, unsigned short sel2) {
	/* Comparar los bits 0 y 1 de los selectores. Si son iguales,
	 * los selectores tienen el mismo DPL.
	 * Recuerde que el formato de un selector es:
	 * bits 0 - 1: DPL
	 * bit 2: Indicador de tabla (0 = gdt, 1 = ldt)
	 * bits 3-15 : indice del selector.
	 * Asi, al realizar la operacion 'and' con el valor 0x03, se enmascaran
	 * solo los bits correspondientes al DPL. Luego los dos valores se
	 * comparan para determinar si son iguales. */
	return ((sel1 & 0x03) == (sel2 & 0x03));
}

/**
 * @brief Esta rutina permite mostrar el estado en el cual
 * se encuentra el procesador cuando ocurrio una interrupción.
 * @param state Apuntador al marco de pila de interrupción.
 */
static __inline__ dump_interrupt_state(interrupt_state * state) {
	unsigned int current_cs;
	printf("Interrupt state: \n");
	printf("======================================\n");
	printf("gs: %x fs: %x es: %x ds: %x\n", state->gs, state->fs, state->es,
			state->ds);
	printf("edi: %x esi: %x ebp: %x esp: %x ebx: %x edx: %x ecx: %x eax: %x\n",
			state->edi, state->esi, state->ebp, state->esp, state->ebx,
			state->edx, state->ecx, state->eax);
	printf("Number=%d Error code: %d\n", state->number, state->error_code);
	printf("old eip: %u old cs: %x\n", state->old_eip, state->old_cs);

	inline_assembly("push %%cs; pop %%eax" :"=a"(current_cs));
	printf("Current cs: %u\n", current_cs);

	/* Verificar si ocurrio un cambio de nivel de privilegios */
	if (current_cs != state->old_cs && !same_dpl(current_cs, state->old_cs)) {
		printf("Old DPL: %d Old ss: %x Old esp: %u\n", state->old_cs & 0x03,
				state->old_ss, state->old_esp);
	}

	printf("EFLAGS: %b\n", state->old_eflags);
	printf("======================================\n");
}

/** @brief Referencia al selector de segmento de codigo del kernel */
extern unsigned short kernel_code_selector;

/**
 * @brief Esta rutina se encarga de cargar la IDT.
 */
void setup_idt(void) {

	int i;

	/* Configurar la base y el limite de la estructura idt_pointer
	 * que se utilizara en la instruccion lidt. */
	idt_pointer.limit = (sizeof(idt_descriptor) * MAX_IDT_ENTRIES);
	idt_pointer.base = (unsigned int) &idt;

	for (i = 0; i < MAX_IDT_ENTRIES; i++) {
		idt[i] = idt_descriptor_32(kernel_code_selector, isr_table[i],
				RING0_DPL, INTERRUPT_GATE_TYPE);
	}

	/* Y finalmente cargar la IDT con inline assembly */
	inline_assembly("lidt (%0)" : :"a"(&idt_pointer));
}

/**
 * @brief Instala un nuevo manejador de interrupción para un número de
 * interrupción determinado.
 * @param index Número de interrupción para la cual se desea instalar el
 * manejador
 * @param handler Función para el manejo de la interrupción.
 */
void install_interrupt_handler(unsigned char index, interrupt_handler handler) {
	if (interrupt_handlers[index] != NULL_INTERRUPT_HANDLER) {
		printf("Error! handler for routine %d was already set!\n", index);
		return;
	}
	interrupt_handlers[index] = handler;
}

/**
 * @brief Desinstala un manejador de interrupción
 * @param index Número de la interrupción para la cual se va a desinstalar
 * el manejador
 */
void uninstall_interrupt_handler(unsigned char index) {
	/* Simplemente quitar la referencia a la rutina de manejo de interrupción.*/
	if (index < MAX_IDT_ENTRIES) {
		interrupt_handlers[index] = NULL_INTERRUPT_HANDLER;
	}
}

/**
 * @brief Esta rutina recibe el control de la Rutina de Servicio
 * de Interrupción (ISR) isr0, isr1.. etc. correspondiente.
 * Su trabajo consiste en determinar el vector de interrupción a partir del
 * estado que recibe como parametro, y de invocar la rutina de manejo de
 * interrupción adecuada, si existe.
 */
void interrupt_dispatcher() {

	/* Esta variable definida en isr.S contiene el valor del apuntador
	 * actual al tope de la pila (esp)*/
	extern unsigned int current_esp;

	interrupt_state * state;

	state = (interrupt_state *)current_esp;

	interrupt_handler handler;

	/* Buscar la rutina que maneja la interrupcion */
	handler = interrupt_handlers[state->number];

	/* Si la rutina existe, ejecutarla y pasarle como parametro los registros.*/
	if (handler != NULL_INTERRUPT_HANDLER) {
		handler(state);
	} else {
		/* En caso contrario, informar que ocurrio una interrupcion
		 * que no tiene un manejador asociado.*/
		if (state->number < 32) { /* Excepcion*/
			printf("x86 Exception [%d]. System Halted!\n", state->number);
		} else { /* Interrupcion */
			printf("Unhandled interrupt [%d]. System halted.", state->number);
		}
		/* Mostrar el estado de la interrupcion. */
		dump_interrupt_state(state);

		/* Bloquear el kernel cuando ocurre una excepcion o una
		 * interrupcion que no tiene manejador asociado.
		 * Recuerde que esta rutina se esta ejecutando con las interrupciones
		 * deshabilitadas, por lo cual no existe forma de romper el
		 * ciclo infinito que se define a continuacion.*/
		for (;;)
			;
	}
}
