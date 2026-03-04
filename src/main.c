#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "font.h"

// Below this line we experiment

uint16_t *unicode;

// Above this line we are experimenting

// Set the base revision to 4, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(4);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}


void printRC(int r,int c){
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    volatile uint32_t *fb_ptr = framebuffer->address;
    fb_ptr[r * (framebuffer->pitch / 4) + c] = 0xffffff;
}

/* the linear framebuffer */
extern char *fb;
/* number of bytes in each line, it's possible it's not screen width * bytesperpixel! */
extern int scanline;
/* import our font that's in the object file we've created above */
extern const unsigned char _binary_font_psf_start[];

extern const unsigned char _binary_font_psf_end[];

extern const unsigned char _binary_font_psf_size;
#define PIXEL uint32_t   /* pixel pointer */

// helper PS1 Sruct

typedef struct {
    uint32_t width;            // always 8 for PSF1
    uint32_t height;           // header->characterSize
    uint32_t numglyph;         // (fontMode & 1) ? 512 : 256
    uint32_t bytes_per_glyph;  // == height
    const uint8_t *glyphs;     // start of glyph bitmaps (immediately after header)
} PSF1_Font;


static inline uint32_t psf1_index_ascii(const PSF1_Font *F, unsigned char ch){
    return (uint32_t)ch % F->numglyph;
}

static inline const uint8_t *psf1_glyph_ptr(const PSF1_Font *F, uint32_t idx){
    return F->glyphs + (uint64_t)idx * (uint64_t)F->bytes_per_glyph;
}

static const PSF1_Font *putchar(
    /* note that this is int, not char as it's a unicode character */
    unsigned char c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32_t fg, uint32_t bg)
{
    /* cast the address to PSF header struct */
    static PSF1_Font F;

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    //volatile uint32_t *fb_ptr = framebuffer->address;

    const uint8_t *base = _binary_font_psf_start;
    // we use uint8_t as a generic way to point to a size of 1 byte since we are reading raw binary. 
    // Another pattenr is to use const unsigned char

    PSF1_Header *header = (PSF1_Header *)base;
    PSF_font *font_data = _binary_font_psf_start + sizeof(PSF1_Header);

    if (header->magic != PSF1_FONT_MAGIC) {
	    ;
	    ; // This mean magic number is not as expected
        return;
    }
    size_t blob_size = (size_t)(_binary_font_psf_end - _binary_font_psf_start);
    // Still a work in progress

    uint32_t ng = (header->fontMode & 1u) ? 512u : 256u;
    F.width = 8u;
    F.height = header->characterSize;
    F.numglyph = ng;
    F.bytes_per_glyph = header->characterSize;
    F.glyphs = base + sizeof(*header);

    uint8_t *fb_base = (uint8_t *)framebuffer->address;
    
    uint32_t idx = psf1_index_ascii(&F,c);
    const uint8_t *glyph = psf1_glyph_ptr(&F, idx);
    int px = cx, py = cy;

    for (uint32_t row = 0; row < F.height; ++row) {
        uint8_t bits = glyph[row];
        uint8_t *dst_row =
            fb_base +
            (uint64_t)(py + (int)row) * framebuffer->pitch +
            (uint64_t)px * 4;

        for (uint32_t x = 0; x < 8; ++x) {
            uint8_t on = (uint8_t)((bits >> (7u - x)) & 1u);
            uint32_t *dst_px = (uint32_t *)(dst_row + x * 4);
            *dst_px = on ? fg : bg;
        }
    }
    return &F;
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    char *message = "hello world!";
    for (int i = 0; message[i] != '\0';i++){
        putchar(message[i],10 * i,10,0xffffff,0x000000);
    }
    char *message2 = "Welcome to my operating system";
    for (int i = 0; message2[i] != '\0';i++){
        putchar(message2[i],10 * i,30,0xffffff,0x000000);
    }
    char *message3 = "Paging is enabled (although its its not implemented...)";
    for (int i = 0; message3[i] != '\0';i++){
        putchar(message3[i],10 * i,50,0xffffff,0x000000);
    }
    char *message4 = "Progress:.... We have a stack but no heap";
    for (int i = 0; message4[i] != '\0';i++){
        putchar(message4[i],10 * i,70,0xffffff,0x000000);
    }
    char *message5 = "We have kernal space... no user space...";
    for (int i = 0; message5[i] != '\0';i++){
        putchar(message5[i],10 * i,90,0xffffff,0x000000);
    }
    char *message6 = "First order of business is to collect free mem for our heap and brk sys call...";
    for (int i = 0; message6[i] != '\0';i++){
        putchar(message6[i],10 * i,110,0xffffff,0x000000);
    }
    // putchar('e',4,20,0xffffff,0x000000);
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.

    // We're done, just hang...
    hcf();
}
