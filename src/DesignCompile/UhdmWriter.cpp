/*
 Copyright 2019 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   UhdmWriter.cpp
 * Author: alain
 *
 * Created on January 17, 2020, 9:13 PM
 */

#include <Surelog/CommandLine/CommandLineParser.h>
#include <Surelog/Design/DesignElement.h>
#include <Surelog/Design/FileContent.h>
#include <Surelog/Design/ModPort.h>
#include <Surelog/Design/ModuleDefinition.h>
#include <Surelog/Design/ModuleInstance.h>
#include <Surelog/Design/Netlist.h>
#include <Surelog/Design/Parameter.h>
#include <Surelog/Design/Signal.h>
#include <Surelog/DesignCompile/CompileDesign.h>
#include <Surelog/DesignCompile/UhdmChecker.h>
#include <Surelog/DesignCompile/UhdmWriter.h>
#include <Surelog/Package/Package.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/SourceCompile/SymbolTable.h>
#include <Surelog/Testbench/ClassDefinition.h>
#include <Surelog/Testbench/Program.h>
#include <Surelog/Testbench/TypeDef.h>
#include <Surelog/Testbench/Variable.h>
#include <Surelog/Utils/StringUtils.h>

#include <cstring>

// UHDM
#include <uhdm/ElaboratorListener.h>
#include <uhdm/Serializer.h>
#include <uhdm/SynthSubset.h>
#include <uhdm/UhdmLint.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_listener.h>
#include <uhdm/vpi_visitor.h>

namespace SURELOG {

using namespace UHDM;  // NOLINT (we're using a whole bunch of these)

unsigned int getBuiltinType(VObjectType type) {
  switch (type) {
    case VObjectType::slNInpGate_And:
      return vpiAndPrim;
    case VObjectType::slNInpGate_Or:
      return vpiOrPrim;
    case VObjectType::slNInpGate_Nor:
      return vpiNorPrim;
    case VObjectType::slNInpGate_Nand:
      return vpiNandPrim;
    case VObjectType::slNInpGate_Xor:
      return vpiXorPrim;
    case VObjectType::slNInpGate_Xnor:
      return vpiXnorPrim;
    case VObjectType::slNOutGate_Buf:
      return vpiBufPrim;
    case VObjectType::slNOutGate_Not:
      return vpiNotPrim;
    case VObjectType::slPassEnSwitch_Tranif0:
      return vpiTranif0Prim;
    case VObjectType::slPassEnSwitch_Tranif1:
      return vpiTranif1Prim;
    case VObjectType::slPassEnSwitch_RTranif1:
      return vpiRtranif1Prim;
    case VObjectType::slPassEnSwitch_RTranif0:
      return vpiRtranif0Prim;
    case VObjectType::slPassSwitch_Tran:
      return vpiTranPrim;
    case VObjectType::slPassSwitch_RTran:
      return vpiRtranPrim;
    case VObjectType::slCmosSwitchType_Cmos:
      return vpiCmosPrim;
    case VObjectType::slCmosSwitchType_RCmos:
      return vpiRcmosPrim;
    case VObjectType::slEnableGateType_Bufif0:
      return vpiBufif0Prim;
    case VObjectType::slEnableGateType_Bufif1:
      return vpiBufif1Prim;
    case VObjectType::slEnableGateType_Notif0:
      return vpiNotif0Prim;
    case VObjectType::slEnableGateType_Notif1:
      return vpiNotif1Prim;
    case VObjectType::slMosSwitchType_NMos:
      return vpiNmosPrim;
    case VObjectType::slMosSwitchType_PMos:
      return vpiPmosPrim;
    case VObjectType::slMosSwitchType_RNMos:
      return vpiRnmosPrim;
    case VObjectType::slMosSwitchType_RPMos:
      return vpiRpmosPrim;
    case VObjectType::slPullup:
      return vpiPullupPrim;
    case VObjectType::slPulldown:
      return vpiPulldownPrim;
    default:
      return 0;
  }
}

UhdmWriter::UhdmWriter(CompileDesign* compiler, Design* design)
    : m_compileDesign(compiler), m_design(design) {
  m_helper.seterrorReporting(
      m_compileDesign->getCompiler()->getErrorContainer(),
      m_compileDesign->getCompiler()->getSymbolTable());
}

unsigned int UhdmWriter::getStrengthType(VObjectType type) {
  switch (type) {
    case VObjectType::slSupply0:
      return vpiSupply0;
    case VObjectType::slSupply1:
      return vpiSupply1;
    case VObjectType::slStrong0:
      return vpiStrongDrive;
    case VObjectType::slStrong1:
      return vpiStrongDrive;
    case VObjectType::slPull0:
      return vpiPullDrive;
    case VObjectType::slPull1:
      return vpiPullDrive;
    case VObjectType::slWeak0:
      return vpiWeakDrive;
    case VObjectType::slWeak1:
      return vpiWeakDrive;
    case VObjectType::slHighZ0:
      return vpiHighZ;
    case VObjectType::slHighZ1:
      return vpiHighZ;
    default:
      return 0;
  }
}

unsigned int UhdmWriter::getVpiOpType(VObjectType type) {
  switch (type) {
    case VObjectType::slBinOp_Plus:
      return vpiAddOp;
    case VObjectType::slBinOp_Minus:
      return vpiSubOp;
    case VObjectType::slBinOp_Mult:
      return vpiMultOp;
    case VObjectType::slBinOp_MultMult:
      return vpiPowerOp;
    case VObjectType::slBinOp_Div:
      return vpiDivOp;
    case VObjectType::slBinOp_Great:
      return vpiGtOp;
    case VObjectType::slBinOp_GreatEqual:
      return vpiGeOp;
    case VObjectType::slBinOp_Less:
      return vpiLtOp;
    case VObjectType::slBinOp_Imply:
      return vpiImplyOp;
    case VObjectType::slBinOp_Equivalence:
      return vpiEqOp;
    case VObjectType::slBinOp_LessEqual:
      return vpiLeOp;
    case VObjectType::slBinOp_Equiv:
      return vpiEqOp;
    case VObjectType::slBinOp_Not:
    case VObjectType::slNOT:
      return vpiNeqOp;
    case VObjectType::slBinOp_Percent:
      return vpiModOp;
    case VObjectType::slBinOp_LogicAnd:
      return vpiLogAndOp;
    case VObjectType::slBinOp_LogicOr:
      return vpiLogOrOp;
    case VObjectType::slBinOp_BitwAnd:
      return vpiBitAndOp;
    case VObjectType::slBinOp_BitwOr:
      return vpiBitOrOp;
    case VObjectType::slBinOp_BitwXor:
      return vpiBitXorOp;
    case VObjectType::slBinOp_ReductXnor1:
    case VObjectType::slBinOp_ReductXnor2:
    case VObjectType::slBinModOp_ReductXnor1:
    case VObjectType::slBinModOp_ReductXnor2:
      return vpiBitXNorOp;
    case VObjectType::slBinOp_ReductNand:
      return vpiUnaryNandOp;
    case VObjectType::slBinOp_ReductNor:
      return vpiUnaryNorOp;
    case VObjectType::slUnary_Plus:
      return vpiPlusOp;
    case VObjectType::slUnary_Minus:
      return vpiMinusOp;
    case VObjectType::slUnary_Not:
      return vpiNotOp;
    case VObjectType::slUnary_Tilda:
      return vpiBitNegOp;
    case VObjectType::slUnary_BitwAnd:
      return vpiUnaryAndOp;
    case VObjectType::slUnary_BitwOr:
      return vpiUnaryOrOp;
    case VObjectType::slUnary_BitwXor:
      return vpiUnaryXorOp;
    case VObjectType::slUnary_ReductNand:
      return vpiUnaryNandOp;
    case VObjectType::slUnary_ReductNor:
      return vpiUnaryNorOp;
    case VObjectType::slUnary_ReductXnor1:
    case VObjectType::slUnary_ReductXnor2:
      return vpiUnaryXNorOp;
    case VObjectType::slBinOp_ShiftLeft:
      return vpiLShiftOp;
    case VObjectType::slBinOp_ShiftRight:
      return vpiRShiftOp;
    case VObjectType::slBinOp_ArithShiftLeft:
      return vpiArithLShiftOp;
    case VObjectType::slBinOp_ArithShiftRight:
      return vpiArithRShiftOp;
    case VObjectType::slIncDec_PlusPlus:
      return vpiPostIncOp;
    case VObjectType::slIncDec_MinusMinus:
      return vpiPostDecOp;
    case VObjectType::slConditional_operator:
    case VObjectType::slQmark:
      return vpiConditionOp;
    case VObjectType::slInsideOp:
    case VObjectType::slOpen_range_list:
      return vpiInsideOp;
    case VObjectType::slBinOp_FourStateLogicEqual:
      return vpiCaseEqOp;
    case VObjectType::slBinOp_FourStateLogicNotEqual:
      return vpiCaseNeqOp;
    case VObjectType::slAssignOp_Assign:
      return vpiAssignmentOp;
    case VObjectType::slAssignOp_Add:
      return vpiAddOp;
    case VObjectType::slAssignOp_Sub:
      return vpiSubOp;
    case VObjectType::slAssignOp_Mult:
      return vpiMultOp;
    case VObjectType::slAssignOp_Div:
      return vpiDivOp;
    case VObjectType::slAssignOp_Modulo:
      return vpiModOp;
    case VObjectType::slAssignOp_BitwAnd:
      return vpiBitAndOp;
    case VObjectType::slAssignOp_BitwOr:
      return vpiBitOrOp;
    case VObjectType::slAssignOp_BitwXor:
      return vpiBitXorOp;
    case VObjectType::slAssignOp_BitwLeftShift:
      return vpiLShiftOp;
    case VObjectType::slAssignOp_BitwRightShift:
      return vpiRShiftOp;
    case VObjectType::slAssignOp_ArithShiftLeft:
      return vpiArithLShiftOp;
    case VObjectType::slAssignOp_ArithShiftRight:
      return vpiArithRShiftOp;
    case VObjectType::slMatches:
      return vpiMatchOp;
    case VObjectType::slBinOp_WildcardEqual:
    case VObjectType::slBinOp_WildEqual:
      return vpiWildEqOp;
    case VObjectType::slBinOp_WildcardNotEqual:
    case VObjectType::slBinOp_WildNotEqual:
      return vpiWildNeqOp;
    case VObjectType::slIff:
      return vpiIffOp;
    case VObjectType::slNON_OVERLAP_IMPLY:
      return vpiNonOverlapImplyOp;
    case VObjectType::slOVERLAP_IMPLY:
      return vpiOverlapImplyOp;
    case VObjectType::slOVERLAPPED:
      return vpiOverlapFollowedByOp;
    case VObjectType::slNONOVERLAPPED:
      return vpiNonOverlapFollowedByOp;
    case VObjectType::slUNTIL:
      return vpiUntilOp;
    case VObjectType::slS_UNTIL:
      return vpiUntilOp;
    case VObjectType::slUNTIL_WITH:
      return vpiUntilWithOp;
    case VObjectType::slS_UNTIL_WITH:
      return vpiUntilWithOp;
    case VObjectType::slIMPLIES:
      return vpiImpliesOp;
    case VObjectType::slIFF:
      return vpiIffOp;
    case VObjectType::slCycle_delay_range:
      return vpiCycleDelayOp;
    case VObjectType::slConsecutive_repetition:
      return vpiConsecutiveRepeatOp;
    case VObjectType::slNon_consecutive_repetition:
      return vpiRepeatOp;
    case VObjectType::slGoto_repetition:
      return vpiGotoRepeatOp;
    case VObjectType::slThroughout:
      return vpiThroughoutOp;
    case VObjectType::slWithin:
      return vpiWithinOp;
    case VObjectType::slIntersect:
      return vpiIntersectOp;
    case VObjectType::slFirstMatch:
      return vpiFirstMatchOp;
    default:
      return 0;
  }
}

bool isMultidimensional(const UHDM::typespec* ts) {
  bool isMultiDimension = false;
  if (ts) {
    if (ts->UhdmType() == uhdmlogic_typespec) {
      logic_typespec* lts = (logic_typespec*)ts;
      if (lts->Ranges() && lts->Ranges()->size() > 1) isMultiDimension = true;
    } else if (ts->UhdmType() == uhdmarray_typespec) {
      array_typespec* lts = (array_typespec*)ts;
      if (lts->Ranges() && lts->Ranges()->size() > 1) isMultiDimension = true;
    } else if (ts->UhdmType() == uhdmpacked_array_typespec) {
      packed_array_typespec* lts = (packed_array_typespec*)ts;
      if (lts->Ranges() && lts->Ranges()->size() > 1) isMultiDimension = true;
    } else if (ts->UhdmType() == uhdmbit_typespec) {
      bit_typespec* lts = (bit_typespec*)ts;
      if (lts->Ranges() && lts->Ranges()->size() > 1) isMultiDimension = true;
    }
  }
  return isMultiDimension;
}

bool writeElabParameters(Serializer& s, ModuleInstance* instance,
                         UHDM::scope* m, ExprBuilder& exprBuilder) {
  Netlist* netlist = instance->getNetlist();
  DesignComponent* mod = instance->getDefinition();

  std::map<std::string, any*> paramSet;
  if (netlist->param_assigns()) {
    VectorOfany* params = m->Parameters();
    if (params == nullptr) params = s.MakeAnyVec();
    m->Parameters(params);
    m->Param_assigns(netlist->param_assigns());
  }

  if (mod) {
    VectorOfany* params = m->Parameters();
    if (params == nullptr) params = s.MakeAnyVec();
    m->Parameters(params);
    VectorOfany* orig_params = mod->getParameters();
    if (orig_params) {
      for (auto orig : *orig_params) {
        const std::string& name = orig->VpiName();
        bool pushed = false;
        // Specifc handling of type parameters
        if (orig->UhdmType() == uhdmtype_parameter) {
          for (auto p : instance->getTypeParams()) {
            if (p->getName() == orig->VpiName()) {
              any* uparam = p->getUhdmParam();
              if (uparam) {
                uparam->VpiParent(m);
                for (VectorOfany::iterator itr = params->begin();
                     itr != params->end(); itr++) {
                  if ((*itr)->VpiName() == p->getName()) {
                    params->erase(itr);
                    break;
                  }
                }
                params->push_back(uparam);
                pushed = true;
              }
              break;
            }
          }
          if (!pushed) {
            // These point to the sole copy (unelaborated)
            for (VectorOfany::iterator itr = params->begin();
                 itr != params->end(); itr++) {
              if ((*itr)->VpiName() == orig->VpiName()) {
                params->erase(itr);
                break;
              }
            }
            params->push_back(orig);
          }
        } else {
          // Regular param
          ElaboratorListener listener(&s);
          any* pclone = UHDM::clone_tree(orig, s, &listener);
          pclone->VpiParent(m);
          paramSet.insert(std::make_pair(name, pclone));
          /*

            Keep the value of the parameter used during definition. The
          param_assign contains the actual value useful for elaboration

          const typespec* ts = ((parameter*)pclone)->Typespec();
          bool multi = isMultidimensional(ts);
          if (((parameter*)pclone)->Ranges() &&
          ((parameter*)pclone)->Ranges()->size() > 1) multi = true;

          if (instance->getComplexValue(name)) {
          } else {
            Value* val = instance->getValue(name, exprBuilder);
            if (val && val->isValid() && (!multi)) {
              ((parameter*)pclone)->VpiValue(val->uhdmValue());
            }
          }
          */
          params->push_back(pclone);
        }
      }
    }
  }

  if (netlist->param_assigns()) {
    for (auto ps : *m->Param_assigns()) {
      ps->VpiParent(m);
      const std::string& name = ps->Lhs()->VpiName();
      std::map<std::string, any*>::iterator itr = paramSet.find(name);
      if (itr != paramSet.end()) {
        ps->Lhs((*itr).second);
      }
    }
  }
  return true;
}

unsigned int UhdmWriter::getVpiDirection(VObjectType type) {
  unsigned int direction = vpiInout;
  if (type == VObjectType::slPortDir_Inp ||
      type == VObjectType::slTfPortDir_Inp)
    direction = vpiInput;
  else if (type == VObjectType::slPortDir_Out ||
           type == VObjectType::slTfPortDir_Out)
    direction = vpiOutput;
  else if (type == VObjectType::slPortDir_Inout ||
           type == VObjectType::slTfPortDir_Inout)
    direction = vpiInout;
  else if (type == VObjectType::slTfPortDir_Ref ||
           type == VObjectType::slTfPortDir_ConstRef)
    direction = vpiRef;
  return direction;
}

unsigned int UhdmWriter::getVpiNetType(VObjectType type) {
  unsigned int nettype = 0;
  switch (type) {
    case VObjectType::slNetType_Wire:
      nettype = vpiWire;
      break;
    case VObjectType::slIntVec_TypeReg:
      nettype = vpiReg;
      break;
    case VObjectType::slNetType_Supply0:
      nettype = vpiSupply0;
      break;
    case VObjectType::slNetType_Supply1:
      nettype = vpiSupply1;
      break;
    case VObjectType::slIntVec_TypeLogic:
      nettype = vpiLogicNet;
      break;
    case VObjectType::slNetType_Wand:
      nettype = vpiWand;
      break;
    case VObjectType::slNetType_Wor:
      nettype = vpiWor;
      break;
    case VObjectType::slNetType_Tri:
      nettype = vpiTri;
      break;
    case VObjectType::slNetType_Tri0:
      nettype = vpiTri0;
      break;
    case VObjectType::slNetType_Tri1:
      nettype = vpiTri1;
      break;
    case VObjectType::slNetType_TriReg:
      nettype = vpiTriReg;
      break;
    case VObjectType::slNetType_TriAnd:
      nettype = vpiTriAnd;
      break;
    case VObjectType::slNetType_TriOr:
      nettype = vpiTriOr;
      break;
    case VObjectType::slNetType_Uwire:
      nettype = vpiUwire;
      break;
    default:
      break;
  }
  return nettype;
}

static void writePorts(std::vector<Signal*>& orig_ports, BaseClass* parent,
                       VectorOfport* dest_ports, VectorOfnet* dest_nets,
                       Serializer& s, UhdmWriter::ComponentMap& componentMap,
                       UhdmWriter::ModPortMap& modPortMap,
                       UhdmWriter::SignalBaseClassMap& signalBaseMap,
                       UhdmWriter::SignalMap& signalMap,
                       ModuleInstance* instance = nullptr) {
  int lastPortDirection = vpiInout;
  for (Signal* orig_port : orig_ports) {
    port* dest_port = s.MakePort();
    signalBaseMap.insert(std::make_pair(orig_port, dest_port));
    signalMap.insert(std::make_pair(orig_port->getName(), orig_port));
    const FileContent* fC = orig_port->getFileContent();
    if (fC->Type(orig_port->getNodeId()) == slStringConst)
      dest_port->VpiName(orig_port->getName());
    if (orig_port->getDirection() != slNoType)
      lastPortDirection =
          UhdmWriter::getVpiDirection(orig_port->getDirection());
    dest_port->VpiDirection(lastPortDirection);
    dest_port->VpiLineNo(
        orig_port->getFileContent()->Line(orig_port->getNodeId()));
    dest_port->VpiColumnNo(
        orig_port->getFileContent()->Column(orig_port->getNodeId()));
    dest_port->VpiEndLineNo(
        orig_port->getFileContent()->EndLine(orig_port->getNodeId()));
    dest_port->VpiEndColumnNo(
        orig_port->getFileContent()->EndColumn(orig_port->getNodeId()));
    dest_port->VpiFile(orig_port->getFileContent()->getFileName());
    dest_port->VpiParent(parent);
    if (ModPort* orig_modport = orig_port->getModPort()) {
      ref_obj* ref = s.MakeRef_obj();
      dest_port->Low_conn(ref);
      std::map<ModPort*, modport*>::iterator itr =
          modPortMap.find(orig_modport);
      if (itr != modPortMap.end()) {
        ref->Actual_group((*itr).second);
      }
    } else if (ModuleDefinition* orig_interf = orig_port->getInterfaceDef()) {
      ref_obj* ref = s.MakeRef_obj();
      dest_port->Low_conn(ref);
      const auto& found = componentMap.find(orig_interf);
      if (found != componentMap.end()) {
        ref->Actual_group(found->second);
      }
    }
    dest_ports->push_back(dest_port);
  }
}

void writeDataTypes(const DesignComponent::DataTypeMap& datatypeMap,
                    BaseClass* parent, VectorOftypespec* dest_typespecs,
                    Serializer& s, bool setParent) {
  std::set<uint64_t> ids;
  for (const auto& entry : datatypeMap) {
    const DataType* dtype = entry.second;
    if (dtype->getCategory() == DataType::Category::REF) {
      dtype = dtype->getDefinition();
    }
    if (dtype->getCategory() == DataType::Category::TYPEDEF) {
      if (dtype->getTypespec() == nullptr) dtype = dtype->getDefinition();
    }
    typespec* tps = dtype->getTypespec();
    if (parent->UhdmType() == uhdmpackage) {
      if (tps && (tps->VpiName().find("::") == std::string::npos)) {
        const std::string newName = parent->VpiName() + "::" + tps->VpiName();
        tps->VpiName(newName);
      }
    }

    if (tps) {
      if (!tps->Instance()) {
        if (parent->UhdmType() != uhdmclass_defn)
          tps->Instance((instance*)parent);
      }
      if (setParent) tps->VpiParent(parent);
      if (ids.find(tps->UhdmId()) == ids.end()) {
        dest_typespecs->push_back(tps);
        ids.insert(tps->UhdmId());
      }
    }
  }
}

void writeNets(std::vector<Signal*>& orig_nets, BaseClass* parent,
               VectorOfnet* dest_nets, Serializer& s,
               UhdmWriter::SignalBaseClassMap& signalBaseMap,
               UhdmWriter::SignalMap& signalMap, UhdmWriter::SignalMap& portMap,
               ModuleInstance* instance = nullptr) {
  for (auto& orig_net : orig_nets) {
    net* dest_net = nullptr;
    if (instance) {
      for (net* net : *instance->getNetlist()->nets()) {
        UhdmWriter::SignalMap::iterator itr = signalMap.find(net->VpiName());
        if (itr == signalMap.end()) {
          if (net->VpiName() == orig_net->getName()) {
            dest_net = net;
            break;
          }
        }
      }
    } else {
      dest_net = s.MakeLogic_net();
    }
    if (dest_net) {
      const FileContent* fC = orig_net->getFileContent();
      if (fC->Type(orig_net->getNodeId()) == slStringConst) {
        UhdmWriter::SignalMap::iterator portItr =
            portMap.find(orig_net->getName());
        if (portItr != portMap.end()) {
          Signal* sig = (*portItr).second;
          if (sig) {
            UhdmWriter::SignalBaseClassMap::iterator itr =
                signalBaseMap.find(sig);
            if (itr != signalBaseMap.end()) {
              port* p = (port*)((*itr).second);
              if (p->Low_conn() == nullptr) {
                ref_obj* ref = s.MakeRef_obj();
                ref->Actual_group(dest_net);
                p->Low_conn(ref);
              } else if (p->Low_conn()->UhdmType() == uhdmref_obj) {
                ref_obj* ref = (ref_obj*)p->Low_conn();
                if (ref->Actual_group() == nullptr) {
                  ref->Actual_group(dest_net);
                }
              }
            }
          }
        }
        signalBaseMap.insert(std::make_pair(orig_net, dest_net));
        signalMap.insert(std::make_pair(orig_net->getName(), orig_net));
        dest_net->VpiName(orig_net->getName());
        dest_net->VpiLineNo(
            orig_net->getFileContent()->Line(orig_net->getNodeId()));
        dest_net->VpiColumnNo(
            orig_net->getFileContent()->Column(orig_net->getNodeId()));
        dest_net->VpiEndLineNo(
            orig_net->getFileContent()->EndLine(orig_net->getNodeId()));
        dest_net->VpiEndColumnNo(
            orig_net->getFileContent()->EndColumn(orig_net->getNodeId()));
        dest_net->VpiFile(orig_net->getFileContent()->getFileName());
        dest_net->VpiNetType(UhdmWriter::getVpiNetType(orig_net->getType()));
        dest_net->VpiParent(parent);
        dest_nets->push_back(dest_net);
      }
    }
  }
}

void mapLowConns(std::vector<Signal*>& orig_ports, Serializer& s,
                 UhdmWriter::SignalBaseClassMap& signalBaseMap) {
  for (Signal* orig_port : orig_ports) {
    if (Signal* lowconn = orig_port->getLowConn()) {
      std::map<Signal*, BaseClass*>::iterator itrlow =
          signalBaseMap.find(lowconn);
      if (itrlow != signalBaseMap.end()) {
        std::map<Signal*, BaseClass*>::iterator itrport =
            signalBaseMap.find(orig_port);
        if (itrport != signalBaseMap.end()) {
          ref_obj* ref = s.MakeRef_obj();
          ((port*)(*itrport).second)->Low_conn(ref);
          ref->Actual_group((*itrlow).second);
        }
      }
    }
  }
}

void writeClass(ClassDefinition* classDef, VectorOfclass_defn* dest_classes,
                Serializer& s, UhdmWriter::ComponentMap& componentMap,
                BaseClass* parent) {
  if (!classDef->getFileContents().empty() &&
      classDef->getType() == VObjectType::slClass_declaration) {
    const FileContent* fC = classDef->getFileContents()[0];
    class_defn* c = classDef->getUhdmDefinition();
    c->VpiParent(parent);
    // Typepecs
    VectorOftypespec* typespecs = s.MakeTypespecVec();
    c->Typespecs(typespecs);
    writeDataTypes(classDef->getDataTypeMap(), c, typespecs, s, true);

    // Variables
    // Already bound in TestbenchElaboration

    // Function and tasks
    c->Task_funcs(classDef->getTask_funcs());
    if (c->Task_funcs()) {
      for (auto tf : *c->Task_funcs()) {
        if (tf->VpiParent() == 0) tf->VpiParent(c);
      }
    }
    // Parameters
    if (classDef->getParameters()) {
      c->Parameters(classDef->getParameters());
      for (auto ps : *c->Parameters()) {
        ps->VpiParent(c);
      }
    }
    // Param_assigns
    if (classDef->getParam_assigns()) {
      c->Param_assigns(classDef->getParam_assigns());
      for (auto ps : *c->Param_assigns()) {
        ps->VpiParent(c);
      }
    }
    componentMap.insert(std::make_pair(classDef, c));
    c->VpiParent(parent);
    dest_classes->push_back(c);
    const std::string& name = classDef->getName();
    if (c->VpiName().empty()) c->VpiName(name);
    if (c->VpiFullName().empty()) c->VpiFullName(name);
    c->Attributes(classDef->Attributes());
    if (fC) {
      // Builtin classes have no file
      c->VpiFile(fC->getFileName());
      const NodeId modId = classDef->getNodeIds()[0];
      const NodeId startId = fC->sl_collect(modId, VObjectType::slClass);
      c->VpiLineNo(fC->Line(startId));
      c->VpiColumnNo(fC->Column(startId));
      c->VpiEndLineNo(fC->EndLine(modId));
      c->VpiEndColumnNo(fC->EndColumn(modId));
    }

    for (auto& nested : classDef->getClassMap()) {
      ClassDefinition* c_nested = nested.second;
      VectorOfclass_defn* dest_classes = s.MakeClass_defnVec();
      writeClass(c_nested, dest_classes, s, componentMap, c);
    }
  }
}

void writeClasses(ClassNameClassDefinitionMultiMap& orig_classes,
                  VectorOfclass_defn* dest_classes, Serializer& s,
                  UhdmWriter::ComponentMap& componentMap, BaseClass* parent) {
  for (auto& orig_class : orig_classes) {
    ClassDefinition* classDef = orig_class.second;
    writeClass(classDef, dest_classes, s, componentMap, parent);
  }
}

void writeVariables(const DesignComponent::VariableMap& orig_vars,
                    BaseClass* parent, VectorOfvariables* dest_vars,
                    Serializer& s, UhdmWriter::ComponentMap& componentMap) {
  for (auto& orig_var : orig_vars) {
    Variable* var = orig_var.second;
    const DataType* dtype = var->getDataType();
    const ClassDefinition* classdef =
        datatype_cast<const ClassDefinition*>(dtype);
    if (classdef) {
      class_var* cvar = s.MakeClass_var();
      cvar->VpiName(var->getName());
      cvar->VpiLineNo(var->getFileContent()->Line(var->getNodeId()));
      cvar->VpiColumnNo(var->getFileContent()->Column(var->getNodeId()));
      cvar->VpiFile(var->getFileContent()->getFileName());
      cvar->VpiParent(parent);
      const auto& found = componentMap.find(classdef);
      if (found != componentMap.end()) {
        // TODO: Bind Class type,
        // class_var -> class_typespec -> class_defn
      }
      dest_vars->push_back(cvar);
    }
  }
}

class ReInstanceTypespec : public VpiListener {
 public:
  explicit ReInstanceTypespec(package* p) : m_package(p) {}
  ~ReInstanceTypespec() override {}

  void leaveShort_real_typespec(const short_real_typespec* object,
                                const BaseClass* parent, vpiHandle handle,
                                vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveReal_typespec(const real_typespec* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveByte_typespec(const byte_typespec* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveShort_int_typespec(const short_int_typespec* object,
                               const BaseClass* parent, vpiHandle handle,
                               vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveInt_typespec(const int_typespec* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveLong_int_typespec(const long_int_typespec* object,
                              const BaseClass* parent, vpiHandle handle,
                              vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveInteger_typespec(const integer_typespec* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveTime_typespec(const time_typespec* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveString_typespec(const string_typespec* object,
                            const BaseClass* parent, vpiHandle handle,
                            vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveChandle_typespec(const chandle_typespec* object,
                             const BaseClass* parent, vpiHandle handle,
                             vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leavePacked_array_typespec(const packed_array_typespec* object,
                                  const BaseClass* parent, vpiHandle handle,
                                  vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveArray_typespec(const array_typespec* object,
                           const BaseClass* parent, vpiHandle handle,
                           vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveVoid_typespec(const void_typespec* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveSequence_typespec(const sequence_typespec* object,
                              const BaseClass* parent, vpiHandle handle,
                              vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveProperty_typespec(const property_typespec* object,
                              const BaseClass* parent, vpiHandle handle,
                              vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveInterface_typespec(const interface_typespec* object,
                               const BaseClass* parent, vpiHandle handle,
                               vpiHandle parentHandle) final {
    reInstance(object);
  }

  void leaveStruct_typespec(const struct_typespec* object,
                            const BaseClass* parent, vpiHandle handle,
                            vpiHandle parentHandle) final {
    reInstance(object);
  }
  void leaveUnion_typespec(const union_typespec* object,
                           const BaseClass* parent, vpiHandle handle,
                           vpiHandle parentHandle) final {
    reInstance(object);
  }
  void leaveEnum_typespec(const enum_typespec* object, const BaseClass* parent,
                          vpiHandle handle, vpiHandle parentHandle) final {
    reInstance(object);
  }
  void leaveLogic_typespec(const logic_typespec* object,
                           const BaseClass* parent, vpiHandle handle,
                           vpiHandle parentHandle) final {
    reInstance(object);
  }
  void leaveBit_typespec(const bit_typespec* object, const BaseClass* parent,
                         vpiHandle handle, vpiHandle parentHandle) final {
    reInstance(object);
  }
  void leaveUnsupported_typespec(const unsupported_typespec* object,
                                 const BaseClass* parent, vpiHandle handle,
                                 vpiHandle parentHandle) final {
    reInstance(object);
  }
  void leaveFunction(const function* object, const BaseClass* parent,
                     vpiHandle handle, vpiHandle parentHandle) final {
    reInstance(object);
  }
  void leaveTask(const task* object, const BaseClass* parent, vpiHandle handle,
                 vpiHandle parentHandle) final {
    reInstance(object);
  }
  void reInstance(const any* cobject) {
    any* object = (any*)cobject;
    const instance* inst = nullptr;
    if (typespec* tps = any_cast<typespec*>(object)) {
      inst = (instance*)tps->Instance();
    } else if (function* tps = any_cast<function*>(object)) {
      inst = (instance*)tps->Instance();
    } else if (task* tps = any_cast<task*>(object)) {
      inst = (instance*)tps->Instance();
    }
    if (inst) {
      const std::string& name = inst->VpiName();
      design* d = (design*)m_package->VpiParent();
      for (auto pack : *d->AllPackages()) {
        if (pack->VpiName() == name) {
          if (typespec* tps = any_cast<typespec*>(object)) {
            tps->Instance(pack);
          } else if (function* tps = any_cast<function*>(object)) {
            tps->Instance(pack);
          } else if (task* tps = any_cast<task*>(object)) {
            tps->Instance(pack);
          }
          break;
        }
      }
    }
  }

 private:
  package* m_package = nullptr;
};

// Non-elaborated package typespec Instance relation need to point to
// non-elablarated package
void reInstanceTypespec(Serializer& serializer, any* root, package* p) {
  ReInstanceTypespec* listener = new ReInstanceTypespec(p);
  vpiHandle handle = serializer.MakeUhdmHandle(root->UhdmType(), root);
  listen_any(handle, listener);
  vpi_release_handle(handle);
}

void writePackage(Package* pack, package* p, Serializer& s,
                  UhdmWriter::ComponentMap& componentMap, bool top) {
  p->VpiFullName(pack->getName() + "::");
  if (top) {
    // Typepecs
    VectorOftypespec* typespecs = s.MakeTypespecVec();
    p->Typespecs(typespecs);
    writeDataTypes(pack->getDataTypeMap(), p, typespecs, s, top);
    for (auto item : pack->getImportedSymbols()) {
      typespecs->push_back(item);
    }
    // Classes
    ClassNameClassDefinitionMultiMap& orig_classes =
        pack->getClassDefinitions();
    VectorOfclass_defn* dest_classes = s.MakeClass_defnVec();
    writeClasses(orig_classes, dest_classes, s, componentMap, p);
    p->Class_defns(dest_classes);
    // Parameters
    if (pack->getParameters()) {
      p->Parameters(pack->getParameters());
      for (auto ps : *p->Parameters()) {
        ps->VpiParent(p);
        if (ps->UhdmType() == uhdmparameter) {
          ((parameter*)ps)->VpiFullName(pack->getName() + "::" + ps->VpiName());
        } else {
          ((type_parameter*)ps)
              ->VpiFullName(pack->getName() + "::" + ps->VpiName());
        }
      }
    }
  }
  // Param_assigns
  if (top) {
    if (pack->getParam_assigns()) {
      p->Param_assigns(pack->getParam_assigns());
      for (auto ps : *p->Param_assigns()) {
        ps->VpiParent(p);
      }
    }
  } else {
    if (pack->getOrigParam_assigns()) {
      p->Param_assigns(pack->getOrigParam_assigns());
      for (auto ps : *p->Param_assigns()) {
        ps->VpiParent(p);
        reInstanceTypespec(s, ps, p);
      }
    }
  }
  if (top) {
    // Function and tasks
    p->Task_funcs(pack->getTask_funcs());
    if (p->Task_funcs()) {
      for (auto tf : *p->Task_funcs()) {
        tf->VpiParent(p);
        tf->Instance(p);
        ((task_func*)tf)->VpiFullName(pack->getName() + "::" + tf->VpiName());
      }
    }

    // Variables
    Netlist* netlist = pack->getNetlist();
    if (netlist) {
      p->Variables(netlist->variables());
      if (netlist->variables()) {
        for (auto obj : *netlist->variables()) {
          obj->VpiParent(p);
          ((variables*)obj)
              ->VpiFullName(pack->getName() + "::" + obj->VpiName());
        }
      }
    }
    // Nets
    UhdmWriter::SignalBaseClassMap signalBaseMap;
    UhdmWriter::SignalMap portMap;
    UhdmWriter::SignalMap netMap;
    std::vector<Signal*> orig_nets = pack->getSignals();
    VectorOfnet* dest_nets = s.MakeNetVec();
    writeNets(orig_nets, p, dest_nets, s, signalBaseMap, netMap, portMap,
              nullptr);
    p->Nets(dest_nets);
  }
}

void UhdmWriter::writeModule(ModuleDefinition* mod, module* m, Serializer& s,
                             UhdmWriter::ComponentMap& componentMap,
                             UhdmWriter::ModPortMap& modPortMap,
                             ModuleInstance* instance) {
  UhdmWriter::SignalBaseClassMap signalBaseMap;
  UhdmWriter::SignalMap portMap;
  UhdmWriter::SignalMap netMap;

  // Let decls
  if (!mod->getLetStmts().empty()) {
    VectorOflet_decl* decls = s.MakeLet_declVec();
    m->Let_decls(decls);
    for (auto stmt : mod->getLetStmts()) {
      decls->push_back((let_decl*)stmt.second->Decl());
    }
  }

  // Typepecs
  VectorOftypespec* typespecs = s.MakeTypespecVec();
  m->Typespecs(typespecs);
  writeDataTypes(mod->getDataTypeMap(), m, typespecs, s, true);
  for (auto item : mod->getImportedSymbols()) {
    typespecs->push_back(item);
  }
  // Ports
  std::vector<Signal*>& orig_ports = mod->getPorts();
  VectorOfport* dest_ports = s.MakePortVec();
  VectorOfnet* dest_nets = s.MakeNetVec();
  writePorts(orig_ports, m, dest_ports, dest_nets, s, componentMap, modPortMap,
             signalBaseMap, portMap, instance);
  m->Ports(dest_ports);
  // Nets
  std::vector<Signal*> orig_nets = mod->getSignals();
  writeNets(orig_nets, m, dest_nets, s, signalBaseMap, netMap, portMap,
            instance);
  m->Nets(dest_nets);
  mapLowConns(orig_ports, s, signalBaseMap);
  // Classes
  ClassNameClassDefinitionMultiMap& orig_classes = mod->getClassDefinitions();
  VectorOfclass_defn* dest_classes = s.MakeClass_defnVec();
  writeClasses(orig_classes, dest_classes, s, componentMap, m);
  m->Class_defns(dest_classes);
  // Variables
  // DesignComponent::VariableMap& orig_vars = mod->getVariables();
  // VectorOfvariables* dest_vars = s.MakeVariablesVec();
  // writeVariables(orig_vars, m, dest_vars, s, componentMap);
  // m->Variables(dest_vars);

  // Cont assigns
  std::vector<cont_assign*>* orig_cont_assigns = mod->getContAssigns();
  if (orig_cont_assigns) {
    std::vector<cont_assign*>* assigns = m->Cont_assigns();
    if (assigns == nullptr) {
      m->Cont_assigns(s.MakeCont_assignVec());
      assigns = m->Cont_assigns();
    }
    for (auto ps : *orig_cont_assigns) {
      assigns->push_back(ps);
      ps->VpiParent(m);
    }
  }

  // Processes
  m->Process(mod->getProcesses());
  if (m->Process()) {
    for (auto ps : *m->Process()) {
      ps->VpiParent(m);
    }
  }
  // Parameters
  if (mod->getParameters()) {
    m->Parameters(mod->getParameters());
    for (auto ps : *m->Parameters()) {
      ps->VpiParent(m);
    }
  }
  // Param_assigns
  if (mod->getParam_assigns()) {
    m->Param_assigns(mod->getParam_assigns());
    for (auto ps : *m->Param_assigns()) {
      ps->VpiParent(m);
    }
  }
  // Function and tasks
  if (auto from = mod->getTask_funcs()) {
    UHDM::VectorOftask_func* target = m->Task_funcs();
    if (target == nullptr) {
      m->Task_funcs(s.MakeTask_funcVec());
      target = m->Task_funcs();
    }
    for (auto tf : *from) {
      target->push_back(tf);
      if (tf->VpiParent() == 0) tf->VpiParent(m);
      if (tf->Instance() == 0) tf->Instance(m);
    }
  }

  // ClockingBlocks
  for (const auto& ctupple : mod->getClockingBlockMap()) {
    const ClockingBlock& cblock = ctupple.second;
    switch (cblock.getType()) {
      case ClockingBlock::Type::Default: {
        m->Default_clocking(cblock.getActual());
        break;
      }
      case ClockingBlock::Type::Global: {
        m->Global_clocking(cblock.getActual());
        break;
      }
      case ClockingBlock::Type::Regular: {
        VectorOfclocking_block* cblocks = m->Clocking_blocks();
        if (cblocks == nullptr) {
          m->Clocking_blocks(s.MakeClocking_blockVec());
          cblocks = m->Clocking_blocks();
        }
        cblocks->push_back(cblock.getActual());
        break;
      }
    }
  }

  // Assertions
  if (mod->getAssertions()) {
    m->Assertions(mod->getAssertions());
    for (auto ps : *m->Assertions()) {
      ps->VpiParent(m);
    }
  }
}

void UhdmWriter::writeInterface(ModuleDefinition* mod, interface* m,
                                Serializer& s, ComponentMap& componentMap,
                                ModPortMap& modPortMap,
                                ModuleInstance* instance) {
  SignalBaseClassMap signalBaseMap;
  SignalMap portMap;
  SignalMap netMap;

  // Let decls
  if (!mod->getLetStmts().empty()) {
    VectorOflet_decl* decls = s.MakeLet_declVec();
    m->Let_decls(decls);
    for (auto stmt : mod->getLetStmts()) {
      decls->push_back((let_decl*)stmt.second->Decl());
    }
  }

  // Typepecs
  VectorOftypespec* typespecs = s.MakeTypespecVec();
  m->Typespecs(typespecs);
  writeDataTypes(mod->getDataTypeMap(), m, typespecs, s, true);
  for (auto item : mod->getImportedSymbols()) {
    typespecs->push_back(item);
  }
  // Ports
  std::vector<Signal*>& orig_ports = mod->getPorts();
  VectorOfport* dest_ports = s.MakePortVec();
  VectorOfnet* dest_nets = s.MakeNetVec();
  writePorts(orig_ports, m, dest_ports, dest_nets, s, componentMap, modPortMap,
             signalBaseMap, portMap, instance);
  m->Ports(dest_ports);
  std::vector<Signal*> orig_nets = mod->getSignals();
  writeNets(orig_nets, m, dest_nets, s, signalBaseMap, netMap, portMap,
            instance);
  m->Nets(dest_nets);
  // Modports
  ModuleDefinition::ModPortSignalMap& orig_modports =
      mod->getModPortSignalMap();
  VectorOfmodport* dest_modports = s.MakeModportVec();
  for (auto& orig_modport : orig_modports) {
    modport* dest_modport = s.MakeModport();
    // dest_modport->Interface(m); // Loop in elaboration!
    dest_modport->VpiParent(m);
    const FileContent* orig_fC = orig_modport.second.getFileContent();
    const NodeId orig_nodeId = orig_modport.second.getNodeId();
    dest_modport->VpiFile(orig_fC->getFileName());
    dest_modport->VpiLineNo(orig_fC->Line(orig_nodeId));
    dest_modport->VpiColumnNo(orig_fC->Column(orig_nodeId));
    dest_modport->VpiEndLineNo(orig_fC->EndLine(orig_nodeId));
    dest_modport->VpiEndColumnNo(orig_fC->EndColumn(orig_nodeId));
    modPortMap.insert(std::make_pair(&orig_modport.second, dest_modport));
    dest_modport->VpiName(orig_modport.first);
    VectorOfio_decl* ios = s.MakeIo_declVec();
    for (auto& sig : orig_modport.second.getPorts()) {
      const FileContent* fC = sig.getFileContent();
      io_decl* io = s.MakeIo_decl();
      io->VpiName(sig.getName());
      NodeId id = sig.getNodeId();
      io->VpiFile(fC->getFileName());
      io->VpiLineNo(fC->Line(id));
      io->VpiColumnNo(fC->Column(id));
      io->VpiEndLineNo(fC->EndLine(id));
      io->VpiEndColumnNo(fC->EndColumn(id));
      if (NodeId Expression = fC->Sibling(id)) {
        any* exp =
            m_helper.compileExpression(mod, fC, Expression, m_compileDesign,
                                       nullptr, instance, true, true);
        io->Expr(exp);
      }
      unsigned int direction = UhdmWriter::getVpiDirection(sig.getDirection());
      io->VpiDirection(direction);
      ios->push_back(io);
    }
    dest_modport->Io_decls(ios);
    dest_modports->push_back(dest_modport);
  }
  m->Modports(dest_modports);

  // Cont assigns
  std::vector<cont_assign*>* orig_cont_assigns = mod->getContAssigns();
  if (orig_cont_assigns) {
    std::vector<cont_assign*>* assigns = m->Cont_assigns();
    if (assigns == nullptr) {
      m->Cont_assigns(s.MakeCont_assignVec());
      assigns = m->Cont_assigns();
    }
    for (auto ps : *orig_cont_assigns) {
      assigns->push_back(ps);
      ps->VpiParent(m);
    }
  }

  // Processes
  m->Process(mod->getProcesses());
  if (m->Process()) {
    for (auto ps : *m->Process()) {
      ps->VpiParent(m);
    }
  }

  // Parameters
  if (mod->getParameters()) {
    m->Parameters(mod->getParameters());
    for (auto ps : *m->Parameters()) {
      ps->VpiParent(m);
    }
  }
  // Param_assigns
  if (mod->getParam_assigns()) {
    m->Param_assigns(mod->getParam_assigns());
    for (auto ps : *m->Param_assigns()) {
      ps->VpiParent(m);
    }
  }

  // Function and tasks
  m->Task_funcs(mod->getTask_funcs());
  if (m->Task_funcs()) {
    for (auto tf : *m->Task_funcs()) {
      if (tf->VpiParent() == 0) tf->VpiParent(m);
      if (tf->Instance() == 0) tf->Instance(m);
    }
  }

  // ClockingBlocks
  for (const auto& ctupple : mod->getClockingBlockMap()) {
    const ClockingBlock& cblock = ctupple.second;
    switch (cblock.getType()) {
      case ClockingBlock::Type::Default: {
        m->Default_clocking(cblock.getActual());
        break;
      }
      case ClockingBlock::Type::Global: {
        m->Global_clocking(cblock.getActual());
        break;
      }
      case ClockingBlock::Type::Regular: {
        VectorOfclocking_block* cblocks = m->Clocking_blocks();
        if (cblocks == nullptr) {
          m->Clocking_blocks(s.MakeClocking_blockVec());
          cblocks = m->Clocking_blocks();
        }
        cblocks->push_back(cblock.getActual());
        break;
      }
    }
  }
}

void writeProgram(Program* mod, program* m, Serializer& s,
                  UhdmWriter::ComponentMap& componentMap,
                  UhdmWriter::ModPortMap& modPortMap,
                  ModuleInstance* instance = nullptr) {
  UhdmWriter::SignalBaseClassMap signalBaseMap;
  UhdmWriter::SignalMap portMap;
  UhdmWriter::SignalMap netMap;
  // Typepecs
  VectorOftypespec* typespecs = s.MakeTypespecVec();
  m->Typespecs(typespecs);
  writeDataTypes(mod->getDataTypeMap(), m, typespecs, s, true);
  for (auto item : mod->getImportedSymbols()) {
    typespecs->push_back(item);
  }
  // Ports
  std::vector<Signal*>& orig_ports = mod->getPorts();
  VectorOfport* dest_ports = s.MakePortVec();
  VectorOfnet* dest_nets = s.MakeNetVec();
  writePorts(orig_ports, m, dest_ports, dest_nets, s, componentMap, modPortMap,
             signalBaseMap, portMap, instance);
  m->Ports(dest_ports);
  // Nets
  std::vector<Signal*>& orig_nets = mod->getSignals();
  writeNets(orig_nets, m, dest_nets, s, signalBaseMap, netMap, portMap,
            instance);
  m->Nets(dest_nets);
  mapLowConns(orig_ports, s, signalBaseMap);
  // Parameters
  if (mod->getParameters()) {
    m->Parameters(mod->getParameters());
    for (auto ps : *m->Parameters()) {
      ps->VpiParent(m);
    }
  }
  // Param_assigns
  if (mod->getParam_assigns()) {
    m->Param_assigns(mod->getParam_assigns());
    for (auto ps : *m->Param_assigns()) {
      ps->VpiParent(m);
    }
  }
  // Classes
  ClassNameClassDefinitionMultiMap& orig_classes = mod->getClassDefinitions();
  VectorOfclass_defn* dest_classes = s.MakeClass_defnVec();
  writeClasses(orig_classes, dest_classes, s, componentMap, m);
  m->Class_defns(dest_classes);
  // Variables
  const DesignComponent::VariableMap& orig_vars = mod->getVariables();
  VectorOfvariables* dest_vars = s.MakeVariablesVec();
  writeVariables(orig_vars, m, dest_vars, s, componentMap);
  m->Variables(dest_vars);
  // Processes
  m->Process(mod->getProcesses());
  if (m->Process()) {
    for (auto ps : *m->Process()) {
      ps->VpiParent(m);
    }
  }
  m->Task_funcs(mod->getTask_funcs());
  if (m->Task_funcs()) {
    for (auto tf : *m->Task_funcs()) {
      tf->VpiParent(m);
    }
  }

  // ClockingBlocks
  for (const auto& ctupple : mod->getClockingBlockMap()) {
    const ClockingBlock& cblock = ctupple.second;
    switch (cblock.getType()) {
      case ClockingBlock::Type::Default: {
        m->Default_clocking(cblock.getActual());
        break;
      }
      case ClockingBlock::Type::Regular: {
        VectorOfclocking_block* cblocks = m->Clocking_blocks();
        if (cblocks == nullptr) {
          m->Clocking_blocks(s.MakeClocking_blockVec());
          cblocks = m->Clocking_blocks();
        }
        cblocks->push_back(cblock.getActual());
        break;
      }
      default:
        break;
    }
  }
}

bool UhdmWriter::writeElabProgram(Serializer& s, ModuleInstance* instance,
                                  program* m) {
  Netlist* netlist = instance->getNetlist();

  DesignComponent* mod = instance->getDefinition();
  if (mod) {
    // Let decls
    if (!mod->getLetStmts().empty()) {
      VectorOflet_decl* decls = s.MakeLet_declVec();
      m->Let_decls(decls);
      for (auto stmt : mod->getLetStmts()) {
        decls->push_back((let_decl*)stmt.second->Decl());
      }
    }
    // Typepecs
    VectorOftypespec* typespecs = s.MakeTypespecVec();
    m->Typespecs(typespecs);
    writeDataTypes(mod->getDataTypeMap(), m, typespecs, s, false);
    for (auto item : mod->getImportedSymbols()) {
      typespecs->push_back(item);
    }
  }

  m->Ports(netlist->ports());
  if (netlist->ports()) {
    for (auto obj : *netlist->ports()) {
      obj->VpiParent(m);
    }
  }
  m->Nets(netlist->nets());
  if (netlist->nets()) {
    for (auto obj : *netlist->nets()) {
      obj->VpiParent(m);
    }
  }
  m->Gen_scope_arrays(netlist->gen_scopes());
  m->Variables(netlist->variables());
  if (netlist->variables()) {
    for (auto obj : *netlist->variables()) {
      obj->VpiParent(m);
    }
  }
  m->Array_vars(netlist->array_vars());
  if (netlist->array_vars()) {
    for (auto obj : *netlist->array_vars()) {
      obj->VpiParent(m);
    }
  }
  m->Named_events(netlist->named_events());
  if (netlist->named_events()) {
    for (auto obj : *netlist->named_events()) {
      obj->VpiParent(m);
    }
  }
  m->Array_nets(netlist->array_nets());
  if (netlist->array_nets()) {
    for (auto obj : *netlist->array_nets()) {
      obj->VpiParent(m);
    }
  }

  if (netlist->cont_assigns()) {
    std::vector<cont_assign*>* assigns = m->Cont_assigns();
    if (assigns == nullptr) {
      m->Cont_assigns(s.MakeCont_assignVec());
      assigns = m->Cont_assigns();
    }
    for (auto obj : *netlist->cont_assigns()) {
      obj->VpiParent(m);
      assigns->push_back(obj);
    }
  }

  if (mod) {
    if (auto from = mod->getTask_funcs()) {
      UHDM::VectorOftask_func* target = m->Task_funcs();
      if (target == nullptr) {
        m->Task_funcs(s.MakeTask_funcVec());
        target = m->Task_funcs();
      }
      for (auto tf : *from) {
        target->push_back(tf);
        if (tf->VpiParent() == 0) tf->VpiParent(m);
        if (tf->Instance() == 0) tf->Instance(m);
      }
    }
  }

  if (mod) {
    lateBinding(s, mod, m);
  }

  return true;
}

bool UhdmWriter::writeElabGenScope(Serializer& s, ModuleInstance* instance,
                                   gen_scope* m, ExprBuilder& exprBuilder) {
  Netlist* netlist = instance->getNetlist();

  ModuleDefinition* mod =
      valuedcomponenti_cast<ModuleDefinition*>(instance->getDefinition());
  if (mod) {
    // Let decls
    if (!mod->getLetStmts().empty()) {
      VectorOflet_decl* decls = s.MakeLet_declVec();
      m->Let_decls(decls);
      for (auto stmt : mod->getLetStmts()) {
        decls->push_back((let_decl*)stmt.second->Decl());
      }
    }
    // Typepecs
    VectorOftypespec* typespecs = s.MakeTypespecVec();
    m->Typespecs(typespecs);
    writeDataTypes(mod->getDataTypeMap(), m, typespecs, s, true);
    for (auto item : mod->getImportedSymbols()) {
      typespecs->push_back(item);
    }
  }

  m->Nets(netlist->nets());
  if (netlist->nets()) {
    for (auto obj : *netlist->nets()) {
      obj->VpiParent(m);
    }
  }

  std::vector<gen_scope_array*>* gen_scope_arrays = netlist->gen_scopes();
  if (gen_scope_arrays) {
    for (gen_scope_array* scope_arr : *gen_scope_arrays) {
      for (gen_scope* scope : *scope_arr->Gen_scopes()) {
        m->Cont_assigns(scope->Cont_assigns());
        if (m->Cont_assigns()) {
          for (auto ps : *m->Cont_assigns()) {
            ps->VpiParent(m);
          }
        }
        m->Process(scope->Process());
        if (m->Process()) {
          for (auto ps : *m->Process()) {
            ps->VpiParent(m);
          }
        }

        writeElabParameters(s, instance, m, exprBuilder);

        // Loop indexes
        for (auto& param : instance->getMappedValues()) {
          const std::string& name = param.first;
          Value* val = param.second.first;
          VectorOfany* params = nullptr;
          params = m->Parameters();
          if (params == nullptr) {
            params = s.MakeAnyVec();
          }
          m->Parameters(params);
          bool found = false;
          for (auto p : *params) {
            if (p->VpiName() == name) {
              found = true;
              break;
            }
          }
          if (!found) {
            parameter* p = s.MakeParameter();
            p->VpiName(name);
            if (val && val->isValid()) p->VpiValue(val->uhdmValue());
            p->VpiFile(instance->getFileName());
            p->VpiLineNo(param.second.second);
            p->VpiParent(m);
            p->VpiLocalParam(true);
            int_typespec* ts = s.MakeInt_typespec();
            p->Typespec(ts);
            params->push_back(p);
          }
        }
      }
    }
  }

  m->Variables(netlist->variables());
  if (netlist->variables()) {
    for (auto obj : *netlist->variables()) {
      obj->VpiParent(m);
    }
  }
  m->Array_vars(netlist->array_vars());
  if (netlist->array_vars()) {
    for (auto obj : *netlist->array_vars()) {
      obj->VpiParent(m);
    }
  }
  m->Named_events(netlist->named_events());
  if (netlist->named_events()) {
    for (auto obj : *netlist->named_events()) {
      obj->VpiParent(m);
    }
  }
  m->Array_nets(netlist->array_nets());
  if (netlist->array_nets()) {
    for (auto obj : *netlist->array_nets()) {
      obj->VpiParent(m);
    }
  }

  DesignComponent* def = instance->getDefinition();
  if (def->getTask_funcs()) {
    const any* tmp = m;
    while ((tmp->UhdmType() != uhdmmodule) &&
           (tmp->UhdmType() != uhdminterface)) {
      tmp = tmp->VpiParent();
    }
    UHDM::instance* mm = (UHDM::instance*)tmp;
    // Function and tasks
    UHDM::VectorOftask_func* target = mm->Task_funcs();
    if (target == nullptr) {
      mm->Task_funcs(s.MakeTask_funcVec());
      target = mm->Task_funcs();
    }
    for (auto tf : *def->getTask_funcs()) {
      target->push_back(tf);
      if (tf->VpiParent() == 0) tf->VpiParent(mm);
    }
  }

  if (netlist->cont_assigns()) {
    std::vector<cont_assign*>* assigns = m->Cont_assigns();
    if (assigns == nullptr) {
      m->Cont_assigns(s.MakeCont_assignVec());
      assigns = m->Cont_assigns();
    }
    for (auto obj : *netlist->cont_assigns()) {
      obj->VpiParent(m);
      assigns->push_back(obj);
    }
  }

  // ClockingBlocks
  for (const auto& ctupple : mod->getClockingBlockMap()) {
    const ClockingBlock& cblock = ctupple.second;
    switch (cblock.getType()) {
      case ClockingBlock::Type::Default: {
        // No default clocking
        // m->Default_clocking(cblock.getActual());
        break;
      }
      case ClockingBlock::Type::Regular: {
        VectorOfclocking_block* cblocks = m->Clocking_blocks();
        if (cblocks == nullptr) {
          m->Clocking_blocks(s.MakeClocking_blockVec());
          cblocks = m->Clocking_blocks();
        }
        cblocks->push_back(cblock.getActual());
        break;
      }
      default:
        break;
    }
  }

  if (mod) {
    lateBinding(s, mod, m);
  }

  return true;
}

void UhdmWriter::lateBinding(UHDM::Serializer& s, DesignComponent* mod,
                             scope* m) {
  for (UHDM::ref_obj* ref : mod->getLateBinding()) {
    if (ref->Actual_group()) continue;
    std::string name = ref->VpiName();
    name = StringUtils::trim(name);
    if (name.find("::") != std::string::npos) {
      continue;
    }

    const any* parent = ref->VpiParent();
    while (parent) {
      if (parent->UhdmType() == uhdmfunction) {
        function* func = (function*)parent;
        if (parent->VpiName() == name) {
          if (const any* ret = func->Return()) {
            ElaboratorListener listener(&s);
            any* pclone = UHDM::clone_tree(ret, s, &listener);
            variables* var = (variables*)pclone;
            var->VpiName(name);
            ref->Actual_group(pclone);
          }
          break;
        }
        if (auto decls = func->Io_decls()) {
          for (auto decl : *decls) {
            if (decl->VpiName() == name) {
              if (decl->UhdmType() == uhdmref_var) continue;
              if (decl->UhdmType() == uhdmref_obj) continue;
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (auto vars = func->Variables()) {
          for (auto decl : *vars) {
            if (decl->VpiName() == name) {
              if (decl->UhdmType() == uhdmref_var) continue;
              if (decl->UhdmType() == uhdmref_obj) continue;
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (auto params = func->Parameters()) {
          for (auto decl : *params) {
            if (decl->VpiName() == name) {
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (const UHDM::instance* inst = func->Instance()) {
          if (inst->UhdmType() == uhdmpackage) {
            package* pack = (package*)inst;
            if (pack->Variables()) {
              for (auto n : *pack->Variables()) {
                if (n->VpiName() == name) {
                  if (n->UhdmType() == uhdmref_var) continue;
                  if (n->UhdmType() == uhdmref_obj) continue;
                  ref->Actual_group(n);
                  break;
                }
                const std::string pname =
                    std::string(m->VpiName() + "::" + name);
                if (n->VpiName() == pname) {
                  if (n->UhdmType() == uhdmref_var) continue;
                  if (n->UhdmType() == uhdmref_obj) continue;
                  ref->Actual_group(n);
                  break;
                }
              }
            }
          }
        }
        if (ref->Actual_group()) break;
      } else if (parent->UhdmType() == uhdmtask) {
        task* ta = (task*)parent;
        if (auto decls = ta->Io_decls()) {
          for (auto decl : *decls) {
            if (decl->VpiName() == name) {
              if (decl->UhdmType() == uhdmref_var) continue;
              if (decl->UhdmType() == uhdmref_obj) continue;
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (auto vars = ta->Variables()) {
          for (auto decl : *vars) {
            if (decl->VpiName() == name) {
              if (decl->UhdmType() == uhdmref_var) continue;
              if (decl->UhdmType() == uhdmref_obj) continue;
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (const UHDM::instance* inst = ta->Instance()) {
          if (inst->UhdmType() == uhdmpackage) {
            package* pack = (package*)inst;
            if (pack->Variables()) {
              for (auto n : *pack->Variables()) {
                if (n->VpiName() == name) {
                  if (n->UhdmType() == uhdmref_var) continue;
                  if (n->UhdmType() == uhdmref_obj) continue;
                  ref->Actual_group(n);
                  break;
                }
                const std::string pname =
                    std::string(m->VpiName() + "::" + name);
                if (n->VpiName() == pname) {
                  if (n->UhdmType() == uhdmref_var) continue;
                  if (n->UhdmType() == uhdmref_obj) continue;
                  ref->Actual_group(n);
                  break;
                }
              }
            }
          }
        }
        if (ref->Actual_group()) break;
      } else if (parent->UhdmType() == uhdmfor_stmt) {
        for_stmt* f = (for_stmt*)parent;
        VectorOfany* inits = f->VpiForInitStmts();
        if (inits) {
          for (auto init : *inits) {
            if (init->UhdmType() == uhdmassign_stmt) {
              assign_stmt* as = (assign_stmt*)init;
              const expr* lhs = as->Lhs();
              if (lhs->VpiName() == name) {
                if (lhs->UhdmType() == uhdmref_var) continue;
                if (lhs->UhdmType() == uhdmref_obj) continue;
                ref->Actual_group((expr*)lhs);
                break;
              }
            }
          }
        }
      } else if (parent->UhdmType() == uhdmbegin) {
        begin* b = (begin*)parent;
        if (auto vars = b->Variables()) {
          for (auto decl : *vars) {
            if (decl->VpiName() == name) {
              if (decl->UhdmType() == uhdmref_var) continue;
              if (decl->UhdmType() == uhdmref_obj) continue;
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (ref->Actual_group()) break;
        if (auto params = b->Parameters()) {
          for (auto decl : *params) {
            if (decl->VpiName() == name) {
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (ref->Actual_group()) break;
        VectorOfany* stmts = b->Stmts();
        if (stmts) {
          for (auto init : *stmts) {
            if (init->UhdmType() == uhdmassign_stmt) {
              assign_stmt* as = (assign_stmt*)init;
              const expr* lhs = as->Lhs();
              if (lhs->VpiName() == name) {
                if (lhs->UhdmType() == uhdmref_var) continue;
                if (lhs->UhdmType() == uhdmref_obj) continue;
                ref->Actual_group((expr*)lhs);
                break;
              }
            }
          }
        }
      } else if (parent->UhdmType() == uhdmnamed_begin) {
        named_begin* b = (named_begin*)parent;
        if (auto vars = b->Variables()) {
          for (auto decl : *vars) {
            if (decl->VpiName() == name) {
              if (decl->UhdmType() == uhdmref_var) continue;
              if (decl->UhdmType() == uhdmref_obj) continue;
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (ref->Actual_group()) break;
        if (auto params = b->Parameters()) {
          for (auto decl : *params) {
            if (decl->VpiName() == name) {
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (ref->Actual_group()) break;
        VectorOfany* stmts = b->Stmts();
        if (stmts) {
          for (auto init : *stmts) {
            if (init->UhdmType() == uhdmassign_stmt) {
              assign_stmt* as = (assign_stmt*)init;
              const expr* lhs = as->Lhs();
              if (lhs->VpiName() == name) {
                if (lhs->UhdmType() == uhdmref_var) continue;
                if (lhs->UhdmType() == uhdmref_obj) continue;
                ref->Actual_group((expr*)lhs);
                break;
              }
            }
          }
        }
      } else if (parent->UhdmType() == uhdmfork_stmt) {
        fork_stmt* b = (fork_stmt*)parent;
        if (auto vars = b->Variables()) {
          for (auto decl : *vars) {
            if (decl->VpiName() == name) {
              if (decl->UhdmType() == uhdmref_var) continue;
              if (decl->UhdmType() == uhdmref_obj) continue;
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (ref->Actual_group()) break;
        if (auto params = b->Parameters()) {
          for (auto decl : *params) {
            if (decl->VpiName() == name) {
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (ref->Actual_group()) break;
        VectorOfany* stmts = b->Stmts();
        if (stmts) {
          for (auto init : *stmts) {
            if (init->UhdmType() == uhdmassign_stmt) {
              assign_stmt* as = (assign_stmt*)init;
              const expr* lhs = as->Lhs();
              if (lhs->VpiName() == name) {
                if (lhs->UhdmType() == uhdmref_var) continue;
                if (lhs->UhdmType() == uhdmref_obj) continue;
                ref->Actual_group((expr*)lhs);
                break;
              }
            }
          }
        }
      } else if (parent->UhdmType() == uhdmnamed_fork) {
        named_fork* b = (named_fork*)parent;
        if (auto vars = b->Variables()) {
          for (auto decl : *vars) {
            if (decl->VpiName() == name) {
              if (decl->UhdmType() == uhdmref_var) continue;
              if (decl->UhdmType() == uhdmref_obj) continue;
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (ref->Actual_group()) break;
        if (auto params = b->Parameters()) {
          for (auto decl : *params) {
            if (decl->VpiName() == name) {
              ref->Actual_group(decl);
              break;
            }
          }
        }
        if (ref->Actual_group()) break;
        VectorOfany* stmts = b->Stmts();
        if (stmts) {
          for (auto init : *stmts) {
            if (init->UhdmType() == uhdmassign_stmt) {
              assign_stmt* as = (assign_stmt*)init;
              const expr* lhs = as->Lhs();
              if (lhs->VpiName() == name) {
                if (lhs->UhdmType() == uhdmref_var) continue;
                if (lhs->UhdmType() == uhdmref_obj) continue;
                ref->Actual_group((expr*)lhs);
                break;
              }
            }
          }
        }
      }
      if (ref->Actual_group()) break;
      parent = parent->VpiParent();
    }
    if (ref->Actual_group()) continue;

    if (m->UhdmType() == uhdmmodule || m->UhdmType() == uhdminterface ||
        m->UhdmType() == uhdmprogram) {
      instance* inst = (instance*)m;
      if (inst->Nets()) {
        for (auto n : *inst->Nets()) {
          if (n->VpiName() == name) {
            ref->Actual_group(n);
            break;
          }
        }
        if (ref->Actual_group()) continue;
      }
      if (inst->Array_nets()) {
        for (auto n : *inst->Array_nets()) {
          if (n->VpiName() == name) {
            ref->Actual_group(n);
            break;
          }
        }
        if (ref->Actual_group()) continue;
      }
    }
    if (m->Variables()) {
      for (auto n : *m->Variables()) {
        if (n->VpiName() == name) {
          ref->Actual_group(n);
          break;
        }
        const std::string pname = std::string(m->VpiName() + "::" + name);
        if (n->VpiName() == pname) {
          ref->Actual_group(n);
          break;
        }
      }
      if (ref->Actual_group()) continue;
    }

    if (m->Param_assigns()) {
      bool isParam = false;
      for (auto p : *m->Param_assigns()) {
        const any* lhs = p->Lhs();
        if (lhs->VpiName() == name) {
          // Do not bind blindly here, let the uhdmelab do this correctly
          // ref->Actual_group((any*)p->Rhs());
          isParam = true;
          break;
        }
      }
      if (isParam) continue;
    }
    if (m->Parameters()) {
      bool isParam = false;
      for (auto p : *m->Parameters()) {
        if (p->VpiName() == name) {
          isParam = true;
          break;
        }
      }
      if (isParam) continue;
    }

    if (m->Typespecs()) {
      bool isTypespec = false;
      std::vector<std::string> importedPackages;
      for (auto n : *m->Typespecs()) {
        if (n->UhdmType() == uhdmimport) {
          importedPackages.push_back(n->VpiName());
        }
      }

      for (auto n : *m->Typespecs()) {
        if (n->UhdmType() == uhdmenum_typespec) {
          enum_typespec* tps = any_cast<enum_typespec*>(n);
          if (tps && tps->Enum_consts()) {
            for (auto c : *tps->Enum_consts()) {
              if (c->VpiName() == name) {
                ref->Actual_group(c);
                break;
              }
            }
          }
        }
        for (const auto& imp : importedPackages) {
          if (n->VpiName() == std::string(imp + "::" + name)) {
            isTypespec = true;
            break;
          }
        }
        if (n->VpiName() == name) {
          isTypespec = true;
          break;
        }
        if (ref->Actual_group()) break;
      }
      if (isTypespec) continue;
      if (ref->Actual_group()) continue;
    }
    const FileContent* fC = mod->getFileContents()[0];
    for (const auto& td : fC->getTypeDefMap()) {
      const DataType* dt = td.second;
      while (dt) {
        typespec* n = dt->getTypespec();
        if (n && (n->UhdmType() == uhdmenum_typespec)) {
          enum_typespec* tps = any_cast<enum_typespec*>(n);
          if (tps && tps->Enum_consts()) {
            for (auto c : *tps->Enum_consts()) {
              if (c->VpiName() == name) {
                ref->Actual_group(c);
                break;
              }
            }
          }
        }
        dt = dt->getDefinition();
        if (ref->Actual_group()) break;
      }
      if (ref->Actual_group()) break;
    }

    if (m->Variables()) {
      for (auto var : *m->Variables()) {
        if (var->UhdmType() == uhdmenum_var) {
          const enum_typespec* tps =
              any_cast<const enum_typespec*>(var->Typespec());
          if (tps && tps->Enum_consts()) {
            for (auto c : *tps->Enum_consts()) {
              if (c->VpiName() == name) {
                ref->Actual_group(c);
                break;
              }
            }
          }
        }
        if (ref->Actual_group()) break;
      }
    }
    if (!ref->Actual_group()) {
      if (mod) {
        if (auto elem = mod->getDesignElement()) {
          if (elem->m_defaultNetType == slNoType) {
            Location loc(m_compileDesign->getCompiler()
                             ->getSymbolTable()
                             ->registerSymbol(ref->VpiFile().string()),
                         ref->VpiLineNo(), ref->VpiColumnNo(),
                         m_compileDesign->getCompiler()
                             ->getSymbolTable()
                             ->registerSymbol(name));
            Error err(ErrorDefinition::ELAB_ILLEGAL_IMPLICIT_NET, loc);
            m_compileDesign->getCompiler()->getErrorContainer()->addError(err);
            m_compileDesign->getCompiler()->getErrorContainer()->printMessages(
                m_compileDesign->getCompiler()
                    ->getCommandLineParser()
                    ->muteStdout());
          } else {
            if (m->UhdmType() == uhdmmodule || m->UhdmType() == uhdminterface ||
                m->UhdmType() == uhdmprogram) {
              instance* inst = (instance*)m;
              logic_net* net = s.MakeLogic_net();
              net->VpiName(name);
              net->VpiNetType(
                  UhdmWriter::getVpiNetType(elem->m_defaultNetType));
              ref->Actual_group(net);
              VectorOfnet* nets = inst->Nets();
              if (nets == nullptr) {
                inst->Nets(s.MakeNetVec());
                nets = inst->Nets();
              }
              nets->push_back(net);
            }
          }
        }
      }
    }
  }
}

bool UhdmWriter::writeElabModule(Serializer& s, ModuleInstance* instance,
                                 module* m, ExprBuilder& exprBuilder) {
  Netlist* netlist = instance->getNetlist();
  if (netlist == nullptr) return true;
  m->Ports(netlist->ports());

  DesignComponent* mod = instance->getDefinition();
  if (mod) {
    // Let decls
    if (!mod->getLetStmts().empty()) {
      VectorOflet_decl* decls = s.MakeLet_declVec();
      m->Let_decls(decls);
      for (auto stmt : mod->getLetStmts()) {
        decls->push_back((let_decl*)stmt.second->Decl());
      }
    }
    // Typepecs
    VectorOftypespec* typespecs = s.MakeTypespecVec();
    m->Typespecs(typespecs);
    writeDataTypes(mod->getDataTypeMap(), m, typespecs, s, false);
    for (auto item : mod->getImportedSymbols()) {
      typespecs->push_back(item);
    }
  }

  writeElabParameters(s, instance, m, exprBuilder);

  if (netlist->ports()) {
    for (auto obj : *netlist->ports()) {
      obj->VpiParent(m);
    }
  }
  m->Nets(netlist->nets());
  if (netlist->nets()) {
    for (auto obj : *netlist->nets()) {
      obj->VpiParent(m);
    }
  }
  m->Gen_scope_arrays(netlist->gen_scopes());
  if (netlist->gen_scopes()) {
    for (auto obj : *netlist->gen_scopes()) {
      obj->VpiParent(m);
    }
  }
  m->Variables(netlist->variables());
  if (netlist->variables()) {
    for (auto obj : *netlist->variables()) {
      obj->VpiParent(m);
    }
  }
  m->Array_vars(netlist->array_vars());
  if (netlist->array_vars()) {
    for (auto obj : *netlist->array_vars()) {
      obj->VpiParent(m);
    }
  }
  m->Named_events(netlist->named_events());
  if (netlist->named_events()) {
    for (auto obj : *netlist->named_events()) {
      obj->VpiParent(m);
    }
  }
  m->Array_nets(netlist->array_nets());
  if (netlist->array_nets()) {
    for (auto obj : *netlist->array_nets()) {
      obj->VpiParent(m);
    }
  }

  if (netlist->cont_assigns()) {
    std::vector<cont_assign*>* assigns = m->Cont_assigns();
    if (assigns == nullptr) {
      m->Cont_assigns(s.MakeCont_assignVec());
      assigns = m->Cont_assigns();
    }
    for (auto obj : *netlist->cont_assigns()) {
      obj->VpiParent(m);
      assigns->push_back(obj);
    }
  }

  if (mod) {
    if (auto from = mod->getTask_funcs()) {
      UHDM::VectorOftask_func* target = m->Task_funcs();
      if (target == nullptr) {
        m->Task_funcs(s.MakeTask_funcVec());
        target = m->Task_funcs();
      }
      for (auto tf : *from) {
        target->push_back(tf);
        if (tf->VpiParent() == 0) tf->VpiParent(m);
        if (tf->Instance() == 0) tf->Instance(m);
      }
    }
  }

  if (mod) {
    lateBinding(s, mod, m);
  }

  return true;
}

bool UhdmWriter::writeElabInterface(Serializer& s, ModuleInstance* instance,
                                    interface* m, ExprBuilder& exprBuilder) {
  Netlist* netlist = instance->getNetlist();

  DesignComponent* mod = instance->getDefinition();
  if (mod) {
    // Let decls
    if (!mod->getLetStmts().empty()) {
      VectorOflet_decl* decls = s.MakeLet_declVec();
      m->Let_decls(decls);
      for (auto stmt : mod->getLetStmts()) {
        decls->push_back((let_decl*)stmt.second->Decl());
      }
    }
    // Typepecs
    VectorOftypespec* typespecs = s.MakeTypespecVec();
    m->Typespecs(typespecs);
    writeDataTypes(mod->getDataTypeMap(), m, typespecs, s, false);
    for (auto item : mod->getImportedSymbols()) {
      typespecs->push_back(item);
    }
  }

  writeElabParameters(s, instance, m, exprBuilder);

  m->Ports(netlist->ports());
  if (netlist->ports()) {
    for (auto obj : *netlist->ports()) {
      obj->VpiParent(m);
    }
  }
  m->Nets(netlist->nets());
  if (netlist->nets()) {
    for (auto obj : *netlist->nets()) {
      obj->VpiParent(m);
    }
  }
  m->Gen_scope_arrays(netlist->gen_scopes());
  if (netlist->gen_scopes()) {
    for (auto obj : *netlist->gen_scopes()) {
      obj->VpiParent(m);
    }
  }
  m->Variables(netlist->variables());
  if (netlist->variables()) {
    for (auto obj : *netlist->variables()) {
      obj->VpiParent(m);
    }
  }
  m->Array_vars(netlist->array_vars());
  if (netlist->array_vars()) {
    for (auto obj : *netlist->array_vars()) {
      obj->VpiParent(m);
    }
  }
  m->Named_events(netlist->named_events());
  if (netlist->named_events()) {
    for (auto obj : *netlist->named_events()) {
      obj->VpiParent(m);
    }
  }
  m->Array_nets(netlist->array_nets());
  if (netlist->array_nets()) {
    for (auto obj : *netlist->array_nets()) {
      obj->VpiParent(m);
    }
  }

  // Cont assigns
  std::vector<cont_assign*>* orig_cont_assigns = mod->getContAssigns();
  if (orig_cont_assigns) {
    std::vector<cont_assign*>* assigns = m->Cont_assigns();
    if (assigns == nullptr) {
      m->Cont_assigns(s.MakeCont_assignVec());
      assigns = m->Cont_assigns();
    }
    for (auto ps : *orig_cont_assigns) {
      assigns->push_back(ps);
      ps->VpiParent(m);
    }
  }

  // Processes
  m->Process(mod->getProcesses());
  if (m->Process()) {
    for (auto ps : *m->Process()) {
      ps->VpiParent(m);
    }
  }

  if (netlist->cont_assigns()) {
    std::vector<cont_assign*>* assigns = m->Cont_assigns();
    if (assigns == nullptr) {
      m->Cont_assigns(s.MakeCont_assignVec());
      assigns = m->Cont_assigns();
    }
    for (auto obj : *netlist->cont_assigns()) {
      obj->VpiParent(m);
      assigns->push_back(obj);
    }
  }

  // Modports
  ModuleDefinition* module = (ModuleDefinition*)mod;
  ModuleDefinition::ModPortSignalMap& orig_modports =
      module->getModPortSignalMap();
  VectorOfmodport* dest_modports = s.MakeModportVec();
  for (auto& orig_modport : orig_modports) {
    modport* dest_modport = s.MakeModport();
    dest_modport->Interface(m);
    dest_modport->VpiName(orig_modport.first);
    dest_modport->VpiParent(m);
    const FileContent* orig_fC = orig_modport.second.getFileContent();
    const NodeId orig_nodeId = orig_modport.second.getNodeId();
    dest_modport->VpiFile(orig_fC->getFileName());
    dest_modport->VpiLineNo(orig_fC->Line(orig_nodeId));
    dest_modport->VpiColumnNo(orig_fC->Column(orig_nodeId));
    dest_modport->VpiEndLineNo(orig_fC->EndLine(orig_nodeId));
    dest_modport->VpiEndColumnNo(orig_fC->EndColumn(orig_nodeId));
    VectorOfio_decl* ios = s.MakeIo_declVec();
    for (auto& sig : orig_modport.second.getPorts()) {
      const FileContent* fC = sig.getFileContent();
      io_decl* io = s.MakeIo_decl();
      io->VpiName(sig.getName());
      NodeId id = sig.getNodeId();
      io->VpiFile(fC->getFileName());
      io->VpiLineNo(fC->Line(id));
      io->VpiColumnNo(fC->Column(id));
      io->VpiEndLineNo(fC->EndLine(id));
      io->VpiEndColumnNo(fC->EndColumn(id));
      if (NodeId Expression = fC->Sibling(id)) {
        any* exp =
            m_helper.compileExpression(mod, fC, Expression, m_compileDesign,
                                       nullptr, instance, true, true);
        io->Expr(exp);
      }
      unsigned int direction = UhdmWriter::getVpiDirection(sig.getDirection());
      io->VpiDirection(direction);
      io->VpiParent(dest_modport);
      ios->push_back(io);
    }
    dest_modport->Io_decls(ios);
    dest_modports->push_back(dest_modport);
  }
  m->Modports(dest_modports);

  if (mod) {
    if (auto from = mod->getTask_funcs()) {
      UHDM::VectorOftask_func* target = m->Task_funcs();
      if (target == nullptr) {
        m->Task_funcs(s.MakeTask_funcVec());
        target = m->Task_funcs();
      }
      for (auto tf : *from) {
        target->push_back(tf);
        if (tf->VpiParent() == 0) tf->VpiParent(m);
        if (tf->Instance() == 0) tf->Instance(m);
      }
    }
  }

  if (mod) {
    lateBinding(s, mod, m);
  }

  return true;
}

void writePrimTerms(ModuleInstance* instance, primitive* prim, int vpiGateType,
                    Serializer& s) {
  Netlist* netlist = instance->getNetlist();
  VectorOfprim_term* terms = s.MakePrim_termVec();
  prim->Prim_terms(terms);
  if (netlist->ports()) {
    unsigned int index = 0;
    VectorOfport* ports = netlist->ports();
    for (auto port : *ports) {
      prim_term* term = s.MakePrim_term();
      terms->push_back(term);
      expr* hconn = (expr*)port->High_conn();
      hconn->VpiParent(prim);
      term->Expr(hconn);
      term->VpiFile(port->VpiFile());
      term->VpiLineNo(port->VpiLineNo());
      term->VpiColumnNo(port->VpiColumnNo());
      term->VpiEndLineNo(port->VpiEndLineNo());
      term->VpiEndColumnNo(port->VpiEndColumnNo());
      term->VpiDirection(port->VpiDirection());
      term->VpiParent(prim);
      term->VpiTermIndex(index);
      if (vpiGateType == vpiBufPrim || vpiGateType == vpiNotPrim) {
        if (index < ports->size() - 1) {
          term->VpiDirection(vpiOutput);
        } else {
          term->VpiDirection(vpiInput);
        }
      } else if (vpiGateType == vpiTranif1Prim ||
                 vpiGateType == vpiTranif0Prim ||
                 vpiGateType == vpiRtranif1Prim ||
                 vpiGateType == vpiRtranif0Prim || vpiGateType == vpiTranPrim ||
                 vpiGateType == vpiRtranPrim) {
        if (index < ports->size() - 1) {
          term->VpiDirection(vpiInout);
        } else {
          term->VpiDirection(vpiInput);
        }
      } else {
        if (index == 0) {
          term->VpiDirection(vpiOutput);
        } else {
          term->VpiDirection(vpiInput);
        }
      }
      index++;
    }
  }
}

void UhdmWriter::writeInstance(ModuleDefinition* mod, ModuleInstance* instance,
                               any* m, CompileDesign* compileDesign,
                               UhdmWriter::ComponentMap& componentMap,
                               UhdmWriter::ModPortMap& modPortMap,
                               UhdmWriter::InstanceMap& instanceMap,
                               ExprBuilder& exprBuilder) {
  Serializer& s = compileDesign->getSerializer();
  VectorOfmodule* subModules = nullptr;
  VectorOfprogram* subPrograms = nullptr;
  VectorOfinterface* subInterfaces = nullptr;
  VectorOfprimitive* subPrimitives = nullptr;
  VectorOfprimitive_array* subPrimitiveArrays = nullptr;
  VectorOfgen_scope_array* subGenScopeArrays = nullptr;

  if (m->UhdmType() == uhdmmodule) {
    writeElabModule(s, instance, (module*)m, exprBuilder);
  } else if (m->UhdmType() == uhdmgen_scope) {
    writeElabGenScope(s, instance, (gen_scope*)m, exprBuilder);
  } else if (m->UhdmType() == uhdminterface) {
    writeElabInterface(s, instance, (interface*)m, exprBuilder);
  }

  for (unsigned int i = 0; i < instance->getNbChildren(); i++) {
    ModuleInstance* child = instance->getChildren(i);
    DesignComponent* childDef = child->getDefinition();
    if (ModuleDefinition* mm =
            valuedcomponenti_cast<ModuleDefinition*>(childDef)) {
      VObjectType insttype = child->getType();
      if (insttype == VObjectType::slModule_instantiation) {
        if (subModules == nullptr) subModules = s.MakeModuleVec();
        module* sm = s.MakeModule();
        if (childDef && !childDef->getFileContents().empty() &&
            compileDesign->getCompiler()->isLibraryFile(
                childDef->getFileContents()[0]->getSymbolId())) {
          sm->VpiCellInstance(true);
        }
        sm->VpiName(child->getInstanceName());
        sm->VpiDefName(child->getModuleName());
        sm->VpiFullName(child->getFullPathName());
        const FileContent* defFile = mm->getFileContents()[0];
        sm->VpiDefFile(defFile->getFileName());
        sm->VpiDefLineNo(defFile->Line(mm->getNodeIds()[0]));
        sm->VpiFile(child->getFileName());
        sm->VpiLineNo(child->getLineNb());
        sm->VpiColumnNo(child->getColumnNb());
        sm->VpiEndLineNo(child->getEndLineNb());
        sm->VpiEndColumnNo(child->getEndColumnNb());
        subModules->push_back(sm);
        if (m->UhdmType() == uhdmmodule) {
          ((module*)m)->Modules(subModules);
          sm->Instance((module*)m);
          sm->Module((module*)m);
          sm->VpiParent(m);
        } else if (m->UhdmType() == uhdmgen_scope) {
          ((gen_scope*)m)->Modules(subModules);
          sm->VpiParent(m);
        }
        writeInstance(mm, child, sm, compileDesign, componentMap, modPortMap,
                      instanceMap, exprBuilder);
      } else if (insttype == VObjectType::slConditional_generate_construct ||
                 insttype == VObjectType::slLoop_generate_construct ||
                 insttype == VObjectType::slGenerate_block ||
                 insttype == VObjectType::slGenerate_item ||
                 insttype == VObjectType::slGenerate_module_loop_statement ||
                 insttype ==
                     VObjectType::slGenerate_module_conditional_statement ||
                 insttype == VObjectType::slGenerate_module_block ||
                 insttype == VObjectType::slGenerate_module_item ||
                 insttype == VObjectType::slGenerate_module_named_block ||
                 insttype == VObjectType::slGenerate_module_block ||
                 insttype == VObjectType::slGenerate_module_item ||
                 insttype == VObjectType::slGenerate_interface_loop_statement ||
                 insttype ==
                     VObjectType::slGenerate_interface_conditional_statement ||
                 insttype == VObjectType::slGenerate_interface_block ||
                 insttype == VObjectType::slGenerate_interface_item ||
                 insttype == VObjectType::slGenerate_interface_named_block ||
                 insttype == VObjectType::slGenerate_interface_block ||
                 insttype == VObjectType::slGenerate_interface_item) {
        if (subGenScopeArrays == nullptr)
          subGenScopeArrays = s.MakeGen_scope_arrayVec();
        gen_scope_array* sm = s.MakeGen_scope_array();
        sm->VpiName(child->getInstanceName());
        sm->VpiFullName(child->getFullPathName());
        sm->VpiFile(child->getFileName());
        sm->VpiLineNo(child->getLineNb());
        sm->VpiColumnNo(child->getColumnNb());
        sm->VpiEndLineNo(child->getEndLineNb());
        sm->VpiEndColumnNo(child->getEndColumnNb());
        subGenScopeArrays->push_back(sm);
        gen_scope* a_gen_scope = s.MakeGen_scope();
        sm->Gen_scopes(s.MakeGen_scopeVec());
        sm->Gen_scopes()->push_back(a_gen_scope);
        a_gen_scope->VpiParent(sm);
        UHDM_OBJECT_TYPE utype = m->UhdmType();
        if (utype == uhdmmodule) {
          ((module*)m)->Gen_scope_arrays(subGenScopeArrays);
          sm->VpiParent(m);
        } else if (utype == uhdmgen_scope) {
          ((gen_scope*)m)->Gen_scope_arrays(subGenScopeArrays);
          sm->VpiParent(m);
        } else if (utype == uhdminterface) {
          ((interface*)m)->Gen_scope_arrays(subGenScopeArrays);
          sm->VpiParent(m);
        }
        writeInstance(mm, child, a_gen_scope, compileDesign, componentMap,
                      modPortMap, instanceMap, exprBuilder);

      } else if (insttype == VObjectType::slInterface_instantiation) {
        if (subInterfaces == nullptr) subInterfaces = s.MakeInterfaceVec();
        interface* sm = s.MakeInterface();
        sm->VpiName(child->getInstanceName());
        sm->VpiDefName(child->getModuleName());
        sm->VpiFullName(child->getFullPathName());
        sm->VpiFile(child->getFileName());
        sm->VpiLineNo(child->getLineNb());
        sm->VpiColumnNo(child->getColumnNb());
        sm->VpiEndLineNo(child->getEndLineNb());
        sm->VpiEndColumnNo(child->getEndColumnNb());
        const FileContent* defFile = mm->getFileContents()[0];
        sm->VpiDefFile(defFile->getFileName());
        sm->VpiDefLineNo(defFile->Line(mm->getNodeIds()[0]));
        subInterfaces->push_back(sm);
        UHDM_OBJECT_TYPE utype = m->UhdmType();
        if (utype == uhdmmodule) {
          ((module*)m)->Interfaces(subInterfaces);
          sm->Instance((module*)m);
          sm->VpiParent(m);
        } else if (utype == uhdmgen_scope) {
          ((gen_scope*)m)->Interfaces(subInterfaces);
          sm->VpiParent(m);
        } else if (utype == uhdminterface) {
          ((interface*)m)->Interfaces(subInterfaces);
          sm->VpiParent(m);
        }
        writeInstance(mm, child, sm, compileDesign, componentMap, modPortMap,
                      instanceMap, exprBuilder);

      } else if ((insttype == VObjectType::slUdp_instantiation) ||
                 (insttype == VObjectType::slGate_instantiation)) {
        UHDM::primitive* gate = nullptr;
        UHDM::primitive_array* gate_array = nullptr;
        const FileContent* fC = child->getFileContent();
        NodeId gatenode = fC->Child(child->getNodeId());
        VObjectType gatetype = fC->Type(gatenode);
        int vpiGateType = getBuiltinType(gatetype);
        if (insttype == VObjectType::slUdp_instantiation) {
          UHDM::udp* udp = s.MakeUdp();
          gate = udp;
          if (ModuleDefinition* mm =
                  valuedcomponenti_cast<ModuleDefinition*>(childDef)) {
            udp->Udp_defn(mm->getUdpDefn());
          }
          if (UHDM::VectorOfrange* ranges = child->getNetlist()->ranges()) {
            gate_array = s.MakeUdp_array();
            VectorOfprimitive* prims = s.MakePrimitiveVec();
            gate_array->Primitives(prims);
            gate_array->Ranges(ranges);
            prims->push_back(gate);
            if (subPrimitiveArrays == nullptr)
              subPrimitiveArrays = s.MakePrimitive_arrayVec();
            subPrimitiveArrays->push_back(gate_array);
          } else {
            if (subPrimitives == nullptr) subPrimitives = s.MakePrimitiveVec();
            subPrimitives->push_back(gate);
          }
        } else if (vpiGateType == vpiPmosPrim || vpiGateType == vpiRpmosPrim ||
                   vpiGateType == vpiNmosPrim || vpiGateType == vpiRnmosPrim ||
                   vpiGateType == vpiCmosPrim || vpiGateType == vpiRcmosPrim ||
                   vpiGateType == vpiTranif1Prim ||
                   vpiGateType == vpiTranif0Prim ||
                   vpiGateType == vpiRtranif1Prim ||
                   vpiGateType == vpiRtranif0Prim ||
                   vpiGateType == vpiTranPrim || vpiGateType == vpiRtranPrim) {
          gate = s.MakeSwitch_tran();
          if (UHDM::VectorOfrange* ranges = child->getNetlist()->ranges()) {
            gate_array = s.MakeSwitch_array();
            VectorOfprimitive* prims = s.MakePrimitiveVec();
            gate_array->Primitives(prims);
            gate_array->Ranges(ranges);
            prims->push_back(gate);
            if (subPrimitiveArrays == nullptr)
              subPrimitiveArrays = s.MakePrimitive_arrayVec();
            subPrimitiveArrays->push_back(gate_array);
          } else {
            if (subPrimitives == nullptr) subPrimitives = s.MakePrimitiveVec();
            subPrimitives->push_back(gate);
          }
          gate->VpiPrimType(vpiGateType);
        } else {
          gate = s.MakeGate();
          if (UHDM::VectorOfrange* ranges = child->getNetlist()->ranges()) {
            gate_array = s.MakeGate_array();
            gate_array->VpiName(child->getInstanceName());
            gate_array->VpiFullName(child->getFullPathName());
            gate_array->VpiFile(child->getFileName());
            gate_array->VpiLineNo(child->getLineNb());
            gate_array->VpiColumnNo(child->getColumnNb());
            gate_array->VpiEndLineNo(child->getEndLineNb());
            gate_array->VpiEndColumnNo(child->getEndColumnNb());
            VectorOfprimitive* prims = s.MakePrimitiveVec();
            gate_array->Primitives(prims);
            gate_array->Ranges(ranges);
            prims->push_back(gate);
            if (subPrimitiveArrays == nullptr)
              subPrimitiveArrays = s.MakePrimitive_arrayVec();
            subPrimitiveArrays->push_back(gate_array);
          } else {
            if (subPrimitives == nullptr) subPrimitives = s.MakePrimitiveVec();
            subPrimitives->push_back(gate);
          }
          gate->VpiPrimType(vpiGateType);
        }
        if (UHDM::VectorOfexpr* delays = child->getNetlist()->delays()) {
          if (delays->size() == 1) {
            gate->Delay((*delays)[0]);
          }
        }

        gate->VpiName(child->getInstanceName());
        gate->VpiDefName(child->getModuleName());
        gate->VpiFullName(child->getFullPathName());
        gate->VpiFile(child->getFileName());
        gate->VpiLineNo(child->getLineNb());
        gate->VpiColumnNo(child->getColumnNb());
        gate->VpiEndLineNo(child->getEndLineNb());
        gate->VpiEndColumnNo(child->getEndColumnNb());
        UHDM_OBJECT_TYPE utype = m->UhdmType();
        if (utype == uhdmmodule) {
          ((module*)m)->Primitives(subPrimitives);
          ((module*)m)->Primitive_arrays(subPrimitiveArrays);
          gate->VpiParent(m);
        } else if (utype == uhdmgen_scope) {
          ((gen_scope*)m)->Primitives(subPrimitives);
          ((gen_scope*)m)->Primitive_arrays(subPrimitiveArrays);
          gate->VpiParent(m);
        }
        writePrimTerms(child, gate, vpiGateType, s);
      } else {
        // Unknown object type
      }
    } else if (Program* prog = valuedcomponenti_cast<Program*>(childDef)) {
      if (subPrograms == nullptr) subPrograms = s.MakeProgramVec();
      program* sm = s.MakeProgram();
      sm->VpiName(child->getInstanceName());
      sm->VpiDefName(child->getModuleName());
      sm->VpiFullName(child->getFullPathName());
      sm->VpiFile(child->getFileName());
      sm->VpiLineNo(child->getLineNb());
      sm->VpiColumnNo(child->getColumnNb());
      sm->VpiEndLineNo(child->getEndLineNb());
      sm->VpiEndColumnNo(child->getEndColumnNb());
      const FileContent* defFile = prog->getFileContents()[0];
      sm->VpiDefFile(defFile->getFileName());
      sm->VpiDefLineNo(defFile->Line(prog->getNodeIds()[0]));
      subPrograms->push_back(sm);
      UHDM_OBJECT_TYPE utype = m->UhdmType();
      if (utype == uhdmmodule) {
        ((module*)m)->Programs(subPrograms);
        sm->Instance((module*)m);
        sm->VpiParent(m);
      } else if (utype == uhdmgen_scope) {
        ((gen_scope*)m)->Programs(subPrograms);
        sm->VpiParent(m);
      }
      writeElabProgram(s, child, sm);
    } else {
      // Undefined module
      if (subModules == nullptr) subModules = s.MakeModuleVec();
      module* sm = s.MakeModule();
      sm->VpiName(child->getInstanceName());
      sm->VpiDefName(child->getModuleName());
      sm->VpiFullName(child->getFullPathName());
      sm->VpiFile(child->getFileName());
      sm->VpiLineNo(child->getLineNb());
      sm->VpiColumnNo(child->getColumnNb());
      sm->VpiEndLineNo(child->getEndLineNb());
      sm->VpiEndColumnNo(child->getEndColumnNb());
      subModules->push_back(sm);
      UHDM_OBJECT_TYPE utype = m->UhdmType();
      if (utype == uhdmmodule) {
        ((module*)m)->Modules(subModules);
        sm->Instance((module*)m);
        sm->Module((module*)m);
        sm->VpiParent(m);
      } else if (utype == uhdmgen_scope) {
        ((gen_scope*)m)->Modules(subModules);
        sm->VpiParent(m);
      }
      writeInstance(mm, child, sm, compileDesign, componentMap, modPortMap,
                    instanceMap, exprBuilder);
    }
  }
}

void printUhdmStats(Serializer& s) {
  std::cout << "UHDM Objects Stats:\n";
  std::map<std::string, unsigned long> stats = s.ObjectStats();
  std::multimap<unsigned long, std::string> rstats;
  for (const auto& stat : stats) {
    if (stat.second) rstats.insert(std::make_pair(stat.second, stat.first));
  }
  for (const auto& stat : rstats) {
    std::cout << stat.second << " " << stat.first << "\n";
  }
  std::cout << "\n";
}

vpiHandle UhdmWriter::write(const std::string& uhdmFile) {
  ComponentMap componentMap;
  ModPortMap modPortMap;
  InstanceMap instanceMap;
  Serializer& s = m_compileDesign->getSerializer();
  ExprBuilder exprBuilder;
  exprBuilder.seterrorReporting(
      m_compileDesign->getCompiler()->getErrorContainer(),
      m_compileDesign->getCompiler()->getSymbolTable());

  Location loc(m_compileDesign->getCompiler()->getSymbolTable()->registerSymbol(
      uhdmFile));
  Error err(ErrorDefinition::UHDM_CREATING_MODEL, loc);
  m_compileDesign->getCompiler()->getErrorContainer()->addError(err);
  m_compileDesign->getCompiler()->getErrorContainer()->printMessages(
      m_compileDesign->getCompiler()->getCommandLineParser()->muteStdout());

  vpiHandle designHandle = 0;
  std::vector<vpiHandle> designs;
  if (m_design) {
    design* d = s.MakeDesign();
    designHandle = reinterpret_cast<vpiHandle>(new uhdm_handle(uhdmdesign, d));
    std::string designName = "unnamed";
    auto topLevelModules = m_design->getTopLevelModuleInstances();
    for (auto inst : topLevelModules) {
      designName = inst->getModuleName();
      break;
    }
    d->VpiName(designName);
    designs.push_back(designHandle);
    // -------------------------------
    // Non-Elaborated Model

    // FileContent
    VectorOftypespec* typespecs = s.MakeTypespecVec();
    d->Typespecs(typespecs);
    for (auto& fileIdContent : m_design->getAllFileContents()) {
      // Typepecs
      writeDataTypes(fileIdContent.second->getDataTypeMap(), d, typespecs, s,
                     true);

      // Function and tasks
      if (auto from = fileIdContent.second->getTask_funcs()) {
        UHDM::VectorOftask_func* target = d->Task_funcs();
        if (target == nullptr) {
          d->Task_funcs(s.MakeTask_funcVec());
          target = d->Task_funcs();
        }
        for (auto tf : *from) {
          target->push_back(tf);
          if (tf->VpiParent() == 0) tf->VpiParent(d);
        }
      }

      // Parameters
      if (auto from = fileIdContent.second->getParameters()) {
        UHDM::VectorOfany* target = d->Parameters();
        if (target == nullptr) {
          d->Parameters(s.MakeAnyVec());
          target = d->Parameters();
        }
        for (auto tf : *from) {
          target->push_back(tf);
          if (tf->VpiParent() == 0) tf->VpiParent(d);
        }
      }

      // Param assigns
      if (auto from = fileIdContent.second->getParam_assigns()) {
        UHDM::VectorOfparam_assign* target = d->Param_assigns();
        if (target == nullptr) {
          d->Param_assigns(s.MakeParam_assignVec());
          target = d->Param_assigns();
        }
        for (auto tf : *from) {
          target->push_back(tf);
          if (tf->VpiParent() == 0) tf->VpiParent(d);
        }
      }
    }

    // Packages
    SURELOG::PackageDefinitionVec packages =
        m_design->getOrderedPackageDefinitions();
    for (auto& pack : m_design->getPackageDefinitions()) {
      if (pack.first == "builtin") {
        if (pack.second) packages.insert(packages.begin(), pack.second);
        break;
      }
    }

    VectorOfpackage* v2 = s.MakePackageVec();
    for (Package* pack : packages) {
      if (!pack) continue;
      if (!pack->getFileContents().empty() &&
          pack->getType() == VObjectType::slPackage_declaration) {
        const FileContent* fC = pack->getFileContents()[0];
        package* p = (package*)pack->getUhdmInstance();
        componentMap.insert(std::make_pair(pack, p));
        p->VpiParent(d);
        p->VpiTop(true);
        p->VpiDefName(pack->getName());
        p->Attributes(pack->Attributes());
        writePackage(pack, p, s, componentMap, true);
        if (fC) {
          // Builtin package has no file
          p->VpiFile(fC->getFileName());
          const NodeId modId = pack->getNodeIds()[0];
          const NodeId startId = fC->sl_collect(modId, VObjectType::slPackage);
          p->VpiLineNo(fC->Line(startId));
          p->VpiColumnNo(fC->Column(startId));
          p->VpiEndLineNo(fC->EndLine(modId));
          p->VpiEndColumnNo(fC->EndColumn(modId));
        }
        v2->push_back(p);
      }
    }
    d->TopPackages(v2);

    VectorOfpackage* v3 = s.MakePackageVec();
    d->AllPackages(v3);
    for (Package* pack : packages) {
      if (!pack) continue;
      if (!pack->getFileContents().empty() &&
          pack->getType() == VObjectType::slPackage_declaration) {
        const FileContent* fC = pack->getFileContents()[0];
        package* p = s.MakePackage();
        p->VpiName(pack->getName());
        p->VpiParent(d);
        p->VpiDefName(pack->getName());
        p->Attributes(pack->Attributes());
        v3->push_back(p);
        writePackage(pack, p, s, componentMap, false);
        if (fC) {
          // Builtin package has no file
          p->VpiFile(fC->getFileName());
          const NodeId modId = pack->getNodeIds()[0];
          const NodeId startId = fC->sl_collect(modId, VObjectType::slPackage);
          p->VpiLineNo(fC->Line(startId));
          p->VpiColumnNo(fC->Column(startId));
          p->VpiEndLineNo(fC->EndLine(modId));
          p->VpiEndColumnNo(fC->EndColumn(modId));
        }
      }
    }

    // Programs
    auto programs = m_design->getProgramDefinitions();
    VectorOfprogram* uhdm_programs = s.MakeProgramVec();
    for (const auto& progNamePair : programs) {
      Program* prog = progNamePair.second;
      if (!prog->getFileContents().empty() &&
          prog->getType() == VObjectType::slProgram_declaration) {
        const FileContent* fC = prog->getFileContents()[0];
        program* p = s.MakeProgram();
        componentMap.insert(std::make_pair(prog, p));
        p->VpiParent(d);
        p->VpiDefName(prog->getName());
        p->VpiFile(fC->getFileName());
        const NodeId modId = prog->getNodeIds()[0];
        const NodeId startId = fC->sl_collect(modId, VObjectType::slProgram);
        p->VpiLineNo(fC->Line(startId));
        p->VpiColumnNo(fC->Column(startId));
        p->VpiEndLineNo(fC->EndLine(modId));
        p->VpiEndColumnNo(fC->EndColumn(modId));
        p->Attributes(prog->Attributes());
        writeProgram(prog, p, s, componentMap, modPortMap);
        uhdm_programs->push_back(p);
      }
    }
    d->AllPrograms(uhdm_programs);

    // Interfaces
    auto modules = m_design->getModuleDefinitions();
    VectorOfinterface* uhdm_interfaces = s.MakeInterfaceVec();
    for (const auto& modNamePair : modules) {
      ModuleDefinition* mod = modNamePair.second;
      if (mod->getFileContents().empty()) {
        // Built-in primitive
      } else if (mod->getType() == VObjectType::slInterface_declaration) {
        const FileContent* fC = mod->getFileContents()[0];
        interface* m = s.MakeInterface();
        componentMap.insert(std::make_pair(mod, m));
        m->VpiParent(d);
        m->VpiDefName(mod->getName());
        m->VpiFile(fC->getFileName());
        const NodeId modId = mod->getNodeIds()[0];
        const NodeId startId = fC->sl_collect(modId, VObjectType::slInterface);
        m->VpiLineNo(fC->Line(startId));
        m->VpiColumnNo(fC->Column(startId));
        m->VpiEndLineNo(fC->EndLine(modId));
        m->VpiEndColumnNo(fC->EndColumn(modId));
        m->Attributes(mod->Attributes());
        uhdm_interfaces->push_back(m);
        writeInterface(mod, m, s, componentMap, modPortMap);
      }
    }
    d->AllInterfaces(uhdm_interfaces);

    // Modules
    VectorOfmodule* uhdm_modules = s.MakeModuleVec();
    // Udps
    VectorOfudp_defn* uhdm_udps = s.MakeUdp_defnVec();
    for (const auto& modNamePair : modules) {
      ModuleDefinition* mod = modNamePair.second;
      if (mod->getFileContents().empty()) {
        // Built-in primitive
      } else if (mod->getType() == VObjectType::slModule_declaration) {
        const FileContent* fC = mod->getFileContents()[0];
        module* m = s.MakeModule();
        if (m_compileDesign->getCompiler()->isLibraryFile(
                mod->getFileContents()[0]->getSymbolId())) {
          m->VpiCellInstance(true);
        }
        componentMap.insert(std::make_pair(mod, m));
        m->VpiParent(d);
        m->VpiDefName(mod->getName());
        m->Attributes(mod->Attributes());
        m->VpiFile(fC->getFileName());
        const NodeId modId = mod->getNodeIds()[0];
        const NodeId startId =
            fC->sl_collect(modId, VObjectType::slModule_keyword);
        m->VpiLineNo(fC->Line(startId));
        m->VpiColumnNo(fC->Column(startId));
        m->VpiEndLineNo(fC->EndLine(modId));
        m->VpiEndColumnNo(fC->EndColumn(modId));
        uhdm_modules->push_back(m);
        writeModule(mod, m, s, componentMap, modPortMap);
      } else if (mod->getType() == VObjectType::slUdp_declaration) {
        const FileContent* fC = mod->getFileContents()[0];
        UHDM::udp_defn* defn = mod->getUdpDefn();
        if (defn) {
          defn->VpiParent(d);
          defn->VpiDefName(mod->getName());
          defn->VpiFile(fC->getFileName());
          const NodeId modId = mod->getNodeIds()[0];
          const NodeId startId =
              fC->sl_collect(modId, VObjectType::slPrimitive);
          defn->VpiLineNo(fC->Line(startId));
          defn->VpiColumnNo(fC->Column(startId));
          defn->VpiEndLineNo(fC->EndLine(modId));
          defn->VpiEndColumnNo(fC->EndColumn(modId));
          defn->Attributes(mod->Attributes());
          uhdm_udps->push_back(defn);
        }
      }
    }
    d->AllModules(uhdm_modules);
    d->AllUdps(uhdm_udps);

    // Classes
    auto classes = m_design->getClassDefinitions();
    VectorOfclass_defn* v4 = s.MakeClass_defnVec();
    for (const auto& classNamePair : classes) {
      ClassDefinition* classDef = classNamePair.second;
      if (!classDef->getFileContents().empty() &&
          classDef->getType() == VObjectType::slClass_declaration) {
        class_defn* c = classDef->getUhdmDefinition();
        if (!c->VpiParent()) {
          writeClass(classDef, v4, s, componentMap, d);
        }
      }
    }
    d->AllClasses(v4);

    // -------------------------------
    // Elaborated Model (Folded)

    // Top-level modules
    VectorOfmodule* uhdm_top_modules = s.MakeModuleVec();
    for (ModuleInstance* inst : topLevelModules) {
      DesignComponent* component = inst->getDefinition();
      ModuleDefinition* mod =
          valuedcomponenti_cast<ModuleDefinition*>(component);
      const auto& itr = componentMap.find(mod);
      module* m = s.MakeModule();
      m->VpiTopModule(true);
      m->VpiTop(true);
      module* def = (module*)itr->second;
      m->VpiDefName(def->VpiDefName());
      m->VpiName(def->VpiDefName());  // Top's instance name is module name
      m->VpiFullName(
          def->VpiDefName());  // Top's full instance name is module name
      m->VpiFile(def->VpiFile());
      m->VpiLineNo(def->VpiLineNo());
      m->VpiColumnNo(def->VpiColumnNo());
      m->VpiEndLineNo(def->VpiEndLineNo());
      m->VpiEndColumnNo(def->VpiEndColumnNo());
      writeInstance(mod, inst, m, m_compileDesign, componentMap, modPortMap,
                    instanceMap, exprBuilder);
      uhdm_top_modules->push_back(m);
    }
    d->TopModules(uhdm_top_modules);
  }

  if (m_compileDesign->getCompiler()->getCommandLineParser()->getUhdmStats())
    printUhdmStats(s);

  // ----------------------------------
  // Fully elaborated model
  if (m_compileDesign->getCompiler()->getCommandLineParser()->getElabUhdm()) {
    Error err(ErrorDefinition::UHDM_ELABORATION, loc);
    m_compileDesign->getCompiler()->getErrorContainer()->addError(err);
    m_compileDesign->getCompiler()->getErrorContainer()->printMessages(
        m_compileDesign->getCompiler()->getCommandLineParser()->muteStdout());

    ElaboratorListener* listener = new ElaboratorListener(&s, false);
    listener->uniquifyTypespec(false);
    listen_designs(designs, listener);
    delete listener;
  }

  // Lint only the elaborated model
  UhdmLint* linter = new UhdmLint(&s);
  listen_designs(designs, linter);
  delete linter;

  if (m_compileDesign->getCompiler()
          ->getCommandLineParser()
          ->reportNonSynthesizable()) {
    std::set<const any*> nonSynthesizableObjects;
    SynthSubset* annotate = new SynthSubset(&s, nonSynthesizableObjects, true);
    listen_designs(designs, annotate);
    delete annotate;
  }

  if (m_compileDesign->getCompiler()->getCommandLineParser()->getUhdmStats())
    printUhdmStats(s);

  if (m_compileDesign->getCompiler()->getCommandLineParser()->writeUhdm()) {
    {
      Error err(ErrorDefinition::UHDM_WRITE_DB, loc);
      m_compileDesign->getCompiler()->getErrorContainer()->addError(err);
      m_compileDesign->getCompiler()->getErrorContainer()->printMessages(
          m_compileDesign->getCompiler()->getCommandLineParser()->muteStdout());
    }

    s.Save(uhdmFile);
  }

  if (m_compileDesign->getCompiler()->getCommandLineParser()->getDebugUhdm() ||
      m_compileDesign->getCompiler()->getCommandLineParser()->getCoverUhdm()) {
    // Check before restore
    Location loc(
        m_compileDesign->getCompiler()->getSymbolTable()->registerSymbol(
            std::string(uhdmFile) + ".chk.html"));
    Error err(ErrorDefinition::UHDM_WRITE_HTML_COVERAGE, loc);
    m_compileDesign->getCompiler()->getErrorContainer()->addError(err);
    m_compileDesign->getCompiler()->getErrorContainer()->printMessages(
        m_compileDesign->getCompiler()->getCommandLineParser()->muteStdout());

    UhdmChecker* uhdmchecker = new UhdmChecker(m_compileDesign, m_design);
    uhdmchecker->check(std::string(uhdmFile) + ".chk");
    delete uhdmchecker;
  }
  if (m_compileDesign->getCompiler()->getCommandLineParser()->getDebugUhdm()) {
    if (m_compileDesign->getCompiler()->getCommandLineParser()->writeUhdm()) {
      Location loc(
          m_compileDesign->getCompiler()->getSymbolTable()->registerSymbol(
              uhdmFile));
      Error err1(ErrorDefinition::UHDM_LOAD_DB, loc);
      m_compileDesign->getCompiler()->getErrorContainer()->addError(err1);
      m_compileDesign->getCompiler()->getErrorContainer()->printMessages(
          m_compileDesign->getCompiler()->getCommandLineParser()->muteStdout());
      const std::vector<vpiHandle>& restoredDesigns = s.Restore(uhdmFile);

      Error err2(ErrorDefinition::UHDM_VISITOR, loc);
      m_compileDesign->getCompiler()->getErrorContainer()->addError(err2);
      m_compileDesign->getCompiler()->getErrorContainer()->printMessages(
          m_compileDesign->getCompiler()->getCommandLineParser()->muteStdout());

      std::cout << "====== UHDM =======\n";
      if (!restoredDesigns.empty()) {
        designHandle = restoredDesigns[0];
      }
      vpi_show_ids(
          m_compileDesign->getCompiler()->getCommandLineParser()->showVpiIds());
      std::string restored = visit_designs(restoredDesigns);
      std::cout << restored;
      std::cout << "===================\n";
    } else {
      Location loc(
          m_compileDesign->getCompiler()->getSymbolTable()->registerSymbol(
              "in-memory uhdm"));
      Error err2(ErrorDefinition::UHDM_VISITOR, loc);
      m_compileDesign->getCompiler()->getErrorContainer()->addError(err2);
      m_compileDesign->getCompiler()->getErrorContainer()->printMessages(
          m_compileDesign->getCompiler()->getCommandLineParser()->muteStdout());
      std::cout << "====== UHDM =======\n";
      vpi_show_ids(
          m_compileDesign->getCompiler()->getCommandLineParser()->showVpiIds());
      std::string result = visit_designs({designHandle});
      std::cout << result;
      std::cout << "===================\n";
    }
  }
  m_compileDesign->getCompiler()->getErrorContainer()->printMessages(
      m_compileDesign->getCompiler()->getCommandLineParser()->muteStdout());
  return designHandle;
}
}  // namespace SURELOG
