//
// Copyright (c) 2024, Daily
//

#include "rtvi_utils.h"

#include <random>

static const size_t ID_LENGTH = 10;

std::string rtvi::generate_random_id() {
    const std::string characters =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> distribution(
            0, characters.size() - 1
    );

    std::string id;
    for (size_t i = 0; i < ID_LENGTH; ++i) {
        id += characters[distribution(generator)];
    }

    return id;
}
