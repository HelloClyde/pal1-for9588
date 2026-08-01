#ifndef PAL9588_CTYPE_H
#define PAL9588_CTYPE_H

static inline int isdigit(int value)
{
    return value >= '0' && value <= '9';
}

static inline int isspace(int value)
{
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

static inline int isalpha(int value)
{
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z');
}

static inline int isalnum(int value)
{
    return isalpha(value) || isdigit(value);
}

static inline int isxdigit(int value)
{
    return isdigit(value) || (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

static inline int tolower(int value)
{
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

static inline int toupper(int value)
{
    return value >= 'a' && value <= 'z' ? value - ('a' - 'A') : value;
}

#endif
