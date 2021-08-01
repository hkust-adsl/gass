#include "llvm/MC/MCInstrDesc.h"

//=------------------------------------------------------------=//
// OperandType
//=------------------------------------------------------------=//
namespace llvm {
namespace GASS {
enum OperandType : unsigned {
  OPERAND_CONSTANT_MEM = MCOI::OPERAND_FIRST_TARGET
};
}
}