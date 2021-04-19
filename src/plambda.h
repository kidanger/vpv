#ifndef PLAMBDA_HEADER
#define PLAMBDA_HEADER

#ifdef __cplusplus
extern "C" {
#endif

float* execute_plambda(int n, float** x, int* w, int* h, int* pd,
    char* program, int* od, char** error);

#ifdef __cplusplus
}
#endif

#endif
