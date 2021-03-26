#ifndef _SHIM_H
#define _SHIM_H


void *
get_memory(UINT16 addr)
{
#ifdef SDL
    return (void*)&shim_memory[addr];
#else
    return (unsigned char *)addr;
#endif
}


#endif
