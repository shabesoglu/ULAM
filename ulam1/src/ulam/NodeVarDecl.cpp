#include <stdlib.h>
#include "NodeVarDecl.h"
#include "Token.h"
#include "CompilerState.h"
#include "SymbolVariableDataMember.h"
#include "SymbolVariableStack.h"
#include "NodeIdent.h"

namespace MFM {

  NodeVarDecl::NodeVarDecl(SymbolVariable * sym, NodeTypeDescriptor * nodetype, CompilerState & state) : Node(state), m_varSymbol(sym), m_vid(0), m_nodeInitExpr(NULL), m_currBlockNo(0), m_nodeTypeDesc(nodetype)
  {
    if(sym)
      {
	m_vid = sym->getId();
	m_currBlockNo = sym->getBlockNoOfST();
      }
  }

  NodeVarDecl::NodeVarDecl(const NodeVarDecl& ref) : Node(ref), m_varSymbol(NULL), m_vid(ref.m_vid), m_nodeInitExpr(NULL), m_currBlockNo(ref.m_currBlockNo), m_nodeTypeDesc(NULL)
  {
    if(ref.m_nodeTypeDesc)
      m_nodeTypeDesc = (NodeTypeDescriptor *) ref.m_nodeTypeDesc->instantiate();

    if(ref.m_nodeInitExpr)
      m_nodeInitExpr = (Node *) ref.m_nodeInitExpr->instantiate();
  }

  NodeVarDecl::~NodeVarDecl()
  {
    delete m_nodeTypeDesc;
    m_nodeTypeDesc = NULL;

    delete m_nodeInitExpr;
    m_nodeInitExpr = NULL;
  }

  Node * NodeVarDecl::instantiate()
  {
    return new NodeVarDecl(*this);
  }

  void NodeVarDecl::updateLineage(NNO pno)
  {
    Node::updateLineage(pno);
    if(m_nodeTypeDesc)
      m_nodeTypeDesc->updateLineage(getNodeNo());
    if(m_nodeInitExpr)
      m_nodeInitExpr->updateLineage(getNodeNo());
  } //updateLineage

  bool NodeVarDecl::exchangeKids(Node * oldnptr, Node * newnptr)
  {
    if(m_nodeInitExpr == oldnptr)
      {
	m_nodeInitExpr = newnptr;
	return true;
      }
    return false;
  } //exhangeKids

  bool NodeVarDecl::findNodeNo(NNO n, Node *& foundNode)
  {
    if(Node::findNodeNo(n, foundNode))
      return true;
    if(m_nodeTypeDesc && m_nodeTypeDesc->findNodeNo(n, foundNode))
      return true;
    if(m_nodeInitExpr && m_nodeInitExpr->findNodeNo(n, foundNode))
      return true;
    return false;
  } //findNodeNo

  void NodeVarDecl::checkAbstractInstanceErrors()
  {
    UTI nuti = getNodeType();
    UlamType * nut = m_state.getUlamTypeByIndex(nuti);
    if(nut->getUlamTypeEnum() == Class)
      {
	SymbolClass * csym = NULL;
	AssertBool isDefined = m_state.alreadyDefinedSymbolClass(nuti, csym);
	assert(isDefined);
	if(csym->isAbstract())
	  {
	    std::ostringstream msg;
	    msg << "Instance of Abstract Class ";
	    msg << m_state.getUlamTypeNameBriefByIndex(nuti).c_str();
	    msg << " used with variable symbol name '" << getName() << "'";
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	    setNodeType(Nav);
	  }
      }
  } //checkAbstractInstanceErrors

  //see also SymbolVariable: printPostfixValuesOfVariableDeclarations via ST.
  void NodeVarDecl::printPostfix(File * fp)
  {
    printTypeAndName(fp);
    if(m_nodeInitExpr)
      {
	fp->write(" =");
	m_nodeInitExpr->printPostfix(fp);
      }
    fp->write("; ");
  } //printPostfix

  void NodeVarDecl::printTypeAndName(File * fp)
  {
    UTI vuti = m_varSymbol->getUlamTypeIdx();
    UlamKeyTypeSignature vkey = m_state.getUlamKeyTypeSignatureByIndex(vuti);
    UlamType * vut = m_state.getUlamTypeByIndex(vuti);

    fp->write(" ");
    if(vut->getUlamTypeEnum() != Class)
      fp->write(vkey.getUlamKeyTypeSignatureNameAndBitSize(&m_state).c_str());
    else
      fp->write(vut->getUlamTypeNameBrief().c_str());

    fp->write(" ");
    fp->write(getName());

    s32 arraysize = m_state.getArraySize(vuti);
    if(arraysize > NONARRAYSIZE)
      {
	fp->write("[");
	fp->write_decimal(arraysize);
	fp->write("]");
      }
    else if(arraysize == UNKNOWNSIZE)
      {
	fp->write("[UNKNOWN]");
      }
  } //printTypeAndName

  const char * NodeVarDecl::getName()
  {
    return m_state.m_pool.getDataAsString(m_varSymbol->getId()).c_str();
  }

  const std::string NodeVarDecl::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }

  bool NodeVarDecl::getSymbolPtr(Symbol *& symptrref)
  {
    symptrref = m_varSymbol;
    return true;
  }

  void NodeVarDecl::setInitExpr(Node * node)
  {
    assert(node);
    m_nodeInitExpr = node;
    m_nodeInitExpr->updateLineage(getNodeNo()); //for unknown subtrees
  }

  bool NodeVarDecl::hasInitExpr()
  {
    return (m_nodeInitExpr != NULL);
  }

  bool NodeVarDecl::foldInitExpression()
  {
    assert(0); //TBD;
  }

  FORECAST NodeVarDecl::safeToCastTo(UTI newType)
  {
    assert(m_nodeInitExpr);

    FORECAST rscr = CAST_CLEAR;
    UTI nuti = getNodeType();
    //cast RHS if necessary and safe
    if(UlamType::compare(nuti, newType, m_state) != UTIC_SAME)
	  {
	    //different msg if try to assign non-class to a class type
	    if(m_state.getUlamTypeByIndex(nuti)->getUlamTypeEnum() == Class)
	      {
		std::ostringstream msg;
		msg << "Incompatible class type ";
		msg << m_state.getUlamTypeNameBriefByIndex(nuti).c_str();
		msg << " and ";
		msg << m_state.getUlamTypeNameBriefByIndex(newType).c_str();
		msg << " used with variable initialization";
		MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
		rscr = CAST_BAD; //error
	      }
	    else
	      {
		rscr = m_nodeInitExpr->safeToCastTo(nuti);
		if(rscr != CAST_CLEAR)
		  {
		    std::ostringstream msg;
		    if(m_state.getUlamTypeByIndex(nuti)->getUlamTypeEnum() == Bool)
		      msg << "Use a comparison operator";
		    else
		      msg << "Use explicit cast";
		    msg << " to convert "; // the real converting-message
		    msg << m_state.getUlamTypeNameBriefByIndex(newType).c_str();
		    msg << " to ";
		    msg << m_state.getUlamTypeNameBriefByIndex(nuti).c_str();
		    msg << " for variable initialization";
		    if(rscr == CAST_BAD)
		      MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
		    else
		      MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
		  }
		else if(!Node::makeCastingNode(m_nodeInitExpr, nuti, m_nodeInitExpr))
		  rscr = CAST_BAD; //error
	      }
	  } //safe cast
      return rscr;
  } //safeToCastTo

  UTI NodeVarDecl::checkAndLabelType()
  {
    UTI it = Nav;

    // instantiate, look up in current block
    if(m_varSymbol == NULL)
      checkForSymbol();

    if(m_varSymbol)
      {
	it = m_varSymbol->getUlamTypeIdx(); //base type has arraysize

	UTI cuti = m_state.getCompileThisIdx();
	if(m_nodeTypeDesc)
	  {
	    UTI duti = m_nodeTypeDesc->checkAndLabelType(); //sets goagain
	    if((duti != Nav) && (duti != Hzy) && (duti != it))
	      {
		std::ostringstream msg;
		msg << "REPLACING Symbol UTI" << it;
		msg << ", " << m_state.getUlamTypeNameBriefByIndex(it).c_str();
		msg << " used with variable symbol name '" << getName();
		msg << "' with node type descriptor type: ";
		msg << m_state.getUlamTypeNameBriefByIndex(duti).c_str();
		msg << " UTI" << duti << " while labeling class: ";
		msg << m_state.getUlamTypeNameBriefByIndex(cuti).c_str();
		MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
		m_varSymbol->resetUlamType(duti); //consistent!
		m_state.mapTypesInCurrentClass(it, duti);
		it = duti;
	      }
	  }

	  if(!m_state.isComplete(it))
	  {
	    std::ostringstream msg;
	    msg << "Incomplete Variable Decl for type: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(it).c_str();
	    msg << " used with variable symbol name '" << getName();
	    msg << "' UTI" << it << " while labeling class: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(cuti).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	    it = Hzy;
	    m_state.setGoAgain(); //since not error
	  }
      } //end var_symbol

    ULAMTYPE etyp = m_state.getUlamTypeByIndex(it)->getUlamTypeEnum();
    if(etyp == Void)
      {
	//void only valid use is as a func return type
	std::ostringstream msg;
	msg << "Invalid use of type ";
	msg << m_state.getUlamTypeNameBriefByIndex(it).c_str();
	msg << " with variable symbol name '" << getName() << "'";
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	it = Nav;
      }

    if(m_nodeInitExpr)
      {
	UTI eit = m_nodeInitExpr->checkAndLabelType();
	if(eit == Nav)
	  {
	    std::ostringstream msg;
	    msg << "Initial value expression for: ";
	    msg << m_state.m_pool.getDataAsString(m_vid).c_str();
	    msg << ", initialization is invalid";
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	    setNodeType(Nav);
	    return Nav; //short-circuit
	  }

	if(eit == Hzy)
	  {
	    std::ostringstream msg;
	    msg << "Initial value expression for: ";
	    msg << m_state.m_pool.getDataAsString(m_vid).c_str();
	    msg << ", initialization is not ready";
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	    m_state.setGoAgain(); //since not error
	    setNodeType(Hzy);
	    return Hzy; //short-circuit
	  }

	setNodeType(it); //needed before safeToCast
	FORECAST scr = safeToCastTo(eit);
	if(scr == CAST_BAD)
	  it = Nav; //error
	else if(scr == CAST_HAZY)
	  it = Hzy; //not ready
	//else CAST_SAFE ok
      }
    setStoreIntoAble(true);
    setNodeType(it);
    return getNodeType();
  } //checkAndLabelType

  void NodeVarDecl::checkForSymbol()
  {
    //in case of a cloned unknown
    NodeBlock * currBlock = getBlock();
    m_state.pushCurrentBlockAndDontUseMemberBlock(currBlock);

    Symbol * asymptr = NULL;
    bool hazyKin = false;
    if(m_state.alreadyDefinedSymbol(m_vid, asymptr, hazyKin) && !hazyKin)
      {
	if(!asymptr->isTypedef() && !asymptr->isConstant() && !asymptr->isModelParameter() && !asymptr->isFunction())
	  {
	    m_varSymbol = (SymbolVariable *) asymptr;
	  }
	else
	  {
	    std::ostringstream msg;
	    msg << "(1) <" << m_state.m_pool.getDataAsString(m_vid).c_str();
	    msg << "> is not a variable, and cannot be used as one";
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	  }
      }
    else
      {
	std::ostringstream msg;
	msg << "(2) Variable <" << m_state.m_pool.getDataAsString(m_vid).c_str();
	msg << "> is not defined, and cannot be used";
	if(!hazyKin)
	  MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	else
	  MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
      } //alreadyDefined

    m_state.popClassContext(); //restore
  } //checkForSymbol

  NNO NodeVarDecl::getBlockNo()
  {
    return m_currBlockNo;
  }

  NodeBlock * NodeVarDecl::getBlock()
  {
    assert(m_currBlockNo);
    NodeBlock * currBlock = (NodeBlock *) m_state.findNodeNoInThisClass(m_currBlockNo);
    assert(currBlock);
    return currBlock;
  }

  void NodeVarDecl::packBitsInOrderOfDeclaration(u32& offset)
  {
    assert((s32) offset >= 0); //neg is invalid
    assert(m_varSymbol);

    m_varSymbol->setPosOffset(offset);
    UTI it = m_varSymbol->getUlamTypeIdx();

    if(!m_state.isComplete(it))
      {
	UTI cuti = m_state.getCompileThisIdx();
	UTI mappedUTI = Nav;
	if(m_state.mappedIncompleteUTI(cuti, it, mappedUTI))
	  {
	    std::ostringstream msg;
	    msg << "Substituting Mapped UTI" << mappedUTI;
	    msg << ", " << m_state.getUlamTypeNameBriefByIndex(mappedUTI).c_str();
	    msg << " for incomplete Variable Decl for type: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(it).c_str();
	    msg << " used with variable symbol name '" << getName();
	    msg << "' (UTI" << it << ") while bit packing class: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(cuti).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	    it = mappedUTI;
	    m_varSymbol->resetUlamType(it); //consistent!
	  }

	if(!m_state.isComplete(it)) //reloads
	  {
	    std::ostringstream msg;
	    msg << "Unresolved type <";
	    msg << m_state.getUlamTypeNameBriefByIndex(it).c_str();
	    msg << "> used with variable symbol name '" << getName();
	    msg << "' found while bit packing class ";
	    msg << m_state.getUlamTypeNameBriefByIndex(cuti).c_str();
	    MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
	  }
      } //not complete

    ULAMCLASSTYPE classtype = m_state.getUlamTypeByIndex(it)->getUlamClass();
    if(m_state.getTotalBitSize(it) > MAXBITSPERLONG && classtype == UC_NOTACLASS)
      {
	std::ostringstream msg;
	msg << "Data member <" << getName() << "> of type: ";
	msg << m_state.getUlamTypeNameBriefByIndex(it).c_str();
	msg << ", total size: " << (s32) m_state.getTotalBitSize(it);
	msg << " MUST fit into " << MAXBITSPERLONG << " bits;";
	msg << " Local variables do not have this restriction";
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
      }

    if(m_state.getTotalBitSize(it) > MAXBITSPERINT && classtype == UC_QUARK)
      {
	std::ostringstream msg;
	msg << "Data member <" << getName() << "> of type: ";
	msg << m_state.getUlamTypeNameBriefByIndex(it).c_str();
	msg << ", total size: " << (s32) m_state.getTotalBitSize(it);
	msg << " MUST fit into " << MAXBITSPERINT << " bits;";
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
      }

    if(classtype == UC_ELEMENT)
      {
	std::ostringstream msg;
	msg << "Data member <" << getName() << "> of type: ";
	msg << m_state.getUlamTypeNameBriefByIndex(it).c_str();
	msg << ", is an element, and is NOT permitted; Local variables, quarks, ";
	msg << "and Model Parameters do not have this restriction";
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), ERR);
      }

    offset += m_state.getTotalBitSize(m_varSymbol->getUlamTypeIdx());
  } //packBitsInOrderOfDeclaration

  void NodeVarDecl::calcMaxDepth(u32& depth, u32& maxdepth, s32 base)
  {
    assert(m_varSymbol);
    s32 newslot = depth + base;
    s32 oldslot = ((SymbolVariable *) m_varSymbol)->getStackFrameSlotIndex();
    if(oldslot != newslot)
      {
	std::ostringstream msg;
	msg << "'" << m_state.m_pool.getDataAsString(m_vid).c_str();
	msg << "' was at slot: " << oldslot << ", new slot is " << newslot;
	MSG(getNodeLocationAsString().c_str(), msg.str().c_str(), DEBUG);
	((SymbolVariable *) m_varSymbol)->setStackFrameSlotIndex(newslot);
      }
    depth += m_state.slotsNeeded(getNodeType());

    if(m_nodeInitExpr)
      m_nodeInitExpr->calcMaxDepth(depth, maxdepth, base);
  } //calcMaxDepth

  void NodeVarDecl::countNavNodes(u32& cnt)
  {
    if(m_nodeTypeDesc)
      m_nodeTypeDesc->countNavNodes(cnt);

    if(m_nodeInitExpr)
      m_nodeInitExpr->countNavNodes(cnt);

    Node::countNavNodes(cnt);
  } //countNavNodes

  EvalStatus NodeVarDecl::eval()
  {
    assert(m_varSymbol);

    UTI nuti = getNodeType();
    if(nuti == Nav)
      return ERROR;

    if(nuti == Hzy)
      return NOTREADY;

    assert(m_varSymbol->getUlamTypeIdx() == nuti); //is it so? if so, some cleanup needed

    assert(!m_varSymbol->isAutoLocal()); //NodeVarRef::eval

    UlamType * nut = m_state.getUlamTypeByIndex(nuti);
    ULAMCLASSTYPE classtype = nut->getUlamClass();
    if(nut->getUlamTypeEnum() == UAtom)
      {
	UlamValue atomUV = UlamValue::makeAtom(m_varSymbol->getUlamTypeIdx());
	m_state.m_funcCallStack.storeUlamValueInSlot(atomUV, ((SymbolVariableStack *) m_varSymbol)->getStackFrameSlotIndex());
      }
    else if(classtype == UC_ELEMENT)
      {
	UlamValue atomUV = UlamValue::makeDefaultAtom(m_varSymbol->getUlamTypeIdx(), m_state);
	m_state.m_funcCallStack.storeUlamValueInSlot(atomUV, ((SymbolVariableStack *) m_varSymbol)->getStackFrameSlotIndex());
      }
    else if(!m_varSymbol->isDataMember())
      {
	if(classtype == UC_NOTACLASS)
	  {
	    //local variable to a function;
	    // t.f. must be SymbolVariableStack, not SymbolVariableDataMember
	    u32 len = m_state.getTotalBitSize(nuti);
	    UlamValue immUV;
	    if(len <= MAXBITSPERINT)
	      immUV = UlamValue::makeImmediate(m_varSymbol->getUlamTypeIdx(), 0, m_state);
	    else if(len <= MAXBITSPERLONG)
	      immUV = UlamValue::makeImmediateLong(m_varSymbol->getUlamTypeIdx(), 0, m_state);
	    else
	      immUV = UlamValue::makePtr(((SymbolVariableStack *) m_varSymbol)->getStackFrameSlotIndex(), STACK, getNodeType(), m_state.determinePackable(getNodeType()), m_state, 0, m_varSymbol->getId()); //array ptr

	    m_state.m_funcCallStack.storeUlamValueInSlot(immUV, ((SymbolVariableStack *) m_varSymbol)->getStackFrameSlotIndex());
	  }
	else
	  {
	    //must be a local quark!
	    u32 dq = 0;
	    AssertBool isDefinedQuark = m_state.getDefaultQuark(nuti, dq);
	    assert(isDefinedQuark);
	    if(m_state.isScalar(nuti))
	      {
		UlamValue immUV = UlamValue::makeImmediate(nuti, dq, m_state);
		m_state.m_funcCallStack.storeUlamValueInSlot(immUV, ((SymbolVariableStack *) m_varSymbol)->getStackFrameSlotIndex());
	      }
	    else
	      {
		u32 len = m_state.getTotalBitSize(nuti);
		u32 bitsize = m_state.getBitSize(nuti);
		u32 pos = 0;
		u32 arraysize = m_state.getArraySize(nuti);
		u32 mask = _GetNOnes32((u32) bitsize);
		dq &= mask;
		if(len <= MAXBITSPERINT)
		  {
		    u32 dqarrval = 0;
		    //similar to build default quark value in NodeVarDeclDM
		    for(u32 j = 1; j <= arraysize; j++)
		      {
			dqarrval |= (dq << (len - (pos + (j * bitsize))));
		      }

		    UlamValue immUV = UlamValue::makeImmediate(nuti, dqarrval, m_state);
		    m_state.m_funcCallStack.storeUlamValueInSlot(immUV, ((SymbolVariableStack *) m_varSymbol)->getStackFrameSlotIndex());
		  }
		else if(len <= MAXBITSPERLONG)
		  {
		    u64 dqarrval = 0;
		    //similar to build default quark value in NodeVarDeclDM
		    for(u32 j = 1; j <= arraysize; j++)
		      {
			dqarrval |= (dq << (len - (pos + (j * bitsize))));
		      }

		    UlamValue immUV = UlamValue::makeImmediateLong(nuti, dqarrval, m_state);
		    m_state.m_funcCallStack.storeUlamValueInSlot(immUV, ((SymbolVariableStack *) m_varSymbol)->getStackFrameSlotIndex());
		  }
		else
		  {
		    //unpacked array of quarks is unsupported at this time!!
		    assert(0);
		  }
	      }
	  }
      }
    else
      {
	//see NodeVarDeclDM for data members..
	assert(0);
      }

    if(m_nodeInitExpr)
      return evalInitExpr();
    return NORMAL;
  } //eval

  EvalStatus NodeVarDecl::evalInitExpr()
  {
    EvalStatus evs = NORMAL; //init
    // quark or nonclass data member;
    evalNodeProlog(0); //new current node eval frame pointer

    makeRoomForSlots(1); //always 1 slot for ptr

    evs = evalToStoreInto();
    if(evs != NORMAL)
      {
	evalNodeEpilog();
	return evs;
      }

    UlamValue pluv = m_state.m_nodeEvalStack.loadUlamValuePtrFromSlot(1);

    u32 slot = makeRoomForNodeType(getNodeType());

    evs = m_nodeInitExpr->eval();

    if(evs == NORMAL)
      {
	if(slot)
	  {
	    UlamValue ruv = m_state.m_nodeEvalStack.loadUlamValueFromSlot(slot+1); //immediate scalar
	    m_state.assignValue(pluv,ruv);

	    //also copy result UV to stack, -1 relative to current frame pointer
	    Node::assignReturnValueToStack(ruv);
	  }
      } //normal
    evalNodeEpilog();
    return evs;
  } //evalInitExpr

  EvalStatus NodeVarDecl::evalToStoreInto()
  {
    evalNodeProlog(0); //new current node eval frame pointer

    // return ptr to this local var (from NodeIdent's makeUlamValuePtr)
    UlamValue rtnUVPtr = makeUlamValuePtr();

    //copy result UV to stack, -1 relative to current frame pointer
    Node::assignReturnValuePtrToStack(rtnUVPtr);

    evalNodeEpilog();
    return NORMAL;
  } //evalToStoreInto

  UlamValue NodeVarDecl::makeUlamValuePtr()
  {
    // (from NodeIdent's makeUlamValuePtr)
    UlamValue ptr;
    if(m_varSymbol->isSelf())
      {
	//when "self/atom" is a quark, we're inside a func called on a quark (e.g. dm or local)
	//'atom' gets entire atom/element containing this quark; including its type!
	//'self' gets type/pos/len of the quark from which 'atom' can be extracted
	UlamValue selfuvp = m_state.m_currentSelfPtr;
	UTI ttype = selfuvp.getPtrTargetType();
	if((m_state.getUlamTypeByIndex(ttype)->getUlamClass() == UC_QUARK))
	  {
	    u32 vid = m_varSymbol->getId();
	    if(vid == m_state.m_pool.getIndexForDataString("atom"))
	      {
		selfuvp = m_state.getAtomPtrFromSelfPtr();
	      }
	    //else
	  }
	return selfuvp;
      } //done

    assert(!m_varSymbol->isAutoLocal()); //nodevarref, not here!

    ULAMCLASSTYPE classtype = m_state.getUlamTypeByIndex(getNodeType())->getUlamClass();
    if(classtype == UC_ELEMENT)
      {
	  // ptr to explicit atom or element, (e.g.'f' in f.a=1) becomes new m_currentObjPtr
	  ptr = UlamValue::makePtr(m_varSymbol->getStackFrameSlotIndex(), STACK, getNodeType(), UNPACKED, m_state, 0, m_varSymbol->getId());
      }
    else
      {
	assert(!m_varSymbol->isDataMember());
	//local variable on the stack; could be array ptr!
	ptr = UlamValue::makePtr(m_varSymbol->getStackFrameSlotIndex(), STACK, getNodeType(), m_state.determinePackable(getNodeType()), m_state, 0, m_varSymbol->getId());
      }
    return ptr;
  } //makeUlamValuePtr

  // parse tree in order declared, unlike the ST.
  void NodeVarDecl::genCode(File * fp, UlamValue& uvpass)
  {
    assert(m_varSymbol);
    assert(m_state.isComplete(getNodeType()));

    assert(!m_varSymbol->isDataMember()); //NodeVarDeclDM::genCode
    assert(!m_varSymbol->isAutoLocal()); //NodeVarRef::genCode

    UTI vuti = m_varSymbol->getUlamTypeIdx();
    UlamType * vut = m_state.getUlamTypeByIndex(vuti);

    ULAMCLASSTYPE vclasstype = vut->getUlamClass();

    if(m_nodeInitExpr)
      {
	m_nodeInitExpr->genCode(fp, uvpass);

	m_state.indent(fp);
	fp->write(vut->getLocalStorageTypeAsString().c_str()); //for C++ local vars
	fp->write(" ");
	fp->write(m_varSymbol->getMangledName().c_str());
	fp->write("("); // use constructor (not equals)
	fp->write(m_state.getTmpVarAsString(vuti, uvpass.getPtrSlotIndex(), uvpass.getPtrStorage()).c_str()); //VALUE
	fp->write(")");
	fp->write(";\n"); //func call args aren't NodeVarDecl's
	m_state.m_currentObjSymbolsForCodeGen.clear();
	return;
      }

    //initialize T to default atom (might need "OurAtom" if data member ?)
    if(vclasstype == UC_ELEMENT)
      {
	m_state.indent(fp);
	fp->write(vut->getLocalStorageTypeAsString().c_str()); //for C++ local vars
	fp->write(" ");
	fp->write(m_varSymbol->getMangledName().c_str());
	if(vut->isScalar())
	  {
	    fp->write(" = ");
	    fp->write(m_state.getUlamTypeByIndex(vuti)->getUlamTypeMangledName().c_str());
	    fp->write("<EC>");
	    fp->write("::THE_INSTANCE");
	    fp->write(".GetDefaultAtom()"); //returns object of type T
	  }
	//else
      }
    else if(vclasstype == UC_QUARK)
      {
	//left-justified?
	m_state.indent(fp);
	fp->write(vut->getLocalStorageTypeAsString().c_str()); //for C++ local vars
	fp->write(" ");
	fp->write(m_varSymbol->getMangledName().c_str());
      }
    else
      {
	m_state.indent(fp);
	fp->write(vut->getLocalStorageTypeAsString().c_str()); //for C++ local vars
	fp->write(" ");
	fp->write(m_varSymbol->getMangledName().c_str()); //default 0
      }
    fp->write(";\n"); //func call args aren't NodeVarDecl's
    m_state.m_currentObjSymbolsForCodeGen.clear();
  } //genCode

  void NodeVarDecl::generateUlamClassInfo(File * fp, bool declOnly, u32& dmcount)
  {
    assert(0); //see NodeVarDeclDM data members only
  } //generateUlamClassInfo

} //end MFM
