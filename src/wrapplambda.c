#include <stdio.h>

#include <setjmp.h>
#if __STDC_VERSION__ >= 201112L
_Thread_local
#endif
jmp_buf g_jmpbuf;

#define _FAIL_C
#include <stdarg.h>
static void fail(const char *fmt, ...) __attribute__((noreturn));
static void fail(const char *fmt, ...)
{
	va_list argp;
	fprintf(stderr, "plambda error: ");
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
	fprintf(stderr, "\n");
	fflush(NULL);
	longjmp(g_jmpbuf, 1);
}

#define HIDE_ALL_MAINS
#include "plambda.c"

float* execute_plambda(int n, float** x, int* w, int* h, int* pd, char* program, int* opd)
{
	if (setjmp(g_jmpbuf)) {
		return 0;
	}

	struct plambda_program p[1];
	plambda_compile_program(p, program);

	if (n > 0 && p->var->n == 0) {
		int maxplen = n*20 + strlen(program) + 100;
		char newprogram[maxplen];
		add_hidden_variables(newprogram, maxplen, n, program);
		plambda_compile_program(p, newprogram);
	}

	if (n != p->var->n && !(n == 1 && p->var->n == 0))
		fail("the program expects %d variables but %d images "
			 "were given", p->var->n, n);

	//print_compiled_program(p);
	int pdreal = eval_dim(p, x, pd);

	float *out = xmalloc(*w * *h * pdreal * sizeof*out);
	*opd = run_program_vectorially(out, pdreal, p, x, w, h, pd);
	assert(*opd == pdreal);

	collection_of_varnames_end(p->var);
	return out;
}

// vim:set foldmethod=marker:
