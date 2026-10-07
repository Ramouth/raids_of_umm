#include "test_runner.h"

// ./raids_tests [filter] — only suites whose name contains it run; "!text" runs all the others.
int main(int argc, char** argv) {
    return TestRunner::get().run(argc > 1 ? argv[1] : "");
}
