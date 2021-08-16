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
 * Este programa está nomeado como log.c e possui 151 linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 * licinio@bb.com.br		(Licínio Luis Branco)
 * kraucer@bb.com.br		(Kraucer Fernandes Mazuco)
 * macmiranda@bb.com.br		(Marco Aurélio Caldas Miranda)
 *
 */

#include <config.h>

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#endif // WIN32

#include <internals.h>
#include <stdio.h>
#include <stdarg.h>
#include <config.h>
#include <lib3270.h>
#include <lib3270/log.h>
#include <errno.h>

/*---[ Constants ]------------------------------------------------------------------------------------------*/

static void (*loghandler)(H3270 *session, const char *module, int rc, const char *fmt, va_list arg_ptr) = default_log_writer;

/*---[ Implementacao ]--------------------------------------------------------------------------------------*/

static void logfile(H3270 *session, const char *module, int rc, const char *fmt, va_list arg_ptr) {

	FILE *f = fopen(session->logfile, "a");

	if(f) {

		time_t ltime = time(0);

	   char timestamp[80];
#ifdef HAVE_LOCALTIME_R
		struct tm tm;
		strftime(timestamp, 79, "%x %X", localtime_r(&ltime,&tm));
#else
		strftime(timestamp, 79, "%x %X", localtime(&ltime));
#endif // HAVE_LOCALTIME_R

		fprintf(f,"%s %s\t",timestamp,module);
		vfprintf(f,fmt,arg_ptr);
		fprintf(f,"\n");

		fclose(f);

	} else {

		loghandler(session,module,rc,fmt,arg_ptr);

	}

}

LIB3270_EXPORT const char * lib3270_get_log_filename(H3270 * hSession) {
	return hSession->logfile;
}

LIB3270_EXPORT int lib3270_set_log_filename(H3270 * hSession, const char *filename) {

	if(hSession->logfile) {
		lib3270_free(hSession->logfile);
	}

	if(filename && *filename) {
		hSession->logfile = lib3270_strdup(filename);
	} else {
		hSession->logfile = NULL;
	}

	return 0;

}

LIB3270_EXPORT void lib3270_set_log_handler(void (*handler)(H3270 *, const char *, int, const char *, va_list)) {
	loghandler = handler ? handler : default_log_writer;
}

LIB3270_EXPORT int lib3270_write_log(H3270 *session, const char *module, const char *fmt, ...) {
	va_list arg_ptr;
	va_start(arg_ptr, fmt);

	if(session && session->logfile) {
		logfile(session,module ? module : LIB3270_STRINGIZE_VALUE_OF(PRODUCT_NAME),0,fmt,arg_ptr);
	} else {
		loghandler(session,module ? module : LIB3270_STRINGIZE_VALUE_OF(PRODUCT_NAME),0,fmt,arg_ptr);
	}

	va_end(arg_ptr);
	return 0;
}

LIB3270_EXPORT int lib3270_write_rc(H3270 *session, const char *module, int rc, const char *fmt, ...) {
	va_list arg_ptr;
	va_start(arg_ptr, fmt);

	if(session && session->logfile) {
		logfile(session,module ? module : LIB3270_STRINGIZE_VALUE_OF(PRODUCT_NAME),rc,fmt,arg_ptr);
	} else {
		loghandler(session,module ? module : LIB3270_STRINGIZE_VALUE_OF(PRODUCT_NAME),rc,fmt,arg_ptr);
	}

	va_end(arg_ptr);
	return rc;
}

LIB3270_EXPORT void lib3270_write_va_log(H3270 *session, const char *module, const char *fmt, va_list arg) {
	if(session && session->logfile) {
		logfile(session,module ? module : LIB3270_STRINGIZE_VALUE_OF(PRODUCT_NAME),0,fmt,arg);
	} else {
		loghandler(session,module ? module : LIB3270_STRINGIZE_VALUE_OF(PRODUCT_NAME),0,fmt,arg);
	}
}

