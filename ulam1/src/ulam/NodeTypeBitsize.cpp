#include <stdlib.h>
#include "NodeTypeBitsize.h"
#include "CompilerState.h"


namespace MFM {

  NodeTypeBitsize::NodeTypeBitsize(Node * node, CompilerState & state) : Node(state), m_node(node) {}

  NodeTypeBitsize::~NodeTypeBitsize()
  {
    delete m_node;
    m_node = NULL;
  }

  void NodeTypeBitsize::printPostfix(File * fp)
  { 
    m_node->printPostfix(fp); 
  }


  const char * NodeTypeBitsize::getName()
  {
    return "(bitsize)";
  }


  const std::string NodeTypeBitsize::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }


  UTI NodeTypeBitsize::checkAndLabelType()
  {
    UTI it = Nav;
    it = m_node->checkAndLabelType();
    assert(it == m_state.getUlamTypeOfConstant(Int));
    setNodeType(it);
    return getNodeType();
  }


  EvalStatus NodeTypeBitsize::eval()
  {
    assert(0);  //not in parse tree; part of symbol's type
    return NORMAL;
  }


  // called during parsing Type of variable, typedef, func return val;
  // Requires a constant expression for the bitsize, else error;
  // guards against even Bool's.
  bool NodeTypeBitsize::getTypeBitSizeInParen(u32& rtnBitSize, ULAMTYPE BUT)
  {
    u32 newbitsize = ANYBITSIZECONSTANT;
    UTI sizetype = checkAndLabelType();
    if(sizetype == m_state.getUlamTypeOfConstant(Int))
      {
	evalNodeProlog(0); //new current frame pointer
	makeRoomForNodeType(getNodeType()); //offset a constant expression
	m_node->eval();  
	UlamValue bitUV = m_state.m_nodeEvalStack.popArg();
	evalNodeEpilog();     
	
	newbitsize = bitUV.getImmediateData(m_state);
	//if(newbitsize == 0)
	//  {
	//    MSG(getNodeLocationAsString().c_str(), "Type Bitsize specifier in () is not a constant expression", ERR);
	//    return false;
	//  }
	
	// warn against even Bool bits, and reduce by 1.
	if(BUT == Bool && ((newbitsize % 2) == 0) )
	  {
	    newbitsize--;
	    std::ostringstream msg;
	    msg << "Bool Type with EVEN number of bits is internally inconsistent; Reduced by one to " << newbitsize << " bits" ;
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), WARN);
	  }
      }
    else
      {
	MSG(getNodeLocationAsString().c_str(), "Type Bitsize specifier in () is not a constant expression", ERR);
	return false;
      }

    rtnBitSize = newbitsize;
    return true;
  }

} //end MFM

