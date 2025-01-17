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
 * Este programa está nomeado como - e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 *
 */

#include "private.h"
#include <utilc.h>

/*--[ Implement ]------------------------------------------------------------------------------------*/

 char * lib3270_url_get(H3270 *hSession, const char *u, const char **error) {

	lib3270_autoptr(char) url = lib3270_unescape(u);

	if(strncasecmp(url,"file://",7) == 0) {

		// Load local file contents.
		char *rc = lib3270_file_get_contents(hSession,url+7);
		if(!rc)
			*error = strerror(errno);
		return rc;
	}

	if(strncasecmp(url,"http://",7) == 0 || strncasecmp(url,"https://",8)) {

		// Use WinHTTP
		return lib3270_url_get_using_http(hSession, url, error);

	}

#if defined(HAVE_LIBCURL)

	return lib3270_url_get_using_curl(hSession,url,error);

#else

	// Can't get contents
	*error = _("No handler for URL scheme.");
	errno = EINVAL;
	return NULL;


#endif // HAVE_LIBCURL


 }
