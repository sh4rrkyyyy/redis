#pragma once

#include <cstddef>
#include <array>

namespace cactus {

#if defined(__x86_64__)

struct MachineContext {
    // Opaque pointer saved during suspension.
    void* data;

    // Saved registers.
    void* r15;
    void* r14;
    void* r13;
    void* r12;
    void* rbx;
    void* rbp;

    // After activation, context starts executing this instruction.
    void* saved_ip;

    // Hack for initial setup.
    void* fake_return_address;
};

#elif defined(__aarch64__)

struct MachineContext {
    // Opaque pointer saved during suspension.
    void* data;

    void* x19;
    void* x20;
    void* x21;
    void* x22;
    void* x23;
    void* x24;
    void* x25;
    void* x26;
    void* x27;
    void* x28;
    void* x29;

    // After activation, context starts executing this instruction.
    void* saved_ip;  // AKA lr, AKA x30

    void* d8;
    void* d9;
    void* d10;
    void* d11;
    void* d12;
    void* d13;
    void* d14;
    void* d15;
};

#else
#error Unsupported architecture
#endif

struct SwitchArgs;
struct SavedContext;

struct IContextEntry {
    // Run returns saved context that should be activated right after Run is finished.
    virtual SavedContext* Run() = 0;
};

struct SavedContext {
    SavedContext(const SavedContext&) = delete;

    SavedContext();

    // Create new context with a given stack.
    SavedContext(char* stack, size_t stack_size);

    // Activate this context and suspend current context in @me.
    void Activate(SavedContext* suspend_to);

    // Suspended context is always stored on top of the corresponding stack.
    MachineContext* sp = nullptr;

    // Function run on the first context activation.
    IContextEntry* entry = nullptr;

    void* sanitizer_fake_stack = nullptr;
    void* sanitizer_stack_bottom = nullptr;
    size_t sanitizer_stack_size = 0;
};

}  // namespace cactus
