#include "GASS.h"
#include "GASSMCInstLowering.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Constants.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/MC/MCInst.h"

using namespace llvm;

#define DEBUG_TYPE "gass-mc-lowering"

void GASSMCInstLower::lowerToMCFlags(const MachineInstr &MI, MCInst &MCI) {
  // 1. decode
  // uint16_t flags; // 3+3+6+4 
  // Wait Mask (6 bits) :: Read Barrier Idx (3 bits) :: 
  // Write Barrier Idx (3 bits) :: Stall Cycles (4 bits)
  uint16_t MIFlags = MI.getFlags();
  uint32_t WaitMask    = (MIFlags & 0b1111'1100'0000'0000) >> 10;
  uint32_t ReadBarIdx  = (MIFlags & 0b0000'0011'1000'0000) >> 7;
  uint32_t WriteBarIdx = (MIFlags & 0b0000'0000'0111'0000) >> 4;
  uint32_t Stalls      = (MIFlags & 0b0000'0000'0000'1111);

  // 2. encode
  // 00 (2 bits) :: 0000 (reuse, 4 bits) :: Wait Mask (6 bits) ::
  // Read Barrier Idx (3 bits) :: Write Barrier Idx (3 bits) ::
  // 1 (yield 1 bit) :: stalls (4 bits) :: 0...0 (padding 9 bits)
  uint32_t MCFlags = 0x10; // Yield
  MCFlags |= Stalls;
  MCFlags |= WriteBarIdx << 5;
  MCFlags |= ReadBarIdx << 8;
  MCFlags |= WaitMask << 11;
  MCFlags = MCFlags << 9;
  MCI.setFlags(MCFlags);
}

void GASSMCInstLower::lowerToMCOperand(const MachineOperand &MO, 
                                       MCOperand &MCOp) {
  switch (MO.getType()) {
  default:
    llvm_unreachable("unknown operand type");
  case MachineOperand::MO_Register:
    MCOp = MCOperand::createReg(MO.getReg());
    break;
  case MachineOperand::MO_Immediate:
    MCOp = MCOperand::createImm(MO.getImm());
    break;
  case MachineOperand::MO_FPImmediate: {
    APFloat Val = MO.getFPImm()->getValueAPF();
    bool ignored;
    Val.convert(APFloat::IEEEdouble(), APFloat::rmTowardZero, &ignored);
    MCOp = MCOperand::createFPImm(Val.convertToDouble());
    break;
  }
  case MachineOperand::MO_MachineBasicBlock:
    // Encode BrOffset here
    // TODO: how about JMP?
    // relative offset (18-bit signed)
    // TODO: should query TII
    uint64_t SrcOffset = MIOffsets->lookup(MO.getParent()) + 16;
    uint64_t DstOffset = MBBOffsets->lookup(MO.getMBB());
    uint64_t BrOffset = DstOffset - SrcOffset;
    MCOp = MCOperand::createImm(BrOffset);
    break;
  }
}

void GASSMCInstLower::LowerToMCInst(const MachineInstr *MI, MCInst &Inst,
                     DenseMap<const MachineBasicBlock*, uint64_t> MBBOffsets,
                     DenseMap<const MachineInstr *, uint64_t> MIOffsets) {
  this->MIOffsets = &MIOffsets;
  this->MBBOffsets = &MBBOffsets;

  Inst.setOpcode(MI->getOpcode());

  for (unsigned i=0; i != MI->getNumOperands(); ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    MCOperand MCOp;
    lowerToMCOperand(MO, MCOp);
    Inst.addOperand(MCOp);
  }

  // Store control info in flags :) amazing
  // Inst.setFlags(0x000fea00);
  lowerToMCFlags(*MI, Inst);
}