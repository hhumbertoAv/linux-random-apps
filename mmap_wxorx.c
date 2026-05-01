#include <stdio.h>     
#include <string.h>    
#include <stddef.h>    
#include <sys/mman.h>  
#include <unistd.h>    

void normal_function(void)
{
    printf("Normal function executed.\n");
}

struct Frame
{
    // This buffer can store only 10 characters.
    char buffer[10];

    // This variable stores the address of a function.
    // Later, the program will call whatever function is stored here.
    void (*callback)(void);
};

int main(void)
{
    // Create a variable called frame.
    // It contains the small buffer and the function pointer.
    struct Frame frame;

    // These bytes are malicious machine instructions for x86-64 (nah! fake malicious).
    // When executed, they simply return the number 42.
    // In a real attack, this could be replaced by more dangerous code.
    unsigned char malicious_code[] = {
        0xb8, 0x2a, 0x00, 0x00, 0x00, // Put 42 into the return register.
        0xc3                          // Return from the function.
    };

    // Ask the kernel for a new memory region.
    void *mem = mmap(
        NULL,                          // Let the kernel choose where to put it.
        4096,                          // Size: one memory page.
        PROT_READ | PROT_WRITE | PROT_EXEC, // The region can be read, written and executed.
        MAP_PRIVATE | MAP_ANONYMOUS,   // This memory is private and not linked to a file.
        -1,                            // No file is used.
        0                              // No file offset is needed.
    );

    // If mmap() failed, stop the program.
    if (mem == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }

    // Copy the malicious machine instructions into the memory region.
    // This is possible because the region has write permission.
    memcpy(mem, malicious_code, sizeof(malicious_code));

    // At the beginning, callback points to the safe function.
    frame.callback = normal_function;

    // Show what happens before the overflow.
    printf("Before overflow:\n");

    // This calls normal_function().
    frame.callback();

    // Find where callback is located inside the struct.
    // We do this because the compiler may add hidden padding between fields.
    size_t offset = offsetof(struct Frame, callback);

    // Create fake malicious input large enough to reach callback.
    unsigned char payload[offset + sizeof(void *)];

    // Fill the first part with dummy bytes.
    // These bytes will fill buffer and any padding before reaching callback.
    memset(payload, 'A', offset);

    // Add the address of the mmap memory region after the dummy bytes.
    // This is the address of the malicious code we want callback to contain after the overflow.
    memcpy(payload + offset, &mem, sizeof(void *));

    // This is the vulnerable line.
    // frame.buffer has only 10 bytes, but payload is bigger.
    // The extra bytes overwrite nearby memory, including callback.
    // As a result, callback is redirected to the malicious code stored in mem.
    memcpy(frame.buffer, payload, sizeof(payload));

    // Show what happens after the overflow.
    printf("After overflow:\n");

    // Treat callback as a function that returns an int.
    // After the overflow, callback points to the mmap memory region containing malicious code.
    int (*func)(void) = (int (*)(void)) frame.callback;

    // Call the overwritten function pointer.
    // The CPU jumps to the mmap region and executes the malicious bytes stored there.
    printf("Result from injected code: %d\n", func());

    // Free the memory region created with mmap().
    munmap(mem, 4096);

    // End the program successfully.
    return 0;


    /*************************************
    Now imagine that the input data comes from a file.
    An external user provides real malicious bytes instead of harmless data.
    In this demo, the bytes only return 42, but in a real attack they could do something harmful!!!
    **************************************/ 

}