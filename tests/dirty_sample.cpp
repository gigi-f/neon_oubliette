#include <cstdlib>

void dirty_function() {
    // VIOLATION: cppcoreguidelines-no-malloc
    void* ptr = malloc(100);
}

void another_dirty_function() {
    // VIOLATION: cppcoreguidelines-owning-memory (Raw pointer ownership)
    int* p = new int(10);
}
