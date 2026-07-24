#include <assert.h>
#include <string.h>

int main(void)
{
    const char text[] = "alpha:beta";

    assert(strpbrk(text, ":") == text + 5);
    assert(strpbrk(text, "xyz:") == text + 5);
    assert(strpbrk(text, "ba") == text);
    assert(strpbrk(text, "") == 0);
    assert(strpbrk("", "abc") == 0);
    assert(strpbrk(text, "XYZ") == 0);
    assert(strpbrk("end!", "!") == &"end!"[3]);
    return 0;
}
