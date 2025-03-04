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
 * File:   DesignComponent.h
 * Author: alain
 *
 * Created on March 25, 2018, 10:27 PM
 */

#ifndef SURELOG_DESIGNCOMPONENT_H
#define SURELOG_DESIGNCOMPONENT_H
#pragma once

#include <Surelog/Common/PortNetHolder.h>
#include <Surelog/Common/SymbolId.h>
#include <Surelog/Design/FileCNodeId.h>
#include <Surelog/Design/ValuedComponentI.h>
#include <Surelog/Design/LetStmt.h>
#include <Surelog/SourceCompile/VObjectTypes.h>

// UHDM
#include <uhdm/uhdm_forward_decl.h>

#include <filesystem>

namespace SURELOG {

class DataType;
class DesignElement;
class FileContent;
class Function;
class Package;
class ParamAssign;
class Parameter;
class Task;
class TypeDef;
class Variable;

class ExprEval {
 public:
  ExprEval(UHDM::expr* expr, ValuedComponentI* instance,
           const std::filesystem::path& fileName, int lineNumber,
           UHDM::any* pexpr)
      : m_expr(expr),
        m_instance(instance),
        m_fileName(fileName),
        m_lineNumber(lineNumber),
        m_pexpr(pexpr) {}
  UHDM::expr* m_expr;
  ValuedComponentI* m_instance;
  std::filesystem::path m_fileName;
  int m_lineNumber;
  UHDM::any* m_pexpr;
};

class DesignComponent : public ValuedComponentI, public PortNetHolder {
  SURELOG_IMPLEMENT_RTTI(DesignComponent, ValuedComponentI)
 public:
  DesignComponent(const DesignComponent* parent, DesignComponent* definition)
      : ValuedComponentI(parent, definition), m_instance(nullptr) {}
  ~DesignComponent() override {}

  virtual unsigned int getSize() const = 0;
  virtual VObjectType getType() const = 0;
  virtual bool isInstance() const = 0;
  virtual const std::string& getName() const = 0;
  void append(DesignComponent*);

  typedef std::map<std::string, DataType*> DataTypeMap;
  typedef std::map<std::string, TypeDef*> TypeDefMap;
  typedef std::vector<DataType*> DataTypeVec;
  typedef std::vector<TypeDef*> TypeDefVec;
  typedef std::map<std::string, Function*> FunctionMap;
  typedef std::map<std::string, Task*> TaskMap;
  typedef std::map<std::string, Variable*> VariableMap;
  typedef std::map<std::string, Parameter*> ParameterMap;
  typedef std::vector<Parameter*> ParameterVec;
  typedef std::vector<ParamAssign*> ParamAssignVec;

  void addFileContent(const FileContent* fileContent, NodeId nodeId);
  const std::vector<const FileContent*>& getFileContents() const {
    return m_fileContents;
  }
  const std::vector<NodeId>& getNodeIds() const { return m_nodeIds; }

  // Precompiled Object of interest
  const std::vector<FileCNodeId>& getObjects(VObjectType type) const;
  void addObject(VObjectType type, FileCNodeId object);

  void addNamedObject(const std::string& name, FileCNodeId object,
                      DesignComponent* def = nullptr);
  std::map<std::string, std::pair<FileCNodeId, DesignComponent*>>&
  getNamedObjects() {
    return m_namedObjects;
  }
  const std::pair<FileCNodeId, DesignComponent*>* getNamedObject(
      const std::string& name) const;

  DataTypeMap& getUsedDataTypeMap() { return m_usedDataTypes; }
  DataType* getUsedDataType(const std::string& name);
  void insertUsedDataType(const std::string& dataTypeName, DataType* dataType);

  const DataTypeMap& getDataTypeMap() const { return m_dataTypes; }
  const DataType* getDataType(const std::string& name) const;
  void insertDataType(const std::string& dataTypeName, DataType* dataType);

  const TypeDefMap& getTypeDefMap() const { return m_typedefs; }
  const TypeDef* getTypeDef(const std::string& name) const;
  void insertTypeDef(TypeDef* p);

  FunctionMap& getFunctionMap() { return m_functions; }
  virtual Function* getFunction(const std::string& name) const;
  void insertFunction(Function* p);

  TaskMap& getTaskMap() { return m_tasks; }
  virtual Task* getTask(const std::string& name) const;
  void insertTask(Task* p);

  void addAccessPackage(Package* p) { m_packages.push_back(p); }
  const std::vector<Package*>& getAccessPackages() const { return m_packages; }

  void addVariable(Variable* var);
  const VariableMap& getVariables() const { return m_variables; }
  Variable* getVariable(const std::string& name);

  const ParameterMap& getParameterMap() const { return m_parameterMap; }
  Parameter* getParameter(const std::string& name) const;
  void insertParameter(Parameter* p);
  const ParameterVec& getOrderedParameters() const {
    return m_orderedParameters;
  }

  void addParamAssign(ParamAssign* assign) { m_paramAssigns.push_back(assign); }
  const ParamAssignVec& getParamAssignVec() const { return m_paramAssigns; }

  void addImportedSymbol(UHDM::import* i) { m_imported_symbols.push_back(i); }
  const std::vector<UHDM::import*>& getImportedSymbols() const {
    return m_imported_symbols;
  }

  void needLateBinding(UHDM::ref_obj* obj) { m_needLateBinding.push_back(obj); }
  const std::vector<UHDM::ref_obj*>& getLateBinding() const {
    return m_needLateBinding;
  }

  void needLateTypedefBinding(UHDM::any* obj) {
    m_needLateTypedefBinding.push_back(obj);
  }
  const std::vector<UHDM::any*>& getLateTypedefBinding() const {
    return m_needLateTypedefBinding;
  }
  void lateBinding(bool on) { m_lateBinding = on; }

  void setUhdmInstance(UHDM::instance* instance) { m_instance = instance; }
  UHDM::instance* getUhdmInstance() { return m_instance; }
  void scheduleParamExprEval(const std::string& name, ExprEval& expr_eval) {
    m_scheduledParamExprEval.push_back(std::make_pair(name, expr_eval));
  }
  std::vector<std::pair<std::string, ExprEval>>& getScheduledParamExprEval() {
    return m_scheduledParamExprEval;
  }
  void setDesignElement(const DesignElement* elem) { m_designElement = elem; }
  const DesignElement* getDesignElement() { return m_designElement; }
  
  void insertLetStmt(const std::string& name, LetStmt* decl);
  LetStmt* getLetStmt(const std::string& name);
  std::map<std::string, LetStmt*>& getLetStmts() { return m_letDecls; }
 protected:
  std::vector<const FileContent*> m_fileContents;
  std::vector<NodeId> m_nodeIds;
  FunctionMap m_functions;
  TaskMap m_tasks;

 private:
  std::map<VObjectType, std::vector<FileCNodeId>> m_objects;
  std::map<std::string, std::pair<FileCNodeId, DesignComponent*>>
      m_namedObjects;
  std::vector<FileCNodeId> m_empty;

 protected:
  DataTypeMap m_dataTypes;

 private:
  DataTypeMap m_usedDataTypes;
  TypeDefMap m_typedefs;
  std::vector<Package*> m_packages;
  VariableMap m_variables;
  std::vector<UHDM::import*> m_imported_symbols;
  std::vector<UHDM::ref_obj*> m_needLateBinding;
  std::vector<UHDM::any*> m_needLateTypedefBinding;
  ParameterMap m_parameterMap;
  ParameterVec m_orderedParameters;
  ParamAssignVec m_paramAssigns;
  UHDM::instance* m_instance;
  std::vector<std::pair<std::string, ExprEval>> m_scheduledParamExprEval;
  const DesignElement* m_designElement = nullptr;
  std::map<std::string, LetStmt*> m_letDecls;
  bool m_lateBinding = true;
};

};  // namespace SURELOG

#endif /* SURELOG_DESIGNCOMPONENT_H */
