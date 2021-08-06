__global__ void vecadd(float *C, float *A, float *B, int N) {
  int tid = threadIdx.x + blockIdx.x * blockDim.x;

  if (tid < N)
    C[tid] = A[tid] * B[tid];
}