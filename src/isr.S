/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 *
 * @brief Contiene la definicion y la implementacion de las
 * rutinas de servicio de interrupcion para las 255 interrupciones que se pueden
 * generar en un procesador IA-32.
 * Todas las rutinas establecen un marco de pila uniforme, y luego invocan
 * a la rutina interrupt_dispatcher().
 */

 /** @verbatim */

.intel_syntax noprefix /* Usar sintaxis Intel, sin prefijo para los registros */
.section .text		/* Segmento de texto */
.code32				/* 32 bits - Modo protegido */

#define ASM 1 /* Solo incluir las constantes del archivo pm.h */
#include <pm.h>

/*
* Macro: isr_no_error_code
* Descripcion: Este macro permite crear las  rutinas de servicio de
*			   interrupcion para interrupciones que
*			   Intel x86 que no generan codigo de error.
*			   Estas son:
*			   Las excepciones Intel con vectores 0-7, 9 y 16-32.
*			   Las interrupciones con vectores 33 en adelante.
*              Con el fin de mantener un marco de pila constante con las rutinas
*              de servicio de interrupcion que si generan codigo de error, se
* 			   inserta un '0' como codigo de error.
*/
.macro isr_no_error_code id
 .global isr\id /* Para que esta rutina se accesible desde C*/
 isr\id:
    /* Deshabilitar las interrupciones*/
 	cli
 	/* Ahora se crea un marco de pila estandar para invocar la rutina general
 	interrupt_dispatcher. */
 	/* Codigo de error, siempre 0 para este tipo de interrupciones */
	push 0
	/* # de excepcion generada */
	push \id
	/* Almacenar en la pila los registros de proposito general en el siguiente
	orden: eax, ecx, edx, ebx, esp original, ebp, esi, y edi */
	pusha
	/* Almacenar en la pila los registros de segmento de datos */
	push ds
	push es
	push fs
	push gs

	/* Este marco de pila se crea en el contexto de ejecucion actual. */

	/*
	La pila luce asi:
	+--------------------------+
	| old ss                   | Estos valores son almacenados automaticamente
	|--------------------------| en la pila cuando ocurre una interrupcion
	| old esp                  | ..
	|--------------------------| ..
	| eflags                   | ..
	|--------------------------| ..
	| old cs                   | ..
	|--------------------------| ..
	| old eip                  | ..
	|--------------------------| -------------------------------------------
	| 0 (codigo de error)      | push 0 (siempre codigo de error = 0)
	|--------------------------|
	| # de excepcion generada  | push \id
	|--------------------------|
	| eax                      | pusha
	|--------------------------|
	| ecx                      |(recuerde que pusha almacena en la pila los
	|--------------------------|registros en el siguiente orden:
	| edx                      |eax, ecx, edx, ebx, esp original, ebp, esi,
	|--------------------------|edi)
	| ebx                      |
	|--------------------------|
	| esp antes de pusha       |
	|--------------------------|
	| ebp                      |
	|--------------------------|
	| esi                      |
	|--------------------------|
	| edi                      |
	|--------------------------|------------------------------------------
	| ds                       | ahora los registros de segmento de datos
	|--------------------------|
	| es                       |
	|--------------------------|
	| fs                       |
	|--------------------------|
	| gs                       |
	|--------------------------|<--esp (current_esp)
	*/

	/* Configurar los registros de segmento de datos para que contengan
	el selector de datos para el kernel definido en la GDT */
	movw ax, KERNEL_DATA_SELECTOR
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	/* Almacenar la posicion actual del apuntador de la pila ss:esp */
	mov [current_ss], ss
	mov [current_esp], esp

	/* Apuntar al tope de la pila temporal del kernel */
	mov ss, ax
	mov esp, OFFSET interrupt_stack_top

	/* interrupt_dispatcher recibe como parametro una estructura de tipo regs,
	la cual se almaceno con los 'push' anteriores  */

	call interrupt_dispatcher

	/* Retornar de la interrupcion, recuperando el estado del procesador
	a partir del marco de interrupcion almacenado. */
	call return_from_interrupt
.endm

/*
* Macro: isr_no_error_code
* Descripcion: Este macro permite crear las  rutinas de servicio de
*			   interrupcion (isr) para las interrupciones que si generan codigo
*			   de error. Estas son las excepciones Intel con vectores 8 y 10-14
*/
.macro isr_error_code id
 .global isr\id /* Para que esta rutina se accesible desde C */
 isr\id:
    /* Deshabilitar las interrupciones*/
 	cli
 	/* Ahora se crea un marco de pila estandar para invocar la rutina general
 	. */
 	/* El codigo de error es almacenado  por el procesador en la pila
 	de forma automatica cuando ocurre la excepcion
 	*/
	/* # de excepcion generada */
	push \id
	/* Almacenar en la pila los registros de proposito general en el siguiente
	orden: eax, ecx, edx, ebx, esp original, ebp, esi, y edi */
	pusha
	/* Almacenar en la pila los registros de segmento de datos */
	push ds
	push es
	push fs
	push gs

	/* Este marco de pila se crea en el contexto de ejecucion actual. */

	/*
	La pila luce asi:
	+--------------------------+
	| old ss                   | Estos valores son almacenados automaticamente
	|--------------------------| en la pila cuando ocurre una excepcion
	| old esp                  | ..
	|--------------------------| ..
	| eflags                   | ..
	|--------------------------| ..
	| old cs                   | ..
	|--------------------------| ..
	| old eip                  | ..
	|--------------------------|
	|   (codigo de error)      | El codigo de error se almacena automaticamente
	|--------------------------|---------------------------------------------
	| # de excepcion generada  | push \id
	|--------------------------|
	| eax                      | pusha
	|--------------------------|
	| ecx                      |(recuerde que pusha almacena en la pila los
	|--------------------------|registros en el siguiente orden:
	| edx                      |eax, ecx, edx, ebx, esp original, ebp, esi,
	|--------------------------|edi)
	| ebx                      |
	|--------------------------|
	| esp antes de pusha       |
	|--------------------------|
	| ebp                      |
	|--------------------------|
	| esi                      |
	|--------------------------|
	| edi                      |
	|--------------------------|------------------------------------------
	| ds                       | ahora los registros de segmento de datos
	|--------------------------|
	| es                       |
	|--------------------------|
	| fs                       |
	|--------------------------|
	| gs                       |
	|--------------------------|<--esp
	*/

	/* Configurar los registros de segmento de datos para que contengan
	el selector de datos para el kernel definido en la GDT */
	movw ax, KERNEL_DATA_SELECTOR
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	/* Almacenar la posicion actual del apuntador de la pila ss:esp */
	mov [current_ss], ss
	mov [current_esp], esp

	/* Apuntar al tope de la pila temporal del kernel */
	mov ss, ax
	mov esp, OFFSET interrupt_stack_top

	/* interrupt_dispatcher recibe como parametro una estructura de tipo regs,
	la cual se almaceno con los 'push' anteriores  */

	call interrupt_dispatcher

	/* Retornar de la interrupcion, recuperando el estado del procesador
	a partir del marco de interrupcion almacenado. */
	call return_from_interrupt
.endm

/*
Rutina: return_from_interrupt
Descripcion: A partir de un marco de interrupcion, continua con la ejecucion
de una tarea.
*/
.global return_from_interrupt
return_from_interrupt:
	/* Recuperar el apuntador de la pila ss:esp almacenado luego de crear
	el marco de pila para la interrupcion */
	mov ss, [current_ss]
	mov esp, [current_esp]

	/* Ahora sacar los parametros enviados a la pila en orden inverso*/
	pop gs
	pop fs
	pop es
	pop ds
	/* los registros de proposito general */
	popa
	/* Codigo de error e interrupcion generada */
	add esp, 8

	/*
	Ahora la pila luce asi:
	+--------------------------+
	| old ss                   | Si ocurrió un cambio de contexto de pila,
	|--------------------------| se almacena la posición de la pila anterior
	| old esp                  | (SS:ESP).
	|--------------------------|
	| eflags                   | Estado del procesador (EFLAGS)
	|--------------------------|
	| old cs                   | Dirección lineal CS:EIP a la cual se debe
	|--------------------------| retornar (punto en el cual se interrumpió
	| old eip                  | el procesador)
	+--------------------------+ <-- ESP (tope de la pila)
	*/

	/* Retornar de la interrupcion */
	iret
	/* Esta rutina 'no retorna', ya que continua la ejecucion en el contexto
	que fue interrumpido. */


/* Definir las rutinas de servicio de interrupcion. Se debe tener en cuenta que
* las rutinas con vectores 0-7, 9, y 16 en adelante no generan codigo
de error, mientras que las rutinas 8, y 10-14 si generan codigo de error. */

/* Es importante recordar que las interrupciones con vector 0-31 (las primeras
 32 entradas en la IDT) corresponden a excepciones especificas de la
 arquitectura Intel. Consulte el manual de Intel Volume 3 Systems Programming
 Guide para mas detalles. */

 /* Implementacion de las 256 rutinas de servicio de interrupcion (ISR)*/

isr_no_error_code 0 /* 0: Divide By Zero*/
isr_no_error_code 1 /* 1: Debug Exception */
isr_no_error_code 2 /* 2: Non Maskable Interrupt */
isr_no_error_code 3 /* 3: Int 3 Exception */
isr_no_error_code 4 /* 4: INTO Exception */
isr_no_error_code 5 /* 5: Out of Bounds Exception */
isr_no_error_code 6 /* 6: Invalid Opcode Exception */
isr_no_error_code 7 /* 7: Coprocessor Not Available */
isr_error_code 8	/* 8: Double Fault Exception */
isr_no_error_code 9 /* 9: Coprocessor Segment Overrun Exception */
isr_error_code 10	/* 10: Bad TSS Exception */
isr_error_code 11	/* 11: Segment Not Present Exception*/
isr_error_code 12	/* 12: Stack Fault Exception*/
isr_error_code 13	/* 13: General Protection Fault Exception*/
isr_error_code 14	/* 14: Page Fault Exception*/
isr_no_error_code 15 /* 15: Reserved Exception*/
isr_no_error_code 16 /* 16: Floating Point Exception*/
isr_no_error_code 17 /* 17: Alignment Check Exception*/
isr_no_error_code 18 /* 18: Machine Check Exception*/
isr_no_error_code 19 /* 19: Reserved Exception*/
isr_no_error_code 20 /* 20: Reserved Exception*/
isr_no_error_code 21 /* 21: Reserved Exception*/
isr_no_error_code 22 /* 22: Reserved Exception*/
isr_no_error_code 23 /* 23: Reserved Exception*/
isr_no_error_code 24 /* 24: Reserved Exception*/
isr_no_error_code 25 /* 25: Reserved Exception*/
isr_no_error_code 26 /* 26: Reserved Exception*/
isr_no_error_code 27 /* 27: Reserved Exception*/
isr_no_error_code 28 /* 28: Reserved Exception*/
isr_no_error_code 29 /* 29: Reserved Exception*/
isr_no_error_code 30 /* 30: Reserved Exception*/
isr_no_error_code 31 /* 31: Reserved Exception*/

/* Las interrupciones con vector 32 en adelante no general codigo de error */

isr_no_error_code 32
isr_no_error_code 33
isr_no_error_code 34
isr_no_error_code 35
isr_no_error_code 36
isr_no_error_code 37
isr_no_error_code 38
isr_no_error_code 39
isr_no_error_code 40
isr_no_error_code 41
isr_no_error_code 42
isr_no_error_code 43
isr_no_error_code 44
isr_no_error_code 45
isr_no_error_code 46
isr_no_error_code 47
isr_no_error_code 48
isr_no_error_code 49
isr_no_error_code 50
isr_no_error_code 51
isr_no_error_code 52
isr_no_error_code 53
isr_no_error_code 54
isr_no_error_code 55
isr_no_error_code 56
isr_no_error_code 57
isr_no_error_code 58
isr_no_error_code 59
isr_no_error_code 60
isr_no_error_code 61
isr_no_error_code 62
isr_no_error_code 63
isr_no_error_code 64
isr_no_error_code 65
isr_no_error_code 66
isr_no_error_code 67
isr_no_error_code 68
isr_no_error_code 69
isr_no_error_code 70
isr_no_error_code 71
isr_no_error_code 72
isr_no_error_code 73
isr_no_error_code 74
isr_no_error_code 75
isr_no_error_code 76
isr_no_error_code 77
isr_no_error_code 78
isr_no_error_code 79
isr_no_error_code 80
isr_no_error_code 81
isr_no_error_code 82
isr_no_error_code 83
isr_no_error_code 84
isr_no_error_code 85
isr_no_error_code 86
isr_no_error_code 87
isr_no_error_code 88
isr_no_error_code 89
isr_no_error_code 90
isr_no_error_code 91
isr_no_error_code 92
isr_no_error_code 93
isr_no_error_code 94
isr_no_error_code 95
isr_no_error_code 96
isr_no_error_code 97
isr_no_error_code 98
isr_no_error_code 99
isr_no_error_code 100
isr_no_error_code 101
isr_no_error_code 102
isr_no_error_code 103
isr_no_error_code 104
isr_no_error_code 105
isr_no_error_code 106
isr_no_error_code 107
isr_no_error_code 108
isr_no_error_code 109
isr_no_error_code 110
isr_no_error_code 111
isr_no_error_code 112
isr_no_error_code 113
isr_no_error_code 114
isr_no_error_code 115
isr_no_error_code 116
isr_no_error_code 117
isr_no_error_code 118
isr_no_error_code 119
isr_no_error_code 120
isr_no_error_code 121
isr_no_error_code 122
isr_no_error_code 123
isr_no_error_code 124
isr_no_error_code 125
isr_no_error_code 126
isr_no_error_code 127
isr_no_error_code 128
isr_no_error_code 129
isr_no_error_code 130
isr_no_error_code 131
isr_no_error_code 132
isr_no_error_code 133
isr_no_error_code 134
isr_no_error_code 135
isr_no_error_code 136
isr_no_error_code 137
isr_no_error_code 138
isr_no_error_code 139
isr_no_error_code 140
isr_no_error_code 141
isr_no_error_code 142
isr_no_error_code 143
isr_no_error_code 144
isr_no_error_code 145
isr_no_error_code 146
isr_no_error_code 147
isr_no_error_code 148
isr_no_error_code 149
isr_no_error_code 150
isr_no_error_code 151
isr_no_error_code 152
isr_no_error_code 153
isr_no_error_code 154
isr_no_error_code 155
isr_no_error_code 156
isr_no_error_code 157
isr_no_error_code 158
isr_no_error_code 159
isr_no_error_code 160
isr_no_error_code 161
isr_no_error_code 162
isr_no_error_code 163
isr_no_error_code 164
isr_no_error_code 165
isr_no_error_code 166
isr_no_error_code 167
isr_no_error_code 168
isr_no_error_code 169
isr_no_error_code 170
isr_no_error_code 171
isr_no_error_code 172
isr_no_error_code 173
isr_no_error_code 174
isr_no_error_code 175
isr_no_error_code 176
isr_no_error_code 177
isr_no_error_code 178
isr_no_error_code 179
isr_no_error_code 180
isr_no_error_code 181
isr_no_error_code 182
isr_no_error_code 183
isr_no_error_code 184
isr_no_error_code 185
isr_no_error_code 186
isr_no_error_code 187
isr_no_error_code 188
isr_no_error_code 189
isr_no_error_code 190
isr_no_error_code 191
isr_no_error_code 192
isr_no_error_code 193
isr_no_error_code 194
isr_no_error_code 195
isr_no_error_code 196
isr_no_error_code 197
isr_no_error_code 198
isr_no_error_code 199
isr_no_error_code 200
isr_no_error_code 201
isr_no_error_code 202
isr_no_error_code 203
isr_no_error_code 204
isr_no_error_code 205
isr_no_error_code 206
isr_no_error_code 207
isr_no_error_code 208
isr_no_error_code 209
isr_no_error_code 210
isr_no_error_code 211
isr_no_error_code 212
isr_no_error_code 213
isr_no_error_code 214
isr_no_error_code 215
isr_no_error_code 216
isr_no_error_code 217
isr_no_error_code 218
isr_no_error_code 219
isr_no_error_code 220
isr_no_error_code 221
isr_no_error_code 222
isr_no_error_code 223
isr_no_error_code 224
isr_no_error_code 225
isr_no_error_code 226
isr_no_error_code 227
isr_no_error_code 228
isr_no_error_code 229
isr_no_error_code 230
isr_no_error_code 231
isr_no_error_code 232
isr_no_error_code 233
isr_no_error_code 234
isr_no_error_code 235
isr_no_error_code 236
isr_no_error_code 237
isr_no_error_code 238
isr_no_error_code 239
isr_no_error_code 240
isr_no_error_code 241
isr_no_error_code 242
isr_no_error_code 243
isr_no_error_code 244
isr_no_error_code 245
isr_no_error_code 246
isr_no_error_code 247
isr_no_error_code 248
isr_no_error_code 249
isr_no_error_code 250
isr_no_error_code 251
isr_no_error_code 252
isr_no_error_code 253
isr_no_error_code 254
isr_no_error_code 255

.globl isr_table
isr_table:
.long isr0
.long isr1
.long isr2
.long isr3
.long isr4
.long isr5
.long isr6
.long isr7
.long isr8
.long isr9
.long isr10
.long isr11
.long isr12
.long isr13
.long isr14
.long isr15
.long isr16
.long isr17
.long isr18
.long isr19
.long isr20
.long isr21
.long isr22
.long isr23
.long isr24
.long isr25
.long isr26
.long isr27
.long isr28
.long isr29
.long isr30
.long isr31
.long isr32
.long isr33
.long isr34
.long isr35
.long isr36
.long isr37
.long isr38
.long isr39
.long isr40
.long isr41
.long isr42
.long isr43
.long isr44
.long isr45
.long isr46
.long isr47
.long isr48
.long isr49
.long isr50
.long isr51
.long isr52
.long isr53
.long isr54
.long isr55
.long isr56
.long isr57
.long isr58
.long isr59
.long isr60
.long isr61
.long isr62
.long isr63
.long isr64
.long isr65
.long isr66
.long isr67
.long isr68
.long isr69
.long isr70
.long isr71
.long isr72
.long isr73
.long isr74
.long isr75
.long isr76
.long isr77
.long isr78
.long isr79
.long isr80
.long isr81
.long isr82
.long isr83
.long isr84
.long isr85
.long isr86
.long isr87
.long isr88
.long isr89
.long isr90
.long isr91
.long isr92
.long isr93
.long isr94
.long isr95
.long isr96
.long isr97
.long isr98
.long isr99
.long isr100
.long isr101
.long isr102
.long isr103
.long isr104
.long isr105
.long isr106
.long isr107
.long isr108
.long isr109
.long isr110
.long isr111
.long isr112
.long isr113
.long isr114
.long isr115
.long isr116
.long isr117
.long isr118
.long isr119
.long isr120
.long isr121
.long isr122
.long isr123
.long isr124
.long isr125
.long isr126
.long isr127
.long isr128
.long isr129
.long isr130
.long isr131
.long isr132
.long isr133
.long isr134
.long isr135
.long isr136
.long isr137
.long isr138
.long isr139
.long isr140
.long isr141
.long isr142
.long isr143
.long isr144
.long isr145
.long isr146
.long isr147
.long isr148
.long isr149
.long isr150
.long isr151
.long isr152
.long isr153
.long isr154
.long isr155
.long isr156
.long isr157
.long isr158
.long isr159
.long isr160
.long isr161
.long isr162
.long isr163
.long isr164
.long isr165
.long isr166
.long isr167
.long isr168
.long isr169
.long isr170
.long isr171
.long isr172
.long isr173
.long isr174
.long isr175
.long isr176
.long isr177
.long isr178
.long isr179
.long isr180
.long isr181
.long isr182
.long isr183
.long isr184
.long isr185
.long isr186
.long isr187
.long isr188
.long isr189
.long isr190
.long isr191
.long isr192
.long isr193
.long isr194
.long isr195
.long isr196
.long isr197
.long isr198
.long isr199
.long isr200
.long isr201
.long isr202
.long isr203
.long isr204
.long isr205
.long isr206
.long isr207
.long isr208
.long isr209
.long isr210
.long isr211
.long isr212
.long isr213
.long isr214
.long isr215
.long isr216
.long isr217
.long isr218
.long isr219
.long isr220
.long isr221
.long isr222
.long isr223
.long isr224
.long isr225
.long isr226
.long isr227
.long isr228
.long isr229
.long isr230
.long isr231
.long isr232
.long isr233
.long isr234
.long isr235
.long isr236
.long isr237
.long isr238
.long isr239
.long isr240
.long isr241
.long isr242
.long isr243
.long isr244
.long isr245
.long isr246
.long isr247
.long isr248
.long isr249
.long isr250
.long isr251
.long isr252
.long isr253
.long isr254
.long isr255


.section .data
/* Las variables definidas dentro de esta seccion se convierten en
variables globales del kernel. */

/* Esta variable almacena el valor del selector ss en el momento en que
se va a invocar la rutina de manejo de interrupcion interrupt_handler */
.global current_ss
current_ss:
	.word 0x0000

/* Esta variable almacena el valor del registro esp en el momento en que se
va a invocar la rutina de manejo de interrupcion interrupt_handler */
.global current_esp
current_esp:
	.long 0x00000000

/* Buffer temporal que el kernel usa como pila para invocar los manejadores
de interrupcion */

interrupt_stack_base:
/* 1024 bytes, espacio suficiente para almacenar 256 valores en la pila
 temporal*/
.space 1024

interrupt_stack_top: /* Tope de la pila temporal del kernel */
.long 0x00000000

/**
@endverbatim
*/
