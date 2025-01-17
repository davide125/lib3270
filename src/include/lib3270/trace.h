/*
 * "Software pw3270, desenvolvido com base nos códigos fontes do WC3270  e X3270
 * (Paul Mattes Paul.Mattes@usa.net), de emulação de terminal 3270 para acesso a
 * aplicativos mainframe. Registro no INPI sob o nome G3270.
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
 * Este programa está nomeado como session.h e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 * kraucer@bb.com.br		(Kraucer Fernandes Mazuco)
 *
 */

#ifndef LIB3270_TRACE_H_INCLUDED

	#define LIB3270_TRACE_H_INCLUDED 1

	#include <lib3270.h>
	#include <lib3270/os.h>

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef _WIN32
	#define LIB3270_AS_PRINTF(a,b) /* __attribute__((format(printf, a, b))) */
#else
	#define LIB3270_AS_PRINTF(a,b) __attribute__((format(printf, a, b)))
#endif

	typedef void (*LIB3270_TRACE_HANDLER)(H3270 *, void *, const char *, va_list);

	/**
	 * @brief Set trace handle callback.
	 *
	 * @param hSession	TN3270 Session handle.
	 * @param handler	Callback to write in trace file or show trace window (NULL send all trace to stdout/syslog).
	 * @param userdata	User data to pass to the trace handler.
	 *
	 */
	LIB3270_EXPORT void lib3270_set_trace_handler(H3270 *hSession, LIB3270_TRACE_HANDLER handler, void *userdata);

	/**
	 * @brief Get trace handle callback.
	 *
	 * @param hSession	TN3270 Session handle.
	 * @param handler	Callback to write in trace file or show trace window (NULL send all trace to stdout/syslog).
	 * @param userdata	User data to pass to the trace handler.
	 *
	 */
	LIB3270_EXPORT void lib3270_get_trace_handler(H3270 *hSession, LIB3270_TRACE_HANDLER *handler, void **userdata);

	/**
	 * @brief Write on trace file.
	 *
	 * Write text on trace file.
	 *
	 * @param fmt 	String format.
	 * @param ...	Arguments.
	 *
	 */
	LIB3270_EXPORT void lib3270_write_trace(H3270 *session, const char *fmt, ...) LIB3270_AS_PRINTF(2,3);

	/**
	 * @brief Write on trace file.
	 *
	 * Write text on trace file, if DStrace is enabled.
	 *
	 * @param fmt 	String format.
	 * @param ...	Arguments.
	 *
	 */
	LIB3270_EXPORT void lib3270_write_dstrace(H3270 *session, const char *fmt, ...) LIB3270_AS_PRINTF(2,3);

	/**
	 * @brief Write on trace file.
	 *
	 * Write text on trace file, if network trace is enabled.
	 *
	 * @param fmt 	String format.
	 * @param ...	Arguments.
	 *
	 */
	LIB3270_EXPORT void lib3270_write_nettrace(H3270 *session, const char *fmt, ...) LIB3270_AS_PRINTF(2,3);

		/**
	 * @brief Write on trace file.
	 *
	 * Write text on trace file, if screen trace is enabled.
	 *
	 * @param fmt 	String format.
	 * @param ...	Arguments.
	 *
	 */
	LIB3270_EXPORT void lib3270_write_screen_trace(H3270 *session, const char *fmt, ...) LIB3270_AS_PRINTF(2,3);

	/**
	 * @brief Write on trace file.
	 *
	 * Write text on trace file, if event is enabled.
	 *
	 * @param fmt 	String format.
	 * @param ...	Arguments.
	 *
	 */
	LIB3270_EXPORT void lib3270_write_event_trace(H3270 *session, const char *fmt, ...) LIB3270_AS_PRINTF(2,3);

	LIB3270_EXPORT void LIB3270_DEPRECATED(lib3270_trace_event(H3270 *session, const char *fmt, ...)) LIB3270_AS_PRINTF(2,3);

	/**
	 * @brief Write datablock on trace file.
	 *
	 * @param hSession	TN3270 Session handle.
	 * @param msg		Message.
	 * @param data		Data block in ASCII.
	 * @param datalen	Length of the data block.
	 *
	 */
	LIB3270_EXPORT void lib3270_trace_data(H3270 *hSession, const char *msg, const unsigned char *data, size_t datalen);


#ifdef __cplusplus
	}
#endif

#endif // LIB3270_TRACE_H_INCLUDED
