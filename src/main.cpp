#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "session/CompilationSession.h"
using namespace std;



struct CompileOptions {
    std::string inputFile;
    bool run = false;
    bool jit = false;
    bool printAST = false;
    enum { O0 , O1 , O2, O3, Os, Oz } optLevel = O2;
    std::string optimization = "-O2";
    std::string outputBin = "glue_program";
};

struct Compiler {
    string GLUE_VERSION = "0.2.0";
};



void printHelp(const char* progName) {
    Compiler comp;

    std::cout <<
    "Usage: " << progName << " [options] <file.glue>\n"
    "\n"
    "Options:\n"
    "  --run,         Compile and run file\n"
    "  --jit      Compile and run file using JIT\n"
    "  --o <0|1|2|3|s|z>  Optimization level\n"
    "  --output,  Compiled program name\n"
    "\n"
    "Usages:\n"
    "  " << progName << " program.glue       \n"
    "  " << progName << " program.glue --run -o2   \n"
    "  " << progName << " program.glue --jit  \n"
    "  " << progName << " program.glue --output app --run \n";

    std::cout << " \n Glue version: " << comp.GLUE_VERSION << "\n";
    std::cout << " Created by Glueeed" "\n";


}



CompileOptions parseArgs(int argc, char* argv[]) {
    CompileOptions opts;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--run") {
            opts.run = true;
        }
        else if (arg == "--o0" || arg == "-o0") {
            opts.optLevel = CompileOptions::O0;
            opts.optimization = "-O0";
        }
        else if (arg == "--o1" || arg == "-o1") {
            opts.optLevel = CompileOptions::O1;
            opts.optimization = "-O1";
        }
        else if (arg == "--o2" || arg == "-o2") {
            opts.optLevel = CompileOptions::O2;
            opts.optimization = "-O2";
        }
        else if (arg == "--o3" || arg == "-o3") {
            opts.optLevel = CompileOptions::O3;
            opts.optimization = "-O3";
        }
        else if (arg == "--os" || arg == "-os") {
            opts.optLevel = CompileOptions::Os;
            opts.optimization = "-Os";
        }
        else if (arg == "--oz" || arg == "-oz") {
            opts.optLevel = CompileOptions::Oz;
            opts.optimization = "-Oz";
        }


        else if (arg == "--jit") {
            opts.jit = true;
        }
        else if (arg == "--ast") {
            opts.printAST = true;
        }
        else if (arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "Błąd: option '" << arg << "' requires name file\n";
                exit(1);
            }
            opts.outputBin = argv[++i];
        }
        else if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            exit(0);
        }
        else if (arg[0] != '-') {
            opts.inputFile = arg;
        }
        else {
            std::cerr << "Error: Unknown command  '" << arg << "'\n";
            std::cerr << "Use '--help' to show options\n";
            exit(1);
        }
    }

    return opts;
}

int main(int argc, char* argv[]) {
    CompileOptions opts = parseArgs(argc, argv);

    if (opts.inputFile.empty()) {
        std::cerr << "Error: you must provide .glue file to compilate\n";
        printHelp(argv[0]);
        return 1;
    }

    CompilationSession session(opts.inputFile);
    try {
        session.loadEntry();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    llvm::LLVMContext context;
    Parser parser(session.mainUnit().tokens, context);


    try {
        parser.parse();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }

    if (opts.printAST) {
        std::cout << "=== AST ===" << std::endl;
        parser.printAST();
        std::cout << "=== End AST ===" << std::endl;
        return 0;
    }

    try {
        parser.runSemanticAnalysis();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }

    CodeGenerator& codegen = parser.getCodeGenerator();
    codegen.generateCode(parser.getRoot());

    if (opts.jit) {
        codegen.runJit();
    } else if (opts.run) {
        codegen.compileAndRun(opts.outputBin, true, opts.optimization);
    } else {
        codegen.compileAndRun(opts.outputBin, false, opts.optimization);
    }

    return 0;
}
