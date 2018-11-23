
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <lib3270.h>

#define MAX_ARGS 10

int main(int numpar, char *param[])
{
	H3270		* h;
	int			  rc	= 0;
	const char  * url	= getenv("TN3270URL");


	h = lib3270_session_new("");
	printf("3270 session %p created\n]",h);

//	lib3270_set_toggle(session,LIB3270_TOGGLE_DS_TRACE,1);

	lib3270_set_url(h,url ? url : "tn3270://fandezhi.efglobe.com");
	rc = lib3270_connect(h,1);

	printf("\nConnect exits with rc=%d\n",rc);

	lib3270_wait_for_ready(h,10);


	lib3270_session_free(h);

	return 0;
}