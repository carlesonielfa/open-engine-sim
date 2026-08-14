#include "engine_catalog.h"

#include <gtest/gtest.h>

#if defined(ATG_ENGINE_SIM_PIRANHA_ENABLED)
#include "compiler.h"
#include <filesystem>
#include <fstream>
#endif

TEST(EngineCatalog, ContainsOnlyRunnableScriptsGroupedByDirectory) {
    const auto &catalog = engineCatalog();
    ASSERT_FALSE(catalog.empty());
    EXPECT_EQ(catalog.front().group, "Atg Video 1");
    EXPECT_EQ(catalog.back().group, "Atg Video 2");
    for (const EngineCatalogEntry &entry : catalog) {
        EXPECT_EQ(entry.relativeScriptPath.find("engines/"), 0u);
        EXPECT_FALSE(entry.name.empty());
        EXPECT_EQ(entry.relativeScriptPath.find("/radial.mr"), std::string::npos) << entry.relativeScriptPath;
    }
}

#if defined(ATG_ENGINE_SIM_PIRANHA_ENABLED)
TEST(EngineCatalog, EverySelectionEntryCompilesThroughItsGeneratedEntrypoint) {
    const std::filesystem::path assets(ENGINE_SIM_TEST_ASSET_DIRECTORY);
    const std::filesystem::path entryPoint = std::filesystem::temp_directory_path()
        / "engine-sim-catalog-test-entry.mr";

    for (const EngineCatalogEntry &entry : engineCatalog()) {
        {
            std::ofstream file(entryPoint);
            ASSERT_TRUE(file.good());
            file << "import \"" << (assets / entry.relativeScriptPath).generic_string() << "\"\n\nmain()\n";
        }
        es_script::Compiler compiler;
        compiler.initialize(assets.string());
        EXPECT_TRUE(compiler.compile(entryPoint.string())) << entry.relativeScriptPath;
        compiler.destroy();
    }
    std::error_code ignored;
    std::filesystem::remove(entryPoint, ignored);
}
#endif
