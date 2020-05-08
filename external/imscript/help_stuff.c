#include <string.h> // strcmp
#include <stdio.h>  // puts, snprintf
#include <stdlib.h> // system, exit, abs

static int do_man(int raw)
{
#ifdef __OpenBSD__
#define MANPIPE "|mandoc -a"
#else
#define MANPIPE "|man -l -"
#endif
	char buf[0x200];
	snprintf(buf, 0x200, "help2man -N -S imscript -n \"%s\" %s%s %s",
			//                                  1    2 3  4
			help_string_oneliner,     // 1
			raw>0 ? "" : "./",        // 2
			help_string_name,         // 3
			abs(raw)>1 ? "" : MANPIPE // 4
			);
	return system(buf);
}

static void if_help_is_requested_print_it_and_exit_the_program(char *s)
{
	if (!strcmp(s, "-?"))         exit(0*puts(help_string_oneliner));
	if (!strcmp(s, "-h"))         exit(0*puts(help_string_usage));
	if (!strcmp(s, "--help"))     exit(0*puts(help_string_long));
	if (!strcmp(s, "--version"))  exit(0*puts(help_string_version));
	if (!strcmp(s, "--man"))      exit(do_man( 1));
	if (!strcmp(s, "--manraw"))   exit(do_man( 2));
	if (!strcmp(s, "--man-x"))    exit(do_man(-1));
	if (!strcmp(s, "--manraw-x")) exit(do_man(-2));
	if (!strcmp(s, "--help-oneliner")) puts(help_string_oneliner);
}

