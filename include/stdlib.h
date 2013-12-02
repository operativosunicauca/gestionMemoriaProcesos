/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 *
 * @brief Contiene las definiciones de algunas funciones de utilidad.
 * @anchor kernel_stdlib
 */

#ifndef STDLIB_H_
#define STDLIB_H_

/** @brief Macro para verificar el valor de un bit especifico de una variable */
#define test_bit(x, n) (x & (1 << n))

/** @brief Macro para establecer en 1 el valor de un bit de la variable */
#define set_bit(x, n) x |= (1 << n)

/** @brief Macro para establecer en 0 el valor de un bit de la variable */
#define clear_bit(x, n)  x &= ~(1<<n)

/**
 * @brief Convierte un numero en base 2, 10 0 16 a un string terminado
 * en nulo. Si la base es 10, toma el numero con signo.
 *  @param n N�mero a transformar en cadena
 *  @param buf Buffer que contiene el n�mero transformado a cadena de caracteres
 *  @param base Base a la cual se desea transformar el n�mero (2, 10 o 16).
 *  @return  Apuntador al buffer en el cual se encuentra el n�mero transformado
 */
char * itoa(unsigned int n, char *buf, int base);

/**
 * @brief Convierte un numero sin signo en base 2, 10 0 16 a un string terminado
 * en nulo. Si la base es 10, toma el numero con signo.
 *  @param n N�mero a transformar en cadena
 *  @param buf Buffer que contiene el n�mero transformado a cadena de caracteres
 *  @param base Base a la cual se desea transformar el n�mero (2, 10 o 16).
 *  @return  Apuntador al buffer en el cual se encuentra el n�mero transformado
 */
char * itoa(unsigned int n, char *buf, int base);

/**
 * @brief Convierte un string a un entero, en la base especificada
 *  @param buf Buffer que contiene el numero
 * 	@param base Base en la cual se quiere transformar el numero
 *  @return N�mero en la base especificada.
 */
int atoi(char * buf, int base);


#endif /* STDLIB_H_ */
