#include "NodeBinaryOpMinusEqual.h"
#include "CompilerState.h"

namespace MFM {

  NodeBinaryOpMinusEqual::NodeBinaryOpMinusEqual(Node * left, Node * right, CompilerState & state) : NodeBinaryOpEqualArith(left,right,state) {}

  NodeBinaryOpMinusEqual::~NodeBinaryOpMinusEqual(){}


  const char * NodeBinaryOpMinusEqual::getName()
  {
    return "-=";
  }


  const std::string NodeBinaryOpMinusEqual::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }


  //same as NodeBinaryOpSubtract
  UlamValue NodeBinaryOpMinusEqual::makeImmediateBinaryOp(UTI type, u32 ldata, u32 rdata, u32 len)
  {
    //return UlamValue::makeImmediate(type, (s32) ldata - (s32) rdata, len);
    UlamValue rtnUV;
    ULAMTYPE typEnum = m_state.getUlamTypeByIndex(type)->getUlamTypeEnum();
    switch(typEnum)
      {
      case Unary:
	{
	  //convert to binary before the operation; then convert back to unary
	  u32 leftCount1s = PopCount(ldata);
	  u32 rightCount1s = PopCount(rdata);
	  s32 diffof1s = leftCount1s - rightCount1s;
	  if(diffof1s > 0)
	    rtnUV = UlamValue::makeImmediate(type, _GetNOnes32(diffof1s), len);
	  else
	    rtnUV = UlamValue::makeImmediate(type, 0, len); //least surprising
	}
	break;
      default:
	rtnUV = UlamValue::makeImmediate(type, (s32) ldata - (s32) rdata, len);
	break;
      };
    return rtnUV;
  }


  //same as NodeBinaryOpSubtract
  void NodeBinaryOpMinusEqual::appendBinaryOp(UlamValue& refUV, u32 ldata, u32 rdata, u32 pos, u32 len)
  {
    //refUV.putData(pos, len, (s32) ldata - (s32) rdata);
    UTI type = refUV.getUlamValueTypeIdx();
    ULAMTYPE typEnum = m_state.getUlamTypeByIndex(type)->getUlamTypeEnum();
    switch(typEnum)
      {
      case Unary:
	{
	  //convert to binary before the operation; then convert back to unary
	  u32 leftCount1s = PopCount(ldata);
	  u32 rightCount1s = PopCount(rdata);
	  s32 diffOf1s = leftCount1s - rightCount1s;
	  if(diffOf1s > 0)
	    refUV.putData(pos, len, _GetNOnes32(diffOf1s));
	  else
	    refUV.putData(pos, len, 0);
	}
	break;
      default:
	refUV.putData(pos, len, (s32) ldata - (s32) rdata);
	break;
      };
    return;
  }

} //end MFM
