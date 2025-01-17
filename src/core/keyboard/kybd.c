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
 * Este programa está nomeado como kybd.c e possui - linhas de código.
 *
 * Contatos:
 *
 * perry.werneck@gmail.com	(Alexandre Perry de Souza Werneck)
 * erico.mendonca@gmail.com	(Erico Mascarenhas Mendonça)
 *
 */

/**
 *	@brief This module handles the keyboard for the 3270 emulator.
 */

#pragma GCC diagnostic ignored "-Wsign-compare"


struct ta;

#define LIB3270_TA struct ta

#include <internals.h>
#include <lib3270/trace.h>
#include <lib3270/selection.h>
#include <lib3270/log.h>
#include <lib3270/toggle.h>

#ifndef ANDROID
	#include <stdlib.h>
#endif // !ANDROID

#if defined(X3270_DISPLAY) /*[*/
#include <X11/Xatom.h>
#endif
#define XK_3270
#if defined(X3270_APL) /*[*/
#define XK_APL
#endif /*]*/

#include <fcntl.h>
#include "3270ds.h"
//#include "resources.h"

#include "ansic.h"
#include "ctlrc.h"
#include "ftc.h"
#include "hostc.h"
#include "kybdc.h"
#include "popupsc.h"
#include "screenc.h"
#include "screen.h"
#include "statusc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
//#include "utf8c.h"
#include "utilc.h"
#if defined(X3270_DBCS) /*[*/
#include "widec.h"
#endif /*]*/

#include <lib3270/actions.h>

#ifdef X3270_TRACE
static const char *ia_name[] =
{
	"String", "Paste", "Screen redraw", "Keypad", "Default", "Key",
	"Macro", "Script", "Peek", "Typeahead", "File transfer", "Command",
	"Keymap", "Idle"
};
#endif // X3270_TRACE

static const unsigned char pf_xlate[] =
{
	AID_PF1,  AID_PF2,  AID_PF3,  AID_PF4,  AID_PF5,  AID_PF6,
	AID_PF7,  AID_PF8,  AID_PF9,  AID_PF10, AID_PF11, AID_PF12,
	AID_PF13, AID_PF14, AID_PF15, AID_PF16, AID_PF17, AID_PF18,
	AID_PF19, AID_PF20, AID_PF21, AID_PF22, AID_PF23, AID_PF24
};
static const unsigned char pa_xlate[] =
{
	AID_PA1, AID_PA2, AID_PA3
};
#define PF_SZ			(sizeof(pf_xlate)/sizeof(pf_xlate[0]))
#define PA_SZ			(sizeof(pa_xlate)/sizeof(pa_xlate[0]))

static		Boolean key_Character(H3270 *hSession, int code, Boolean with_ge, Boolean pasting,Boolean *skipped);
static int	flush_ta(H3270 *hSession);
static void	key_AID(H3270 *session, unsigned char aid_code);
static void	kybdlock_set(H3270 *session, unsigned int bits);

/* Composite key mappings. */

/*
struct akeysym
{
	KeySym keysym;
	enum keytype keytype;
};
*/

#define ak_eq(k1, k2)	(((k1).keysym  == (k2).keysym) && \
			 ((k1).keytype == (k2).keytype))


static const char dxl[] = "0123456789abcdef";
#define FROM_HEX(c)	(strchr(dxl, tolower(c)) - dxl)

/**
 * @brief Create a new typeahead action.
 *
 * Check for typeahead availability and create a new TA structure.
 *
 * @return new typeahead struct or NULL if it's not available.
 */
struct ta * new_ta(H3270 *hSession, enum _ta_type type)
{
	struct ta *ta;

	// If no connection, forget it.
	if (!lib3270_is_connected(hSession))
	{
		lib3270_ring_bell(hSession);
		lib3270_write_event_trace(hSession,"typeahead action dropped (not connected)\n");
		return NULL;
	}

	// If operator error, complain and drop it.
	if (hSession->kybdlock & KL_OERR_MASK)
	{
		lib3270_ring_bell(hSession);
		lib3270_write_event_trace(hSession,"typeahead action dropped (operator error)\n");
		return NULL;
	}

	// If scroll lock, complain and drop it.
	if (hSession->kybdlock & KL_SCROLLED)
	{
		lib3270_ring_bell(hSession);
		lib3270_write_event_trace(hSession,"typeahead action dropped (scrolled)\n");
		return NULL;
	}

	// If typeahead disabled, complain and drop it.
	if (!hSession->typeahead)
	{
		lib3270_ring_bell(hSession);
		lib3270_write_event_trace(hSession,"typeahead action dropped (no typeahead)\n");
		return NULL;
	}

	ta = (struct ta *) lib3270_malloc(sizeof(*ta));
	ta->next = (struct ta *) NULL;
	ta->type = type;

	if(hSession->ta_head)
	{
		hSession->ta_tail->next = ta;
	}
	else
	{
		hSession->ta_head = ta;
		status_typeahead(hSession,True);
	}

	hSession->ta_tail = ta;

	return ta;
}


/**
 * @brief Put a "Key-aid" on the typeahead queue
 *
 * @param hSession	TN3270 Session handle.
 * @param aid_code	Key-ad code to put on typeahead.
 */
 static void enq_key(H3270 *hSession, unsigned char aid_code)
 {
	struct ta *ta = new_ta(hSession, TA_TYPE_KEY_AID);

 	if(!ta)
		return;

	ta->args.aid_code = aid_code;

	lib3270_write_event_trace(hSession,"typeahead action Key-aid queued (kybdlock 0x%x)\n", hSession->kybdlock);
 }


/**
 * @brief Put an action on the typeahead queue.
 */
void enq_ta(H3270 *hSession, void (*fn)(H3270 *, const char *, const char *), const char *parm1, const char *parm2)
{
	struct ta *ta = new_ta(hSession, TA_TYPE_DEFAULT);

 	if(!ta)
		return;

	ta->args.def.fn	= fn;

	if (parm1)
		ta->args.def.parm[0] = NewString(parm1);

	if (parm2)
		ta->args.def.parm[1] = NewString(parm2);


	lib3270_write_event_trace(hSession,"typeahead action queued (kybdlock 0x%x)\n", hSession->kybdlock);
}

void enq_action(H3270 *hSession, int (*fn)(H3270 *))
{
	struct ta *ta = new_ta(hSession, TA_TYPE_ACTION);

	if(!ta)
		return;

	ta->args.action		= fn;
	lib3270_write_event_trace(hSession,"single action queued (kybdlock 0x%x)\n", hSession->kybdlock);

}

/**
 * @brief Execute an action from the typeahead queue.
 */
int run_ta(H3270 *hSession)
{
	struct ta *ta;

	if (hSession->kybdlock || (ta = hSession->ta_head) == (struct ta *)NULL)
		return 0;

	if ((hSession->ta_head = ta->next) == (struct ta *)NULL)
	{
		hSession->ta_tail = (struct ta *)NULL;
		status_typeahead(hSession,False);
	}

	switch(ta->type)
	{
	case TA_TYPE_DEFAULT:
		ta->args.def.fn(hSession,ta->args.def.parm[0],ta->args.def.parm[1]);
		lib3270_free(ta->args.def.parm[0]);
		lib3270_free(ta->args.def.parm[1]);
		break;

	case TA_TYPE_CURSOR_MOVE:
		ta->args.move.fn(hSession,ta->args.move.direction,ta->args.move.sel);
		break;

	case TA_TYPE_ACTION:
		ta->args.action(hSession);
		break;

	case TA_TYPE_KEY_AID:
		key_AID(hSession,ta->args.aid_code);
		break;

	default:
		popup_an_error(hSession, _( "Unexpected type %d in typeahead queue" ), ta->type);

	}

	lib3270_free(ta);

	return 1;
}

/**
 * @brief Flush the typeahead queue.
 * @return whether or not anything was flushed.
 */
static int flush_ta(H3270 *hSession)
{
	struct ta *ta, *next;
	int any = 0;

	for (ta = hSession->ta_head; ta != (struct ta *) NULL; ta = next)
	{
		if(ta->type == TA_TYPE_DEFAULT)
		{
			lib3270_free(ta->args.def.parm[0]);
			lib3270_free(ta->args.def.parm[1]);
		}
		next = ta->next;
		lib3270_free(ta);
		any++;
	}
	hSession->ta_head = hSession->ta_tail = (struct ta *) NULL;
	status_typeahead(hSession,False);
	return any;
}

/**
 * @brief Set bits in the keyboard lock.
 */
static void kybdlock_set(H3270 *hSession, unsigned int bits)
{
	unsigned int n;

	n = hSession->kybdlock | bits;
	if (n != hSession->kybdlock)
	{
#if defined(KYBDLOCK_TRACE)
		lib3270_write_event_trace(hSession,"  %s: kybdlock |= 0x%04x, 0x%04x -> 0x%04x\n", "set", bits, hSession->kybdlock, n);
#endif
		if ((hSession->kybdlock ^ bits) & KL_DEFERRED_UNLOCK)
		{
			// Turned on deferred unlock.
			hSession->unlock_delay_time = time(NULL);
		}
		hSession->kybdlock = n;
		status_changed(hSession,LIB3270_MESSAGE_KYBDLOCK);
	}
}

/**
 * @brief Clear bits in the keyboard lock.
 *
 */
void lib3270_kybdlock_clear(H3270 *hSession, LIB3270_KEYBOARD_LOCK_STATE bits)
{
	unsigned int n = hSession->kybdlock & ~( (unsigned int) bits);

//	trace("%s: kybdlock=%d",__FUNCTION__,n);

	if (n != hSession->kybdlock)
	{
#if defined(KYBDLOCK_TRACE)
		lib3270_write_event_trace(hSession,"  %s: kybdlock &= ~0x%04x, 0x%04x -> 0x%04x\n", "clear", bits, hSession->kybdlock, n);
#endif
		if ((hSession->kybdlock ^ n) & KL_DEFERRED_UNLOCK)
		{
			/* Turned off deferred unlock. */
			hSession->unlock_delay_time = 0;
		}
		hSession->kybdlock = n;
		status_changed(hSession,LIB3270_MESSAGE_KYBDLOCK);
	}
}

/**
 * @brief Set or clear enter-inhibit mode.
 *
 * @param session	Session handle
 * @param inhibit	New state
 *
 */
void kybd_inhibit(H3270 *session, Boolean inhibit)
{
	if (inhibit)
	{
		kybdlock_set(session,KL_ENTER_INHIBIT);
		if (session->kybdlock == KL_ENTER_INHIBIT)
			status_reset(session);
	}
	else
	{
		lib3270_kybdlock_clear(session,KL_ENTER_INHIBIT);
		if (!session->kybdlock)
			status_reset(session);
	}
}

/**
 * @brief Called when a host connects or disconnects.
 */
void kybd_connect(H3270 *session, int connected, void GNUC_UNUSED(*dunno))
{
	if (session->kybdlock & KL_DEFERRED_UNLOCK)
		RemoveTimer(session, session->unlock_id);

	lib3270_kybdlock_clear(session, -1);

	if (connected)
	{
		/* Wait for any output or a WCC(restore) from the host */
		kybdlock_set(session,KL_AWAITING_FIRST);
	}
	else
	{
		kybdlock_set(session,KL_NOT_CONNECTED);
		(void) flush_ta(session);
	}
}

/**
 * @brief Called when we switch between 3270 and ANSI modes.
 */
void kybd_in3270(H3270 *hSession, int GNUC_UNUSED(in3270), void GNUC_UNUSED(*dunno))
{
	if (hSession->kybdlock & KL_DEFERRED_UNLOCK)
		RemoveTimer(hSession, hSession->unlock_id);

	lib3270_kybdlock_clear(hSession,~KL_AWAITING_FIRST);

	/* There might be a macro pending. */
	if (CONNECTED)
		ps_process(hSession);
}

/**
 * @brief Lock the keyboard because of an operator error.
 *
 * @see lib3270_set_lock_on_operator_error
 */
void operator_error(H3270 *hSession, int error_type)
{
	if(hSession->oerr_lock)
	{
		status_oerr(hSession,error_type);
		mcursor_set(hSession,LIB3270_POINTER_LOCKED);
		kybdlock_set(hSession,(unsigned int)error_type);
		flush_ta(hSession);
	}
	else
	{
		lib3270_ring_bell(hSession);
	}
}

/**
 * @brief Handle an AID (Attention IDentifier) key.
 *
 * This is the common stuff that gets executed for all AID keys (PFs, PAs, Clear and etc).
 *
 */
static void key_AID(H3270 *hSession, unsigned char aid_code)
{
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		register unsigned i;

		trace("aid_code: %02x IN_ANSI: %d",aid_code,IN_ANSI);

		if (aid_code == AID_ENTER) {
			net_sendc(hSession, '\r');
			return;
		}
		for (i = 0; i < PF_SZ; i++)
			if (aid_code == pf_xlate[i]) {
				ansi_send_pf(hSession,i+1);
				return;
			}
		for (i = 0; i < PA_SZ; i++)
			if (aid_code == pa_xlate[i]) {
				ansi_send_pa(hSession,i+1);
				return;
			}
		return;
	}
#endif /*]*/

#if defined(X3270_PLUGIN) /*[*/
	plugin_aid(aid_code);
#endif /*]*/

	trace("IN_SSCP: %d cursor_addr: %d",IN_SSCP,hSession->cursor_addr);

	if (IN_SSCP)
	{
		if (hSession->kybdlock & KL_OIA_MINUS)
			return;

		if (aid_code != AID_ENTER && aid_code != AID_CLEAR)
		{
			status_changed(hSession,LIB3270_MESSAGE_MINUS);
			kybdlock_set(hSession,KL_OIA_MINUS);
			return;
		}
	}

	if (IN_SSCP && aid_code == AID_ENTER)
	{
		/* Act as if the host had written our input. */
		hSession->buffer_addr = hSession->cursor_addr;
	}

	if (!IN_SSCP || aid_code != AID_CLEAR)
	{
		status_twait(hSession);
		mcursor_set(hSession,LIB3270_POINTER_WAITING);
		lib3270_set_toggle(hSession,LIB3270_TOGGLE_INSERT,0);
		kybdlock_set(hSession,KL_OIA_TWAIT | KL_OIA_LOCKED);
	}

	hSession->aid = aid_code;
	ctlr_read_modified(hSession, hSession->aid, False);
	hSession->cbk.set_timer(hSession,1);
	status_ctlr_done(hSession);
}

LIB3270_EXPORT int lib3270_pfkey(H3270 *hSession, int key)
{
	FAIL_IF_NOT_ONLINE(hSession)

	if (key < 1 || key > PF_SZ)
		return errno = EINVAL;

	if (hSession->kybdlock & KL_OIA_MINUS)
		return errno = EPERM;

	if (hSession->kybdlock)
	{
		if(hSession->host_type & LIB3270_HOST_AS400)
			enq_key(hSession,pa_xlate[0]);

 		enq_key(hSession,pf_xlate[key-1]);
	}
	else
	{
		if(hSession->host_type & LIB3270_HOST_AS400)
			key_AID(hSession,pa_xlate[0]);

		key_AID(hSession,pf_xlate[key-1]);
	}

	return 0;
}

LIB3270_EXPORT int lib3270_pakey(H3270 *hSession, int key)
{
	FAIL_IF_NOT_ONLINE(hSession)

	if (key < 1 || key > PA_SZ)
	{
		return errno = EINVAL;
	}

	if (hSession->kybdlock & KL_OIA_MINUS)
		return errno = EPERM;
	else if (hSession->kybdlock)
		enq_key(hSession,pa_xlate[key-1]);
	else
		key_AID(hSession,pa_xlate[key-1]);

	return 0;
}

/**
 * @brief Prepare for an insert of 'count' bytes.
 *
 * @return True if the insert is legal, False otherwise.
 */
static Boolean ins_prep(H3270 *hSession, int faddr, int baddr, int count)
{
	int next_faddr;
	int xaddr;
	int need;
	int ntb;
	int tb_start = -1;
	int copy_len;

	/* Find the end of the field. */
	if (faddr == -1)
	{
		/* Unformatted.  Use the end of the line. */
		next_faddr = (((baddr / hSession->view.cols) + 1) * hSession->view.cols) % (hSession->view.rows*hSession->view.cols);
	}
	else
	{
		next_faddr = faddr;
		INC_BA(next_faddr);
		while (next_faddr != faddr && !hSession->ea_buf[next_faddr].fa)
		{
			INC_BA(next_faddr);
		}
	}

	/* Are there enough NULLs or trailing blanks available? */
	xaddr = baddr;
	need = count;
	ntb = 0;
	while (need && (xaddr != next_faddr)) {
		if (hSession->ea_buf[xaddr].cc == EBC_null)
			need--;
		else if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_BLANK_FILL) &&
			((hSession->ea_buf[xaddr].cc == EBC_space) ||
			 (hSession->ea_buf[xaddr].cc == EBC_underscore))) {
			if (tb_start == -1)
				tb_start = xaddr;
			ntb++;
		} else {
			tb_start = -1;
			ntb = 0;
		}
		INC_BA(xaddr);
	}
#if defined(_ST) /*[*/
	printf("need %d at %d, tb_start at %d\n", count, baddr, tb_start);
#endif /*]*/
	if (need - ntb > 0)
	{
		operator_error(hSession,KL_OERR_OVERFLOW);
		return False;
	}

	/*
	 * Shift the buffer to the right until we've consumed the available
	 * (and needed) NULLs.
	 */
	need = count;
	xaddr = baddr;
	while (need && (xaddr != next_faddr)) {
		int n_nulls = 0;
		int first_null = -1;

		while (need &&
		       ((hSession->ea_buf[xaddr].cc == EBC_null) ||
		        (tb_start >= 0 && xaddr >= tb_start))) {
			need--;
			n_nulls++;
			if (first_null == -1)
				first_null = xaddr;
			INC_BA(xaddr);
		}
		if (n_nulls) {
			int to;

			/* Shift right n_nulls worth. */
			copy_len = first_null - baddr;
			if (copy_len < 0)
				copy_len += hSession->view.rows * hSession->view.cols;
			to = (baddr + n_nulls) % (hSession->view.rows * hSession->view.cols);
/*
#if defined(_ST)
			printf("found %d NULLs at %d\n", n_nulls, first_null);
			printf("copying %d from %d to %d\n", copy_len, to,first_null);
#endif
*/
			if (copy_len)
				ctlr_wrapping_memmove(hSession,to, baddr, copy_len);
		}
		INC_BA(xaddr);
	}

	return True;

}

#define GE_WFLAG	0x100
#define PASTE_WFLAG	0x200

static void key_Character_wrapper(H3270 *hSession, const char *param1, const char GNUC_UNUSED(*param2))
{
	int code;
	Boolean with_ge = False;
	Boolean pasting = False;

	code = atoi(param1);

	if (code & GE_WFLAG)
	{
		with_ge = True;
		code &= ~GE_WFLAG;
	}

	if (code & PASTE_WFLAG)
	{
		pasting = True;
		code &= ~PASTE_WFLAG;
	}

	(void) key_Character(hSession, code, with_ge, pasting, NULL);
}

/**
 * @brief Handle an ordinary displayable character key.
 *
 * Lots of stuff to handle insert-mode, protected fields and etc.
 *
 */
static Boolean key_Character(H3270 *hSession, int code, Boolean with_ge, Boolean pasting, Boolean *skipped)
{
	register int	baddr, faddr, xaddr;
	register unsigned char	fa;
	enum dbcs_why why;

	if (skipped != NULL)
		*skipped = False;

	if (hSession->kybdlock)
	{
		char codename[64];

		(void) sprintf(codename, "%d", code |(with_ge ? GE_WFLAG : 0) | (pasting ? PASTE_WFLAG : 0));

		enq_ta(hSession,key_Character_wrapper, codename, CN);

		return False;
	}
	baddr = hSession->cursor_addr;
	faddr = lib3270_field_addr(hSession,baddr);
	fa = get_field_attribute(hSession,baddr);

	if (hSession->ea_buf[baddr].fa || FA_IS_PROTECTED(fa))
	{
		operator_error(hSession,KL_OERR_PROTECTED);
		return False;
	}

	if (hSession->numeric_lock && FA_IS_NUMERIC(fa) &&
	    !((code >= EBC_0 && code <= EBC_9) ||
	      code == EBC_minus || code == EBC_period)) {
		operator_error(hSession,KL_OERR_NUMERIC);
		return False;
	}

	/* Can't put an SBCS in a DBCS field. */
	if (hSession->ea_buf[faddr].cs == CS_DBCS) {
		operator_error(hSession,KL_OERR_DBCS);
		return False;
	}

	/* If it's an SI (end of DBCS subfield), move over one position. */
	if (hSession->ea_buf[baddr].cc == EBC_si) {
		INC_BA(baddr);
		if (baddr == faddr)
		{
			operator_error(hSession,KL_OERR_OVERFLOW);
			return False;
		}
	}

	/* Add the character. */
	if (hSession->ea_buf[baddr].cc == EBC_so) {

		if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_INSERT))
		{
			if (!ins_prep(hSession,faddr, baddr, 1))
				return False;
		}
		else
		{
			Boolean was_si = False;

			/*
			 * Overwriting an SO (start of DBCS subfield).
			 * If it's followed by an SI, replace the SO/SI
			 * pair with x/space.  If not, replace it and
			 * the following DBCS character with
			 * x/space/SO.
			 */
			xaddr = baddr;
			INC_BA(xaddr);
			was_si = (hSession->ea_buf[xaddr].cc == EBC_si);
			ctlr_add(hSession,xaddr, EBC_space, CS_BASE);
			ctlr_add_fg(hSession,xaddr, 0);
#if defined(X3270_ANSI) /*[*/
			ctlr_add_bg(hSession,xaddr, 0);
#endif /*]*/
			if (!was_si)
			{
				INC_BA(xaddr);
				ctlr_add(hSession,xaddr, EBC_so, CS_BASE);
				ctlr_add_fg(hSession,xaddr, 0);
#if defined(X3270_ANSI) /*[*/
				ctlr_add_bg(hSession,xaddr, 0);
#endif /*]*/
			}
		}

	} else switch (ctlr_lookleft_state(baddr, &why)) {
	case DBCS_RIGHT:
		DEC_BA(baddr);
		/* fall through... */
	case DBCS_LEFT:
		if (why == DBCS_ATTRIBUTE) {
			if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_INSERT)) {
				if (!ins_prep(hSession,faddr, baddr, 1))
					return False;
			} else {
				/*
				 * Replace single DBCS char with
				 * x/space.
				 */
				xaddr = baddr;
				INC_BA(xaddr);
				ctlr_add(hSession,xaddr, EBC_space, CS_BASE);
				ctlr_add_fg(hSession,xaddr, 0);
				ctlr_add_gr(hSession,xaddr, 0);
			}
		} else {
			Boolean was_si;

			if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_INSERT))
			{
				/*
				 * Inserting SBCS into a DBCS subfield.
				 * If this is the first position, we
				 * can just insert one character in
				 * front of the SO.  Otherwise, we'll
				 * need room for SI (to end subfield),
				 * the character, and SO (to begin the
				 * subfield again).
				 */
				xaddr = baddr;
				DEC_BA(xaddr);
				if (hSession->ea_buf[xaddr].cc == EBC_so) {
					DEC_BA(baddr);
					if (!ins_prep(hSession, faddr, baddr, 1))
						return False;
				} else {
					if (!ins_prep(hSession, faddr, baddr, 3))
						return False;
					xaddr = baddr;
					ctlr_add(hSession,xaddr, EBC_si,CS_BASE);
					ctlr_add_fg(hSession,xaddr, 0);
					ctlr_add_gr(hSession,xaddr, 0);
					INC_BA(xaddr);
					INC_BA(baddr);
					INC_BA(xaddr);
					ctlr_add(hSession,xaddr, EBC_so,CS_BASE);
					ctlr_add_fg(hSession,xaddr, 0);
					ctlr_add_gr(hSession,xaddr, 0);
				}
			} else {
				/* Overwriting part of a subfield. */
				xaddr = baddr;
				ctlr_add(hSession,xaddr, EBC_si, CS_BASE);
				ctlr_add_fg(hSession,xaddr, 0);
				ctlr_add_gr(hSession,xaddr, 0);
				INC_BA(xaddr);
				INC_BA(baddr);
				INC_BA(xaddr);
				was_si = (hSession->ea_buf[xaddr].cc == EBC_si);
				ctlr_add(hSession,xaddr, EBC_space, CS_BASE);
				ctlr_add_fg(hSession,xaddr, 0);
				ctlr_add_gr(hSession,xaddr, 0);
				if (!was_si)
				{
					INC_BA(xaddr);
					ctlr_add(hSession,xaddr, EBC_so,CS_BASE);
					ctlr_add_fg(hSession,xaddr, 0);
					ctlr_add_gr(hSession,xaddr, 0);
				}
			}
		}
		break;
	default:
	case DBCS_NONE:
		if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_INSERT) && !ins_prep(hSession, faddr, baddr, 1))
			return False;
		break;
	}
	ctlr_add(hSession,baddr, (unsigned char)code,(unsigned char)(with_ge ? CS_GE : 0));
	ctlr_add_fg(hSession,baddr, 0);
	ctlr_add_gr(hSession,baddr, 0);
	INC_BA(baddr);

	/* Replace leading nulls with blanks, if desired. */
	if (hSession->formatted && lib3270_get_toggle(hSession,LIB3270_TOGGLE_BLANK_FILL))
	{
		register int	baddr_fill = baddr;

		DEC_BA(baddr_fill);
		while (baddr_fill != faddr) {

			/* Check for backward line wrap. */
			if ((baddr_fill % hSession->view.cols) == hSession->view.cols - 1)
			{
				Boolean aborted = True;
				register int baddr_scan = baddr_fill;

				/*
				 * Check the field within the preceeding line
				 * for NULLs.
				 */
				while (baddr_scan != faddr) {
					if (hSession->ea_buf[baddr_scan].cc != EBC_null)
					{
						aborted = False;
						break;
					}
					if (!(baddr_scan % hSession->view.cols))
						break;
					DEC_BA(baddr_scan);
				}
				if (aborted)
					break;
			}

			if (hSession->ea_buf[baddr_fill].cc == EBC_null)
				ctlr_add(hSession,baddr_fill, EBC_space, 0);
			DEC_BA(baddr_fill);
		}
	}

	mdt_set(hSession,hSession->cursor_addr);

	/*
	 * Implement auto-skip, and don't land on attribute bytes.
	 * This happens for all pasted data (even DUP), and for all
	 * keyboard-generated data except DUP.
	 */
	if (pasting || (code != EBC_dup))
	{
		while (hSession->ea_buf[baddr].fa)
		{
			if (skipped != NULL)
				*skipped = True;
			if (FA_IS_SKIP(hSession->ea_buf[baddr].fa))
				baddr = lib3270_get_next_unprotected(hSession,baddr);
			else
				INC_BA(baddr);
		}
		cursor_move(hSession,baddr);
	}

	(void) ctlr_dbcs_postprocess(hSession);
	return True;
}

LIB3270_EXPORT int lib3270_input_string(H3270 *hSession, const unsigned char *str, int length)
{
	int rc = 0;

	if(check_online_session(hSession))
	{
		trace("%s: %s",__FUNCTION__,strerror(errno));
		return errno;
	}

	if(length < 0)
		length = strlen((char *) str);

	int pos;
	for(pos = 0; pos < length && str[pos] && !rc; pos++)
	{
		if (KYBDLOCK_IS_OERR(hSession))
			return (errno = EPERM);

		rc = key_ACharacter(hSession,(str[pos] & 0xff), KT_STD, IA_KEY, NULL);
		trace("%s: key_ACharacter(%c)=%d",__FUNCTION__,str[pos] & 0xff,rc);
	}

	screen_update(hSession,0,hSession->view.rows * hSession->view.cols);

	return rc;
}

/**
 * @brief Handle an ordinary character key, given an ASCII code.
 *
 */
int key_ACharacter(H3270 *hSession, unsigned char c, enum keytype keytype, enum iaction cause,Boolean *skipped)
{
	if (skipped != NULL)
		*skipped = False;

	lib3270_write_event_trace(hSession," %s -> Key(\"%s\") Hex(%02x)\n",ia_name[(int) cause], ctl_see((int) c), (int) c);

	if (IN_3270)
	{
		if (c < ' ')
		{
			lib3270_write_event_trace(hSession,"  dropped (control char)\n");
			return errno = EINVAL;
		}
		(void) key_Character(hSession, (int) hSession->charset.asc2ebc[c], keytype == KT_GE, False, skipped);
	}
#if defined(X3270_ANSI) /*[*/
	else if (IN_ANSI)
	{
		net_sendc(hSession,(char) c);
	}
#endif /*]*/
	else
	{
		lib3270_write_event_trace(hSession,"  dropped (not connected)\n");
		return errno = ENOTCONN;
	}
	return 0;
}

LIB3270_EXPORT int lib3270_clear_operator_error(H3270 *hSession)
{
	if(!hSession->kybdlock)
		return ENOENT;

	if(KYBDLOCK_IS_OERR(hSession))
	{
		lib3270_kybdlock_clear(hSession,KL_OERR_MASK);
		status_reset(hSession);
		return 0;
	}
	return EINVAL;
}

/**
 * @brief Deferred keyboard unlock.
 */
static int defer_unlock(H3270 *hSession, void GNUC_UNUSED(*userdata))
{
	lib3270_kybdlock_clear(hSession,KL_DEFERRED_UNLOCK);
	status_reset(hSession);
	if(CONNECTED)
		ps_process(hSession);
	return 0;
}

/**
 * @brief Reset keyboard lock.
 *
 * @param hSession	Session handle.
 * @param explicit	Explicit request from the keyboard.
 *
 */
void do_reset(H3270 *hSession, Boolean explicit)
{
	/*
	 * If explicit (from the keyboard) and there is typeahead or
	 * a half-composed key, simply flush it.
	 */

	if (explicit || lib3270_get_ft_state(hSession) != LIB3270_FT_STATE_NONE)
	{
		if (flush_ta(hSession))
			return;
	}

	/* Always clear insert mode. */
	lib3270_set_toggle(hSession,LIB3270_TOGGLE_INSERT,0);

	/* Otherwise, if not connect, reset is a no-op. */
	if (!CONNECTED)
		return;

	/*
	 * Remove any deferred keyboard unlock.  We will either unlock the
	 * keyboard now, or want to defer further into the future.
	 */
	if (hSession->kybdlock & KL_DEFERRED_UNLOCK)
		RemoveTimer(hSession, hSession->unlock_id);

	/*
	 * If explicit (from the keyboard), unlock the keyboard now.
	 * Otherwise (from the host), schedule a deferred keyboard unlock.
	 */
	if (explicit || lib3270_get_ft_state(hSession) != LIB3270_FT_STATE_NONE || (!hSession->unlock_delay) || (hSession->unlock_delay_time != 0 && (time(NULL) - hSession->unlock_delay_time) > 1))
	{
		lib3270_kybdlock_clear(hSession,-1);
	}
	else if (hSession->kybdlock & (KL_DEFERRED_UNLOCK | KL_OIA_TWAIT | KL_OIA_LOCKED | KL_AWAITING_FIRST))
	{
		lib3270_kybdlock_clear(hSession,~KL_DEFERRED_UNLOCK);
		kybdlock_set(hSession,KL_DEFERRED_UNLOCK);

		if(hSession->unlock_delay_ms)
		{
			hSession->unlock_id = AddTimer(hSession->unlock_delay_ms, hSession, defer_unlock, NULL);
		}
		else
		{
			hSession->unlock_id = 0;
			defer_unlock(hSession, NULL);
		}

	}

	/* Clean up other modes. */
	status_reset(hSession);
	mcursor_set(hSession,LIB3270_POINTER_UNLOCKED);

}

LIB3270_EXPORT int lib3270_kybdreset(H3270 *hSession)
{
	lib3270_unselect(hSession);
	do_reset(hSession,True);
	return 0;
}

/* Find the next unprotected word, or -1 */
static int nu_word(H3270 *hSession, int baddr)
{
	int baddr0 = baddr;
	unsigned char c;
	Boolean prot;

	prot = FA_IS_PROTECTED(get_field_attribute(hSession,baddr));

	do
	{
		c = hSession->ea_buf[baddr].cc;
		if (hSession->ea_buf[baddr].fa)
			prot = FA_IS_PROTECTED(hSession->ea_buf[baddr].fa);
		else if (!prot && c != EBC_space && c != EBC_null)
			return baddr;
		INC_BA(baddr);
	} while (baddr != baddr0);

	return -1;
}

/**
 * Find the next word in this field
 *
 * @return Next word or -1
 */
static int nt_word(H3270 *hSession, int baddr)
{
	int baddr0 = baddr;
	unsigned char c;
	Boolean in_word = True;

	do
	{
		c = hSession->ea_buf[baddr].cc;

		if (hSession->ea_buf[baddr].fa)
			return -1;

		if (in_word)
		{
			if (c == EBC_space || c == EBC_null)
				in_word = False;
		} else
		{
			if (c != EBC_space && c != EBC_null)
				return baddr;
		}
		INC_BA(baddr);
	} while (baddr != baddr0);

	return -1;
}


/**
 * @brief Cursor to next unprotected word.
 */
LIB3270_EXPORT int lib3270_nextword(H3270 *hSession)
{
	register int	baddr;
	unsigned char c;

	FAIL_IF_NOT_ONLINE(hSession);

	if (hSession->kybdlock) {
		enq_action(hSession, lib3270_nextword );
		return 0;
	}

#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return 0;
#endif /*]*/
	if (!hSession->formatted)
		return 0;

	/* If not in an unprotected field, go to the next unprotected word. */
	if (hSession->ea_buf[hSession->cursor_addr].fa ||
	    FA_IS_PROTECTED(get_field_attribute(hSession,hSession->cursor_addr))) {
		baddr = nu_word(hSession,hSession->cursor_addr);
		if (baddr != -1)
			cursor_move(hSession,baddr);
		return 0;
	}

	/* If there's another word in this field, go to it. */
	baddr = nt_word(hSession,hSession->cursor_addr);
	if (baddr != -1) {
		cursor_move(hSession,baddr);
		return 0;
	}

	/* If in a word, go to just after its end. */
	c = hSession->ea_buf[hSession->cursor_addr].cc;
	if (c != EBC_space && c != EBC_null) {
		baddr = hSession->cursor_addr;
		do {
			c = hSession->ea_buf[baddr].cc;
			if (c == EBC_space || c == EBC_null) {
				cursor_move(hSession,baddr);
				return 0;
			} else if (hSession->ea_buf[baddr].fa) {
				baddr = nu_word(hSession,baddr);
				if (baddr != -1)
					cursor_move(hSession,baddr);
				return 0;
			}
			INC_BA(baddr);
		} while (baddr != hSession->cursor_addr);
	}
	/* Otherwise, go to the next unprotected word. */
	else {
		baddr = nu_word(hSession,hSession->cursor_addr);
		if (baddr != -1)
			cursor_move(hSession,baddr);
	}

	return 0;
}

/**
 * @brief Cursor to first field on next line or any lines after that.
 */
LIB3270_EXPORT int lib3270_newline(H3270 *hSession)
{
	register int	baddr, faddr;
	register unsigned char	fa;

	FAIL_IF_NOT_ONLINE(hSession);

	if (hSession->kybdlock)
	{
		enq_action(hSession, lib3270_newline);
		return 0;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
	{
		net_sendc(hSession,'\n');
		return 0;
	}
#endif /*]*/
	baddr = (hSession->cursor_addr + hSession->view.cols) % (hSession->view.cols * hSession->view.rows);	/* down */
	baddr = (baddr / hSession->view.cols) * hSession->view.cols;			/* 1st col */
	faddr = lib3270_field_addr(hSession,baddr);
	fa = hSession->ea_buf[faddr].fa;
	if (faddr != baddr && !FA_IS_PROTECTED(fa))
		cursor_move(hSession,baddr);
	else
		cursor_move(hSession,lib3270_get_next_unprotected(hSession,baddr));

	return 0;
}

/**
 * @brief DUP key
 */
LIB3270_EXPORT int lib3270_dup(H3270 *hSession)
{
	FAIL_IF_NOT_ONLINE(hSession);

	if (hSession->kybdlock)
	{
		enq_action(hSession, lib3270_dup);
		return 0;
	}
#if defined(X3270_ANSI)
	if (IN_ANSI)
		return 0;
#endif
	if (key_Character(hSession, EBC_dup, False, False, NULL))
	{
		hSession->cbk.display(hSession);
		cursor_move(hSession,lib3270_get_next_unprotected(hSession,hSession->cursor_addr));
	}

	return 0;
}

/**
 * @brief FM key
 */
LIB3270_EXPORT int lib3270_fieldmark(H3270 *hSession)
{
	FAIL_IF_NOT_ONLINE(hSession);

	if (hSession->kybdlock)
	{
		enq_action(hSession, lib3270_fieldmark);
		return 0;
	}
#if defined(X3270_ANSI)
	if (IN_ANSI)
		return 0 ;
#endif
	(void) key_Character(hSession, EBC_fm, False, False, NULL);

	return 0;
}

/**
 * @brief Send an "Enter" action.
 *
 * Called when the user press the key enter.
 *
 */
LIB3270_EXPORT int lib3270_enter(H3270 *hSession)
{
	FAIL_IF_NOT_ONLINE(hSession);

	trace("%s (kybdlock & KL_OIA_MINUS): %d kybdlock: %d",__FUNCTION__,(hSession->kybdlock & KL_OIA_MINUS),hSession->kybdlock);

	if (hSession->kybdlock & KL_OIA_MINUS)
	{
		return errno = EPERM;
	}
	else if (hSession->kybdlock)
	{
		enq_action(hSession, lib3270_enter);
	}
	else
	{
		key_AID(hSession,AID_ENTER);
	}

	return 0;
}

LIB3270_EXPORT int lib3270_sysreq(H3270 *hSession)
{
	FAIL_IF_NOT_ONLINE(hSession);

	if (IN_ANSI)
	{
		return 0;
	}

#if defined(X3270_TN3270E) /*[*/
	if (IN_E)
	{
		net_abort(hSession);
	}
	else
#endif /*]*/
	{
		if (hSession->kybdlock & KL_OIA_MINUS)
			return 0;
		else if (hSession->kybdlock)
			enq_action(hSession, lib3270_sysreq);
		else
			key_AID(hSession,AID_SYSREQ);
	}

	return 0;
}

/**
 * @brief Clear AID key
 */
LIB3270_EXPORT int lib3270_clear(H3270 *hSession)
{
//	reset_idle_timer();
	if (hSession->kybdlock & KL_OIA_MINUS)
		return 0;
	if (hSession->kybdlock && CONNECTED) {
		enq_action(hSession, lib3270_clear);
		return 0;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		ansi_send_clear(hSession);
		return 0;
	}
#endif /*]*/
	hSession->buffer_addr = 0;
	ctlr_clear(hSession,True);
	cursor_move(hSession,0);
	if (CONNECTED)
		key_AID(hSession,AID_CLEAR);
	return 0;
}

/**
 * @brief Erase End Of Line Key.
 *
 */
LIB3270_EXPORT int lib3270_eraseeol(H3270 *hSession)
{
	register int	baddr;
	register unsigned char	fa;
	enum dbcs_state d;
	enum dbcs_why why = DBCS_FIELD;

//	reset_idle_timer();
	if (hSession->kybdlock)
	{
		enq_action(hSession, lib3270_eraseeol);
		return 0;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return 0;
#endif /*]*/

	baddr = hSession->cursor_addr;
	fa = get_field_attribute(hSession,baddr);
	if (FA_IS_PROTECTED(fa) || hSession->ea_buf[baddr].fa)
	{
		operator_error(hSession,KL_OERR_PROTECTED);
		return errno = EPERM;
	}

	if (hSession->formatted)
	{
		/* erase to next field attribute or current line */
		do
		{
			ctlr_add(hSession,baddr, EBC_null, 0);
			INC_BA(baddr);
		} while (!hSession->ea_buf[baddr].fa && BA_TO_COL(baddr) > 0);

		mdt_set(hSession,hSession->cursor_addr);
	}
	else
	{
		/* erase to end of current line */
		do
		{
			ctlr_add(hSession,baddr, EBC_null, 0);
			INC_BA(baddr);
		} while(baddr != 0 && BA_TO_COL(baddr) > 0);
	}

	/* If the cursor was in a DBCS subfield, re-create the SI. */
	d = ctlr_lookleft_state(cursor_addr, &why);
	if (IS_DBCS(d) && why == DBCS_SUBFIELD)
	{
		if (d == DBCS_RIGHT)
		{
			baddr = hSession->cursor_addr;
			DEC_BA(baddr);
			hSession->ea_buf[baddr].cc = EBC_si;
		} else
			hSession->ea_buf[hSession->cursor_addr].cc = EBC_si;
	}
	(void) ctlr_dbcs_postprocess(hSession);
	hSession->cbk.display(hSession);
	return 0;
}

/**
 * @brief Erase End Of Field Key.
 *
 */
LIB3270_EXPORT int lib3270_eraseeof(H3270 *hSession)
{
	register int	baddr;
	register unsigned char	fa;
	enum dbcs_state d;
	enum dbcs_why why = DBCS_FIELD;

//	reset_idle_timer();
	if (hSession->kybdlock)
	{
		enq_action(hSession, lib3270_eraseeof);
		return 0;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return 0;
#endif /*]*/
	baddr = hSession->cursor_addr;
	fa = get_field_attribute(hSession,baddr);
	if (FA_IS_PROTECTED(fa) || hSession->ea_buf[baddr].fa) {
		operator_error(hSession,KL_OERR_PROTECTED);
		return errno = EPERM;
	}
	if (hSession->formatted)
	{	/* erase to next field attribute */
		do
		{
			ctlr_add(hSession,baddr, EBC_null, 0);
			INC_BA(baddr);
		} while (!hSession->ea_buf[baddr].fa);
		mdt_set(hSession,hSession->cursor_addr);
	} else {	/* erase to end of screen */
		do {
			ctlr_add(hSession,baddr, EBC_null, 0);
			INC_BA(baddr);
		} while (baddr != 0);
	}

	/* If the cursor was in a DBCS subfield, re-create the SI. */
	d = ctlr_lookleft_state(cursor_addr, &why);
	if (IS_DBCS(d) && why == DBCS_SUBFIELD) {
		if (d == DBCS_RIGHT) {
			baddr = hSession->cursor_addr;
			DEC_BA(baddr);
			hSession->ea_buf[baddr].cc = EBC_si;
		} else
			hSession->ea_buf[hSession->cursor_addr].cc = EBC_si;
	}
	(void) ctlr_dbcs_postprocess(hSession);
	hSession->cbk.display(hSession);
	return 0;
}

LIB3270_EXPORT int lib3270_eraseinput(H3270 *hSession)
{
	register int	baddr, sbaddr;
	unsigned char	fa;
	Boolean		f;

//	reset_idle_timer();
	if (hSession->kybdlock) {
		enq_action(hSession,  lib3270_eraseinput );
		return 0;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
		return 0;
#endif /*]*/
	if (hSession->formatted)
	{
		/* find first field attribute */
		baddr = 0;
		do {
			if (hSession->ea_buf[baddr].fa)
				break;
			INC_BA(baddr);
		} while (baddr != 0);
		sbaddr = baddr;
		f = False;
		do {
			fa = hSession->ea_buf[baddr].fa;
			if (!FA_IS_PROTECTED(fa)) {
				mdt_clear(hSession,baddr);
				do {
					INC_BA(baddr);
					if (!f) {
						cursor_move(hSession,baddr);
						f = True;
					}
					if (!hSession->ea_buf[baddr].fa) {
						ctlr_add(hSession,baddr, EBC_null, 0);
					}
				} while (!hSession->ea_buf[baddr].fa);
			} else {	/* skip protected */
				do {
					INC_BA(baddr);
				} while (!hSession->ea_buf[baddr].fa);
			}
		} while (baddr != sbaddr);
		if (!f)
			cursor_move(hSession,0);
	} else {
		ctlr_clear(hSession,True);
		cursor_move(hSession,0);
	}
	hSession->cbk.display(hSession);
	return 0;
}

/**
 * @brief Delete field key.

 * Similar to EraseEOF, but it wipes out the entire field
 * rather than just to the right of the cursor, and it leaves the cursor at
 * the front of the field.
 *
 * Which is to say, does a ^U.
 */
LIB3270_EXPORT int lib3270_deletefield(H3270 *hSession)
{
	register int	baddr;
	register unsigned char	fa;

//	reset_idle_timer();
	if (hSession->kybdlock) {
		enq_action(hSession, lib3270_deletefield);
//		enq_ta(DeleteField_action, CN, CN);
		return 0;
	}
#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI) {
		net_send_kill(hSession);
		return 0;
	}
#endif /*]*/
	if (!hSession->formatted)
		return 0;

	baddr = hSession->cursor_addr;
	fa = get_field_attribute(hSession,baddr);
	if (FA_IS_PROTECTED(fa) || hSession->ea_buf[baddr].fa) {
		operator_error(hSession,KL_OERR_PROTECTED);
		return errno = EPERM;
	}
	while (!hSession->ea_buf[baddr].fa)
		DEC_BA(baddr);
	INC_BA(baddr);
	mdt_set(hSession,hSession->cursor_addr);
	cursor_move(hSession,baddr);
	while (!hSession->ea_buf[baddr].fa) {
		ctlr_add(hSession,baddr, EBC_null, 0);
		INC_BA(baddr);
	}
	hSession->cbk.display(hSession);
	return 0;
}


/**
 * @brief Move the cursor to the first blank after the last nonblank in the field.
 *
 * Move the cursor to the first blank after the last nonblank in the
 * field, or if the field is full, to the last character in the field.
 *
 */
LIB3270_EXPORT int lib3270_fieldend(H3270 *hSession)
{
	int baddr;

	if (hSession->kybdlock)
	{
		enq_action(hSession, lib3270_fieldend );
		return 0;
	}

	baddr = lib3270_get_field_end(hSession,hSession->cursor_addr);
	if(baddr >= 0)
		cursor_move(hSession,baddr);

	return 0;
}

int lib3270_get_field_end(H3270 *hSession, int baddr)
{
	int faddr;
	unsigned char	fa, c;
	int	last_nonblank = -1;

#if defined(X3270_ANSI) /*[*/
	if (IN_ANSI)
	{
		return -(errno = ENOTSUP);
	}
#endif /*]*/

	if (!hSession->formatted)
		return -(errno = ENOTSUP);

	faddr = lib3270_field_addr(hSession,baddr);
	fa = hSession->ea_buf[faddr].fa;
	if (faddr == baddr || FA_IS_PROTECTED(fa))
		return -(errno = EPERM);

	baddr = faddr;
	while (True)
	{
		INC_BA(baddr);
		c = hSession->ea_buf[baddr].cc;
		if (hSession->ea_buf[baddr].fa)
			break;
		if (c != EBC_null && c != EBC_space)
			last_nonblank = baddr;
	}

	if (last_nonblank == -1)
	{
		baddr = faddr;
		INC_BA(baddr);
	}
	else
	{
		baddr = last_nonblank;
		INC_BA(baddr);
		if (hSession->ea_buf[baddr].fa)
			baddr = last_nonblank;
	}

	return baddr;
}

/**
 * @brief PA key action for String actions.
 */
static void do_pa(H3270 *hSession, unsigned n)
{
	if (n < 1 || n > PA_SZ)
	{
		popup_an_error(hSession, _( "Unknown PA key %d" ), n);
		return;
	}

	lib3270_pakey(hSession,n);

}

/**
 * @brief PF key action for String actions.
 */
static void do_pf(H3270 *hSession, unsigned n)
{
	if (n < 1 || n > PF_SZ)
	{
		popup_an_error(hSession, _( "Unknown PF key %d" ), n);
		return;
	}

	lib3270_pfkey(hSession,n);
}

/*
 * Move the cursor back within the legal paste area.
 * Returns a Boolean indicating success.
 */
static Boolean remargin(H3270 *hSession, int lmargin)
{
	Boolean ever = False;
	int baddr, b0 = 0;
	int faddr;
	unsigned char fa;

	baddr = hSession->cursor_addr;
	while (BA_TO_COL(baddr) < lmargin)
	{
		baddr = ROWCOL_TO_BA(BA_TO_ROW(baddr), lmargin);
		if (!ever)
		{
			b0 = baddr;
			ever = True;
		}
		faddr = lib3270_field_addr(hSession,baddr);
		fa = hSession->ea_buf[faddr].fa;

		if (faddr == baddr || FA_IS_PROTECTED(fa))
		{
			baddr = lib3270_get_next_unprotected(hSession,baddr);
			if (baddr <= b0)
				return False;
		}
	}

	cursor_move(hSession,baddr);
	return True;
}

LIB3270_EXPORT int lib3270_emulate_input(H3270 *hSession, const char *s, int len, int pasting)
{
	enum { BASE, BACKSLASH, BACKX, BACKP, BACKPA, BACKPF, OCTAL, HEX, XGE } state = BASE;
	int literal = 0;
	int nc = 0;
	enum iaction ia = pasting ? IA_PASTE : IA_STRING;
	int orig_addr;
	int orig_col;
	Boolean skipped = False;

#if defined(X3270_DBCS) /*[*/
	unsigned char ebc[2];
	unsigned char cx;
	static UChar *w_ibuf = NULL;
	static size_t w_ibuf_len = 0;
	UChar c;
	UChar *ws;
#else /*][*/
	char c;
	const char *ws;
#endif /*]*/

	CHECK_SESSION_HANDLE(hSession);

	orig_addr = hSession->cursor_addr;
	orig_col  = BA_TO_COL(hSession->cursor_addr);

	if(len < 0)
		len = strlen(s);

	/*
	 * Convert from a multi-byte string to a Unicode string.
	 */
#if defined(X3270_DBCS) /*[*/
	if (len > w_ibuf_len)
	{
		w_ibuf_len = len;
		w_ibuf = (UChar *)Realloc(w_ibuf, w_ibuf_len * sizeof(UChar));
	}
	len = mb_to_unicode(s, len, w_ibuf, w_ibuf_len, NULL);
	if (len < 0)
	{
		return -1; /* failed */
	}
	ws = w_ibuf;
#else /*][*/
	ws = s;
#endif /*]*/

	/*
	 * In the switch statements below, "break" generally means "consume
	 * this character," while "continue" means "rescan this character."
	 */
	while (len)
	{

		/*
		 * It isn't possible to unlock the keyboard from a string,
		 * so if the keyboard is locked, it's fatal
		 */
		if (hSession->kybdlock)
		{
			lib3270_write_event_trace(hSession,"  keyboard locked, string dropped\n");
			return errno = EPERM;
		}

		if (pasting && IN_3270)
		{

			/* Check for cursor wrap to top of screen. */
			if (hSession->cursor_addr < orig_addr)
				return len-1;		/* wrapped */

			/* Jump cursor over left margin. */
			if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_MARGINED_PASTE) && BA_TO_COL(hSession->cursor_addr) < orig_col)
			{
				if (!remargin(hSession,orig_col))
					return len-1;
				skipped = True;
			}
		}

		c = *ws;

		switch (state)
		{
		    case BASE:
			switch (c)
			{
			case '\b':
			    lib3270_cursor_left(hSession);
				skipped = False;
				break;

			case '\f':
				if (pasting)
				{
					key_ACharacter(hSession,(unsigned char) ' ',KT_STD, ia, &skipped);
				} else
				{
					lib3270_clear(hSession);
					skipped = False;
					if (IN_3270)
						return len-1;
				}
				break;

			case '\n':
				if (pasting)
				{
					if (!skipped)
						lib3270_newline(hSession);
					skipped = False;
				}
				else
				{
					lib3270_enter(hSession);
					skipped = False;
					if (IN_3270)
						return len-1;
				}
				break;

			case '\r':	/* ignored */
				break;

			case '\t':
			    lib3270_nextfield(hSession);
				skipped = False;
				break;

			case '\\':	/* backslashes are NOT special when pasting */
				if (!pasting)
					state = BACKSLASH;
				else
					key_ACharacter(hSession,(unsigned char) c,KT_STD, ia, &skipped);
				break;

			case '\033': /* ESC is special only when pasting */
				if (pasting)
					state = XGE;
				break;

			case '[':	/* APL left bracket */
				key_ACharacter(hSession,(unsigned char) c, KT_STD, ia, &skipped);
				break;

			case ']':	/* APL right bracket */
				key_ACharacter(hSession,(unsigned char) c, KT_STD, ia, &skipped);
				break;

			default:
/*
#if defined(X3270_DBCS)
				//
				// Try mapping it to the 8-bit character set,
				// otherwise to the 16-bit character set.
				//
				if (dbcs_map8(c, &cx)) {
					key_ACharacter((unsigned char)cx,
					    KT_STD, ia_cause, &skipped);
					break;
				} else if (dbcs_map16(c, ebc)) {
					(void) key_WCharacter(ebc, &skipped);
					break;
				} else {
					lib3270_write_event_trace(hSession,"Cannot convert U+%04x to "
					    "EBCDIC\n", c & 0xffff);
					break;
				}
#endif */
				key_ACharacter(hSession,(unsigned char) c, KT_STD, ia, &skipped);
				break;
			}
			break;

		    case BACKSLASH:	/* last character was a backslash */
				switch (c)
				{
				case 'a':
					popup_an_error(hSession,_( "%s: Bell not supported" ),action_name(String_action));
					state = BASE;
					break;

				case 'b':
					lib3270_cursor_left(hSession);
					skipped = False;
					state = BASE;
					break;

				case 'f':
					lib3270_clear(hSession);
					skipped = False;
					state = BASE;
					if (IN_3270)
						return len-1;
					else
						break;

				case 'n':
					lib3270_enter(hSession);
					skipped = False;
					state = BASE;
					if (IN_3270)
						return len-1;
					else
						break;

				case 'p':
					state = BACKP;
					break;

				case 'r':
					lib3270_newline(hSession);
					skipped = False;
					state = BASE;
					break;

				case 't':
					lib3270_nextfield(hSession);
					skipped = False;
					state = BASE;
					break;

				case 'T':
					lib3270_nextfield(hSession);
					skipped = False;
					state = BASE;
					break;

				case 'v':
					popup_an_error(hSession,_( "%s: Vertical tab not supported" ),action_name(String_action));
					state = BASE;
					break;

				case 'x':
					state = BACKX;
					break;

				case '\\':
					key_ACharacter(hSession,(unsigned char) c, KT_STD, ia,&skipped);
					state = BASE;
					break;

				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					state = OCTAL;
					literal = 0;
					nc = 0;
					continue;

				default:
					state = BASE;
					continue;
				}
				break;

		    case BACKP:	/* last two characters were "\p" */
				switch (c)
				{
				case 'a':
					literal = 0;
					nc = 0;
					state = BACKPA;
					break;

				case 'f':
					literal = 0;
					nc = 0;
					state = BACKPF;
					break;

				default:
					popup_an_error(hSession,_( "%s: Unknown character after \\p" ),action_name(String_action));
					state = BASE;
					break;
				}
				break;

		    case BACKPF: /* last three characters were "\pf" */
				if (nc < 2 && isdigit(c))
				{
					literal = (literal * 10) + (c - '0');
					nc++;
				}
				else if (!nc)
				{
					popup_an_error(hSession,_( "%s: Unknown character after \\pf" ),action_name(String_action));
					state = BASE;
				}
				else
				{
					do_pf(hSession,literal);
					skipped = False;
					if (IN_3270)
						return len-1;
					state = BASE;
					continue;
				}
				break;

		    case BACKPA: /* last three characters were "\pa" */
				if (nc < 1 && isdigit(c))
				{
					literal = (literal * 10) + (c - '0');
					nc++;
				}
				else if (!nc)
				{
					popup_an_error(hSession,_( "%s: Unknown character after \\pa" ),action_name(String_action));
					state = BASE;
				}
				else
				{
					do_pa(hSession, literal);
					skipped = False;
					if (IN_3270)
						return len-1;
					state = BASE;
					continue;
				}
				break;

		    case BACKX:	/* last two characters were "\x" */
				if (isxdigit(c))
				{
					state = HEX;
					literal = 0;
					nc = 0;
					continue;
				}
				else
				{
					popup_an_error(hSession,_( "%s: Missing hex digits after \\x" ),action_name(String_action));
					state = BASE;
					continue;
				}
				continue;

		    case OCTAL:	/* have seen \ and one or more octal digits */
				if (nc < 3 && isdigit(c) && c < '8')
				{
					literal = (literal * 8) + FROM_HEX(c);
					nc++;
					break;
				}
				else
				{
					key_ACharacter(hSession,(unsigned char) literal, KT_STD,ia, &skipped);
					state = BASE;
					continue;
				}

		    case HEX:	/* have seen \ and one or more hex digits */
				if (nc < 2 && isxdigit(c))
				{
					literal = (literal * 16) + FROM_HEX(c);
					nc++;
					break;
				}
				else
				{
					key_ACharacter(hSession,(unsigned char) literal, KT_STD, ia, &skipped);
					state = BASE;
					continue;
				}

		    case XGE:	/* have seen ESC */
				switch (c)
				{
					case ';':	/* FM */
						key_Character(hSession, EBC_fm, False, True, &skipped);
						break;

					case '*':	/* DUP */
						key_Character(hSession, EBC_dup, False, True, &skipped);
						break;

					default:
						key_ACharacter(hSession,(unsigned char) c, KT_GE, ia,&skipped);
						break;
				}
				state = BASE;
				break;
		}
		ws++;
		len--;
	}

	switch (state)
	{
	    case BASE:
			if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_MARGINED_PASTE) && BA_TO_COL(hSession->cursor_addr) < orig_col)
			{
				(void) remargin(hSession,orig_col);
			}
			break;

	    case OCTAL:
	    case HEX:
			key_ACharacter(hSession,(unsigned char) literal, KT_STD, ia, &skipped);
			state = BASE;
			if (lib3270_get_toggle(hSession,LIB3270_TOGGLE_MARGINED_PASTE) && BA_TO_COL(hSession->cursor_addr) < orig_col)
			{
				(void) remargin(hSession,orig_col);
			}
			break;

	    case BACKPF:
			if (nc > 0)
			{
				do_pf(hSession,literal);
				state = BASE;
			}
			break;

	    case BACKPA:
			if (nc > 0)
			{
				do_pa(hSession,literal);
				state = BASE;
			}
			break;

	    default:
			popup_an_error(hSession,"%s: Missing data after \\",action_name(String_action));
			break;
	}

	hSession->cbk.display(hSession);
	return len;
}

#if defined(X3270_FT) /*[*/
/*
 * Set up the cursor and input field for command input.
 * Returns the length of the input field, or 0 if there is no field
 * to set up.
 */
int kybd_prime(H3270 *hSession)
{
	int baddr;
	register unsigned char fa;
	int len = 0;

	/*
	 * No point in trying if the screen isn't formatted, the keyboard
	 * is locked, or we aren't in 3270 mode.
	 */
	if (!hSession->formatted || hSession->kybdlock || !IN_3270)
		return 0;

	fa = get_field_attribute(hSession,hSession->cursor_addr);
	if (hSession->ea_buf[hSession->cursor_addr].fa || FA_IS_PROTECTED(fa))
	{
		/*
		 * The cursor is not in an unprotected field.  Find the
		 * next one.
		 */
		baddr = lib3270_get_next_unprotected(hSession,hSession->cursor_addr);

		/* If there isn't any, give up. */
		if (!baddr)
			return 0;

		/* Move the cursor there. */
	}
	else
	{
		/* Already in an unprotected field.  Find its start. */
		baddr = hSession->cursor_addr;
		while (!hSession->ea_buf[baddr].fa)
		{
			DEC_BA(baddr);
		}
		INC_BA(baddr);
	}

	/* Move the cursor to the beginning of the field. */
	cursor_move(hSession,baddr);

	/* Erase it. */
	while (!hSession->ea_buf[baddr].fa)
	{
		ctlr_add(hSession,baddr, 0, 0);
		len++;
		INC_BA(baddr);
	}

	/* Return the field length. */
	return len;
}
#endif /*]*/

