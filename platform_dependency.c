#include <windows.h>
#include <intrin.h>

typedef enum KeyId
{
    KEY_0 = 0x30,
    KEY_BACKSPACE = VK_BACK,
    KEY_DELETE = VK_DELETE,
    KEY_LEFT = VK_LEFT,
    KEY_RIGHT = VK_RIGHT
} KeyId;

#define ALLOCATE(size) VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#define FREE_MEMORY(address) VirtualFree(address, 0, MEM_RELEASE)
#define BIT_SCAN_REVERSE(index, mask) _BitScanReverse(index, mask)

uint64_t get_time()
{
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart;
}