#include <iostream>
#include <string>

#include "core/allocator.h"
#include "core/color.h"
#include "core/constants.h"
#include "core/error.h"
#include "core/loader.h"
#include "core/platform.h"
#include "core/solver.h"

using namespace lambda;

int main(int argc, char * argv[]) {
    std::string program;

    try {
        if (argc > 0) program = argv[0];

        if (argc != 2) throw Error("invalid number of arguments");

        Allocator allocator;

        ColorSpace::initialize(allocator);

        std::string spaceName = argv[1];

        const ColorSpace * space = ColorSpace::get(spaceName);

        if (!space) throw Error("invalid color space");

        std::string file = LAMBDA_DATA_DIRECTORY "/" + spaceName + ".coeff";

        float * upsamplingScale = allocator.allocate<float>(constants::UPSAMPLING_RESOLUTION);
        float (* upsamplingLUT)[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3] = allocator.allocate<float[constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][constants::UPSAMPLING_RESOLUTION][3]>(3);

        if (!Loader::loadUpsampling(file, upsamplingScale, upsamplingLUT)) {
            Solver::solveGrid(space, upsamplingScale, upsamplingLUT);

            Loader::saveUpsampling(file, upsamplingScale, upsamplingLUT);
        }
    }
    catch (const std::exception & exception) {
        ErrorHandler::handle(program, exception);

        return 1;
    }

    return 0;
}