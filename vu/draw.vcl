; _____     ___ ____     ___ ____
;  ____|   |    ____|   |        | |____|
; |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
;-----------------------------------------------------------------------
; (c) 2020 h4570 Sandro Sobczyński <sandro.sobczynski@gmail.com>
; Licenced under Academic Free License version 2.0
; Review ps2sdk README & LICENSE files for further details.
;
;
;---------------------------------------------------------------
; draw_3D.vcl                                                   |
;---------------------------------------------------------------
; A VU1 microprogram to draw 3D object using XYZ2, RGBAQ and ST|
; This program uses double buffering (xtop)                    |
;                                                              |
; Many thanks to:                                              |
; - Dr Henry Fortuna                                           |
; - Jesper Svennevid, Daniel Collin                            |
; - Guilherme Lampert                                          |
;---------------------------------------------------------------

.syntax new
.name VU1Draw3D
.vu
.init_vf_all
.init_vi_all

--enter
--endenter

    ;/////////// --- Prepare registers --- /////////////
    iaddiu      clipFlagsMask,      VI00,       0x400   ; Setup bitwise AND value for clip flag
    iaddiu      stIdxFlagsMask,     VI00,       0x3     ; Setup bitwise AND value for st idx flag
    
    iaddiu      stTexXFlagsMask,    VI00,       0x3c    ; Setup bitwise AND value for tex X coord
    iaddiu      stTexYFlagsMask,    VI00,       0x3c0   ; Setup bitwise AND value for tex Y coord

    iaddiu      vertCountMask,      VI00,       0x7FFF  ; Setup bitwise AND value for vert count
    
    loi         (1.0/(16.0*4.0))                        ; Load the X ST offset (texture size + divide by 4(1<<2) to account for bitshift)
    addi.x      stOffset,           VF00,       I       ; Put value into x field of vector
    loi         (1.0/(16.0*64.0))                       ; Load the Y ST offset (texture size + divide by 64(1<<(2+4)) to account for bitshift)
    addi.y      stOffset,           VF00,       I       ; Put value into y field of vector
    ;////////////////////////////////////////////


    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh

    fcset   0x000000    ; VCL won't let us use CLIP without first zeroing
                        ; the clip flags

    xtop    iBase

    lqi      matrixRow0,     (iBase++) ; load view-projection matrix
    lqi      matrixRow1,     (iBase++)
    lqi      matrixRow2,     (iBase++)
    lqi      matrixRow3,     (iBase++)

    lqi      scale,          (iBase++) ; load program params
                                       ; float : X, Y, Z - scale vector that we will use to scale the verts after projecting them.
                                       ; float : W - vert count.
    lqi      gifSetTag,      (iBase++) ; GIF tag - set
    lqi      texGifTag1,     (iBase++) ; GIF tag - texture LOD
    lqi      texGifTag2,     (iBase++) ; GIF tag - texture buffer & CLUT
    lqi      rgba,           (iBase++) ; RGBA
                                       ; u32 : R, G, B, A (0-128)
    mtir     totVertCnt,       scale[w]; load tot vert count from scale vector

    ;////////////////////////////////////////////


    ;//////////// --- Load data 2 --- /////////////
    ; This is done every start of run
runStart:

    lqi      primTag,        (iBase++)                  ; load the GIF prim tag, contains the vert count in the lower 15 bits
    mtir     vertCount,      primTag[x]                 ; load vert count with EOP bit
    iand     vertCount,      vertCount,   vertCountMask ; Mask out the EOP bit to get only the vert count

    iadd     kickAddress,    iBase,       vertCount     ; pointer for XGKICK
    iadd     destAddress,    iBase,       vertCount     ; helper pointer for data inserting

    ;////////////////////////////////////////////


    ;/////////// --- Store tags --- /////////////
    sqi gifSetTag,  (destAddress++) ;
    sqi texGifTag1, (destAddress++) ; texture LOD tag
    sqi gifSetTag,  (destAddress++) ;
    sqi texGifTag2, (destAddress++) ; texture buffer & CLUT tag
    sqi primTag,    (destAddress++) ; prim + tell gs how many data will be
    ;////////////////////////////////////////////


    ;/////////////// --- Loop --- ///////////////
    ;iadd vertexCounter, VI00, vertCount ; loop vertCount times
    vertexLoop:
    ; Loop unrolling (4 min iterations, 4 slop count)
    --LoopCS 4,4

        ;////////// --- Load loop data --- //////////
        lqi vertex, (iBase++)          ; load xyz & flags
                                       ; float : X, Y, Z
                                       ; u32 : flags     
        itof4.xyz vertex, vertex
        ;////////////////////////////////////////////    


        ;////////////// --- Vertex --- //////////////
        mtir        vertexFlags,         vertex[w]      ; Load the w vertex flags

        mul         acc,    matrixRow0, vertex[x]   ; transform each vertex by the matrix
        madd        acc,    matrixRow1, vertex[y]
        madd        acc,    matrixRow2, vertex[z]
        madd        vertex, matrixRow3, vf00[w]     ; Ignore W and assume 1.0f
       
        clipw.xyz   vertex,    vertex                       ; Dr. Fortuna: This instruction checks if the vertex is outside
                                                            ; the viewing frustum. If it is, then the appropriate
                                                            ; clipping flags are set
        fcand		VI01,      0x3FFFF                      ; Bitwise AND the clipping flags with 0x3FFFF, this makes
                                                            ; sure that we get the clipping judgement for the last three
                                                            ; verts (i.e. that make up the triangle we are about to draw)
        iand        clipFlags, vertexFlags, clipFlagsMask   ; Get the clipping flags from the vertex flags
        ior         VI01,      VI01,        clipFlags       ; Or the clipping checks with our flags
        iaddiu      iADC,      VI01,        0x7FFF          ; Add 0x7FFF. If any of the clipping flags were set this will
                                                            ; cause the triangle not to be drawn (any values above 0x8000
                                                            ; that are stored in the w component of XYZ2 will set the ADC
                                                            ; bit, which tells the GS not to perform a drawing kick on this
                                                            ; triangle.

        isw.w       iADC,      2(destAddress)           ; Write into .w of XYZ2 the iADC to kick or not
        
        div         q,         vf00[w],     vertex[w]   ; perspective divide (1/vert[w]):
        mulq.xyz    vertex,    vertex,      q
        mula.xyz    acc,       scale,       vf00[w]     ; scale to GS screen space
        madd.xyz    vertex,    vertex,      scale       ; multiply and add the scales -> vert = vert * scale + scale
        ftoi4.xyz   vertex,    vertex                   ; convert vertex to 12:4 fixed point format
        ;////////////////////////////////////////////


        ;//////////////// --- ST --- ////////////////
        iand        stIdx,     vertexFlags, stIdxFlagsMask      ; Get the ST ID by masking it from the flags
        lq          stq,       0(stIdx)                         ; Load the base ST from the top of the memory by using the id
        adda.xy     acc,       stq,         VF00                ; Move xy it into acc for the last madd 

        iand        texX,      vertexFlags, stTexXFlagsMask     ; Mask out texture X coord
        mfir.x      texOffset, texX                             ; And move the integer into the float register
        iand        texY,      vertexFlags, stTexYFlagsMask     ; Mask out texture Y coord
        mfir.y      texOffset, texY                             ; And move the integer into the float register

        itof0.xy    texOffset, texOffset                        ; Turn integer into floats
        madd.xy     stq,       stOffset,    texOffset           ; STQ = ACC (base ST coords) + stOffset (bitshift + scale) * texOffset (Tex X,Y)
        mulq        modStq,    stq,         q                   ; Perspective correction
        ;////////////////////////////////////////////


        ;//////////// --- Store data --- ////////////
        sqi modStq,      (destAddress++)      ; STQ
        sqi rgba,        (destAddress++)      ; RGBA ; q is grabbed from stq
        sqi.xyz vertex,  (destAddress++)      ; XYZ2 (.w is written before)
        ;////////////////////////////////////////////

        iaddi   vertCount,  vertCount,  -1         ; decrement the loop counter 
        ibne    vertCount,  VI00,       vertexLoop ; and repeat if needed

    ;//////////////////////////////////////////// 

    --barrier

    xgkick kickAddress ; dispatch to the GS rasterizer.

    --barrier

    --cont        ; Wait for the next batch of data

    xtop    iBase       ; Fetch the new top pointer
    b       runStart    ; And jump back at the start of loading the data

--exit
--endexit
