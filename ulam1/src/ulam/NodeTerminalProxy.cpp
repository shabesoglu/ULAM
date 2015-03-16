#include <stdlib.h>
#include "NodeTerminalProxy.h"
#include "CompilerState.h"

namespace MFM {

  NodeTerminalProxy::NodeTerminalProxy(Token memberTok, UTI memberType, Token funcTok, CompilerState & state) : NodeTerminal(UNKNOWNSIZE, state), m_ofTok(memberTok), m_uti(memberType), m_funcTok(funcTok), m_ready(false)
  {
    Node::setNodeLocation(funcTok.m_locator);
  }

  NodeTerminalProxy::NodeTerminalProxy(const NodeTerminalProxy& ref) : NodeTerminal(ref), m_ofTok(ref.m_ofTok), m_uti(m_state.mapIncompleteUTIForCurrentClassInstance(ref.m_uti)), m_funcTok(ref.m_funcTok), m_ready(ref.m_ready) {}

  NodeTerminalProxy::~NodeTerminalProxy() {}

  Node * NodeTerminalProxy::instantiate()
  {
    return new NodeTerminalProxy(*this);
  }

  void NodeTerminalProxy::printPostfix(File * fp)
  {
    fp->write(" ");
    if(m_ready)
      fp->write(NodeTerminal::getName());
    else
      {
	fp->write(m_state.getTokenDataAsString(&m_ofTok).c_str());
	fp->write(" ");
	fp->write(m_state.getTokenDataAsString(&m_funcTok).c_str());
	fp->write(" .");
      }
  } //printPostfix

  const std::string NodeTerminalProxy::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }

  UTI NodeTerminalProxy::checkAndLabelType()
  {
    Symbol * asymptr = NULL;
    if(m_uti == Nav)
      {
	if(m_state.alreadyDefinedSymbol(m_ofTok.m_dataindex,asymptr))
	  {
	    m_uti = asymptr->getUlamTypeIdx();
	    std::ostringstream msg;
	    msg << "Determined incomplete type for member '";
	    msg << m_state.getTokenDataAsString(&m_ofTok).c_str();
	    msg << "'s Proxy, as type: ";
	    msg << m_state.getUlamTypeNameByIndex(m_uti).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	  }
      }

    if(!updateProxy())
      setNodeType(Nav);  //invalid func
    else
      setConstantTypeForNode(m_funcTok); //enough info to set this constant node's type

    return getNodeType(); //updated to Unsigned, hopefully
  }  //checkandLabelType

  EvalStatus NodeTerminalProxy::eval()
  {
    EvalStatus evs = NORMAL; //init ok
    evalNodeProlog(0); //new current frame pointer

    updateProxy();

    if(!m_state.isComplete(m_uti))
      evs = ERROR;
    else
      {
	UlamValue rtnUV;
	evs = NodeTerminal::makeTerminalValue(rtnUV);

	//copy result UV to stack, -1 relative to current frame pointer
	if(evs == NORMAL)
	  Node::assignReturnValueToStack(rtnUV);
      }
    evalNodeEpilog();
    return evs;
  } //eval

  void NodeTerminalProxy::genCode(File * fp, UlamValue& uvpass)
  {
    updateProxy();

    return NodeTerminal::genCode(fp, uvpass);
  }

  void NodeTerminalProxy::genCodeToStoreInto(File * fp, UlamValue& uvpass)
  {
    updateProxy();

    return NodeTerminal::genCodeToStoreInto(fp, uvpass);
  }

  bool NodeTerminalProxy::setConstantValue(Token tok)
  {
    bool rtnB = false;
    UlamType * cut = m_state.getUlamTypeByIndex(m_uti);
    assert(cut->isComplete());

    switch(tok.m_type)
      {
      case TOK_KW_SIZEOF:
	{
	  m_constant.uval =  cut->getTotalBitSize(); //unsigned
	  rtnB = true;
	}
	break;
      case TOK_KW_MAXOF:
	{
	  if(cut->isMinMaxAllowed())
	    {
	      m_constant.uval = cut->getMax();
	      rtnB = true;
	    }
	}
	break;
      case TOK_KW_MINOF:
	{
	  if(cut->isMinMaxAllowed())
	    {
	      m_constant.sval = cut->getMin();
	      rtnB = true;
	    }
	}
	break;
      default:
	assert(0);
      };
    return rtnB;
  } //setConstantValue

  //minof, maxof should be the same type as the original member token, unsigned for sizeof
  UTI NodeTerminalProxy::setConstantTypeForNode(Token tok)
  {
    UTI newType = Nav;  //init
    switch(tok.m_type)
      {
      case TOK_KW_SIZEOF:
	newType = m_state.getUlamTypeOfConstant(Unsigned);
	break;
      case TOK_KW_MAXOF:
      case TOK_KW_MINOF:
      {
	// not a class! since sizeof is only func that applies to classes
	ULAMTYPE etype = m_state.getUlamTypeByIndex(m_uti)->getUlamTypeEnum();
	newType = m_state.getUlamTypeOfConstant(etype);
	break;
      }
      default:
	assert(0);
      };

    setNodeType(newType);
    setStoreIntoAble(false);
    return newType;
  } //setConstantTypeForNode

  bool NodeTerminalProxy::updateProxy()
  {
    if(m_uti == Nav)
      return false;

    if(m_ready)
      return true;

    bool rtnb = true;
    m_state.constantFoldIncompleteUTI(m_uti); //update if possible

    //attempt to map UTI
    if(!m_state.isComplete(m_uti))
      {
	UTI cuti = m_state.getCompileThisIdx();
	UTI mappedUTI = Nav;
	if(m_state.mappedIncompleteUTI(cuti, m_uti, mappedUTI))
	  {
	    std::ostringstream msg;
	    msg << "Substituting Mapped UTI" << mappedUTI;
	    msg << ", " << m_state.getUlamTypeNameByIndex(mappedUTI).c_str();
	    msg << " for incomplete Proxy type: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(m_uti).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	    m_uti = mappedUTI;
	  }
      }

    if(!m_state.isComplete(m_uti))
      {
	std::ostringstream msg;
	msg << "Proxy Type: " << m_state.getUlamTypeNameByIndex(m_uti).c_str();
	msg << " is still incomplete and unknown for its '";
	msg << m_state.getTokenDataAsString(&m_funcTok).c_str();
	msg << "' while compiling class: ";
	msg << m_state.getUlamTypeNameBriefByIndex(m_state.getCompileThisIdx()).c_str();
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), WARN);
	//rtnb = false; don't want to stop after parsing.
      }
    else
      {
	//depending on the of-func, update our constant
	if(!setConstantValue(m_funcTok))
	  {
	    std::ostringstream msg;
	    msg << "Proxy Type: " << m_state.getUlamTypeNameByIndex(m_uti).c_str();
	    msg << " constant value for its <";
	    msg << m_state.getTokenDataAsString(&m_funcTok).c_str();
	    msg << "> is still incomplete and unknown while compiling class: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(m_state.getCompileThisIdx()).c_str();
	    MSG(&m_funcTok, msg.str().c_str(), ERR);
	    rtnb = false;
	  }
	else
	  {
	    std::ostringstream msg;
	    msg << "Yippee! Proxy Type: " << m_state.getUlamTypeNameByIndex(m_uti).c_str();
	    msg << " (UTI" << getNodeType() << ") is KNOWN (=" << m_constant.uval;
	    msg << ") while compiling class: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(m_state.getCompileThisIdx()).c_str();
	    MSG(&m_funcTok, msg.str().c_str(), DEBUG);
	    m_ready = true;
	  }
      }
    return rtnb;
  } //updateProxy

} //end MFM
