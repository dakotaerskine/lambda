#include <iostream>
#include <string>

#include "core/context.h"
#include "core/dispatcher.h"
#include "core/error.h"
#include "core/parser.h"
#include "core/platform.h"
#include "core/tables.h"
#include "core/writer.h"

int main(int argc, char * argv[]) {
    std::string program, input, output;

    try {
        Parser::parseArguments(argc, argv, program, input, output);

        Tables tables;

        Renderer renderer;
        Payload payload;

        Parser::parseLRD(input, renderer, payload);

        Context context(payload, renderer);

        Float duration = Dispatcher::dispatchRender(program, renderer);

        Writer::writeImage(output, renderer, duration);
    }
    catch (const std::exception & e) {
        std::cerr << errorMessage(program, e) << std::endl;
        return 1;
    }

    return 0;
}