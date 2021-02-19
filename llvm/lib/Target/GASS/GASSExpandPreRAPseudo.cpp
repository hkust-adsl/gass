#include "GASS.h"
#include "GASSRegisterInfo.h"
#include "GASSInstrInfo.h"
#include "GASSTargetMachine.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"

using namespace llvm;

#define DEBUG_TYPE "gass-expand-pre-ra-pseudo"

namespace {
class GASSExpandPreRAPseudo : public MachineFunctionPass {
  const GASSInstrInfo *TII = nullptr;
public:
  static char ID;

  GASSExpandPreRAPseudo() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  MachineOperand buildExtractSubReg(
    MachineBasicBlock::iterator MI, MachineRegisterInfo &MRI,
    MachineOperand &SuperReg, const TargetRegisterClass *SuperRC, 
    unsigned SubIdx, const TargetRegisterClass *SubRC) const;
};

char GASSExpandPreRAPseudo::ID = 0;

} // namespace

INITIALIZE_PASS(GASSExpandPreRAPseudo, DEBUG_TYPE,
                "Expand pre-RA GASS pseudo instr", false, false)

// helper (Ref: AMDGPU/SIInstrInfo.cpp)
MachineOperand GASSExpandPreRAPseudo::buildExtractSubReg(
    MachineBasicBlock::iterator MI, MachineRegisterInfo &MRI,
    MachineOperand &SuperReg, const TargetRegisterClass *SuperRC, 
    unsigned SubIdx, const TargetRegisterClass *SubRC) const {
  MachineBasicBlock *MBB = MI->getParent();
  DebugLoc DL = MI->getDebugLoc();
  Register SubReg = MRI.createVirtualRegister(SubRC);

  // Just in case the super register is itself a sub-register, copy it to a new
  // value so we don't need to worry about merging its subreg index with the
  // SubIdx passed to this function. The register coalescer should be able to
  // eliminate this extra copy.
  Register NewSuperReg = MRI.createVirtualRegister(SuperRC);

  BuildMI(*MBB, MI, DL, TII->get(TargetOpcode::COPY), NewSuperReg)
    .addReg(SuperReg.getReg(), 0, SuperReg.getSubReg());

  BuildMI(*MBB, MI, DL, TII->get(TargetOpcode::COPY), SubReg)
    .addReg(NewSuperReg, 0, SubIdx);

  return MachineOperand::CreateReg(SubReg, false);
}

bool GASSExpandPreRAPseudo::runOnMachineFunction(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto &ST = MF.getSubtarget<GASSSubtarget>();
  TII = ST.getInstrInfo();
  const GASSRegisterInfo *TRI = ST.getRegisterInfo();

  std::vector<MachineInstr*> ToDeletInstrs;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineBasicBlock::iterator MII = MBB.begin();
                                     MII != MBB.end(); ++MII) {
      MachineInstr &MI = *MII;
      unsigned Opc = MI.getOpcode();
      const DebugLoc &DL = MI.getDebugLoc();
      LLVM_DEBUG(dbgs() << "Start to lower ");
      LLVM_DEBUG(MI.dump());
      switch (Opc) {
      default: break;
      case GASS::IADD64rr:{
        // IADD64rr dst, lhs, rhs; ->
        //   IADDCARRY dst.sub0, c, lhs.sub0, rhs.sub0, !PT;
        //   IADDCARRY dst.sub1, PT, lhs.sub1, rhs.sub1, c;
        MachineOperand &Dst = MI.getOperand(0);
        MachineOperand &LHS = MI.getOperand(1);
        MachineOperand &RHS = MI.getOperand(2);
        
        Register NewDst = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register DstSub0 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register DstSub1 = MRI.createVirtualRegister(&GASS::VReg32RegClass);

        Register CarryReg = MRI.createVirtualRegister(&GASS::VReg1RegClass);

        const DebugLoc &DL = MI.getDebugLoc();

        const TargetRegisterClass *LHSRC = &GASS::VReg64RegClass;
                                           // MRI.getRegClass(LHS.getReg());
        const TargetRegisterClass *RHSRC = &GASS::VReg64RegClass;
        const TargetRegisterClass *LHSSubRC = &GASS::VReg32RegClass;
        const TargetRegisterClass *RHSSubRC = &GASS::VReg32RegClass;
        
        MachineOperand LHSSub0 = buildExtractSubReg(MII, MRI, LHS, LHSRC,
                                                    GASS::sub0, LHSSubRC);
        MachineOperand RHSSub0 = buildExtractSubReg(MII, MRI, RHS, RHSRC,
                                                    GASS::sub0, RHSSubRC);

        MachineOperand LHSSub1 = buildExtractSubReg(MII, MRI, LHS, LHSRC,
                                                    GASS::sub1, LHSSubRC);
        MachineOperand RHSSub1 = buildExtractSubReg(MII, MRI, RHS, RHSRC,
                                                    GASS::sub1, RHSSubRC);

        // IADD.X $dst.sub0, $c, $lhs.sub0, $rhs.sub0, !pt;
        BuildMI(MBB, MI, DL, TII->get(GASS::IADDXrr), DstSub0)
          .addReg(CarryReg, RegState::Define)
          .add(LHSSub0)
          .add(RHSSub0)
          .addReg(GASS::NPT)
          .addReg(GASS::PT); // PredMask
        BuildMI(MBB, MI, DL, TII->get(GASS::IADDXrr), DstSub1)
          .addReg(GASS::PT, RegState::Define)
          .add(LHSSub1)
          .add(RHSSub1)
          .addReg(CarryReg, RegState::Kill)
          .addReg(GASS::PT);

        // Merge result
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), NewDst)
          .addReg(DstSub0)
          .addImm(GASS::sub0)
          .addReg(DstSub1)
          .addImm(GASS::sub1);

        MRI.replaceRegWith(Dst.getReg(), NewDst);

        ToDeletInstrs.push_back(&*MII);
      } break;
      case GASS::SEXT: {
        // sext i64 dst, src0; -> 
        //   COPY dst.sub0, src0;
        //   shf.r.hi dst.sub1, rz, 31, src0;
        MachineOperand &Dst = MI.getOperand(0);
        MachineOperand &Src = MI.getOperand(1);

        // TODO: Can we get subreg type from superreg type?
        Register NewDst = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register DstSub0 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register DstSub1 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), DstSub0)
          .add(Src);
        BuildMI(MBB, MI, DL, TII->get(GASS::SHFrir), DstSub1)
          .addReg(GASS::RZ32)
          .addImm(31)
          .add(Src)
          .addImm(GASS::SHF_FLAGS::R)
          .addImm(GASS::SHF_FLAGS::S32)
          .addImm(GASS::SHF_FLAGS::HI)
          .addReg(GASS::PT); // PredMask
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), NewDst)
          .addReg(DstSub0)
          .addImm(GASS::sub0)
          .addReg(DstSub1)
          .addImm(GASS::sub1);

        MRI.replaceRegWith(Dst.getReg(), NewDst);

        ToDeletInstrs.push_back(&*MII);
      } break;
      }
    }
  }

  // erase deleted MIs
  for (MachineInstr *MI : ToDeletInstrs) {
    MI->eraseFromParent();
  }
}

FunctionPass *llvm::createGASSExpandPreRAPseudoPass() {
  return new GASSExpandPreRAPseudo();
}