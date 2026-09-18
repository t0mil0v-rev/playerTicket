// tokenpizder
// Author: DoubleLuc (https://github.com/t0mil0v-rev)

import memory;
import process;

#include <cstdio>

int main() {
    const auto token = process::get_ticket();
    if (!token || token->empty()) {
        std::fprintf(stderr, "[!] Token not found. Make sure Standoff 2 is running in emulator and logged in.\n");
        return 1;
    }

    std::printf("%s\n", token->c_str());
    return 0;
}
