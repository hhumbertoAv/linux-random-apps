#include "vmlinux.h"          // Generated kernel type universe (BTF -> C structs).
#include <bpf/bpf_helpers.h>  // eBPF helper defs + SEC() macro magic.
#include <bpf/bpf_core_read.h>// CO-RE reads: “read fields safely across kernel versions”.

// A tiny counter map:
struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);   // one element: index 0
    __type(key, __u32);       // key type
    __type(value, __u64);     // value type
} counter SEC(".maps");       // tell the loader: “this is a BPF map”

// The HOOK: Attach this program to the tracepoint "signal:signal_generate".
// Translation: run this function in kernel context every time the kernel generates a signal.
// The argument "ctx" is a pointer to the tracepoint data payload (event fields).
SEC("tracepoint/signal/signal_generate")
int on_signal_generate(struct trace_event_raw_signal_generate *ctx)
{
    // Read the "sig" field (signal number) from the tracepoint context.
    __u32 sig = BPF_CORE_READ(ctx, sig);

    // Our map has exactly one element, so we always use key = 0.
    __u32 key = 0;

    // Will hold the pointer to the counter value inside the map.
    __u64 *v;

    // Filter: we only count SIGTSTP.
    // SIGTSTP is the "terminal stop" signal, commonly produced by Ctrl+Z.
    if (sig != SIGTSTP)
        return 0;

    // Look up the address of counter[0] inside the map.
    // This returns a pointer to the value (or NULL if something went wrong).
    v = bpf_map_lookup_elem(&counter, &key);

    // If lookup succeeded, increment the counter atomically.
    // WARN: Atomic matters because multiple CPUs can execute this program at the same time.
    if (v)
        __sync_fetch_and_add(v, 1);

    // Tracepoint programs conventionally return 0.
    return 0;
}

// Tell the kernel the program license.
// "GPL" is commonly used and unlocks some helpers/features that are restricted otherwise.
char LICENSE[] SEC("license") = "GPL";
