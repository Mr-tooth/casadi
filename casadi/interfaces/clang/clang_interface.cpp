/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010-2014 Joel Andersson, Joris Gillis, Moritz Diehl,
 *                            K.U. Leuven. All rights reserved.
 *    Copyright (C) 2011-2014 Greg Horn
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include "clang_interface.hpp"

#include "casadi/core/std_vector_tools.hpp"
#include "casadi/core/function/mx_function.hpp"
#include "casadi/core/casadi_meta.hpp"

using namespace clang;
using namespace clang::driver;


using namespace std;
namespace casadi {

  extern "C"
  int CASADI_JITCOMPILER_CLANG_EXPORT
  casadi_register_jitcompiler_clang(JitCompilerInternal::Plugin* plugin) {
    plugin->creator = ClangJitCompilerInterface::creator;
    plugin->name = "clang";
    plugin->doc = ClangJitCompilerInterface::meta_doc.c_str();
    plugin->version = 23;
    return 0;
  }

  extern "C"
  void CASADI_JITCOMPILER_CLANG_EXPORT casadi_load_jitcompiler_clang() {
    JitCompilerInternal::registerPlugin(casadi_register_jitcompiler_clang);
  }

  ClangJitCompilerInterface* ClangJitCompilerInterface::clone() const {
    // Return a deep copy
    ClangJitCompilerInterface* node = new ClangJitCompilerInterface(f_);
    if (!node->is_init_)
      node->init();
    return node;
  }

  ClangJitCompilerInterface::ClangJitCompilerInterface(const Function& f) :
      JitCompilerInternal(f) {
    addOption("include_path", OT_STRING, "", "Include paths for the JIT compiler."
      " The include directory shipped with CasADi will be automatically appended.");
    addOption("flags", OT_STRINGVECTOR, GenericType(),
      "Compile flags for the JIT compiler. Default: -O3.");

    myerr_ = 0;
    executionEngine_ = 0;
    context_ = 0;
    act_ = 0;
  }

  ClangJitCompilerInterface::~ClangJitCompilerInterface() {
    if (act_) delete act_;
    if (myerr_) delete myerr_;
    if (executionEngine_) delete executionEngine_;
    if (context_) delete context_;
  }

  void ClangJitCompilerInterface::init() {
    // Initialize the base classes
    JitCompilerInternal::init();
    log("ClangJitCompilerInterface::init", "Enter");


    std::string name = "foo";

    f_.generate(name);

    // Path to the C file
    string inputPath = name + ".c";

    context_ = new llvm::LLVMContext();

    // Arguments to pass to the clang frontend
    vector<const char *> args;
    args.push_back(inputPath.c_str());

    std::vector<std::string> flags;

    if (hasSetOption("flags")) {
      flags = getOption("flags");
      for (int i=0;i<flags.size();++i) {
        args.push_back(flags[i].c_str());
      }
    } else {
      args.push_back("-O3");
    }

    // The compiler invocation needs a DiagnosticsEngine so it can report problems
    DiagnosticOptions* DiagOpts = new DiagnosticOptions();
    myerr_ = new llvm::raw_os_ostream(userOut<true>());
    TextDiagnosticPrinter *DiagClient = new TextDiagnosticPrinter(*myerr_, DiagOpts);

    DiagnosticIDs* DiagID = new DiagnosticIDs();
    // This object takes ownerships of all three passed-in pointers
    DiagnosticsEngine Diags(DiagID, DiagOpts, DiagClient);

    // Create the compiler invocation
    clang::CompilerInvocation* CI = new clang::CompilerInvocation();
    clang::CompilerInvocation::CreateFromArgs(*CI, &args[0], &args[0] + args.size(), Diags);

    vector<string> argsFromInvocation;

    // Create the compiler instance
    clang::CompilerInstance Clang;
    Clang.setInvocation(CI);

    // Get ready to report problems
    Clang.createDiagnostics();
    if (!Clang.hasDiagnostics())
      casadi_error("Cannot create diagnostics");

    #ifdef _WIN32
    char pathsep = ';';
    const std::string filesep("\\");
    #else
    char pathsep = ':';
    const std::string filesep("/");
    #endif

    // Search path
    std::stringstream paths;
    paths << getOption("include_path").toString() << pathsep;
    paths << CasadiOptions::getJitIncludePath() << pathsep;
    paths << CasadiMeta::getInstallPrefix() << filesep <<
      "include" << filesep << "casadi" << filesep << "jit";
    std::string path;
    while (std::getline(paths, path, pathsep)) {
      log("ClangInterface::init", std::string("Adding path:") + path);
      Clang.getHeaderSearchOpts().AddPath(path.c_str(), frontend::CSystem, false, false);
    }

    // Create an action and make the compiler instance carry it out
    act_ = new clang::EmitLLVMOnlyAction(context_);
    if (!Clang.ExecuteAction(*act_))
      casadi_error("Cannot execute action");

    // Grab the module built by the EmitLLVMOnlyAction
    std::unique_ptr<llvm::Module> module = act_->takeModule();
    module_ = module.get();

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    // Create the JIT.  This takes ownership of the module.
    std::string ErrStr;
    executionEngine_ =
      llvm::EngineBuilder(std::move(module)).setEngineKind(llvm::EngineKind::JIT)
      .setErrorStr(&ErrStr).create();
    if (!executionEngine_) {
      casadi_error("Could not create ExecutionEngine: " << ErrStr);
    }

    executionEngine_->finalizeObject();
  }

  void* ClangJitCompilerInterface::getFunction(const std::string& symname) {
    return executionEngine_->getPointerToFunction(module_->getFunction(symname));
  }

  void ClangJitCompilerInterface::evalD(const double** arg, double** res, int* iw, double* w) {
    casadi_assert(0);
  }


} // namespace casadi
