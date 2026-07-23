#include <assert.h>
#include <ctype.h>

int main(void)
{
    assert(iscntrl(0));
    assert(iscntrl('\n'));
    assert(iscntrl(127));
    assert(!iscntrl('A'));
    assert(isascii(0));
    assert(isascii(127));
    assert(!isascii(-1));
    assert(!isascii(128));
    return 0;
}
