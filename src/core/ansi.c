/*
 * "Software pw3270, desenvolvido com base nos códigos fontes do WC3270  e X3270
 * (Paul Mattes Paul.Mattes@usa.net), de emulação de terminal 3270 para acesso a
 * aplicativos mainframe. Registro no INPI sob o nome G3270. Registro no INPI sob o nome G3270.
 *
 * Copyright (C) <2008> <Banco do Brasil S.A.>
 *
 * Este programa é software livre. Você pode redistribuí-lo e/ou modificá-lo sob
 * os termos da GPL v.2 - Licença Pública Geral  GNU,  conforme  publicado  pela
 * Free Software Foundation.
 *
 * Este programa é distribuído na expectativa de  ser  útil,  mas  SEM  QUALQUER
 * GARANTIA; sem mesmo a garantia implícita de COMERCIALIZAÇÃO ou  de  ADEQUAÇÃO
 * A QUALQUER PROPÓSITO EM PARTICULAR. Consulte a Licença Pública Geral GNU para
 * obter mais detalhes.
 *
 * Você deve ter recebido uma cópia da Licença Pública Geral GNU junto com este
 * programa; se não, escreva para a Free Software Foundation, Inc., 51 Franklin
 * St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Este programa está nomeado como ansi.c e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 * licinio@bb.com.br		(Licínio Luis Branco)
 * kraucer@bb.com.br		(Kraucer Fernandes Mazuco)
 *
 */


/**
 * @brief ANSI terminal emulation.
 */

#pragma GCC diagnostic ignored "-Wsign-compare"

#ifdef _WIN32
	#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif // _WIN32

#include <internals.h>
#include <lib3270/toggle.h>

#if defined(X3270_ANSI) /*[*/

#if defined(X3270_DISPLAY) /*[*/
#include <X11/Shell.h>
#endif /*]*/

#if defined(X3270_DBCS) /*[*/
#include <lib3270/3270ds.h>
#endif /*]*/

#include "ansic.h"
#include "ctlrc.h"
#include "hostc.h"
#include "screenc.h"
#include "telnetc.h"
#include "trace_dsc.h"
//#include "utf8c.h"
#if defined(X3270_DBCS) /*[*/
#include "widec.h"
#endif /*]*/

#define MB_MAX	LIB3270_MB_MAX

#define	SC	1	/**< @brief save cursor position */
#define RC	2	/**< @brief restore cursor position */
#define NL	3	/**< @brief new line */
#define UP	4	/**< @brief cursor up */
#define	E2	5	/**< @brief second level of ESC processing */
#define rS	6	/**< @brief reset */
#define IC	7	/**< @brief insert chars */
#define DN	8	/**< @brief cursor down */
#define RT	9	/**< @brief cursor right */
#define LT	10	/**< @brief cursor left */
#define CM	11	/**< @brief cursor motion */
#define ED	12	/**< @brief erase in display */
#define EL	13	/**< @brief erase in line */
#define IL	14	/**< @brief insert lines */
#define DL	15	/**< @brief delete lines */
#define DC	16	/**< @brief delete characters */
#define	SG	17	/**< @brief set graphic rendition */
#define BL	18	/**< @brief ring bell */
#define NP	19	/**< @brief new page */
#define BS	20	/**< @brief backspace */
#define CR	21	/**< @brief carriage return */
#define LF	22	/**< @brief line feed */
#define HT	23	/**< @brief horizontal tab */
#define E1	24	/**< @brief first level of ESC processing */
#define Xx	25	/**< @brief undefined control character (nop) */
#define Pc	26	/**< @brief printing character */
#define Sc	27	/**< @brief semicolon (after ESC [) */
#define Dg	28	/**< @brief digit (after ESC [ or ESC [ ?) */
#define RI	29	/**< @brief reverse index */
#define DA	30	/**< @brief send device attributes */
#define SM	31	/**< @brief set mode */
#define RM	32	/**< @brief reset mode */
#define DO	33	/**< @brief return terminal ID (obsolete) */
#define SR	34	/**< @brief device status report */
#define CS	35	/**< @brief character set designate */
#define E3	36	/**< @brief third level of ESC processing */
#define DS	37	/**< @brief DEC private set */
#define DR	38	/**< @brief DEC private reset */
#define DV	39	/**< @brief DEC private save */
#define DT	40	/**< @brief DEC private restore */
#define SS	41	/**< @brief set scrolling region */
#define TM	42	/**< @brief text mode (ESC ]) */
#define T2	43	/**< @brief semicolon (after ESC ]) */
#define TX	44	/**< @brief text parameter (after ESC ] n ;) */
#define TB	45	/**< @brief text parameter done (ESC ] n ; xxx BEL) */
#define TS	46	/**< @brief tab set */
#define TC	47	/**< @brief tab clear */
#define C2	48	/**< @brief character set designate (finish) */
#define G0	49	/**< @brief select G0 character set */
#define G1	50	/**< @brief select G1 character set */
#define G2	51	/**< @brief select G2 character set */
#define G3	52	/**< @brief select G3 character set */
#define S2	53	/**< @brief select G2 for next character */
#define S3	54	/**< @brief select G3 for next character */
#define MB	55	/**< @brief process multi-byte character */

#define DATA	LIB3270_ANSI_STATE_DATA
#define ESC		LIB3270_ANSI_STATE_ESC
#define CSDES	LIB3270_ANSI_STATE_CSDES
#define N1		LIB3270_ANSI_STATE_N1
#define DECP	LIB3270_ANSI_STATE_DECP
#define TEXT2	LIB3270_ANSI_STATE_TEXT2
#define MBPEND	LIB3270_ANSI_STATE_MBPEND

static enum lib3270_ansi_state ansi_data_mode(H3270 *, int, int);
static enum lib3270_ansi_state dec_save_cursor(H3270 *, int, int);
static enum lib3270_ansi_state dec_restore_cursor(H3270 *, int, int);
static enum lib3270_ansi_state ansi_newline(H3270 *, int, int);
static enum lib3270_ansi_state ansi_cursor_up(H3270 *, int, int);
static enum lib3270_ansi_state ansi_esc2(H3270 *, int, int);
static enum lib3270_ansi_state ansi_reset(H3270 *, int, int);
static enum lib3270_ansi_state ansi_insert_chars(H3270 *, int, int);
static enum lib3270_ansi_state ansi_cursor_down(H3270 *, int, int);
static enum lib3270_ansi_state ansi_cursor_right(H3270 *, int, int);
static enum lib3270_ansi_state ansi_cursor_left(H3270 *, int, int);
static enum lib3270_ansi_state ansi_cursor_motion(H3270 *, int, int);
static enum lib3270_ansi_state ansi_erase_in_display(H3270 *, int, int);
static enum lib3270_ansi_state ansi_erase_in_line(H3270 *, int, int);
static enum lib3270_ansi_state ansi_insert_lines(H3270 *, int, int);
static enum lib3270_ansi_state ansi_delete_lines(H3270 *, int, int);
static enum lib3270_ansi_state ansi_delete_chars(H3270 *, int, int);
static enum lib3270_ansi_state ansi_sgr(H3270 *, int, int);
static enum lib3270_ansi_state ansi_bell(H3270 *, int, int);
static enum lib3270_ansi_state ansi_newpage(H3270 *, int, int);
static enum lib3270_ansi_state ansi_backspace(H3270 *, int, int);
static enum lib3270_ansi_state ansi_cr(H3270 *, int, int);
static enum lib3270_ansi_state ansi_lf(H3270 *, int, int);
static enum lib3270_ansi_state ansi_htab(H3270 *, int, int);
static enum lib3270_ansi_state ansi_escape(H3270 *, int, int);
static enum lib3270_ansi_state ansi_nop(H3270 *, int, int);
static enum lib3270_ansi_state ansi_printing(H3270 *, int, int);
static enum lib3270_ansi_state ansi_semicolon(H3270 *, int, int);
static enum lib3270_ansi_state ansi_digit(H3270 *, int, int);
static enum lib3270_ansi_state ansi_reverse_index(H3270 *, int, int);
static enum lib3270_ansi_state ansi_send_attributes(H3270 *, int, int);
static enum lib3270_ansi_state ansi_set_mode(H3270 *, int, int);
static enum lib3270_ansi_state ansi_reset_mode(H3270 *, int, int);
static enum lib3270_ansi_state dec_return_terminal_id(H3270 *, int, int);
static enum lib3270_ansi_state ansi_status_report(H3270 *, int, int);
static enum lib3270_ansi_state ansi_cs_designate(H3270 *, int, int);
static enum lib3270_ansi_state ansi_esc3(H3270 *, int, int);
static enum lib3270_ansi_state dec_set(H3270 *, int, int);
static enum lib3270_ansi_state dec_reset(H3270 *, int, int);
static enum lib3270_ansi_state dec_save(H3270 *, int, int);
static enum lib3270_ansi_state dec_restore(H3270 *, int, int);
static enum lib3270_ansi_state dec_scrolling_region(H3270 *, int, int);
static enum lib3270_ansi_state xterm_text_mode(H3270 *, int, int);
static enum lib3270_ansi_state xterm_text_semicolon(H3270 *, int, int);
static enum lib3270_ansi_state xterm_text(H3270 *, int, int);
static enum lib3270_ansi_state xterm_text_do(H3270 *, int, int);
static enum lib3270_ansi_state ansi_htab_set(H3270 *, int, int);
static enum lib3270_ansi_state ansi_htab_clear(H3270 *, int, int);
static enum lib3270_ansi_state ansi_cs_designate2(H3270 *, int, int);
static enum lib3270_ansi_state ansi_select_g0(H3270 *, int, int);
static enum lib3270_ansi_state ansi_select_g1(H3270 *, int, int);
static enum lib3270_ansi_state ansi_select_g2(H3270 *, int, int);
static enum lib3270_ansi_state ansi_select_g3(H3270 *, int, int);
static enum lib3270_ansi_state ansi_one_g2(H3270 *, int, int);
static enum lib3270_ansi_state ansi_one_g3(H3270 *, int, int);
static enum lib3270_ansi_state ansi_multibyte(H3270 *, int, int);

typedef enum lib3270_ansi_state (*afn_t)(H3270 *, int, int);

static const afn_t ansi_fn[] = {
/* 0 */		&ansi_data_mode,
/* 1 */		&dec_save_cursor,
/* 2 */		&dec_restore_cursor,
/* 3 */		&ansi_newline,
/* 4 */		&ansi_cursor_up,
/* 5 */		&ansi_esc2,
/* 6 */		&ansi_reset,
/* 7 */		&ansi_insert_chars,
/* 8 */		&ansi_cursor_down,
/* 9 */		&ansi_cursor_right,
/* 10 */	&ansi_cursor_left,
/* 11 */	&ansi_cursor_motion,
/* 12 */	&ansi_erase_in_display,
/* 13 */	&ansi_erase_in_line,
/* 14 */	&ansi_insert_lines,
/* 15 */	&ansi_delete_lines,
/* 16 */	&ansi_delete_chars,
/* 17 */	&ansi_sgr,
/* 18 */	&ansi_bell,
/* 19 */	&ansi_newpage,
/* 20 */	&ansi_backspace,
/* 21 */	&ansi_cr,
/* 22 */	&ansi_lf,
/* 23 */	&ansi_htab,
/* 24 */	&ansi_escape,
/* 25 */	&ansi_nop,
/* 26 */	&ansi_printing,
/* 27 */	&ansi_semicolon,
/* 28 */	&ansi_digit,
/* 29 */	&ansi_reverse_index,
/* 30 */	&ansi_send_attributes,
/* 31 */	&ansi_set_mode,
/* 32 */	&ansi_reset_mode,
/* 33 */	&dec_return_terminal_id,
/* 34 */	&ansi_status_report,
/* 35 */	&ansi_cs_designate,
/* 36 */	&ansi_esc3,
/* 37 */	&dec_set,
/* 38 */	&dec_reset,
/* 39 */	&dec_save,
/* 40 */	&dec_restore,
/* 41 */	&dec_scrolling_region,
/* 42 */	&xterm_text_mode,
/* 43 */	&xterm_text_semicolon,
/* 44 */	&xterm_text,
/* 45 */	&xterm_text_do,
/* 46 */	&ansi_htab_set,
/* 47 */	&ansi_htab_clear,
/* 48 */	&ansi_cs_designate2,
/* 49 */	&ansi_select_g0,
/* 50 */	&ansi_select_g1,
/* 51 */	&ansi_select_g2,
/* 52 */	&ansi_select_g3,
/* 53 */	&ansi_one_g2,
/* 54 */	&ansi_one_g3,
/* 55 */	&ansi_multibyte,
};

static const unsigned char st[8][256] = {
/*
 * State table for base processing (state == DATA)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,BL,BS,HT,LF,LF,NP,CR,G1,G0,
/* 10 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,E1,Xx,Xx,Xx,Xx,
/* 20 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 30 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 40 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 50 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 60 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* 70 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Xx,
/* 80 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,
/* 90 */       Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,Xx,
/* a0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* b0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* c0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* d0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* e0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,
/* f0 */       Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc,Pc
},

/*
 * State table for ESC processing (state == ESC)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0,CS,CS,CS,CS, 0, 0, 0, 0,
/* 30 */	0, 0, 0, 0, 0, 0, 0,SC,RC, 0, 0, 0, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0,NL, 0, 0,TS, 0, 0, 0, 0,RI,S2,S3,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,E2, 0,TM, 0, 0,
/* 60 */	0, 0, 0,rS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,G2,G3,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC ()*+ C processing (state == CSDES)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       C2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 40 */	0,C2,C2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC [ processing (state == N1)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0,Sc, 0, 0, 0,E3,
/* 40 */       IC,UP,DN,RT,LT, 0, 0, 0,CM, 0,ED,EL,IL,DL, 0, 0,
/* 50 */       DC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0,DA, 0, 0,CM,TC,SM, 0, 0, 0,RM,SG,SR, 0,
/* 70 */	0, 0,SS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC [ ? processing (state == DECP)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0, 0, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0, 0, 0, 0, 0, 0,DS, 0, 0, 0,DR, 0, 0, 0,
/* 70 */	0, 0,DT,DV, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC ] processing (state == TEXT)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 30 */       Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg,Dg, 0,T2, 0, 0, 0, 0,
/* 40 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 60 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
},

/*
 * State table for ESC ] n ; processing (state == TEXT2)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */        0, 0, 0, 0, 0, 0, 0,TB, 0, 0, 0, 0, 0, 0, 0, 0,
/* 10 */        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 20 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 30 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 40 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 50 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 60 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 70 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,Xx,
/* 80 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* 90 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* a0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* b0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* c0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* d0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* e0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,
/* f0 */       TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX,TX
},
/*
 * State table for multi-byte characters (state == MBPEND)
 */
{
	     /* 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  */
/* 00 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 10 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 20 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 30 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 40 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 50 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 60 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 70 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 80 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* 90 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* a0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* b0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* c0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* d0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* e0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,
/* f0 */       MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB,MB
},
};

#define CS_G0		LIB3270_ANSI_CS_G0
#define CS_G1		LIB3270_ANSI_CS_G1
#define CS_G2		LIB3270_ANSI_CS_G3
#define CS_G3		LIB3270_ANSI_CS_G3

#define CSD_LD		LIB3270_ANSI_CSD_LD
#define CSD_UK		LIB3270_ANSI_CSD_UK
#define CSD_US		LIB3270_ANSI_CSD_US

// static int      saved_cursor = 0;
#define NN	20
static int      n[NN], nx = 0;
#define NT	256
static char     text[NT + 1];
static int      tx = 0;
// static int      ansi_ch;
// static unsigned char gr = 0;
//static unsigned char saved_gr = 0;
// static unsigned char fg = 0;
// static unsigned char saved_fg = 0;
// static unsigned char bg = 0;
//static unsigned char saved_bg = 0;
// static int	cset = CS_G0;
// static int	saved_cset = CS_G0;
// static int	csd[4] = { CSD_US, CSD_US, CSD_US, CSD_US };
// static int	saved_csd[4] = { CSD_US, CSD_US, CSD_US, CSD_US };
// static int	once_cset = -1;
// static int  insert_mode = 0;
// static int  auto_newline_mode = 0;
// static int  appl_cursor = 0;
// static int  saved_appl_cursor = 0;
//static int  wraparound_mode = 1;
// static int  saved_wraparound_mode = 1;
// static int  rev_wraparound_mode = 0;
// static int  saved_rev_wraparound_mode = 0;
// static int	allow_wide_mode = 0;
// static int	saved_allow_wide_mode = 0;
// static int	wide_mode = 0;
// static int	saved_wide_mode = 0;
// static Boolean  saved_altbuffer = False;
// static int      scroll_top = -1;
// static int      scroll_bottom = -1;
// static unsigned char *tabs = (unsigned char *) NULL;

static const char	gnnames[] = "()*+";
static const char	csnames[] = "0AB";

//static int	cs_to_change;
#if defined(X3270_DBCS) /*[*/
static unsigned char mb_pending = 0;
static char	mb_buffer[LIB3270_MB_MAX];
static int	dbcs_process(H3270 *hSession, int ch, unsigned char ebc[]);
#endif /*]*/
// static int	pmi = 0;
// static char	pending_mbs[LIB3270_MB_MAX];

// static Boolean  held_wrap = False;

static void	ansi_scroll(H3270 *hSession);

static enum lib3270_ansi_state
ansi_data_mode(H3270 GNUC_UNUSED(*hSession), int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	return DATA;
}

static enum lib3270_ansi_state
dec_save_cursor(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int i;

	hSession->saved_cursor = hSession->cursor_addr;
	hSession->saved_cset = hSession->cset;
	for (i = 0; i < 4; i++)
		hSession->saved_csd[i] = hSession->csd[i];
	hSession->saved_fg = hSession->fg;
	hSession->saved_bg = hSession->bg;
	hSession->saved_gr = hSession->gr;
	return DATA;
}

static enum lib3270_ansi_state dec_restore_cursor(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int i;

	hSession->cset = hSession->saved_cset;
	for (i = 0; i < 4; i++)
		hSession->csd[i] = hSession->saved_csd[i];
	hSession->fg = hSession->saved_fg;
	hSession->bg = hSession->saved_bg;
	hSession->gr = hSession->saved_gr;
	cursor_move(hSession,hSession->saved_cursor);
	hSession->held_wrap = 0;
	return DATA;
}

static enum lib3270_ansi_state ansi_newline(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int nc;

	cursor_move(hSession,hSession->cursor_addr - (hSession->cursor_addr % hSession->view.cols));
	nc = hSession->cursor_addr + hSession->view.cols;

	if (nc < hSession->scroll_bottom * hSession->view.cols)
		cursor_move(hSession,nc);
	else
		ansi_scroll(hSession);

	hSession->held_wrap = 0;
	return DATA;
}

static enum lib3270_ansi_state
ansi_cursor_up(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	int rr;

	if (nn < 1)
		nn = 1;
	rr = hSession->cursor_addr / hSession->view.cols;
	if (rr - nn < 0)
		cursor_move(hSession, hSession->cursor_addr % hSession->view.cols);
	else
		cursor_move(hSession, hSession->cursor_addr - (nn * hSession->view.cols));
	hSession->held_wrap = 0;
	return DATA;
}

static enum lib3270_ansi_state
ansi_esc2(H3270 GNUC_UNUSED(*hSession), int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	register int	i;

	for (i = 0; i < NN; i++)
		n[i] = 0;
	nx = 0;
	return N1;
}

static enum lib3270_ansi_state ansi_reset(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int i;
//	static Boolean first = True;

	hSession->gr = 0;
	hSession->saved_gr = 0;
	hSession->fg = 0;
	hSession->saved_fg = 0;
	hSession->bg = 0;
	hSession->saved_bg = 0;
	hSession->cset = CS_G0;
	hSession->saved_cset = CS_G0;

	for(i=0;i<4;i++)
		hSession->csd[i] = hSession->saved_csd[i] = LIB3270_ANSI_CSD_US;

	hSession->once_cset = -1;
	hSession->saved_cursor = 0;
	hSession->insert_mode = 0;
	hSession->auto_newline_mode = 0;
	hSession->appl_cursor = 0;
	hSession->saved_appl_cursor = 0;
	hSession->wraparound_mode = 1;
	hSession->saved_wraparound_mode = 1;
	hSession->rev_wraparound_mode = 0;
	hSession->saved_rev_wraparound_mode = 0;
	hSession->allow_wide_mode = 0;
//	allow_wide_mode = 0;

	hSession->saved_allow_wide_mode = 0;
	hSession->wide_mode = 0;
	hSession->saved_altbuffer = 0;

	hSession->scroll_top = 1;
	hSession->scroll_bottom = hSession->view.rows;

	Replace(hSession->tabs, (unsigned char *)lib3270_malloc((hSession->view.cols+7)/8));
	for (i = 0; i < (hSession->view.cols+7)/8; i++)
		hSession->tabs[i] = 0x01;

	hSession->held_wrap = 0;
	if (!hSession->ansi_reset)
	{
		ctlr_altbuffer(hSession,True);
		ctlr_aclear(hSession, 0, hSession->view.rows * hSession->view.cols, 1);
		ctlr_altbuffer(hSession,False);
		ctlr_clear(hSession,False);
		hSession->cbk.set_width(hSession,80);
		hSession->ansi_reset = 1;
	}
	hSession->pmi = 0;
	return DATA;
}

static enum lib3270_ansi_state
ansi_insert_chars(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	int cc = hSession->cursor_addr % hSession->view.cols;	/* current col */
	int mc = hSession->view.cols - cc;		/* max chars that can be inserted */
	int ns;				/* chars that are shifting */

	if (nn < 1)
		nn = 1;
	if (nn > mc)
		nn = mc;

	/* Move the surviving chars right */
	ns = mc - nn;
	if (ns)
		ctlr_bcopy(hSession,hSession->cursor_addr, hSession->cursor_addr + nn, ns, 1);

	/* Clear the middle of the line */
	ctlr_aclear(hSession, hSession->cursor_addr, nn, 1);
	return DATA;
}

static enum lib3270_ansi_state
ansi_cursor_down(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	int rr;

	if (nn < 1)
		nn = 1;
	rr = hSession->cursor_addr / hSession->view.cols;
	if (rr + nn >= hSession->view.cols)
		cursor_move(hSession,(hSession->view.cols-1)*hSession->view.cols + (hSession->cursor_addr % hSession->view.cols));
	else
		cursor_move(hSession,hSession->cursor_addr + (nn * hSession->view.cols));
	hSession->held_wrap = 0;
	return DATA;
}

static enum lib3270_ansi_state ansi_cursor_right(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	int cc;

	if (nn < 1)
		nn = 1;
	cc = hSession->cursor_addr % hSession->view.cols;
	if (cc == hSession->view.cols-1)
		return DATA;
	if (cc + nn >= hSession->view.cols)
		nn = hSession->view.cols - 1 - cc;
	cursor_move(hSession,hSession->cursor_addr + nn);
	hSession->held_wrap = 0;
	return DATA;
}

static enum lib3270_ansi_state
ansi_cursor_left(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	int cc;

	if (hSession->held_wrap)
	{
		hSession->held_wrap = 0;
		return DATA;
	}
	if (nn < 1)
		nn = 1;
	cc = hSession->cursor_addr % hSession->view.cols;
	if (!cc)
		return DATA;
	if (nn > cc)
		nn = cc;
	cursor_move(hSession,hSession->cursor_addr - nn);
	return DATA;
}

static enum lib3270_ansi_state
ansi_cursor_motion(H3270 *hSession, int n1, int n2)
{
	if (n1 < 1) n1 = 1;
	if (n1 > hSession->view.rows) n1 = hSession->view.rows;
	if (n2 < 1) n2 = 1;
	if (n2 > hSession->view.cols) n2 = hSession->view.cols;
	cursor_move(hSession,(n1 - 1) * hSession->view.cols + (n2 - 1));
	hSession->held_wrap = 0;
	return DATA;
}

static enum lib3270_ansi_state
ansi_erase_in_display(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	switch (nn) {
	    case 0:	/* below */
		ctlr_aclear(hSession, hSession->cursor_addr, (hSession->view.rows * hSession->view.cols) - hSession->cursor_addr, 1);
		break;
	    case 1:	/* above */
		ctlr_aclear(hSession, 0, hSession->cursor_addr + 1, 1);
		break;
	    case 2:	/* all (without moving cursor) */
//		if (hSession->cursor_addr == 0 && !hSession->is_altbuffer) scroll_save(hSession->rows, True);
		ctlr_aclear(hSession, 0, hSession->view.rows * hSession->view.cols, 1);
		break;
	}
	return DATA;
}

static enum lib3270_ansi_state
ansi_erase_in_line(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	int nc = hSession->cursor_addr % hSession->view.cols;

	switch (nn) {
	    case 0:	/* to right */
		ctlr_aclear(hSession, hSession->cursor_addr, hSession->view.cols - nc, 1);
		break;
	    case 1:	/* to left */
		ctlr_aclear(hSession, hSession->cursor_addr - nc, nc+1, 1);
		break;
	    case 2:	/* all */
		ctlr_aclear(hSession, hSession->cursor_addr - nc, hSession->view.cols, 1);
		break;
	}
	return DATA;
}

static enum lib3270_ansi_state
ansi_insert_lines(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	int rr = hSession->cursor_addr / hSession->view.cols;	/* current row */
	int mr = hSession->scroll_bottom - rr;		/* rows left at and below this one */
	int ns;										/* rows that are shifting */

	/* If outside of the scrolling region, do nothing */
	if (rr < hSession->scroll_top - 1 || rr >= hSession->scroll_bottom)
		return DATA;

	if (nn < 1)
		nn = 1;
	if (nn > mr)
		nn = mr;

	/* Move the victims down */
	ns = mr - nn;
	if (ns)
		ctlr_bcopy(hSession,rr * hSession->view.cols, (rr + nn) * hSession->view.cols, ns * hSession->view.cols, 1);

	/* Clear the middle of the screen */
	ctlr_aclear(hSession, rr * hSession->view.cols, nn * hSession->view.cols, 1);
	return DATA;
}

static enum lib3270_ansi_state
ansi_delete_lines(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	int rr = hSession->cursor_addr / hSession->view.cols;	/* current row */
	int mr = hSession->scroll_bottom - rr;				/* max rows that can be deleted */
	int ns;												/* rows that are shifting */

	/* If outside of the scrolling region, do nothing */
	if (rr < hSession->scroll_top - 1 || rr >= hSession->scroll_bottom)
		return DATA;

	if (nn < 1)
		nn = 1;
	if (nn > mr)
		nn = mr;

	/* Move the surviving rows up */
	ns = mr - nn;
	if (ns)
		ctlr_bcopy(hSession,(rr + nn) * hSession->view.cols, rr * hSession->view.cols, ns * hSession->view.cols, 1);

	/* Clear the rest of the screen */
	ctlr_aclear(hSession, (rr + ns) * hSession->view.cols, nn * hSession->view.cols, 1);
	return DATA;
}

static enum lib3270_ansi_state
ansi_delete_chars(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	int cc = hSession->cursor_addr % hSession->view.cols;	/* current col */
	int mc = hSession->view.cols - cc;						/* max chars that can be deleted */
	int ns;													/* chars that are shifting */

	if (nn < 1)
		nn = 1;
	if (nn > mc)
		nn = mc;

	/* Move the surviving chars left */
	ns = mc - nn;
	if (ns)
		ctlr_bcopy(hSession,hSession->cursor_addr + nn, hSession->cursor_addr, ns, 1);

	/* Clear the end of the line */
	ctlr_aclear(hSession, hSession->cursor_addr + ns, nn, 1);
	return DATA;
}

static enum lib3270_ansi_state
ansi_sgr(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int i;

	for (i = 0; i <= nx && i < NN; i++)
	    switch (n[i]) {
		case 0:
		    hSession->gr = 0;
		    hSession->fg = 0;
		    hSession->bg = 0;
		    break;
		case 1:
		    hSession->gr |= GR_INTENSIFY;
		    break;
		case 4:
		    hSession->gr |= GR_UNDERLINE;
		    break;
		case 5:
		    hSession->gr |= GR_BLINK;
		    break;
		case 7:
		    hSession->gr |= GR_REVERSE;
		    break;
		case 30:
		    hSession->fg = 0xf0;	/* black */
		    break;
		case 31:
		    hSession->fg = 0xf2;	/* red */
		    break;
		case 32:
		    hSession->fg = 0xf4;	/* green */
		    break;
		case 33:
		    hSession->fg = 0xf6;	/* yellow */
		    break;
		case 34:
		    hSession->fg = 0xf1;	/* blue */
		    break;
		case 35:
		    hSession->fg = 0xf3;	/* magenta */
		    break;
		case 36:
#if defined(WC3270) /*[*/
		    hSession->fg = 0xf6;	/* turquoise */
#else /*][*/
		    hSession->fg = 0xfd;	/* cyan */
#endif /*]*/
		    break;
		case 37:
#if defined(WC3270) /*[*/
		    hSession->fg = 0xf7;	/* white */
#else /*][*/
		    hSession->fg = 0xff;	/* white */
#endif /*]*/
		    break;
		case 39:
		    hSession->fg = 0;	/* default */
		    break;
		case 40:
		    hSession->bg = 0xf0;	/* black */
		    break;
		case 41:
		    hSession->bg = 0xf2;	/* red */
		    break;
		case 42:
		    hSession->bg = 0xf4;	/* green */
		    break;
		case 43:
		    hSession->bg = 0xf6;	/* yellow */
		    break;
		case 44:
		    hSession->bg = 0xf1;	/* blue */
		    break;
		case 45:
		    hSession->bg = 0xf3;	/* magenta */
		    break;
		case 46:
#if defined(WC3270) /*[*/
		    hSession->bg = 0xf6;	/* turquoise */
#else /*][*/
		    hSession->bg = 0xfd;	/* cyan */
#endif /*]*/
		    break;
		case 47:
#if defined(WC3270) /*[*/
		    hSession->bg = 0xf7;	/* white */
#else /*][*/
		    hSession->bg = 0xff;	/* white */
#endif /*]*/
		    break;
		case 49:
		    hSession->bg = 0;	/* default */
		    break;
	    }

	return DATA;
}

static enum lib3270_ansi_state
ansi_bell(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	lib3270_ring_bell(hSession);
	return DATA;
}

static enum lib3270_ansi_state ansi_newpage(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	ctlr_clear(hSession,False);
	return DATA;
}

static enum lib3270_ansi_state ansi_backspace(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	if (hSession->held_wrap)
	{
		hSession->held_wrap = 0;
		return DATA;
	}

	if (hSession->rev_wraparound_mode)
	{
		if (hSession->cursor_addr > (hSession->scroll_top - 1) * hSession->view.cols)
			cursor_move(hSession,hSession->cursor_addr - 1);
	}
	else
	{
		if (hSession->cursor_addr % hSession->view.cols)
			cursor_move(hSession,hSession->cursor_addr - 1);
	}
	return DATA;
}

static enum lib3270_ansi_state ansi_cr(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	if (hSession->cursor_addr % hSession->view.cols)
		cursor_move(hSession,hSession->cursor_addr - (hSession->cursor_addr % hSession->view.cols));

	if (hSession->auto_newline_mode)
		(void) ansi_lf(hSession, 0, 0);

	hSession->held_wrap = 0;
	return DATA;
}

static enum lib3270_ansi_state ansi_lf(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int nc = hSession->cursor_addr + hSession->view.cols;

	hSession->held_wrap = 0;

	// If we're below the scrolling region, don't scroll.
	if((hSession->cursor_addr / hSession->view.cols) >= hSession->scroll_bottom)
	{
		if (nc < hSession->view.rows * hSession->view.cols)
			cursor_move(hSession,nc);
		return DATA;
	}

	if (nc < hSession->scroll_bottom * hSession->view.cols)
		cursor_move(hSession,nc);
	else
		ansi_scroll(hSession);
	return DATA;
}

static enum lib3270_ansi_state ansi_htab(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int col = hSession->cursor_addr % hSession->view.cols;
	int i;

	hSession->held_wrap = 0;
	if (col == hSession->view.cols-1)
		return DATA;
	for (i = col+1; i < hSession->view.cols-1; i++)
		if (hSession->tabs[i/8] & 1<<(i%8))
			break;
	cursor_move(hSession,hSession->cursor_addr - col + i);
	return DATA;
}

static enum lib3270_ansi_state ansi_escape(H3270 GNUC_UNUSED(*hSession), int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	return ESC;
}

static enum lib3270_ansi_state ansi_nop(H3270 GNUC_UNUSED(*hSession), int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	return DATA;
}

#define PWRAP { \
    nc = hSession->cursor_addr + 1; \
    if (nc < hSession->scroll_bottom * hSession->view.cols) \
	    cursor_move(hSession,nc); \
    else { \
	    if (hSession->cursor_addr / hSession->view.cols >= hSession->scroll_bottom) \
		    cursor_move(hSession,hSession->cursor_addr / hSession->view.cols * hSession->view.cols); \
	    else { \
		    ansi_scroll(hSession); \
		    cursor_move(hSession,nc - hSession->view.cols); \
	    } \
    } \
}

static enum lib3270_ansi_state
ansi_printing(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int nc;
	unsigned char ebc_ch;
	int default_cs = CS_BASE;
#if defined(X3270_DBCS) /*[*/
	enum dbcs_state d;
	Boolean preserve_right = False;
#endif /*]*/

/*
	if ((hSession->pmi == 0) && (hSession->ansi_ch & 0x80)) {
	    	char mbs[2];
		enum ulfail fail;
		unsigned char ch;

		mbs[0] = (char)hSession->ansi_ch;
		mbs[1] = '\0';

		ch = utf8_lookup(mbs, &fail, NULL);
		if (ch == 0) {
			switch (fail) {
			case ULFAIL_NOUTF8:
			    	// Leave it alone.
				break;
			case ULFAIL_INCOMPLETE:
				// Start munching multi-byte.
				hSession->pmi = 0;
				hSession->pending_mbs[hSession->pmi++] = (char)hSession->ansi_ch;
				return MBPEND;
			case ULFAIL_INVALID:
				// Invalid multi-byte -> '?'
				hSession->ansi_ch = '?';
				// XXX: If DBCS, we should let
				// ICU have a crack at it
				//
				break;
			}
		}
	}
	*/
	hSession->pmi = 0;

	if (hSession->held_wrap)
	{
		PWRAP;
		hSession->held_wrap = 0;
	}

	if (hSession->insert_mode)
		(void) ansi_insert_chars(hSession,1, 0);

	switch(hSession->csd[(hSession->once_cset != -1) ? hSession->once_cset : hSession->cset])
	{
	    case CSD_LD:	/* line drawing "0" */
		if (hSession->ansi_ch >= 0x5f && hSession->ansi_ch <= 0x7e)
			ctlr_add(hSession,hSession->cursor_addr, (unsigned char)(hSession->ansi_ch - 0x5f),CS_LINEDRAW);
		else
			ctlr_add(hSession,hSession->cursor_addr, hSession->charset.asc2ebc[hSession->ansi_ch], CS_BASE);
		break;
	    case CSD_UK:	/* UK "A" */
		if (hSession->ansi_ch == '#')
			ctlr_add(hSession,hSession->cursor_addr, 0x1e, CS_LINEDRAW);
		else
			ctlr_add(hSession,hSession->cursor_addr, hSession->charset.asc2ebc[hSession->ansi_ch], CS_BASE);
		break;
	    case CSD_US:	/* US "B" */
		ebc_ch = hSession->charset.asc2ebc[hSession->ansi_ch];
#if defined(X3270_DBCS) /*[*/
		d = ctlr_dbcs_state(cursor_addr);
		if (dbcs) {
			if (mb_pending || (ansi_ch & 0x80)) {
				int len;
				unsigned char ebc[2];

				len = dbcs_process(hSession, ansi_ch, ebc);
				switch (len) {
				    default:
				    case 0:
					/* Translation failed. */
					return DATA;
				    case 1:
					/* It was really SBCS. */
					ebc_ch = ebc[0];
					break;
				    case 2:
					/* DBCS. */
					if ((cursor_addr % COLS) == (COLS-1)) {
						ebc_ch = EBC_space;
						break;
					}
					ctlr_add(cursor_addr, ebc[0], CS_DBCS);
					ctlr_add_gr(cursor_addr, gr);
					ctlr_add_fg(cursor_addr, fg);
					ctlr_add_bg(cursor_addr, bg);
					if (wraparound_mode) {
						if (!((cursor_addr + 1) % COLS)) {
							held_wrap = 1;
						} else {
							PWRAP;
						}
					} else {
						if ((cursor_addr % COLS) != (COLS - 1))
							cursor_move(cursor_addr + 1);
					}

					/*
					 * Set up the right-hand side to be
					 * stored below.
					 */
					ebc_ch = ebc[1];
					default_cs = CS_DBCS;
					preserve_right = True;
					break;
				}
			} else if (ansi_ch & 0x80) {
				(void) dbcs_process(hSession, ansi_ch, NULL);
				ebc_ch = EBC_space;
			}
		}

		/* Handle conflicts with existing DBCS characters. */
		if (!preserve_right &&
		    (d == DBCS_RIGHT || d == DBCS_RIGHT_WRAP)) {
			int xaddr;

			xaddr = cursor_addr;
			DEC_BA(xaddr);
			ctlr_add(xaddr, EBC_space, CS_BASE);
			ea_buf[xaddr].db = DBCS_NONE;
			ea_buf[cursor_addr].db = DBCS_NONE;
		}
		if (d == DBCS_LEFT || d == DBCS_LEFT_WRAP) {
			int xaddr;

			xaddr = cursor_addr;
			INC_BA(xaddr);
			ctlr_add(xaddr, EBC_space, CS_BASE);
			ea_buf[xaddr].db = DBCS_NONE;
			ea_buf[cursor_addr].db = DBCS_NONE;
		}
#endif /*]*/
		ctlr_add(hSession,hSession->cursor_addr, ebc_ch, default_cs);
#if defined(X3270_DBCS) /*[*/
		if (default_cs == CS_DBCS)
			(void) ctlr_dbcs_postprocess(hSession);
#endif /*]*/
		break;
	}
	hSession->once_cset = -1;
	ctlr_add_gr(hSession,hSession->cursor_addr, hSession->gr);
	ctlr_add_fg(hSession,hSession->cursor_addr, hSession->fg);
	ctlr_add_bg(hSession,hSession->cursor_addr, hSession->bg);
	if (hSession->wraparound_mode) {
		/*
		 * There is a fascinating behavior of xterm which we will
		 * attempt to emulate here.  When a character is printed in the
		 * last column, the cursor sticks there, rather than wrapping
		 * to the next line.  Another printing character will put the
		 * cursor in column 2 of the next line.  One cursor-left
		 * sequence won't budge it; two will.  Saving and restoring
		 * the cursor won't move the cursor, but will cancel all of
		 * the above behaviors...
		 *
		 * In my opinion, very strange, but among other things, 'vi'
		 * depends on it!
		 */
		if (!((hSession->cursor_addr + 1) % hSession->view.cols)) {
			hSession->held_wrap = 1;
		} else {
			PWRAP;
		}
	} else {
		if ((hSession->cursor_addr % hSession->view.cols) != (hSession->view.cols - 1))
			cursor_move(hSession,hSession->cursor_addr + 1);
	}
	return DATA;
}

static enum lib3270_ansi_state ansi_multibyte(H3270 *hSession, int ig1, int ig2)
{
	char mbs[MB_MAX];
	unsigned char ch;
//	enum ulfail fail;
	afn_t fn;

	if (hSession->pmi >= MB_MAX - 2)
	{
		/* String too long. */
		hSession->pmi = 0;
		hSession->ansi_ch = '?';
		return ansi_printing(hSession,ig1, ig2);
	}

	strncpy(mbs, hSession->pending_mbs, hSession->pmi);
	mbs[hSession->pmi] = (char) hSession->ansi_ch;
	mbs[hSession->pmi + 1] = '\0';

	/*
	ch = utf8_lookup(mbs, &fail, NULL);
	if (ch != 0)
	{
		// Success!
		hSession->ansi_ch = ch;
		return ansi_printing(hSession, ig1, ig2);
	}

	if (fail == ULFAIL_INCOMPLETE)
	{
	   	// Go get more.
    	hSession->pending_mbs[hSession->pmi++] = (char)hSession->ansi_ch;
		return MBPEND;
	}
	*/

	/* Failure. */

	/* Replace the sequence with '?'. */
	ch = hSession->ansi_ch; /* save for later */
	hSession->pmi = 0;
	hSession->ansi_ch = '?';
	(void) ansi_printing(hSession, ig1, ig2);

	/* Reprocess whatever we choked on. */
	hSession->ansi_ch = ch;
	hSession->state = DATA;
	fn = ansi_fn[st[(int)DATA][hSession->ansi_ch]];
	return (*fn)(hSession,n[0], n[1]);
}

static enum lib3270_ansi_state ansi_semicolon(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	if (nx >= NN)
		return DATA;
	nx++;
	return hSession->state;
}

static enum lib3270_ansi_state ansi_digit(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	n[nx] = (n[nx] * 10) + (hSession->ansi_ch - '0');
	return hSession->state;
}

static enum lib3270_ansi_state ansi_reverse_index(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int rr = hSession->cursor_addr / hSession->view.cols;	/* current row */
	int np = (hSession->scroll_top - 1) - rr;				/* number of rows in the scrolling region, above this line */
	int ns;													/* number of rows to scroll */
	int nn = 1;												/* number of rows to index */

	hSession->held_wrap = 0;

	/* If the cursor is above the scrolling region, do a simple margined
	   cursor up.  */
	if (np < 0) {
		(void) ansi_cursor_up(hSession, nn, 0);
		return DATA;
	}

	/* Split the number of lines to scroll into ns */
	if (nn > np) {
		ns = nn - np;
		nn = np;
	} else
		ns = 0;

	/* Move the cursor up without scrolling */
	if (nn)
		(void) ansi_cursor_up(hSession,nn, 0);

	/* Insert lines at the top for backward scroll */
	if (ns)
		(void) ansi_insert_lines(hSession, ns, 0);

	return DATA;
}

static enum lib3270_ansi_state ansi_send_attributes(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	if (!nn)
		net_sends(hSession,"\033[?1;2c");
	return DATA;
}

static enum lib3270_ansi_state dec_return_terminal_id(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	return ansi_send_attributes(hSession, 0, 0);
}

static enum lib3270_ansi_state ansi_set_mode(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	switch (nn)
	{
	case 4:
		hSession->insert_mode = 1;
		break;
	case 20:
		hSession->auto_newline_mode = 1;
		break;
	}
	return DATA;
}

static enum lib3270_ansi_state ansi_reset_mode(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	switch (nn)
	{
	case 4:
		hSession->insert_mode = 0;
		break;
	case 20:
		hSession->auto_newline_mode = 0;
		break;
	}
	return DATA;
}

static enum lib3270_ansi_state ansi_status_report(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	static char cpr[22];

	switch (nn)
	{
	case 5:
		net_sends(hSession,"\033[0n");
		break;

	case 6:
		(void) snprintf(cpr, 22, "\033[%d;%dR",(hSession->cursor_addr/hSession->view.cols) + 1, (hSession->cursor_addr%hSession->view.cols) + 1);
		net_sends(hSession,cpr);
		break;
	}
	return DATA;
}

static enum lib3270_ansi_state ansi_cs_designate(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	hSession->cs_to_change = strchr(gnnames, hSession->ansi_ch) - gnnames;
	return CSDES;
}

static enum lib3270_ansi_state ansi_cs_designate2(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	hSession->csd[hSession->cs_to_change] = strchr(csnames, hSession->ansi_ch) - csnames;
	return DATA;
}

static enum lib3270_ansi_state ansi_select_g0(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	hSession->cset = CS_G0;
	return DATA;
}

static enum lib3270_ansi_state ansi_select_g1(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	hSession->cset = CS_G1;
	return DATA;
}

static enum lib3270_ansi_state
ansi_select_g2(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	hSession->cset = CS_G2;
	return DATA;
}

static enum lib3270_ansi_state
ansi_select_g3(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	hSession->cset = CS_G3;
	return DATA;
}

static enum lib3270_ansi_state
ansi_one_g2(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	hSession->once_cset = CS_G2;
	return DATA;
}

static enum lib3270_ansi_state
ansi_one_g3(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	hSession->once_cset = CS_G3;
	return DATA;
}

static enum lib3270_ansi_state
ansi_esc3(H3270 GNUC_UNUSED(*hSession), int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	return DECP;
}

static enum lib3270_ansi_state
dec_set(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int i;

	for (i = 0; i <= nx && i < NN; i++)
		switch (n[i])
		{
		case 1:	/* application cursor keys */
			hSession->appl_cursor = 1;
			break;
		case 2:	/* set G0-G3 */
			hSession->csd[0] = hSession->csd[1] = hSession->csd[2] = hSession->csd[3] = CSD_US;
			break;
		case 3:	/* 132-column mode */
			if(hSession->allow_wide_mode)
			{
				hSession->wide_mode = 1;
				hSession->cbk.set_width(hSession,132);
			}
			break;
		case 7:	/* wraparound mode */
			hSession->wraparound_mode = 1;
			break;
		case 40:	/* allow 80/132 switching */
			hSession->allow_wide_mode = 1;
			break;
		case 45:	/* reverse-wraparound mode */
			hSession->rev_wraparound_mode = 1;
			break;
		case 47:	/* alt buffer */
			ctlr_altbuffer(hSession,True);
			break;
		}
	return DATA;
}

static enum lib3270_ansi_state
dec_reset(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int i;

	for (i = 0; i <= nx && i < NN; i++)
		switch (n[i])
		{
		case 1:	/* normal cursor keys */
			hSession->appl_cursor = 0;
			break;
		case 3:	/* 132-column mode */
			if (hSession->allow_wide_mode)
			{
				hSession->wide_mode = 0;
				hSession->cbk.set_width(hSession,80);
			}
			break;
		case 7:	/* no wraparound mode */
			hSession->wraparound_mode = 0;
			break;
		case 40:	/* allow 80/132 switching */
			hSession->allow_wide_mode = 0;
			break;
		case 45:	/* no reverse-wraparound mode */
			hSession->rev_wraparound_mode = 0;
			break;
		case 47:	/* alt buffer */
			ctlr_altbuffer(hSession,False);
			break;
		}
	return DATA;
}

static enum lib3270_ansi_state dec_save(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int i;

	for (i = 0; i <= nx && i < NN; i++)
		switch (n[i])
		{
		case 1:	/* application cursor keys */
			hSession->saved_appl_cursor = hSession->appl_cursor;
			break;
		case 3:	/* 132-column mode */
			hSession->saved_wide_mode = hSession->wide_mode;
			break;
		case 7:	/* wraparound mode */
			hSession->saved_wraparound_mode = hSession->wraparound_mode;
			break;
		case 40:	/* allow 80/132 switching */
			hSession->saved_allow_wide_mode = hSession->allow_wide_mode;
			break;
		case 45:	/* reverse-wraparound mode */
			hSession->saved_rev_wraparound_mode = hSession->rev_wraparound_mode;
			break;
		case 47:	/* alt buffer */
			hSession->saved_altbuffer = hSession->is_altbuffer;
			break;
		}
	return DATA;
}

static enum lib3270_ansi_state
dec_restore(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	int i;

	for (i = 0; i <= nx && i < NN; i++)
		switch (n[i])
		{
		case 1:	/* application cursor keys */
			hSession->appl_cursor = hSession->saved_appl_cursor;
			break;
		case 3:	/* 132-column mode */
			if (hSession->allow_wide_mode)
			{
				hSession->wide_mode = hSession->saved_wide_mode;
				hSession->cbk.set_width(hSession,hSession->wide_mode ? 132 : 80);
			}
			break;
		case 7:	/* wraparound mode */
			hSession->wraparound_mode = hSession->saved_wraparound_mode;
			break;
		case 40:	/* allow 80/132 switching */
			hSession->allow_wide_mode = hSession->saved_allow_wide_mode;
			break;
		case 45:	/* reverse-wraparound mode */
			hSession->rev_wraparound_mode = hSession->saved_rev_wraparound_mode;
			break;
		case 47:	/* alt buffer */
			ctlr_altbuffer(hSession,hSession->saved_altbuffer);
			break;
		}
	return DATA;
}

static enum lib3270_ansi_state
dec_scrolling_region(H3270 *hSession, int top, int bottom)
{
	if (top < 1)
		top = 1;
	if (bottom > hSession->view.rows)
		bottom = hSession->view.rows;

	if (top <= bottom && (top > 1 || bottom < hSession->view.rows))
	{
		hSession->scroll_top = top;
		hSession->scroll_bottom = bottom;
		cursor_move(hSession,0);
	}
	else
	{
		hSession->scroll_top = 1;
		hSession->scroll_bottom = hSession->view.rows;
	}
	return DATA;
}

static enum lib3270_ansi_state
xterm_text_mode(H3270 GNUC_UNUSED(*hSession), int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	nx = 0;
	n[0] = 0;
	return LIB3270_ANSI_STATE_TEXT;
}

static enum lib3270_ansi_state
xterm_text_semicolon(H3270 GNUC_UNUSED(*hSession), int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	tx = 0;
	return LIB3270_ANSI_STATE_TEXT2;
}

static enum lib3270_ansi_state
xterm_text(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	if (tx < NT)
		text[tx++] = hSession->ansi_ch;
	return hSession->state;
}

static enum lib3270_ansi_state
xterm_text_do(H3270 GNUC_UNUSED(*hSession), int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
/*
#if defined(X3270_DISPLAY) || defined(WC3270)
	text[tx] = '\0';
#endif

#if defined(X3270_DISPLAY)
	switch (n[0]) {
	    case 0:	// icon name and window title
		XtVaSetValues(toplevel, XtNiconName, text, NULL);
		XtVaSetValues(toplevel, XtNtitle, text, NULL);
		break;
	    case 1:	// icon name
		XtVaSetValues(toplevel, XtNiconName, text, NULL);
		break;
	    case 2:	// window_title
		XtVaSetValues(toplevel, XtNtitle, text, NULL);
		break;
	    case 50:	// font
		screen_newfont(text, False, False);
		break;
	    default:
		break;
	}
#endif


#if defined(WC3270)
	switch (n[0]) {
	    case 0:	// icon name and window title
	    case 2:	// window_title
		screen_title(text);
		break;
	    default:
		break;
	}
#endif
*/

	return DATA;
}

static enum lib3270_ansi_state
ansi_htab_set(H3270 *hSession, int GNUC_UNUSED(ig1), int GNUC_UNUSED(ig2))
{
	register int col = hSession->cursor_addr % hSession->view.cols;

	hSession->tabs[col/8] |= 1<<(col%8);
	return DATA;
}

static enum lib3270_ansi_state
ansi_htab_clear(H3270 *hSession, int nn, int GNUC_UNUSED(ig2))
{
	register int col, i;

	switch (nn)
	{
	case 0:
		col = hSession->cursor_addr % hSession->view.cols;
		hSession->tabs[col/8] &= ~(1<<(col%8));
		break;
	case 3:
		for (i = 0; i < (hSession->view.cols+7)/8; i++)
			hSession->tabs[i] = 0;
		break;
	}
	return DATA;
}

/*
 * Scroll the screen or the scrolling region.
 */
static void ansi_scroll(H3270 *hSession)
{
	hSession->held_wrap = 0;

	/* Save the top line */
	if (hSession->scroll_top == 1 && hSession->scroll_bottom == hSession->view.rows)
	{
//		if (!hSession->is_altbuffer)
//			scroll_save(1, False);
		ctlr_scroll(hSession);
		return;
	}

	/* Scroll all but the last line up */
	if (hSession->scroll_bottom > hSession->scroll_top)
		ctlr_bcopy(hSession,hSession->scroll_top * hSession->view.cols,
		    (hSession->scroll_top - 1) * hSession->view.cols,
		    (hSession->scroll_bottom - hSession->scroll_top) * hSession->view.cols,
		    1);

	/* Clear the last line */
	ctlr_aclear(hSession, (hSession->scroll_bottom - 1) * hSession->view.cols, hSession->view.cols, 1);
}

/* Callback for when we enter ANSI mode. */
void ansi_in3270(H3270 *session, int in3270, void GNUC_UNUSED(*dunno))
{
	if (!in3270)
		(void) ansi_reset(session, 0, 0);
}

#if defined(X3270_DBCS) /*[*/
static void trace_pending_mb(H3270 *hSession)
{
	int i;

	for (i = 0; i < mb_pending; i++)
	{
		trace_ds(hSession," %02x", mb_buffer[i] & 0xff);
	}
}
#endif /*]*/


/*
 * External entry points
 */
void
ansi_process(H3270 *hSession, unsigned int c)
{
	afn_t fn;

	c &= 0xff;
	hSession->ansi_ch = c;

//	scroll_to_bottom();

#if defined(X3270_TRACE) /*[*/
	if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_SCREEN_TRACE))
		trace_char(hSession,(char)c);
#endif /*]*/

	fn = ansi_fn[st[(int)hSession->state][c]];

#if defined(X3270_DBCS) /*[*/
	if (mb_pending && fn != &ansi_printing)
	{
		trace_ds(hSession,"Dropped incomplete multi-byte character");
		trace_pending_mb(hSession);
		trace_ds(hSession,"\n");
		mb_pending = 0;
	}
#endif /*]*/

	hSession->state = (*fn)(hSession, n[0], n[1]);
}

void
ansi_send_up(H3270 *hSession)
{
	if (hSession->appl_cursor)
		net_sends(hSession,"\033OA");
	else
		net_sends(hSession,"\033[A");
}

void
ansi_send_down(H3270 *hSession)
{
	if (hSession->appl_cursor)
		net_sends(hSession,"\033OB");
	else
		net_sends(hSession,"\033[B");
}

void
ansi_send_right(H3270 *hSession)
{
	if (hSession->appl_cursor)
		net_sends(hSession,"\033OC");
	else
		net_sends(hSession,"\033[C");
}

void
ansi_send_left(H3270 *hSession)
{
	if (hSession->appl_cursor)
		net_sends(hSession,"\033OD");
	else
		net_sends(hSession,"\033[D");
}

void
ansi_send_home(H3270 *hSession)
{
	net_sends(hSession,"\033[H");
}

void
ansi_send_clear(H3270 *hSession)
{
	net_sends(hSession,"\033[2K");
}

void
ansi_send_pf(H3270 *hSession, int nn)
{
	static char fn_buf[6];
	static int code[] = {
		/*
		 * F1 through F12 are VT220 codes. (Note the discontinuity --
		 * \E[16~ is missing)
		 */
		11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 23, 24,
		/*
		 * F13 through F20 are defined for xterm.
		 */
		25, 26, 28, 29, 31, 32, 33, 34,
		/*
		 * F21 through F24 are x3270 extensions.
		 */
		35, 36, 37, 38
	};

	if (nn < 1 || ((size_t) nn) > sizeof(code)/sizeof(code[0]))
		return;
	(void) sprintf(fn_buf, "\033[%d~", code[nn-1]);
	net_sends(hSession,fn_buf);
}

void ansi_send_pa(H3270 *hSession, int nn)
{
	static char fn_buf[4];
	static char code[4] = { 'P', 'Q', 'R', 'S' };

	if (nn < 1 || nn > 4)
		return;
	(void) sprintf(fn_buf, "\033O%c", code[nn-1]);
	net_sends(hSession,fn_buf);
}

void toggle_lineWrap(H3270 *hSession, struct lib3270_toggle GNUC_UNUSED(*t), LIB3270_TOGGLE_TYPE GNUC_UNUSED(type))
{
	if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_LINE_WRAP))
		hSession->wraparound_mode = 1;
	else
		hSession->wraparound_mode = 0;
}

#if defined(X3270_DBCS) /*[*/
/* Accumulate and process pending DBCS characters. */
static int dbcs_process(H3270 *hSession, int ch, unsigned char ebc[])
{
	UChar Ubuf[2];
	UErrorCode err = U_ZERO_ERROR;

	// See if we have too many.
	if (mb_pending >= MB_MAX) {
		trace_ds(hSession,"Multi-byte character ");
		trace_pending_mb(hSession);
		trace_ds(hSession," too long, dropping\n");
		mb_pending = 0;
		return 0;
	}


	// Store it and see if we're done.
	mb_buffer[mb_pending++] = ch & 0xff;
	// An interesting idea.
	if (mb_pending == 1)
	    	return 0;

	if (mb_to_unicode(mb_buffer, mb_pending, Ubuf, 2, &err) > 0) {
		// It translated!
		if (dbcs_map8(Ubuf[0], ebc)) {
			mb_pending = 0;
			return 1;
		} else if (dbcs_map16(Ubuf[0], ebc)) {
			mb_pending = 0;
			return 2;
		} else {
			trace_ds(hSession,"Can't map multi-byte character");
			trace_pending_mb(hSession);
			trace_ds(hSession," -> U+%04x to SBCS or DBCS, dropping\n",
			    Ubuf[0] & 0xffff);
			mb_pending = 0;
			return 0;
		}
	}

	// It failed.  See why
	switch (err) {
	case U_TRUNCATED_CHAR_FOUND:
		/* 'Cause we're not finished. */
		return 0;
	case U_INVALID_CHAR_FOUND:
	case U_ILLEGAL_CHAR_FOUND:
		trace_ds(hSession,"Invalid multi-byte character");
		trace_pending_mb(hSession);
		trace_ds(hSession,", dropping\n");
		break;
	default:
		trace_ds(hSession,"Unexpected ICU error %d translating multi-type character", (int)err);
		trace_pending_mb(hSession);
		trace_ds(hSession,", dropping\n");
		break;
	}
	mb_pending = 0;
	return 0;
}
#endif /*]*/

#endif /*]*/
