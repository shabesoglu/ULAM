#include <stdio.h>
#include "NodeInstanceof.h"
#include "CompilerState.h"

namespace MFM {

  NodeInstanceof::NodeInstanceof(Token tokof, NodeTypeDescriptor * nodetype, CompilerState & state) : NodeAtomof(tokof, nodetype, state) { }

  NodeInstanceof::NodeInstanceof(const NodeInstanceof& ref) : NodeAtomof(ref) { }

  NodeInstanceof::~NodeInstanceof() { }

  Node * NodeInstanceof::instantiate()
  {
    return new NodeInstanceof(*this);
  }

  const char * NodeInstanceof::getName()
  {
    return ".instanceof";
  }

  const std::string NodeInstanceof::prettyNodeName()
  {
    return nodeName(__PRETTY_FUNCTION__);
  }

  FORECAST NodeInstanceof::safeToCastTo(UTI newType)
  {
    UTI nuti = getNodeType(); //UAtom for references; ow type of class/atom
    if(m_state.isAtom(nuti))
      return NodeAtomof::safeToCastTo(newType);

    UlamType * newut = m_state.getUlamTypeByIndex(newType);
    return newut->safeCast(nuti);
  } //safeToCastTo

  UTI NodeInstanceof::checkAndLabelType()
  {
    NodeAtomof::checkAndLabelType();

    UTI oftype = NodeAtomof::getOfType();
    if(m_state.okUTItoContinue(oftype))
      {
	bool isself = m_varSymbol ? m_varSymbol->isSelf() : false;
	if(m_state.isReference(oftype) || isself) //including 'self'
	  setNodeType(UAtom); //effective type known only at runtime
	else
	  setNodeType(oftype); //Type or variable
      }

    Node::setStoreIntoAble(TBOOL_FALSE);
    return getNodeType();
  } //checkAndLabelType

  UlamValue NodeInstanceof::makeUlamValuePtr()
  {
    // (from NodeVarDecl's makeUlamValuePtr)
    UlamValue ptr;
    UlamValue atomuv;

    UTI auti = getOfType();
    UlamType * aut = m_state.getUlamTypeByIndex(auti);
    ULAMCLASSTYPE aclasstype = aut->getUlamClass();

    //u32 atop = 1; //m_state.m_funcCallStack.getAbsoluteTopOfStackIndexOfNextSlot();
    u32 atop = m_state.m_funcCallStack.getAbsoluteStackIndexOfSlot(1);
    if(m_state.isAtom(auti))
      atomuv = UlamValue::makeAtom(auti);
    else if(aclasstype == UC_ELEMENT)
      atomuv = UlamValue::makeDefaultAtom(auti, m_state);
    else if(aclasstype == UC_QUARK)
      {
	u32 dq = 0;
	AssertBool isDefinedQuark = m_state.getDefaultQuark(auti, dq); //returns scalar dq
	assert(isDefinedQuark);
	atomuv = UlamValue::makeImmediate(auti, dq, m_state);
      }
    else
      assert(0);

    m_state.m_funcCallStack.storeUlamValueAtStackIndex(atomuv, atop); //stackframeslotindex ?
    //m_state.m_funcCallStack.storeUlamValueInSlot(atomuv, atop); //relative to current frame

    ptr = UlamValue::makePtr(atop, STACK, auti, UNPACKED, m_state, 0);
    ptr.setUlamValueTypeIdx(PtrAbs);
    return ptr;
  } //makeUlamValuePtr

  void NodeInstanceof::genCode(File * fp, UlamValue& uvpass)
  {
    //generates a new instance of..
    UTI nuti = getNodeType();
    UlamType * nut = m_state.getUlamTypeByIndex(nuti);
    s32 tmpVarNum = m_state.getNextTmpVarNumber(); //tmp for atomref

    //starts out as its default type; references (UAtom) are updated:
    m_state.indent(fp); //non-const
    fp->write(nut->getLocalStorageTypeAsString().c_str()); //for C++ local vars
    fp->write(" ");
    fp->write(m_state.getTmpVarAsString(nuti, tmpVarNum, TMPBITVAL).c_str());
    fp->write(";\n");

    // a reference (including 'self'), returns a UAtom of effective type;
    // effective self known only at runtime.
    if((m_token.m_type == TOK_IDENTIFIER))
      {
	assert(m_varSymbol);
	UTI vuti = m_varSymbol->getUlamTypeIdx();
	bool isself = m_varSymbol->isSelf();

	if(m_state.isReference(vuti) || isself)
	  {
	    u32 tmpuclass = m_state.getNextTmpVarNumber(); //only for this case
	    m_state.indent(fp);
	    fp->write("const UlamClass<EC> * ");
	    fp->write(m_state.getUlamClassTmpVarAsString(tmpuclass).c_str());
	    fp->write(" = ");
	    if(isself)
	      fp->write("ur");
	    else
	      fp->write(m_varSymbol->getMangledName().c_str());
	    fp->write(".GetEffectiveSelf();\n");

	    m_state.indent(fp);
	    fp->write("if(");
	    fp->write(m_state.getUlamClassTmpVarAsString(tmpuclass).c_str());
	    fp->write(" == NULL) FAIL(ILLEGAL_ARGUMENT);\n");

	    m_state.indent(fp);
	    fp->write("if(");
	    fp->write(m_state.getUlamClassTmpVarAsString(tmpuclass).c_str());
	    fp->write("->AsUlamQuark() != NULL)\n");

	    //returns ATOM_UNDEFINED_TYPE, an immediate default quark
	    m_state.m_currentIndentLevel++;
	    m_state.indent(fp);
	    fp->write(m_state.getTmpVarAsString(nuti, tmpVarNum, TMPBITVAL).c_str());
	    fp->write(".Write(");
	    fp->write("((UlamQuark<EC> *) ");
	    fp->write(m_state.getUlamClassTmpVarAsString(tmpuclass).c_str());
	    fp->write(")->getDefaultQuark()");
	    fp->write("); //instanceof default quark\n");
	    m_state.m_currentIndentLevel--;

	    m_state.indent(fp);
	    fp->write("else\n");

	    m_state.m_currentIndentLevel++;
	    m_state.indent(fp);
	    fp->write(m_state.getTmpVarAsString(nuti, tmpVarNum, TMPBITVAL).c_str());
	    fp->write(".WriteAtom(");
	    fp->write("((UlamElement<EC> *) ");
	    fp->write(m_state.getUlamClassTmpVarAsString(tmpuclass).c_str());
	    fp->write(")->GetDefaultAtom()); //instanceof default element\n");
	    m_state.m_currentIndentLevel--;
	  }
      }
    uvpass = UlamValue::makePtr(tmpVarNum, TMPBITVAL, nuti, UNPACKED, m_state, 0, m_varSymbol ? m_varSymbol->getId() : 0);
#if 0

    // THE READ: coming soon!!
    s32 tmpVarNum2 = m_state.getNextTmpVarNumber(); //tmp for atomref
    STORAGE rstor = nut->getUlamClass() == UC_QUARK ? TMPREGISTER : TMPBITVAL;
    PACKFIT packfit = nut->getUlamClass() == UC_QUARK ? PACKEDLOADABLE : UNPACKED;

    m_state.indent(fp);
    fp->write("const ");
    fp->write(nut->getTmpStorageTypeAsString().c_str()); //for C++ local vars
    fp->write(m_state.getTmpVarAsString(nuti, tmpVarNum2, rstor).c_str());
    fp->write(" = ");
    fp->write(m_state.getTmpVarAsString(nuti, tmpVarNum, TMPBITVAL).c_str());
    fp->write(nut->getReadMethodForCodeGen().c_str());
    fp->write("();\n");

    uvpass = UlamValue::makePtr(tmpVarNum2, rstor, nuti, packfit, m_state, 0, m_varSymbol ? m_varSymbol->getId() : 0);
#endif
  } //genCode

  void NodeInstanceof::genCodeToStoreInto(File * fp, UlamValue& uvpass)
  {
    //lhs
    assert(getStoreIntoAble() == TBOOL_TRUE); //not so.
    //NodeInstanceof::genCode(fp, uvpass);
  } //genCodeToStoreInto

} //end MFM
