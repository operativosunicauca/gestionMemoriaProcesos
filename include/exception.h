/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License.
 * @brief En este archivo se definen los tipos y las funciones relacionados
 * con la gestion de excepciones en la arquitectura IA-32
 *
 */

#ifndef EXCEPTION_H_
#define EXCEPTION_H_

/** Importar la definicion del tipo manejador de interrupcion*/
#include <idt.h>


/** En la arquitectura IA-32 se definen 32 excepciones. */
#define MAX_EXCEPTIONS 32

/** Alias para las rutinas de manejo de excepcion*/
typedef interrupt_handler exception_handler;

/**
 * @brief Esta rutina crea un manejador de interrupcion para las
 * 32 excepciones x86 e inicializa la tabla de manejadores de excepcion.
 */
void setup_exceptions(void);

#endif /* EXCEPTION_H_ */
