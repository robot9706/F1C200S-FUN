#include "stringfunc.h"
#include <stdint.h>

static char toLower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + 32;
    }
    return c;
}

int stricmp (const char *s1, const char *s2)
{
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    
    int result;
    
    if (p1 == p2)
    {
        return 0;
    }
    
    while ((result = toLower(*p1) - toLower(*p2++)) == 0)
    {
        if (*p1++ == '\0')
        {
            break;
        }
    }

    return result;
}

int strnicmp(const char* s1, const char* s2, int num)
{
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    
    int result;
    
    if (p1 == p2)
    {
        return 0;
    }
    
    int loc = 0;
    while (loc < num && (result = toLower(*p1) - toLower(*p2++)) == 0)
    {
        loc++;

        if (*p1++ == '\0')
        {
            break;
        }
    }

    return result;
}