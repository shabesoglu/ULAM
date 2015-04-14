#include <stdlib.h>
#include "NodeTypeSelect.h"
#include "CompilerState.h"


namespace MFM {

  NodeTypeSelect::NodeTypeSelect(Token typetoken, UTI auti, NodeType * node, CompilerState & state) : NodeType(typetoken, auti, state), m_nodeSelect(node)
  {
    if(m_nodeSelect)
      m_nodeSelect->updateLineage(getNodeNo()); //for unknown subtrees
  }

  NodeTypeSelect::NodeTypeSelect(const NodeTypeSelect& ref) : NodeType(ref), m_nodeSelect(NULL)
  {
    if(ref.m_nodeSelect)
      m_nodeSelect = (NodeType *) ref.m_nodeSelect->instantiate();
  }

  NodeTypeSelect::~NodeTypeSelect()
  {
    delete m_nodeSelect;
    m_nodeSelect = NULL;
  } //destructor

  Node * NodeTypeSelect::instantiate()
  {
    return new NodeTypeSelect(*this);
  }

  void NodeTypeSelect::updateLineage(NNO pno)
  {
    setYourParentNo(pno);
    if(m_nodeSelect)
      m_nodeSelect->updateLineage(getNodeNo());
  }//updateLineage

  bool NodeTypeSelect::findNodeNo(NNO n, Node *& foundNode)
  {
    if(Node::findNodeNo(n, foundNode))
      return true;
    if(m_nodeSelect && m_nodeSelect->findNodeNo(n, foundNode))
      return true;
    return false;
  } //findNodeNo

  void NodeTypeSelect::printPostfix(File * fp)
  {
    fp->write(getName());
  }

  const char * NodeTypeSelect::getName()
  {
    std::ostringstream nstr;
    if(m_nodeSelect)
      {
	nstr << m_nodeSelect->getName();
	nstr << ".";
      }
    nstr << m_state.getTokenDataAsString(&m_typeTok);
    return nstr.str().c_str();
  } //getName

  const std::string NodeTypeSelect::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }

  UTI NodeTypeSelect::checkAndLabelType()
  {
    if(isReadyType())
      return getNodeType();

    UTI it = Nav;
    //    it = m_nodeSelect->checkAndLabelType();
    if(resolveType(it))
      {
	m_ready = true; // set here
      }

    setNodeType(it);
    return getNodeType();
  } //checkAndLabelType


  bool NodeTypeSelect::resolveType(UTI& rtnuti)
  {
    bool rtnb = false;
    if(isReadyType())
      {
	rtnuti = getNodeType();
	return true;
      }

    // we are in a "chain" of type selects..
    assert(m_nodeSelect);

    UTI seluti;
    if(m_nodeSelect->resolveType(seluti))
      {
	UlamType * selut = m_state.getUlamTypeByIndex(seluti);
	ULAMTYPE seletype = selut->getUlamTypeEnum();
	if(seletype == Class)
	  {
	    SymbolClass * csym = NULL;
	    assert(m_state.alreadyDefinedSymbolClass(seluti, csym));

	    m_state.pushClassContext(seluti, csym->getClassBlockNode(), csym->getClassBlockNode(), false, NULL);
	    // find our id in the "selected" class, must be a typedef at top level
	    Symbol * asymptr = NULL;
	    if(m_state.alreadyDefinedSymbol(m_typeTok.m_dataindex, asymptr))
	      {
		if(asymptr->isTypedef())
		  {
		    rtnuti = asymptr->getUlamTypeIdx(); //should be mapped if necessary
			rtnb = true;
		  }
		else
		  {
		    //error id is not a typedef
		  }
	      }
	    else
	      {
		//error! id not found

	      }

	    m_state.popClassContext();
	  }
	else
	  {
	    // not a class, possible scalar with "known" bitsize for our unknown arraysize
	    UTI nuti = getNodeType();
	    UTI mappedUTI = nuti;
	    UTI cuti = m_state.getCompileThisIdx();
	    if(m_state.mappedIncompleteUTI(cuti, nuti, mappedUTI))
	      nuti = mappedUTI;

	    rtnb = resolveTypeArraysize(nuti);
	    rtnuti = nuti;
	  }
      } //else select not ready, so neither are we!!
    return rtnb;
  } //resolveType

  void NodeTypeSelect::countNavNodes(u32& cnt)
  {
    Node::countNavNodes(cnt);
    m_nodeSelect->countNavNodes(cnt);
  }

} //end MFM
