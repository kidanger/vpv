/// this file is a copy-pasta of iio's internal functions

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npy.h"

#define I_CAN_HAS_INT64
#define I_CAN_HAS_LONGDOUBLE

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

#define FORI(n) for(int i=0;i<(int)(n);i++)
#define FORJ(n) for(int j=0;j<(int)(n);j++)
#define FORK(n) for(int k=0;k<(int)(n);k++)
#define FORL(n) for(int l=0;l<(int)(n);l++)

typedef long long longlong;
typedef long double longdouble;

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
	default: fprintf(stderr, "unrecognized type %d", type);
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

static void iio_type_unid(int *size, bool *ieefp, bool *signedness, int type)
{
	int t = normalize_type(type);
	*size = iio_type_size(t);
	*ieefp = (t==IIO_TYPE_HALF || t==IIO_TYPE_FLOAT
			|| t==IIO_TYPE_DOUBLE || t==IIO_TYPE_LONGDOUBLE);
	*signedness = (t==IIO_TYPE_INT8 || t==IIO_TYPE_INT16
			|| t==IIO_TYPE_INT32 || t==IIO_TYPE_INT64);
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


int npy_read_header(FILE *fin, struct npy_info* ni)
{
	// tag + fixed-size header
	char s[10];
	if (fread(s, 1, 10, fin) != 10)
		return 0;
	if (s[6] != 1 || s[7] != 0)
		return fprintf(stderr, "only NPY 1.0 is supported\n"), 0;

	// variable-size header
	int npy_header_size = s[8] + 0x100 * s[9];
	char npy_header[npy_header_size];
	if (fread(npy_header, 1, npy_header_size, fin) != npy_header_size)
		return 0;

	// extract fields from the npy header
	char descr[10];
	char order[10];
	int n;
	n = sscanf(npy_header, "{'descr': '%[^']', 'fortran_order': %[^,],"
			" 'shape': (%zu, %zu, %zu, %zu", descr, order,
			ni->dims+0, ni->dims+1, ni->dims+2, ni->dims+3);
	if (n < 3) return fprintf(stderr, "badly formed npy header\n"), 0;
	ni->ndims = n - 2;

	// parse type string
	int type;
	char *desc = descr; // pointer to the bare description
	if (*descr=='<' || *descr=='>' || *descr=='=' || *descr=='|')
		desc += 1;
	if (false) ;
	else if (0 == strcmp(desc, "f8")) type = IIO_TYPE_DOUBLE;
	else if (0 == strcmp(desc, "f4")) type = IIO_TYPE_FLOAT;
	else if (0 == strcmp(desc, "b1")) type = IIO_TYPE_UINT8;
	else if (0 == strcmp(desc, "u1")) type = IIO_TYPE_UINT8;
	else if (0 == strcmp(desc, "u2")) type = IIO_TYPE_UINT16;
	else if (0 == strcmp(desc, "u4")) type = IIO_TYPE_UINT32;
	else if (0 == strcmp(desc, "u8")) type = IIO_TYPE_UINT64;
	else if (0 == strcmp(desc, "i1")) type = IIO_TYPE_INT8;
	else if (0 == strcmp(desc, "i2")) type = IIO_TYPE_INT16;
	else if (0 == strcmp(desc, "i4")) type = IIO_TYPE_INT32;
	else if (0 == strcmp(desc, "i8")) type = IIO_TYPE_INT64;
	else if (0 == strcmp(desc, "c8")) type = IIO_TYPE_FLOAT;
	else if (0 == strcmp(desc, "c16")) type = IIO_TYPE_DOUBLE;
	else return fprintf(stderr,
			"IIO ERROR: unrecognized npy type \"%s\"\n", desc), 0;
	if (*desc == 'c') ni->dims[ni->ndims++] = 2; // 1 complex = 2 reals

	strncpy(ni->desc, descr, 10);
	ni->type = type;
	ni->fortran_order = 0;
	ni->header_offset = 10 + npy_header_size;
	return 1;
}

size_t npy_type_size(int type)
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
	default: fprintf(stderr, "unrecognized type %d", type);
	}
	return 0;
}

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

	default: fprintf(stderr, "bad conversion from %d to %d\n", src_fmt, dest_fmt);
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

float* npy_convert_to_float(void* src, int n, int src_fmt)
{
	int dest_fmt = IIO_TYPE_FLOAT;
	src_fmt = normalize_type(src_fmt);
	if (src_fmt == dest_fmt) return src;
	size_t src_width = iio_type_size(src_fmt);
	size_t dest_width = iio_type_size(dest_fmt);
	char *r = malloc(n * dest_width);
	for (int i = 0; i < n; i++) {
		void *to   = i * dest_width + r;
		void *from = i * src_width  + (char *)src;
		convert_datum(to, from, dest_fmt, src_fmt);
	}
	free(src);
	return (float*) r;
}

