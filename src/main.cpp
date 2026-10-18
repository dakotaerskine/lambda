#include <iostream>
#include <string>

#include "core/allocator.h"
#include "core/dispatcher.h"
#include "core/error.h"
#include "core/parser.h"
#include "core/platform.h"
#include "core/tables.h"
#include "core/writer.h"
#include "render/integrator.h"

using namespace lambda;

int main(int argc, char * argv[]) {
    std::string program, input, output;

    try {
        Parser::parseArguments(argc, argv, program, input, output);

        Allocator allocator;

        Tables::initialize(allocator);

        Integrator * integrator = Parser::parseLRD(input, allocator);

        Float duration = Dispatcher::dispatchRender(program, allocator, integrator);

        Writer::writeImage(output, integrator, duration);
    }
    catch (const std::exception & e) {
        std::cerr << error::message(program, e) << std::endl;

        return 1;
    }

    return 0;
}