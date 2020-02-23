

#define assert(c) do{                                                       \
    if(!(c)){                                                               \
        printf("%s:%d: assert failed: %s\n", __FILE__,__LINE__,#c);         \
        while(1);                                                           \
    }                                                                       \
}while(0)

