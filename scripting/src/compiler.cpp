#include "../include/compiler.h"
#include <sstream>

es_script::Compiler::Output *es_script::Compiler::s_output = nullptr;

es_script::Compiler::Compiler() {
    m_compiler = nullptr;
}

es_script::Compiler::~Compiler() {
    assert(m_compiler == nullptr);
}

es_script::Compiler::Output *es_script::Compiler::output() {
    if (s_output == nullptr) {
        s_output = new Output;
    }

    return s_output;
}

void es_script::Compiler::resetOutput() {
    // Output is process-global because script nodes publish into it. Never let
    // a new compile inherit pointers from the previously loaded engine.
    // Ownership of published engine/vehicle/transmission/functions is
    // transferred to the application. The Output object itself owns none of
    // those raw pointers, so deleting only the container is safe and prevents
    // one container leak per script compile.
    delete s_output;
    s_output = new Output;
}

void es_script::Compiler::initialize(const std::string &assetDirectory) {
    resetOutput();
    m_compiler = new piranha::Compiler(&m_rules);
    m_compiler->setFileExtension(".mr");
    // Engine scripts import both core definitions from assets/es and helper
    // files from the packaged asset tree. Android user imports are copied into
    // that same tree, so expose both roots to Piranha.
    m_compiler->addSearchPath(assetDirectory + "/es/");
    m_compiler->addSearchPath(assetDirectory + "/");

    m_rules.initialize();
}

bool es_script::Compiler::compile(const piranha::IrPath &path) {
    bool successful = false;
    m_lastErrorText.clear();
    std::ostringstream errorText;

    std::ofstream file("error_log.log", std::ios::out);
    piranha::IrCompilationUnit *unit = m_compiler->compile(path);
    if (unit == nullptr) {
        errorText << "Can't find file: " << path.toString() << "\n";
    }
    else {
        const piranha::ErrorList *errors = m_compiler->getErrorList();
        if (errors->getErrorCount() == 0) {
            unit->build(&m_program);

            m_program.initialize();

            successful = true;
        }
        else {
            for (int i = 0; i < errors->getErrorCount(); ++i) {
                const piranha::CompilationError *err = errors->getCompilationError(i);
                printError(err, file);
                const piranha::ErrorCode_struct &ec = err->getErrorCode();
                errorText << err->getCompilationUnit()->getPath().getStem()
                    << "(" << err->getErrorLocation()->lineStart << "): error "
                    << ec.stage << ec.code << ": " << ec.info << "\n";
            }
        }
    }

    m_lastErrorText = errorText.str();
    if (!m_lastErrorText.empty()) file << m_lastErrorText;
    file.close();

    return successful;
}

es_script::Compiler::Output es_script::Compiler::execute() {
    const bool result = m_program.execute();

    if (!result) {
        // Runtime errors are reported by the Piranha program.
    }

    return *output();
}

void es_script::Compiler::destroy() {
    m_program.free();
    m_compiler->free();

    delete m_compiler;
    m_compiler = nullptr;
}

void es_script::Compiler::printError(
    const piranha::CompilationError *err,
    std::ofstream &file) const
{
    const piranha::ErrorCode_struct &errorCode = err->getErrorCode();
    file << err->getCompilationUnit()->getPath().getStem()
        << "(" << err->getErrorLocation()->lineStart << "): error "
        << errorCode.stage << errorCode.code << ": " << errorCode.info << std::endl;

    piranha::IrContextTree *context = err->getInstantiation();
    while (context != nullptr) {
        piranha::IrNode *instance = context->getContext();
        if (instance != nullptr) {
            const std::string instanceName = instance->getName();
            const std::string definitionName = (instance->getDefinition() != nullptr)
                ? instance->getDefinition()->getName()
                : "<Type Error>";
            const std::string formattedName = (instanceName.empty())
                ? "<unnamed> " + definitionName
                : instanceName + " " + definitionName;

            file
                << "       While instantiating: "
                << instance->getParentUnit()->getPath().getStem()
                << "(" << instance->getSummaryToken()->lineStart << "): "
                << formattedName << std::endl;
        }

        context = context->getParent();
    }
}
