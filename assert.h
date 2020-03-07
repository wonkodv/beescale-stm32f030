
void panic();

#if VERBOSE

#define assert_comp(a,comp,b) do{                                                        \
    uint32_t _a = (uint32_t)(a);                                                         \
    uint32_t _b = (uint32_t)(b);                                                         \
    if(!(_a comp _b)){                                                                   \
        printf("%s:%d: assert failed: %s %s %s\n"                                        \
               "    %40s == %#8lX == %lu\n"                                              \
               "    %40s == %#8lX == %lu\n",                                             \
               __FILE__,__LINE__, #a, #comp, #b,                                         \
               #a, _a, _a,                                                               \
               #b, _b, _b );                                                             \
        panic();                                                                         \
    }                                                                                    \
}while(0)


#define assert(c) do{                                                       \
    if(!(c)){                                                               \
        printf("%s:%d: assert failed: %s\n", __FILE__,__LINE__,#c);         \
        panic();                                                            \
    }                                                                       \
}while(0)

#else
#define assert(c) do{if(!(c)){panic(NULL);}}while(0)
#define assert_comp(a,comp,b) assert((a) comp (b))
#endif
