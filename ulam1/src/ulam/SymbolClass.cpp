#include <sstream>
#include <string.h>
#include "CompilerState.h"
#include "SymbolClass.h"
#include "SymbolClassName.h"
#include "Resolver.h"

namespace MFM {

  static const char * CModeForHeaderFiles = "/**                                      -*- mode:C++ -*- */\n\n";

  // First line something like: "/* NodeProgram.h - Root Node of Programs for ULAM\n"
  static const char * CopyrightAndLicenseForUlamHeader =   "*\n"
    "**********************************************************************************\n"
    "* This code is generated by the ULAM programming language compilation system.\n"
    "*\n"
    "* The ULAM programming language compilation system is free software:\n"
    "* you can redistribute it and/or modify it under the terms of the GNU\n"
    "* General Public License as published by the Free Software\n"
    "* Foundation, either version 3 of the License, or (at your option)\n"
    "* any later version.\n"
    "*\n"
    "* The ULAM programming language compilation system is distributed in\n"
    "* the hope that it will be useful, but WITHOUT ANY WARRANTY; without\n"
    "* even the implied warranty of MERCHANTABILITY or FITNESS FOR A\n"
    "* PARTICULAR PURPOSE.  See the GNU General Public License for more\n"
    "* details.\n"
    "*\n"
    "* You should have received a copy of the GNU General Public License\n"
    "* along with the ULAM programming language compilation system\n"
    "* software.  If not, see <http://www.gnu.org/licenses/>.\n"
    "*\n"
    "* @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>\n"
    "*/\n\n";

  SymbolClass::SymbolClass(Token id, UTI utype, NodeBlockClass * classblock, SymbolClassNameTemplate * parent, CompilerState& state) : Symbol(id, utype, state), m_resolver(NULL), m_classBlock(classblock), m_parentTemplate(parent), m_quarkunion(false), m_stub(true), m_quarkDefaultValue(0), m_isreadyQuarkDefaultValue(false) /* default */, m_superClass(Nav) {}

  SymbolClass::SymbolClass(const SymbolClass& sref) : Symbol(sref), m_resolver(NULL), m_parentTemplate(sref.m_parentTemplate), m_quarkunion(sref.m_quarkunion), m_stub(sref.m_stub), m_quarkDefaultValue(sref.m_quarkDefaultValue), m_isreadyQuarkDefaultValue(false), m_superClass(m_state.mapIncompleteUTIForCurrentClassInstance(sref.m_superClass))
  {
    if(sref.m_classBlock)
      {
	m_classBlock = (NodeBlockClass * ) sref.m_classBlock->instantiate(); //note: wasn't correct uti during cloning
	// note: if superclass, the prevBlockPtr of m_classBlock hasn't been set yet!
      }
    else
      m_classBlock = NULL; //i.e. UC_UNSEEN

    if(sref.m_resolver)
      m_resolver = new Resolver(getUlamTypeIdx(), m_state); //not a clone, populated later
  }

  SymbolClass::~SymbolClass()
  {
    delete m_classBlock;
    m_classBlock = NULL;

    if(m_resolver)
      {
	delete m_resolver;
	m_resolver = NULL;
      }
  }

  Symbol * SymbolClass::clone()
  {
    assert(0);
    return new SymbolClass(*this);
  }

  void SymbolClass::setClassBlockNode(NodeBlockClass * node)
  {
    m_classBlock = node;
    if(m_classBlock)
      Symbol::setBlockNoOfST(node->getNodeNo());
    else
      Symbol::setBlockNoOfST(0);
  }

  NodeBlockClass * SymbolClass::getClassBlockNode()
  {
    return m_classBlock;
  }

  SymbolClassNameTemplate * SymbolClass::getParentClassTemplate()
  {
    return m_parentTemplate; //could be self
  }

  void SymbolClass::setParentClassTemplate(SymbolClassNameTemplate  * p)
  {
    assert(p);
    m_parentTemplate = p;
  }

  bool SymbolClass::isClass()
  {
    return true;
  }

  bool SymbolClass::isClassTemplate(UTI cuti)
  {
    return false;
  }

  void SymbolClass::setSuperClass(UTI superclass)
  {
    m_superClass = superclass;
  } //setSuperClass

  UTI SymbolClass::getSuperClass()
  {
    return m_superClass; //Nav is none, not a subclass.
  } //getSuperClass

  const std::string SymbolClass::getMangledPrefix()
  {
    return m_state.getUlamTypeByIndex(getUlamTypeIdx())->getUlamTypeUPrefix();
  }

  ULAMCLASSTYPE SymbolClass::getUlamClass()
  {
    return  m_state.getUlamTypeByIndex(getUlamTypeIdx())->getUlamClass();
  }

  void SymbolClass::setUlamClass(ULAMCLASSTYPE type)
  {
    ((UlamTypeClass * ) m_state.getUlamTypeByIndex(getUlamTypeIdx()))->setUlamClass(type);
  }

  void SymbolClass::setQuarkUnion()
  {
    m_quarkunion = true;
  }

  bool SymbolClass::isQuarkUnion()
  {
    return m_quarkunion;
  }

  bool SymbolClass::isStub()
  {
    return m_stub;
  }

  void SymbolClass::unsetStub()
  {
    m_stub = false;
  }

  bool SymbolClass::isCustomArray()
  {
    NodeBlockClass * classNode = getClassBlockNode(); //instance
    assert(classNode);
    return classNode->hasCustomArray(); //checks any super classes
  } //isCustomArray

  UTI SymbolClass::getCustomArrayType()
  {
    assert(isCustomArray());
    NodeBlockClass * classNode = getClassBlockNode(); //instance
    assert(classNode);
    return classNode->getCustomArrayTypeFromGetFunction(); //returns canonical type
  }

  u32 SymbolClass::getCustomArrayIndexTypeFor(Node * rnode, UTI& idxuti, bool& hasHazyArgs)
  {
    assert(isCustomArray());
    NodeBlockClass * classNode = getClassBlockNode(); //instance
    assert(classNode);
    //returns number of matching types; updates last two args.
    return classNode->getCustomArrayIndexTypeFromGetFunction(rnode, idxuti, hasHazyArgs);
  }

  bool SymbolClass::trySetBitsizeWithUTIValues(s32& totalbits)
  {
    NodeBlockClass * classNode = getClassBlockNode(); //instance
    bool aok = true;

    //of course they always aren't! but we know to keep looping..
    UTI suti = getUlamTypeIdx();
    if(! m_state.isComplete(suti))
      {
	std::ostringstream msg;
	msg << "Incomplete Class Type: "  << m_state.getUlamTypeNameByIndex(suti).c_str();
	msg << " (UTI" << suti << ") has 'unknown' sizes, fails sizing pre-test";
	msg << " while compiling class: ";
	msg  << m_state.getUlamTypeNameBriefByIndex(m_state.getCompileThisIdx()).c_str();
	MSG(Symbol::getTokPtr(), msg.str().c_str(),DEBUG);
	aok = false; //moved here;
      }

    if(isQuarkUnion())
      totalbits = classNode->getMaxBitSizeOfVariableSymbolsInTable(); //data members only
    else
      totalbits = classNode->getBitSizesOfVariableSymbolsInTable(); //data members only

    //check to avoid setting EMPTYSYMBOLTABLE instead of 0 for zero-sized classes
    if(totalbits == CYCLEFLAG)  // was < 0
      {
	std::ostringstream msg;
	msg << "cycle error!! " << m_state.getUlamTypeNameByIndex(getUlamTypeIdx()).c_str();
	MSG(Symbol::getTokPtr(), msg.str().c_str(),DEBUG);
	aok = false;
      }
    else if(totalbits == EMPTYSYMBOLTABLE)
      {
	totalbits = 0;
	aok = true;
      }
    else if(totalbits != UNKNOWNSIZE)
      aok = true; //not UNKNOWN
    return aok;
  } //trySetBitSize

  void SymbolClass::printBitSizeOfClass()
  {
    UTI suti = getUlamTypeIdx();
    u32 total = m_state.getTotalBitSize(suti);
    UlamType * sut = m_state.getUlamTypeByIndex(suti);
    ULAMCLASSTYPE classtype = sut->getUlamClass();

    std::ostringstream msg;
    msg << "[UTBUA] Total bits used/available by ";
    msg << (classtype == UC_ELEMENT ? "element " : "quark ");
    msg << m_state.getUlamTypeNameBriefByIndex(suti).c_str() << " : ";

    if(m_state.isComplete(suti))
      {
	s32 remaining = (classtype == UC_ELEMENT ? (MAXSTATEBITS - total) : (MAXBITSPERQUARK - total));
	msg << total << "/" << remaining;
      }
    else
      {
	total = UNKNOWNSIZE;
	s32 remaining = (classtype == UC_ELEMENT ? MAXSTATEBITS : MAXBITSPERQUARK);
	msg << "UNKNOWN" << "/" << remaining;
      }
    MSG(m_state.getFullLocationAsString(getLoc()).c_str(), msg.str().c_str(),INFO);
  } //printBitSizeOfClass

  bool SymbolClass::getDefaultQuark(u32& dqref)
  {
    assert(getUlamClass() == UC_QUARK);

    if(m_isreadyQuarkDefaultValue)
      {
	dqref = m_quarkDefaultValue;
	return true; //short-circuit, known
      }

    UTI suti = getUlamTypeIdx();
    UlamType * sut = m_state.getUlamTypeByIndex(suti);

    assert(sut->isComplete());

    if(sut->getBitSize() == 0)
      {
	m_isreadyQuarkDefaultValue = true;
	dqref = m_quarkDefaultValue = 0;
	return true; //short-circuit, no data members
      }

    dqref = 0; //init
    NodeBlockClass * classblock = getClassBlockNode();
    if(classblock->buildDefaultQuarkValue(dqref))
      {
	m_isreadyQuarkDefaultValue = true;
	m_quarkDefaultValue = dqref;
      }
    else
      m_isreadyQuarkDefaultValue = false;

    return m_isreadyQuarkDefaultValue;
  } //getDefaultQuark

  void SymbolClass::testThisClass(File * fp)
  {
    NodeBlockClass * classNode = getClassBlockNode();
    assert(classNode);
    m_state.pushClassContext(getUlamTypeIdx(), classNode, classNode, false, NULL);

    if(classNode->findTestFunctionNode())
      {
	// set up an atom in eventWindow; init m_currentObjPtr to point to it
	// set up STACK since func call not called
	m_state.setupCenterSiteForTesting();

	m_state.m_nodeEvalStack.addFrameSlots(1); //prolog, 1 for return
	s32 rtnValue = 0;
	EvalStatus evs = classNode->eval();
	if(evs != NORMAL)
	  {
	    rtnValue = -1; //error!
	  }
	else
	  {
	    UlamValue rtnUV = m_state.m_nodeEvalStack.popArg();
	    rtnValue = rtnUV.getImmediateData(32);
	  }

	//#define CURIOUS_T3146
#ifdef CURIOUS_T3146
	//curious..
	{
	  UlamValue objUV = m_state.m_eventWindow.loadAtomFromSite(c0.convertCoordToIndex());
	  u32 data = objUV.getData(25,32); //Int f.m_i (t3146)
	  std::ostringstream msg;
	  msg << "Output for m_i = <" << data << "> (expecting 4 for t3146)";
	  MSG(Symbol::getTokPtr(),msg.str().c_str() , INFO);
	}
#endif
	m_state.m_nodeEvalStack.returnFrame(); //epilog

	fp->write("Exit status: " ); //in compared answer
	fp->write_decimal(rtnValue);
	fp->write("\n");
      } //test eval
    m_state.popClassContext(); //missing?
  } //testThisClass

  void SymbolClass::linkConstantExpressionForPendingArg(NodeConstantDef * constNode)
  {
    if(!m_resolver)
      m_resolver = new Resolver(getUlamTypeIdx(), m_state);
    assert(m_stub); //stubs only have pending args
    m_resolver->linkConstantExpressionForPendingArg(constNode);
  }

  bool SymbolClass::pendingClassArgumentsForClassInstance()
  {
    if(!m_resolver) //stubs only!
      return false; //ok, none pending
    return m_resolver->pendingClassArgumentsForClassInstance();
  }

  void SymbolClass::cloneResolverForStubClassInstance(const SymbolClass * csym, UTI context)
  {
    assert(csym); //from
    if(!m_resolver)
      m_resolver = new Resolver(getUlamTypeIdx(), m_state);
    m_resolver->clonePendingClassArgumentsForStubClassInstance(*(csym->m_resolver), context, this);
  } //cloneResolverForStubClassInstance

  void SymbolClass::cloneResolverUTImap(SymbolClass * csym)
  {
    assert(csym); //to
    assert(m_resolver);
    m_resolver->cloneUTImap(csym);
  } //cloneResolverUTImap

  UTI SymbolClass::getContextForPendingArgs()
  {
    assert(m_resolver);
    return m_resolver->getContextForPendingArgs();
  } //getContextForPendingArgs

  bool SymbolClass::statusNonreadyClassArguments()
  {
    if(!m_resolver) //stubs only!
      return true;
    return m_resolver->statusNonreadyClassArguments();
  }

  bool SymbolClass::constantFoldNonreadyClassArguments()
  {
    if(!m_resolver)
      return true; //nothing to do
    return m_resolver->constantFoldNonreadyClassArgs();
  }

  bool SymbolClass::mapUTItoUTI(UTI auti, UTI mappedUTI)
  {
    if(!m_resolver)
      m_resolver = new Resolver(getUlamTypeIdx(), m_state);

    return m_resolver->mapUTItoUTI(auti, mappedUTI);
  } //mapUTItoUTI

  bool SymbolClass::hasMappedUTI(UTI auti, UTI& mappedUTI)
  {
    if(!m_resolver)
      return false; //not found

    return m_resolver->findMappedUTI(auti, mappedUTI);
  } //hasMappedUTI

  bool SymbolClass::findNodeNoInResolver(NNO n, Node *& foundNode)
  {
    if(!m_resolver)
      return false; //not found

    return m_resolver->findNodeNo(n, foundNode);
  } //findNodeNoInResolver

  /////////////////////////////////////////////////////////////////////////////////
  // from NodeProgram
  /////////////////////////////////////////////////////////////////////////////////

  void SymbolClass::generateCode(FileManager * fm)
  {
    //class context already pushed..
    assert(m_classBlock);

    ULAMCLASSTYPE classtype = m_state.getUlamTypeByIndex(getUlamTypeIdx())->getUlamClass();

    // setup for codeGen
    m_state.m_currentSelfSymbolForCodeGen = this;
    m_state.m_currentObjSymbolsForCodeGen.clear();

    m_state.setupCenterSiteForTesting(); //temporary!!!

    // mangled types and forward class declarations
    genMangledTypesHeaderFile(fm);

    // this class header
    {
      File * fp = fm->open(m_state.getFileNameForThisClassHeader(WSUBDIR).c_str(), WRITE);
      assert(fp);

      generateHeaderPreamble(fp);
      genAllCapsIfndefForHeaderFile(fp);
      generateHeaderIncludes(fp);

      UlamValue uvpass;
      m_classBlock->genCode(fp, uvpass); //compileThisId only, class block

      // include this .tcc
      m_state.indent(fp);
      fp->write("#include \"");
      fp->write(m_state.getFileNameForThisClassBody().c_str());
      fp->write("\"\n\n");

      // include native .tcc for this class if any declared
      if(m_classBlock->countNativeFuncDecls() > 0)
	{
	  m_state.indent(fp);
	  fp->write("#include \"");
	  fp->write(m_state.getFileNameForThisClassBodyNative().c_str());
	  fp->write("\"\n\n");
	}
      genAllCapsEndifForHeaderFile(fp);

      delete fp; //close
    }

    // this class body
    {
      File * fp = fm->open(m_state.getFileNameForThisClassBody(WSUBDIR).c_str(), WRITE);
      assert(fp);

      m_state.m_currentIndentLevel = 0;
      fp->write(CModeForHeaderFiles); //needed for .tcc files too

      UlamValue uvpass;
      m_classBlock->genCodeBody(fp, uvpass); //compileThisId only, MFM namespace

      delete fp; //close
    }

    // "stub" .cpp includes .h (unlike the .tcc body)
    {
      File * fp = fm->open(m_state.getFileNameForThisClassCPP(WSUBDIR).c_str(), WRITE);
      assert(fp);

      m_state.m_currentIndentLevel = 0;

      // include .h in the .cpp
      m_state.indent(fp);
      fp->write("#include \"");
      fp->write(m_state.getFileNameForThisClassHeader().c_str());
      fp->write("\"\n");
      fp->write("\n");

      m_state.indent(fp);
      fp->write("namespace MFM{\n\n");
      fp->write("} //MFM\n\n");

      delete fp; //close
    }

    //separate main.cpp for elements only; that have the test method.
    if(classtype == UC_ELEMENT)
      {
	if(m_classBlock->findTestFunctionNode())
	  generateMain(fm);
      }
  } //generateCode

  void SymbolClass::generateAsOtherInclude(File * fp)
  {
    UTI suti = getUlamTypeIdx();
    if(suti != m_state.getCompileThisIdx() && m_state.getUlamTypeByIndex(suti)->isComplete())
      {
	m_state.indent(fp);
	fp->write("#include \"");
	fp->write(m_state.getFileNameForAClassHeader(suti).c_str());
	fp->write("\"\n");
      }
  } //generateAsOtherInclude

  void SymbolClass::generateAsOtherForwardDef(File * fp)
  {
    UTI suti = getUlamTypeIdx();
    if(suti != m_state.getCompileThisIdx() && m_state.getUlamTypeByIndex(suti)->isComplete())
      {
	UlamType * sut = m_state.getUlamTypeByIndex(suti);
	ULAMCLASSTYPE sclasstype = sut->getUlamClass();

	m_state.indent(fp);
	fp->write("namespace MFM { template ");
	if(sclasstype == UC_QUARK)
	  fp->write("<class EC, u32 POS> ");
	else if(sclasstype == UC_ELEMENT)
	  fp->write("<class EC> ");
	else
	  assert(0);

	fp->write("struct ");
	fp->write(sut->getUlamTypeMangledName().c_str());
	fp->write("; }  //FORWARD\n");
      }
  } //generateAsOtherForwardDef

  void SymbolClass::generateTestInstance(File * fp, bool runtest)
  {
    std::ostringstream runThisTest;
    UTI suti = getUlamTypeIdx();
    UlamType * sut = m_state.getUlamTypeByIndex(suti);
    if(!sut->isComplete()) return;

    // output for each element before testing; a test may include
    // one or more of them!
    if(!runtest)
      {
	fp->write("\n");
	m_state.indent(fp);
	fp->write("{\n");

	m_state.m_currentIndentLevel++;

	m_state.indent(fp);
	fp->write("Element<EC> & elt = ");
	fp->write(sut->getUlamTypeMangledName().c_str());
	fp->write("<EC>::THE_INSTANCE;\n");

	m_state.indent(fp);
	fp->write("elt.AllocateType(etnm); //Force element type allocation now\n");
	m_state.indent(fp);
	fp->write("tile.RegisterElement(elt);\n");

	m_state.m_currentIndentLevel--;

	m_state.indent(fp);
	fp->write("}\n");
      }
    else
      {
	if(getId() == m_state.getCompileThisId())
	  {
	    fp->write("\n");
	    m_state.indent(fp);
	    fp->write("atom = "); //OurAtomAll
	    fp->write(sut->getUlamTypeMangledName().c_str());
	    fp->write("<EC>::THE_INSTANCE.GetDefaultAtom();\n");
	    m_state.indent(fp);
	    fp->write("tile.PlaceAtom(atom, center);\n");
	    m_state.indent(fp);
	    fp->write("rtn = "); //MFM::Ui_Ut_102323Int
	    fp->write(sut->getUlamTypeMangledName().c_str());
	    fp->write("<EC>::THE_INSTANCE.Uf_4test(uc, atom);\n");

	    m_state.indent(fp);
	    fp->write("//std::cerr << rtn.read() << std::endl;\n"); //useful to return result of test?
	    m_state.indent(fp);
	    fp->write("//return rtn.read();\n"); //was useful to return result of test
	  }
      }
  } //generateTestInstance

  void SymbolClass::generateHeaderPreamble(File * fp)
  {
    m_state.m_currentIndentLevel = 0;
    fp->write(CModeForHeaderFiles);
    fp->write("/***********************         DO NOT EDIT        ******************************\n");
    fp->write("*\n");
    fp->write("* ");
    fp->write(m_state.m_pool.getDataAsString(m_state.getCompileThisId()).c_str());
    fp->write(".h - ");
    ULAMCLASSTYPE classtype = m_state.getUlamTypeByIndex(getUlamTypeIdx())->getUlamClass();
    if(classtype == UC_ELEMENT)
      fp->write("Element");
    else if(classtype == UC_QUARK)
      fp->write("Quark");
    else
      assert(0);

    fp->write(" header for ULAM\n");

    fp->write(CopyrightAndLicenseForUlamHeader);
  } //generateHeaderPreamble

  void SymbolClass::genAllCapsIfndefForHeaderFile(File * fp)
  {
    UlamType * cut = m_state.getUlamTypeByIndex(getUlamTypeIdx());
    m_state.indent(fp);
    fp->write("#ifndef ");
    fp->write(Node::allCAPS(cut->getUlamTypeMangledName().c_str()).c_str());
    fp->write("_H\n");

    m_state.indent(fp);
    fp->write("#define ");
    fp->write(Node::allCAPS(cut->getUlamTypeMangledName().c_str()).c_str());
    fp->write("_H\n\n");
  } //genAllCapsIfndefForHeaderFile

  void SymbolClass::genAllCapsEndifForHeaderFile(File * fp)
  {
    UlamType * cut = m_state.getUlamTypeByIndex(getUlamTypeIdx());
    fp->write("#endif //");
    fp->write(Node::allCAPS(cut->getUlamTypeMangledName().c_str()).c_str());
    fp->write("_H\n");
  }

  void SymbolClass::generateHeaderIncludes(File * fp)
  {
    m_state.indent(fp);
    fp->write("#include \"UlamDefs.h\"\n\n");

    //using the _Types.h file
    m_state.indent(fp);
    fp->write("#include \"");
    fp->write(m_state.getFileNameForThisTypesHeader().c_str());
    fp->write("\"\n");
    fp->write("\n");

    //generate includes for all the other classes that have appeared
    m_state.m_programDefST.generateForwardDefsForTableOfClasses(fp);
  } //generateHeaderIncludes

  // create structs with BV, as storage, and typedef
  // for primitive types; useful as args and local variables;
  // important for overloading functions
  void SymbolClass::genMangledTypesHeaderFile(FileManager * fm)
  {
    File * fp = fm->open(m_state.getFileNameForThisTypesHeader(WSUBDIR).c_str(), WRITE);
    assert(fp);

    m_state.m_currentIndentLevel = 0;
    fp->write(CModeForHeaderFiles);

    m_state.indent(fp);
    //use -I ../../../include in g++ command
    fp->write("//#include \"itype.h\"\n");
    fp->write("//#include \"BitVector.h\"\n");
    fp->write("//#include \"BitField.h\"\n");
    fp->write("\n");

    m_state.indent(fp);
    fp->write("#include \"UlamDefs.h\"\n\n");

    // do primitive types before classes so that immediate
    // Quarks/Elements can use them (e.g. immediate index for aref)
    std::map<UlamKeyTypeSignature, UlamType *, less_than_key>::iterator it = m_state.m_definedUlamTypes.begin();
    while(it != m_state.m_definedUlamTypes.end())
      {
	UlamType * ut = it->second;
	//e.g. skip constants, include atom
	if(ut->needsImmediateType() && ut->getUlamClass() == UC_NOTACLASS)
	  ut->genUlamTypeMangledDefinitionForC(fp);
	it++;
      }

    //same except now for user defined Class types
    it = m_state.m_definedUlamTypes.begin();
    while(it != m_state.m_definedUlamTypes.end())
      {
	UlamType * ut = it->second;
	ULAMCLASSTYPE classtype = ut->getUlamClass();
	if(ut->needsImmediateType() && classtype != UC_NOTACLASS)
	  {
	    ut->genUlamTypeMangledDefinitionForC(fp);
	    if(classtype == UC_QUARK)
	      ut->genUlamTypeMangledAutoDefinitionForC(fp);
	  }
	it++;
      }

    // define any model parameter immediate types needed for this class
    NodeBlockClass * classblock = getClassBlockNode();
    assert(classblock);
    classblock->genModelParameterImmediateDefinitions(fp);

    delete fp; //close
  } //genMangledTypesHeaderFile

  // append main to .cpp for debug useage
  // outside the MFM namespace !!!
  void SymbolClass::generateMain(FileManager * fm)
  {
    File * fp = fm->open(m_state.getFileNameForThisClassMain(WSUBDIR).c_str(), WRITE);
    assert(fp);

    m_state.m_currentIndentLevel = 0;

    m_state.indent(fp);
    fp->write("#include <stdio.h>\n");
    m_state.indent(fp);
    fp->write("#include <iostream>\n"); //to cout/cerr rtn
    m_state.indent(fp);
    fp->write("#include \"itype.h\"\n");
    m_state.indent(fp);
    fp->write("#include \"P3Atom.h\"\n");
    m_state.indent(fp);
    fp->write("#include \"SizedTile.h\"\n");
    fp->write("\n");

    m_state.indent(fp);
    fp->write("#include \"UlamDefs.h\"\n\n");

    m_state.indent(fp);
    fp->write("#include \"");
    fp->write(m_state.getFileNameForThisClassHeader().c_str());
    fp->write("\"\n");

    m_state.m_programDefST.generateIncludesForTableOfClasses(fp); //the other classes

    //namespace MFM
    fp->write("\n");
    m_state.indent(fp);
    fp->write("namespace MFM\n");
    m_state.indent(fp);
    fp->write("{\n");

    m_state.m_currentIndentLevel++;
    //For all models
    m_state.indent(fp);
    fp->write("typedef P3Atom OurAtomAll;\n");
    m_state.indent(fp);
    fp->write("typedef Site<P3AtomConfig> OurSiteAll;\n");
    m_state.indent(fp);
    fp->write("typedef EventConfig<OurSiteAll,4> OurEventConfigAll;\n");
    m_state.indent(fp);
    fp->write("typedef SizedTile<OurEventConfigAll, 20> OurTestTile;\n");
    m_state.indent(fp);
    fp->write("typedef ElementTypeNumberMap<OurEventConfigAll> OurEventTypeNumberMapAll;\n");
    fp->write("\n");

    m_state.indent(fp);
    fp->write("typedef ElementTable<OurEventConfigAll> TestElementTable;\n");
    m_state.indent(fp);
    fp->write("typedef EventWindow<OurEventConfigAll> TestEventWindow;\n");
    fp->write("\n");

    m_state.indent(fp);
    fp->write("typedef UlamContext<OurEventConfigAll> OurUlamContext;\n");
    fp->write("\n");

    m_state.indent(fp);
    fp->write("template<class EC>\n");
    m_state.indent(fp);
    fp->write("int TestSingleElement()\n");
    m_state.indent(fp);
    fp->write("{\n");

    m_state.m_currentIndentLevel++;

    m_state.indent(fp);
    fp->write("OurEventTypeNumberMapAll etnm;\n");
    m_state.indent(fp);
    fp->write("OurTestTile tile;\n");
    m_state.indent(fp);
    fp->write("OurUlamContext uc;\n");
    m_state.indent(fp);
    fp->write("const u32 TILE_SIDE = tile.TILE_SIDE;\n");
    m_state.indent(fp);
    fp->write("SPoint center(TILE_SIDE/2, TILE_SIDE/2);  // Hitting no caches, for starters;\n");
    m_state.indent(fp);
    fp->write("uc.SetTile(tile);\n");

    m_state.m_programDefST.generateTestInstancesForTableOfClasses(fp);

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("}\n");

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("} //MFM\n");

    //MAIN STARTS HERE !!!
    fp->write("\n");
    m_state.indent(fp);
    fp->write("int main(int argc, const char** argv)\n");

    m_state.indent(fp);
    fp->write("{\n");

    m_state.m_currentIndentLevel++;

    m_state.indent(fp);
    fp->write("return ");
    fp->write("MFM::TestSingleElement<MFM::OurEventConfigAll>();\n");

    m_state.m_currentIndentLevel--;
    m_state.indent(fp);
    fp->write("} //main \n");

    delete fp; //close
  } //generateMain

  std::string SymbolClass::firstletterTolowercase(const std::string s) //static method
  {
    std::ostringstream up;
    assert(!s.empty());
    std::string c(1,(s[0] >= 'A' && s[0] <= 'Z') ? s[0]-('A'-'a') : s[0]);
    up << c << s.substr(1,s.length());
    return up.str();
  } //firstletterTolowercase

  void SymbolClass::addTargetDescriptionMapEntry(TargetMap& classtargets, u32 scid)
  {
    UlamType * cut = m_state.getUlamTypeByIndex(getUlamTypeIdx());
    std::string className = cut->getUlamTypeNameOnly();
    std::string mangledName = cut->getUlamTypeMangledName();
    struct TargetDesc desc;

    NodeBlockClass * classNode = getClassBlockNode();
    assert(classNode);
    NodeBlockFunctionDefinition * func = classNode->findTestFunctionNode();
    desc.m_hasTest = (func != NULL);

    ULAMCLASSTYPE classtype = cut->getUlamClass();
    desc.m_isQuark = (classtype == UC_QUARK);

    desc.m_bitsize = cut->getTotalBitSize();
    desc.m_loc = classNode->getNodeLocation();
    desc.m_className = className;

    if(scid > 0)
      {
	std::ostringstream sc;
	sc << "/**";
	sc << m_state.m_pool.getDataAsString(scid).c_str();
	sc << "*/";
	desc.m_structuredComment = sc.str();
      }
    else
      desc.m_structuredComment = "NONE";

    classtargets.insert(std::pair<std::string, struct TargetDesc>(mangledName, desc));
  } //addTargetDesciptionMapEntry

  void SymbolClass::addClassMemberDescriptionsMapEntry(ClassMemberMap& classmembers)
  {
    NodeBlockClass * classNode = getClassBlockNode();
    assert(classNode);
    classNode->addClassMemberDescriptionsToInfoMap(classmembers);
  } //addClassMemberDesciptionsMapEntry

} //end MFM
