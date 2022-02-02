// theta.cpp

#pragma warning(disable:4996)

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <new>

#include <map>
#include <set>
#include <vector>

#include "bytecode.h"
#include "diagnostics.h"
#include "emit.h"
#include "lexer.h"
#include "parser.h"
#include "semantics.h"
#include "source-manager.h"
#include "string.h"
#include "token.h"
#include "value.h"
#include "vm.h"

using namespace theta;

int main(int argc, char** argv)
{
    using namespace semantics;

    auto sourceFile = loadSourceFile("test.theta");

    Lexer lexer;
    lexer.init(sourceFile->_text);

    Parser parser;
    parser.init(&lexer);

    auto astProgram = parser.parseProgram();

    Checker checker;
    checker.checkProgram(astProgram);

    bytecode::Emitter emitter;
    auto bcProgram = emitter.emitProgram(astProgram);

    vm::VM vm;
    vm.execute(bcProgram);

    return 0;
}
