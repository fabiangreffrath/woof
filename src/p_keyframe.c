
#include "d_player.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "dsdhacked.h"
#include "i_printf.h"
#include "i_system.h"
#include "info.h"
#include "m_arena.h"
#include "m_random.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_state.h"

#include <stdint.h>
#include <string.h>

typedef struct
{
    char *buffer;
    char *curr_p;
    size_t buffer_size;
    arena_copy_t *thinkers;
    arena_copy_t *msecnodes;
} keyframe_t;

static keyframe_t *current_keyframe = NULL;

inline static void check_buffer(size_t size)
{
    ptrdiff_t offset = current_keyframe->curr_p - current_keyframe->buffer;

    if (offset + size <= current_keyframe->buffer_size)
    {
        return;
    }

    while (offset + size > current_keyframe->buffer_size)
    {
        current_keyframe->buffer_size *= 2;
    }

    current_keyframe->buffer = I_Realloc(current_keyframe->buffer,
        current_keyframe->buffer_size);
    current_keyframe->curr_p = current_keyframe->buffer + offset;
}

inline static void write8_internal(const int8_t data[], int count)
{
    size_t offset = sizeof(int8_t) * count;
    check_buffer(offset);
    memcpy(current_keyframe->curr_p, data, offset);
    current_keyframe->curr_p += offset;
}

inline static void write16_internal(const int16_t data[], int count)
{
    size_t offset = sizeof(int16_t) * count;
    check_buffer(offset);
    memcpy(current_keyframe->curr_p, data, offset);
    current_keyframe->curr_p += offset;
}

inline static void write32_internal(const int32_t data[], int count)
{
    size_t offset = sizeof(int32_t) * count;
    check_buffer(offset);
    memcpy(current_keyframe->curr_p, data, offset);
    current_keyframe->curr_p += offset;
}

#define write8(...)                                \
    write8_internal((const int8_t[]){__VA_ARGS__}, \
                    sizeof((const int8_t[]){__VA_ARGS__}) / sizeof(int8_t))

#define write16(...)                                 \
    write16_internal((const int16_t[]){__VA_ARGS__}, \
                     sizeof((const int16_t[]){__VA_ARGS__}) / sizeof(int16_t))

#define write32(...)                                 \
    write32_internal((const int32_t[]){__VA_ARGS__}, \
                     sizeof((const int32_t[]){__VA_ARGS__}) / sizeof(int32_t))

inline static void writep(void *ptr)
{
    intptr_t intptr = (intptr_t) ptr;
    check_buffer(sizeof(intptr_t));
    memcpy(current_keyframe->curr_p, &intptr, sizeof(intptr_t));
    current_keyframe->curr_p += sizeof(intptr_t);
}

inline static void writex(const void *ptr, size_t size, int count)
{
    size_t offset = size * count;
    check_buffer(offset);
    memcpy(current_keyframe->curr_p, ptr, offset);
    current_keyframe->curr_p += offset;
}

#define MAX_PARAMS 16

inline static void read8_internal(int8_t *data[], int count)
{
    for (int i = 0; i < count; ++i)
    {
        *data[i] = *((int8_t *)current_keyframe->curr_p);
        current_keyframe->curr_p += sizeof(int8_t);
    }
}

inline static void read16_internal(int16_t *data[], int count)
{
    for (int i = 0; i < count; ++i)
    {
        *data[i] = *((int16_t *)current_keyframe->curr_p);
        current_keyframe->curr_p += sizeof(int16_t);
    }
}

inline static void read32_internal(int32_t *data[], int count)
{
    for (int i = 0; i < count; ++i)
    {
        *data[i] = *((int32_t *)current_keyframe->curr_p);
        current_keyframe->curr_p += sizeof(int32_t);
    }
}

#define read8(...)                            \
    read8_internal((int8_t *[]){__VA_ARGS__}, \
                   sizeof((int8_t *[]){__VA_ARGS__}) / sizeof(int8_t *))

#define read16(...)                             \
    read16_internal((int16_t *[]){__VA_ARGS__}, \
                    sizeof((int16_t *[]){__VA_ARGS__}) / sizeof(int16_t *))

#define read32(...)                             \
    read32_internal((int32_t *[]){__VA_ARGS__}, \
                    sizeof((int32_t *[]){__VA_ARGS__}) / sizeof(int32_t *))

static void *readp(void)
{
    intptr_t intptr = 0;
    memcpy(&intptr, current_keyframe->curr_p, sizeof(intptr_t));
    current_keyframe->curr_p += sizeof(intptr_t);
    return (void *)intptr;
}

inline static void readx(void *ptr, size_t size, int count)
{
    size_t offset = size * count;
    memcpy(ptr, current_keyframe->curr_p, offset);
    current_keyframe->curr_p += offset;
}

static void ArchivePlayers(void)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            writex(&players[i], sizeof(player_t), 1);
        }
    }
}

static void UnArchivePlayers(void)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            readx(&players[i], sizeof(player_t), 1);
        }
    }
}

static void ArchiveWorld(void)
{
    int i;
    const sector_t *sec;
    const line_t *li;
    const side_t *si;

    // do sectors
    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        // killough 10/98: save full floor & ceiling heights, including fraction
        write32(sec->floorheight,
                sec->ceilingheight,
                sec->floor_xoffs,
                sec->floor_yoffs,
                sec->ceiling_xoffs,
                sec->ceiling_yoffs,
                sec->floor_rotation,
                sec->ceiling_rotation);

        write16(sec->floorpic,
                sec->ceilingpic,
                sec->lightlevel,
                sec->special, // needed?   yes -- transfer types
                sec->tag);    // needed?   need them -- killough 

        // Woof!
        writep(sec->thinglist);
    }

    // do lines
    for (i = 0, li = lines; i < numlines; i++, li++)
    {
        write16(li->flags,
                li->special,
                li->tag);

        write32(li->angle,
                li->frontmusic,
                li->backmusic);

        // Woof!
        writep(li->frontsector);
        writep(li->backsector);

        for (int j = 0; j < 2; j++)
        {
            if (li->sidenum[j] != NO_INDEX)
            {
                si = &sides[li->sidenum[j]];

                write16(si->toptexture,
                        si->bottomtexture,
                        si->midtexture);

                write32(si->textureoffset,
                        si->rowoffset);
            }
        }
    }
}

static void UnArchiveWorld(void)
{
    int i;
    sector_t *sec;
    line_t *li;
    side_t *si;

    // do sectors
    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        read32(&sec->floorheight,
               &sec->ceilingheight,
               &sec->floor_xoffs,
               &sec->floor_yoffs,
               &sec->ceiling_xoffs,
               &sec->ceiling_yoffs,
               (int32_t *)&sec->floor_rotation,
               (int32_t *)&sec->ceiling_rotation);

        read16(&sec->floorpic,
               &sec->ceilingpic,
               &sec->lightlevel,
               &sec->special,
               &sec->tag);

        // Woof!
        sec->thinglist = readp();
    }

    // do lines
    for (i = 0, li = lines; i < numlines; i++, li++)
    {
        read16((int16_t *)&li->flags,
               &li->special,
               &li->tag);
        
        read32((int32_t *)&li->angle,
               &li->frontmusic,
               &li->backmusic);

        // Woof!
        li->frontsector = readp();
        li->backsector = readp();

        for (int j = 0; j < 2; j++)
        {
            if (li->sidenum[j] != NO_INDEX)
            {
                si = &sides[li->sidenum[j]];
              
                read16(&si->toptexture,
                       &si->bottomtexture,
                       &si->midtexture);
                
                read32(&si->textureoffset,
                       &si->rowoffset);
                si->oldtextureoffset = si->textureoffset;
                si->oldrowoffset = si->rowoffset;
            }
        }
    }
}

static void ArchiveThinkers()
{
    writex(&thinkercap, sizeof(thinkercap), 1);
    current_keyframe->thinkers = M_CopyArena(thinkers);
    current_keyframe->msecnodes = M_CopyArena(msecnodes);
    writex(blocklinks, blocklinks_size, 1);

    write32(floatok,
            felldown,
            tmfloorz,
            tmceilingz);
    writep(ceilingline);
    writep(floorline);
    writep(linetarget);
    writep(sector_list);
    writep(headsecnode);
    writex(tmbbox, sizeof(tmbbox), 1);
    writep(blockline);
    write32(hangsolid);

    write32(numspechit);
    writex(spechit, sizeof(*spechit), numspechit);
    
    write32(opentop,
            openbottom,
            openrange,
            lowfloor);
    writex(&trace, sizeof(trace), 1);
    write32(num_intercepts);
    writex(intercepts, sizeof(*intercepts), num_intercepts);
}

static void UnArchiveThinkers(void)
{
    readx(&thinkercap, sizeof(thinkercap), 1);
    M_RestoreArena(thinkers, current_keyframe->thinkers);
    M_RestoreArena(msecnodes, current_keyframe->msecnodes);
    readx(blocklinks, blocklinks_size, 1);

    read32(&floatok,
           &felldown,
           &tmfloorz,
           &tmceilingz);
    ceilingline = readp();
    floorline = readp();
    linetarget = readp();
    sector_list = readp();
    headsecnode = readp();
    readx(tmbbox, sizeof(tmbbox), 1);
    blockline = readp();
    read32(&hangsolid);

    read32(&numspechit);
    readx(spechit, sizeof(*spechit), numspechit);

    read32(&opentop,
           &openbottom,
           &openrange,
           &lowfloor);
    readx(&trace, sizeof(trace), 1);
    read32(&num_intercepts);
    readx(intercepts, sizeof(*intercepts), num_intercepts);

    setmobjstate_recursion = 0;
    memset(seenstate_tab, 0, sizeof(statenum_t) * num_states);
}

static void ArchiveRNG(void)
{
    writex(&rng, sizeof(rng_t), 1);
}

static void UnArchiveRNG(void)
{
    readx(&rng, sizeof(rng_t), 1);
}

static keyframe_t *SaveKeyFrame(void)
{
    keyframe_t *keyframe = calloc(1, sizeof(*current_keyframe));
    keyframe->buffer_size = 1024 * 1024;
    keyframe->buffer = malloc(keyframe->buffer_size);
    keyframe->curr_p = keyframe->buffer;

    current_keyframe = keyframe;

    write8((gametic - basetic) & 255);

    ArchivePlayers();
    ArchiveWorld();
    ArchiveThinkers();
    ArchiveRNG();

    current_keyframe = NULL;

    return keyframe;
}

static void LoadKeyFrame(keyframe_t *keyframe)
{
    keyframe->curr_p = keyframe->buffer;

    current_keyframe = keyframe;

    int8_t data = 0;
    read8(&data);
    basetic = gametic - data;

    P_MapStart();
    UnArchivePlayers();
    UnArchiveWorld();
    UnArchiveThinkers();
    UnArchiveRNG();
    P_MapEnd();

    current_keyframe = NULL;
}

static void FreeKeyFrame(keyframe_t *keyframe)
{
    if (keyframe->buffer)
    {
        free(keyframe->buffer);
    }
    if (keyframe->thinkers)
    {
        M_FreeArenaCopy(keyframe->thinkers);
    }
    if (keyframe->msecnodes)
    {
        M_FreeArenaCopy(keyframe->msecnodes);
    }
    free(keyframe);
}

#define MAX_KEYFRAMES 300

typedef struct elem_s
{
    keyframe_t *keyframe;
    struct elem_s *next;
    struct elem_s *prev;
} elem_t;

// LIFO queue structure
typedef struct
{
    elem_t* top;
    elem_t* tail;
    int count;
} queue_t;

static queue_t queue;

static boolean IsEmpty(void)
{
    return queue.top == NULL;
}

// Add an element to the top of the queue
static void Push(keyframe_t *keyframe)
{
    // Remove oldest element if queue is full
    if (queue.count == MAX_KEYFRAMES)
    {
        elem_t *oldtail = queue.tail;
        if (oldtail)
        {
            queue.tail = oldtail->prev;
            if (queue.tail)
            {
                queue.tail->next = NULL;
            }
            else
            {
                // Queue becomes empty after removal
                queue.top = NULL;
            }
            FreeKeyFrame(oldtail->keyframe);
            free(oldtail);
            --queue.count;
        }
    }

    elem_t *newelem = calloc(1, sizeof(*newelem));    
    newelem->keyframe = keyframe;
    
    if (IsEmpty())
    {
        queue.top = newelem;
        queue.tail = newelem;
    }
    else
    {
        newelem->next = queue.top;
        queue.top->prev = newelem;
        queue.top = newelem;
    }
    ++queue.count;
}

// Remove and return element from top of queue
static keyframe_t *Pop(void)
{
    if (IsEmpty())
    {
        return NULL;
    }
    
    elem_t* temp = queue.top;
    keyframe_t *keyframe = temp->keyframe;
    queue.top = temp->next;
    if (queue.top)
    {
        queue.top->prev = NULL;
    }
    else
    {
        queue.tail = NULL;
    }
    free(temp);
    --queue.count;

    return keyframe;
}

void P_FreeKeyframeQueue(void)
{
    elem_t* current = queue.top;
    while (current)
    {
        elem_t* temp = current;
        current = current->next;
        FreeKeyFrame(temp->keyframe);
        free(temp);
    }
    memset(&queue, 0, sizeof(queue));
}

void P_SaveKeyframe(void)
{
    Push(SaveKeyFrame());
}

void P_LoadKeyframe(void)
{
    keyframe_t *keyframe = Pop();
    if (keyframe)
    {
        LoadKeyFrame(keyframe);
        FreeKeyFrame(keyframe);
    }
}
