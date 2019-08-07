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
 * Este programa está nomeado como toggles.c e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 *
 */


/**
 *	@file toggles.c
 *	@brief This module handles toggles.
 */

#include <errno.h>
#include <sys/types.h>

#ifdef WIN32
	#include <winsock2.h>
	#include <windows.h>
	#include <ws2tcpip.h>
#else
	#include <sys/socket.h>
#endif // !WIN32

#include <config.h>
#include <lib3270/toggle.h>
#include <lib3270-internals.h>

#include "ansic.h"
#include "ctlrc.h"
#include "popupsc.h"
#include "screenc.h"
#include "trace_dsc.h"
#include "togglesc.h"
#include "utilc.h"
#include <lib3270/log.h>

static const struct _toggle_info
{
	const char * name;				///< @brief Toggle name.
	const char   def;				///< @brief Default value.
	const char * label;				///< @brief Button label.
	const char * summary;			///< @brief Short description.
	const char * description;		///< @brief Toggle description.
}
toggle_info[LIB3270_TOGGLE_COUNT] =
{
		{
			"monocase",
			False,
			N_( "Monocase" ),
			N_( "Uppercase only" ),
			N_( "If set, the terminal operates in uppercase-only mode" )
		},
		{
			"cursorblink",
			True,
			N_( "Blinking Cursor" ),
			N_( "Blinking Cursor" ),
			N_( "If set, the cursor blinks" )
		},
		{
			"showtiming",
			True,
			N_( "Show timer when processing" ),
			N_( "Show timer when processing" ),
			N_( "If set, the time taken by the host to process an AID is displayed on the status line" )
		},
		{
			"cursorpos",
			True,
			N_( "Track Cursor" ),
			N_( "Track Cursor" ),
			N_( "Display the cursor location in the OIA (the status line)" )
		},
		{
			"dstrace",
			False,
			N_( "Data Stream" ),
			N_( "Trace Data Stream" ),
			""
		},
		{
			"linewrap",
			False,
			N_("Wrap around"),
			N_("Wrap around"),
			N_("If set, the NVT terminal emulator automatically assumes a NEWLINE character when it reaches the end of a line.")
		},
		{
			"blankfill",
			False,
			N_( "Blank Fill" ),
			N_( "Blank Fill" ),
			N_( "Automatically convert trailing blanks in a field to NULLs in order to insert a character, and will automatically convert leading NULLs to blanks so that input data is not squeezed to the left" )
		},
		{
			"screentrace",
			False,
			N_( "Screens" ),
			N_( "Trace screen contents" ),
			""
		},
		{
			"eventtrace",
			False,
			N_( "Interface" ),
			N_( "Trace interface events" ),
			""
		},
		{
			"marginedpaste",
			False,
			N_( "Paste with left margin" ),
			N_( "Paste with left margin" ),
			N_( "If set, puts restrictions on how pasted text is placed on the screen. The position of the cursor at the time the paste operation is begun is used as a left margin. No pasted text will fill any area of the screen to the left of that position. This option is useful for pasting into certain IBM editors that use the left side of the screen for control information" )
		},
		{
			"rectselect",
			False,
			N_( "Select by rectangles" ),
			N_( "Select by rectangles" ),
			N_( "If set, the terminal will always select rectangular areas of the screen. Otherwise, it selects continuous regions of the screen" )
		},
		{
			"crosshair",
			False,
			N_( "Cross hair cursor" ),
			N_( "Cross hair cursor" ),
			N_( "If set, the terminal will display a crosshair over the cursor: lines extending the full width and height of the screen, centered over the cursor position. This makes locating the cursor on the screen much easier" )
		},
		{
			"fullscreen",
			False,
			N_( "Full Screen" ),
			N_( "Full Screen" ),
			N_( "If set, asks to place the toplevel window in the fullscreen state" )
		},
		{
			"reconnect",
			False,
			N_( "Auto-Reconnect" ),
			N_( "Auto-Reconnect" ),
			N_( "Automatically reconnect to the host if it ever disconnects" )
		},
		{
			"insert",
			False,
			N_( "Insert" ),
			N_( "Set insert mode" ),
			""
		},
		{
			"smartpaste",
			False,
			N_( "Smart paste" ),
			N_( "Smart paste" ),
			""
		},
		{
			"bold",
			False,
			N_( "Bold" ),
			N_( "Bold" ),
			""
		},
		{
			"keepselected",
			False,
			N_( "Keep selected" ),
			N_( "Keep selected" ),
			""
		},
		{
			"underline",
			False,
			N_( "Underline" ),
			N_( "Show Underline" ),
			""
		},
		{
			"autoconnect",
			False,
			N_( "Auto connect" ),
			N_( "Connect on startup" ),
			""
		},
		{
			"kpalternative",
			False,
			N_( "Use +/- for field navigation" ),
			N_( "Use +/- for field navigation" ),
			N_( "Use the keys +/- from keypad to select editable fields" )
		},
		{
			"beep",
			True,
			N_( "Sound" ),
			N_( "Alert sound" ),
			N_( "Beep on errors" )
		},
		{
			"fieldattr",
			False,
			N_( "Show Field" ),
			N_( "Show Field attribute" ),
			""
		},
		{
			"altscreen",
			True,
			N_( "Alternate screen" ),
			N_( "Resize on alternate screen" ),
			N_( "Auto resize on altscreen" )
		},
		{
			"keepalive",
			True,
			N_( "Network keep alive" ),
			N_( "Network keep alive" ),
			N_( "Enable network keep-alive with SO_KEEPALIVE" )
		},
		{
			"nettrace",
			False,
			N_( "Network data" ),
			N_( "Trace network data flow" ),
			N_( "Enable network in/out trace" )
		},
		{
			"ssltrace",
			False,
			N_( "SSL negotiation" ),
			N_( "Trace SSL negotiation" ),
			N_( "Enable security negotiation messages trace" )
		},
};

LIB3270_EXPORT unsigned char lib3270_get_toggle(H3270 *session, LIB3270_TOGGLE ix)
{
	CHECK_SESSION_HANDLE(session);

	if(ix < 0 || ix >= LIB3270_TOGGLE_COUNT)
		return 0;

	return session->toggle[ix].value != 0;
}

/**
 * @brief Call the internal update routine and listeners.
 */
static void toggle_notify(H3270 *session, struct lib3270_toggle *t, LIB3270_TOGGLE ix)
{
	struct lib3270_toggle_callback * st;

	trace("%s: ix=%d upcall=%p",__FUNCTION__,ix,t->upcall);
	t->upcall(session, t, LIB3270_TOGGLE_TYPE_INTERACTIVE);

	if(session->cbk.update_toggle)
		session->cbk.update_toggle(session,ix,t->value,LIB3270_TOGGLE_TYPE_INTERACTIVE,toggle_info[ix].name);

	for(st = session->listeners.toggle.callbacks[ix]; st != (struct lib3270_toggle_callback *) NULL; st = (struct lib3270_toggle_callback *) st->next)
	{
		st->func(session, ix, t->value, st->data);
	}

}

/**
 * @brief Set toggle state.
 *
 * @param h		Session handle.
 * @param ix	Toggle id.
 * @param value	New toggle state (non zero for true).
 *
 * @returns 0 if the toggle is already at the state, 1 if the toggle was changed; < 0 on error (sets errno).
 */
LIB3270_EXPORT int lib3270_set_toggle(H3270 *session, LIB3270_TOGGLE ix, int value)
{
	char v = value ? True : False;
	struct lib3270_toggle * t;

	CHECK_SESSION_HANDLE(session);

	if(ix < 0 || ix >= LIB3270_TOGGLE_COUNT)
		return -(errno = EINVAL);

	t = &session->toggle[ix];

	if(v == t->value)
		return 0;

	t->value = v;

	toggle_notify(session,t,ix);
	return 1;
}

LIB3270_EXPORT int lib3270_toggle(H3270 *session, LIB3270_TOGGLE ix)
{
	struct lib3270_toggle	*t;

	CHECK_SESSION_HANDLE(session);

	if(ix < 0 || ix >= LIB3270_TOGGLE_COUNT)
		return 0;

	t = &session->toggle[ix];

	t->value = t->value ? False : True;
	toggle_notify(session,t,ix);

	return (int) t->value;
}

static void toggle_altscreen(H3270 *session, struct lib3270_toggle *t, LIB3270_TOGGLE_TYPE GNUC_UNUSED(tt))
{
	if(!session->screen_alt)
		set_viewsize(session,t->value ? 24 : session->maxROWS,80);
}

static void toggle_redraw(H3270 *session, struct lib3270_toggle GNUC_UNUSED(*t), LIB3270_TOGGLE_TYPE GNUC_UNUSED(tt))
{
	session->cbk.display(session);
}

/**
 * @brief No-op toggle.
 */
static void toggle_nop(H3270 GNUC_UNUSED(*session), struct lib3270_toggle GNUC_UNUSED(*t), LIB3270_TOGGLE_TYPE GNUC_UNUSED(tt))
{
}

static void toggle_keepalive(H3270 *session, struct lib3270_toggle GNUC_UNUSED(*t), LIB3270_TOGGLE_TYPE GNUC_UNUSED(tt))
{
	if(session->sock > 0)
	{
		// Update keep-alive option
		int optval = t->value ? 1 : 0;

		if (setsockopt(session->sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(optval)) < 0)
		{
			popup_a_sockerr(session, N_( "Can't %s network keep-alive" ), optval ? _( "enable" ) : _( "disable" ));
		}
		else
		{
			trace_dsn(session,"Network keep-alive is %s\n",optval ? "enabled" : "disabled" );
		}

	}
}

/**
 * @brief Called from system initialization code to handle initial toggle settings.
 */
void initialize_toggles(H3270 *session)
{
	int f;

	for(f=0;f<LIB3270_TOGGLE_COUNT;f++)
		session->toggle[f].upcall	= toggle_nop;

	session->toggle[LIB3270_TOGGLE_RECTANGLE_SELECT].upcall	= toggle_rectselect;
	session->toggle[LIB3270_TOGGLE_MONOCASE].upcall 		= toggle_redraw;
	session->toggle[LIB3270_TOGGLE_UNDERLINE].upcall 		= toggle_redraw;
	session->toggle[LIB3270_TOGGLE_ALTSCREEN].upcall 		= toggle_altscreen;
	session->toggle[LIB3270_TOGGLE_KEEP_ALIVE].upcall		= toggle_keepalive;

	for(f=0;f<LIB3270_TOGGLE_COUNT;f++)
	{
		session->toggle[f].value = toggle_info[f].def;
		if(session->toggle[f].value)
			session->toggle[f].upcall(session,&session->toggle[f],LIB3270_TOGGLE_TYPE_INITIAL);
	}

}

/**
 * @brief Called from system exit code to handle toggles.
 */
void shutdown_toggles(H3270 *session)
{
#if defined(X3270_TRACE)
	static const LIB3270_TOGGLE disable_on_shutdown[] = {LIB3270_TOGGLE_DS_TRACE, LIB3270_TOGGLE_EVENT_TRACE, LIB3270_TOGGLE_SCREEN_TRACE};

	size_t f;

	for(f=0;f< (sizeof(disable_on_shutdown)/sizeof(disable_on_shutdown[0])); f++)
		lib3270_set_toggle(session,disable_on_shutdown[f],0);

#endif
}

LIB3270_EXPORT const char * lib3270_get_toggle_summary(LIB3270_TOGGLE ix)
{
	if(ix < LIB3270_TOGGLE_COUNT)
		return toggle_info[ix].summary;
	return "";
}

LIB3270_EXPORT const char * lib3270_get_toggle_label(LIB3270_TOGGLE ix)
{
	if(ix < LIB3270_TOGGLE_COUNT)
		return toggle_info[ix].label;
	return "";
}


LIB3270_EXPORT const char * lib3270_get_toggle_description(LIB3270_TOGGLE ix)
{
	if(ix < LIB3270_TOGGLE_COUNT)
		return toggle_info[ix].description;
	return "";
}

LIB3270_EXPORT const char * lib3270_get_toggle_name(LIB3270_TOGGLE ix)
{
	if(ix < LIB3270_TOGGLE_COUNT)
		return toggle_info[ix].name;
	return "";
}

LIB3270_EXPORT LIB3270_TOGGLE lib3270_get_toggle_id(const char *name)
{
	if(name)
	{
		int f;
		for(f=0;f<LIB3270_TOGGLE_COUNT;f++)
		{
			if(!strcasecmp(name,toggle_info[f].name))
				return f;
		}
	}
	return -1;
}

LIB3270_EXPORT const void * lib3270_register_toggle_listener(H3270 *hSession, LIB3270_TOGGLE tx, void (*func)(H3270 *, LIB3270_TOGGLE, char, void *),void *data)
{
	struct lib3270_toggle_callback *st;

    CHECK_SESSION_HANDLE(hSession);

	st 			= (struct lib3270_toggle_callback *) lib3270_malloc(sizeof(struct lib3270_toggle_callback));
	st->func	= func;
	st->data	= data;

	if (hSession->listeners.toggle.last[tx])
		hSession->listeners.toggle.last[tx]->next = st;
	else
		hSession->listeners.toggle.callbacks[tx] = st;

	hSession->listeners.toggle.last[tx] = st;

	return (void *) st;

}

LIB3270_EXPORT int lib3270_unregister_toggle_listener(H3270 *hSession, LIB3270_TOGGLE tx, const void *id)
{
	struct lib3270_toggle_callback *st;
	struct lib3270_toggle_callback *prev = (struct lib3270_toggle_callback *) NULL;

	for (st = hSession->listeners.toggle.callbacks[tx]; st != (struct lib3270_toggle_callback *) NULL; st = (struct lib3270_toggle_callback *) st->next)
	{
		if (st == (struct lib3270_toggle_callback *)id)
			break;

		prev = st;
	}

	if (st == (struct lib3270_toggle_callback *)NULL)
	{
		lib3270_write_log(hSession,"lib3270","Invalid call to (%s): %p wasnt found in the list",__FUNCTION__,id);
		return errno = ENOENT;
	}

	if (prev != (struct lib3270_toggle_callback *) NULL)
		prev->next = st->next;
	else
		hSession->listeners.toggle.callbacks[tx] = (struct lib3270_toggle_callback *) st->next;

	for(st = hSession->listeners.toggle.callbacks[tx]; st != (struct lib3270_toggle_callback *) NULL; st = (struct lib3270_toggle_callback *) st->next)
		hSession->listeners.toggle.last[tx] = st;

	lib3270_free((void *) id);

	return 0;

}