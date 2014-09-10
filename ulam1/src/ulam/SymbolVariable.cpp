#include "SymbolVariable.h"
#include "CompilerState.h"

namespace MFM {

  SymbolVariable::SymbolVariable(u32 id, UTI utype, bool packed, CompilerState& state) : Symbol(id, utype, state), m_posOffset(0), m_packed(packed){}

  SymbolVariable::~SymbolVariable()
  {}


  s32 SymbolVariable::getStackFrameSlotIndex()
  {
    assert(0);
    return 0;  //== not on stack
  }


  u32 SymbolVariable::getDataMemberSlotIndex()
  {
    assert(0);
    return 0;  //== not a data member
  }


  //packed bit position of data members; relative to ATOMFIRSTSTATEBITPOS
  u32 SymbolVariable::getPosOffset()
  {
    return m_posOffset;
  }


  void SymbolVariable::setPosOffset(u32 offsetIntoAtom)
  {
    m_posOffset = offsetIntoAtom;
  }


  bool SymbolVariable::isPacked()
  {
    return m_packed;
  }


  void SymbolVariable::setPacked(bool p)
  {
    m_packed = p;
  }


  void SymbolVariable::printPostfixValuesOfVariableDeclarations(File * fp, ULAMCLASSTYPE classtype)
  {
    assert(0);  //only for Class' data members 
  }

} //end MFM
