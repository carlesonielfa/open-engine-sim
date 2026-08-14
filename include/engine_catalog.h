#ifndef ATG_ENGINE_SIM_ENGINE_CATALOG_H
#define ATG_ENGINE_SIM_ENGINE_CATALOG_H

#include <string>
#include <vector>

struct EngineCatalogEntry {
    std::string group;
    std::string name;
    std::string relativeScriptPath;
};

const std::vector<EngineCatalogEntry> &engineCatalog();

#endif /* ATG_ENGINE_SIM_ENGINE_CATALOG_H */
