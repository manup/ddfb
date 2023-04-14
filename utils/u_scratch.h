#ifndef U_SCRATCH_H
#define U_SCRATCH_H

#define U_SCRATCH_PUSH() \
    unsigned scratch_pos; \
    scratch_pos = U_ScratchPos()

#define U_SCRATCH_POP() U_ScratchRestore(scratch_pos)

void U_ScratchInit(unsigned size);
void *U_ScratchAlloc(unsigned size);
unsigned U_ScratchPos(void);
void U_ScratchRestore(unsigned pos);
void U_ScratchReset(void);
void U_ScratchFree(void);

#endif /* U_SCRATCH_H */
