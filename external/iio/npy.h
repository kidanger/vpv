#pragma once

#include <stdlib.h>

struct npy_info {
    size_t header_offset;
    size_t dims[4];
    int ndims;
    int type;
    int fortran_order;
    char desc[10];
};

int npy_read_header(FILE *fin, struct npy_info* ni);
size_t npy_type_size(int type);
float* npy_convert_to_float(void* src, int n, int src_fmt);

