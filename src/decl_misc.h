
#ifndef DECL_MISC_H
#define DECL_MISC_H

#include "doomtype.h"
#include "m_scanner.h"

inline static int CheckKeywordInternal(scanner_t *sc,
                                       const char *keywords[], int count)
{
    const char *string = SC_GetString(sc);
    for (int i = 0; i < count; ++i)
    {
        if (strcasecmp(keywords[i], string) == 0)
        {
            return i;
        }
    }
    return -1;
}

#define CheckKeyword(sc, ...)                                  \
    CheckKeywordInternal(sc, (const char *[]){__VA_ARGS__},    \
                         sizeof((const char *[]){__VA_ARGS__}) \
                             / sizeof(const char *))

inline static int RequireKeywordInternal(scanner_t *sc, const char *keywords[],
                                         int count)
{
    int result = CheckKeywordInternal(sc, keywords, count);
    if (result >= 0)
    {
        return result;
    }
    SC_Error(sc, "Invalid keyword at this point.");
    return -1;
}

#define RequireKeyword(sc, ...) \
    RequireKeywordInternal(sc, (const char *[]){__VA_ARGS__},    \
                           sizeof((const char *[]){__VA_ARGS__}) \
                               / sizeof(const char *))

// Helper function for when we need to parse a signed integer.
inline static int GetNegativeInteger(scanner_t *sc)
{
    boolean neg = SC_CheckToken(sc, '-');
    SC_MustGetToken(sc, TK_IntConst);
    return neg ? -SC_GetNumber(sc) : SC_GetNumber(sc);
}

#endif
