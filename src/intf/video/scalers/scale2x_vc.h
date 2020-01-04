#ifndef __SCALE2XV_H
#define __SCALE2XV_H

/* See scale2x.h for function descriptions and information */
#include "scale2x.h"

#ifdef _MSC_VER

/***************************************************************************/
/* Scale2x MMX implementation */

static void internal_scale2x_16_mmx_single(scale2x_uint16* dst, const scale2x_uint16* src0, const scale2x_uint16* src1, const scale2x_uint16* src2, unsigned count) {
  /* always do the first and last run */
  count -= 2*4;

  __asm {
    mov eax, src0;
    mov ebx, src1;
    mov ecx, src2;
    mov edx, dst;
    mov esi, count;

    /* first run */
    /* set the current, current_pre, current_next registers */
    pxor mm0,mm0; /* use a fake black out of screen */
    movq mm7, qword ptr [ebx];
    movq mm1, qword ptr [ebx + 8];
    psrlq mm0, 48;
    psllq mm1, 48;
    movq mm2, mm7;
    movq mm3, mm7;
    psllq mm2, 16;
    psrlq mm3, 16;
    por mm0, mm2;
    por mm1, mm3;

    /* current_upper */
    movq mm6, qword ptr [eax];

    /* compute the upper-left pixel for dst0 on %%mm2 */
    /* compute the upper-right pixel for dst0 on %%mm4 */
    movq mm2, mm0;
    movq mm4, mm1;
    movq mm3, mm0;
    movq mm5, mm1;
    pcmpeqw mm2, mm6;
    pcmpeqw mm4, mm6;
    pcmpeqw mm3, qword ptr [ecx];
    pcmpeqw mm5, qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqw mm2,mm1;
    pcmpeqw mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst0 */
    movq mm3,mm2;
    punpcklwd mm2,mm4;
    punpckhwd mm3,mm4;
    movq qword ptr [edx], mm2;
    movq qword ptr [edx + 8], mm3;

    /* next */
    add eax, 8;
    add ebx, 8;
    add ecx, 8;
    add edx, 16;

    /* central runs */
    shr esi, 2;
    jz label1;
    align 4;
  label0:

    /* set the current, current_pre, current_next registers */
    movq mm0, qword ptr [ebx-8];
    movq mm7, qword ptr [ebx];
    movq mm1, qword ptr [ebx+8];
    psrlq mm0,48;
    psllq mm1,48;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,16;
    psrlq mm3,16;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6, qword ptr [eax];

    /* compute the upper-left pixel for dst0 on %%mm2 */
    /* compute the upper-right pixel for dst0 on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqw mm2,mm6;
    pcmpeqw mm4,mm6;
    pcmpeqw mm3, qword ptr [ecx];
    pcmpeqw mm5, qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqw mm2,mm1;
    pcmpeqw mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst0 */
    movq mm3,mm2;
    punpcklwd mm2,mm4;
    punpckhwd mm3,mm4;
    movq qword ptr [edx], mm2;
    movq qword ptr [edx+8], mm3;

    /* next */
    add eax,8;
    add ebx,8;
    add ecx,8;
    add edx,16;

    dec esi;
    jnz label0;
  label1:

    /* final run */
    /* set the current, current_pre, current_next registers */
    movq mm0, qword ptr [ebx-8];
    movq mm7, qword ptr [ebx];
    pxor mm1,mm1; /* use a fake black out of screen */
    psrlq mm0,48;
    psllq mm1,48;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,16;
    psrlq mm3,16;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6, qword ptr [eax];

    /* compute the upper-left pixel for dst0 on %%mm2 */
    /* compute the upper-right pixel for dst0 on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqw mm2,mm6;
    pcmpeqw mm4,mm6;
    pcmpeqw mm3, qword ptr [ecx];
    pcmpeqw mm5, qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqw mm2,mm1;
    pcmpeqw mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst0 */
    movq mm3,mm2;
    punpcklwd mm2,mm4;
    punpckhwd mm3,mm4;
    movq qword ptr [edx], mm2;
    movq qword ptr [edx+8], mm3;

    mov src0, eax;
    mov src1, ebx;
    mov src2, ecx;
    mov dst, edx;
    mov count, esi;
  }
}

static void internal_scale2x_32_mmx_single(scale2x_uint32* dst, const scale2x_uint32* src0, const scale2x_uint32* src1, const scale2x_uint32* src2, unsigned count) {
  /* always do the first and last run */
  count -= 2*2;

  __asm {
    mov eax, src0;
    mov ebx, src1;
    mov ecx, src2;
    mov edx, dst;
    mov esi, count;

    /* first run */
    /* set the current, current_pre, current_next registers */
    pxor mm0,mm0;
    movq mm7,qword ptr [ebx];
    movq mm1,qword ptr [ebx + 8];
    psrlq mm0,32;
    psllq mm1,32;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,32;
    psrlq mm3,32;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6,qword ptr [eax];

    /* compute the upper-left pixel for dst0 on %%mm2 */
    /* compute the upper-right pixel for dst0 on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqd mm2,mm6;
    pcmpeqd mm4,mm6;
    pcmpeqd mm3,qword ptr [ecx];
    pcmpeqd mm5,qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqd mm2,mm1;
    pcmpeqd mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst0 */
    movq mm3,mm2;
    punpckldq mm2,mm4;
    punpckhdq mm3,mm4;
    movq qword ptr [edx],mm2;
    movq qword ptr [edx+8],mm3;

    /* next */
    add eax,8;
    add ebx,8;
    add ecx,8;
    add edx,16;

    /* central runs */
    shr esi,1;
    jz label1;
label0:

  /* set the current, current_pre, current_next registers */
    movq mm0,qword ptr [ebx-8];
    movq mm7,qword ptr [ebx];
    movq mm1,qword ptr [ebx+8];
    psrlq mm0,32;
    psllq mm1,32;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,32;
    psrlq mm3,32;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6,qword ptr[eax];

    /* compute the upper-left pixel for dst0 on %%mm2 */
    /* compute the upper-right pixel for dst0 on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqd mm2,mm6;
    pcmpeqd mm4,mm6;
    pcmpeqd mm3,qword ptr[ecx];
    pcmpeqd mm5,qword ptr[ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqd mm2,mm1;
    pcmpeqd mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst0 */
    movq mm3,mm2;
    punpckldq mm2,mm4;
    punpckhdq mm3,mm4;
    movq qword ptr [edx],mm2;
    movq qword ptr [edx+8],mm3;

    /* next */
    add eax,8;
    add ebx,8;
    add ecx,8;
    add edx,16;

    dec esi;
    jnz label0;
label1:

    /* final run */
    /* set the current, current_pre, current_next registers */
    movq mm0,qword ptr [ebx-8];
    movq mm7,qword ptr [ebx];
    pxor mm1,mm1;
    psrlq mm0,32;
    psllq mm1,32;
    movq mm2,mm7;
    movq mm3,mm7;
    psllq mm2,32;
    psrlq mm3,32;
    por mm0,mm2;
    por mm1,mm3;

    /* current_upper */
    movq mm6,qword ptr [eax];

    /* compute the upper-left pixel for dst0 on %%mm2 */
    /* compute the upper-right pixel for dst0 on %%mm4 */
    movq mm2,mm0;
    movq mm4,mm1;
    movq mm3,mm0;
    movq mm5,mm1;
    pcmpeqd mm2,mm6;
    pcmpeqd mm4,mm6;
    pcmpeqd mm3,qword ptr [ecx];
    pcmpeqd mm5,qword ptr [ecx];
    pandn mm3,mm2;
    pandn mm5,mm4;
    movq mm2,mm0;
    movq mm4,mm1;
    pcmpeqd mm2,mm1;
    pcmpeqd mm4,mm0;
    pandn mm2,mm3;
    pandn mm4,mm5;
    movq mm3,mm2;
    movq mm5,mm4;
    pand mm2,mm6;
    pand mm4,mm6;
    pandn mm3,mm7;
    pandn mm5,mm7;
    por mm2,mm3;
    por mm4,mm5;

    /* set *dst0 */
    movq mm3,mm2;
    punpckldq mm2,mm4;
    punpckhdq mm3,mm4;
    movq qword ptr [edx],mm2;
    movq qword ptr [edx+8],mm3;

    mov src0, eax;
    mov src1, ebx;
    mov src2, ecx;
    mov dst, edx;
    mov count, esi;
  }
}

static void internal_scale2x_16_mmx(scale2x_uint16* dst0, scale2x_uint16* dst1, const scale2x_uint16* src0, const scale2x_uint16* src1, const scale2x_uint16* src2, unsigned count) {
  internal_scale2x_16_mmx_single(dst0, src0, src1, src2, count);
  internal_scale2x_16_mmx_single(dst1, src2, src1, src0, count);
}

static void internal_scale2x_32_mmx(scale2x_uint32* dst0, scale2x_uint32* dst1, const scale2x_uint32* src0, const scale2x_uint32* src1, const scale2x_uint32* src2, unsigned count) {
  internal_scale2x_32_mmx_single(dst0, src0, src1, src2, count);
  internal_scale2x_32_mmx_single(dst1, src2, src1, src0, count);
}
#endif

#endif
