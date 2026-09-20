#include "avcdec.h"

#include <stdlib.h>
#include <string.h>

#include <psp2/kernel/sysmem.h>
#include <debugnet.h>

struct avcdec {
    SceAvcdecCtrl decoder;
    SceAvcdecQueryDecoderInfo query;
    SceAvcdecPicture picture;
    SceAvcdecPicture *picture_ptrs[1];
    SceAvcdecArrayPicture array;
    SceUID mem_block;
    int have_picture;
    void *out_buf;
    uint32_t out_width;
    uint32_t out_height;
};

static int g_step = 0;
static int g_rc = 0;
static uint32_t g_frame_mem_size = 0;

int avcdec_last_step(void) { return g_step; }
int avcdec_last_rc(void) { return g_rc; }
uint32_t avcdec_last_framesize(void) { return g_frame_mem_size; }

avcdec *avcdec_create(uint32_t width, uint32_t height, uint32_t num_ref_frames, int *err_out) {
    if (err_out) {
        *err_out = 0;
    }
    avcdec *d = calloc(1, sizeof(*d));
    if (!d) {
        return NULL;
    }

    d->query.horizontal = width;
    d->query.vertical = height;
    d->query.numOfRefFrames = num_ref_frames;

    SceAvcdecDecoderInfo info;
    memset(&info, 0, sizeof(info));
    g_step = 1;
    int r = sceAvcdecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC, &d->query, &info);
    g_rc = r;
    g_frame_mem_size = info.frameMemSize;
    debugNetPrintf(DEBUG, "PSVitaAuto: QueryDecoderMemSize=0x%08X frameMemSize=%u\n",
                   r, info.frameMemSize);
    if (r != 0) {
        if (err_out) *err_out = r;
        free(d);
        return NULL;
    }

    SceVideodecQueryInitInfoHwAvcdec init;
    memset(&init, 0, sizeof(init));
    init.size = sizeof(init);
    init.horizontal = width;
    init.vertical = height;
    init.numOfRefFrames = num_ref_frames;
    init.numOfStreams = 1;

    g_step = 2;
    r = sceVideodecInitLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC, &init);
    g_rc = r;
    debugNetPrintf(DEBUG, "PSVitaAuto: InitLibrary=0x%08X\n", r);
    if (r != 0) {
        if (err_out) *err_out = r;
        free(d);
        return NULL;
    }

    g_step = 3;
    uint32_t sz = (info.frameMemSize + 0xFFFFF) & ~0xFFFFFu;
    d->mem_block = sceKernelAllocMemBlock("avcdec", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW,
                                          sz, NULL);
    g_rc = d->mem_block;
    debugNetPrintf(DEBUG, "PSVitaAuto: AllocMemBlock=0x%08X size=%u\n",
                   d->mem_block, sz);
    if (d->mem_block < 0) {
        if (err_out) *err_out = d->mem_block;
        sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
        free(d);
        return NULL;
    }
    if (sceKernelGetMemBlockBase(d->mem_block, &d->decoder.frameBuf.pBuf) != 0) {
        if (err_out) *err_out = -1;
        sceKernelFreeMemBlock(d->mem_block);
        sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
        free(d);
        return NULL;
    }
    d->decoder.frameBuf.size = sz;

    g_step = 4;
    r = sceAvcdecCreateDecoder(SCE_VIDEODEC_TYPE_HW_AVCDEC, &d->decoder, &d->query);
    g_rc = r;
    debugNetPrintf(DEBUG, "PSVitaAuto: CreateDecoder=0x%08X\n", r);
    if (r != 0) {
        if (err_out) *err_out = r;
        sceKernelFreeMemBlock(d->mem_block);
        sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
        free(d);
        return NULL;
    }

    d->picture_ptrs[0] = &d->picture;
    d->array.numOfOutput = 0;
    d->array.numOfElm = 1;
    d->array.pPicture = d->picture_ptrs;

    return d;
}

void avcdec_destroy(avcdec *d) {
    if (!d) {
        return;
    }
    sceAvcdecDeleteDecoder(&d->decoder);
    sceKernelFreeMemBlock(d->mem_block);
    sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
    free(d);
}

void avcdec_set_output(avcdec *d, void *out, uint32_t out_width, uint32_t out_height) {
    if (!d) {
        return;
    }
    d->out_buf = out;
    d->out_width = out_width;
    d->out_height = out_height;
    d->picture.frame.pixelType = SCE_AVCDEC_PIXELFORMAT_RGBA8888;
    d->picture.frame.framePitch = out_width;
    d->picture.frame.frameWidth = out_width;
    d->picture.frame.frameHeight = out_height;
    d->picture.frame.pPicture[0] = out;
    d->picture.frame.pPicture[1] = NULL;
}

int avcdec_decode(avcdec *d, const uint8_t *data, uint32_t size) {
    SceAvcdecAu au;
    memset(&au, 0, sizeof(au));
    au.pts.upper = 0xFFFFFFFF;
    au.pts.lower = 0xFFFFFFFF;
    au.dts.upper = 0xFFFFFFFF;
    au.dts.lower = 0xFFFFFFFF;
    au.es.pBuf = (void *)data;
    au.es.size = size;

    d->array.numOfOutput = 0;
    d->picture.size = sizeof(d->picture);

    int r = sceAvcdecDecode(&d->decoder, &au, &d->array);
    d->have_picture = (r == 0 && d->array.numOfOutput > 0);
    return r;
}

const SceAvcdecPicture *avcdec_last_picture(avcdec *d) {
    return d->have_picture ? &d->picture : NULL;
}
