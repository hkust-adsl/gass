#include "GASS.h"
#include "GASSRegisterInfo.h"
#include "GASSInstrInfo.h"
#include "GASSTargetMachine.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/ErrorHandling.h"

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

  bool Modified = false;

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
          .addImm(1).addReg(GASS::PT) // !PT
          .addImm(0).addReg(GASS::PT); // PredMask
        BuildMI(MBB, MI, DL, TII->get(GASS::IADDXrr), DstSub1)
          .addReg(GASS::PT, RegState::Define)
          .add(LHSSub1)
          .add(RHSSub1)
          .addImm(0).addReg(CarryReg, RegState::Kill)
          .addImm(0).addReg(GASS::PT);

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
          .addImm(0).addReg(GASS::PT); // PredMask
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), NewDst)
          .addReg(DstSub0)
          .addImm(GASS::sub0)
          .addReg(DstSub1)
          .addImm(GASS::sub1);

        MRI.replaceRegWith(Dst.getReg(), NewDst);

        ToDeletInstrs.push_back(&*MII);
      } break;
      case GASS::ZEXT: {
        llvm_unreachable("Not implemented");
      } break;
      case GASS::HMMA884_f32_f32_Pseudo: {
        // mma.sync.aligned.m8n8k4.f32.f16.f16.f32 
        //    $d:8xf32, $a:2xv2f16, $b:2xv2f16, $c:8xf32
        //  ==>
        // HMMA.884.F32.F32.STEP0 $d[0:1]:v2f32, $a:v4f16, $b:v4f16, $c[0:1]:v2f32
        // HMMA.884.F32.F32.STEP1 $d[2:3]:v2f32, $a:v4f16, $b:v4f16, $c[2:3]:v2f32
        // HMMA.884.F32.F32.STEP2 $d[4:5]:v2f32, $a:v4f16, $b:v4f16, $c[4:5]:v2f32
        // HMMA.884.F32.F32.STEP3 $d[6:7]:v2f32, $a:v4f16, $b:v4f16, $c[6:7]:v2f32

        // 1. Prepare input data
        Register D0 = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register D1 = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register D2 = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register D3 = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register A = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register B = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register C0 = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register C1 = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register C2 = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register C3 = MRI.createVirtualRegister(&GASS::VReg64RegClass);

        Register AReg0 = MI.getOperand(8).getReg();
        Register AReg1 = MI.getOperand(9).getReg();
        Register BReg0 = MI.getOperand(10).getReg();
        Register BReg1 = MI.getOperand(11).getReg();
        Register CReg0 = MI.getOperand(12).getReg();
        Register CReg1 = MI.getOperand(13).getReg();
        Register CReg2 = MI.getOperand(14).getReg();
        Register CReg3 = MI.getOperand(15).getReg();
        Register CReg4 = MI.getOperand(16).getReg();
        Register CReg5 = MI.getOperand(17).getReg();
        Register CReg6 = MI.getOperand(18).getReg();
        Register CReg7 = MI.getOperand(19).getReg();

        assert(MI.getOperand(20).isImm() && MI.getOperand(21).isImm());
        unsigned ALayout = MI.getOperand(20).getImm();
        unsigned BLayout = MI.getOperand(21).getImm();

        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), A)
          .addReg(AReg0)
          .addImm(GASS::sub0)
          .addReg(AReg1)
          .addImm(GASS::sub1); // Do not need PredMask;
        
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), B)
          .addReg(BReg0)
          .addImm(GASS::sub0)
          .addReg(BReg1)
          .addImm(GASS::sub1);

        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), C0)
          .addReg(CReg0)
          .addImm(GASS::sub0)
          .addReg(CReg1)
          .addImm(GASS::sub1);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), C1)
          .addReg(CReg2)
          .addImm(GASS::sub0)
          .addReg(CReg3)
          .addImm(GASS::sub1);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), C2)
          .addReg(CReg4)
          .addImm(GASS::sub0)
          .addReg(CReg5)
          .addImm(GASS::sub1);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), C3)
          .addReg(CReg6)
          .addImm(GASS::sub0)
          .addReg(CReg7)
          .addImm(GASS::sub1);

        // 2. MMA
        BuildMI(MBB, MI, DL, TII->get(GASS::HMMA884_f32_f32), D0)
          .addReg(A)
          .addReg(B)
          .addReg(C0)
          .addImm(ALayout)
          .addImm(BLayout)
          .addImm(GASS::TensorCore::STEP0)
          .addImm(0).addReg(GASS::PT); // PredMask
        BuildMI(MBB, MI, DL, TII->get(GASS::HMMA884_f32_f32), D1)
          .addReg(A)
          .addReg(B)
          .addReg(C1)
          .addImm(ALayout)
          .addImm(BLayout)
          .addImm(GASS::TensorCore::STEP1)
          .addImm(0).addReg(GASS::PT); // PredMask
        BuildMI(MBB, MI, DL, TII->get(GASS::HMMA884_f32_f32), D2)
          .addReg(A)
          .addReg(B)
          .addReg(C2)
          .addImm(ALayout)
          .addImm(BLayout)
          .addImm(GASS::TensorCore::STEP2)
          .addImm(0).addReg(GASS::PT); // PredMask
        BuildMI(MBB, MI, DL, TII->get(GASS::HMMA884_f32_f32), D3)
          .addReg(A)
          .addReg(B)
          .addReg(C3)
          .addImm(ALayout)
          .addImm(BLayout)
          .addImm(GASS::TensorCore::STEP3)
          .addImm(0).addReg(GASS::PT); // PredMask

        // 3. extract data
        Register DReg0 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register DReg1 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register DReg2 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register DReg3 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register DReg4 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register DReg5 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register DReg6 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register DReg7 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        // Note: TargetOpcode::EXTRACT_SUBREG won't work here.
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), DReg0)
          .addReg(D0, 0, GASS::sub0);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), DReg1)
          .addReg(D0, 0, GASS::sub1);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), DReg2)
          .addReg(D1, 0, GASS::sub0);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), DReg3)
          .addReg(D1, 0, GASS::sub1);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), DReg4)
          .addReg(D2, 0, GASS::sub0);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), DReg5)
          .addReg(D2, 0, GASS::sub1);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), DReg6)
          .addReg(D3, 0, GASS::sub0);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), DReg7)
          .addReg(D3, 0, GASS::sub1);

        MRI.replaceRegWith(MI.getOperand(0).getReg(), DReg0);
        MRI.replaceRegWith(MI.getOperand(1).getReg(), DReg1);
        MRI.replaceRegWith(MI.getOperand(2).getReg(), DReg2);
        MRI.replaceRegWith(MI.getOperand(3).getReg(), DReg3);
        MRI.replaceRegWith(MI.getOperand(4).getReg(), DReg4);
        MRI.replaceRegWith(MI.getOperand(5).getReg(), DReg5);
        MRI.replaceRegWith(MI.getOperand(6).getReg(), DReg6);
        MRI.replaceRegWith(MI.getOperand(7).getReg(), DReg7);

        ToDeletInstrs.push_back(&*MII);
      } break;
      case GASS::MOV64i: {
        // MOV64i $dst, $src;
        //  ->
        // MOV32i $dst.sub0, $src[63-32];
        // MOV32i $dst.sub1, $src[31-0];
        Register Dst = MI.getOperand(0).getReg();
        const MachineOperand &Constant = MI.getOperand(1);
        assert(Constant.isImm());

        uint64_t ConstantValue = Constant.getImm();
        uint32_t ConstantHi = uint32_t(ConstantValue >> 32);
        uint32_t ConstantLo = uint32_t(ConstantValue & 0xffffffff);

        Register NewDst = MRI.createVirtualRegister(&GASS::VReg64RegClass);
        Register NewDst0 = MRI.createVirtualRegister(&GASS::VReg32RegClass);
        Register NewDst1 = MRI.createVirtualRegister(&GASS::VReg32RegClass);

        BuildMI(MBB, MI, DL, TII->get(GASS::MOV32i), NewDst0)
          .addImm(ConstantLo)
          .addImm(0).addReg(GASS::PT);
        BuildMI(MBB, MI, DL, TII->get(GASS::MOV32i), NewDst1)
          .addImm(ConstantHi)
          .addImm(0).addReg(GASS::PT);
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::REG_SEQUENCE), NewDst)
          .addReg(NewDst0).addImm(GASS::sub0)
          .addReg(NewDst1).addImm(GASS::sub1);

        MRI.replaceRegWith(Dst, NewDst);

        ToDeletInstrs.push_back(&*MII);
      } break;
      }
    }
  }

  // erase deleted MIs
  for (MachineInstr *MI : ToDeletInstrs) {
    Modified = true;
    MI->eraseFromParent();
  }

  return Modified;
}

FunctionPass *llvm::createGASSExpandPreRAPseudoPass() {
  return new GASSExpandPreRAPseudo();
}