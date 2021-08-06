; ModuleID = 'vecadd.cu'
source_filename = "vecadd.cu"
target datalayout = "e-i64:64-i128:128-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

%struct.__cuda_builtin_threadIdx_t = type { i8 }
%struct.__cuda_builtin_blockIdx_t = type { i8 }
%struct.__cuda_builtin_blockDim_t = type { i8 }

@threadIdx = extern_weak dso_local addrspace(1) global %struct.__cuda_builtin_threadIdx_t, align 1
@blockIdx = extern_weak dso_local addrspace(1) global %struct.__cuda_builtin_blockIdx_t, align 1
@blockDim = extern_weak dso_local addrspace(1) global %struct.__cuda_builtin_blockDim_t, align 1

; Function Attrs: convergent noinline norecurse nounwind optnone
define dso_local void @_Z6vecaddPfS_S_i(float* %0, float* %1, float* %2, i32 %3) #0 {
  %5 = alloca float*, align 8
  %6 = alloca float*, align 8
  %7 = alloca float*, align 8
  %8 = alloca i32, align 4
  %9 = alloca i32, align 4
  store float* %0, float** %5, align 8
  store float* %1, float** %6, align 8
  store float* %2, float** %7, align 8
  store i32 %3, i32* %8, align 4
  %10 = call i32 @llvm.nvvm.read.ptx.sreg.tid.x() #2, !range !4
  %11 = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x() #2, !range !5
  %12 = call i32 @llvm.nvvm.read.ptx.sreg.ntid.x() #2, !range !6
  %13 = mul i32 %11, %12
  %14 = add i32 %10, %13
  store i32 %14, i32* %9, align 4
  %15 = load i32, i32* %9, align 4
  %16 = load i32, i32* %8, align 4
  %17 = icmp slt i32 %15, %16
  br i1 %17, label %18, label %34

18:                                               ; preds = %4
  %19 = load float*, float** %6, align 8
  %20 = load i32, i32* %9, align 4
  %21 = sext i32 %20 to i64
  %22 = getelementptr inbounds float, float* %19, i64 %21
  %23 = load float, float* %22, align 4
  %24 = load float*, float** %7, align 8
  %25 = load i32, i32* %9, align 4
  %26 = sext i32 %25 to i64
  %27 = getelementptr inbounds float, float* %24, i64 %26
  %28 = load float, float* %27, align 4
  %29 = fmul contract float %23, %28
  %30 = load float*, float** %5, align 8
  %31 = load i32, i32* %9, align 4
  %32 = sext i32 %31 to i64
  %33 = getelementptr inbounds float, float* %30, i64 %32
  store float %29, float* %33, align 4
  br label %34

34:                                               ; preds = %18, %4
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @llvm.nvvm.read.ptx.sreg.tid.x() #1

; Function Attrs: nounwind readnone
declare i32 @llvm.nvvm.read.ptx.sreg.ctaid.x() #1

; Function Attrs: nounwind readnone
declare i32 @llvm.nvvm.read.ptx.sreg.ntid.x() #1

attributes #0 = { convergent noinline norecurse nounwind optnone "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="sm_20" "target-features"="+ptx32,+sm_20" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind }

!nvvm.annotations = !{!0}
!llvm.module.flags = !{!1, !2}
!llvm.ident = !{!3}

!0 = !{void (float*, float*, float*, i32)* @_Z6vecaddPfS_S_i, !"kernel", i32 1}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 4, !"nvvm-reflect-ftz", i32 0}
!3 = !{!"Ubuntu clang version 11.1.0-++20210428103915+1fdec59bffc1-1~exp1~20210428204556.164"}
!4 = !{i32 0, i32 1024}
!5 = !{i32 0, i32 65535}
!6 = !{i32 1, i32 1025}
