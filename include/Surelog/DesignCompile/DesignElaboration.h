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
 * File:   DesignElaboration.h
 * Author: alain
 *
 * Created on July 12, 2017, 8:55 PM
 */

#ifndef SURELOG_DESIGNELABORATION_H
#define SURELOG_DESIGNELABORATION_H
#pragma once

#include <Surelog/Config/Config.h>
#include <Surelog/DesignCompile/TestbenchElaboration.h>

namespace SURELOG {

class BindStmt;
class ModuleDefinitionFactory;
class ModuleInstanceFactory;

class DesignElaboration : public TestbenchElaboration {
 public:
  DesignElaboration(CompileDesign* compileDesign);
  DesignElaboration(const DesignElaboration& orig) = delete;
  ~DesignElaboration() override;

  bool createModuleAndPackageDefinitions();

  bool elaborate() override;

 private:
  bool bindDataTypes_() override;
  bool bindPackagesDataTypes_();
  bool bindDataTypes_(ModuleInstance* instance, DesignComponent* component);
  void bind_ports_nets_(std::vector<Signal*>& ports,
                        std::vector<Signal*>& signals, const FileContent* fC,
                        ModuleInstance* instance, DesignComponent* mod);
  bool createBuiltinPrimitives_();
  bool setupConfigurations_();
  bool identifyTopModules_();
  bool elaborateAllModules_(bool onlyTopLevel);
  void reportElaboration_();
  bool elaborateModule_(const std::string& moduleName, const FileContent* fileContent,
                        bool onlyTopLevel);
  void checkElaboration_();
  void collectParams_(std::vector<std::string>& params, const FileContent* fC,
                      NodeId nodeId, ModuleInstance* instance,
                      NodeId parentParamOverride);
  void elaborateInstance_(const FileContent* fC, NodeId nodeId,
                          NodeId parentParamOverride,
                          ModuleInstanceFactory* factory,
                          ModuleInstance* parent, Config* config,
                          std::vector<ModuleInstance*>& parentSubInstances);
  void recurseInstanceLoop_(std::vector<int>& from, std::vector<int>& to,
                            std::vector<int>& indexes, unsigned int pos,
                            DesignComponent* def, const FileContent* fC,
                            NodeId subInstanceId, NodeId paramOverride,
                            ModuleInstanceFactory* factory,
                            ModuleInstance* parent, Config* config,
                            std::string instanceName, const std::string& modName,
                            std::vector<ModuleInstance*>& allSubInstances);
  void recurseBuildInstanceClause_(const std::string& parentPath, Config* config,
                                   std::set<Config*>& stack);
  ModuleInstance* createBindInstance_(BindStmt* bind, ModuleInstance* parent,
                                      ModuleInstanceFactory* factory,
                                      Config* config);
  void reduceUnnamedBlocks_();
  void checkConfigurations_();
  bool bindAllInstances_(ModuleInstance*, ModuleInstanceFactory* factory,
                         Config* config);
  void createFileList_();
  Config* getInstConfig(const std::string& name);
  Config* getCellConfig(const std::string& name);
  std::vector<std::pair<std::string, const FileContent*>> m_topLevelModules;
  std::set<std::string> m_uniqueTopLevelModules;
  ModuleDefinitionFactory* m_moduleDefFactory;
  ModuleInstanceFactory* m_moduleInstFactory;
  std::set<std::string> m_toplevelConfigModules;
  std::map<std::string, Config> m_instConfig;
  std::map<std::string, Config> m_cellConfig;
  std::map<std::string, UseClause> m_instUseClause;
  std::map<std::string, UseClause> m_cellUseClause;
};

};  // namespace SURELOG

#endif /* SURELOG_DESIGNELABORATION_H */
