/**
 * @file
 * @ingroup kernel_code 
 * @author Erwin Meza <emezav@gmail.com>
 * @copyright GNU Public License. 
 *
 * @brief Contiene las primitivas basicas para entrada / salida
 */

#ifndef STDIO_H_
#define STDIO_H_

#include <asm.h> /* Para operaciones sobre puertos de E/S */

/*@brief Constante que define la direcci�n lineal del inicio de la memoria de
 * video*/
#define VIDEO_ADDR 0XB8000

/** @brief Apuntador al inicio de la memoria de video.
 * @details
 * La memoria de video se encuentra mapeada en la direcci�n lineal 0xB8000.
 * Cada caracter en pantalla ocupa dos caracteres (bytes) en la memoria de
 * video:
 * - El byte menos significativo contiene el caracter ASCII a mostrar
 * - El byte m�s significativo contiene los atributos de texto y fondo
 *   del caracter a mostrar. A su vez este byte se subdivide en:
 *   @verbatim
 *    7  6  5  4  3  2  1  0
 *   +-----------------------+
 *   |I |F |F |F |I |B |B |B |
 *   +-----------------------+
 *   @endverbatim
 *   Los bits F correspondel al color del texto (Foreground).
 *   Los bits B corresponden al color de fondo (Background).
 *   El bit I corresponde a la intensidad del color de fondo (0 = oscuro,
 *   1 = claro) o del color del texto.
 */
extern unsigned short * videoptr;

/** @brief Byte que almacena los atributos de texto */
extern char text_attributes;

/** @brief N�mero de l�neas de la pantalla */
#define SCREEN_LINES 25

/** @brief N�mero de columnas de la pantalla */
#define SCREEN_COLUMNS 80

/** @brief Espacios en un tabulador */
#define TABSIZE 8

/** @brief Variable que controla el n�mero de lineas de la pantalla */
extern int screen_lines;

/** @brief Variable que controla el n�mero de columnas de la pantalla */
extern int screen_columns;

/** @brief Macro para calcular el byte de atributos de texto y fondo */
#define COLOR(fg, bg) ((bg << 4) | fg)


/** @brief Negro */
#define BLACK 0
/** @brief Azul */
#define BLUE 1
/** @brief Verde */
#define GREEN 2
/** @brief Cyan */
#define CYAN 3
/** @brief Rojo */
#define RED 4
/** @brief Magenta */
#define MAGENTA 5
/** @brief Caf�*/
#define BROWN 6
/** @brief Gris claro */
#define LIGHTGRAY 7
/** @brief Gris oscuro */
#define DARKGRAY 8
/** @brief Azul claro*/
#define LIGHTBLUE 9
/** @brief Verde claro  */
#define LIGHTGREEN 10
/** @brief Cyan claro */
#define LIGHTCYAN 11
/** @brief Rojo claro */
#define LIGHTRED 12
/** @brief Magenta claro */
#define LIGHTMAGENTA 13
/** @brief Caf� claro */
#define LIGHTBROWN 14
/** @brief Blanco */
#define WHITE 15

/** @brief Caracter ASCII de espacio */
#define SPACE 0x20
/** @brief Caracter ASCII de backspace */
#define BACKSPACE 0x08
/** @brief Caracter ASCII de Fin de l�nea*/
#define LF '\n'
/** @brief Caracter ASCII de Retorno de carro*/
#define CR '\r'
/** @brief Caracter ASCII de Tabulador */
#define TAB 0x09

/**
 * @brief Funci�n para imprimir un caracter
 *
 * Esta rutina imprime directamente en la memoria de video. No valida
 * caracteres especiales.
 * @param c caracter ascii a imprimir
 */
void putchar(char c);

/**
 * @brief Funci�n para imprimir una cadena de caracteres.
 *
 * Esta rutina no valida caracteres especiales.
 * @param s Cadena terminada en nulo que se desea imprimir
 */
void puts(char * s );


/**
 * @brief  Esa funcion implementa en forma basica el comportamiento de
 * 'printf' en C.
 * @param format Formato de la cadena de salida
 * @param ...  Lista de referencias a memoria de las variables a imprimir
 *
*/
void printf(char * ,...);

#endif /* STDIO_H_ */
