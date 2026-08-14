#include "../include/engine_catalog.h"

#include <algorithm>
#include <cctype>

namespace {
std::string titleize(std::string value) {
    std::replace(value.begin(), value.end(), '_', ' ');
    std::replace(value.begin(), value.end(), '-', ' ');
    bool capitalize = true;
    for (char &character : value) {
        if (character == ' ') {
            capitalize = true;
        }
        else if (capitalize) {
            character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            capitalize = false;
        }
    }
    return value;
}

EngineCatalogEntry makeEntry(const char *relativePath) {
    const std::string path(relativePath);
    const size_t groupStart = std::string("engines/").size();
    const size_t groupEnd = path.find('/', groupStart);
    const size_t filenameStart = path.rfind('/') + 1;
    std::string filename = path.substr(filenameStart);
    filename.erase(filename.size() - 3); // .mr
    const size_t numericPrefixEnd = filename.find('_');
    if (numericPrefixEnd != std::string::npos
        && std::all_of(filename.begin(), filename.begin() + numericPrefixEnd,
            [](unsigned char c) { return std::isdigit(c); }))
    {
        filename.erase(0, numericPrefixEnd + 1);
    }
    return {
        titleize(path.substr(groupStart, groupEnd - groupStart)),
        titleize(filename),
        path
    };
}
}

const std::vector<EngineCatalogEntry> &engineCatalog() {
    static const std::vector<EngineCatalogEntry> catalog = [] {
        const char *paths[] = {
#include "engine_catalog_paths.inc"
        };
        std::vector<EngineCatalogEntry> result;
        result.reserve(sizeof(paths) / sizeof(paths[0]));
        for (const char *path : paths) result.push_back(makeEntry(path));
        return result;
    }();
    return catalog;
}
