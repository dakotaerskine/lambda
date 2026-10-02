#include <iostream>
#include <string>

#include "core/builder.h"
#include "core/context.h"
#include "core/dispatcher.h"
#include "core/error.h"
#include "core/parser.h"
#include "core/platform.h"
#include "core/tables.h"
#include "core/writer.h"

using namespace lambda;

int main(int argc, char * argv[]) {
    std::string program, input, output;

    try {
        Parser::parseArguments(argc, argv, program, input, output);

        Tables::initialize();

        Context context;

        Parser::parseLRD(input, context);

        Builder::buildContext(context);

        Renderer * renderer = context.getRenderer();

        Float duration = Dispatcher::dispatchRender(program, renderer);

        Writer::writeImage(output, renderer, duration);
    }
    catch (const std::exception & e) {
        std::cerr << error::message(program, e) << std::endl;

        return 1;
    }

    return 0;
}