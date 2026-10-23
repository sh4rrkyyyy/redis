#include "test.h"

#include <cactus/cactus.h>

using namespace cactus;

FIBER_TEST_CASE("Mutex lock") {
    Mutex m;
    WaitGroup g;

    for (int i = 0; i < 10; i++) {
        g.Spawn([&] { MutexGuard guard(m); });
    }

    g.Wait();
}
