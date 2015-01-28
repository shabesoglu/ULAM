#include <stdio.h>
#include "NodeControl.h"
#include "CompilerState.h"
#include "UlamTypeBool.h"

namespace MFM {

  NodeControl::NodeControl(Node * condNode, Node * trueNode, CompilerState & state): Node(state), m_nodeCondition(condNode), m_nodeBody(trueNode) {}
  NodeControl::NodeControl(const NodeControl& ref) : Node(ref)
  {
    m_nodeCondition = ref.m_nodeCondition->clone();
    m_nodeBody = ref.m_nodeBody->clone();
  }

  NodeControl::~NodeControl()
  {
    delete m_nodeCondition;
    m_nodeCondition = NULL;
    delete m_nodeBody;
    m_nodeBody = NULL;
  }

  void NodeControl::updateLineage(Node * p)
  {
    setYourParent(p);
    m_nodeCondition->updateLineage(this);
    m_nodeBody->updateLineage(this);
  }

  void NodeControl::print(File * fp)
  {
    printNodeLocation(fp);
    UTI myut = getNodeType();
    char id[255];
    if(myut == Nav)
      sprintf(id,"%s<NOTYPE>\n",prettyNodeName().c_str());
    else
      sprintf(id,"%s<%s>\n",prettyNodeName().c_str(), m_state.getUlamTypeNameByIndex(myut).c_str());
    fp->write(id);

    fp->write("condition:\n");
    if(m_nodeCondition)
      m_nodeCondition->print(fp);
    else
      fp->write("<NULLCOND>\n");

    sprintf(id,"%s:\n",getName());
    fp->write(id);

    if(m_nodeBody)
      m_nodeBody->print(fp);
    else
      fp->write("<NULLTRUE>\n");
  }


  void NodeControl::printPostfix(File * fp)
  {
    if(m_nodeCondition)
      m_nodeCondition->printPostfix(fp);
    else
      fp->write("<NULLCONDITION>");

    fp->write(" cond");

    if(m_nodeBody)
      m_nodeBody->printPostfix(fp);
    else
      fp->write("<NULLTRUE>");

    printOp(fp);  //operators last
  }


  void NodeControl::printOp(File * fp)
  {
    char myname[16];
    sprintf(myname," %s", getName());
    fp->write(myname);
  }


  UTI NodeControl::checkAndLabelType()
  {
    assert(m_nodeCondition && m_nodeBody);
    UTI newType = Bool;
    ULAMTYPE newEnumTyp = Bool;

    // condition should be a bool, always cast
    UTI cuti = m_nodeCondition->checkAndLabelType();
    assert(m_state.isScalar(cuti));

    UlamType * cut = m_state.getUlamTypeByIndex(cuti);
    ULAMTYPE ctypEnum = cut->getUlamTypeEnum();

    if(ctypEnum != newEnumTyp)
      {
	m_nodeCondition = makeCastingNode(m_nodeCondition, newType);
      }
    else
      {
	//always cast: Bools are maintained as unsigned in gen code, until c-bool is needed
	m_nodeCondition = makeCastingNode(m_nodeCondition, cuti);
	newType = cuti;
      }

    m_nodeBody->checkAndLabelType(); //side-effect
    setNodeType(newType);  //stays the same

    setStoreIntoAble(false);
    return getNodeType();
  } //checkAndLabelType


  void NodeControl::countNavNodes(u32& cnt)
  {
    m_nodeCondition->countNavNodes(cnt);
    m_nodeBody->countNavNodes(cnt);
  }


  void NodeControl::genCode(File * fp, UlamValue& uvpass)
  {
    assert(m_nodeCondition && m_nodeBody);

    //bracket for overall tmpvars (e.g. condition and body) is
    // generated by the specific control; e.g. final bracket depends
    // on presence of 'else'.

    m_nodeCondition->genCode(fp, uvpass);

    bool isTerminal = false;
    UTI cuti = uvpass.getUlamValueTypeIdx();

    if(cuti == Ptr)
      {
	cuti = uvpass.getPtrTargetType();
      }
    else
      {
	isTerminal = true;
      }

    UlamType * cut = m_state.getUlamTypeByIndex(cuti);

    m_state.indent(fp);
    fp->write(getName());  //if, while
    fp->write("(");

    if(isTerminal)
      {
	// fp->write("(bool) ");
	// write out terminal explicitly
	u32 data = uvpass.getImmediateData(m_state);
	char dstr[40];
	cut->getDataAsString(data, dstr, 'z');
	fp->write(dstr);
      }
    else
      {
	if(m_state.m_genCodingConditionalAs)
	  {
	    assert(cut->getUlamTypeEnum() == Bool);
	    fp->write(((UlamTypeBool *) cut)->getConvertToCboolMethod().c_str());
	    fp->write("((");
	    fp->write(m_state.getTmpVarAsString(cuti, uvpass.getPtrSlotIndex()).c_str());
	    fp->write(" >= 0 ? 1 : 0), ");   //test for 'has' part of 'as'
	    fp->write_decimal(cut->getBitSize());
	    fp->write(")");
	  }
	else
	  {
	    // regular condition
	    assert(cut->getUlamTypeEnum() == Bool);
	    fp->write(((UlamTypeBool *) cut)->getConvertToCboolMethod().c_str());
	    fp->write("(");
	    fp->write(m_state.getTmpVarAsString(cuti, uvpass.getPtrSlotIndex(), uvpass.getPtrStorage()).c_str());
	    fp->write(", ");
	    fp->write_decimal(cut->getBitSize());
	    fp->write(")");
	  }
      }
    fp->write(")\n");

    m_state.indent(fp);
    fp->write("{\n");
    m_state.m_currentIndentLevel++;

    //note: in case of as-conditional, uvpass still has the tmpvariable containing the pos!
    m_nodeBody->genCode(fp, uvpass);

    //probably should have been done within the body, to avoid any
    //subsequent if/whiles from misinterpretting it as there's; if so, again, moot.
    assert(!m_state.m_genCodingConditionalAs);

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("} // end ");
    fp->write(getName()); //end
    fp->write("\n");
  } //genCode

} //end MFM
