#include <assert.h>
#include <stdio.h>
#include <iostream>
#include "SymbolTableOfVariables.h"
#include "SymbolParameterValue.h"
#include "SymbolVariable.h"
#include "SymbolVariableDataMember.h"
#include "CompilerState.h"
#include "NodeBlockClass.h"
#include "MapParameterDesc.h"
#include "MapDataMemberDesc.h"
#include "MapConstantDesc.h"
#include "MapTypedefDesc.h"

namespace MFM {

  SymbolTableOfVariables::SymbolTableOfVariables(CompilerState& state): SymbolTable(state) { }

  SymbolTableOfVariables::SymbolTableOfVariables(const SymbolTableOfVariables& ref) : SymbolTable(ref) { }

  SymbolTableOfVariables::~SymbolTableOfVariables() { }

  //called by NodeBlockClass.
  u32 SymbolTableOfVariables::getNumberOfConstantSymbolsInTable(bool argsOnly)
  {
    u32 cntOfConstants = 0;
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	assert(sym);
	if(sym->isConstant())
	  {
	    if(!argsOnly || ((SymbolConstantValue *) sym)->isArgument())
	      cntOfConstants++;
	  }
	it++;
      }
    return cntOfConstants;
  } //getNumberOfConstantSymbolsInTable

  //called by NodeBlock.
  u32 SymbolTableOfVariables::getTotalSymbolSize()
  {
    u32 totalsizes = 0;
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	//typedefs don't contribute to the total bit size
	if(variableSymbolWithCountableSize(sym))
	  {
	    totalsizes += m_state.slotsNeeded(sym->getUlamTypeIdx());
	  }
	it++;
      }
    return totalsizes;
  } //getTotalSymbolSize

  s32 SymbolTableOfVariables::getTotalVariableSymbolsBitSize()
  {
    ULAMCLASSTYPE cclasstype = m_state.getUlamTypeByIndex(m_state.getCompileThisIdx())->getUlamClassType();

    s32 totalsizes = 0;
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	assert(!sym->isFunction());

	if(!variableSymbolWithCountableSize(sym))
	  {
	    it++;
	    continue;
	  }

	UTI suti = sym->getUlamTypeIdx();
	UlamType * sut = m_state.getUlamTypeByIndex(suti);
	s32 symsize = calcVariableSymbolTypeSize(suti); //recursively

	if(symsize == CYCLEFLAG) //was < 0
	  {
	    std::ostringstream msg;
	    msg << "cycle error!!! " << m_state.getUlamTypeNameByIndex(suti).c_str();
	    MSG(sym->getTokPtr(), msg.str().c_str(), ERR);
	  }
	else if(symsize == EMPTYSYMBOLTABLE)
	  {
	    symsize = 0;
	    m_state.setBitSize(suti, symsize); //total bits NOT including arrays
	  }
	else if(symsize <= UNKNOWNSIZE)
	  {
	    std::ostringstream msg;
	    msg << "UNKNOWN !!! " << m_state.getUlamTypeNameByIndex(suti).c_str();
	    msg << " UTI" << suti << " while compiling class: ";
	    msg << m_state.getUlamTypeNameBriefByIndex(m_state.getCompileThisIdx()).c_str();
	    MSG(sym->getTokPtr(), msg.str().c_str(), DEBUG);
	    totalsizes = UNKNOWNSIZE;
	    break;
	  }
	else
	  m_state.setBitSize(suti, symsize); //symsize does not include arrays

	if((cclasstype == UC_TRANSIENT) && (sut->getUlamClassType() == UC_ELEMENT))
	  {
	    s32 arraysize = sut->getArraySize();
	    arraysize = arraysize <= 0 ? 1 : arraysize;
	    totalsizes += (BITSPERATOM * arraysize);
	  }
	else
	  totalsizes += m_state.getTotalBitSize(suti); //covers up any unknown sizes; includes arrays
	it++;
      } //while
    return totalsizes;
  } //getTotalVariableSymbolsBitSize

  s32 SymbolTableOfVariables::getMaxVariableSymbolsBitSize()
  {
    s32 maxsize = 0;
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	assert(!sym->isFunction());

	if(!variableSymbolWithCountableSize(sym))
	  {
	    it++;
	    continue;
	  }

	UTI sut = sym->getUlamTypeIdx();
	s32 symsize = calcVariableSymbolTypeSize(sut); //recursively

	if(symsize == CYCLEFLAG) //was < 0
	  {
	    std::ostringstream msg;
	    msg << "cycle error!!!! " << m_state.getUlamTypeNameByIndex(sut).c_str();
	    MSG(sym->getTokPtr(), msg.str().c_str(),ERR);
	  }
	else if(symsize == EMPTYSYMBOLTABLE)
	  {
	    symsize = 0;
	    m_state.setBitSize(sut, symsize); //total bits NOT including arrays
	  }
	else
	  {
	    m_state.setBitSize(sut, symsize); //symsize does not include arrays
	  }

	if((s32) m_state.getTotalBitSize(sut) > maxsize)
	  maxsize = m_state.getTotalBitSize(sut); //includes arrays

	it++;
      }
    return maxsize;
  } //getMaxVariableSymbolsBitSize

  //#define OPTIMIZE_PACKED_BITS
#ifdef OPTIMIZE_PACKED_BITS
  //currently, packing is done by Nodes since the order of declaration is available;
  //but in case we may want to optimize the layout someday,
  //we keep this here since all the symbols are available in one place.
  void SymbolTableOfVariables::packBitsForTableOfVariableDataMembers()
  {
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    u32 offsetIntoAtom = 0;

    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	UTI suti = sym->getUlamTypeIdx();
	if(sym->isDataMember() && variableSymbolWithCountableSize(sym) && !m_state.isClassAQuarkUnion(suti))
	  {
	    //updates the offset with the bit size of sym
	    ((SymbolVariable *) sym)->setPosOffset(offsetIntoAtom);
	    offsetIntoAtom += m_state.getTotalBitSize(suti); //times array size
	  }
	it++;
      }
  }
#endif

  void SymbolTableOfVariables::initializeElementDefaultsForEval(UlamValue& uvsite, UTI startuti)
  {
    if(m_idToSymbolPtr.empty()) return;

    u32 startpos = ATOMFIRSTSTATEBITPOS; //use relative offsets

    std::map<u32, Symbol* >::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	UTI suti = sym->getUlamTypeIdx();
	UlamType * sut = m_state.getUlamTypeByIndex(suti);

	//skip quarkunion initializations
	if(sym->isDataMember() && variableSymbolWithCountableSize(sym) && !m_state.isClassAQuarkUnion(suti))
	  {
	    s32 bitsize = sut->getBitSize();
	    u32 pos = ((SymbolVariableDataMember *) sym)->getPosOffset();

	    //updates the UV at offset with the default of sym; non-class arrays have none
	    if(((SymbolVariableDataMember *) sym)->hasInitValue())
	      {
		u64 dval = 0;
		if(((SymbolVariableDataMember *) sym)->getInitValue(dval))
		  {
		    u32 wordsize = sut->getTotalWordSize();
		    if(wordsize <= MAXBITSPERINT)
		      uvsite.putData(pos + startpos, bitsize, (u32) dval); //absolute pos
		    else if(wordsize <= MAXBITSPERLONG)
		      uvsite.putDataLong(pos + startpos, bitsize, dval); //absolute pos
		    else
		      assert(0);
		  }
	      }
	    else if(sut->getUlamTypeEnum() == Class)
	      {
		u64 dpkval = 0;
		if(m_state.getPackedDefaultClass(suti, dpkval))
		  {
		    //could be a "packloadable" array of them
		    u32 len = sut->getTotalBitSize();
		    s32 arraysize = sut->getArraySize();
		    arraysize = (arraysize == NONARRAYSIZE ? 1 : arraysize);
		    u64 dpkarr = 0;
		    m_state.getDefaultAsPackedArray(len, bitsize, arraysize, 0u, dpkval, dpkarr);
		    uvsite.putDataLong(pos + startpos, len, dpkarr);
		  }
	      }
	    //else nothing to do?
	  } //countable
	it++;
      } //while
    return;
  } //initializeElementDefaultsForEval

  s32 SymbolTableOfVariables::findPosOfUlamTypeInTable(UTI utype, UTI& insidecuti)
  {
    s32 posfound = -1;
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	if(sym->isDataMember() && variableSymbolWithCountableSize(sym))
	  {
	    UTI suti = sym->getUlamTypeIdx();
	    if(UlamType::compare(suti, utype, m_state) == UTIC_SAME)
	      {
		posfound = ((SymbolVariable *) sym)->getPosOffset();
		insidecuti = suti;
		break;
	      }
	    else
	      {
		// check possible inheritance
		UTI superuti = m_state.isClassASubclass(suti);
		assert(superuti != Hzy);
		if((superuti != Nouti) && (UlamType::compare(superuti, utype, m_state) == UTIC_SAME))
		  {
		    posfound = ((SymbolVariable *) sym)->getPosOffset(); //starts at beginning
		    insidecuti = suti;
		    break;
		  }
	      }
	  }
	it++;
      }
    return posfound;
  } //findPosOfUlamTypeInTable

  //replaced with parse tree method to preserve order of declaration
  void SymbolTableOfVariables::genCodeForTableOfVariableDataMembers(File * fp, ULAMCLASSTYPE classtype)
  {
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	if(!sym->isTypedef() && sym->isDataMember()) //including model parameters
	  {
	    ((SymbolVariable *) sym)->generateCodedVariableDeclarations(fp, classtype);
	  }
	it++;
      }
  } //genCodeForTableOfVariableDataMembers (unused)

  void SymbolTableOfVariables::genModelParameterImmediateDefinitionsForTableOfVariableDataMembers(File *fp)
  {
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	if(sym->isModelParameter())
	  {
	    UTI suti = sym->getUlamTypeIdx();
	    m_state.getUlamTypeByIndex(suti)->genUlamTypeMangledImmediateModelParameterDefinitionForC(fp);
	  }
	it++;
      }
  } //genModelParameterImmediateDefinitionsForTableOfVariableDataMembers

  void SymbolTableOfVariables::genCodeBuiltInFunctionHasOverTableOfVariableDataMember(File * fp)
  {
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	if(sym->isDataMember() && variableSymbolWithCountableSize(sym))
	  {
	    UTI suti = sym->getUlamTypeIdx();
	    UlamType * sut = m_state.getUlamTypeByIndex(suti);
	    if(sut->getUlamClassType() == UC_QUARK)
	      {
		m_state.indentUlamCode(fp);
		fp->write("if(!strcmp(namearg,\"");
		fp->write(sut->getUlamTypeMangledName().c_str()); //mangled, including class args!
		fp->write("\")) return (");
		fp->write_decimal(((SymbolVariable *) sym)->getPosOffset());
		fp->write("); //pos offset\n");

		UTI superuti = m_state.isClassASubclass(suti);
		assert(superuti != Hzy);
		while(superuti != Nouti) //none
		  {
		    UlamType * superut = m_state.getUlamTypeByIndex(superuti);
		    m_state.indentUlamCode(fp);
		    fp->write("if(!strcmp(namearg,\"");
		    fp->write(superut->getUlamTypeMangledName().c_str()); //mangled, including class args!
		    fp->write("\")) return (");
		    fp->write_decimal(((SymbolVariable *) sym)->getPosOffset()); //same offset starts at 0
		    fp->write("); //inherited pos offset\n");

		    superuti = m_state.isClassASubclass(superuti); //any more
		  } //while
	      }
	  }
	it++;
      }
  } //genCodeBuiltInFunctionHasOverTableOfVariableDataMember

  void SymbolTableOfVariables::addClassMemberDescriptionsToMap(UTI classType, ClassMemberMap& classmembers)
  {
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	ClassMemberDesc * descptr = NULL;
	Symbol * sym = it->second;
	if(sym->isModelParameter() && ((SymbolParameterValue *)sym)->isReady())
	  {
	    descptr = new ParameterDesc((SymbolParameterValue *) sym, classType, m_state);
	    assert(descptr);
	  }
	else if(sym->isDataMember())
	  {
	    descptr = new DataMemberDesc((SymbolVariableDataMember *) sym, classType, m_state);
	    assert(descptr);
	  }
	else if(sym->isTypedef())
	  {
	    descptr = new TypedefDesc((SymbolTypedef *) sym, classType, m_state);
	    assert(descptr);
	  }
	else if(sym->isConstant() && ((SymbolConstantValue *)sym)->isReady())
	  {
	    descptr = new ConstantDesc((SymbolConstantValue *) sym, classType, m_state);
	    assert(descptr);
	  }
	else
	  {
	    //error not ready perhaps
	    assert(0); //(functions done separately)
	  }

	if(descptr)
	  {
	    //concat mangled class and parameter names to avoid duplicate keys into map
	    std::ostringstream fullMangledName;
	    fullMangledName << descptr->m_mangledClassName << "_" << descptr->m_mangledMemberName;
	    classmembers.insert(std::pair<std::string, ClassMemberDescHolder>(fullMangledName.str(), ClassMemberDescHolder(descptr)));
	  }
	it++;
      }
  } //addClassMemberDescriptionsToMap

  //storage for class members persists, so we give up preserving
  //order of declaration that the NodeVarDecl in the parseTree
  //provides, in order to distinguish between an instance's data
  //members on the STACK verses the classes' data members in
  //EVENTWINDOW.
  void SymbolTableOfVariables::printPostfixValuesForTableOfVariableDataMembers(File * fp, s32 slot, u32 startpos, ULAMCLASSTYPE classtype)
  {
    std::map<u32, Symbol *>::iterator it = m_idToSymbolPtr.begin();
    while(it != m_idToSymbolPtr.end())
      {
	Symbol * sym = it->second;
	if(sym->isTypedef() || sym->isConstant() || sym->isModelParameter() || sym->isDataMember())
	  {
	    sym->printPostfixValuesOfVariableDeclarations(fp, slot, startpos, classtype);
	  }
	it++;
      }
  } //printPostfixValuesForTableOfVariableDataMembers

  //PRIVATE HELPER METHODS:
  s32 SymbolTableOfVariables::calcVariableSymbolTypeSize(UTI arguti)
  {
    if(!m_state.okUTItoContinue(arguti))
      {
	assert(arguti != Nav);
	if(arguti == Nouti)
	  return UNKNOWNSIZE;
	//else continue if Hzy
      }

    s32 totbitsize = m_state.getBitSize(arguti);

    if(m_state.getUlamTypeByIndex(arguti)->getUlamClassType() == UC_NOTACLASS) //includes Atom type
      {
	return totbitsize; //arrays handled by caller, just bits here
      }

    //not a primitive (class), array
    if(m_state.getArraySize(arguti) > 0)
      {
	if(totbitsize >= 0)
	  {
	    return totbitsize;
	  }
	if(totbitsize == CYCLEFLAG) //was < 0
	  {
	    assert(0);
	    return CYCLEFLAG;
	  }
	if(totbitsize == EMPTYSYMBOLTABLE)
	  {
	    return 0; //empty, ok
	  }
	else
	  {
	    assert(totbitsize <= UNKNOWNSIZE || m_state.getArraySize(arguti) == UNKNOWNSIZE);

	    m_state.setBitSize(arguti, CYCLEFLAG); //before the recusive call..

	    //get base type, scalar type of class
	    SymbolClass * csym = NULL;
	    if(m_state.alreadyDefinedSymbolClass(arguti, csym))
	      {
		return calcVariableSymbolTypeSize(csym->getUlamTypeIdx()); //NEEDS CORRECTION
	      }
	  }
      }
    else if(m_state.isScalar(arguti)) //not primitive type (class), and not array (scalar)
      {
	if(totbitsize >= 0)
	  {
	    return totbitsize;
	  }

	if(totbitsize == CYCLEFLAG) //was < 0
	  {
	    return CYCLEFLAG; //error! cycle
	  }
	else if(totbitsize == EMPTYSYMBOLTABLE)
	  {
	    return 0; //empty, ok
	  }
	else
	  {
	    assert(totbitsize == UNKNOWNSIZE);
	    //get base type
	    SymbolClass * csym = NULL;
	    if(m_state.alreadyDefinedSymbolClass(arguti, csym))
	      {
		s32 csize;
		UTI cuti = csym->getUlamTypeIdx();
		if((csize = m_state.getBitSize(cuti)) >= 0)
		  {
		    return csize;
		  }
		else if(csize == CYCLEFLAG)  //was < 0
		  {
		    //error! cycle..replace with message
		    return csize;
		  }
		else if(csize == EMPTYSYMBOLTABLE)
		  {
		    return 0; //empty, ok
		  }
		else if(csym->isStub())
		  {
		    return UNKNOWNSIZE; //csize?
		  }
		else if(m_state.isAnonymousClass(cuti))
		  {
		    return UNKNOWNSIZE; //csize?
		  }
		else
		  {
		    //==0, redo variable total
		    NodeBlockClass * classblock = csym->getClassBlockNode();
		    assert(classblock);

		    //a class cannot contain a copy of itself!
		    if(classblock == m_state.getClassBlock())
		      {
			UTI suti = csym->getUlamTypeIdx();
			std::ostringstream msg;
			msg << "Class '" << m_state.getUlamTypeNameBriefByIndex(suti).c_str();
			msg << "' cannot contain a copy of itself"; //error/t3390
			MSG(csym->getTokPtr(), msg.str().c_str(), ERR);
			classblock->setNodeType(Nav);
			return UNKNOWNSIZE;
		      }

		    m_state.pushClassContext(csym->getUlamTypeIdx(), classblock, classblock, false, NULL);

		    if(csym->isQuarkUnion())
		      csize = classblock->getMaxBitSizeOfVariableSymbolsInTable();
		    else
		      csize = classblock->getBitSizesOfVariableSymbolsInTable(); //data members only

		    m_state.popClassContext(); //restore
		    return csize;
		  }
	      }
	  } //totbitsize == 0
      } //not primitive, not array
    return UNKNOWNSIZE; //was CYCLEFLAG
  } //calcVariableSymbolTypeSize (recursively)

  bool SymbolTableOfVariables::variableSymbolWithCountableSize(Symbol * sym)
  {
    // may be a zero-sized quark!!
    return (!sym->isTypedef() && !sym->isModelParameter() && !sym->isConstant());
  }

} //end MFM