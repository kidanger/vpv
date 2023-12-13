// IIO: a library for reading small images                                  {{{1
//
// Goal: load an image (of unknown format) from a given file
//
// Technique: read the first few bytes of the file to identify the format, and
// call the appropriate image library (lipng, lipjpeg, libtiff, etc).  For
// simple, uncompressed formats, write the loading code by hand.  For formats
// having a library with a nice API, call the library functions.  For other or
// unrecognized formats use an external program (convert, anytopnm,
// gm convert...) to convert them into a readable format.  If anything else
// fails, assume that the image is in headings+raw format, and try to extract
// its dimensions and headings using some heuristics (file name containing
// "%dx%d", headings containing ascii numbers, etc.)
//
// Difficulties: most image libraries expect to be fed a whole file, not a
// beheaded file.  Thus, some hackery is necessary.
//
// See file "iio.txt" for slightly more detailed documentation, and "iio.h" for
// the API
//


// #defines                                                                 {{{1

//
// editable configuration
//

#define IIO_SHOW_DEBUG_MESSAGES

//#define IIO_DISABLE_IMGLIBS

//#define IIO_ABORT_ON_ERROR

#define I_CAN_HAS_LIBPNG
#define I_CAN_HAS_LIBJPEG
#define I_CAN_HAS_LIBTIFF
//#define I_CAN_HAS_LIBWEBP
//#define I_CAN_HAS_LIBHEIF
//#define I_CAN_HAS_LIBAVIF
//#define I_CAN_HAS_LIBHDF5
//#define I_CAN_HAS_LIBEXR

//#define I_CAN_HAS_WGET
//#define I_CAN_HAS_WHATEVER
//#define I_CAN_KEEP_TMP_FILES

#define I_CAN_HAS_INT64

//
// portability macros to choose OS features
//
//
#define I_CAN_POSIX
#define I_CAN_LINUX
#define I_CAN_GETENV


//
// no need to edit below here
//

#ifdef IIO_SHOW_DEBUG_MESSAGES
#  ifdef __STRICT_ANSI__
#    define IIO_DEBUG(...) do {\
	if (xgetenv("IIO_DEBUG")){\
	fprintf(stderr,"DEBUG(%s:%d:%s): ",__FILE__,__LINE__,__func__);\
	fprintf(stderr,__VA_ARGS__);}} while(0)
#  else//__STRICT_ANSI__
#    define IIO_DEBUG(...) do {\
	if (xgetenv("IIO_DEBUG")) {\
		switch(atoi(xgetenv("IIO_DEBUG"))) {\
		case 3: fprintf(stderr,"DEBUG(%s:%d:%s): ",__FILE__,__LINE__,__PRETTY_FUNCTION__); break;\
		case 2: fprintf(stderr,"DEBUG(%s:%d): ",__FILE__,__LINE__); break;\
		case 1: fprintf(stderr,"DEBUG: "); break;\
		default: break; } fprintf(stderr,__VA_ARGS__); } } while(0)
#  endif//__STRICT_ANSI__
#else//IIO_SHOW_DEBUG_MESSAGES
#  define IIO_DEBUG(...) do { do_nop(__VA_ARGS__); } while(0) /* nothing */
#endif//IIO_SHOW_DEBUG_MESSAGES


#ifdef IIO_DISABLE_LIBEXR
#undef I_CAN_HAS_LIBEXR
#endif

#ifdef IIO_DISABLE_LIBPNG
#undef I_CAN_HAS_LIBPNG
#endif

#ifdef IIO_DISABLE_LIBJPEG
#undef I_CAN_HAS_LIBJPEG
#endif

#ifdef IIO_DISABLE_LIBTIFF
#undef I_CAN_HAS_LIBTIFF
#endif

#ifdef IIO_DISABLE_LIBHDF5
#undef I_CAN_HAS_LIBHDF5
#endif

#ifdef IIO_DISABLE_LIBWEBP
#undef I_CAN_HAS_LIBWEBP
#endif

#ifdef IIO_DISABLE_LIBHEIF
#undef I_CAN_HAS_LIBHEIF
#endif

#ifdef IIO_DISABLE_LIBAVIF
#undef I_CAN_HAS_LIBAVIF
#endif


#ifdef IIO_DISABLE_IMGLIBS
#undef I_CAN_HAS_LIBPNG
#undef I_CAN_HAS_LIBJPEG
#undef I_CAN_HAS_LIBTIFF
#undef I_CAN_HAS_LIBEXR
#undef I_CAN_HAS_LIBHDF5
#undef I_CAN_HAS_LIBWEBP
#undef I_CAN_HAS_LIBAVIF
#undef I_CAN_HAS_LIBHEIF
#endif




// internal shit
#define IIO_MAX_DIMENSION 20

//
// enum-like, only used internally
//

#define IIO_TYPE_INT8 1
#define IIO_TYPE_UINT8 2
#define IIO_TYPE_INT16 3
#define IIO_TYPE_UINT16 4
#define IIO_TYPE_INT32 5
#define IIO_TYPE_UINT32 6
#define IIO_TYPE_FLOAT 7
#define IIO_TYPE_DOUBLE 8
#define IIO_TYPE_LONGDOUBLE 9
#define IIO_TYPE_INT64 10
#define IIO_TYPE_UINT64 11
#define IIO_TYPE_HALF 12
#define IIO_TYPE_UINT1 13
#define IIO_TYPE_UINT2 14
#define IIO_TYPE_UINT4 15
#define IIO_TYPE_CHAR 16
#define IIO_TYPE_SHORT 17
#define IIO_TYPE_INT 18
#define IIO_TYPE_LONG 19
#define IIO_TYPE_LONGLONG 20

#define IIO_FORMAT_WHATEVER 0
#define IIO_FORMAT_QNM 1
#define IIO_FORMAT_PNG 2
#define IIO_FORMAT_JPEG 3
#define IIO_FORMAT_TIFF 4
#define IIO_FORMAT_RIM 5
#define IIO_FORMAT_BMP 6
#define IIO_FORMAT_EXR 7
#define IIO_FORMAT_JP2 8
#define IIO_FORMAT_VTK 9
#define IIO_FORMAT_CIMG 10
#define IIO_FORMAT_PAU 11
#define IIO_FORMAT_DICOM 12
#define IIO_FORMAT_PFM 13
#define IIO_FORMAT_NIFTI 14
#define IIO_FORMAT_PCX 15
#define IIO_FORMAT_GIF 16
#define IIO_FORMAT_XPM 17
#define IIO_FORMAT_RAFA 18
#define IIO_FORMAT_FLO 19
#define IIO_FORMAT_JUV 20
#define IIO_FORMAT_LUM 21
#define IIO_FORMAT_PCM 22
#define IIO_FORMAT_ASC 23
#define IIO_FORMAT_PDS 24
#define IIO_FORMAT_RAW 25
#define IIO_FORMAT_RWA 26
#define IIO_FORMAT_CSV 27
#define IIO_FORMAT_VRT 28
#define IIO_FORMAT_FFD 29
#define IIO_FORMAT_DLM 30
#define IIO_FORMAT_NPY 31
#define IIO_FORMAT_VIC 32
#define IIO_FORMAT_CCS 33
#define IIO_FORMAT_FIT 34
#define IIO_FORMAT_HDF5 35
#define IIO_FORMAT_TXT 36
#define IIO_FORMAT_RAT 37
#define IIO_FORMAT_WEBP 38
#define IIO_FORMAT_HEIF 39
#define IIO_FORMAT_AVIF 40
#define IIO_FORMAT_UNRECOGNIZED (-1)

//
// sugar
//
#define FORI(n) for(int i=0;i<(int)(n);i++)
#define FORJ(n) for(int j=0;j<(int)(n);j++)
#define FORK(n) for(int k=0;k<(int)(n);k++)
#define FORL(n) for(int l=0;l<(int)(n);l++)

//
// hacks
//
#ifndef __attribute__
#  ifndef __GNUC__
#    define __attribute__(x) /*NOTHING*/
#  endif
#endif




// #includes                                                                {{{1

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <libgen.h> // needed for dirname() multi-platform

#ifdef __MINGW32__ // needed for tmpfile(), this flag is also set by MINGW64 
#include <windows.h>
#endif



#ifdef I_CAN_HAS_LIBPNG
// ugly "feature" in png.h forces this header to be included here
#  include <png.h>
#endif

// portabil

#if _POSIX_C_SOURCE >= 200809L
#  define I_CAN_HAS_FMEMOPEN 1
#endif

#if _POSIX_C_SOURCE >= 200112L || __OpenBSD__ || __DragonFly__ || __FreeBSD__ || __APPLE__
#  define I_CAN_HAS_MKSTEMP 1
#endif




// typedefs                                                                 {{{1

typedef long long longlong;
typedef long double longdouble;




// utility functions                                                        {{{1

#ifndef IIO_ABORT_ON_ERROR

// NOTE: libpng has a nasty "feature" whereby you have to include libpng.h
// before setjmp.h if you want to use both.   This induces the following
// hackery:
#  ifndef I_CAN_HAS_LIBPNG
#    include <setjmp.h>
#  endif//I_CAN_HAS_LIBPNG
#  if __STDC_VERSION__ >= 201112L
_Thread_local
#  endif
jmp_buf global_jump_buffer;
#endif//IIO_ABORT_ON_ERROR

//#include <errno.h> // only for errno
#include <ctype.h> // for isspace
#include <stdlib.h>

#ifdef I_CAN_LINUX
#  include <unistd.h>
static const char *emptystring = "";
static const char *myname(void)
{
#  define n 0x29a
	static char buf[n];
	long p = getpid();
	snprintf(buf, n, "/proc/%ld/cmdline", p);
	FILE *f = fopen(buf, "r");
	if (!f) return emptystring;
	int c, i = 0;
	while ((c = fgetc(f)) != EOF && i < n) {
#  undef n
		buf[i] = c ? c : ' ';
		i += 1;
	}
	if (i) buf[i-1] = '\0';
	fclose(f);
	return buf;
}
#else
static const char *myname(void) { return ""; }
#endif//I_CAN_LINUX


static char *xgetenv(const char *s)
{
#ifdef I_CAN_GETENV
	return getenv(s);
#else//I_CAN_GETENV
	return 0;
#endif// I_CAN_GETENV
}

static void fail(const char *fmt, ...) __attribute__((noreturn,format(printf,1,2)));
static void fail(const char *fmt, ...)

{
	va_list argp;
	fprintf(stderr, "\nIIO_ERROR(\"%s\"): ", myname());
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
	fprintf(stderr, "\n\n");
	fflush(NULL);
//	if (global_hack_to_never_fail)
//	{
//		IIO_DEBUG("now wave a dead chicken and press enter\n");
//		getchar();
//		return;
//	}
//	exit(43);
#ifndef IIO_ABORT_ON_ERROR
	longjmp(global_jump_buffer, 1);
	//iio_single_jmpstuff(true, false);
#else//IIO_ABORT_ON_ERROR
	exit(xgetenv("IIO_SEGFAULT_ON_ERROR")?*(volatile int*)0:-1);
//#  ifdef NDEBUG
//	exit(-1);
//#  else//NDEBUG
	//print_trace(stderr);
//	exit(*(int *)0x43);
//#  endif//NDEBUG
#endif//IIO_ABORT_ON_ERROR
}

#ifndef IIO_SHOW_DEBUG_MESSAGES
static void do_nop(void *p, ...)
{
	va_list argp;
	va_start(argp, p);
	va_end(argp);
}
#endif

static void *xmalloc(size_t size)
{
	if (size == 0)
		fail("xmalloc: zero size");
	void *p = malloc(size);
	if (!p)
	{
		double sm = size / (0x100000 * 1.0);
		fail("xmalloc: out of memory when requesting "
			"%zu bytes (%gMB)",//:\"%s\"",
			size, sm);//, strerror(errno));
	}
	return p;
}

static void *xrealloc(void *p, size_t s)
{
	void *r = realloc(p, s);
	if (!r) fail("realloc failed");
	return r;
}

static void xfree(void *p)
{
	if (!p)
		fail("thou shalt not free a null pointer!");
	free(p);
}

#  if __STDC_VERSION__ >= 201112L
_Thread_local
#  endif
static const
char *global_variable_containing_the_name_of_the_last_opened_file = NULL;

static FILE *xfopen(const char *s, const char *p)
{
	global_variable_containing_the_name_of_the_last_opened_file = NULL;

	if (!s) fail("trying to open a file with NULL name");

	if (0 == strcmp("-", s))
	{
		if (*p == 'w')
			return stdout;
		else if (*p == 'r')
			return stdin;
		else
			fail("unknown fopen mode \"%s\"", p);
	}
	if (0 == strcmp("--", s) && *p == 'w') return stderr;

	// NOTE: the 'b' flag is required for I/O on Windows systems
	// on unix, it is ignored
	char pp[3] = { p[0], 'b', '\0' };
	FILE *f = fopen(s, pp);
	if (f == NULL)
		fail("can not open file \"%s\" in mode \"%s\"",// (%s)",
				s, pp);//, strerror(errno));
	global_variable_containing_the_name_of_the_last_opened_file = s;
	IIO_DEBUG("fopen (%s) = %p\n", s, (void*)f);
	return f;
}

static void xfclose(FILE *f)
{
	global_variable_containing_the_name_of_the_last_opened_file = NULL;
	if (f != stdout && f != stdin && f != stderr) {
		IIO_DEBUG("fclose (%p)", (void*)f);
		int r = fclose(f);
		if (r) fail("fclose error (%d)", r);//\"%s\"",strerror(errno));
	}
}

static int pick_char_for_sure(FILE *f)
{
	int c = getc(f);
	if (EOF == c) {
		xfclose(f);
		fail("input file ended before expected");
	}
	//IIO_DEBUG("pcs = '%c'\n", c);
	return c;
}

static void eat_spaces(FILE *f)
{
	//IIO_DEBUG("inside eat spaces\n");
	int c;
	do
		c = pick_char_for_sure(f);
	while (isspace(c));
	ungetc(c, f);
}

static void eat_line(FILE *f)
{
	//IIO_DEBUG("inside eat line\n");
	while (pick_char_for_sure(f) != '\n')
		;
}

static void eat_spaces_and_comments(FILE *f)
{
	//IIO_DEBUG("inside eat spaces and comments\n");
	int c, comment_char = '#';
	eat_spaces(f);
uppsala:
	c = pick_char_for_sure(f);
	if (c == comment_char)
	{
		eat_line(f);
		eat_spaces(f);
	} else
		ungetc(c, f);
	if (c == comment_char) goto uppsala;
}

static void fill_temporary_filename(char *out)
{
#ifdef I_CAN_HAS_MKSTEMP
		char tfn[] = "/tmp/iio_tmp_file_XXXXXX\0";
		int r = mkstemp(tfn);
		if (r == -1) {
			perror("hola");
			fail("could not create tmp filename");
		}
#else
		static char buf[L_tmpnam+1];
		char *tfn = tmpnam(buf);
#endif//I_CAN_HAS_MKSTEMP
		snprintf(out, FILENAME_MAX, "%s", tfn);
}


// struct iio_image { ... };                                                {{{1

// This struct is used for exchanging image information between internal
// functions.  It could be safely eliminated, and this information be passed as
// five or six variables.
struct iio_image {
	int dimension;        // 1, 2, 3 or 4, typically
	int sizes[IIO_MAX_DIMENSION];
	int pixel_dimension;
	int type;             // IIO_TYPE_*

	char *rem;            // image comment (only used by output functions)

	void *data;
};


// struct iio_image management                                              {{{1

static void iio_image_assert_struct_consistency(struct iio_image *x)
{
	assert(x->dimension > 0);
	assert(x->dimension <= IIO_MAX_DIMENSION);
	FORI(x->dimension) assert(x->sizes[i] > 0);
	assert(x->pixel_dimension > 0);
	switch(x->type) {
	case IIO_TYPE_INT8: case IIO_TYPE_UINT8: case IIO_TYPE_INT16:
	case IIO_TYPE_UINT16: case IIO_TYPE_INT32: case IIO_TYPE_UINT32:
	case IIO_TYPE_FLOAT: case IIO_TYPE_DOUBLE: case IIO_TYPE_LONGDOUBLE:
	case IIO_TYPE_INT64: case IIO_TYPE_UINT64: case IIO_TYPE_HALF:
	case IIO_TYPE_CHAR: case IIO_TYPE_SHORT: case IIO_TYPE_INT:
	case IIO_TYPE_LONG: case IIO_TYPE_LONGLONG: break;
	default: assert(false);
	}
}

// API
static size_t iio_type_size(int type)
{
	switch(type) {
	case IIO_TYPE_INT8: return 1;
	case IIO_TYPE_UINT8: return 1;
	case IIO_TYPE_INT16: return 2;
	case IIO_TYPE_UINT16: return 2;
	case IIO_TYPE_INT32: return 4;
	case IIO_TYPE_UINT32: return 4;
	case IIO_TYPE_INT64: return 8;
	case IIO_TYPE_UINT64: return 8;
	case IIO_TYPE_CHAR: return sizeof(char); // 1
	case IIO_TYPE_SHORT: return sizeof(short);
	case IIO_TYPE_INT: return sizeof(int);
	case IIO_TYPE_LONG: return sizeof(long);
	case IIO_TYPE_LONGLONG: return sizeof(long long);
	case IIO_TYPE_FLOAT: return sizeof(float);
	case IIO_TYPE_DOUBLE: return sizeof(double);
	case IIO_TYPE_LONGDOUBLE: return sizeof(long double);
	case IIO_TYPE_HALF: return sizeof(float)/2;
	default: fail("unrecognized type %d", type);
	}
}

// XXX TODO FIXME: this is architecture dependent!
// this function actually requires A LOT of magic to be portable
// the present solution works well for the typical 32 and 64 bit platforms
static int normalize_type(int type_in)
{
	int type_out;
	switch(type_in) {
	case IIO_TYPE_CHAR: type_out = IIO_TYPE_UINT8; break;
	case IIO_TYPE_SHORT: type_out = IIO_TYPE_INT16; break;
	case IIO_TYPE_INT: type_out = IIO_TYPE_UINT32; break;
	default: type_out = type_in; break;
	}
	if (type_out != type_in) {
		// the following assertion fails on many architectures
		assert(iio_type_size(type_in) == iio_type_size(type_out));
	}
	return type_out;
}


// internal API
static
int iio_type_id(size_t sample_size, bool ieeefp_sample, bool signed_sample)
{
	if (ieeefp_sample) {
		if (signed_sample) fail("signed floats are a no-no!");
		switch(sample_size) {
		case sizeof(float):       return IIO_TYPE_FLOAT;
		case sizeof(double):      return IIO_TYPE_DOUBLE;
#ifdef I_CAN_HAS_LONGDOUBLE
		case sizeof(long double): return IIO_TYPE_LONGDOUBLE;
#endif//I_CAN_HAS_LONGDOUBLE
		case sizeof(float)/2:     return IIO_TYPE_HALF;
		default: fail("bad float size %zu", sample_size);
		}
	} else {
		switch(sample_size) {
		case 1: return signed_sample ? IIO_TYPE_INT8 : IIO_TYPE_UINT8;
		case 2: return signed_sample ? IIO_TYPE_INT16 : IIO_TYPE_UINT16;
		case 4: return signed_sample ? IIO_TYPE_INT32 : IIO_TYPE_UINT32;
		case 8: return signed_sample ? IIO_TYPE_INT64 : IIO_TYPE_UINT64;
		default: fail("bad integral size %zu", sample_size);
		}
	}
}

// internal API
static void iio_type_unid(int *size, bool *ieefp, bool *signedness, int type)
{
	int t = normalize_type(type);
	*size = iio_type_size(t);
	*ieefp = (t==IIO_TYPE_HALF || t==IIO_TYPE_FLOAT
			|| t==IIO_TYPE_DOUBLE || t==IIO_TYPE_LONGDOUBLE);
	*signedness = (t==IIO_TYPE_INT8 || t==IIO_TYPE_INT16
			|| t==IIO_TYPE_INT32 || t==IIO_TYPE_INT64);
}

// internal API
static int iio_image_number_of_elements(struct iio_image *x)
{
	iio_image_assert_struct_consistency(x);
	int r = 1;
	FORI(x->dimension) r *= x->sizes[i];
	return r;
}

// internal API
static int iio_image_number_of_samples(struct iio_image *x)
{
	return iio_image_number_of_elements(x) * x->pixel_dimension;
}

// internal API
static size_t  iio_image_sample_size(struct iio_image *x)
{
	return iio_type_size(x->type);
}

// internal API
static size_t  iio_image_pixel_size(struct iio_image *x)
{
	return iio_type_size(x->type) * x->pixel_dimension;
}

// internal API
static size_t  iio_image_data_size(struct iio_image *x)
{
	return iio_image_sample_size(x) * iio_image_number_of_samples(x);
}

static const char *iio_strtyp(int type)
{
#define M(t) case IIO_TYPE_ ## t: return #t
	switch(type) {
	M(INT8); M(UINT8); M(INT16); M(UINT16); M(INT32); M(UINT32); M(INT64);
	M(UINT64); M(FLOAT); M(DOUBLE); M(LONGDOUBLE); M(HALF); M(UINT1);
	M(UINT2); M(UINT4); M(CHAR); M(SHORT); M(INT); M(LONG); M(LONGLONG);
	default: return "unrecognized";
	}
#undef M
}

static int iio_inttyp(const char *type_name)
{
	int n = strlen(type_name);
	char utyp[n+1]; utyp[n] = '\0';
	for (int i = 0; i < n; i++)
		utyp[i] = toupper(type_name[i]);
#define M(t) if(!strcmp(utyp,#t))return IIO_TYPE_ ## t
	M(INT8); M(UINT8); M(INT16); M(UINT16); M(INT32); M(UINT32); M(INT64);
	M(UINT64); M(FLOAT); M(DOUBLE); M(LONGDOUBLE); M(HALF); M(UINT1);
	M(UINT2); M(UINT4); M(CHAR); M(SHORT); M(INT); M(LONG); M(LONGLONG);
#undef M
	fail("unrecognized typename \"%s\"", type_name);
}

static const char *iio_strfmt(int format)
{
#define M(f) case IIO_FORMAT_ ## f: return #f
	switch(format) {
	M(WHATEVER); M(QNM); M(PNG); M(JPEG);
	M(TIFF); M(RIM); M(BMP); M(EXR); M(JP2);
	M(VTK); M(CIMG); M(PAU); M(DICOM); M(PFM); M(NIFTI);
	M(PCX); M(GIF); M(XPM); M(RAFA); M(FLO); M(LUM); M(JUV);
	M(PCM); M(ASC); M(RAW); M(RWA); M(PDS); M(CSV); M(VRT); M(RAT);
	M(FFD); M(DLM); M(NPY); M(VIC); M(CCS); M(FIT); M(HDF5);
	M(TXT); M(WEBP); M(HEIF); M(AVIF);
	M(UNRECOGNIZED);
	default: fail("caca de la grossa (%d)", format);
	}
#undef M
}

//#ifdef IIO_SHOW_DEBUG_MESSAGES
//static void iio_print_image_info(FILE *f, struct iio_image *x)
//{
//	fprintf(f, "iio_print_image_info %p\n", (void *)x);
//	fprintf(f, "dimension = %d\n", x->dimension);
//	int *s = x->sizes;
//	switch(x->dimension) {
//	case 1: fprintf(f, "size = %d\n", s[0]); break;
//	case 2: fprintf(f, "sizes = %dx%d\n", s[0],s[1]); break;
//	case 3: fprintf(f, "sizes = %dx%dx%d\n", s[0],s[1],s[2]); break;
//	case 4: fprintf(f, "sizes = %dx%dx%dx%d\n", s[0],s[1],s[2],s[3]); break;
//	default: fail("unsupported dimension %d", x->dimension);
//	}
//	fprintf(f, "pixel_dimension = %d\n", x->pixel_dimension);
//	fprintf(f, "type = %s\n", iio_strtyp(x->type));
//	fprintf(f, "data = %p\n", (void *)x->data);
//}
//#endif//IIO_SHOW_DEBUG_MESSAGES


static void iio_image_fill(struct iio_image *x,
		int dimension, int *sizes,
		int type, int pixel_dimension)
{
	x->dimension = dimension;
	FORI(dimension) x->sizes[i] = sizes[i];
	x->type = type;
	x->pixel_dimension = pixel_dimension;
	x->data = NULL;
	x->rem = NULL;
}


static void iio_wrap_image_struct_around_data(struct iio_image *x,
		int dimension, int *sizes, int pixel_dimension,
		int type, void *data)
{
	assert(dimension < IIO_MAX_DIMENSION);
	x->dimension = dimension;
	FORI(x->dimension) x->sizes[i] = sizes[i];
	x->pixel_dimension = pixel_dimension;
	x->type = type;
	x->data = data;
	x->rem = NULL;
}


static void iio_image_build_independent(struct iio_image *x,
		int dimension, int *sizes, int type, int pixel_dimension)
{
	assert(dimension > 0);
	assert(dimension <= 4);
	assert(pixel_dimension > 0);
	iio_image_fill(x, dimension, sizes, type, pixel_dimension);
	size_t datalength = 1; FORI(dimension) datalength *= sizes[i];
	size_t datasize = datalength * iio_type_size(type) * pixel_dimension;
	x->data = xmalloc(datasize);
}

static void iio_image_init2d(struct iio_image *x,
		int w, int h, int pd, int type)
{
	x->dimension = 2;
	x->sizes[0] = w;
	x->sizes[1] = h;
	x->pixel_dimension = pd;
	x->type = type;
	x->data = NULL;
	x->rem = NULL;
}

static void inplace_swap_pixels(struct iio_image *x, int i, int j, int a, int b)
{
	if (x->dimension != 2)
		fail("can only flip 2-dimensional images");
	int w = x->sizes[0];
	//int h = x->sizes[1]; // unused
	int pixsize = x->pixel_dimension * iio_image_sample_size(x);
	uint8_t *p = (i + j * w) * pixsize + (uint8_t*)x->data;
	uint8_t *q = (a + b * w) * pixsize + (uint8_t*)x->data;
	for (int k = 0; k < pixsize; k++)
	{
		uint8_t tmp = p[k];
		p[k] = q[k];
		q[k] = tmp;
	}
}

static void inplace_flip_horizontal(struct iio_image *x)
{
	int w = x->sizes[0];
	int h = x->sizes[1];
	for (int j = 0; j < h; j++)
	for (int i = 0; i < w/2; i++)
		inplace_swap_pixels(x, i, j, w - i - 1, j);
}

static void inplace_flip_vertical(struct iio_image *x)
{
	int w = x->sizes[0];
	int h = x->sizes[1];
	for (int j = 0; j < h/2; j++)
	for (int i = 0; i < w; i++)
		inplace_swap_pixels(x, i, j, i, h - j - 1);
}

static void inplace_transpose(struct iio_image *x)
{
	int w = x->sizes[0];
	int h = x->sizes[1];
	if (w != h)
	{
		void rectangular_not_inplace_transpose(struct iio_image*);
		rectangular_not_inplace_transpose(x);
	} else {
		for (int i = 0; i < w; i++)
		for (int j = 0; j < i; j++)
			inplace_swap_pixels(x, i, j, j, i);
	}
}

static void inplace_reorient(struct iio_image *x, int orientation)
{
	int orx = orientation & 0xff;
	int ory = (orientation & 0xff00)/0x100;
	if (isupper(orx))
		inplace_flip_horizontal(x);
	if (isupper(ory))
		inplace_flip_vertical(x);
	if (toupper(orx) > toupper(ory))
		inplace_transpose(x);
}

static int insideP(int w, int h, int i, int j)
{
	return i>=0 && j>=0 && i<w && j<h;
}


static void inplace_trim(struct iio_image *x,
		int trim_left, int trim_bottom, int trim_right, int trim_top)
{
	IIO_DEBUG("inplace trim (%dx%d) le=%d bo=%d ri=%d to=%d\n",
			x->sizes[0], x->sizes[1],
			trim_left, trim_bottom, trim_right, trim_top);
	assert(2 == x->dimension);
	int pd = x->pixel_dimension;
	int ss = iio_image_sample_size(x);
	int ps = pd * ss;
	int w = x->sizes[0];
	int h = x->sizes[1];
	int nw = w - trim_left - trim_right; // new width
	int nh = h - trim_bottom - trim_top; // new height
	assert(nw > 0 && nh > 0);
	char *old_data = x->data;
	char *new_data = xmalloc(nw * nh * ps);
	if (ss == sizeof(float))
		for (int i = 0; i < nw*nh*pd; i++)
			((float*)new_data)[i] = 0.0f/0.0f;
	else if (ss == sizeof(double))
		for (int i = 0; i < nw*nh*pd; i++)
			((double*)new_data)[i] = 0.0f/0.0f;
	else
		for (int i = 0; i < nw*nh*ps; i++)
			((char*)new_data)[i] = 0;
	for (int j = 0; j < nh; j++)
	for (int i = 0; i < nw; i++)
	if (insideP(w, h, i+trim_left, j+trim_top))
		memcpy(
			new_data + ps * (j*nw+i),
			old_data + ps * ((j+trim_top)*w+(i+trim_left)),
			ps
		);
	x->sizes[0] = nw;
	x->sizes[1] = nh;
	x->data = new_data;
	xfree(old_data);
}

void rectangular_not_inplace_transpose(struct iio_image *x)
{
	assert(2 == x->dimension);
	int ps = x->pixel_dimension * iio_image_sample_size(x); // pixel_size
	int w = x->sizes[0];
	int h = x->sizes[1];
	int nw = h;
	int nh = w;
	char *old_data = x->data;
	char *new_data = xmalloc(nw * nh * ps);
	for (int i = 0; i < nw*nh*ps; i++)
		((char*)new_data)[i] = 1;
	for (int j = 0; j < nh; j++)
	for (int i = 0; i < nw; i++)
		memcpy(
			new_data + ps * (j*nw + i),
			old_data + ps * (i*w  + j),
			ps
		);
	x->sizes[0] = nw;
	x->sizes[1] = nh;
	x->data = new_data;
	xfree(old_data);
}

void general_copy3d_with_transposition(
		void *y,   // output buffer
		void *x,   // input buffer
		int v[3],  // output dimensions
		int u[3],  // input dimensions (in C-order)
		int s,     // element size in bytes
		char *t    // tranformation, e,g, "xZy" swaps xy, inverts Z
		)
{
	int w = u[0]; // "x"
	int h = u[1]; // "y"
	int d = u[2]; // "z"
	char (*X)[h][w][s] = x;
	if (0 == strcmp(t, "xyz")) {
		char (*Y)[h][w][s] = y;     // "z" does not appear here
		for (int k = 0; k < d; k++) // these three loops don't change
		for (int j = 0; j < h; j++) //
		for (int i = 0; i < w; i++) //
			memcpy(Y[k][j][i], X[k][j][i], s); // set arguments of Y
		v[0] = w; v[1] = h; v[2] = d; // set sizes accordingly
	} else if (0 == strcmp(t, "yxz")) {
		char (*Y)[w][h][s] = y;
		FORK(d) FORJ(h) FORI(w) memcpy(Y[k][i][j], X[k][j][i], s);
		v[0] = h; v[1] = w; v[2] = d;
	} else if (0 == strcmp(t, "xzy")) {
		char (*Y)[d][w][s] = y;
		FORK(d) FORJ(h) FORI(w) memcpy(Y[j][k][i], X[k][j][i], s);
		v[0] = w; v[1] = d; v[2] = h;
	} else if (0 == strcmp(t, "zxy")) {
		char (*Y)[w][d][s] = y;
		FORK(d) FORJ(h) FORI(w) memcpy(Y[j][i][k], X[k][j][i], s);
		v[0] = d; v[1] = w; v[2] = h;
	} else if (0 == strcmp(t, "zyx")) {
		char (*Y)[h][d][s] = y;
		FORK(d) FORJ(h) FORI(w) memcpy(Y[i][j][k], X[k][j][i], s);
		v[0] = d; v[1] = h; v[2] = w;
	} else if (0 == strcmp(t, "yzx")) {
		char (*Y)[d][h][s] = y;
		FORK(d) FORJ(h) FORI(w) memcpy(Y[i][k][j], X[k][j][i], s);
		v[0] = h; v[1] = d; v[2] = w;
	} else fail("unrecognized trasnposition string \"%s\"", t);
	// TODO: build a macro to write all this code
	// TODO 2: add reversals along each dimension (total of 6x8=48 modes)
}

static void inplace_3dreorient(struct iio_image *x, char *s)
{
	if (x->dimension != 2) fail("TODO: implement some squeezing here");
	IIO_DEBUG("3dflip %dx%d %d \"%s\"\n", x->sizes[0], x->sizes[1],
			x->pixel_dimension, s);

	char *t = xmalloc(iio_image_data_size(x));
	int z[3] = {x->pixel_dimension, x->sizes[0], x->sizes[1]};
	int Z[3];
	general_copy3d_with_transposition(t, x->data, Z, z,
			iio_image_sample_size(x), s);
	memcpy(x->data, t, iio_image_data_size(x));
	xfree(t);
	x->sizes[0] = Z[1];
	x->sizes[1] = Z[2];
	x->pixel_dimension = Z[0];
}


// data conversion                                                          {{{1

// macros to crop a numeric value
#define T0(x) ((x)>0?(x):0)
#define T8(x) ((x)>0?((x)<0xff?(x):0xff):0)
#define T6(x) ((x)>0?((x)<0xffff?(x):0xffff):0)

#define CC(a,b) (77*(a)+(b)) // hash number of a conversion pair
#define I8 IIO_TYPE_INT8
#define U8 IIO_TYPE_UINT8
#define I6 IIO_TYPE_INT16
#define U6 IIO_TYPE_UINT16
#define I2 IIO_TYPE_INT32
#define U2 IIO_TYPE_UINT32
#define I4 IIO_TYPE_INT64
#define U4 IIO_TYPE_UINT64
#define F4 IIO_TYPE_FLOAT
#define F8 IIO_TYPE_DOUBLE
#define F6 IIO_TYPE_LONGDOUBLE
static void convert_datum(void *dest, void *src, int dest_fmt, int src_fmt)
{
	// NOTE: verify that this switch is optimized outside of the loop in
	// which it is contained.  Otherwise, produce some self-modifying code
	// here.
	switch(CC(dest_fmt,src_fmt)) {

	// change of sign (lossless, but changes interpretation)
	case CC(I8,U8): *(  int8_t*)dest = *( uint8_t*)src; break;
	case CC(I6,U6): *( int16_t*)dest = *(uint16_t*)src; break;
	case CC(I2,U2): *( int32_t*)dest = *(uint32_t*)src; break;
	case CC(U8,I8): *( uint8_t*)dest = *(  int8_t*)src; break;
	case CC(U6,I6): *(uint16_t*)dest = *( int16_t*)src; break;
	case CC(U2,I2): *(uint32_t*)dest = *( int32_t*)src; break;

	// different size signed integers (3 lossy, 3 lossless)
	case CC(I8,I6): *(  int8_t*)dest = *( int16_t*)src; break;//iw810
	case CC(I8,I2): *(  int8_t*)dest = *( int32_t*)src; break;//iw810
	case CC(I6,I2): *( int16_t*)dest = *( int32_t*)src; break;//iw810
	case CC(I6,I8): *( int16_t*)dest = *(  int8_t*)src; break;
	case CC(I2,I8): *( int32_t*)dest = *(  int8_t*)src; break;
	case CC(I2,I6): *( int32_t*)dest = *( int16_t*)src; break;

	// different size unsigned integers (3 lossy, 3 lossless)
	case CC(U8,U6): *( uint8_t*)dest = T8(*(uint16_t*)src); break;//iw810
	case CC(U8,U2): *( uint8_t*)dest = *(uint32_t*)src; break;//iw810
	case CC(U6,U2): *(uint16_t*)dest = *(uint32_t*)src; break;//iw810
	case CC(U6,U8): *(uint16_t*)dest = *( uint8_t*)src; break;
	case CC(U2,U8): *(uint32_t*)dest = *( uint8_t*)src; break;
	case CC(U2,U6): *(uint32_t*)dest = *(uint16_t*)src; break;

	// diferent size mixed integers, make signed (3 lossy, 3 lossless)
	case CC(I8,U6): *(  int8_t*)dest = *(uint16_t*)src; break;//iw810
	case CC(I8,U2): *(  int8_t*)dest = *(uint32_t*)src; break;//iw810
	case CC(I6,U2): *( int16_t*)dest = *(uint32_t*)src; break;//iw810
	case CC(I6,U8): *( int16_t*)dest = *( uint8_t*)src; break;
	case CC(I2,U8): *( int32_t*)dest = *( uint8_t*)src; break;
	case CC(I2,U6): *( int32_t*)dest = *(uint16_t*)src; break;

	// diferent size mixed integers, make unsigned (3 lossy, 3 lossless)
	case CC(U8,I6): *( uint8_t*)dest = *( int16_t*)src; break;//iw810
	case CC(U8,I2): *( uint8_t*)dest = *( int32_t*)src; break;//iw810
	case CC(U6,I2): *(uint16_t*)dest = *( int32_t*)src; break;//iw810
	case CC(U6,I8): *(uint16_t*)dest = *(  int8_t*)src; break;
	case CC(U2,I8): *(uint32_t*)dest = *(  int8_t*)src; break;
	case CC(U2,I6): *(uint32_t*)dest = *( int16_t*)src; break;

	// from float (typically lossy, except for small integers)
	case CC(I8,F4): *(  int8_t*)dest = *( float*)src; break;//iw810
	case CC(I6,F4): *( int16_t*)dest = *( float*)src; break;//iw810
	case CC(I2,F4): *( int32_t*)dest = *( float*)src; break;
	case CC(I8,F8): *(  int8_t*)dest = *(double*)src; break;//iw810
	case CC(I6,F8): *( int16_t*)dest = *(double*)src; break;//iw810
	case CC(I2,F8): *( int32_t*)dest = *(double*)src; break;//iw810
	case CC(U8,F4): *( uint8_t*)dest = T8(0.5+*( float*)src); break;//iw810
	case CC(U6,F4): *(uint16_t*)dest = T6(0.5+*( float*)src); break;//iw810
	case CC(U2,F4): *(uint32_t*)dest = *( float*)src; break;
	case CC(U8,F8): *( uint8_t*)dest = T8(0.5+*(double*)src); break;//iw810
	case CC(U6,F8): *(uint16_t*)dest = T6(0.5+*(double*)src); break;//iw810
	case CC(U2,F8): *(uint32_t*)dest = *(double*)src; break;//iw810

	// to float (typically lossless, except for large integers)
	case CC(F4,I8): *( float*)dest = *(  int8_t*)src; break;
	case CC(F4,I6): *( float*)dest = *( int16_t*)src; break;
	case CC(F4,I2): *( float*)dest = *( int32_t*)src; break;//iw810
	case CC(F8,I8): *(double*)dest = *(  int8_t*)src; break;
	case CC(F8,I6): *(double*)dest = *( int16_t*)src; break;
	case CC(F8,I2): *(double*)dest = *( int32_t*)src; break;
	case CC(F4,U8): *( float*)dest = *( uint8_t*)src; break;
	case CC(F4,U6): *( float*)dest = *(uint16_t*)src; break;
	case CC(F4,U2): *( float*)dest = *(uint32_t*)src; break;//iw810
	case CC(F8,U8): *(double*)dest = *( uint8_t*)src; break;
	case CC(F8,U6): *(double*)dest = *(uint16_t*)src; break;
	case CC(F8,U2): *(double*)dest = *(uint32_t*)src; break;
	case CC(F8,F4): *(double*)dest = *(  float*)src; break;
	case CC(F4,F8): *( float*)dest = *( double*)src; break;

#ifdef I_CAN_HAS_INT64
	// to int64_t and uint64_t
	case CC(I4,I8): *( int64_t*)dest = *(  int8_t*)src; break;
	case CC(I4,I6): *( int64_t*)dest = *( int16_t*)src; break;
	case CC(I4,I2): *( int64_t*)dest = *( int32_t*)src; break;
	case CC(I4,U8): *( int64_t*)dest = *( uint8_t*)src; break;
	case CC(I4,U6): *( int64_t*)dest = *(uint16_t*)src; break;
	case CC(I4,U2): *( int64_t*)dest = *(uint32_t*)src; break;
	case CC(I4,F4): *( int64_t*)dest = *(   float*)src; break;
	case CC(I4,F8): *( int64_t*)dest = *(  double*)src; break;
	case CC(U4,I8): *(uint64_t*)dest = *(  int8_t*)src; break;
	case CC(U4,I6): *(uint64_t*)dest = *( int16_t*)src; break;
	case CC(U4,I2): *(uint64_t*)dest = *( int32_t*)src; break;
	case CC(U4,U8): *(uint64_t*)dest = *( uint8_t*)src; break;
	case CC(U4,U6): *(uint64_t*)dest = *(uint16_t*)src; break;
	case CC(U4,U2): *(uint64_t*)dest = *(uint32_t*)src; break;
	case CC(U4,F4): *(uint64_t*)dest = *(   float*)src; break;
	case CC(U4,F8): *(uint64_t*)dest = *(  double*)src; break;

	// from int64_t and uint64_t
	case CC(I8,I4): *(  int8_t*)dest = *( int64_t*)src; break;
	case CC(I6,I4): *( int16_t*)dest = *( int64_t*)src; break;
	case CC(I2,I4): *( int32_t*)dest = *( int64_t*)src; break;
	case CC(U8,I4): *( uint8_t*)dest = *( int64_t*)src; break;
	case CC(U6,I4): *(uint16_t*)dest = *( int64_t*)src; break;
	case CC(U2,I4): *(uint32_t*)dest = *( int64_t*)src; break;
	case CC(F4,I4): *(   float*)dest = *( int64_t*)src; break;
	case CC(F8,I4): *(  double*)dest = *( int64_t*)src; break;
	case CC(I8,U4): *(  int8_t*)dest = *(uint64_t*)src; break;
	case CC(I6,U4): *( int16_t*)dest = *(uint64_t*)src; break;
	case CC(I2,U4): *( int32_t*)dest = *(uint64_t*)src; break;
	case CC(U8,U4): *( uint8_t*)dest = *(uint64_t*)src; break;
	case CC(U6,U4): *(uint16_t*)dest = *(uint64_t*)src; break;
	case CC(U2,U4): *(uint32_t*)dest = *(uint64_t*)src; break;
	case CC(F4,U4): *(   float*)dest = *(uint64_t*)src; break;
	case CC(F8,U4): *(  double*)dest = *(uint64_t*)src; break;
#endif//I_CAN_HAS_INT64

#ifdef I_CAN_HAS_LONGDOUBLE
	// to longdouble
	case CC(F6,I8): *(longdouble*)dest = *(  int8_t*)src; break;
	case CC(F6,I6): *(longdouble*)dest = *( int16_t*)src; break;
	case CC(F6,I2): *(longdouble*)dest = *( int32_t*)src; break;
	case CC(F6,U8): *(longdouble*)dest = *( uint8_t*)src; break;
	case CC(F6,U6): *(longdouble*)dest = *(uint16_t*)src; break;
	case CC(F6,U2): *(longdouble*)dest = *(uint32_t*)src; break;
	case CC(F6,F4): *(longdouble*)dest = *(   float*)src; break;
	case CC(F6,F8): *(longdouble*)dest = *(  double*)src; break;

	// from longdouble
	case CC(I8,F6): *(  int8_t*)dest = *(longdouble*)src; break;
	case CC(I6,F6): *( int16_t*)dest = *(longdouble*)src; break;
	case CC(I2,F6): *( int32_t*)dest = *(longdouble*)src; break;
	case CC(U8,F6): *( uint8_t*)dest = T8(0.5+*(longdouble*)src); break;
	case CC(U6,F6): *(uint16_t*)dest = T6(0.5+*(longdouble*)src); break;
	case CC(U2,F6): *(uint32_t*)dest = *(longdouble*)src; break;
	case CC(F4,F6): *(   float*)dest = *(longdouble*)src; break;
	case CC(F8,F6): *(  double*)dest = *(longdouble*)src; break;

#ifdef I_CAN_HAS_INT64 //(nested)
	case CC(F6,I4): *(longdouble*)dest = *( int64_t*)src; break;
	case CC(F6,U4): *(longdouble*)dest = *(uint64_t*)src; break;
	case CC(I4,F6): *( int64_t*)dest = *(longdouble*)src; break;
	case CC(U4,F6): *(uint64_t*)dest = *(longdouble*)src; break;
#endif//I_CAN_HAS_INT64 (nested)

#endif//I_CAN_HAS_LONGDOUBLE

	default: fail("bad conversion from %d to %d", src_fmt, dest_fmt);
	}
}
#undef CC
#undef I8
#undef U8
#undef I6
#undef U6
#undef I2
#undef U2
#undef F4
#undef F8
#undef F6

static void *convert_data(void *src, int n, int dest_fmt, int src_fmt)
{
	if (src_fmt == IIO_TYPE_FLOAT)
		IIO_DEBUG("first float sample = %g\n", *(float*)src);
	size_t src_width = iio_type_size(src_fmt);
	size_t dest_width = iio_type_size(dest_fmt);
	IIO_DEBUG("converting %d samples from %s to %s\n", n, iio_strtyp(src_fmt), iio_strtyp(dest_fmt));
	IIO_DEBUG("src width = %zu\n", src_width);
	IIO_DEBUG("dest width = %zu\n", dest_width);
	char *r = xmalloc(n * dest_width);
	// NOTE: the switch inside "convert_datum" should be optimized
	// outside of this loop
	for (int i = 0; i < n; i++)
	{
		void *to   = i * dest_width + r;
		void *from = i * src_width  + (char *)src;
		convert_datum(to, from, dest_fmt, src_fmt);
	}
	xfree(src);
	return r;
}

static void unpack_nibbles_to_bytes(uint8_t out[2], uint8_t in)
{
	out[1] = (in & 0x0f);    //0b00001111
	out[0] = (in & 0xf0)>>4; //0b11110000
}

static void unpack_couples_to_bytes(uint8_t out[4], uint8_t in)
{
	out[3] = (in & 0x03);    //0b00000011
	out[2] = (in & 0x0c)>>2; //0b00001100
	out[1] = (in & 0x30)>>4; //0b00110000
	out[0] = (in & 0xc0)>>6; //0b11000000
}

static void unpack_bits_to_bytes(uint8_t out[8], uint8_t in)
{
	out[7] = (in & 1) ? 1 : 0;
	out[6] = (in & 2) ? 1 : 0;
	out[5] = (in & 4) ? 1 : 0;
	out[4] = (in & 8) ? 1 : 0;
	out[3] = (in & 16) ? 1 : 0;
	out[2] = (in & 32) ? 1 : 0;
	out[1] = (in & 64) ? 1 : 0;
	out[0] = (in & 128) ? 1 : 0;
}

static void unpack_to_bytes_here(uint8_t *dest, uint8_t *src, int n, int bits)
{
	//fprintf(stderr, "unpacking %d bytes %d-fold\n", n, bits);
	assert(bits==1 || bits==2 || bits==4);
	//size_t unpack_factor = 8 / bits;
	switch(bits) {
	case 1: FORI(n)    unpack_bits_to_bytes(dest + 8*i, src[i]); break;
	case 2: FORI(n) unpack_couples_to_bytes(dest + 4*i, src[i]); break;
	case 4: FORI(n) unpack_nibbles_to_bytes(dest + 2*i, src[i]); break;
	default: fail("very strange error");
	}
}

static void iio_convert_samples(struct iio_image *x, int desired_type)
{
	int source_type = normalize_type(x->type);
	desired_type = normalize_type(desired_type);
	if (source_type == desired_type) return;
	IIO_DEBUG("converting from %s to %s\n", iio_strtyp(x->type), iio_strtyp(desired_type));
	int n = iio_image_number_of_samples(x);
	x->data = convert_data(x->data, n, desired_type, source_type);
	x->type = desired_type;
}

static void iio_hacky_colorize(struct iio_image *x, int pd)
{
	// TODO: do something sensible for 2 or 4 channels
	if (x->pixel_dimension != 1)
		fail("please, do not colorize color stuff");
	int n = iio_image_number_of_elements(x);
	int ss = iio_image_sample_size(x);
	void *rdata = xmalloc(n*ss*pd);
	char *data_dest = rdata;
	char *data_src = x->data;
	FORI(n)
		FORJ(pd)
			memcpy(data_dest + (pd*i + j)*ss, data_src + i*ss, ss);
	xfree(x->data);
	x->data=rdata;
	x->pixel_dimension = pd;
}

// uncolorize
static void iio_hacky_uncolorize(struct iio_image *x)
{
	if (x->pixel_dimension != 3)
		fail("please, do not uncolorize non-color stuff");
	assert(x->pixel_dimension == 3);
	int source_type = normalize_type(x->type);
	int n = iio_image_number_of_elements(x);
	switch(source_type) {
	case IIO_TYPE_UINT8: {
		uint8_t (*xd)[3] = x->data;
		uint8_t *r = xmalloc(n*sizeof*r);
		FORI(n)
			r[i] = .299*xd[i][0] + .587*xd[i][1] + .114*xd[i][2];
		xfree(x->data);
		x->data = r;
		}
		break;
	case IIO_TYPE_UINT16: {
		uint16_t (*xd)[3] = x->data;
		uint16_t *r = xmalloc(n*sizeof*r);
		FORI(n)
			r[i] = .299*xd[i][0] + .587*xd[i][1] + .114*xd[i][2];
		xfree(x->data);
		x->data = r;
		}
		break;
	case IIO_TYPE_FLOAT: {
		float (*xd)[3] = x->data;
		float *r = xmalloc(n*sizeof*r);
		FORI(n)
			r[i] = .299*xd[i][0] + .587*xd[i][1] + .114*xd[i][2];
		xfree(x->data);
		x->data = r;
		}
		break;
	default: fail("uncolorize type not supported");
	}
	x->pixel_dimension = 1;
}

// uncolorize
static void iio_hacky_uncolorizea(struct iio_image *x)
{
	if (x->pixel_dimension != 4)
		fail("please, do not uncolorizea non-colora stuff");
	assert(x->pixel_dimension == 4);
	int source_type = normalize_type(x->type);
	int n = iio_image_number_of_elements(x);
	switch(source_type) {
	case IIO_TYPE_UINT8: {
		uint8_t (*xd)[4] = x->data;
		uint8_t *r = xmalloc(n*sizeof*r);
		FORI(n)
			r[i] = .299*xd[i][0] + .587*xd[i][1] + .114*xd[i][2];
		xfree(x->data);
		x->data = r;
		}
		break;
	case IIO_TYPE_FLOAT: {
		float (*xd)[4] = x->data;
		float *r = xmalloc(n*sizeof*r);
		FORI(n)
			r[i] = .299*xd[i][0] + .587*xd[i][1] + .114*xd[i][2];
		xfree(x->data);
		x->data = r;
		}
		break;
	default: fail("uncolorizea type not supported");
	}
	x->pixel_dimension = 1;
}


// general memory and file utilities                                        {{{1


// Input: a partially read stream "f"
// (of which "bufn" bytes are already read into "buf")
//
// Output: a malloc'd block with the whole file content
//
// Implementation: re-invent the wheel
static void *load_rest_of_file(long *on, FILE *f, void *buf, size_t bufn)
{
	size_t n = bufn, ntop = n + 0x3000;
	char *t =  xmalloc(ntop);
	if (!t) fail("out of mem (%zu) while loading file", ntop);
	memcpy(t, buf, bufn);
	while (1) {
		if (n >= ntop) {
			ntop = 1000 + 2*(ntop + 1);
			t = xrealloc(t, ntop);
			if (!t) fail("out of mem (%zu) loading file", ntop);

		}
		int r = fgetc(f);
		if (r == EOF)
			break;
		t[n] = r;//iw810
		n += 1;
	}
	*on = n;
	return t;
}

// Input: a pointer to raw data
//
// Output: the name of a temporary file containing the data
//
// Implementation: re-invent the wheel
static char *put_data_into_temporary_file(void *filedata, size_t filesize)
{
	static char filename[FILENAME_MAX];
	fill_temporary_filename(filename);
	FILE *f = xfopen(filename, "w");
	int cx = fwrite(filedata, filesize, 1, f);
	if (cx != 1) fail("fwrite to temporary file failed");
	xfclose(f);
	return filename;
}

static void delete_temporary_file(char *filename)
{
	(void)filename;
#ifdef I_CAN_KEEP_TMP_FILES
	fprintf(stderr, "WARNING: kept temporary file %s around\n", filename);
#else
	remove(filename);
#endif
}


// Allows read access to memory via a FILE*
// Always returns a valid FILE*
static FILE *iio_fmemopen(void *data, size_t size)
{
#ifdef I_CAN_HAS_FMEMOPEN // GNU case
	FILE *r = fmemopen(data, size, "r");
	if (!r) fail("fmemopen failed");
	return r;
#elif  I_CAN_HAS_FUNOPEN // BSD case
	fail("implement fmemopen using funopen here");
#else // portable case
	FILE *f;
	#ifdef __MINGW32__
		// creating a tempfile can be very slow
		// this is extremely inefficient
		char filename[FILENAME_MAX], pathname[FILENAME_MAX];
		GetTempPath(FILENAME_MAX, pathname);
		GetTempFileName(pathname,"temp",0,filename);
		f = fopen(filename,"w+bTD");
		IIO_DEBUG("creating MINGW temp file %s\n", filename);
	#else
		f = tmpfile();
	#endif // MINGW32
	if (!f) fail("tmpfile failed");
	int cx = fwrite(data, size, 1, f);
	if (cx != 1) fail("fwrite failed");
	rewind(f);
	return f;
#endif // I_CAN_HAS_...
}


// beautiful hack follows
static void *matrix_build(int w, int h, size_t n)
{
	size_t p = sizeof(void *);
	char *r = xmalloc(h*p + w*h*n);
	for (int i = 0; i < h; i++)
		*(void **)(r + i*p) = r + h*p + i*w*n;
	return r;
}

static void *wrap_2dmatrix_around_data(void *x, int w, int h, size_t s)
{
	void *r = matrix_build(w, h, s);
	char *y = h*sizeof(void *) + (char *)r;
	memcpy(y, x, w*h*s);
	xfree(x);
	return r;
}


// todo make this function more general, or a front-end to a general
// "data trasposition" routine
static void break_pixels_float(float *broken, float *clear, int n, int pd)
{
	FORI(n) FORL(pd)
		broken[n*l + i] = clear[pd*i + l];
}

static void
recover_broken_pixels_float(float *clear, float *broken, int n, int pd)
{
	FORL(pd) FORI(n)
		clear[pd*i + l] = broken[n*l + i];
}


//static void break_pixels_uint8(uint8_t *broken, uint8_t *clear, int n, int pd)
//{
//	FORI(n) FORL(pd)
//		broken[n*l + i] = clear[pd*i + l];
//}

static void break_pixels_double(double *broken, double *clear, int n, int pd)
{
	FORI(n) FORL(pd)
		broken[n*l + i] = clear[pd*i + l];
}

static void
recover_broken_pixels_uint8(uint8_t *clear, uint8_t *broken, int n, int pd)
{
	FORL(pd) FORI(n)
		clear[pd*i + l] = broken[n*l + i];
}

static void
recover_broken_pixels_int(int *clear, int *broken, int n, int pd)
{
	FORL(pd) FORI(n)
		clear[pd*i + l] = broken[n*l + i];
}

//static void break_pixels_int(int *broken, int *clear, int n, int pd)
//{
//	FORI(n) FORL(pd)
//		broken[n*l + i] = clear[pd*i + l];
//}


static
void repair_broken_pixels(void *clear, void *broken, int n, int pd, int sz)
{
	char *c = clear;
	char *b = broken;
	FORL(pd) FORI(n)
		memcpy(c + sz*(pd*i+l), b + sz*(n*l + i), sz);
}

static void repair_broken_pixels_inplace(void *x, int n, int pd, int sz)
{
	char *t = malloc(n * pd * sz);
	memcpy(t, x, n * pd * sz);
	repair_broken_pixels(x, t, n, pd, sz);
	free(t);
}

static void
recover_broken_pixels_double(double *clear, double *broken, int n, int pd)
{
	FORL(pd) FORI(n)
		clear[pd*i + l] = broken[n*l + i];
}

// individual format readers                                                {{{1
// PNG reader                                                               {{{2

static void swap_two_bytes(char *here)
{
	char tmp = here[0];
	here[0] = here[1];
	here[1] = tmp;
}

#ifdef I_CAN_HAS_LIBPNG
//#include <png.h>
#include <limits.h> // for CHAR_BIT only
static int read_beheaded_png(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	(void)header;
	// TODO: reorder this mess
	png_structp pp = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!pp) fail("png_create_read_struct fail");
	png_infop pi = png_create_info_struct(pp);
	if (!pi) fail("png_create_info_struct fail");
	if (setjmp(png_jmpbuf(pp))) fail("png error");
	png_init_io(pp, f);
	png_set_sig_bytes(pp, nheader);
	int transforms = PNG_TRANSFORM_IDENTITY
			| PNG_TRANSFORM_PACKING
			| PNG_TRANSFORM_EXPAND
			;
	png_read_png(pp, pi, transforms, NULL);
	png_uint_32 w, h;
	int channels;
	int depth;
	w = png_get_image_width(pp, pi);
	h = png_get_image_height(pp, pi);
	channels = png_get_channels(pp, pi);
	depth = png_get_bit_depth(pp, pi);
	IIO_DEBUG("png get width = %d\n", (int)w);
	IIO_DEBUG("png get height = %d\n", (int)h);
	IIO_DEBUG("png get channels = %d\n", channels);
	IIO_DEBUG("png get depth = %d\n", depth);
	int sizes[2] = {w, h};
	png_bytepp rows = png_get_rows(pp, pi);
	switch (depth) {
	case 1:
	case 8:
		IIO_DEBUG("first byte = %d\n", (int)rows[0][0]);
		iio_image_build_independent(x,2,sizes,IIO_TYPE_CHAR,channels);
		FORJ(h) FORI(w) FORL(channels) {
			char *data = x->data;
			png_byte *b = rows[j] + i*channels + l;
			data[(i+j*w)*channels+l] = *b;
		}
		x->type = IIO_TYPE_CHAR;
		break;
	case 16:
		iio_image_build_independent(x,2,sizes,IIO_TYPE_UINT16,channels);
		FORJ(h) FORI(w) FORL(channels) {
			uint16_t *data = x->data;
			png_byte *b = rows[j] + 2*(i*channels + l);
			swap_two_bytes((char*)b);
			uint16_t *tb = (uint16_t *)b;
			data[(i+j*w)*channels+l] = *tb;
		}
		x->type = IIO_TYPE_UINT16;
		break;
	default: fail("unsuported bit depth %d", depth);
	}
	png_destroy_read_struct(&pp, &pi, NULL);
	return 0;
}

#endif//I_CAN_HAS_LIBPNG

// JPEG reader                                                              {{{2

#ifdef I_CAN_HAS_LIBJPEG
#  include <jpeglib.h>

void on_jpeg_error(j_common_ptr cinfo)
{
	// this error happens, for example, when reading a truncated jpeg file
	char buf[JMSG_LENGTH_MAX];
	(*cinfo->err->format_message)(cinfo, buf);
	fail("JPEG: %s", buf);
}

static int read_whole_jpeg(struct iio_image *x, FILE *f)
{
	// allocate and initialize a JPEG decompression object
	struct jpeg_decompress_struct cinfo[1];
	struct jpeg_error_mgr jerr[1];
	cinfo->err = jpeg_std_error(jerr);
	jerr[0].error_exit = on_jpeg_error;
	jpeg_create_decompress(cinfo);

	// specify the source of the compressed data
	jpeg_stdio_src(cinfo, f);

	// obtain image info
	jpeg_read_header(cinfo, 1);
	int size[2], depth;
	size[0] = cinfo->image_width;
	size[1] = cinfo->image_height;
	depth = cinfo->num_components;
	IIO_DEBUG("jpeg header width = %d\n", size[0]);
	IIO_DEBUG("jpeg header height = %d\n", size[1]);
	IIO_DEBUG("jpeg header colordepth = %d\n", depth);
	iio_image_build_independent(x, 2, size, IIO_TYPE_CHAR, depth);

	// start decompress
	jpeg_start_decompress(cinfo);
	assert(size[0] == (int)cinfo->output_width);
	assert(size[1] == (int)cinfo->output_height);
	assert(depth == cinfo->out_color_components);
	assert(cinfo->output_components == cinfo->out_color_components);

	// read scanlines
	FORI(size[1]) {
		void *wheretoputit = i*depth*size[0] + (char *)x->data;
		JSAMPROW scanline[1] = { wheretoputit };
		int r = jpeg_read_scanlines(cinfo, scanline, 1);
		if (1 != r) fail("failed to rean jpeg scanline %d", i);
	}

	// finish decompress
	jpeg_finish_decompress(cinfo);

	// release the JPEG decompression object
	jpeg_destroy_decompress(cinfo);

	return 0;
}

static int read_beheaded_jpeg(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	long filesize;
	// TODO (optimization): if "f" is rewindable, rewind it!
	void *filedata = load_rest_of_file(&filesize, fin, header, nheader);
	FILE *f = iio_fmemopen(filedata, filesize);

	int r = read_whole_jpeg(x, f);
	if (r) fail("read whole jpeg returned %d", r);
	fclose(f);
	xfree(filedata);

	return 0;
}
#endif//I_CAN_HAS_LIBJPEG

// TIFF reader                                                              {{{2

#ifdef I_CAN_HAS_LIBTIFF
#  include <tiffio.h>

static TIFF *tiffopen_fancy(const char *filename, char *mode)
{
	char *comma = strrchr(filename, ',');
	if (*mode != 'r' || !comma)
	def:	return TIFFOpen(filename, mode);

	int aftercomma = strlen(comma + 1);
	int ndigits = strspn(comma + 1, "0123456789");

	if (aftercomma != ndigits) goto def;
	int index = atoi(comma + 1);

	char buf[strlen(filename)];
	snprintf(buf, strlen(filename), "%s", filename);
	comma = strrchr(buf, ',');
	*comma = '\0';

	TIFF *tif = TIFFOpen(buf, mode);
	if (!tif) return tif;
	for (int i = 0; i < index; i++)
		TIFFReadDirectory(tif);

	return tif;
}

static int read_whole_tiff(struct iio_image *x, const char *filename)
{
	IIO_DEBUG("read whole tiff  \"%s\"\n", filename);
	// tries to read data in the correct format (via scanlines)
	// if it fails, it tries to read ABGR data
	TIFFSetWarningHandler(NULL);//suppress warnings

	//fprintf(stderr, "TIFFOpen \"%s\"\n", filename);
	TIFF *tif = tiffopen_fancy(filename, "rm");
	if (!tif) fail("could not open TIFF file \"%s\"", filename);
	uint32_t w, h;
	uint16_t spp, bps, fmt;
	int r = 0, fmt_iio=-1;
	r += TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
	IIO_DEBUG("tiff get field width %d (r=%d)\n", (int)w, r);
	r += TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
	IIO_DEBUG("tiff get field length %d (r=%d)\n", (int)h, r);
	if (r != 2) fail("can not read tiff of unknown size");

	r = TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
	if(!r) spp=1;
	if(r)IIO_DEBUG("tiff get field spp %d (r=%d)\n", spp, r);

	r = TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bps);
	if(!r) bps=1;
	if(r)IIO_DEBUG("tiff get field bps %d (r=%d)\n", bps, r);

	r = TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &fmt);
	if(!r) fmt = SAMPLEFORMAT_UINT;
	if(r) IIO_DEBUG("tiff get field fmt %d (r=%d)\n", fmt, r);

	// TODO: consider the missing cases (run through PerlMagick's format database)
	uint32_t rps = 0;
	r = TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rps);
	IIO_DEBUG("rps %d (r=%d)\n", rps, r);

	IIO_DEBUG("fmt  = %d\n", fmt);
	IIO_DEBUG("w = %d\n", (int)w);

	// deal with complex issues
	bool complicated = false; // complicated = complex and broken
	if (fmt == SAMPLEFORMAT_COMPLEXINT || fmt == SAMPLEFORMAT_COMPLEXIEEEFP)
	{
		IIO_DEBUG("complex TIFF!\n");
		spp *= 2;
		bps /= 2;
		complicated = true; // to be updated later
	}
	if (fmt == SAMPLEFORMAT_COMPLEXINT   ) fmt = SAMPLEFORMAT_INT;
	if (fmt == SAMPLEFORMAT_COMPLEXIEEEFP) fmt = SAMPLEFORMAT_IEEEFP;

	// set appropriate size and type flags
	if (fmt == SAMPLEFORMAT_UINT) {
		if (1 == bps) fmt_iio = IIO_TYPE_UINT1;
		else if (2 == bps) fmt_iio = IIO_TYPE_UINT2;
		else if (4 == bps) fmt_iio = IIO_TYPE_UINT4;
		else if (8 == bps) fmt_iio = IIO_TYPE_UINT8;
		else if (16 == bps) fmt_iio = IIO_TYPE_UINT16;
		else if (32 == bps) fmt_iio = IIO_TYPE_UINT32;
		else fail("unrecognized UINT type of size %d bits", bps);
	} else if (fmt == SAMPLEFORMAT_INT) {
		if (8 == bps) fmt_iio = IIO_TYPE_INT8;
		else if (16 == bps) fmt_iio = IIO_TYPE_INT16;
		else if (32 == bps) fmt_iio = IIO_TYPE_INT32;
		else fail("unrecognized INT type of size %d bits", bps);
	} else if (fmt == SAMPLEFORMAT_IEEEFP) {
		IIO_DEBUG("floating tiff!\n");
		if (32 == bps) fmt_iio = IIO_TYPE_FLOAT;
		else if (64 == bps) fmt_iio = IIO_TYPE_DOUBLE;
		else fail("unrecognized FLOAT type of size %d bits", bps);
	} else fail("unrecognized tiff sample format %d (see tiff.h)", fmt);

	if (bps >= 8 && bps != 8*iio_type_size(fmt_iio)) {
		IIO_DEBUG("bps = %d\n", bps);
		IIO_DEBUG("fmt_iio = %d\n", fmt_iio);
		IIO_DEBUG("ts = %zu\n", iio_type_size(fmt_iio));
		IIO_DEBUG("8*ts = %zu\n", 8*iio_type_size(fmt_iio));
	}
	if (bps >= 8) assert(bps == 8*iio_type_size(fmt_iio));

	uint16_t planarity;
	r = TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarity);
	if (r != 1) planarity = PLANARCONFIG_CONTIG;
	bool broken = planarity == PLANARCONFIG_SEPARATE;
	complicated = complicated && broken; // complicated = complex and broken

	uint16_t compression;
	r = TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
	if (r != 1) compression = 1; // 1 == no compression
	IIO_DEBUG("TIFF Tag Compression = %d\n", compression);
	IIO_DEBUG("w = %d\n", (int)w);

	uint32_t rows_per_strip;
	r = TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
	IIO_DEBUG("r = %d\n", (int)r);
	IIO_DEBUG("w = %d\n", (int)w);
	if (r != 1) rows_per_strip = 0;
	IIO_DEBUG("TIFF Tag Rows Per Strip = %d\n", rows_per_strip);
	IIO_DEBUG("w = %d\n", (int)w);


	// acquire memory block
	uint32_t scanline_size = (w * (int)spp * (int)bps)/8;
	int rbps = (bps/8) ? (bps/8) : 1;
	uint32_t uscanline_size = w * (int)spp * (int)rbps;
	IIO_DEBUG("w = %d\n", (int)w);
	IIO_DEBUG("bps = %d\n", (int)bps);
	IIO_DEBUG("spp = %d\n", (int)spp);
	IIO_DEBUG("rbps = %d\n", (int)rbps);
	IIO_DEBUG("sls = %d\n", (int)scanline_size);
	IIO_DEBUG("uss = %d\n", (int)uscanline_size);
	int sls = TIFFScanlineSize(tif);
	IIO_DEBUG("sls(r) = %d\n", (int)sls);
	IIO_DEBUG("planarity = %d (%s)\n", r, broken?"broken":"normal");

	if (xgetenv("IIO_OVERRIDE_SLS"))
		scanline_size = sls;

	if ((int)scanline_size != sls)
	{
		if (broken && sls*spp == (int)scanline_size)
			goto go_on;
		// use basic RGBA reader for inconsistently reported images
		// this may happen when each channel has a different format
		fprintf(stderr, "IIO TIFF WARN: scanline_size,sls = %d,%d\n",
				(int)scanline_size,sls);
		IIO_DEBUG("tiff read RGBA image interfacing:\n");
		IIO_DEBUG("\tw = %d\n", w);
		IIO_DEBUG("\th = %d\n", h);
		iio_image_init2d(x, w, h, 4, IIO_TYPE_UINT8);
		x->data = xmalloc(w*h*4);
		r = TIFFReadRGBAImageOriented(tif, w, h, (uint32_t*)x->data, ORIENTATION_TOPLEFT, 0);
		IIO_DEBUG("\tr = %d\n", r);
		if (!r) fail("TIFFReadRGBAImage(\"%s\") failed\n", filename);
		return 0;
	}
go_on:
	if (!broken)
		assert((int)scanline_size == sls);
	else
		assert((int)scanline_size == spp*sls);
	assert((int)scanline_size >= sls);
	uint8_t *data = xmalloc(w * h * spp * rbps * (complicated?2:1));
	uint8_t *buf = xmalloc(scanline_size);

	// use a particular reader for tiled tiff
	if (TIFFIsTiled(tif)) {
		int tisize = TIFFTileSize(tif);
		uint32_t tilewidth, tilelength;
		TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tilewidth);
		TIFFGetField(tif, TIFFTAG_TILELENGTH, &tilelength);
		IIO_DEBUG("tilewidth = %u\n", tilewidth);
		IIO_DEBUG("tilelength = %u\n", tilelength);
		IIO_DEBUG("tisize = %d (%u)\n", tisize, tilewidth*tilelength);

		if (bps < 8)
			fail("only byte-oriented tiles are supported (%d)",bps);
		int Bps = bps/8;

		IIO_DEBUG("bps = %u\n", bps);
		IIO_DEBUG("Bps = %d\n", Bps);

		uint8_t *tbuf = xmalloc(tisize*Bps*spp);
		for (uint32_t tx = 0; tx < w; tx += tilewidth)
		for (uint32_t ty = 0; ty < h; ty += tilelength)
		{
			IIO_DEBUG("tile at %u %u\n", tx, ty);
			if (!broken) {
				if (-1 == TIFFReadTile(tif, tbuf, tx, ty, 0, 0))
					memset(tbuf, -1, TIFFTileSize(tif));
			}
			for (uint16_t l = 0; l < spp; l++)
			{
			int L = l, Spp = spp;
			if (broken) {
				TIFFReadTile(tif, tbuf, tx, ty, 0, l);
				L = 0;
				Spp = 1;
			}
			for (uint32_t j = 0; j < tilelength; j++)
			for (uint32_t i = 0; i < tilewidth; i++)
			for (int b = 0; b < Bps; b++)
			{
				uint32_t ii = i + tx;
				uint32_t jj = j + ty;
				if (ii < w && jj < h)
				{
				int idx_i = ((j*tilewidth + i)*Spp + L)*Bps + b;
				int idx_o = ((jj*w + ii)*spp + l)*Bps + b;
				uint8_t s = tbuf[idx_i];
				((uint8_t*)data)[idx_o] = s;
				}
			}
			}
		}
		xfree(tbuf);
	} else {

		// dump scanline data
		if (broken && bps < 8) fail("cannot unpack broken scanlines");
		if (!broken) FORI(h) {
			r = TIFFReadScanline(tif, buf, i, 0);
			if (r < 0) fail("error read tiff row %d/%d", i, (int)h);

			if (bps < 8) {
				//fprintf(stderr,"unpacking %dth scanline\n",i);
				unpack_to_bytes_here(data + i*uscanline_size,
						buf, scanline_size, bps);
				fmt_iio = IIO_TYPE_UINT8;
			} else {
				memcpy(data + i*sls, buf, sls);
			}
		}
		else {
			int f = complicated ? 2 : 1; // bizarre case, squeeze!
			if (compression==1) FORI(h)
			{
				unsigned char *dest = data + i*spp*sls/f;
				FORJ(spp/f)
				{
					r = TIFFReadScanline(tif, buf, i, j);
					IIO_DEBUG("TIFFReadScanline (%d,%d) "
						       "=> %d\n", i, j, r);
					if (r < 0)
						fail("tiff bad %d/%d;%d (%d)",
								i, (int)h, j,f);
					memcpy(dest + j*sls, buf, sls);
				}
				if (!complicated)
					repair_broken_pixels_inplace(dest,
							w, spp, bps/8);
			} else { // compression > 1
				unsigned char *dest = data;
				fail("shit not implemented yet");
			}
		}
	}

	TIFFClose(tif);

	xfree(buf);

	// fill struct fields
	iio_image_init2d(x, w, h, spp, fmt_iio);
	x->data = data;
	return 0;
}

// Note: TIFF library only reads from a named file.  The only way to use the
// library if you have your image in a stream, is to write the data to a
// temporary file, and read it from there.  This can be optimized in the future
// by recovering the original filename through hacks, in case it was not a pipe.
static int read_beheaded_tiff(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	if (global_variable_containing_the_name_of_the_last_opened_file) {
		int r = read_whole_tiff(x,
		global_variable_containing_the_name_of_the_last_opened_file);
		if (r) fail("read whole tiff returned %d", r);
		return 0;
	}

	long filesize;
	void *filedata = load_rest_of_file(&filesize, fin, header, nheader);
	char *filename = put_data_into_temporary_file(filedata, filesize);
	xfree(filedata);

	int r = read_whole_tiff(x, filename);
	if (r) fail("read whole tiff returned %d", r);

	delete_temporary_file(filename);

	return 0;
}

#endif//I_CAN_HAS_LIBTIFF

// HDF5 reader                                                              {{{2
#ifdef I_CAN_HAS_LIBHDF5
#define H5_USE_110_API
#include <hdf5.h>

// DISCLAIMER:
//
// By writing the code below, I do not condone by any means the usage and
// proliferation of HDF5 files.  If I am adding support for them, it is because
// somebody has to extract the damn numbers from these stupid files.  I firmly
// believe that the perpetrators of the HDF5 file format and associated
// libraries should be tarred and feathered.
//

static int read_whole_hdf5(struct iio_image *x, const char *filename_raw)
{
	// The structure of a HDF5 file is the following:
	// - each file contains several datasets
	// - each dataset is accessed through a dataspace
	// - the dataspace is divided into chunks
	// - we access the dataspace by defining an hyperslab
	// - your hyperslab may (partially) overlap several chunks
	// - actually, chunks seem to be unnecessary, you can forget about them
	// - to acctually read the hyperslab you map it to a memspace
	// - the "HD5read" function reads the hyperslab data into an array
	// - the array members are of a data type (an "abstract" id)
	// - the data type belongs to a data class (int, float, date, ...)
	// - the data type is encoded into a certain order (endianness)
	// - if the data type is integer, it has a sign scheme (X's complement)
	// Thus it is really straightforward to read HDF5 images as shown below.

	IIO_DEBUG("read whole hdf5 filename_raw=\"%s\"\n", filename_raw);

	// if dataset is given as environement variable, take it
	char *dataset_id = xgetenv("IIO_HDF5_DSET");
	if (!dataset_id)
		dataset_id = "/dset";

	// if dataset is given by comma-suffix, take it
	char filename[FILENAME_MAX];
	snprintf(filename, FILENAME_MAX, "%s", filename_raw);
	char *comma = strrchr(filename, ',');
	if (comma) {
		*comma = '\0';
		dataset_id = 1 + comma;
	}

	// otherwise, try a default dataset
	if (!dataset_id)
		dataset_id = "/dset";


	IIO_DEBUG("read whole hdf5 filename=\"%s\"\n", filename);
	IIO_DEBUG("read whole hdf5 dataset=\"%s\"\n", dataset_id);

	// open the file, the dataset, and extract basic info
	hid_t       f;  // file
	hid_t       d;  // dataset
	hid_t       s;  // dataspace
	hid_t       t;  // data type
	H5T_class_t c;  // data class (yes, you are in a world of pain now)
	herr_t      e;  // error status code

	// open file
	f = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
	IIO_DEBUG("h5 f = %d\n", (int)f);

	// open dataset
	//d = H5Dopen2(f, dataset_id, H5P_DEFAULT);
	// there is not a working "open_dataset" function,
	// if you want to use it, you have to write it yourself
	hid_t my_hd5open(hid_t, char *);
	d = my_hd5open(f, dataset_id);
	IIO_DEBUG("h5 d = %d\n", (int)d);
	if (d < 0) fail("could not find dataset \"%s\" on file \"%s\"",
			dataset_id, filename);

	// extract type and class
	t = H5Dget_type(d);
	c = H5Tget_class(t);
	H5T_sign_t sgn = H5Tget_sign(t); // fml
	H5Tget_sign(t);
	IIO_DEBUG("h5 t = %d\n", (int)t);
	IIO_DEBUG("h5 c = %d\n", (int)c);
	IIO_DEBUG("h5 sgn = %d\n", (int)sgn);

	// bytes per sample
	size_t Bps = H5Tget_size(t);
	IIO_DEBUG("h5 Bps = %zu\n", Bps);

	// extract dataspace
	s = H5Dget_space(d);

	// number of dimensions of this dataspace
	int ndim = H5Sget_simple_extent_ndims(s);

	// sizes along each dimension
	hsize_t dim[ndim];
	size_t n = 1;
	e = H5Sget_simple_extent_dims(s, dim, NULL);
	IIO_DEBUG("h5 ndim = %d\n", ndim);
	for (int i = 0; i < ndim; i++)
	{
		n *= dim[i];
		IIO_DEBUG("\tdim[%d] = %d\n", i, (int)dim[i]);
	}
	IIO_DEBUG("h5 n = %d\n", (int)n);

	// extract hyperslab from within dataset
	//e = H5Sselect_hyperslab();

	void *buf = xmalloc(n * Bps);
	e = H5Dread(d, t, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
	IIO_DEBUG("h5 e(read) = %d\n", (int)e);


	// close dataset and file, whatever that means
	e = H5Dclose(d);
	e = H5Fclose(f);
	H5garbage_collect(); // it leaves many, many leaks, but still
	IIO_DEBUG("h5 e(dclose) = %d\n", (int)e);
	IIO_DEBUG("h5 e(fclose) = %d\n", (int)e);

	// identify corresponding IIO_TYPE
	int typ = -1;
	if (c==H5T_FLOAT   && Bps==4) typ = IIO_TYPE_FLOAT;
	if (c==H5T_FLOAT   && Bps==8) typ = IIO_TYPE_DOUBLE;
	if (c==H5T_INTEGER && Bps==1 && sgn==0) typ = IIO_TYPE_UINT8;
	if (c==H5T_INTEGER && Bps==2 && sgn==0) typ = IIO_TYPE_UINT16;
	if (c==H5T_INTEGER && Bps==4 && sgn==0) typ = IIO_TYPE_UINT32;
	if (c==H5T_INTEGER && Bps==1 && sgn==1) typ = IIO_TYPE_INT8;
	if (c==H5T_INTEGER && Bps==2 && sgn==1) typ = IIO_TYPE_INT16;
	if (c==H5T_INTEGER && Bps==4 && sgn==1) typ = IIO_TYPE_INT32;

	if (typ < 0 || ndim > 4)
		fail("unrecognized HDF5 c=%d Bps=%d ndim=%d\n",
				(int)c, (int)Bps, ndim);

	// identify IIO sizes (with some squeezing if necessary)
	// philosophy: data is not reordered here
	int w=1, h=1, pd=1, brk=0;
	if (ndim==2) {w=dim[1]; h=dim[0]; }
	else if (ndim==3 && dim[0]==1) { w=dim[2]; h=dim[1]; }
	else if (ndim==3 && dim[2]==1) { w=dim[1]; h=dim[0]; }
	else if (ndim==4 && dim[0]==1) { w=dim[2]; h=dim[1]; pd=dim[3]; brk=1; }
	else if (ndim==3) {w=dim[1]; h=dim[0]; pd=dim[2]; }
	else fail("h5 bad ndim=%d dim[0]=%d...", ndim, (int)dim[0]);

	IIO_DEBUG("h5 w=%d h=%d pd=%d brk=%d\n", w, h, pd, brk);

	// squeeze
	if (h == 1 && pd > 1) {h = w; w = pd; pd = 1; }
	//if (w == 1 && pd > 1) {w = pd; pd = 1; }

	// fill-in image struct
	iio_image_init2d(x, w, h, pd, typ);
	x->data = buf;
	//if (brk) repair_broken_pixels_inplace(x, w*h, pd, bytes_per_sample);
	return 0;
}

static int read_beheaded_hdf5(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	if (global_variable_containing_the_name_of_the_last_opened_file) {
		int r = read_whole_hdf5(x,
		global_variable_containing_the_name_of_the_last_opened_file);
		if (r) fail("read whole tiff returned %d", r);
		return 0;
	}

	long filesize;
	void *filedata = load_rest_of_file(&filesize, fin, header, nheader);
	char *filename = put_data_into_temporary_file(filedata, filesize);
	xfree(filedata);

	int r = read_whole_hdf5(x, filename);
	if (r) fail("read whole hdf5 returned %d", r);

	delete_temporary_file(filename);

	return 0;
}

struct twostrings { char *a, *b; };

static bool string_suffix(const char *s, const char *suf);
static herr_t find_suffix(hid_t o, const char *n, const H5O_info_t *i, void *d)
{
	(void)o;
	struct twostrings *p = d;
	if (i->type == H5O_TYPE_DATASET && string_suffix(n, p->a))
		return strncpy(p->b, n, FILENAME_MAX),1;
	return 0;
}

// the fact that this function needs to be written is unbearable
//
// Note: this function cannot be implemented using the "H5Lexists" function,
// you have to write the iterator yourself.  The function H5Lexsits only serves
// to check whether a dataset belongs to a group.  It fails (meaning, the HD5F
// library spews a lot of errors into the terminal and crashes) if you try to
// call this function using an absolute or relative path with non-existing
// intermediate components.
hid_t my_hd5open(hid_t f, char *suffix)
{
	char dset[FILENAME_MAX] = {0};

	H5O_iterate_t u = find_suffix;
	struct twostrings p = {suffix, dset};
	p.a = suffix;
	herr_t e = H5Ovisit1(f, H5_INDEX_NAME, H5_ITER_NATIVE, u, &p);
	IIO_DEBUG("H50visit1 e = %d\n", (int)e);
	if (*dset) IIO_DEBUG("HDF5_DSET = /%s\n", dset);
	return *dset ? H5Dopen2(f, dset, H5P_DEFAULT) : -1;
}

#endif//I_CAN_HAS_LIBHDF5


// WEBP reader                                                              {{{2
#ifdef I_CAN_HAS_LIBWEBP
#include <webp/decode.h>
static int read_beheaded_webp(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	long filesize;
	void *filedata = load_rest_of_file(&filesize, f, header, nheader);
	if (!filedata) return 1;

	int w, h;
	int rv = WebPGetInfo(filedata, filesize, &w, &h);
	if (!rv) return 2;

	uint8_t *data = xmalloc(4 * w * h);
	uint8_t *r = WebPDecodeRGBAInto(filedata, filesize, data, 4*w*h, 4*w);
	if (!r) return 3;

	xfree(filedata); // XXX: leak upon load failure

	iio_image_init2d(x, w, h, 4, IIO_TYPE_UINT8);
	x->data = data;
	return 0;
}
#endif//I_CAN_HAS_LIBWEBP

// AVIF reader                                                              {{{2
#ifdef I_CAN_HAS_LIBAVIF
#include <avif/avif.h>
static int read_beheaded_avif(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	// inspired by the example on libavif avif_example_decode_memory.c
	//

	long filesize;
	void *filedata = load_rest_of_file(&filesize, f, header, nheader);
	if (!filedata) return 1;

	avifRGBImage rgb;
	memset(&rgb, 0, sizeof rgb);
	avifDecoder *decoder = avifDecoderCreate();

	avifResult r = avifDecoderSetIOMemory(decoder, filedata, filesize);
	if (r != AVIF_RESULT_OK)
		fail("cannot set IO on avif (%s)", avifResultToString(r));
	r = avifDecoderParse(decoder);
	if (r != AVIF_RESULT_OK)
		fail("cannot decode avif image (%s)", avifResultToString(r));
	avifRGBImageSetDefaults(&rgb, decoder->image);
	avifRGBImageAllocatePixels(&rgb);
	r = avifImageYUVToRGB(decoder->image, &rgb);
	if (r != AVIF_RESULT_OK)
		fail("cannot YUV_to_RGB avif (%s)", avifResultToString(r));

	int w = decoder->image->width;
	int h = decoder->image->height;
	int typ = rgb.depth > 8 ? IIO_TYPE_UINT16 : IIO_TYPE_UINT8;
	iio_image_init2d(x, w, h, 4, typ);
	memcpy(x->data, rgb.pixels, iio_image_data_size(x));

	avifRGBImageFreePixels(&rgb);
	avifDecoderDestroy(decoder);
	xfree(filedata);
	return 0;
}
#endif//I_CAN_HAS_LIBAVIF

// HEIF reader                                                              {{{2
#ifdef I_CAN_HAS_LIBHEIF
#include <libheif/heif.h>
static int read_beheaded_heif(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	long filesize;
	void *filedata = load_rest_of_file(&filesize, f, header, nheader);
	if (!filedata) return 1;

	struct heif_context *ctx = heif_context_alloc();
	heif_context_read_from_memory_without_copy(ctx, filedata, filesize, 0);

	struct heif_image_handle* handle;
	heif_context_get_primary_image_handle(ctx, &handle);

	struct heif_image *img;
	heif_decode_image(handle, &img,
			heif_colorspace_RGB, heif_chroma_interleaved_RGB, 0);

	int stride;
	const uint8_t *data = heif_image_get_plane_readonly(img,
			heif_channel_interleaved, &stride);  // no alloc here

	int w = heif_image_handle_get_width(handle);
	int h = heif_image_handle_get_height(handle);

	iio_image_init2d(x, w, h, 3, IIO_TYPE_UINT8);
	x->data = xmalloc(w*h*3);
	for (int j = 0; j < h; j+=1)
	for (int i = 0; i < w*3; i+=1)
		((uint8_t *)x->data)[j*w*3+i] = data[j*stride+i];

	free(filedata);
	heif_context_free(ctx);
	heif_image_handle_release(handle);
	heif_image_release(img);
	return 0;
}
#endif//I_CAN_HAS_LIBHEIF


// QNM readers                                                              {{{2

#include <ctype.h>

static void llegeix_floats_en_bytes(FILE *don, float *on, int quants)
{
	for (int i = 0; i < quants; i++)
	{
		float c;
		c = pick_char_for_sure(don);//iw810
		on[i] = c;
	}
}

static void llegeix_floats_en_shorts(FILE *don, float *on, int quants)
{
	for (int i = 0; i < quants; i++)
	{
		float c;
		c = pick_char_for_sure(don);
		c *= 256;
		c += pick_char_for_sure(don);
		on[i] = c;
	}
}

static void llegeix_floats_en_ascii(FILE *don, float *on, int quants)
{
	for (int i = 0; i < quants; i++)
	{
		int r;
		float c;
		r = fscanf(don, "%f ", &c);
		if (r != 1)
			fail("no s'han pogut llegir %d numerets del fitxer &%p\n",
					quants, (void *)don);
		on[i] = c;
	}
}

static int read_qnm_numbers(float *data, FILE *f, int n, int m, bool use_ascii)
{
	if (use_ascii)
		llegeix_floats_en_ascii(f, data, n);
	else {
		if (m < 0x100)
			llegeix_floats_en_bytes(f, data, n);
		else if (m < 0x10000)
			llegeix_floats_en_shorts(f, data, n);
		else
			fail("too large maxval %d", m);
	}
	// TODO: error checking
	return n;
}

// qnm_types:
//  2 P2 (ascii  2d grayscale pgm)
//  5 P5 (binary 2d grayscale pgm)
//  3 P3 (ascii  2d color     ppm)
//  6 P6 (binary 2d color     ppm)
// 12 Q2 (ascii  3d grayscale pgm)
// 15 Q5 (binary 3d grayscale pgm)
// 13 Q3 (ascii  3d color     ppm)
// 16 Q6 (binary 3d color     ppm)
// 17 Q7 (ascii  3d nd           )
// 19 Q9 (binary 3d nd           )
static int read_beheaded_qnm(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	assert(nheader == 2); (void)header; (void)nheader;
	int w, h, d = 1, m, pd = 1;
	int c1 = header[0];
	int c2 = header[1] - '0';
	IIO_DEBUG("QNM reader (%c %d)...\n", c1, c2);
	eat_spaces_and_comments(f);
	if (1 != fscanf(f, "%d", &w)) return -1;
	eat_spaces_and_comments(f);
	if (1 != fscanf(f, "%d", &h)) return -2;
	if (c1 == 'Q') {
		if (1 != fscanf(f, "%d", &d)) return -3;
		eat_spaces_and_comments(f);
	}
	if (c2 == 7 || c2 == 9) {
		if (1 != fscanf(f, "%d", &pd)) return -4;
		eat_spaces_and_comments(f);
	}
	if (1 != fscanf(f, "%d", &m)) return -5;
	// maxval is ignored and the image is always read into floats
	if (!isspace(pick_char_for_sure(f))) return -6;

	bool use_ascii = (c2 == 2 || c2 == 3 || c2 == 7);
	bool use_2d = (d == 1); if (!use_2d) assert(c1 == 'Q');
	if (c2 == 3 || c2 == 6)
		pd = 3;
	size_t nn = w * h * d * pd; // number of numbers
	float *data = xmalloc(nn * sizeof*data);
	IIO_DEBUG("QNM reader w = %d\n", w);
	IIO_DEBUG("QNM reader h = %d\n", h);
	IIO_DEBUG("QNM reader d = %d\n", d);
	IIO_DEBUG("QNM reader pd = %d\n", pd);
	IIO_DEBUG("QNM reader m = %d\n", m);
	IIO_DEBUG("QNM reader use_2d = %d\n", use_2d);
	IIO_DEBUG("QNM reader use_ascii = %d\n", use_ascii);
	int r = read_qnm_numbers(data, f, nn, m, use_ascii);
	if (nn - r) return (xfree(data),-7);

	iio_image_init2d(x, w, h, pd, IIO_TYPE_FLOAT);
	x->dimension = use_2d ? 2 : 3;
	if (d > 1) x->sizes[2] = d;
	x->data = data;
	return 0;
}

// PCM reader                                                               {{{2
// PCM is a file format to store complex float images
// it is used by some people also for optical flow fields
static int read_beheaded_pcm(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	(void)header;
	assert(nheader == 2);
	int w, h;
	float scale;
	if (1 != fscanf(f, " %d", &w)) return -1;
	if (1 != fscanf(f, " %d", &h)) return -2;
	if (1 != fscanf(f, " %g", &scale)) return -3;
	if (!isspace(pick_char_for_sure(f))) return -6;

	fprintf(stderr, "%d PCM w h scale = %d %d %g\n", nheader, w, h, scale);

	assert(sizeof(float) == 4);
	float *data = xmalloc(w * h * 2 * sizeof(float));
	int r = fread(data, sizeof(float), w * h * 2, f);
	if (r != w * h * 2) return (xfree(data),-7);
	iio_image_init2d(x, w, h, 2, IIO_TYPE_FLOAT);
	x->data = data;
	return 0;
}


// RIM reader                                                               {{{2
// (megawave's image format)

// documentation of this format:
//
// CIMAGE header bytes: ("IMG" format)
// short 0: 'MI'
// short 1: comment length
// short 2: image width
// short 3: image height
//
//
// FIMAGE header bytes: ("RIM" format)
// short 0: 'IR'
// short 1: comment length
// short 2: image width
// short 3: image height
//
//
// CCIMAGE header bytes: ("MTI" format)
//
//
//

static uint16_t rim_getshort(FILE *f, bool swp)
{
	int a = pick_char_for_sure(f);
	int b = pick_char_for_sure(f);
	IIO_DEBUG("rgs %.2x%.2x\n", a, b);
	assert(a >= 0);
	assert(b >= 0);
	assert(a < 256);
	assert(b < 256);
	uint16_t r = swp ? b * 0x100 + a : a * 0x100 + b;
	return r;
}

// Note: different order than for shorts.
// Fascinating braindeadness.
static uint32_t rim_getint(FILE *f, bool swp)
{
	int a = pick_char_for_sure(f);
	int b = pick_char_for_sure(f);
	int c = pick_char_for_sure(f);
	int d = pick_char_for_sure(f);
	IIO_DEBUG("rgi %.2x%.2x %.2x%.2x\n", a, b, c, d);
	assert(a >= 0); assert(b >= 0); assert(c >= 0); assert(d >= 0);
	assert(a < 256); assert(b < 256); assert(c < 256); assert(d < 256);
	uint32_t r = swp ?
		a*0x1000000 + b*0x10000 + c*0x100 + d:
		d*0x1000000 + c*0x10000 + b*0x100 + a;
	return r;

}

static int read_beheaded_rim_cimage(struct iio_image *x, FILE *f, bool swp)
{
	uint16_t lencomm = rim_getshort(f, swp);
	uint16_t dx = rim_getshort(f, swp);
	uint16_t dy = rim_getshort(f, swp);
	IIO_DEBUG("RIM READER lencomm = %d\n", (int)lencomm);
	IIO_DEBUG("RIM READER dx = %d\n", (int)dx);
	IIO_DEBUG("RIM READER dy = %d\n", (int)dy);
	FORI(28) rim_getshort(f, swp); // skip shit (ascii numbers and zeros)
	FORI(lencomm) {
		int c = pick_char_for_sure(f); // skip further shit (comments)
		(void)c;
		IIO_DEBUG("RIM READER comment[%d] = '%c'\n", i, c);
	}
	float *data = xmalloc(dx * dy);
	size_t r = fread(data, 1, dx*dy, f);
	if (r != (size_t)(dx*dy))
		fail("could not read entire RIM file:\n"
				"expected %zu chars, but got only %zu",
				(size_t)dx*dy, r);
	int s[2] = {dx, dy};
	iio_wrap_image_struct_around_data(x, 2, s, 1, IIO_TYPE_UINT8, data);
	return 0;
}

static void byteswap4(void *data, int n)
{
	char *t = data;
	FORI(n) {
		char *t4 = t + 4*i;
		char tt[4] = {t4[3], t4[2], t4[1], t4[0]};
		FORL(4) t4[l] = tt[l];
	}
}

static int read_beheaded_rim_fimage(struct iio_image *x, FILE *f, bool swp)
{
	IIO_DEBUG("rim reader fimage swp = %d", swp);
	uint16_t lencomm = rim_getshort(f, swp);
	uint16_t dx = rim_getshort(f, swp);
	uint16_t dy = rim_getshort(f, swp);
	FORI(28) rim_getshort(f, swp); // skip shit (ascii numbers and zeros)
	FORI(lencomm) {
		int c = pick_char_for_sure(f); // skip further shit (comments)
		(void)c;
	}
	// now, read dx*dy floats
	float *data = xmalloc(dx * dy * sizeof*data);
	size_t r = fread(data, sizeof*data, dx*dy, f);
	if (r != (size_t)(dx*dy))
		fail("could not read entire RIM file:\n"
				"expected %zu floats, but got only %zu",
				(size_t)dx*dy, r);
	assert(sizeof(float) == 4);
	if (swp) byteswap4(data, r);
	int s[2] = {dx, dy};
	iio_wrap_image_struct_around_data(x, 2, s, 1, IIO_TYPE_FLOAT, data);
	return 0;
}

static int read_beheaded_rim_ccimage(struct iio_image *x, FILE *f, bool swp)
{
	uint16_t iv = rim_getshort(f, swp);
	if (iv != 0x4956 && iv != 0x5649 && iv != 0x4557 && iv != 0x5745)
		fail("bad ccimage header %x",(int)iv);
	uint32_t pm_np = rim_getint(f, swp);
	uint32_t pm_nrow = rim_getint(f, swp);
	uint32_t pm_ncol = rim_getint(f, swp);
	uint32_t pm_band = rim_getint(f, swp);(void)pm_band;
	uint32_t pm_form = rim_getint(f, swp);
	uint32_t pm_cmtsize = rim_getint(f, swp);(void)pm_cmtsize;

	IIO_DEBUG("RIM READER pm_np = %d\n", (int)pm_np);
	IIO_DEBUG("RIM READER pm_nrow = %d\n", (int)pm_nrow);
	IIO_DEBUG("RIM READER pm_ncol = %d\n", (int)pm_ncol);
	IIO_DEBUG("RIM READER pm_band = %d\n", (int)pm_band);
	IIO_DEBUG("RIM READER pm_form = %x\n", (int)pm_form);
	IIO_DEBUG("RIM READER pm_cmtsize = %d\n", (int)pm_cmtsize);
	uint32_t nsamples = pm_np * pm_nrow * pm_ncol;
	if (pm_form == 0x8001) { // ccimage
		uint8_t *data = xmalloc(nsamples);
		size_t r = fread(data, 1, nsamples, f);
		if (r != nsamples) fail("rim reader failed at reading %zu "
			"samples (got only %zu)\n", (size_t)nsamples, r);
		uint8_t *good_data = xmalloc(nsamples);
		FORJ(pm_nrow) FORI(pm_ncol) FORL(pm_np)
			good_data[l+(i+j*pm_ncol)*pm_np] =
			data[i + j*pm_ncol + l*pm_ncol*pm_nrow];
		xfree(data); data = good_data;
		int s[2] = {pm_ncol, pm_nrow};
		iio_wrap_image_struct_around_data(x, 2, s, pm_np, IIO_TYPE_UINT8, data);
	} else if (pm_form == 0xc004) { // cfimage
		float *data = xmalloc(4*nsamples);
		size_t r = fread(data, 4, nsamples, f);
		if (r != nsamples) fail("rim reader failed at reading %zu "
			"samples (got only %zu)\n", (size_t)nsamples, r);
		float *good_data = xmalloc(4*nsamples);
		FORJ(pm_nrow) FORI(pm_ncol) FORL(pm_np)
			good_data[l+(i+j*pm_ncol)*pm_np] =
			data[i + j*pm_ncol + l*pm_ncol*pm_nrow];
		xfree(data); data = good_data;
		int s[2] = {pm_ncol, pm_nrow};
		iio_wrap_image_struct_around_data(x, 2, s, pm_np, IIO_TYPE_FLOAT, data);
	} else
		fail("unsupported PM_form %x", pm_form);
	return 0;
}

static int read_beheaded_rim(struct iio_image *x, FILE *f, char *h, int nh)
{
	assert(nh == 2); (void)nh;
	if (h[0] == 'I' && h[1] == 'R')
		return read_beheaded_rim_fimage(x, f, false);
	if (h[0] == 'R' && h[1] == 'I')
		return read_beheaded_rim_fimage(x, f, true);

	if (h[0] == 'M' && h[1] == 'I')
		return read_beheaded_rim_cimage(x, f, false);
	if (h[0] == 'I' && h[1] == 'M')
		return read_beheaded_rim_cimage(x, f, true);

	if (h[0] == 'W' && h[1] == 'E')
		return read_beheaded_rim_ccimage(x, f, false);
	if (h[0] == 'V' && h[1] == 'I')
		return read_beheaded_rim_ccimage(x, f, true);
	return 1;
}

static void switch_2endianness(void *tt, int n)
{
	char *t = tt;
	FORI(n) {
		char tmp[2] = {t[0], t[1]};
		t[0] = tmp[1];
		t[1] = tmp[0];
		t += 2;
	}
}
static void switch_4endianness(void *tt, int n)
{
	char *t = tt;
	FORI(n) {
		char tmp[4] = {t[0], t[1], t[2], t[3]};
		t[0] = tmp[3];
		t[1] = tmp[2];
		t[2] = tmp[1];
		t[3] = tmp[0];
		t += 4;
	}
}
static void switch_8endianness(void *tt, int n)
{
	char *t = tt;
	FORI(n) {
		char tmp[8] = {t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7]};
		t[0] = tmp[7];
		t[1] = tmp[6];
		t[2] = tmp[5];
		t[3] = tmp[4];
		t[4] = tmp[3];
		t[5] = tmp[2];
		t[6] = tmp[1];
		t[7] = tmp[0];
		t += 8;
	}
}

// PFM reader                                                               {{{2
static int read_beheaded_pfm(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	assert(4 == sizeof(float));
	assert(nheader == 2); (void)nheader;
	assert('f' == tolower(header[1]));
	int w, h, pd = isupper(header[1]) ? 3 : 1;
	float scale;
	if (!isspace(pick_char_for_sure(f))) return -1;
	if (3 != fscanf(f, "%d %d\n%g", &w, &h, &scale)) return -2;
	if (!isspace(pick_char_for_sure(f))) return -3;
	float *data = xmalloc(w*h*4*pd);
	if (1 != fread(data, w*h*4*pd, 1, f)) return (xfree(data),-4);

	iio_image_init2d(x, w, h, 2, IIO_TYPE_FLOAT);
	x->data = data;
	return 0;
}

// FLO reader                                                               {{{2
static int read_beheaded_flo(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	(void)header; (void)nheader;
	int w = rim_getint(f, false);
	int h = rim_getint(f, false);
	float *data = xmalloc(w*h*2*sizeof*data);
	if (1 != fread(data, w*h*4*2, 1, f)) return (xfree(data),-1);

	iio_image_init2d(x, w, h, 2, IIO_TYPE_FLOAT);
	x->data = data;
	return 0;
}

// JUV reader                                                               {{{2
static int read_beheaded_juv(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	char buf[255];
	FORI(nheader) buf[i] = header[i];
	FORI(255-nheader) buf[i+nheader] = pick_char_for_sure(f);
	int w, h, r = sscanf(buf, "#UV {\n dimx %d dimy %d\n}\n", &w, &h);
	if (r != 2) return -1;
	size_t sf = sizeof(float);
	float *u = xmalloc(w*h*sf); r = fread(u, sf, w*h, f);
	if(r != w*h) return (xfree(u),-2);
	float *v = xmalloc(w*h*sf); r = fread(v, sf, w*h, f);
	if(r != w*h) return (xfree(u),xfree(v),-2);
	float *uv = xmalloc(2*w*h*sf);
	FORI(w*h) uv[2*i] = u[i];
	FORI(w*h) uv[2*i+1] = v[i];
	xfree(u); xfree(v);
	iio_image_init2d(x, w, h, 2, IIO_TYPE_FLOAT);
	x->data = uv;
	return 0;
}

// LUM reader                                                               {{{2

static int lum_pickshort(char *ss)
{
	uint8_t *s = (uint8_t*)ss;
	int a = s[0];
	int b = s[1];
	return 0x100*a + b;
}

static int read_beheaded_lum12(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	int w = *(uint32_t*)header;
	int h = *(uint32_t*)(header+4);
	while (nheader++ < 11968)
		pick_char_for_sure(f);
	uint16_t *data = xmalloc(w*h*sizeof*data);
	if (1 != fread(data, w*h*2, 1, f)) return (xfree(data),-1);
	iio_image_init2d(x, w, h, 1, IIO_TYPE_UINT16);
	x->data = data;
	return 0;
}

static int read_beheaded_lum(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	if (header[8] == '1')
		return read_beheaded_lum12(x, f, header, nheader);
	int w = lum_pickshort(header+2);
	int h = lum_pickshort(header+6);
	while (nheader++ < 0xf94)
		pick_char_for_sure(f);
	float *data = xmalloc(w*h*sizeof*data);
	if (1 != fread(data, w*h*4, 1, f)) return (xfree(data),-1);
	switch_4endianness(data, w*h);
	iio_image_init2d(x, w, h, 1, IIO_TYPE_FLOAT);
	x->data = data;
	return 0;
}

// BMP reader                                                               {{{2
static int read_beheaded_bmp(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	long len;
	char *bmp = load_rest_of_file(&len, f, header, nheader);
	(void)bmp; (void)x;

	fail("BMP reader not yet written");

	xfree(bmp);

	return 0;
}


// EXR reader                                                               {{{2

#ifdef I_CAN_HAS_LIBEXR
#include <OpenEXR/ImfCRgbaFile.h>
// EXTERNALIZED TO :  read_exr_float.cpp

static int read_whole_exr(struct iio_image *x, const char *filename)
{
	struct ImfInputFile *f = ImfOpenInputFile(filename);
	if (!f) fail("could not read exr from %s", filename);
	int r;

	const char *nom = ImfInputFileName(f);
	IIO_DEBUG("ImfInputFileName returned %s\n", nom);

	r = ImfInputChannels(f);
	IIO_DEBUG("ImfInputChannels returned %d\n", r);

	const struct ImfHeader *header = ImfInputHeader(f);
	int xmin, ymin, xmax, ymax;
	ImfHeaderDataWindow(header, &xmin, &ymin, &xmax, &ymax);
	IIO_DEBUG("xmin ymin xmax ymax = %d %d %d %d\n", xmin,ymin,xmax,ymax);

	int width = xmax - xmin + 1;
	int height = ymax - ymin + 1;
	struct ImfRgba *data = xmalloc(width*height*sizeof*data);

	r = ImfInputSetFrameBuffer(f, data, 1, width);
	IIO_DEBUG("ImfInputSetFrameBuffer returned %d\n", r);

	r = ImfInputReadPixels(f, ymin, ymax);
	IIO_DEBUG("ImfInputReadPixels returned %d\n", r);

	r = ImfCloseInputFile(f);
	IIO_DEBUG("ImfCloseInputFile returned %d\n", r);

	float *finaldata = xmalloc(4*width*height*sizeof*data);
	FORI(width*height) {
		finaldata[4*i+0] = ImfHalfToFloat(data[i].r);
		finaldata[4*i+1] = ImfHalfToFloat(data[i].g);
		finaldata[4*i+2] = ImfHalfToFloat(data[i].b);
		finaldata[4*i+3] = ImfHalfToFloat(data[i].a);
	}
	xfree(data);

	// fill struct fields
	iio_image_init2d(x, w, h, 4, IIO_TYPE_FLOAT);
	x->data = finaldata;
	return 0;
}

// Note: OpenEXR library only reads from a named file.  The only way to use the
// library if you have your image in a stream, is to write the data to a
// temporary file, and read it from there.  This can be optimized in the future
// by recovering the original filename through hacks, in case it was not a pipe.
static int read_beheaded_exr(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	if (global_variable_containing_the_name_of_the_last_opened_file) {
		int r = read_whole_exr(x,
		global_variable_containing_the_name_of_the_last_opened_file);
		if (r) fail("read whole tiff returned %d", r);
		return 0;
	}

	long filesize;
	void *filedata = load_rest_of_file(&filesize, fin, header, nheader);
	char *filename = put_data_into_temporary_file(filedata, filesize);
	xfree(filedata);

	int r = read_whole_exr(x, filename);
	if (r) fail("read whole exr returned %d", r);

	delete_temporary_file(filename);

	return 0;
}

#endif//I_CAN_HAS_LIBEXR



// ASC reader                                                               {{{2
static int read_beheaded_asc(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	long filesize;
	char *filedata = load_rest_of_file(&filesize, f, header, nheader);
	int n[5], r = sscanf(filedata, "%d %d %d %d\n%n", n, n+1, n+2, n+3,n+4);
	if (r != 4) return 1;
	x->dimension = 2;
	x->sizes[0] = n[0];
	x->sizes[1] = n[1];
	x->sizes[2] = n[2];
	if (n[2] > 1)
		x->dimension = 3;
	x->pixel_dimension = n[3];
	x->type = IIO_TYPE_FLOAT;
	int nsamples = iio_image_number_of_samples(x);
	float *xdata = xmalloc(nsamples * sizeof*xdata);
	IIO_DEBUG("asc %d,%d,%d,%d\n", n[0], n[1], n[2], n[3]);

	// read data
	float *numbers = xdata;
	char *delim = " \n", *tok = strtok(filedata + n[4], delim);
	while (tok && numbers < (float*)(xdata)+nsamples)
	{
		*numbers++ = atof(tok);
		tok = strtok(NULL, delim);
	}
	free(filedata);

	x->data = xmalloc(nsamples * sizeof*xdata);
	recover_broken_pixels_float(x->data, xdata, n[0]*n[1]*n[2], n[3]);
	xfree(xdata);
	x->rem = NULL;
	return 0;

	//(void)nheader;
	////assert(header[nheader-1] == '\n');
	//int n[4], r = sscanf(header, "%d %d %d %d\n", n, n+1, n+2, n+3);
	//if (r != 4) return 1;
	//x->dimension = 2;
	//x->sizes[0] = n[0];
	//x->sizes[1] = n[1];
	//x->sizes[2] = n[2];
	//if (n[2] > 1)
	//	x->dimension = 3;
	//x->pixel_dimension = n[3];
	//x->type = IIO_TYPE_FLOAT;
	//int nsamples = iio_image_number_of_samples(x);
	//float *xdata = xmalloc(nsamples * sizeof*xdata);
	//IIO_DEBUG("asc %d,%d,%d,%d\n", n[0], n[1], n[2], n[3]);
	//read_qnm_numbers(xdata, f, nsamples, 0, true);
	//x->data = xmalloc(nsamples * sizeof*xdata);
	//recover_broken_pixels_float(x->data, xdata, n[0]*n[1]*n[2], n[3]);
	//xfree(xdata);
	//x->contiguous_data = false;
	//return 0;
}

// PDS reader                                                               {{{2

// read a line of text until either
// 	- n characters are read
// 	- a newline character is found
// 	- a zero character is found
// 	- the end of file is reached
// returns the number of read characters, not including the end zero
// Calling this functions should always result in a valid string on l
static int getlinen(char *l, int n, FILE *f)
{
	int c, i = 0;
	while (i < n-1 && (c = fgetc(f)) != EOF && c != '\n' && c)
		if (isprint(c))
			l[i++] = c;
	if (c == EOF) return -1;
	l[i] = '\0';
	return i;
}

// parse a line of the form "KEY = VALUE"
static void pds_parse_line(char *key, char *value, char *line)
{
	int r = sscanf(line, "%s = %s\n", key, value);
	if (r != 2) {
		*key = *value = '\0'; return; }
	IIO_DEBUG("PARSED \"%s\" = \"%s\"\n", key, value);
}

#ifndef SAMPLEFORMAT_UINT
// definitions from tiff.h, needed also for pds
#define SAMPLEFORMAT_UINT  1
#define SAMPLEFORMAT_INT  2
#define SAMPLEFORMAT_IEEEFP  3
#define SAMPLEFORMAT_VOID  4
#define SAMPLEFORMAT_COMPLEXINT 5
#define SAMPLEFORMAT_COMPLEXIEEEFP 6
#endif//SAMPLEFORMAT_UINT

static int read_beheaded_pds(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	(void)header; (void)nheader;
	// check that the file is named, and not a pipe
	//const char *fn;
	//fn = global_variable_containing_the_name_of_the_last_opened_file;
	//if (!fn)
	//	return 1;

	// get an object name, if different to "^IMAGE"
	char *object_id = xgetenv("IIO_PDS_OBJECT");
	if (!object_id)
		object_id = "^IMAGE";

	// parse the header and obtain the image dimensions and type name
	int n, nmax = 1000, cx = 0;
	char line[nmax], key[nmax], value[nmax];
	int rbytes = -1, w = -1, h = -1, spp = 1, bps = 1, obj = -1;
	int crop_left = 0, crop_right = 0;
	int sfmt = SAMPLEFORMAT_UINT;
	bool in_object = false;
	bool flip_h = false, flip_v = false, allturn = false;
	while ((n = getlinen(line, nmax, f)) >= 0  && cx++ < nmax)
	{
		pds_parse_line(key, value, line);
		if (!*key || !*value) continue;
		IIO_DEBUG("PDS \"%s\" = \"%s\"\n", key, value);
		if (!strcmp(key, "RECORD_BYTES")) rbytes = atoi(value);
		if (!strcmp(key, "RECORD_TYPE"))
			if (strstr(value, "UNDEFINED")) rbytes = 1;
		if (!strcmp(key, object_id))      obj = atoi(value);
		if (!strcmp(key, "OBJECT") && !strcmp(value, object_id+1))
			in_object = true;
		if (!in_object) continue;
		if (!strcmp(key, "LINES"))        h = atoi(value);
		if (!strcmp(key, "LINE_SAMPLES")) w = atoi(value);
		if (!strcmp(key, "SAMPLE_BITS"))  bps = atoi(value);
		if (!strcmp(key, "BANDS"))        spp = atoi(value);
		if (!strcmp(key, "SAMPLE_TYPE")) {
			if (strstr(value, "REAL")) sfmt = SAMPLEFORMAT_IEEEFP;
			if (strstr(value, "UNSIGNED")) sfmt = SAMPLEFORMAT_UINT;
			if (strstr(value, "INTEGER"))sfmt=SAMPLEFORMAT_UINT;
		}
		if (!strcmp(key, "SAMPLE_DISPLAY_DIRECTION"))
			flip_h = allturn !=! strcmp(value, "RIGHT");
		if (!strcmp(key, "LINE_DISPLAY_DIRECTION"))
			flip_v = allturn !=! strcmp(value, "DOWN");
		if (!strcmp(key, "LINE_PREFIX_BYTES")) crop_left = atoi(value);
		if (!strcmp(key, "LINE_SUFFIX_BYTES")) crop_right = atoi(value);
		// TODO: support the 8 possible rotations and orientations
		// (RAW-equivalents: xy xY Xy XY yx yX Yx YX)
		if (!strcmp(key, "END_OBJECT") && !strcmp(value, object_id+1))
			break;
	}
	w = w + crop_left + crop_right;

	IIO_DEBUG("rbytes = %d\n", rbytes);
	IIO_DEBUG("object_id = %s\n", object_id);
	IIO_DEBUG("obj = %d\n", obj);
	IIO_DEBUG("w = %d\n", w);
	IIO_DEBUG("h = %d\n", h);
	IIO_DEBUG("bps = %d\n", bps);
	IIO_DEBUG("spp = %d\n", spp);
	IIO_DEBUG("sfmt = %d\n", sfmt);

	// identify the sample type
	int typ = -1;
	if (sfmt==SAMPLEFORMAT_IEEEFP && bps==32) typ = IIO_TYPE_FLOAT;
	if (sfmt==SAMPLEFORMAT_IEEEFP && bps==64) typ = IIO_TYPE_DOUBLE;
	if (sfmt==SAMPLEFORMAT_UINT && bps==8)    typ = IIO_TYPE_UINT8;
	if (sfmt==SAMPLEFORMAT_UINT && bps==16)   typ = IIO_TYPE_UINT16;
	if (sfmt==SAMPLEFORMAT_UINT && bps==32)   typ = IIO_TYPE_UINT32;
	assert(typ > 0);

	// fill-in the image struct
	iio_image_init2d(x, w, h, spp, typ);

	// alloc memory for image data
	int size = w * h * spp * (bps/8);
	x->data = xmalloc(size);

	// read data
	n = fseek(f, rbytes * (obj - 1) , SEEK_SET);
	if (n) { free(x->data); return 2; }
	n = fread(x->data, size, 1, f);
	if (n != 1) { free(x->data); return 3; }

	// if necessary, transpose and trim data
	if (flip_v) inplace_flip_vertical(x);
	if (flip_h) inplace_flip_horizontal(x);
	inplace_trim(x, crop_left, 0, crop_right, 0);

	// return
	return 0;
}

// CSV reader                                                               {{{2

static int read_beheaded_csv(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	// load whole file
	long filesize;
	char *filedata = load_rest_of_file(&filesize, fin, header, nheader);

	// height = number of newlines
	int h = 0;
	for (int i = 0 ; i < filesize; i++) if (filedata[i] == '\n') h += 1;

	// width = ( number of commas  + h ) / h
	int nc = 0;
	for (int i = 0 ; i < filesize; i++) if (filedata[i] == ',') nc += 1;
	int w = nc / h + 1;

	// fill-in the image struct
	iio_image_init2d(x, w, h, 1, IIO_TYPE_FLOAT);

	// alloc memory for image data
	int size = w * h * sizeof(float);
	x->data = xmalloc(size);
	float *numbers = x->data;

	// read data
	char *delim = ",\n", *tok = strtok(filedata, delim);
	while (tok && numbers < (float*)(x->data)+w*h)
	{
		*numbers++ = atof(tok);
		tok = strtok(NULL, delim);
	}

	// cleanup and exit
	free(filedata);
	return 0;
}

// TXT reader                                                               {{{2

static int read_beheaded_txt(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	// load whole file
	long filesize;
	char *filedata = load_rest_of_file(&filesize, fin, header, nheader);

	// TODO: generalize to three and four dimensions using different
	// separators (space, tab, newline, (two newlines?))

	// height = number of newlines
	int h = 0;
	for (int i = 0 ; i < filesize; i++) if (filedata[i] == '\n') h += 1;

	// width = ( number of spaces  + h ) / h
	int nc = 0;
	for (int i = 0 ; i < filesize; i++) if (filedata[i] == ' ') nc += 1;
	int w = nc / h + 1;

	// fill-in the image struct
	iio_image_init2d(x, w, h, 1, IIO_TYPE_FLOAT);

	// alloc memory for image data
	int size = w * h * sizeof(float);
	x->data = xmalloc(size);
	float *numbers = x->data;

	// read data
	char *delim = " \n", *tok = strtok(filedata, delim);
	while (tok && numbers < (float*)(x->data)+w*h)
	{
		*numbers++ = atof(tok);
		tok = strtok(NULL, delim);
	}

	// cleanup and exit
	free(filedata);
	return 0;
}

// DLM reader                                                               {{{2

static int read_beheaded_dlm(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	(void)x;
	(void)fin;
	(void)header;
	(void)nheader;
	fail("dlm reader not implemented, use csv by now\n");
	return 1;
}


// VRT reader                                                               {{{2
//
// VRT = GDAL Virtual images are a text file describing the relative
// position of several tiles, specified by their filenames.  The idea is neat
// but the format itself is an abomination based on wrong misconceptions.
// Here we provide a minimal implementation for some common cases.

static int xml_get_numeric_attr(int *out, char *line, char *tag, char *attr)
{
	if (!strstr(line, tag)) return 0;
	line = strstr(line, attr);
	if (!line) return 0;
	*out = atoi(line + 2 + strlen(attr));
	return 1;
}

static int xml_get_tag_content(char *out, char *line, char *tag)
{
	int n = strlen(line);
	char fmt[n], tmp[n];
	snprintf(fmt, n, " <%s %%*[^>]>%%[^<]s</%s>", tag, tag);
	int r = sscanf(line, fmt, tmp);
	if (r != 1) return 0;
	memcpy(out, tmp, 1+strlen(tmp));
	return 1;
}


float *iio_read_image_float(const char *fname, int *w, int *h); // forward
static int read_beheaded_vrt(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	(void)header; (void)nheader;
	int n = FILENAME_MAX + 0x200, cx = 0, w = 0, h = 0;
	char fname[n], dirvrt[n], fullfname[n], line[n], *sl = fgets(line, n, fin);
	if (!sl) return 1;
	cx += xml_get_numeric_attr(&w, line, "Dataset", "rasterXSize");
	cx += xml_get_numeric_attr(&h, line, "Dataset", "rasterYSize");
	if (!w || !h) return 2;
	if (cx != 2) return 3;
	iio_image_init2d(x, w, h, 1, IIO_TYPE_FLOAT);
	x->data = xmalloc(w * h * sizeof(float));
	float (*xx)[w] = x->data;
	int pos[4] = {0,0,0,0}, pos_cx = 0, has_fname = 0;

	// obtain the path where the vrt file is located
	strncpy(dirvrt, global_variable_containing_the_name_of_the_last_opened_file, n-1);
	char* dirvrt2 = dirname(dirvrt);

	while (1) {
		sl = fgets(line, n, fin);
		if (!sl) break;
		pos_cx += xml_get_numeric_attr(pos+0, line, "DstRect", "xOff");
		pos_cx += xml_get_numeric_attr(pos+1, line, "DstRect", "yOff");
		pos_cx += xml_get_numeric_attr(pos+2, line, "DstRect", "xSize");
		pos_cx += xml_get_numeric_attr(pos+3, line, "DstRect", "ySize");
		has_fname += xml_get_tag_content(fname, line, "SourceFilename");
		if (pos_cx == 4 && has_fname == 1)
		{
			pos_cx = has_fname = 0;
			int wt, ht;
			snprintf(fullfname,FILENAME_MAX,"%s/%s",dirvrt2,fname);
			float *xt = iio_read_image_float(fullfname, &wt, &ht);
			if (!xt) return 4;
			for (int j = 0; j < pos[3]; j++)
			for (int i = 0; i < pos[2]; i++)
			{
				int ii = i + pos[0];
				int jj = j + pos[1];
				if (insideP(w,h, ii,jj) && insideP(wt,ht, i,j))
					xx[jj][ii] = xt[j*wt+i];
			}
			xfree(xt);
		}
	}
	return 0;
}

// FARBFELD reader                                                          {{{2
static int read_beheaded_ffd(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	(void)header; (void)nheader;
	for (int i = 0; i < 4; i++)
		pick_char_for_sure(fin);
	int s[8];
	for (int i = 0; i < 8; i++)
		s[i] = pick_char_for_sure(fin);
	uint32_t w = s[3] + 0x100 * s[2] + 0x10000 * s[1] + 0x1000000 * s[0];
	uint32_t h = s[7] + 0x100 * s[6] + 0x10000 * s[5] + 0x1000000 * s[4];

	iio_image_init2d(x, w, h, 4, IIO_TYPE_UINT16);
	x->data = xmalloc(w * h * 4 * sizeof(uint16_t));
	uint32_t r = fread(x->data, 2, w*h*4, fin);
	if (r != w*h*4) return 1;
	switch_2endianness(x->data, r);
	return 0;
}

// RAT reader                                                               {{{2
static int read_beheaded_rat(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	(void)header; (void)nheader;
	assert(nheader == 4);
	int fullb = 30; // includes size of dummy RAT header fields
	char s[4+4*fullb]; // whole header (mostly unused)
	for (int i = 4; i < 4 + 4*fullb; i++)
		s[i] = pick_char_for_sure(fin);
	int32_t d = *(int32_t*)(s + 4*2);
	int32_t w = *(int32_t*)(s + 4*3);
	int32_t h = *(int32_t*)(s + 4*4);
	int32_t var = *(int32_t*)(s + 4*5);

	IIO_DEBUG("RAT d = %d\n", d);
	IIO_DEBUG("RAT w = %d\n", w);
	IIO_DEBUG("RAT h = %d\n", h);
	IIO_DEBUG("RAT var = %d\n", var);

	if (var != 6)
		fail("RAR option var=%d not implemented; ask enric", var);

	iio_image_init2d(x, w, h, 2*d*d, IIO_TYPE_FLOAT);
	long total = w * h * d * d * 2;
	x->data = xmalloc(total * sizeof(float));
	long r = fread(x->data, sizeof(float), total, fin);
	if (r != total) fail("total=%ld r=%ld\n", total, r);//return 1;
	return 0;
}

// NUMPY reader                                                             {{{2
static int read_beheaded_npy(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	(void)header;
	assert(nheader == 4);
	int s[6]; // remaining part of the fixed-size header
	for (int i = 0; i < 6; i++)
		s[i] = pick_char_for_sure(fin);
	if (s[2] != 1 || s[3] != 0) return 1; // only NPY 1.0 is supported
	int npy_header_size = s[4] + 0x100 * s[5];
	char npy_header[npy_header_size]; // variable-size header
	for (int i = 0; i < npy_header_size; i++)
		npy_header[i] = pick_char_for_sure(fin);

	// extract fields from the npy header
	char descr[10];
	char order[10];
	int w, h, pd, n;
	n = sscanf(npy_header, "{'descr': '%[^']', 'fortran_order': %[^,],"
			" 'shape': (%d, %d, %d", descr, order, &h, &w, &pd);
	if (n < 5) pd = 1;
	if (n < 4) h = 1;
	if (n < 3) return 2; // badly formed npy header

	if (h==1 && w>1 && pd>1) // squeeze
	{
		h = w;
		w = pd;
		pd = 1;
	}

	if (order[0] == 'T') // fortran_order == True
	{
		int t = h;
		h = w;
		w = t;
	}

	IIO_DEBUG("npy descr = %s\n", descr);
	IIO_DEBUG("npy order = %s\n", order);
	IIO_DEBUG("npy w = %d\n", w);
	IIO_DEBUG("npy h = %d\n", h);
	IIO_DEBUG("npy pd = %d\n", pd);

	// parse type string
	char *desc = descr; // pointer to the bare description
	if (*descr=='<' || *descr=='>' || *descr=='=' || *descr=='|')
		desc += 1;
	if (false) ;
	else if (0 == strcmp(desc, "f8")) x->type = IIO_TYPE_DOUBLE;
	else if (0 == strcmp(desc, "f4")) x->type = IIO_TYPE_FLOAT;
	//else if (0 == strcmp(desc, "e")) x->type = IIO_TYPE_HALF;
	else if (0 == strcmp(desc, "u1")) x->type = IIO_TYPE_UINT8;
	else if (0 == strcmp(desc, "u2")) x->type = IIO_TYPE_UINT16;
	else if (0 == strcmp(desc, "u4")) x->type = IIO_TYPE_UINT32;
	else if (0 == strcmp(desc, "u8")) x->type = IIO_TYPE_UINT64;
	else if (0 == strcmp(desc, "i1")) x->type = IIO_TYPE_INT8;
	else if (0 == strcmp(desc, "i2")) x->type = IIO_TYPE_INT16;
	else if (0 == strcmp(desc, "i4")) x->type = IIO_TYPE_INT32;
	else if (0 == strcmp(desc, "i8")) x->type = IIO_TYPE_INT64;
	else if (0 == strcmp(desc, "c8")) x->type = IIO_TYPE_FLOAT;
	else if (0 == strcmp(desc, "c16")) x->type = IIO_TYPE_DOUBLE;
	else if (0 == strcmp(desc, "b1")) x->type = IIO_TYPE_INT8;
	else if (0 == strcmp(desc, "B1")) x->type = IIO_TYPE_UINT8;
	else return fprintf(stderr,
			"IIO ERROR: unrecognized npy type \"%s\"\n", desc); 
	if (*desc == 'c') pd *= 2; // 1 complex = 2 reals

	// fill image struct
	iio_image_init2d(x, w, h, pd, x->type);
	size_t bps = iio_type_size(x->type);
	IIO_DEBUG("bps = %d\n", (int)bps);
	x->data = xmalloc(((bps * w) * h) * pd);
	IIO_DEBUG("data = %p\n", (void*)x->data);
	uint64_t r = fread(x->data, bps, w*h*pd, fin);
	if (r != (uint64_t)w*h*pd)
		fprintf(stderr,"IIO WARNING: npy file smaller than expected\n");
	if (order[0] == 'T') inplace_transpose(x);
	return 0;
}

// VICAR reader                                                             {{{2
static int read_beheaded_vic(struct iio_image *x,
		FILE *fin, char *header, int nheader)
{
	(void)header;
	assert(nheader == 8);

	// read label size (first tag)
	char s[10] = {0}; // remaining part of the fixed-size header
	int i = 0;
	do
		s[i++] = pick_char_for_sure(fin);
	while (i < 9 && s[i] != '0');
	IIO_DEBUG("VICAR LBLSIZE = %d\n", atoi(s));

	// read the rest of the label
	int n = atoi(s);      // label size
	char *l = xmalloc(n); // label data (after the first tag)
	for (int k = 0; k < n - i - 8; k++)
		l[k] = pick_char_for_sure(fin);
	l[n-i-9] = 0;

	// VICAR fields with placeholder values
	int f_recsize = -1; // record size in bytes
	char f_format[99] = {0}; // sample type (byte,half,full,real,doubl,comp)
	char f_type[99] = {0};   // file type (always "image")
	char f_org[99] = {0};    // brokennes order (only "bsq"=broken is known)
	char f_ifmt[99] = {0};   // endianness (high or low)
	int f_nl = -1;      // number of lines
	int f_ns = -1;      // number of samples (per line)
	int f_nb = -1;      // numbef of bands (pixel dimension)
	//int f_n1 = -1;      // 1st dimension
	//int f_n2 = -1;      // 2nd dimension
	//int f_n3 = -1;      // 3rd dimension
	int f_nbb = 0;      // trim bytes at left of each line line
	int f_nlb = 0;      // trim records at the beginning

	// parse label tags
	char *tok = strtok(l, " ");
	while (tok) {
		IIO_DEBUG("\tVICAR tok = \"%s\"\n", tok);
		char k[n]; // key
		char v[n]; // value
		int r = sscanf(tok, "%[^=]=%s", k, v);
		if (r != 2) goto cont;
		IIO_DEBUG("\t\tr,k,v = %d, \"%s\", \"%s\"\n", r, k, v);

		// extract VICAR fields
		if (0 == strcmp(k, "RECSIZE")) f_recsize = atoi(v);
		if (0 == strcmp(k, "FORMAT" )) strncpy(f_format, v, 98);
		if (0 == strcmp(k, "TYPE"   )) strncpy(f_type, v, 98);
		if (0 == strcmp(k, "ORG"    )) strncpy(f_org, v, 98);
		if (0 == strcmp(k, "INTFMT" )) strncpy(f_ifmt, v, 98);
		if (0 == strcmp(k, "NL"     )) f_nl  = atoi(v);
		if (0 == strcmp(k, "NS"     )) f_ns  = atoi(v);
		if (0 == strcmp(k, "NB"     )) f_nb  = atoi(v);
		//if (0 == strcmp(k, "N1"     )) f_n1  = atoi(v);
		//if (0 == strcmp(k, "N2"     )) f_n2  = atoi(v);
		//if (0 == strcmp(k, "N3"     )) f_n3  = atoi(v);
		if (0 == strcmp(k, "NBB"    )) f_nbb = atoi(v);
		if (0 == strcmp(k, "NLB"    )) f_nlb = atoi(v);
	cont:	 tok = strtok(0, " ");
	}
	free(l);

	// verify sanity
	if (0 != strcmp(f_type, "'IMAGE'"))
		fail("VICAR reader only groks 'IMAGE' objects (got %s)",f_type);
	if (0 != strcmp(f_org, "'BSQ'"))
		fail("VICAR reader only groks 'BSQ' orgn (got %s)",f_org);
	if (f_recsize <= 0)
		fail("VICAR recsize must be positive");

	// fill-in the struct fields
	x->type = -1;
	if (0==strcmp(f_format,"'BYTE'")) x->type = IIO_TYPE_UINT8;
	if (0==strcmp(f_format,"'HALF'")) x->type = IIO_TYPE_INT16; // (!)
	if (0==strcmp(f_format,"'FULL'")) x->type = IIO_TYPE_INT32;
	if (0==strcmp(f_format,"'REAL'")) x->type = IIO_TYPE_FLOAT;
	if (0==strcmp(f_format,"'DOUBL'")) x->type = IIO_TYPE_DOUBLE;
	if (x->type < 0) fail("VICAR reader unknown format \"%s\"", f_format);
	int bps = iio_type_size(x->type);
	if (f_recsize != f_nbb + f_ns * bps)
		fail("VICAR bad recsize %d != %d + %d * %d\n",
				f_recsize, f_nbb, f_ns, bps);
	iio_image_init2d(x, f_ns, f_nl, f_nb, x->type);

	// fill-in the data
	x->data = xmalloc(x->sizes[0] * x->sizes[1] * x->pixel_dimension * bps);
	int datac = 0;
	char rec[f_recsize];
	for (i = 0; i < f_nlb; i++) // discard the first "nlb" records
	{
		int r = fread(rec, f_recsize, 1, fin);
		if (r != 1) fail("could not read whole VICAR prefx");
	}
	for (i = 0; i < f_nl * f_nb; i++) // read the good records
	{
		int r = fread(rec, f_recsize, 1, fin);
		if (r != 1) fail("could not read whole VICAR file");
		memcpy(datac + (uint8_t*)x->data, rec + f_nbb, f_ns*bps);
		datac += f_ns*bps;
	}

	// fix endianness
	if (bps==2 && 0==strcmp(f_ifmt,"'HIGH'"))
		switch_2endianness(x->data, iio_image_number_of_samples(x));
	if (bps==4 && 0==strcmp(f_ifmt,"'HIGH'"))
		switch_4endianness(x->data, iio_image_number_of_samples(x));

	return 0;
}

// CCSD3ZF reader                                                           {{{2

// clean a short-padded string
//static void sanitize_label(char *s)
//{
//	int n = strlen(s);
//	if (n>1 && 0 == n%2 && !isalnum(s[n-1]))
//		s[n-1] = '\0';
//}

static int read_beheaded_ccs(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	// TODO: merge with the standard PDS reader, as except for the
	// separators the description language is exactly the same
	//

	// note: kills just one file of the test collection (2065211)
	return read_beheaded_pds(x, f, header, nheader);

//	// get an object name, if different to "^IMAGE"
//	char *object_id = xgetenv("IIO_PDS_OBJECT");
//	if (!object_id)
//		object_id = "^IMAGE";
//
//	int n, nmax = 1000, cx = 0;
//	char line[nmax], key[nmax], value[nmax];
//	int rbytes = -1, w = -1, h = -1, spp = 1, bps = 1, obj = -1, ehist = -1;
//	int rtype = 'u';
//	int sfmt = SAMPLEFORMAT_UINT;
//	bool in_object = false;
//	while ((n = getlinen(line, nmax, f)) >= 0 && cx++ < nmax)
//	{
//		pds_parse_line(key, value, line);
//		if (!*key || !*value) continue;
//		sanitize_label(value);
	fail("implemented as PDS variant");
	return 1;
}


// FITS reader                                                              {{{2

static void fit_parse_line(char *k, char *v, char *l)
{
	int r = sscanf(l, "%s = %s", k, v);
	if (r != 2)
		*k = *v = '\0';
}

static int read_beheaded_fit(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	(void)header;
	while (nheader++ < 80)
		pick_char_for_sure(f);

	int n = 0; // line counter
	int bitpix = -1, w = -1, h = -1, pd = 1, d = 2;
	while (1) {
		n += 1;
		char l[81] = {0}; // buffer for each line (80 chars + zero)
		char k[81] = {0}; // "key" field of the line
		char v[81] = {0}; // "value" field of the line
		int r = fread(l, 1, 80, f);
		if (r != 80) fail("FITS reader failed at line %d\n", n);
		fprintf(stderr, "FITS(%d): \"%s\"\n", n, l);
		if (l[0]=='E' && l[1]=='N' && l[2]=='D' && l[3]==' ')
			break;

		fit_parse_line(k, v, l);
		fprintf(stderr, "\tk=\"%s\"\n", k);
		fprintf(stderr, "\tv=\"%s\"\n", v);

		if (!strcmp(k, "BITPIX")) bitpix = atoi(v);
		if (!strcmp(k, "NAXIS" )) d = atoi(v);
		if (!strcmp(k, "NAXIS1")) w = atoi(v);
		if (!strcmp(k, "NAXIS2")) h = atoi(v);
		if (!strcmp(k, "NAXIS3")) pd = atoi(v);
	}

	fprintf(stderr, "n = %d\n", n);

	// read padding lines until the next multiple of 36
	if (n % 36) while (++n % 36) {
		char l[80];
		if (80 != fread(l, 1, 80, f))
			fail("FITS padding failed at line %d\n", n);
	}
	//fprintf(stderr, "n = %d\n", n);

	int typ = -1;
	if (bitpix ==   8) typ = IIO_TYPE_UINT8;
	if (bitpix ==  16) typ = IIO_TYPE_INT16;
	if (bitpix ==  32) typ = IIO_TYPE_INT32;
	if (bitpix == -32) typ = IIO_TYPE_FLOAT;
	if (bitpix == -64) typ = IIO_TYPE_DOUBLE;
	if (typ < 0) fail("unrecognized FITS BITPIX=%d", bitpix);

	fprintf(stderr, "w = %d\n", w);
	fprintf(stderr, "h = %d\n", h);
	fprintf(stderr, "pd = %d\n", pd);
	fprintf(stderr, "d = %d\n", d);
	fprintf(stderr, "typ = %s (%d)\n", iio_strtyp(typ), typ);

	// fill-in the image struct
	iio_image_init2d(x, w, h, pd, typ);
	int s = w * h * pd * iio_image_sample_size(x);
	x->data = xmalloc(s);
	n = fread(x->data, s, 1, f);
	if (n != 1) { xfree(x->data); return 4; }

	// fix endianness
	if (bitpix == 16)
		switch_2endianness(x->data, iio_image_number_of_samples(x));
	if (bitpix == 32 || bitpix == -32)
		switch_4endianness(x->data, iio_image_number_of_samples(x));
	if (bitpix == -64)
		switch_8endianness(x->data, iio_image_number_of_samples(x));

	return 0;
}

// RAW reader                                                               {{{2

// Note: there are two raw readers, either
//
// 1) the user supplies the dimensions and data format, or their location
// 2) the program uses several heuristics to find the dimensions
//
// They are named, respectively, explicit and fancy.
//
// Note2: the fancy reader is not yet implemented

//
// Documentation for the "explicit" raw reader
// -------------------------------------------
//
// Idea: to read a raw file named "file.xxx", open the image
// with name "RAW[...]:file.xxx".  The "..." specify the
// details of the raw format.
//
// 
// Example:
//
// RAW[w320,h200,tFLOAT]:file.xxx
//
// This reads 320x200 floats from "file.xxx".
//
// The contents of [ ] are a list of "tokens", separated by ","
//
// Each token is a character followed by its value
//
// Valid characters with their meaning:
//
// w = width
// h = height
// p = pixel dimension (e.g. 1 or 3)
//
// t =  one of "INT8", "UINT8", "INT16", "UINT16", "INT32", "UINT32", "INT64",
// "UINT64", "FLOAT", "DOUBLE", "LONGDOUBLE", "HALF", "UINT1",
// "UINT2", "UINT4", "CHAR", "SHORT", "INT", "LONG", "LONGLONG",
//
// o = offset bytes to be ignored from the start of the file
//     (if negative, ignored from the byte after th end of the file)
//     ((default=-1 == EOF))
//
// b = 0,1 wether pixel channels are contiguous or broken into planes
//
// e = 0,1 controls the endianness.  By default, the native one
//
// r = xy, xY, Xy, XY, yx, Yx, yX, YX
//         controls the orientation of the coordinates (uppercase=reverse)
//
// All the numeric fields can be read from the same file.  For example,
// "w@44/2" says that the width is read from position 44 of the file
// as an uint16, etc.
//
//
// Annex: Specifying the RAW type using environment variables.
//
// Typically, when IIO fails to recognize the filetype, it tries some desperate
// measures.  One of this desperate measures is enabled by the environment
// variable  IIO_RAW, that specifies the "rawstring" of the file to be opened.
//

// if f ~ /RAW[.*]:.*/ return the position of the colon
static char *raw_prefix(const char *f)
{
	if (f != strstr(f, "RAW["))
		return NULL;
	char *colon = strchr(f, ':');
	if (!colon || colon[-1] != ']')
		return NULL;
	return colon;
}

//// if f ~ /RWA[.*]:.*/ return the position of the colon
//static char *rwa_prefix(const char *f)
//{
//	if (f != strstr(f, "RWA["))
//		return NULL;
//	char *colon = strchr(f, ':');
//	if (!colon || colon[-1] != ']')
//		return NULL;
//	return colon;
//}

// explicit raw reader (input = a given block of memory)
static int parse_raw_binary_image_explicit(struct iio_image *x,
		void *data, size_t ndata,
		int w, int h, int pd,
		int header_bytes, int sample_type,
		bool broken_pixels, bool endianness)
{
	(void)broken_pixels;
	size_t nsamples = w*h*pd;
	size_t ss = iio_type_size(sample_type);
	if (ndata < header_bytes + nsamples*ss) {
		fprintf(stderr, "WARNING:bad raw file size (%zu != %d + %zu)",
				ndata, header_bytes, nsamples*ss);
		//return 1;
	}
	int sizes[2] = {w, h};
	iio_image_build_independent(x, 2, sizes, sample_type, pd);
	size_t n = nsamples * ss;
	memcpy(x->data, header_bytes + (char*)data, n);
	if (endianness) {
		if (ss == 2)
			switch_2endianness(x->data, nsamples);
		if (ss >= 4)
			switch_4endianness(x->data, nsamples);
	}
	return 0;
}

// get an integer field from a data file, whose position
// and type is determined by "tok"
static int raw_gfp(void *dat, int siz, char *tok, int endianness)
{
	int fpos, fsiz = -4;
	if (2 == sscanf(tok, "%d/%d", &fpos, &fsiz));
	else if (1 == sscanf(tok, "%d", &fpos));
	else return 0;
	IIO_DEBUG("raw gfp tok=%s fpos=%d fiz=%d\n", tok, fpos, fsiz);
	void *pvalue = fpos + (char*)dat;
	if (fpos < 0 || abs(fsiz) + fpos > siz)
		fail("can not read field beyond data size");
	if (endianness && abs(fsiz)==2) switch_2endianness(pvalue, 1);
	if (endianness && abs(fsiz)==4) switch_4endianness(pvalue, 1);
	switch(fsiz) {
	case  1: return *( uint8_t*)pvalue;
	case  2: return *(uint16_t*)pvalue;
	case  4: return *(uint32_t*)pvalue;
	case -1: return *(  int8_t*)pvalue;
	case -2: return *( int16_t*)pvalue;
	case -4: return *( int32_t*)pvalue;
	default: fail("unrecognized field size %d", fsiz);
	}
}

static int read_raw_named_image(struct iio_image *x, const char *filespec)
{
	// filespec => description + filename
	char *colon = raw_prefix(filespec);
	size_t desclen = colon - filespec - 5;
	char description[desclen+1];
	char *filename = colon + 1;
	memcpy(description, filespec+4, desclen);
	description[desclen] = '\0';

	// read data from file
	long file_size;
	void *file_contents = NULL;
	{
		FILE *f = xfopen(filename, "r");
		file_contents = load_rest_of_file(&file_size, f, NULL, 0);
		xfclose(f);
	}

	// fill-in data description
	int width = -1;
	int height = -1;
	int pixel_dimension = 1;
	int brokenness = 0;
	int endianness = 0;
	int sample_type = IIO_TYPE_UINT8;
	int offset = -1;
	int orientation = 0;
	int image_index = 0;
	int frame_offset = 0;

	// parse description string
	char *delim = ",", *tok = strtok(description, delim);
	int field;
	while (tok) {
		IIO_DEBUG("\ttoken = %s\n", tok);
		if (tok[1] == '@')
			field = raw_gfp(file_contents, file_size, 2+tok,
					endianness);
		else
			field = atoi(1+tok);
		IIO_DEBUG("\tfield=%d\n", field);
		switch(*tok) {
		case 'w': width           = field;       break;
		case 'h': height          = field;       break;
		case 'd':
		case 'p': pixel_dimension = field;       break;
		case 'o': offset          = field;       break;
		case 'b': brokenness      = field;       break;
		case 'e': endianness      = field;       break;
		case 'i': image_index     = field;       break;
		case 'y': frame_offset    = field;       break;
		case 't': sample_type     = iio_inttyp(1+tok); break;
		case 'r': orientation     = tok[1]+256*tok[2]; break;
		}
		tok = strtok(NULL, delim);
	}
	int sample_size = iio_type_size(sample_type);

	IIO_DEBUG("w = %d\n", width);
	IIO_DEBUG("h = %d\n", height);
	IIO_DEBUG("p = %d\n", pixel_dimension);
	IIO_DEBUG("o = %d\n", offset);
	IIO_DEBUG("b = %d\n", brokenness);
	IIO_DEBUG("i = %d\n", image_index);
	IIO_DEBUG("y = %d\n", frame_offset);
	IIO_DEBUG("t = %s\n", iio_strtyp(sample_type));

	// estimate missing dimensions
	IIO_DEBUG("before estimation w=%d h=%d o=%d\n", width, height, offset);
	int pd = pixel_dimension;
	int ss = sample_size;
	if (offset < 0 && width > 0 && height > 0)
		offset = file_size - width * height * pd * ss;
	if (width < 0 && offset > 0 && height > 0)
		width = (file_size - offset)/(height * pd * ss);
	if (height < 0 && offset > 0 && width > 0)
		height = (file_size - offset)/(width * pd * ss);
	if (offset < 0) offset = 0;
	if (height < 0) height = file_size/(width * pd * ss);
	if (width  < 0) width  = file_size/(height * pd * ss);
	if (offset < 0 || width < 0 || height < 0)
		fail("could not determine width, height and offset"
				"(got %d,%d,%d)", width, height, offset);
	offset += frame_offset;
	offset += image_index * (frame_offset + width*height*pd*ss);
	IIO_DEBUG("after estimation w=%d h=%d o=%d\n", width, height, offset);

	int used_data_size = offset+width*height*pd*ss;
	if (used_data_size > file_size)
		fail("raw file is not large enough");

	int r = parse_raw_binary_image_explicit(x,
			file_contents, file_size,
			width, height, pixel_dimension,
			offset, sample_type, brokenness, endianness);
	if (orientation)
		inplace_reorient(x, orientation);
	xfree(file_contents);
	return r;
}

// read a RAW image specified by the IIO_RAW environment
// caveat: the image *must* be a named file, not a pipe
// (this is for simplicity of the implementation, this restriction can be
// removed when necessary)
static int read_beheaded_raw(struct iio_image *x,
		FILE *f, char *header, int nheader)
{
	(void)f; (void)header; (void)nheader;
	const char *fn;
	fn = global_variable_containing_the_name_of_the_last_opened_file;
	if (!fn)
		return 1;

	char *rp = xgetenv("IIO_RAW");
	if (!rp)
		return 2;

	char buf[FILENAME_MAX];
	snprintf(buf, FILENAME_MAX, "RAW[%s]:%s", rp, fn);
	IIO_DEBUG("simulating read of filename \"%s\"\n", buf);
	return read_raw_named_image(x, buf);
}


// TRANS reader                                                             {{{2

// if f ~ /TRANS[.*]:.*/ return the position of the colon
static char *trans_prefix(const char *f)
{
	if (f != strstr(f, "TRANS["))
		return NULL;
	char *colon = strchr(f, ':');
	if (!colon || colon[-1] != ']')
		return NULL;
	return colon;
}

static void trans_flip(struct iio_image *x, char *s)
{
	IIO_DEBUG("TRANS flip \"%s\"\n", s);
	if (0 == strcmp(s, "leftright")) inplace_flip_horizontal(x);
	if (0 == strcmp(s, "topdown"))   inplace_flip_vertical(x);
	if (0 == strcmp(s, "transpose")) inplace_transpose(x);
	if (0 == strcmp(s, "r270"))      inplace_reorient(x, 'y' + 'X' * 0x100);
	if (0 == strcmp(s, "r90"))       inplace_reorient(x, 'Y' + 'x' * 0x100);
	if (0 == strcmp(s, "r180"))      inplace_reorient(x, 'X' + 'Y' * 0x100);
	if (0 == strcmp(s, "posetrans")) inplace_reorient(x, 'Y' + 'X' * 0x100);
	if (2 == strlen(s)) inplace_reorient(x, s[0] + s[1]*0x100);
	if (3 == strlen(s) && (3*'y')==tolower(*s)+tolower(s[1])+tolower(s[2]))
		inplace_3dreorient(x, s);
}

static int read_image_f(struct iio_image*, FILE *);
static void iio_write_image_default(const char *filename, struct iio_image *x);

// filter the image "x" through the command "p"
static void trans_pipe(struct iio_image *x, const char *p)
{
	char i[FILENAME_MAX];       // input filename
	char o[FILENAME_MAX];       // output filename
	char c[3*FILENAME_MAX];     // command to run the pipe
	fill_temporary_filename(i);
	fill_temporary_filename(o);
	snprintf(c, 3*FILENAME_MAX, "(unset IIO_TRANS; %s) <%s >%s", p, i, o);

	iio_write_image_default(i, x);

	IIO_DEBUG("IIO_TRANS: running pipe \"%s\"\n", p);
	if (!system(c))
	{
		// the monkey flies between two branches
		xfree(x->data);
		FILE *f = xfopen(o, "r");
		read_image_f(x, f);
		xfclose(f);
	}

	delete_temporary_file(i);
	delete_temporary_file(o);
}

static int trans_apply(struct iio_image *x, const char *f)
{
	// extract tranformation from augmented filename
	char t[strlen(f)];
	int i = 0;
	for (; f[i] && f[i] != ']'; i++)
		t[i] = f[i];
	t[i] = '\0';
	IIO_DEBUG("going to apply transformation list \"%s\"\n", t);

	// tokenize the transformation list
	char *delim = ",", *tok = strtok(t, delim);
	while (tok) {
		char o[i]; // operation
		char v[i]; // value
		int r = sscanf(tok, "%[^=]=%[^]]", o, v);
		if (r != 2) goto cont;

		int V = atoi(v), w = x->sizes[0], h = x->sizes[1];

		if (false) ;
		else if (0 == strcmp(o, "flip")) trans_flip(x, v);
		else if (0 == strcmp(o, "pipe")) trans_pipe(x, v);
		else if (0 == strcmp(o, "x")) inplace_trim(x, V,   0,   0, 0);
		else if (0 == strcmp(o, "y")) inplace_trim(x, 0,   0,   0, V);
		else if (0 == strcmp(o, "w")) inplace_trim(x, 0,   0, w-V, 0);
		else if (0 == strcmp(o, "h")) inplace_trim(x, 0, h-V,   0, 0);
	cont:	tok = strtok(NULL, delim);
	}

	return 0;
}

// WHATEVER reader                                                          {{{2

//static int read_image(struct iio_image*, const char *);
static int read_image_f(struct iio_image*, FILE *);
#ifdef I_CAN_HAS_WHATEVER
static int read_beheaded_whatever(struct iio_image *x,
	       	FILE *fin, char *header, int nheader)
{
	// dump data to file
	long filesize;
	void *filedata = load_rest_of_file(&filesize, fin, header, nheader);
	char *filename = put_data_into_temporary_file(filedata, filesize);
	xfree(filedata);

	//char command_format[] = "convert - %s < %s\0";
	char command_format[] = "/usr/bin/convert - %s < %s\0";
	char ppmname[strlen(filename)+10];
	snprintf(ppmname, FILENAME_MAX+10, "%s.ppm", filename);
	char command[strlen(command_format)+1+2*strlen(filename)];
	snprintf(command, FILENAME_MAX+10, command_format, ppmname, filename);
	IIO_DEBUG("COMMAND: %s\n", command);
	int r = system(command);
	IIO_DEBUG("command returned %d\n", r);
	if (r) {
		xfclose(fin);
		fail("could not run command \"%s\" successfully", command);
	}
	FILE *f = xfopen(ppmname, "r");
	r = read_image_f(x, f);
	xfclose(f);

	delete_temporary_file(filename);
	delete_temporary_file(ppmname);

	return r;
}
#endif


// RAW PHOTO reader                                                               {{{2

#ifdef I_USE_LIBRAW
int try_reading_file_with_libraw(const char *fname, struct iio_image *x);
int try_reading_file_with_libraw_4channels(const char *fname, struct iio_image *x);
#endif


// individual format writers                                                {{{1
// PNG writer                                                               {{{2

#ifdef I_CAN_HAS_LIBPNG

static void iio_write_image_as_png(const char *filename, struct iio_image *x)
{
	IIO_DEBUG("png writer filename = \"%s\"\n", filename);
	IIO_DEBUG("png writer w,h,pd = %d,%d,%d\n",
			x->sizes[0],x->sizes[1],x->pixel_dimension);
	IIO_DEBUG("png writer rem = \"%s\"\n", x->rem);

	png_structp pp = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0,0,0);
	if (!pp) fail("png_create_write_struct fail");
	png_infop pi = png_create_info_struct(pp);
	if (!pi) fail("png_create_info_struct fail");
	if (setjmp(png_jmpbuf(pp))) fail("png write error");


	if (x->dimension != 2) fail("can only save 2d images");
	int width = x->sizes[0];
	int height = x->sizes[1];
	int bit_depth = 0;
	switch(normalize_type(x->type)) {
	case IIO_TYPE_UINT16:
	case IIO_TYPE_INT16: bit_depth = 16; break;
	case IIO_TYPE_INT8:
	case IIO_TYPE_UINT8: bit_depth = 8; break;
	default: fail("can not yet save samples of type %s as PNG",
				 iio_strtyp(x->type));
	}
	assert(bit_depth > 0);
	int color_type = PNG_COLOR_TYPE_PALETTE;
	switch(x->pixel_dimension) {
	case 1: color_type = PNG_COLOR_TYPE_GRAY; break;
	case 2: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
	case 3: color_type = PNG_COLOR_TYPE_RGB; break;
	case 4: color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
	default: fail("can not save %d-dimensional samples as PNG",
				 x->pixel_dimension);
	}
	assert(color_type != PNG_COLOR_TYPE_PALETTE);

	if (x->rem) { // add png comment
		png_text pt;
		pt.key = "Comment";
		pt.text = x->rem;
		pt.compression = PNG_TEXT_COMPRESSION_NONE;
		png_set_text(pp, pi, &pt, 1);
	}

	FILE *f = xfopen(filename, "w");
	png_init_io(pp, f);

	int ss = bit_depth/8;
	int pd = x->pixel_dimension;
	png_bytepp row = xmalloc(height * sizeof(png_bytep));
	FORI(height) row[i] = i*pd*width*ss + (uint8_t *)x->data;
	png_set_IHDR(pp, pi, width, height, bit_depth, color_type,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);
	png_set_rows(pp, pi, row);
	int transforms = PNG_TRANSFORM_IDENTITY;
	if (bit_depth == 16) transforms |= PNG_TRANSFORM_SWAP_ENDIAN;
	png_write_png(pp, pi, transforms, NULL);
	xfclose(f);
	png_destroy_write_struct(&pp, &pi);
	xfree(row);
}

#endif//I_CAN_HAS_LIBPNG

// TIFF writer                                                              {{{2

#ifdef I_CAN_HAS_LIBTIFF

static void iio_write_image_as_tiff(const char *filename, struct iio_image *x)
{
	if (x->dimension != 2)
		fail("only 2d images can be saved as TIFFs");
	TIFF *tif = TIFFOpen(filename, "w8");
	if (!tif) fail("could not open TIFF file \"%s\"", filename);

	int ss = iio_image_sample_size(x);
	int sls = x->sizes[0]*x->pixel_dimension*ss;
	int tsf;

	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, x->sizes[0]);
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, x->sizes[1]);
	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, x->pixel_dimension);
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, ss * 8);
	uint16_t caca[1] = {EXTRASAMPLE_UNASSALPHA};
	switch (x->pixel_dimension) {
	case 1:
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
		break;
	case 3:
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
		break;
	case 2:
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
		break;
	case 4:
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
		TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, caca);
		break;
	default:
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
	}

	// disable TIFF compression when saving large images
	if (x->sizes[0] * x->sizes[1] < 2000*2000 && !xgetenv("IIOTIFF_PLAIN"))
		TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
	else
		TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);

	switch(x->type) {
	case IIO_TYPE_DOUBLE:
	case IIO_TYPE_FLOAT: tsf = SAMPLEFORMAT_IEEEFP; break;
	case IIO_TYPE_INT8:
	case IIO_TYPE_INT:
	case IIO_TYPE_INT16:
	case IIO_TYPE_INT32: tsf = SAMPLEFORMAT_INT; break;
	case IIO_TYPE_UINT8:
	case IIO_TYPE_UINT16:
	case IIO_TYPE_UINT32: tsf = SAMPLEFORMAT_UINT; break;
	default: fail("can not save samples of type %s on tiff file",
				 iio_strtyp(x->type));
	}
	TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, tsf);

	// define TIFFTAG_ROWSPERSTRIP to satisfy some readers (e.g. gdal)
	uint32_t rows_per_strip = x->sizes[1];
	rows_per_strip = TIFFDefaultStripSize(tif, rows_per_strip);
	TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rows_per_strip);

	FORI(x->sizes[1]) {
		void *line = i*sls + (char *)x->data;
		int r = TIFFWriteScanline(tif, line, i, 0);
		if (r < 0) fail("error writing %dth TIFF scanline", i);
	}

	if (x->rem) // optional comment
		TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, x->rem);

	TIFFClose(tif);
}

static void iio_write_image_as_tiff_smarter(const char *filename,
		struct iio_image *x)
{
	char *tiffname = strstr(filename, "TIFF:");
	if (tiffname == filename) {
		iio_write_image_as_tiff_smarter(filename+5, x);
		return;
	}
	if (0 == strcmp(filename, "-")) {
		char tfn[FILENAME_MAX];
		fill_temporary_filename(tfn);
		iio_write_image_as_tiff(tfn, x);
		FILE *f = xfopen(tfn, "r");
		int c;
		while ((c = fgetc(f)) != EOF)
			fputc(c, stdout);
		fclose(f);
		delete_temporary_file(tfn);
	} else
		iio_write_image_as_tiff(filename, x);
}

#endif//I_CAN_HAS_LIBTIFF


// JUV writer                                                               {{{2
static void iio_write_image_as_juv(const char *filename, struct iio_image *x)
{
	assert(x->type == IIO_TYPE_FLOAT);
	assert(x->dimension == 2);
	char buf[255]; FORI(255) buf[i] = ' ';
	snprintf(buf, 255, "#UV {\n dimx %d dimy %d\n}\n",
			x->sizes[0], x->sizes[1]);
	size_t sf = sizeof(float);
	int w = x->sizes[0];
	int h = x->sizes[1];
	float *uv = x->data;
	float *u = xmalloc(w*h*sf); FORI(w*h) u[i] = uv[2*i];
	float *v = xmalloc(w*h*sf); FORI(w*h) v[i] = uv[2*i+1];
	FILE *f = xfopen(filename, "w");
	fwrite(buf, 1, 255, f);
	fwrite(u, sf, w*h, f);
	fwrite(v, sf, w*h, f);
	xfclose(f);
	xfree(u); xfree(v);
}


// FLO writer                                                               {{{2
static void iio_write_image_as_flo(const char *filename, struct iio_image *x)
{
	assert(x->type == IIO_TYPE_FLOAT);
	assert(x->dimension == 2);
	union { char s[4]; float f; } pieh = {"PIEH"};
	assert(sizeof(float) == 4);
	assert(pieh.f == 202021.25);
	uint32_t w = x->sizes[0];
	uint32_t h = x->sizes[1];
	FILE *f = xfopen(filename, "w");
	fwrite(&pieh.f, 4, 1, f);
	fwrite(&w, 4, 1, f);
	fwrite(&h, 4, 1, f);
	fwrite(x->data, 4, w*h*2, f);
	xfclose(f);
}

// PFM writer                                                               {{{2
static void iio_write_image_as_pfm(const char *filename, struct iio_image *x)
{
	assert(x->type == IIO_TYPE_FLOAT);
	assert(x->dimension == 2);
	assert(x->pixel_dimension == 1 || x->pixel_dimension == 3);
	FILE *f = xfopen(filename, "w");
	int dimchar = 1 < x->pixel_dimension ? 'F' : 'f';
	int w = x->sizes[0];
	int h = x->sizes[1];
	float scale = -1;
	fprintf(f, "P%c\n%d %d\n%g\n", dimchar, w, h, scale);
	fwrite(x->data, w * h * x->pixel_dimension * sizeof(float), 1 ,f);
	xfclose(f);
}

// PPM writer                                                               {{{2
static void iio_write_image_as_ppm(const char *filename, struct iio_image *x)
{
	assert(x->type == IIO_TYPE_FLOAT);
	assert(x->dimension == 2);
	assert(x->pixel_dimension == 1 || x->pixel_dimension == 3);
	FILE *f = xfopen(filename, "w");
	int dimchar = 1 < x->pixel_dimension ? '3' : '2';
	int w = x->sizes[0];
	int h = x->sizes[1];
	int pd = x->pixel_dimension;
	float *t = (float*) x->data;
	float scale = 255;
	fprintf(f, "P%c\n", dimchar);
	if (x->rem) fprintf(f, "# %s\n", x->rem);
	fprintf(f, "%d %d\n%g\n", w, h, scale);
	for (int i = 0; i < w*h*pd; i++)
		fprintf(f, "%d\n", (int) t[i]);
	xfclose(f);
}

// ASC writer                                                               {{{2
static void iio_write_image_as_asc(const char *filename, struct iio_image *x)
{
	if (x->type == IIO_TYPE_FLOAT)
	{
		assert(x->type == IIO_TYPE_FLOAT);
		assert(x->dimension == 2);
		FILE *f = xfopen(filename, "w");
		int w = x->sizes[0];
		int h = x->sizes[1];
		int pd = x->pixel_dimension;
		fprintf(f, "%d %d 1 %d\n", w, h, pd);
		float *t = xmalloc(w*h*pd*sizeof*t);
		break_pixels_float(t, x->data, w*h, pd);
		for (int i = 0; i < w*h*pd; i++)
			fprintf(f, "%.9g\n", t[i]);
		xfree(t);
		xfclose(f);
	} else if (x->type == IIO_TYPE_DOUBLE) {
		assert(x->type == IIO_TYPE_DOUBLE);
		assert(x->dimension == 2);
		FILE *f = xfopen(filename, "w");
		int w = x->sizes[0];
		int h = x->sizes[1];
		int pd = x->pixel_dimension;
		fprintf(f, "%d %d 1 %d\n", w, h, pd);
		double *t = xmalloc(w*h*pd*sizeof*t);
		break_pixels_double(t, x->data, w*h, pd);
		for (int i = 0; i < w*h*pd; i++)
			fprintf(f, "%.9g\n", t[i]);
		xfree(t);
		xfclose(f);
	}
}

// CSV writer                                                               {{{2
static void iio_write_image_as_csv(const char *filename, struct iio_image *x)
{
	FILE *f = xfopen(filename, "w");
	int w = x->sizes[0];
	int h = x->sizes[1];
	assert(x->pixel_dimension == 1);
	if (x->type == IIO_TYPE_FLOAT) {
		float *t = x->data;
		for (int i = 0; i < w*h; i++)
			fprintf(f, "%.9g%c", t[i], (i+1)%w?',':'\n');
	}
	if (x->type == IIO_TYPE_DOUBLE) {
		double *t = x->data;
		for (int i = 0; i < w*h; i++)
			fprintf(f, "%.9g%c", t[i], (i+1)%w?',':'\n');
	}
	if (x->type == IIO_TYPE_UINT8) {
		uint8_t *t = x->data;
		for (int i = 0; i < w*h; i++)
			fprintf(f, "%d%c", t[i], (i+1)%w?',':'\n');
	}
	xfclose(f);
}

// TXT writer                                                               {{{2
static void iio_write_image_as_txt(const char *filename, struct iio_image *x)
{
	FILE *f = xfopen(filename, "w");
	int w = x->sizes[0];
	int h = x->sizes[1];
	if (x->pixel_dimension != 1)
	{
		if (w == 1) w = x->pixel_dimension;
		else if (h == 1) w = x->pixel_dimension;
		else assert(false);
	}
	if (x->type == IIO_TYPE_FLOAT) {
		float *t = x->data;
		for (int i = 0; i < w*h; i++)
			fprintf(f, "%.9g%c", t[i], (i+1)%w?' ':'\n');
	}
	if (x->type == IIO_TYPE_DOUBLE) {
		double *t = x->data;
		for (int i = 0; i < w*h; i++)
			fprintf(f, "%.9g%c", t[i], (i+1)%w?' ':'\n');
	}
	if (x->type == IIO_TYPE_UINT8) {
		uint8_t *t = x->data;
		for (int i = 0; i < w*h; i++)
			fprintf(f, "%d%c", t[i], (i+1)%w?' ':'\n');
	}
	xfclose(f);
}

// general text&separator writing function
//static void iio_write_image_as_txt_general(
//		const char *filename, struct iio_image *x,
//		char *optional_seplist)
//{
//	(void)filename;
//	(void)x;
//	char *seplist = optional_seplist;
//	if (!seplist) seplist = " \t\n";
//	int nseps = strlen(seplist);
//}

// RAW writer                                                               {{{2
static void iio_write_image_as_raw(const char *filename, struct iio_image *x)
{
	// debugging shit
	IIO_DEBUG("dumping RAW\n");
	IIO_DEBUG("   dim=%d\n", x->dimension);
	for (int i = 0; i < x->dimension; i++)
		IIO_DEBUG("   size[%d] = %d\n", i, x->sizes[i]);
	IIO_DEBUG("   pd = %d\n", x->pixel_dimension);
	IIO_DEBUG("   type = %d \"%s\"\n", x->type, iio_strtyp(x->type));

	// actual writing
	FILE *f = xfopen(filename, "w");
	fwrite(x->data, iio_image_sample_size(x),
			iio_image_number_of_samples(x), f);
	xfclose(f);
}

// NPY writer                                                               {{{2
static void iio_write_image_as_npy(const char *filename, struct iio_image *x)
{
	char *descr = 0; // string to identify the number type (by numpy)
	switch (normalize_type(x->type)) {
		case IIO_TYPE_CHAR   :
		case IIO_TYPE_UINT8  : descr = "<u1"; break;
		case IIO_TYPE_UINT16 : descr = "<u2"; break;
		case IIO_TYPE_UINT32 : descr = "<u4"; break;
		case IIO_TYPE_UINT64 : descr = "<u8"; break;
		case IIO_TYPE_INT8   : descr = "<i1"; break;
		case IIO_TYPE_INT16  : descr = "<i2"; break;
		case IIO_TYPE_INT32  : descr = "<i4"; break;
		case IIO_TYPE_INT64  : descr = "<i8"; break;
		case IIO_TYPE_FLOAT  : descr = "<f4"; break;
		case IIO_TYPE_DOUBLE : descr = "<f8"; break;
		default: fail("unrecognized internal type %d\n", x->type);
	}
	char buf[1000] = {-109, 'N', 'U', 'M', 'P', 'Y', 1, 0, 0}; // magic
	int n = 10;               // size of magic before header string
	n += snprintf(buf+n, 1000-n, "{'descr': '%s', 'fortran_order': "
			"False, 'shape': (", descr);
	for (int i = x->dimension-1; i >= 0; i--)
		n += snprintf(buf+n, 1000-n, "%d, ", x->sizes[i]);
	n += snprintf(buf+n, 1000-n, "%d)}", x->pixel_dimension);
	int m = ((n+15+1)/16)*16; // next multiple of 16 (plus 1)
	buf[8] = (m-10) % 0x100;  // fill-in header size (minus sub-header)
	buf[9] = (m-10) / 0x100;  //
	for (int i = n; i < m-1; i++)
		buf[i] = ' ';     // pad with spaces
	buf[m-1] = '\n';          // must end in EOL.  Note: not 0-finished!

	FILE *f = xfopen(filename, "w");
	fwrite(buf, 1, m, f);                               // write the header
	fwrite(x->data, iio_image_sample_size(x),           // write the data
			iio_image_number_of_samples(x), f);
	xfclose(f);
}

// RIM writer                                                               {{{2

static void rim_putshort(FILE *f, uint16_t n)
{
	int a = n % 0x100;
	int b = n / 0x100;
	assert(a >= 0);
	assert(b >= 0);
	assert(a < 256);
	assert(b < 256);
	fputc(b, f);
	fputc(a, f);
}

static void iio_write_image_as_rim_fimage(const char *fname, struct iio_image *x)
{
	if (x->type != IIO_TYPE_FLOAT) fail("fimage expects float data");
	if (x->dimension != 2) fail("fimage expects 2d image");
	if (x->pixel_dimension != 1) fail("fimage expects gray image");
	FILE *f = xfopen(fname, "w");
	fputc('I', f);
	fputc('R', f);
	rim_putshort(f, 2);
	rim_putshort(f, x->sizes[0]);
	rim_putshort(f, x->sizes[1]);
	FORI(29) rim_putshort(f, 0);
	int r = fwrite(x->data, sizeof(float), x->sizes[0]*x->sizes[1], f);
	if (r != x->sizes[0]*x->sizes[1])
		fail("could not write an entire fimage for some reason");
	xfclose(f);
}

static void iio_write_image_as_rim_cimage(const char *fname, struct iio_image *x)
{
	if (x->type != IIO_TYPE_UINT8) fail("cimage expects byte data");
	if (x->dimension != 2) fail("cimage expects 2d image");
	if (x->pixel_dimension != 1) fail("cimage expects gray image");
	FILE *f = xfopen(fname, "w");
	fputc('M', f);
	fputc('I', f);
	rim_putshort(f, 2);
	rim_putshort(f, x->sizes[0]);
	rim_putshort(f, x->sizes[1]);
	FORI(29) rim_putshort(f, 0);
	int r = fwrite(x->data, 1, x->sizes[0]*x->sizes[1], f);
	if (r != x->sizes[0]*x->sizes[1])
		fail("could not write an entire cimage for some reason");
	xfclose(f);
}

// SIX writer                                                               {{{2

// auxiliary stream interface, used mainly for sixels
struct bytestream {
	int n, ntop;
	uint8_t *t;
};

static void bytestream_init(struct bytestream *s)
{
	s->ntop = 1024;
	s->n = 0;
	s->t = xmalloc(s->ntop);
}

static void bs_putchar(struct bytestream *s, uint8_t x)
{
	if (s->n >= s->ntop)
	{
		s->ntop *= 2;
		s->t = xrealloc(s->t, s->ntop);
	}
	s->t[s->n++] = x;
}

static void bs_puts(struct bytestream *s, char *x)
{
	while (*x)
		bs_putchar(s, *x++);
}

static int bs_printf(struct bytestream *s, char *fmt, ...)
{
	va_list argp;
	char buf[0x1000];
	va_start(argp, fmt);
	int r = vsnprintf(buf, 0x1000, fmt, argp);
	bs_puts(s, buf);
	va_end(argp);
	return r;
}

static void bytestream_free(struct bytestream *s)
{
	xfree(s->t);
}

static int sidx(uint8_t *rgb) // sixel index identifier
{
	int r = 0;
	r += (rgb[0] >> 5) << 5;
	r += (rgb[1] >> 5) << 2;
	r += rgb[2] >> 6;
	return r;
	//return 32*(rgb[0]/32) + 4*(rgb[1]/32) + rgb[2]/64;
}

static void dump_sixels_to_bytestream_rgb3(
		struct bytestream *out,
		uint8_t *x, int w, int h)
{
	bs_puts(out, "\033Pq\n");
	for (int i = 0; i < 0x100; i++)
		bs_printf(out, "#%d;2;%d;%d;%d", i,
				(int)(14.2857*(i/32)),
				(int)(14.2857*((i/4)%8)),
				(int)(33.3333*(i%4)));
	for (int j = 0; j < h/6; j++)
	{
		int m[0x100] = {0}, c = 0;
		for (int i = 0; i < 6*w; i++)
		{
			int k = sidx(x+3*(6*j*w+i));
			if (!m[k]) c += 1;
			m[k] += 1;
		}
		for (int k = 0; k < 0x100; k++)
		if (m[k])
		{
			int b[w], r[w], R[w], n = 0;
			c -= 1;
			for (int i = 0; i < w; i++)
			{
				int s = 0;
				for (int l = 5; l >= 0; l--)
					s = 2*s + (k==sidx(x+3*((6*j+l)*w+i)));
				b[i] = s + 63;
			}
			for (int i = 0; i < w; i++)
				R[i] = 1;
			r[0] = *b;
			for (int i = 1; i < w; i++)
				if (b[i] == r[n])
					R[n] += 1;
				else
					r[++n] = b[i];
			bs_printf(out, "#%d", k);
			for (int i = 0; i <= n; i++)
				if (R[n] < 3)
					for (int l = 0; l < R[i]; l++)
						bs_putchar(out, r[i]);
				else
					bs_printf(out, "!%d%c", R[i], r[i]);
			bs_puts(out, c ? "$\n" : "-\n");
		}
	}
	bs_puts(out, "\033\\");
}

static void dump_sixels_to_bytestream_gray2(
		struct bytestream *out,
		uint8_t *x, int w, int h)
{
	int Q = (1<<2); // quantization over [0..255]
	bs_printf(out, "\033Pq\n");
	for (int i = 0; i < 0x100/Q; i++)
		bs_printf(out, "#%d;2;%d;%d;%d",
			i, (int)(Q*.39*i), (int)(Q*.39*i), (int)(Q*.39*i));
	for (int j = 0; j < h/6; j++) {
		int m[0x100] = {0}, c = 0;
		for (int i = 0; i < 6*w; i++) {
			int k = x[6*j*w+i]/Q;
			if (!m[k]) c += 1;
			m[k] += 1;
		}
		for (int k = 0; k < 0x100/Q; k++)
		if (m[k]) {
			int b[w], r[w], R[w], idx = 0;
			c -= 1;
			for (int i = 0; i < w; i++) {
				b[i] = 0;
				for (int l = 5; l >= 0; l--)
					b[i] = 2*b[i] + (k == x[(6*j+l)*w+i]/Q);
				b[i] += 63;
			}
			for (int i = 0; i < w; i++) R[i] = 1;
			r[0] = *b;
			for (int i = 1; i < w; i++)
				if (b[i] == r[idx]) R[idx] += 1;
				else r[++idx] = b[i];
			bs_printf(out, "#%d", k);
			for (int i = 0; i <= idx; i++)
				if (R[idx] < 3)
					for (int l = 0; l < R[i]; l++)
						bs_printf(out, "%c", r[i]);
				else
					bs_printf(out, "!%d%c", R[i], r[i]);
			bs_printf(out, c ? "$\n" : "-\n");
		}
	}
	bs_printf(out, "\033\\");
}

//static void dump_sixels_to_stdout_rgb3(uint8_t *x, int w, int h)
//{
//	struct bytestream s[1];
//	bytestream_init(s);
//	dump_sixels_to_bytestream_rgb3(s, x, w, h);
//	for (int i = 0; i < s->n; i++)
//		putchar(s->t[i]);
//	bytestream_free(s);
//	//printf("\ePq\n");
//	//for (int i = 0; i < 0x100; i++)
//	//	printf("#%d;2;%d;%d;%d", i,(int)(14.2857*(i/32)),
//	//			(int)(14.2857*((i/4)%8)), (int)(33.3333*(i%4)));
//	//for (int j = 0; j < h/6; j++)
//	//{
//	//	int m[0x100] = {0}, c = 0;
//	//	for (int i = 0; i < 6*w; i++)
//	//	{
//	//		int k = sidx(x+3*(6*j*w+i));
//	//		if (!m[k]) c += 1;
//	//		m[k] += 1;
//	//	}
//	//	for (int k = 0; k < 0x100; k++)
//	//	if (m[k])
//	//	{
//	//		int b[w], r[w], R[w], n = 0;
//	//		c -= 1;
//	//		for (int i = 0; i < w; i++)
//	//		{
//	//			int s = 0;
//	//			for (int l = 5; l >= 0; l--)
//	//				s = 2*s + (k==sidx(x+3*((6*j+l)*w+i)));
//	//			b[i] = s + 63;
//	//		}
//	//		for (int i = 0; i < w; i++)
//	//			R[i] = 1;
//	//		r[0] = *b;
//	//		for (int i = 1; i < w; i++)
//	//			if (b[i] == r[n])
//	//				R[n] += 1;
//	//			else
//	//				r[++n] = b[i];
//	//		printf("#%d", k);
//	//		for (int i = 0; i <= n; i++)
//	//			if (R[n] < 3)
//	//				for (int k = 0; k < R[i]; k++)
//	//					printf("%c", r[i]);
//	//			else
//	//				printf("!%d%c", R[i], r[i]);
//	//		printf(c ? "$\n" : "-\n");
//	//	}
//	//}
//	//printf("\e\\");
//}

//static void dump_sixels_to_stdout_gray2(uint8_t *x, int w, int h)
//{
//	int Q = (1<<2); // quantization over [0..255]
//	printf("\ePq\n");
//	for (int i = 0; i < 0x100/Q; i++)
//		printf("#%d;2;%d;%d;%d",
//			i, (int)(Q*.39*i), (int)(Q*.39*i), (int)(Q*.39*i));
//	for (int j = 0; j < h/6; j++) {
//		int m[0x100] = {0}, c = 0;
//		for (int i = 0; i < 6*w; i++) {
//			int k = x[6*j*w+i]/Q;
//			if (!m[k]) c += 1;
//			m[k] += 1;
//		}
//		for (int k = 0; k < 0x100/Q; k++)
//		if (m[k]) {
//			int b[w], r[w], R[w], idx = 0;
//			c -= 1;
//			for (int i = 0; i < w; i++) {
//				b[i] = 0;
//				for (int l = 5; l >= 0; l--)
//					b[i] = 2*b[i] + (k == x[(6*j+l)*w+i]/Q);
//				b[i] += 63;
//			}
//			for (int i = 0; i < w; i++) R[i] = 1;
//			r[0] = *b;
//			for (int i = 1; i < w; i++)
//				if (b[i] == r[idx]) R[idx] += 1;
//				else r[++idx] = b[i];
//			printf("#%d", k);
//			for (int i = 0; i <= idx; i++)
//				if (R[idx] < 3)
//					for (int k = 0; k < R[i]; k++)
//						printf("%c", r[i]);
//				else
//					printf("!%d%c", R[i], r[i]);
//			printf(c ? "$\n" : "-\n");
//		}
//	}
//	printf("\e\\");
//}

static void penetrate_screen(struct bytestream *z, struct bytestream *s)
{
	//bs_puts(z, "PENETRATE SCREEN\n");
	bs_puts(z, "\x1bP");
	for (int i = 0; i < s->n; i++)
	{
		int c = s->t[i];
		     if (c == '\x90') bs_puts(z, "\x1bP");
		else if (c == '\x9c') bs_puts(z, "\x1b\x1b\\\x1bP\\");
		else if (c == '\x1b' && i+1 < s->n && s->t[i] == '\\')
		{
			bs_puts(z, "\x1b\x1b\\\x1bP\\");
			i += 1;
		}
		else bs_putchar(z, c);
	}
	bs_puts(z, "\x1b\\");
}

static void dump_sixels_to_stdout_uint8(uint8_t *x, int w, int h, int pd)
{

	struct bytestream s[1];
	bytestream_init(s);
	if (pd == 3) dump_sixels_to_bytestream_rgb3(s, x, w, h);
	if (pd == 1) dump_sixels_to_bytestream_gray2(s, x, w, h);
	struct bytestream *S = s;
	bool screen = false;//strstr(xgetenv("TERM"), "screen");
	struct bytestream z[1];
	if (screen) bytestream_init(z);
	if (screen) penetrate_screen(S = z, s);
	for (int i = 0; i < S->n; i++)
		putchar(S->t[i]);
	bytestream_free(s);
	if (screen) bytestream_free(z);
}

static void dump_sixels_to_stdout(struct iio_image *x)
{
	//if (x->type != IIO_TYPE_UINT8)
	{
		void *old_data = x->data;
		int ss = iio_image_sample_size(x);
		int nsamp = iio_image_number_of_samples(x);
		x->data = xmalloc(nsamp*ss);
		memcpy(x->data, old_data, nsamp*ss);
		iio_convert_samples(x, IIO_TYPE_UINT8);
		dump_sixels_to_stdout_uint8(x->data, x->sizes[0], x->sizes[1],
				x->pixel_dimension);
		//if (x->pixel_dimension==3)
		//dump_sixels_to_stdout_rgb3(x->data, x->sizes[0], x->sizes[1]);
		//else if (x->pixel_dimension==1)
		//dump_sixels_to_stdout_gray2(x->data,x->sizes[0],x->sizes[1]);
		xfree(x->data);
		x->data = old_data;
	}
}


// JPEG writer                                                              {{{2

#ifdef I_CAN_HAS_LIBJPEG

static void iio_write_image_as_jpeg(const char *filename, struct iio_image *x)
{
	// allocate and initialize a JPEG compression object
	struct jpeg_compress_struct c[1];
	struct jpeg_error_mgr e[1];
	c->err = jpeg_std_error(e);
	e[0].error_exit = on_jpeg_error;
	jpeg_create_compress(c);

	// specify the destination of the compressed data
	FILE *f = xfopen(filename, "w");
	jpeg_stdio_dest(c, f);

	// set parameters for compression
	c->image_width      = x->sizes[0];
	c->image_height     = x->sizes[1];
	c->input_components = x->pixel_dimension;
	c->in_color_space   = JCS_UNKNOWN;
	if (x->pixel_dimension == 1) c->in_color_space = JCS_GRAYSCALE;
	if (x->pixel_dimension == 3) c->in_color_space = JCS_RGB;
	jpeg_set_defaults(c);

	// optionally, set compression quality
	char *q = xgetenv("IIO_JPEG_QUALITY");
	if (q) jpeg_set_quality(c, atoi(q), 1);

	// start compression, with optional comment field
	jpeg_start_compress(c, true);
	if (x->rem)
		jpeg_write_marker(c, JPEG_COM, (JOCTET*)x->rem, strlen(x->rem));

	// compress scanlines
	int s = x->sizes[0] * iio_image_pixel_size(x);
	JSAMPROW r[1];
	for (int j = 0; j < x->sizes[1]; j++)
	{
		r[0] = j*s + (unsigned char*)x->data;
		jpeg_write_scanlines(c, r, 1);
	}

	// cleanup and exit
	jpeg_finish_compress(c);
	jpeg_destroy_compress(c);
	xfclose(f);
}

#endif//I_CAN_HAS_LIBJPEG

// guess format using magic                                                 {{{1


static char add_to_header_buffer(FILE *f, uint8_t *buf, int *nbuf, int bufmax)
{
	int c = pick_char_for_sure(f);
	if (*nbuf >= bufmax)
		fail("buffer header too small (%d)", bufmax);
	buf[*nbuf] = c;//iw810
	IIO_DEBUG("ATHB[%d] = %x \"%c\"\n", *nbuf, c, c);
	*nbuf += 1;
	return c;
}

static void line_to_header_buffer(FILE *f, uint8_t *buf, int *nbuf, int bufmax)
{
	while (*nbuf < bufmax)
	{
		int c = pick_char_for_sure(f);
		buf[*nbuf] = c;
		*nbuf += 1;
		if (c == '\n')
			break;
	}
}

static int guess_format(FILE *f, char *buf, int *nbuf, int bufmax)
{
	assert(sizeof(uint8_t)==sizeof(char));
	uint8_t *b = (uint8_t*)buf;
	*nbuf = 0;

	//
	// hand-crafted state machine follows
	//

	if (xgetenv("IIO_RAW")) return IIO_FORMAT_RAW;
	if (xgetenv("IIO_TXT")) return IIO_FORMAT_TXT;

	b[0] = add_to_header_buffer(f, b, nbuf, bufmax);
	b[1] = add_to_header_buffer(f, b, nbuf, bufmax);
	if (b[0]=='P' || b[0]=='Q')
		if (b[1] >= '1' && b[1] <= '9')
			return IIO_FORMAT_QNM;

	if (b[0]=='P' && (b[1]=='F' || b[1]=='f'))
		return IIO_FORMAT_PFM;

#ifdef I_CAN_HAS_LIBTIFF
	if ((b[0]=='M' && buf[1]=='M') || (b[0]=='I' && buf[1]=='I'))
		return IIO_FORMAT_TIFF;
#endif//I_CAN_HAS_LIBTIFF

	if (b[0]=='I' && b[1]=='R') return IIO_FORMAT_RIM;
	//if (b[0]=='R' && b[1]=='I') return IIO_FORMAT_RIM; //webp conflict
	if (b[0]=='M' && b[1]=='I') return IIO_FORMAT_RIM;
	if (b[0]=='I' && b[1]=='M') return IIO_FORMAT_RIM;
	if (b[0]=='W' && b[1]=='E') return IIO_FORMAT_RIM;
	if (b[0]=='V' && b[1]=='I') return IIO_FORMAT_RIM;

	if (b[0]=='P' && b[1]=='C') return IIO_FORMAT_PCM;

	if (b[0]=='B' && b[1]=='M') {
		FORI(12) add_to_header_buffer(f, b, nbuf, bufmax);
		return IIO_FORMAT_BMP;
	}

	b[2] = add_to_header_buffer(f, b, nbuf, bufmax);
	b[3] = add_to_header_buffer(f, b, nbuf, bufmax);

#ifdef I_CAN_HAS_LIBWEBP
	if (b[0]=='R' && b[1]=='I' && b[2]=='F' && b[3]=='F')
		return IIO_FORMAT_WEBP;
#endif//I_CAN_HAS_LIBWEBP
	if (b[0]=='R' && b[1]=='I') return IIO_FORMAT_RIM; // (very rare)

#ifdef I_CAN_HAS_LIBPNG
	if (b[1]=='P' && b[2]=='N' && b[3]=='G')
		return IIO_FORMAT_PNG;
#endif//I_CAN_HAS_LIBPNG

#ifdef I_CAN_HAS_LIBEXR
	if (b[0]==0x76 && b[1]==0x2f && b[2]==0x31 && b[3]==0x01)
		return IIO_FORMAT_EXR;
#endif//I_CAN_HAS_LIBEXR

	if (b[0]=='#' && b[1]=='U' && b[2]=='V')
		return IIO_FORMAT_JUV;

	if (b[0]=='P' && b[1]=='I' && b[2]=='E' && b[3]=='H')
		return IIO_FORMAT_FLO; // middlebury flow

	if (b[0]=='P' && b[1]=='D' && b[2]=='S' && b[3]=='_')
		return IIO_FORMAT_PDS; // NASA's planetary data science

	if (b[0]=='<' && b[1]=='V' && b[2]=='R' && b[3]=='T')
		return IIO_FORMAT_VRT; // gdal virtual image

	if (b[0]=='f' && b[1]=='a' && b[2]=='r' && b[3]=='b')
		return IIO_FORMAT_FFD; // farbfeld

	if (b[0]==0x93 &&b [1]=='N' && b[2]=='U' && b[3]=='M')
		// && b[4]=='P' &&b [5]=='Y')
		return IIO_FORMAT_NPY; // Numpy

	if (b[0]==4 && b[1]==0 && b[2]==0 && b[3]==0)
		return IIO_FORMAT_RAT; // Random Access Texture (just for radar)

#ifdef I_CAN_HAS_LIBHDF5
	if (b[0]==0x89 && b[1]=='H' && b[2]=='D' && b[3]=='F')
		return IIO_FORMAT_HDF5;
#endif//I_CAN_HAS_LIBHDF5

	b[4] = add_to_header_buffer(f, b, nbuf, bufmax);
	b[5] = add_to_header_buffer(f, b, nbuf, bufmax);
	b[6] = add_to_header_buffer(f, b, nbuf, bufmax);
	b[7] = add_to_header_buffer(f, b, nbuf, bufmax);

#ifdef I_CAN_HAS_LIBJPEG
	if (b[0]==0xff && b[1]==0xd8 && b[2]==0xff) {
		if (b[3]==0xe0 && b[6]=='J' && b[7]=='F') // JFIF
			return IIO_FORMAT_JPEG;
		if (b[3]==0xe1 && b[6]=='E' && b[7]=='x') // EXIF
			return IIO_FORMAT_JPEG;
		if (b[3]==0xe2 && b[6]=='I' && b[7]=='C') // ICC_PROFILE
			return IIO_FORMAT_JPEG;
		if (b[3]==0xee || b[3]==0xed) // Adobe JPEG
			return IIO_FORMAT_JPEG;
		if (b[3]==0xdb) // Raw JPEG
			return IIO_FORMAT_JPEG;
	}
#endif//I_CAN_HAS_LIBJPEG

#ifdef I_CAN_HAS_LIBHEIF
	if (b[4]=='f' && b[5]=='t' && b[6]=='y' && b[7]=='p')
		return IIO_FORMAT_HEIF;
#endif//I_CAN_HAS_LIBHEIF

#ifdef I_CAN_HAS_LIBAVIF
	// note: same as above, as AVIF uses a HEIF container
	// TODO: clarify the build situation here
	if (b[0]=='f' && b[1]=='t' && b[2]=='y' && b[3]=='p')
		return IIO_FORMAT_AVIF;
#endif//I_CAN_HAS_LIBAVIF


	if (b[0]=='L' && b[1]=='B' && b[2]=='L' && b[3]=='S' &&
			b[4]=='I' && b[5]=='Z' && b[6]=='E' && b[7]=='=')
		return IIO_FORMAT_VIC; // VICAR (a streamlined PDS variant)

	if (b[0]=='S' && b[1]=='I' && b[2]=='M' && b[3]=='P' &&
			b[4]=='L' && b[5]=='E' && b[6]==' ' && b[7]==' ')
		return IIO_FORMAT_FIT; // FITS (standard astronomical images)

	for (int j = 0; j < 3; j++) // allons-y chochotte
	if (b[2-j]=='C' && b[3-j]=='C' && b[4-j]=='S' && b[5-j]=='D' &&
			b[6-j]=='3' && b[7-j]=='Z')
		return IIO_FORMAT_CCS; // CCSD (an ancient PDS-type label)

	if (!strchr((char*)b, '\n')) // protect against very short ASC headers
	{
		int cx = 8;
		for (; cx <= 11; cx++)
		{
			b[cx] = add_to_header_buffer(f, b, nbuf, bufmax);
			if (b[cx] == '\n')
				break;
		}
		if (cx == 12)
		{
			if (b[8]=='F'&&b[9]=='L'&&b[10]=='O'&&b[11]=='A')
				return IIO_FORMAT_LUM;
			if (b[8]=='1'&&b[9]=='2'&&b[10]=='L'&&b[11]=='I')
				return IIO_FORMAT_LUM;
		}
	}

	//b[8] = add_to_header_buffer(f, b, nbuf, bufmax);
	//b[9] = add_to_header_buffer(f, b, nbuf, bufmax);
	//b[10] = add_to_header_buffer(f, b, nbuf, bufmax);
	//b[11] = add_to_header_buffer(f, b, nbuf, bufmax);


	if (!strchr((char*)b, '\n'))
		line_to_header_buffer(f, b, nbuf, bufmax);
	int t[4];

	bool maybe_asc = false;
	if (4 == sscanf((char*)b, "%d %d %d %d\n", t, t+1, t+2, t+3) && t[2]==1)
		maybe_asc = true;

	// fill the rest of the buffer, for computing statistics
	while (*nbuf < bufmax)
		add_to_header_buffer(f, b, nbuf, bufmax);

	// TODO: cleanup the following logic (make CSV a particular case of DLM)

	bool buffer_statistics_agree_with_csv(uint8_t*, int);
	if (buffer_statistics_agree_with_csv(b, bufmax))
		return IIO_FORMAT_CSV;

	bool buffer_statistics_agree_with_dlm(uint8_t*, int);
	bool decide_if_asc_is_actually_txt(uint8_t*, int);
	if (buffer_statistics_agree_with_dlm(b, bufmax))
	{
		bool maybe_txt = decide_if_asc_is_actually_txt(b, bufmax);
		if (maybe_txt) return IIO_FORMAT_TXT;
		if (maybe_asc) return IIO_FORMAT_ASC;
//		return IIO_FORMAT_DLM;
	}


	return IIO_FORMAT_UNRECOGNIZED;
}

bool decide_if_asc_is_actually_txt(uint8_t *s, int n)
{
	// This function resolves an ambiguity between the ASC and the TXT
	// formats.  Both formats represent images using numbers written in
	// ascii.  The ASC format has a header of four numbers representing the
	// sizes of each of the four dimensions.  The TXT format has no header,
	// the image is assumed to have only two dimensions and each row of the
	// images is written in a separate line.  Notice that a TXT of width 4
	// can be confused by a ASC file.
	//
	// The present function resolves this ambiguity using the following
	// criterion: if the second line of the image has exactly four numbers,
	// then it is assumed to be a two-dimensional TXT image.
	//

	// a = number of spaces on the first line (always a==3)
	int a = 0;
	int i = 0;
	for (i = 0; i < n; i++)
		if (s[i] == '\n')
			break;
		else if (s[i] == ' ')
			a += 1;

	// b = number of spaces on the second line
	int b = 0;
	for (i++ ; i < n; i++)
		if (s[i] == '\n')
			break;
		else if (s[i] == ' ')
			b += 1;

	IIO_DEBUG("asc/txt a=%d b=%d (i=%d)\n", a, b, i);

	return a != 3 || a == b;
}

bool buffer_statistics_agree_with_csv(uint8_t *b, int n)
{
	char tmp[n+1];
	memcpy(tmp, b, n);
	tmp[n] = '\0';
	return ((size_t)n == strspn(tmp, "0123456789.e+-,naifNAIF\n"));
	//IIO_DEBUG("strcspn(\"%s\") = %d\n", tmp, r);
}

bool buffer_statistics_agree_with_dlm(uint8_t *b, int n)
{
	char tmp[n+1];
	memcpy(tmp, b, n);
	tmp[n] = '\0';
	return ((size_t)n == strspn(tmp, "0123456789.eE+- naifNAIF\n"));
	//IIO_DEBUG("strcspn(\"%s\") = %d\n", tmp, r);
}

static bool seekable_filenameP(const char *filename)
{
	if (filename[0] == '-')
		return false;
#ifdef I_CAN_POSIX
	FILE *f = fopen(filename, "r");
	if (!f) return false;
	int r = fseek(f, 0, SEEK_CUR);
	fclose(f);
	return r != -1;
#else
	return true;
#endif
}

static bool comma_named_tiff(const char *filename)
{
	char *comma = strrchr(filename, ',');
	if (!comma) return false;

	int lnumber = strlen(comma + 1);
	int ldigits = strspn(comma + 1, "0123456789");
	if (lnumber != ldigits) return false;

	char rfilename[FILENAME_MAX];
	snprintf(rfilename, FILENAME_MAX, "%s", filename);
	comma = rfilename + (comma - filename);
	*comma = '\0';

	bool retval = false;
	if (seekable_filenameP(rfilename)) {
		FILE *f = xfopen(rfilename, "r");
		int bufmax = 0x100, nbuf, format;
		char buf[0x100] = {0};
		format = guess_format(f, buf, &nbuf, bufmax);
		retval = format == IIO_FORMAT_TIFF;
		xfclose(f);
	}
	return retval;
}


#ifdef I_CAN_HAS_LIBHDF5
static bool comma_named_hdf5(const char *filename)
{
	IIO_DEBUG("hdf5 try \"%s\"\n", filename);

	char *comma = strrchr(filename, ',');
	if (!comma) return false;

	//int lnumber = strlen(comma + 1);
	//int ldigits = strspn(comma + 1, "0123456789");
	//if (lnumber != ldigits) return false;

	char rfilename[FILENAME_MAX];
	snprintf(rfilename, FILENAME_MAX, "%s", filename);
	comma = rfilename + (comma - filename);
	*comma = '\0';

	IIO_DEBUG("hdf5 still trying\n\trfilename=\"%s\"\n\tcomma=\"%s\"\n",
			rfilename, comma+1);

	bool retval = false;
	bool skbl = seekable_filenameP(rfilename);
	IIO_DEBUG("skbl = %d\n", skbl);
	if (skbl) {
		FILE *f = xfopen(rfilename, "r");
		int bufmax = 0x100, nbuf, format;
		char buf[0x100] = {0};
		format = guess_format(f, buf, &nbuf, bufmax);
		retval = format == IIO_FORMAT_HDF5;
		IIO_DEBUG("retval = %d\n", retval);
		xfclose(f);
	}
	return retval;
}
#endif//I_CAN_HAS_LIBHDF5

// dispatcher                                                               {{{1

// "centralized dispatcher"
static
int read_beheaded_image(struct iio_image *x, FILE *f, char *h, int hn, int fmt)
{
	IIO_DEBUG("rbi fmt = %d\n", fmt);
	// these functions can be defined in separate, independent files
	// TODO: turn this function into an array of pointers to functions,
	// indexed by a format enum
	switch(fmt) {
	case IIO_FORMAT_QNM:   return read_beheaded_qnm (x, f, h, hn);
	case IIO_FORMAT_RIM:   return read_beheaded_rim (x, f, h, hn);
	case IIO_FORMAT_PFM:   return read_beheaded_pfm (x, f, h, hn);
	case IIO_FORMAT_FLO:   return read_beheaded_flo (x, f, h, hn);
	case IIO_FORMAT_JUV:   return read_beheaded_juv (x, f, h, hn);
	case IIO_FORMAT_LUM:   return read_beheaded_lum (x, f, h, hn);
	case IIO_FORMAT_PCM:   return read_beheaded_pcm (x, f, h, hn);
	case IIO_FORMAT_ASC:   return read_beheaded_asc (x, f, h, hn);
	case IIO_FORMAT_TXT:   return read_beheaded_txt (x, f, h, hn);
	case IIO_FORMAT_BMP:   return read_beheaded_bmp (x, f, h, hn);
	case IIO_FORMAT_PDS:   return read_beheaded_pds (x, f, h, hn);
	case IIO_FORMAT_RAW:   return read_beheaded_raw (x, f, h, hn);
	case IIO_FORMAT_CSV:   return read_beheaded_csv (x, f, h, hn);
	case IIO_FORMAT_VRT:   return read_beheaded_vrt (x, f, h, hn);
	case IIO_FORMAT_FFD:   return read_beheaded_ffd (x, f, h, hn);
	case IIO_FORMAT_DLM:   return read_beheaded_dlm (x, f, h, hn);
	case IIO_FORMAT_NPY:   return read_beheaded_npy (x, f, h, hn);
	case IIO_FORMAT_RAT:   return read_beheaded_rat (x, f, h, hn);
	case IIO_FORMAT_VIC:   return read_beheaded_vic (x, f, h, hn);
	case IIO_FORMAT_FIT:   return read_beheaded_fit (x, f, h, hn);
	case IIO_FORMAT_CCS:   return read_beheaded_ccs (x, f, h, hn);

#ifdef I_CAN_HAS_LIBPNG
	case IIO_FORMAT_PNG:   return read_beheaded_png (x, f, h, hn);
#endif

#ifdef I_CAN_HAS_LIBJPEG
	case IIO_FORMAT_JPEG:  return read_beheaded_jpeg(x, f, h, hn);
#endif

#ifdef I_CAN_HAS_LIBTIFF
	case IIO_FORMAT_TIFF:  return read_beheaded_tiff(x, f, h, hn);
#endif

#ifdef I_CAN_HAS_LIBEXR
	case IIO_FORMAT_EXR:   return read_beheaded_exr (x, f, h, hn);
#endif

#ifdef I_CAN_HAS_LIBHDF5
	case IIO_FORMAT_HDF5:   return read_beheaded_hdf5 (x, f, h, hn);
#endif

#ifdef I_CAN_HAS_LIBWEBP
	case IIO_FORMAT_WEBP:   return read_beheaded_webp (x, f, h, hn);
#endif

#ifdef I_CAN_HAS_LIBAVIF
	case IIO_FORMAT_AVIF:   return read_beheaded_avif (x, f, h, hn);
#endif

#ifdef I_CAN_HAS_LIBHEIF
	case IIO_FORMAT_HEIF:   return read_beheaded_heif (x, f, h, hn);
#endif


	/*
	case IIO_FORMAT_JP2:   return read_beheaded_jp2 (x, f, h, hn);
	case IIO_FORMAT_VTK:   return read_beheaded_vtk (x, f, h, hn);
	case IIO_FORMAT_CIMG:  return read_beheaded_cimg(x, f, h, hn);
	case IIO_FORMAT_PAU:   return read_beheaded_pau (x, f, h, hn);
	case IIO_FORMAT_DICOM: return read_beheaded_dicom(x, f, h, hn);
	case IIO_FORMAT_NIFTI: return read_beheaded_nifti(x, f, h, hn);
	case IIO_FORMAT_PCX:   return read_beheaded_pcx (x, f, h, hn);
	case IIO_FORMAT_GIF:   return read_beheaded_gif (x, f, h, hn);
	case IIO_FORMAT_XPM:   return read_beheaded_xpm (x, f, h, hn);
	case IIO_FORMAT_RAFA:   return read_beheaded_rafa (x, f, h, hn);
	*/

#ifdef I_CAN_HAS_WHATEVER
	case IIO_FORMAT_UNRECOGNIZED: return read_beheaded_whatever(x,f,h,hn);
#else
	case IIO_FORMAT_UNRECOGNIZED: return -2;
#endif

	default:              return -17;
	}
}





// general image reader                                                     {{{1



//
// This function is the core of the library.
// Nearly everything passes through here (except the "raw"  images)
//
static int read_image_f(struct iio_image *x, FILE *f)
{
	int bufmax = 0x100, nbuf, format;
	char buf[0x100] = {0};
	format = guess_format(f, buf, &nbuf, bufmax);
	IIO_DEBUG("iio file format guess: %s {%d}\n", iio_strfmt(format), nbuf);
	//assert(nbuf > 0);
	return read_beheaded_image(x, f, buf, nbuf, format);
}

static int read_image(struct iio_image *x, const char *fname)
{
	int r; // the return-value of this function, zero if it succeeded

	IIO_DEBUG("read image \"%s\"\n", fname);

#ifndef IIO_ABORT_ON_ERROR
	if (setjmp(global_jump_buffer)) {
		IIO_DEBUG("SOME ERROR HAPPENED AND WAS HANDLED\n");
		return 1;
	}
	//if (iio_single_jmpstuff(false, true)) {
	//	IIO_DEBUG("SOME ERROR HAPPENED AND WAS HANDLED\n");
	//	exit(42);
	//}
#endif//IIO_ABORT_ON_ERROR

	// check for semantical name
	if (fname == strstr(fname, "zero:")) {
		int s[2], pd = 1;
		if (3 == sscanf(fname+5, "%dx%d,%d", s, s+1, &pd));
		else if (2 == sscanf(fname+5, "%dx%d", s, s+1));
		else fail("bad semantical name \"%s\"", fname);
		iio_image_build_independent(x, 2, s, IIO_TYPE_CHAR, pd);
		for (int i = 0; i < *s*s[1]*pd; i++)
			((char*)x->data)[i] = 0;
		return 0;
	}
	if (fname == strstr(fname, "one:")) {
		int s[2], pd = 1;
		if (3 == sscanf(fname+4, "%dx%d,%d", s, s+1, &pd));
		else if (2 == sscanf(fname+4, "%dx%d", s, s+1));
		else fail("bad semantical name \"%s\"", fname);
		iio_image_build_independent(x, 2, s, IIO_TYPE_CHAR, pd);
		for (int i = 0; i < *s*s[1]*pd; i++)
			((char*)x->data)[i] = 1;
		return 0;
	}
	if (fname == strstr(fname, "constant:")) {
		float value;
		int s[2], pd = 1;
		if (3 == sscanf(fname+9, "%g:%dx%d", &value, s, s+1));
		else fail("bad semantical name \"%s\"", fname);
		iio_image_build_independent(x, 2, s, IIO_TYPE_FLOAT, pd);
		for (int i = 0; i < *s*s[1]*pd; i++)
			((float*)x->data)[i] = value;
		return 0;
	}

	IIO_DEBUG("read image(no magic) \"%s\"\n", fname);

#ifdef I_CAN_HAS_WGET
	// check for URL
	if (fname == strstr(fname, "http://")
			|| fname==strstr(fname, "https://") ) {
		IIO_DEBUG("read image(wget) \"%s\"\n", fname);
		// TODO: for security, sanitize the fname
		int cmd_len = 2*FILENAME_MAX + 20;
		char tfn[cmd_len], cmd[cmd_len];
		fill_temporary_filename(tfn);
		snprintf(cmd, cmd_len, "wget %s -q -O %s", fname, tfn);
		int rsys = system(cmd);
		if (rsys != 0) fail("system wget returned %d", rsys);
		FILE *f = xfopen(tfn, "r");
		r = read_image_f(x, f);
		xfclose(f);
		delete_temporary_file(tfn);
		return 0;
	} else
#endif//I_CAN_HAS_WGET

	IIO_DEBUG("read image(no wget) \"%s\"\n", fname);

	if (false) {
		;
#ifdef I_CAN_HAS_LIBTIFF
	} else if (comma_named_tiff(fname)) {
		r = read_whole_tiff(x, fname);
#endif//I_CAN_HAS_LIBTIFF
#ifdef I_CAN_HAS_LIBHDF5
	} else if (comma_named_hdf5(fname)) {
		IIO_DEBUG("hdf5 OK\n");
		r = read_whole_hdf5(x, fname);
#endif//I_CAN_HAS_LIBHDF5
#ifdef I_USE_LIBRAW
	} else if (try_reading_file_with_libraw(fname, x)) {
		r=0;
#endif//I_USE_LIBRAW
	} else if (raw_prefix(fname)) {
		r = read_raw_named_image(x, fname);
	} else if (trans_prefix(fname)) {
		int r0 = read_image(x, 1 + trans_prefix(fname)); // recursive
		int r1 = trans_apply(x, 6 + fname); // omit prefix "TRANS["
		r = r0 + r1;
	} else {
		// call CORE
		IIO_DEBUG("read image(core) \"%s\"\n", fname);
		FILE *f = xfopen(fname, "r");
		r = read_image_f(x, f);
		xfclose(f);
	}

	IIO_DEBUG("READ IMAGE return value = %d\n", r);
	if (r) fail("read_image failed r = %d", r);
	IIO_DEBUG("READ IMAGE dimension = %d\n", x->dimension);
	switch(x->dimension) {
	case 1: IIO_DEBUG("READ IMAGE sizes = %d\n",x->sizes[0]);break;
	case 2: IIO_DEBUG("READ IMAGE sizes = %d x %d\n",x->sizes[0],x->sizes[1]);break;
	case 3: IIO_DEBUG("READ IMAGE sizes = %d x %d x %d\n",x->sizes[0],x->sizes[1],x->sizes[2]);break;
	case 4: IIO_DEBUG("READ IMAGE sizes = %d x %d x %d x %d\n",x->sizes[0],x->sizes[1],x->sizes[2],x->sizes[3]);break;
	default: fail("invalid dimension [%d]", x->dimension);
	}
	IIO_DEBUG("READ IMAGE pixel_dimension = %d\n",x->pixel_dimension);
	IIO_DEBUG("READ IMAGE type = %s\n", iio_strtyp(x->type));

	char *trans = xgetenv("IIO_TRANS");
	if (trans && *trans)
		r += trans_apply(x, trans);


	return r;
}


static void iio_write_image_default(const char *filename, struct iio_image *x);



// API (input)                                                              {{{1

static void *rfail(const char *fmt, ...)
{
#ifdef IIO_ABORT_ON_ERROR
	va_list argp;
	va_start(argp, fmt);
	fail(fmt, argp);
	va_end(argp);
#else
	(void)fmt;
#endif
	return NULL;
}

//// 2D only
//static
//void *iio_read_image(const char *fname, int *w, int *h, int desired_sample_type)
//{
//	struct iio_image x[1];
//	int r = read_image(x, fname);
//	if (r) return rfail("could not read image");
//	if (x->dimension != 2) {
//		x->dimension = 2;
//	}
//	*w = x->sizes[0];
//	*h = x->sizes[1];
//	iio_convert_samples(x, desired_sample_type);
//	return x->data;
//}

// API 2D
float *iio_read_image_float_vec(const char *fname, int *w, int *h, int *pd)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
	}
	*w = x->sizes[0];
	*h = x->sizes[1];
	*pd = x->pixel_dimension;
	iio_convert_samples(x, IIO_TYPE_FLOAT);
	return x->data;
}

// API 2D
float *iio_read_image_float_split(const char *fname, int *w, int *h, int *pd)
{
	float *r = iio_read_image_float_vec(fname, w, h, pd);
	if (!r) return rfail("could not read image");
	float *rbroken = xmalloc(*w**h**pd*sizeof*rbroken);
	break_pixels_float(rbroken, r, *w**h, *pd);
	xfree(r);
	return rbroken;
}

// API 2D
float *iio_read_image_float_rgb(const char *fname, int *w, int *h)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	if (x->pixel_dimension != 3) {
		iio_hacky_colorize(x, 3);
	}
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_FLOAT);
	return x->data;
}

// API 2D
double *iio_read_image_double_vec(const char *fname, int *w, int *h, int *pd)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
	}
	*w = x->sizes[0];
	*h = x->sizes[1];
	*pd = x->pixel_dimension;
	iio_convert_samples(x, IIO_TYPE_DOUBLE);
	return x->data;
}

// API 2D
uint8_t *iio_read_image_uint8_vec(const char *fname, int *w, int *h, int *pd)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
	}
	*w = x->sizes[0];
	*h = x->sizes[1];
	*pd = x->pixel_dimension;
	iio_convert_samples(x, IIO_TYPE_UINT8);
	return x->data;
}

// API 2D
uint16_t *iio_read_image_uint16_vec(const char *fname, int *w, int *h, int *pd)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
	}
	*w = x->sizes[0];
	*h = x->sizes[1];
	*pd = x->pixel_dimension;
	iio_convert_samples(x, IIO_TYPE_UINT16);
	return x->data;
}

// API 2D
double *iio_read_image_double_split(const char *fname, int *w, int *h, int *pd)
{
	double *r = iio_read_image_double_vec(fname, w, h, pd);
	if (!r) return rfail("could not read image");
	double *rbroken = xmalloc(*w**h**pd*sizeof*rbroken);
	break_pixels_double(rbroken, r, *w**h, *pd);
	xfree(r);
	return rbroken;
}

// API 2D
uint8_t (*iio_read_image_uint8_rgb(const char *fname, int *w, int *h))[3]
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		fail("non 2d image");
	}
	if (x->pixel_dimension != 3)
		fail("non-color image");
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_UINT8);
	return x->data;
}

// API 2D
uint8_t (**iio_read_image_uint8_matrix_rgb(const char *fnam, int *w, int *h))[3]
{
	struct iio_image x[1];
	int r = read_image(x, fnam);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	if (x->pixel_dimension != 3) {
		iio_hacky_colorize(x, 3);
	}
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_UINT8);
	return wrap_2dmatrix_around_data(x->data, *w, *h, 3);
}

// API 2D
float (**iio_read_image_float_matrix_rgb(const char *fnam, int *w, int *h))[3]
{
	struct iio_image x[1];
	int r = read_image(x, fnam);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	if (x->pixel_dimension != 3) {
		iio_hacky_colorize(x, 3);
	}
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_FLOAT);
	return wrap_2dmatrix_around_data(x->data, *w, *h, 3*sizeof(float));
}

// API 2D
uint8_t ***iio_read_image_uint8_matrix_vec(const char *fname,
	       	int *w, int *h, int *pd)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	*w = x->sizes[0];
	*h = x->sizes[1];
	*pd = x->pixel_dimension;
	//fprintf(stderr, "matrix_vec pd = %d\n", *pd);
	iio_convert_samples(x, IIO_TYPE_UINT8);
	return wrap_2dmatrix_around_data(x->data, *w, *h, *pd);
}

// API 2D
void *iio_read_image_float_matrix_vec(const char *fname,
	       	int *w, int *h, int *pd)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	*w = x->sizes[0];
	*h = x->sizes[1];
	*pd = x->pixel_dimension;
	iio_convert_samples(x, IIO_TYPE_FLOAT);
	return wrap_2dmatrix_around_data(x->data, *w, *h, *pd*sizeof(float));
}

// API 2D
uint8_t **iio_read_image_uint8_matrix(const char *fname, int *w, int *h)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	if (x->pixel_dimension == 3)
		iio_hacky_uncolorize(x);
	if (x->pixel_dimension != 1)
		fail("non-scalar image");
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_UINT8);
	return wrap_2dmatrix_around_data(x->data, *w, *h, 1);
}

float **iio_read_image_float_matrix(const char *fname, int *w, int *h)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	if (x->pixel_dimension == 3)
		iio_hacky_uncolorize(x);
	if (x->pixel_dimension != 1)
		return rfail("non-scalar image");
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_FLOAT);
	return wrap_2dmatrix_around_data(x->data, *w, *h, sizeof(float));
}

// API nd general
void *iio_read_nd_image_as_stored(char *fname,
		int *dimension, int *sizes,
		int *samples_per_pixel, int *sample_size,
		bool *ieefp_samples, bool *signed_samples)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("so much fail");
	*dimension = x->dimension;
	FORI(x->dimension) sizes[i] = x->sizes[i];
	*samples_per_pixel = x->pixel_dimension;
	iio_type_unid(sample_size, ieefp_samples, signed_samples, x->type);
	return x->data;
}

// API nd general
void *iio_read_nd_image_as_desired(char *fname,
		int *dimension, int *sizes,
		int *samples_per_pixel, int desired_sample_size,
		bool desired_ieeefp_samples, bool desired_signed_samples)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("so much fail");
	int desired_type = iio_type_id(desired_sample_size,
				desired_ieeefp_samples, desired_signed_samples);
	iio_convert_samples(x, desired_type);
	*dimension = x->dimension;
	FORI(x->dimension) sizes[i] = x->sizes[i];
	*samples_per_pixel = x->pixel_dimension;
	return x->data;
}

// API 2D
float *iio_read_image_float(const char *fname, int *w, int *h)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	if (x->pixel_dimension == 3)
		iio_hacky_uncolorize(x);
	if (x->pixel_dimension == 4)
		iio_hacky_uncolorizea(x);
	if (x->pixel_dimension != 1)
		return rfail("non-scalarizable image");
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_FLOAT);
	return x->data;
}

// API 2D
double *iio_read_image_double(const char *fname, int *w, int *h)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	if (x->pixel_dimension == 3)
		iio_hacky_uncolorize(x);
	if (x->pixel_dimension != 1)
		return rfail("non-scalar image");
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_DOUBLE);
	return x->data;
}

// API 2D
int *iio_read_image_int(const char *fname, int *w, int *h)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	if (x->pixel_dimension == 3)
		iio_hacky_uncolorize(x);
	if (x->pixel_dimension != 1)
		return rfail("non-scalar image");
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_INT);
	return x->data;
}

// API 2D
uint8_t *iio_read_image_uint8(const char *fname, int *w, int *h)
{
	struct iio_image x[1];
	int r = read_image(x, fname);
	if (r) return rfail("could not read image");
	if (x->dimension != 2) {
		x->dimension = 2;
		return rfail("non 2d image");
	}
	if (x->pixel_dimension == 3)
		iio_hacky_uncolorize(x);
	if (x->pixel_dimension == 4)
		iio_hacky_uncolorizea(x);
	if (x->pixel_dimension != 1)
		return rfail("non-scalarizable image");
	*w = x->sizes[0];
	*h = x->sizes[1];
	iio_convert_samples(x, IIO_TYPE_UINT8);
	return x->data;
}


// API (output)                                                             {{{1

//static bool this_float_is_actually_a_byte(float x)
//{
//	return (x == (int)(x)) && (x >= 0) && (x < 256);
//}

//static bool this_float_is_actually_a_short(float x)
//{
//	return (x == floor(x)) && (x >= 0) && (x < 65536);
//}

//static bool these_floats_are_actually_bytes(float *t, int n)
//{
//	IIO_DEBUG("checking %d floats for byteness (%p)\n", n, (void*)t);
//	FORI(n)
//		if (!this_float_is_actually_a_byte(t[i]))
//			return false;
//	return true;
//}

//static bool these_floats_are_actually_shorts(float *t, int n)
//{
//	IIO_DEBUG("checking %d floats for shortness (%p)\n", n, (void*)t);
//	FORI(n)
//		if (!this_float_is_actually_a_short(t[i]))
//			return false;
//	return true;
//}

static bool string_suffix(const char *s, const char *suf)
{
	int len_s = strlen(s);
	int len_suf = strlen(suf);
	if (len_s < len_suf)
		return false;
	return 0 == strcmp(suf, s + (len_s - len_suf));
}

// if f ~ /REM[.*]:.*/ return the position of the colon
static char *rem_prefix(const char *f)
{
	if (f != strstr(f, "REM["))
		return NULL;
	char *colon = strchr(f, ':');
	if (!colon || colon[-1] != ']')
		return NULL;
	return colon;
}

// Note:
// This function was written without being designed.  See file "saving.txt" for
// an attempt at designing it.
static void iio_write_image_default(const char *filename, struct iio_image *x)
{
	IIO_DEBUG("going to write into filename \"%s\"\n", filename);
	int typ = normalize_type(x->type);
	char rem_text[FILENAME_MAX]; //XXX: fails for recursive calls below
	if (x->dimension != 2) fail("de moment només escrivim 2D");
	if (!strcmp(filename,"-") && isatty(1))
	{
		if (x->sizes[0] <= 855 && x->sizes[1] <= 800 &&
			(x->pixel_dimension==3 || x->pixel_dimension==1))
			dump_sixels_to_stdout(x);
		else
			printf("IMAGE %dx%d,%d %s\n",
					x->sizes[0], x->sizes[1],
					x->pixel_dimension,
					iio_strtyp(x->type));
		return;
	}
	x->rem = xgetenv("IIO_REM");
	if (rem_prefix(filename)) {
		char *colon = rem_prefix(filename);
		int remlen = colon - (filename+4) - 1;
		for (int i = 0; i < remlen; i++)
			rem_text[i] = filename[4+i];
		rem_text[remlen] = 0;
		filename = 1 + colon;
		x->rem = rem_text; // disappears when out of scope
		IIO_DEBUG("rem filename = %s\n", filename);
		IIO_DEBUG("rem text = %s\n", rem_text);
		IIO_DEBUG("rem len = %d\n", remlen);
	}
	if (x->rem)
		IIO_DEBUG("rem = \"%s\"\n", x->rem);
	if (string_suffix(filename, ".uv") && typ == IIO_TYPE_FLOAT
				&& x->pixel_dimension == 2) {
		iio_write_image_as_juv(filename, x);
		return;
	}
	if (string_suffix(filename, ".flo") && typ == IIO_TYPE_FLOAT
				&& x->pixel_dimension == 2) {
		iio_write_image_as_flo(filename, x);
		return;
	}
	if (string_suffix(filename, ".ppm") && typ == IIO_TYPE_FLOAT
		&& (x->pixel_dimension == 1 || x->pixel_dimension == 3)) {
		iio_write_image_as_ppm(filename, x);
		return;
	}
	if (string_suffix(filename, ".pgm") && typ == IIO_TYPE_FLOAT
		&& (x->pixel_dimension == 1 || x->pixel_dimension == 3)) {
		iio_write_image_as_ppm(filename, x);
		return;
	}
	if (string_suffix(filename, ".pfm") && typ == IIO_TYPE_FLOAT
		&& (x->pixel_dimension == 1 || x->pixel_dimension == 3)) {
		iio_write_image_as_pfm(filename, x);
		return;
	}
	if (string_suffix(filename, ".csv") &&
			(typ==IIO_TYPE_FLOAT || typ==IIO_TYPE_DOUBLE ||
			 typ==IIO_TYPE_UINT8)
				&& x->pixel_dimension == 1) {
		iio_write_image_as_csv(filename, x);
		return;
	}
	if (string_suffix(filename, ".txt") &&
			(typ==IIO_TYPE_FLOAT || typ==IIO_TYPE_DOUBLE ||
			 typ==IIO_TYPE_UINT8)
				&& (x->pixel_dimension == 1
					|| x->sizes[0] == 1
					|| x->sizes[1] == 1
				   )
	   ) {
		iio_write_image_as_txt(filename, x);
		return;
	}
	if (string_suffix(filename, ".raw")) {
		iio_write_image_as_raw(filename, x);
		return;
	}
	if (string_suffix(filename, ".mw") && typ == IIO_TYPE_FLOAT
				&& x->pixel_dimension == 1) {
		iio_write_image_as_rim_fimage(filename, x);
		return;
	}
	if (string_suffix(filename, ".mw") && typ == IIO_TYPE_UINT8
				&& x->pixel_dimension == 1) {
		iio_write_image_as_rim_cimage(filename, x);
		return;
	}
	if (string_suffix(filename, ".asc") &&
			(typ == IIO_TYPE_FLOAT || typ == IIO_TYPE_DOUBLE)
				) {
		iio_write_image_as_asc(filename, x);
		return;
	}

	int nsamp = iio_image_number_of_samples(x);

	// NOTE: this is removed because it interferes with the static rem
	// array above.  Nothing is really lost, this was a silly "optimization
	// anyways".  For formats that require it, this quantization is
	// performed non-recursively below.
	//
	//if (typ == IIO_TYPE_FLOAT &&
	//		these_floats_are_actually_bytes(x->data, nsamp))
	//{
	//	IIO_DEBUG("recursive call for byte floats\n");
	//	void *old_data = x->data;
	//	x->data = xmalloc(nsamp*sizeof(float));
	//	memcpy(x->data, old_data, nsamp*sizeof(float));
	//	iio_convert_samples(x, IIO_TYPE_UINT8);
	//	iio_write_image_default(filename, x); // recursive call
	//	xfree(x->data);
	//	x->data = old_data;
	//	return;
	//}

	if (string_suffix(filename, ".npy")) {
		iio_write_image_as_npy(filename, x);
		return;
	}
	if (true) {
		char *txtname = strstr(filename, "TXT:");
		if (txtname == filename) {
			IIO_DEBUG("TXT prefix detected\n");
			iio_write_image_as_txt(filename+4, x);
			return;
		}
	}
	if (true) {
		char *txtname = strstr(filename, "RAW:");
		if (txtname == filename) {
			IIO_DEBUG("RAW prefix detected\n");
			iio_write_image_as_raw(filename+4, x);
			return;
		}
	}
#ifdef I_CAN_HAS_LIBTIFF
	if (true) {
		if (false
				|| string_suffix(filename, ".tiff")
				|| string_suffix(filename, ".tif")
				|| string_suffix(filename, ".TIFF")
				|| string_suffix(filename, ".TIF")
		   )
		{
			IIO_DEBUG("tiff extension detected\n");
			iio_write_image_as_tiff_smarter(filename, x);
			return;
		}
	}
	if (true) {
		char *tiffname = strstr(filename, "TIFF:");
		if (tiffname == filename) {
			IIO_DEBUG("TIFF prefix detected\n");
			iio_write_image_as_tiff_smarter(filename+5, x);
			return;
		}
	}
#endif//I_CAN_HAS_LIBTIFF
#ifdef I_CAN_HAS_LIBPNG
	if (true) {
		char *pngname = strstr(filename, "PNG:");
		if (pngname == filename) {
			IIO_DEBUG("PNG prefix detected\n");
			if (typ != IIO_TYPE_UINT8) {
				void *old_data = x->data;
				int ss = iio_image_sample_size(x);
				x->data = xmalloc(nsamp*ss);
				memcpy(x->data, old_data, nsamp*ss);
				iio_convert_samples(x, IIO_TYPE_UINT8);
				iio_write_image_as_png(filename+4, x);
				xfree(x->data);
				x->data = old_data;
				return;
			}
			iio_write_image_as_png(filename+4, x);
			return;
		}
	}
	if (true) {
		char *pngname = strstr(filename, "PNG16:");
		if (pngname == filename) {
			IIO_DEBUG("PNG16 prefix detected\n");
			if (typ != IIO_TYPE_UINT16) {
				void *old_data = x->data;
				int ss = iio_image_sample_size(x);
				x->data = xmalloc(nsamp*ss);
				memcpy(x->data, old_data, nsamp*ss);
				iio_convert_samples(x, IIO_TYPE_UINT16);
				iio_write_image_as_png(filename+6, x);
				xfree(x->data);
				x->data = old_data;
				return;
			}
			iio_write_image_as_png(filename+6, x);
			return;
		}
	}
	if (true) {
		if (false
				|| string_suffix(filename, ".png")
				|| string_suffix(filename, ".PNG")
			//	|| (typ==IIO_TYPE_UINT8&&x->pixel_dimension==4)
			//	|| (typ==IIO_TYPE_UINT8&&x->pixel_dimension==2)
			//	|| (typ==IIO_TYPE_UINT8&&x->pixel_dimension==1)
			//	|| (typ==IIO_TYPE_UINT8&&x->pixel_dimension==3)
		   )
		{
			IIO_DEBUG("png extension detected or 8bint thing\n");
			if (typ != IIO_TYPE_UINT8) {
				void *old_data = x->data;
				int ss = iio_image_sample_size(x);
				x->data = xmalloc(nsamp*ss);
				memcpy(x->data, old_data, nsamp*ss);
				iio_convert_samples(x, IIO_TYPE_UINT8);
				iio_write_image_as_png(filename, x);
				xfree(x->data);
				x->data = old_data;
				return;
			}
			iio_write_image_as_png(filename, x);
			return;
		}
	}
#endif//I_CAN_HAS_LIBPNG
#ifdef I_CAN_HAS_LIBJPEG
	if (string_suffix(filename, ".jpg") || string_suffix(filename, ".jpeg"))
	{
		IIO_DEBUG("jpeg extension detected\n");
		if (typ != IIO_TYPE_UINT8) {
			void *old_data = x->data;
			int ss = iio_image_sample_size(x);
			x->data = xmalloc(nsamp*ss);
			memcpy(x->data, old_data, nsamp*ss);
			iio_convert_samples(x, IIO_TYPE_UINT8);
			iio_write_image_as_jpeg(filename, x);
			xfree(x->data);
			x->data = old_data;
			return;
		}
		iio_write_image_as_jpeg(filename, x);
		return;
	}
#endif//I_CAN_HAS_LIBJPEG
	IIO_DEBUG("SIDEF:\n");
//#ifdef IIO_SHOW_DEBUG_MESSAGES
//	iio_print_image_info(stderr, x);
//#endif
	//FILE *f = xfopen(filename, "w");
	//if (x->pixel_dimension == 1 && typ == IIO_TYPE_FLOAT) {
	//	int m = these_floats_are_actually_bytes(x->data,x->sizes[0]*x->sizes[1]) ? 255 : 65535;
	//	fprintf(f, "P2\n%d %d\n%d\n", x->sizes[0], x->sizes[1], m);
	//	float *data = x->data;
	//	FORI(x->sizes[0]*x->sizes[1])
	//		fprintf(f, "%a\n", data[i]);
	//} else if (x->pixel_dimension == 3 && typ == IIO_TYPE_FLOAT) {
	//	int m = these_floats_are_actually_bytes(x->data,3*x->sizes[0]*x->sizes[1]) ? 255 : 65535;
	//	float *data = x->data;
	//	fprintf(f, "P3\n%d %d\n%d\n", x->sizes[0], x->sizes[1], m);
	//	FORI(3*x->sizes[0]*x->sizes[1]) {
	//		fprintf(f, "%g\n", data[i]);
	//	}
	//} else if (x->pixel_dimension == 3 && typ == IIO_TYPE_UINT8) {
	//	uint8_t *data = x->data;
	//	int w = x->sizes[0];
	//	int h = x->sizes[1];
	//	if (w*h <= 10000) {
	//		fprintf(f, "P3\n%d %d\n255\n", w, h);
	//		FORI(3*w*h) {
	//			int datum = data[i];
	//			fprintf(f, "%d\n", datum);
	//		}
	//	} else {
	//		fprintf(f, "P6\n%d %d\n255\n", w, h);
	//		fwrite(data, 3*w*h, 1, f);
	//	}
	//} else if (x->pixel_dimension == 4 && typ == IIO_TYPE_UINT8) {
	//	fprintf(stderr, "IIO WARNING: assuming 4 chanels mean RGBA\n");
	//	uint8_t *data = x->data;
	//	fprintf(f, "P3\n%d %d\n255\n", x->sizes[0], x->sizes[1]);
	//	FORI(4*x->sizes[0]*x->sizes[1]) {
	//		if (i%4 == 3) continue;
	//		int datum = data[i];
	//		fprintf(f, "%d\n", datum);
	//	}
	//} else if (x->pixel_dimension == 1 && typ == IIO_TYPE_UINT8) {
	//	uint8_t *data = x->data;
	//	int w = x->sizes[0];
	//	int h = x->sizes[1];
	//	if (w*h <= 10000) {
	//		fprintf(f, "P2\n%d %d\n255\n", w, h);
	//		FORI(w*h) {
	//			int datum = data[i];
	//			fprintf(f, "%d\n", datum);
	//		}
	//	} else {
	//		fprintf(f, "P5\n%d %d\n255\n", w, h);
	//		fwrite(data, w*h, 1, f);
	//	}
	//} else
		iio_write_image_as_npy(filename, x);
	//xfclose(f);
}

void iio_write_image_uint8_matrix_rgb(char *filename, uint8_t (**data)[3],
		int w, int h)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, 3, IIO_TYPE_UINT8);
	x->data = data[0][0];
	iio_write_image_default(filename, x);
}

void iio_write_image_uint8_matrix(char *filename, uint8_t **data, int w, int h)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, 1, IIO_TYPE_UINT8);
	x->data = data[0];
	iio_write_image_default(filename, x);
}

void iio_write_image_float_vec(char *filename, float *data,
		int w, int h, int pd)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, pd, IIO_TYPE_FLOAT);
	x->data = data;
	iio_write_image_default(filename, x);
}

void iio_write_image_float_split(char *filename, float *data,
		int w, int h, int pd)
{
	float *rdata = xmalloc(w*h*pd*sizeof*rdata);
	recover_broken_pixels_float(rdata, data, w*h, pd);
	iio_write_image_float_vec(filename, rdata, w, h, pd);
	xfree(rdata);
}

void iio_write_image_int_vec(char *filename, int *data,
		int w, int h, int pd)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, pd, IIO_TYPE_INT);
	x->data = data;
	iio_write_image_default(filename, x);
}

void iio_write_image_double_vec(char *filename, double *data,
		int w, int h, int pd)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, pd, IIO_TYPE_DOUBLE);
	x->data = data;
	iio_write_image_default(filename, x);
}

void iio_write_image_double_split(char *filename, double *data,
		int w, int h, int pd)
{
	double *rdata = xmalloc(w*h*pd*sizeof*rdata);
	recover_broken_pixels_double(rdata, data, w*h, pd);
	iio_write_image_double_vec(filename, rdata, w, h, pd);
	xfree(rdata);
}

void iio_write_image_int_split(char *filename, int *data,
		int w, int h, int pd)
{
	int *rdata = xmalloc(w*h*pd*sizeof*rdata);
	recover_broken_pixels_int(rdata, data, w*h, pd);
	iio_write_image_int_vec(filename, rdata, w, h, pd);
	xfree(rdata);
}

void iio_write_image_float(char *filename, float *data, int w, int h)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, 1, IIO_TYPE_FLOAT);
	x->data = data;
	iio_write_image_default(filename, x);
}

void iio_write_image_double(char *filename, double *data, int w, int h)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, 1, IIO_TYPE_DOUBLE);
	x->data = data;
	iio_write_image_default(filename, x);
}

void iio_write_image_int(char *filename, int *data, int w, int h)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, 1, IIO_TYPE_INT);
	x->data = data;
	iio_write_image_default(filename, x);
}

void iio_write_image_uint8_vec(char *filename, uint8_t *data,
		int w, int h, int pd)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, pd, IIO_TYPE_UINT8);
	x->data = data;
	iio_write_image_default(filename, x);
}

void iio_write_image_uint8_split(char *filename, uint8_t *data,
		int w, int h, int pd)
{
	uint8_t *rdata = xmalloc(w*h*pd*sizeof*rdata);
	recover_broken_pixels_uint8(rdata, data, w*h, pd);
	iio_write_image_uint8_vec(filename, rdata, w, h, pd);
	xfree(rdata);
}

void iio_write_image_uint16_vec(char *filename, uint16_t *data,
		int w, int h, int pd)
{
	struct iio_image x[1];
	iio_image_init2d(x, w, h, pd, IIO_TYPE_UINT16);
	x->data = data;
	iio_write_image_default(filename, x);
}

void iio_free(char *p)
{
	xfree(p);
}

// API (deprecated)                                                         {{{1
#ifdef IIO_USE_INCONSISTENT_NAMES
// code below generated by an ugly sed script
void iio_save_image_float_vec(char *filename, float *x, int w, int h, int pd)       {return iio_write_image_float_vec       (filename, x, w, h, pd); }
void iio_save_image_float_split(char *filename, float *x, int w, int h, int pd)     {return iio_write_image_float_split     (filename, x, w, h, pd); }
void iio_save_image_double_vec(char *filename, double *x, int w, int h, int pd)     {return iio_write_image_double_vec      (filename, x, w, h, pd); }
void iio_save_image_float(char *filename, float *x, int w, int h)                   {return iio_write_image_float           (filename, x, w, h); }
void iio_save_image_double(char *filename, double *x, int w, int h)                 {return iio_write_image_double          (filename, x, w, h); }
void iio_save_image_int(char *filename, int *x, int w, int h)                       {return iio_write_image_int             (filename, x, w, h); }
void iio_save_image_int_vec(char *filename, int *x, int w, int h, int pd)           {return iio_write_image_int_vec         (filename, x, w, h, pd); }
void iio_save_image_uint8_vec(char *filename, uint8_t *x, int w, int h, int pd)     {return iio_write_image_uint8_vec       (filename, x, w, h, pd); }
void iio_save_image_uint16_vec(char *filename, uint16_t *x, int w, int h, int pd)   {return iio_write_image_uint16_vec      (filename, x, w, h, pd); }
void iio_save_image_uint8_matrix_rgb(char *f, unsigned char (**x)[3], int w, int h) {return iio_write_image_uint8_matrix_rgb(f, x, w, h); }
void iio_save_image_uint8_matrix(char *f, unsigned char **x, int w, int h)          {return iio_write_image_uint8_matrix    (f, x, w, h); }
#endif//IIO_USE_INCONSISTENT_NAMES

// vim:set foldmethod=marker:
