//
// Created by Da Yan on 14/2/2021.
//
#include "GASS.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFunction.h"

using namespace llvm;

#define DEBUG_TYPE "gass-mf-cfg-printer"
#define PASS_NAME "GASS MachineFunction CFG Printer"

namespace {
class GASSMachineFunctionCFGPrinter : public MachineFunctionPass {
public:
  static char ID;

  GASSMachineFunctionCFGPrinter() : MachineFunctionPass(ID){
    initializeGASSMachineFunctionCFGPrinterPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return PASS_NAME; }
};

char GASSMachineFunctionCFGPrinter::ID = 0;
} // anonymous namespace

INITIALIZE_PASS(GASSMachineFunctionCFGPrinter, DEBUG_TYPE, PASS_NAME,
                false, true)


bool GASSMachineFunctionCFGPrinter::runOnMachineFunction(MachineFunction &MF) {
  std::string Filename = WriteGraph(&MF, MF.getName(), false, "");

  return false;
}

//==------------------------------------------------------------------------==//
// public interface
//==------------------------------------------------------------------------==//
FunctionPass *llvm::createGASSMachineFunctionCFGPrinterPass() {
  return new GASSMachineFunctionCFGPrinter();
}