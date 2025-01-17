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
 * Este programa está nomeado como bounds.c e possui - linhas de código.
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

#include <internals.h>

/*--[ Implement ]------------------------------------------------------------------------------------*/

/**
 * Get field region
 *
 * @param hSession	Session handle.
 * @param baddr		Reference position to get the field start/stop offsets.
 * @param start		return location for start of selection, as a character offset.
 * @param end		return location for end of selection, as a character offset.
 *
 * @return Non zero if invalid or not connected (sets errno).
 *
 */
LIB3270_EXPORT int lib3270_get_field_bounds(H3270 *hSession, int baddr, int *start, int *end)
{
	int first;

	first = lib3270_field_addr(hSession,baddr);

	if(first < 0)
		return -first;

	first++;

	if(start)
		*start = first;

	if(end)
	{
		int maxlen = (hSession->view.rows * hSession->view.cols)-1;
		*end	= first + lib3270_field_length(hSession,first);
		if(*end > maxlen)
			*end = maxlen;
	}

	return 0;
}

LIB3270_EXPORT int lib3270_get_word_bounds(H3270 *session, int baddr, int *start, int *end)
{
	int pos;

	CHECK_SESSION_HANDLE(session);

	if(baddr < 0)
		baddr = lib3270_get_cursor_address(session);

	if(baddr > (int) lib3270_get_length(session)) {
		return errno = EINVAL;
	}

	if(!lib3270_is_connected(session))
		return errno = ENOTCONN;

	if(start)
	{
		for(pos = baddr; pos > 0 && !isspace(session->text[pos].chr);pos--);

		*start = pos > 0 ? pos+1 : 0;
	}

	if(end)
	{
		int maxlen = session->view.rows * session->view.cols;
		for(pos = baddr; pos < maxlen && !isspace(session->text[pos].chr);pos++);

		*end = pos < maxlen ? pos-1 : maxlen;
	}

	return 0;
}


