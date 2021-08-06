#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsNVPTX.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/raw_ostream.h"

#include <cuda.h>
#include <cuda_runtime.h>

#include <memory>
#include <iostream>
#include <fstream>

using namespace llvm;

#define gep(...)       builder->CreateGEP(__VA_ARGS__)
#define i32(...)       builder->getInt32(__VA_ARGS__)
#define intrinsic(...) builder->CreateIntrinsic(__VA_ARGS__)
#define sub(...)       builder->CreateSub(__VA_ARGS__)
#define store(...)     builder->CreateStore(__VA_ARGS__)
#define ret(...)       builder->CreateRet(__VA_ARGS__)
// types
#define void_ty        builder->getVoidTy()
#define f16_ty         builder->getHalfTy()
#define f32_ty         builder->getFloatTy()
#define i8_ty          builder->getInt8Ty()
#define i32_ty         builder->getInt32Ty()
#define vec_ty(type, num_el)    VectorType::get(type, num_el, false)
#define undef(...)     UndefValue::get(__VA_ARGS__)

void initLLVM() {
  InitializeAllTargetInfos();
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmParsers();
  InitializeAllAsmPrinters();
}

int dev = 0;

void codeGen(LLVMContext *ctx, Module *mod, int len) {
  auto builder = std::make_unique<IRBuilder<ConstantFolder,
                                            IRBuilderDefaultInserter>>(*ctx);

  FunctionCallee Callee = mod->getOrInsertFunction("bench",
                                                   Type::getVoidTy(*ctx),
                                                   PointerType::get(Type::getInt32Ty(*ctx), 1));
  Function *func = cast<Function>(Callee.getCallee());
    Metadata *md_args[] = {
    ValueAsMetadata::get(func),
    MDString::get(*ctx, "kernel"),
    ValueAsMetadata::get(builder->getInt32(1))
  };
  mod->getOrInsertNamedMetadata("nvvm.annotations")->addOperand(MDNode::get(*ctx, md_args));

  Function::arg_iterator args = func->arg_begin();
  Value *duration = args;
  duration->setName("duration");

  // Create basic blocks
  BasicBlock *entry = BasicBlock::Create(*ctx, "entry", func);
  builder->SetInsertPoint(entry);

  Value *start = intrinsic(Intrinsic::nvvm_read_ptx_sreg_clock, {}, {});
  //--- Code to be tested ---
  for (int i=0; i<len; ++i)
    intrinsic(Intrinsic::nvvm_nop, {}, {});
  //--- End ---
  Value *end = intrinsic(Intrinsic::nvvm_read_ptx_sreg_clock, {}, {});
  Value *dur = sub(end, start);
  Value *tid = intrinsic(Intrinsic::nvvm_read_ptx_sreg_tid_x, {}, {});
  store(dur, gep(duration, tid));
  ret(nullptr);
}

int getComputeCapability() {
  int major, minor;
  cudaDeviceGetAttribute(&major, cudaDevAttrComputeCapabilityMajor, dev);
  cudaDeviceGetAttribute(&minor, cudaDevAttrComputeCapabilityMinor, dev);
  return major*10 + minor;
}

std::vector<char> compileGASS(Module *mod) {
  std::string error;
  mod->setTargetTriple("gass");
  auto target = TargetRegistry::lookupTarget(mod->getTargetTriple(), error);
  if (!target) {
    std::cout << error << "\nAbort\n";
    exit(1);
  }

  int cc = getComputeCapability();
  std::string sm = "sm_" + std::to_string(cc);

  TargetOptions opt;
  TargetMachine *machine = target->createTargetMachine(mod->getTargetTriple(), sm, "", opt,
                                                       Reloc::PIC_, None, CodeGenOpt::Aggressive);
  mod->setDataLayout(machine->createDataLayout());

  legacy::PassManager pm;
  SmallVector<char, 0> buffer;
  raw_svector_ostream stream(buffer);
  machine->addPassesToEmitFile(pm, stream, nullptr, CodeGenFileType::CGFT_ObjectFile);
  pm.run(*mod);

  std::vector<char> result(buffer.begin(), buffer.end());
  return result;
}

void loadCUmodule(CUmodule *cuMod, std::vector<char> const &bin) {
    unsigned int errbufsize = 8192;
    unsigned int logbufsize = 8192;
    char _err[errbufsize];
    char _log[logbufsize];
    CUjit_option JitOpt[] = {CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES, CU_JIT_ERROR_LOG_BUFFER,
                              CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES, CU_JIT_INFO_LOG_BUFFER,
                              CU_JIT_LOG_VERBOSE};
    void* optval[] = {(void*)(uintptr_t)errbufsize, (void*)_err, 
                      (void*)(uintptr_t)logbufsize, (void*)_log, (void*)1};
    {
      std::ofstream cubin_out("/tmp/gass-bench-kernel.cubin", std::ios::out | std::ios::binary);
      cubin_out.write((char*)&bin[0], bin.size()*sizeof(char));
      cubin_out.close();
    }
    // This doesn't work (leads to segfault)
    cuModuleLoadDataEx(cuMod, bin.data(), 5, JitOpt, optval);
}

int executeModule(CUmodule *cuMod) {
  // allocate data
  int *resultDevice = nullptr;
  int *result = (int*)malloc(sizeof(int)*32);
  cudaMalloc((void**)&resultDevice, sizeof(int)*32);
  void *args[] = {&resultDevice};
  CUmodule module;
  CUfunction kernel;
  cuModuleLoad(&module, "/tmp/gass-bench-kernel.cubin");
  cuModuleGetFunction(&kernel, module, "bench");
  cuLaunchKernel(kernel, 1, 1, 1, 32, 1, 1, 0, 0, args, 0);
  cudaMemcpy(result, resultDevice, sizeof(int)*32, cudaMemcpyDeviceToHost);
  int res = result[0];
  cudaFree(resultDevice);
  free(result);
  return res;
}

int main() {
  initLLVM();

  std::map<int, int> res;

  cudaGetDevice(&dev);
  for (int i=0; i<8192; ++i) {
    auto ctx = std::make_unique<LLVMContext>();
    auto mod = std::make_unique<Module>("tput_bench", *ctx);
    codeGen(ctx.get(), mod.get(), i);
    std::vector<char> bin = compileGASS(mod.get());
    CUmodule cuMod;
    loadCUmodule(&cuMod, bin);
    int dur = executeModule(&cuMod);
    res[i] = dur;
    std::cerr << i << "\n";
  }

  for (auto iter : res) {
    std::cout << iter.first << "," << iter.second << "\n";
  }
}