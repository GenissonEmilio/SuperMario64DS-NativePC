// THE OPAQUE 3D PASS, ON THE CARD. Compiled offline with the Windows SDK's
// fxc.exe into hal/gpu_raster_shaders.h; d3dcompiler.dll never loads at run
// time. The exact commands are in that header's own comment.
//
// EVERY DECISION HERE IS A COPY OF ntr/gx.cpp's SOFTWARE RASTER, not an
// improvement on it. The software path is the byte gate and the default, so
// where this file and that loop disagree the software one is right and this is
// the bug. The three places that matter:
//
//  1. VERTEX COLOUR INTERPOLATES SCREEN-LINEAR, which is why it is declared
//     noperspective. gx.cpp interpolates the colour with the plain screen-space
//     barycentrics (l0 * acol + l1 * bcol + l2 * ccol, no 1/w anywhere) while it
//     interpolates u and v through 1/w. A perspective-correct colour would be
//     the more "correct" picture and the wrong one.
//
//  2. THE TEXEL ARITHMETIC IS THE SAME ARITHMETIC. gx.cpp computes
//     round(vertex_colour * texel / 255) per channel with the vertex colour
//     carried as a 0..255 float, so the colour comes in here as 0..255 floats
//     rather than a packed byte colour, and the texel arrives as an 8-bit UNORM
//     sample, whose conversion to float is exactly n/255.
//
//  3. THE ALPHA RULE IS THE SAME RULE. A texel with alpha 0 is no pixel at all:
//     no colour, no coverage and no depth (gx.cpp's "transparent texel"
//     continue). The effective alpha is then the integer (poly_alpha * texel_a +
//     127) / 255 in 0..31, and at the setting this is gated on -- point sampling
//     -- an opaque-class polygon always lands on exactly 31, which makes the
//     blend below an exact replacement.
//
//  4. MODE 2 IS THE SAME TABLE LOOKUP (run hunt2, lane RENDER2). A toon or
//     highlight polygon (POLYGON_ATTR mode 2) takes its colour through the
//     frame's TOON TABLE, picked per pixel by the interpolated vertex red:
//     toon modulates the entry instead of the vertex colour, highlight
//     modulates the vertex red on all three channels and adds the entry after.
//     gx.cpp's band loop does the same arithmetic; the mode rides in the
//     polygon-ID attribute as id + 64 * mode (1 toon, 2 highlight), so the
//     vertex layout did not change.
//
// COVERAGE TRAVELS IN THE POLYGON-ID TARGET, not in the colour target's alpha,
// because the colour target's alpha has to drive the blend. Bit 7 of the R8
// target says "this engine wrote this pixel" and bits 0..5 are the polygon ID
// the shadow pass reads back; a DS polygon ID is six bits, so bit 7 is free.

struct VSIn {
    float4 pos  : POSITION;    // clip space, built on the CPU as (xn*w, yn*w, z*w, w)
    float2 uv   : TEXCOORD0;   // already divided by the bound buffer's size
    float3 col  : COLOR0;      // vertex colour, 0..255 per channel
    float2 attr : TEXCOORD1;   // x = polygon alpha 0..31, y = polygon ID 0..63
                               //     + 64 * toon mode (0 none, 1 toon, 2 highlight)
};

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    noperspective float3 col : COLOR0;
    nointerpolation float2 attr : TEXCOORD1;
};

VSOut vs_main(VSIn i)
{
    VSOut o;
    o.pos = i.pos;
    o.uv = i.uv;
    o.col = i.col;
    o.attr = i.attr;
    return o;
}

Texture2D tex0 : register(t0);
SamplerState smp0 : register(s0);

// the frame's toon table, 32 entries, rgb in 0..255 (gx.cpp's g_toon8)
cbuffer ToonTable : register(b0)
{
    float4 toon[32];
};

struct PSOut {
    float4 col : SV_TARGET0;
    uint id : SV_TARGET1;
};

PSOut shade(VSOut i, float4 t, float ta)
{
    PSOut o;
    float sa = floor((i.attr.x * ta + 127.0) / 255.0);
    uint a = (uint)(i.attr.y + 0.5);
    uint m = a >> 6;
    float3 v = i.col;
    float3 e = float3(0.0, 0.0, 0.0);
    if (m != 0u) {
        // gx.cpp's toon_index: round(red * 31 / 255), 0..31
        int k = clamp((int)(i.col.r * (31.0f / 255.0f) + 0.5f), 0, 31);
        e = toon[k].rgb;
        v = (m == 1u) ? e : i.col.rrr;
    }
    float3 f = floor(v * t.rgb + 0.5);
    if (m == 2u) f += e;
    float3 c = clamp(f, 0.0, 255.0) * (1.0 / 255.0);
    o.col = float4(c, min(sa, 31.0) * (1.0 / 31.0));
    o.id = 0x80u | (a & 63u);
    return o;
}

PSOut ps_main(VSOut i)
{
    float4 t = tex0.Sample(smp0, i.uv);
    // An untextured polygon binds a one-pixel opaque white texture, so this is
    // the same code path with t == 1 and no branch: gx.cpp's own untextured
    // case is literally texel = 0xFFFFFFFF.
    float ta = floor(t.a * 255.0 + 0.5);
    clip(ta - 0.5);                       // alpha-0 texel: not a pixel
    return shade(i, t, ta);
}

// THE SAME PIXEL UNDER TEXTUREFILTER 1 OR 2 (run hunt7, lane FILTEREDGE1). The
// texture bound here is held PREMULTIPLIED (rgb * a, 16 bits a channel), so the
// card's own bilinear / trilinear blend is the alpha-weighted blend gx.cpp's
// sample_bilinear does by hand, and dividing by the blended alpha gives the
// straight colour back: a transparent texel's colour (black, usually) never
// reaches the visible edge of a cut-out. COVERAGE IS NOT THE FILTER'S: s1 is a
// point sampler pinned to the top level, and its texel is the one TextureFilter
// 0 would have taken. Alpha 0 there is no pixel (no colour, no depth), a fully
// opaque texel is a fully opaque pixel that writes depth like any other, and
// only a texel with an alpha of its own (an A3I5 / A5I3 fade) keeps the
// filtered alpha. gx.cpp's filtered_texel_cover is this same rule.
SamplerState smpP : register(s1);

PSOut ps_filt(VSOut i)
{
    float4 tf = tex0.Sample(smp0, i.uv);
    float4 tp = tex0.SampleLevel(smpP, i.uv, 0.0);
    float pa = floor(tp.a * 255.0 + 0.5);
    clip(pa - 0.5);                       // the DS's texel is alpha 0: not a pixel
    float fa = floor(tf.a * 255.0 + 0.5);
    float ta = (pa > 254.5) ? 255.0 : ((fa > 0.5) ? fa : pa);
    float4 t = float4(saturate(tf.rgb / max(tf.a, 1.0 / 65535.0)), ta * (1.0 / 255.0));
    return shade(i, t, ta);
}

// ============================================================================
// THE EDGE-SMOOTHING PASS, ON THE CARD (run perf2, lane GPURB). A copy of
// ntr/gx.cpp's aa_band, operation for operation, not an improvement on it:
// the CPU pass stays the reference and runs whenever the card is not drawing.
//
// EXACT ARITHMETIC. Every value is computed in the same order as the C++, each
// operation rounded once to a 32-bit float, which is what `precise` asks of the
// compiler (no fused multiply-add, no reassociation) and what Direct3D 11
// requires of an add and a multiply. The pixels are integers in and integers
// out: the frame arrives as one packed 0xAARRGGBB word per texel (R32_UINT) and
// the coverage mask as one byte, and the result leaves the same way, so no
// UNORM conversion stands between the CPU's bytes and the shader's.
//
// THE ONE OPERATION DIRECT3D DOES NOT ROUND EXACTLY is the divide (2.5 ULP).
// aa_band divides once, t = |avg - lM| / range, so div_rn below finds the
// correctly rounded quotient: it tries the few floats around the card's own
// answer and keeps the one whose remainder a - q * b, computed exactly with
// Dekker's split product, is smallest. The quotient of two floats is never
// exactly half way between two floats, so there is always one winner.

Texture2D<uint> aa_src : register(t0);
Texture2D<uint> aa_cov : register(t1);

float4 aa_vs(uint id : SV_VertexID) : SV_Position
{
    // one triangle over the whole picture: (-1,1) (3,1) (-1,-3)
    float x = (id == 1) ? 3.0 : -1.0;
    float y = (id == 2) ? -3.0 : 1.0;
    return float4(x, y, 0.0, 1.0);
}

float luma(uint p)
{
    precise float r = (float)((p >> 16) & 0xFFu);
    precise float g = (float)((p >> 8) & 0xFFu);
    precise float b = (float)(p & 0xFFu);
    precise float v = 0.299f * r + 0.587f * g;
    precise float l = v + 0.114f * b;
    return l;
}

// the exact error of p = x * y, p already rounded (Dekker, no fused ops)
float prod_err(float x, float y, float p)
{
    precise float cx = 4097.0f * x;
    precise float xh = cx - (cx - x);
    precise float xl = x - xh;
    precise float cy = 4097.0f * y;
    precise float yh = cy - (cy - y);
    precise float yl = y - yh;
    precise float e = ((xh * yh - p) + xh * yl + xl * yh) + xl * yl;
    return e;
}

// a >= 0, b > 0: the float nearest a / b
float div_rn(float a, float b)
{
    if (a == 0.0f) return 0.0f;
    precise float q0 = a / b;
    float best = q0;
    precise float bestr = 1e30f;
    [unroll] for (int k = -3; k <= 3; ++k) {
        precise float q = asfloat((uint)((int)asuint(q0) + k));
        precise float p = q * b;
        precise float e = prod_err(q, b, p);
        precise float r = (a - p) - e;
        precise float ar = abs(r);
        if (ar < bestr) { bestr = ar; best = q; }
    }
    return best;
}

uint mix_ch(uint a, uint b, float s, float t)
{
    precise float v = (float)a * s + (float)b * t;
    precise float w = v + 0.5f;
    int i = (int)w;
    return (uint)clamp(i, 0, 255);
}

uint mix2(uint a, uint b, float t)
{
    precise float s = 1.0f - t;
    return 0xFF000000u |
           (mix_ch((a >> 16) & 0xFFu, (b >> 16) & 0xFFu, s, t) << 16) |
           (mix_ch((a >> 8) & 0xFFu, (b >> 8) & 0xFFu, s, t) << 8) |
           mix_ch(a & 0xFFu, b & 0xFFu, s, t);
}

uint aa_ps(float4 pos : SV_Position) : SV_Target0
{
    int w, h;
    aa_src.GetDimensions(w, h);
    int x = (int)pos.x, y = (int)pos.y;
    uint pM = aa_src.Load(int3(x, y, 0));
    if (aa_cov.Load(int3(x, y, 0)) == 0u) return pM;
    int xw = x > 0 ? x - 1 : 0;
    int xe = x + 1 < w ? x + 1 : x;
    int yn = y > 0 ? y - 1 : 0;
    int ys = y + 1 < h ? y + 1 : y;
    uint pN = aa_src.Load(int3(x, yn, 0));
    uint pS = aa_src.Load(int3(x, ys, 0));
    uint pW = aa_src.Load(int3(xw, y, 0));
    uint pE = aa_src.Load(int3(xe, y, 0));
    precise float lM = luma(pM), lN = luma(pN), lS = luma(pS);
    precise float lW = luma(pW), lE = luma(pE);
    float lo = min(min(min(min(lM, lN), lS), lW), lE);
    float hi = max(max(max(max(lM, lN), lS), lW), lE);
    precise float range = hi - lo;
    precise float hr = hi * 0.125f;
    if (range < 8.0f || range < hr) return pM;
    precise float d2x = abs((lW + lE) - 2.0f * lM);
    precise float d2y = abs((lN + lS) - 2.0f * lM);
    uint n1 = (d2x >= d2y) ? pW : pN;
    uint n2 = (d2x >= d2y) ? pE : pS;
    precise float avg = 0.25f * (((lN + lS) + lW) + lE);
    precise float t = div_rn(abs(avg - lM), range);
    t = t * t;
    if (t > 0.5f) t = 0.5f;
    if (t <= 0.002f) return pM;
    uint nb = mix2(n1, n2, 0.5f);
    return mix2(pM, nb, t);
}

// ============================================================================
// THE WHOLE 3D FRAME ON THE CARD (run cardfull, lane CARDFULL1). The pixel
// shaders below draw ntr/gx.cpp's SECOND pass -- the translucent polygons and
// the mode-3 shadow volumes -- over the targets the opaque pass filled, and the
// last one runs the edge smoothing on the card's own colour target. The same
// rule as everything above: each is a copy of what gx.cpp's band loop does, and
// where they disagree the software raster is right.
//
// THE COLOUR TARGET'S ALPHA IS COVERAGE here. The opaque pass above carries
// coverage in bit 7 of the polygon-ID target, which is enough while only the
// opaque pass draws, but a translucent fragment must not touch the ID plane (the
// shadow test reads it), so these shaders put "this pixel was drawn" in the
// colour alpha instead, through a MAX blend on the alpha channel only. A
// fragment's alpha is therefore its blend weight (sa / 31) and, at the same
// time, a nonzero coverage mark; a fragment that draws nothing outputs alpha 0
// and, with weight 0, changes neither the colour nor the mark.
//
//  ps_tl      a translucent polygon's pixels with a blend weight under 31
//  ps_tlop    the same triangles' pixels that came out at 31: gx.cpp draws
//             those as OPAQUE (depth, colour, polygon ID written, the
//             translucent-ID record cleared). A card cannot write depth for
//             some pixels of a draw and not others, so a polygon that can reach
//             31 is drawn twice, once by each shader, each keeping only its own
//             pixels.
//  ps_shadow  a shadow polygon (mode 3, polygon ID not 0), which draws only
//             where the stencil mask is set and the opaque polygon ID under it
//             differs. It never discards: a discarded fragment skips the output
//             merger and with it the stencil clear gx.cpp does at every
//             stencilled pixel the polygon touches, so "nothing to draw" is
//             alpha 0 instead.
// ============================================================================

// FILTERED = the texture is held premultiplied (16 bits a channel) and coverage
// comes from a point sampler pinned to the top level: ps_filt above, run hunt7
// lane FILTEREDGE1, applied to the whole-frame shaders by run hunt8 lane
// CARDPORT1. Alpha 0 at the DS's own texel is no pixel, a fully opaque texel is
// a fully opaque pixel, and only a texel with an alpha of its own keeps the
// filtered alpha.
void shade_full(VSOut i, bool filtered, out float3 c, out float sa, out float ta)
{
    float4 t = tex0.Sample(smp0, i.uv);
    if (filtered) {
        float4 tp = tex0.SampleLevel(smpP, i.uv, 0.0);
        float pa = floor(tp.a * 255.0 + 0.5);
        float fa = floor(t.a * 255.0 + 0.5);
        ta = (pa < 0.5) ? 0.0 : ((pa > 254.5) ? 255.0 : ((fa > 0.5) ? fa : pa));
        t = float4(saturate(t.rgb / max(t.a, 1.0 / 65535.0)), t.a);
    } else {
        ta = floor(t.a * 255.0 + 0.5);
    }
    sa = floor((i.attr.x * ta + 127.0) / 255.0);
    uint a = (uint)(i.attr.y + 0.5);
    uint m = a >> 6;
    float3 v = i.col;
    float3 e = float3(0.0, 0.0, 0.0);
    if (m != 0u) {
        int k = clamp((int)(i.col.r * (31.0f / 255.0f) + 0.5f), 0, 31);
        e = toon[k].rgb;
        v = (m == 1u) ? e : i.col.rrr;
    }
    float3 f = floor(v * t.rgb + 0.5);
    if (m == 2u) f += e;
    c = clamp(f, 0.0, 255.0) * (1.0 / 255.0);
}

// the weight of a drawn fragment, never quite zero so that coverage is marked
// even where gx.cpp's blend of a zero weight leaves the colour alone
static const float kMinW = 1.0 / 1024.0;

float4 tl_body(VSOut i, bool filtered)
{
    float3 c;
    float sa, ta;
    shade_full(i, filtered, c, sa, ta);
    clip(ta - 0.5);
    clip(30.5 - sa);
    return float4(c, max(sa * (1.0 / 31.0), kMinW));
}

PSOut tlop_body(VSOut i, bool filtered)
{
    PSOut o;
    float3 c;
    float sa, ta;
    shade_full(i, filtered, c, sa, ta);
    clip(ta - 0.5);
    clip(sa - 30.5);
    o.col = float4(c, 1.0);
    o.id = 0x80u | ((uint)(i.attr.y + 0.5) & 63u);
    return o;
}

Texture2D<uint> idtex : register(t1);

float4 shadow_body(VSOut i, bool filtered)
{
    float3 c;
    float sa, ta;
    shade_full(i, filtered, c, sa, ta);
    uint pid = (uint)(i.attr.y + 0.5) & 63u;
    uint under = idtex.Load(int3((int)i.pos.x, (int)i.pos.y, 0)) & 63u;
    if (under == pid || ta < 0.5) return float4(0.0, 0.0, 0.0, 0.0);
    return float4(c, max(sa * (1.0 / 31.0), kMinW));
}

float4 ps_tl(VSOut i) : SV_TARGET0 { return tl_body(i, false); }
float4 ps_tl_f(VSOut i) : SV_TARGET0 { return tl_body(i, true); }
PSOut ps_tlop(VSOut i) { return tlop_body(i, false); }
PSOut ps_tlop_f(VSOut i) { return tlop_body(i, true); }
float4 ps_shadow(VSOut i) : SV_TARGET0 { return shadow_body(i, false); }
float4 ps_shadow_f(VSOut i) : SV_TARGET0 { return shadow_body(i, true); }

// ----------------------------------------------------------------------------
// THE EDGE SMOOTHING ON THE CARD'S OWN COLOUR TARGET. aa_band's rule exactly as
// aa_ps above, but the frame is read straight out of the colour target
// (B8G8R8A8_UNORM: n / 255 times 255 plus a half truncates back to n for every
// 8-bit n, so no byte moves) and coverage is that target's alpha. The result
// carries its own verdict in its alpha byte, which is what lets the CPU read
// back ONE picture and still tell the three cases apart:
//   0x00  not covered: the CPU leaves the pixel as the frame's clear made it
//   0xFF  covered, smoothing left it as it was
//   0xFE  covered and smoothing changed it (counted, like aa_band's hits)
Texture2D<float4> fa_src : register(t0);

uint fa_pack(float4 c)
{
    uint r = (uint)(c.r * 255.0 + 0.5);
    uint g = (uint)(c.g * 255.0 + 0.5);
    uint b = (uint)(c.b * 255.0 + 0.5);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

uint fa_ps(float4 pos : SV_Position) : SV_Target0
{
    int w, h;
    fa_src.GetDimensions(w, h);
    int x = (int)pos.x, y = (int)pos.y;
    float4 cM = fa_src.Load(int3(x, y, 0));
    if (cM.a <= 0.0) return 0u;
    uint pM = fa_pack(cM);
    int xw = x > 0 ? x - 1 : 0;
    int xe = x + 1 < w ? x + 1 : x;
    int yn = y > 0 ? y - 1 : 0;
    int ys = y + 1 < h ? y + 1 : y;
    uint pN = fa_pack(fa_src.Load(int3(x, yn, 0)));
    uint pS = fa_pack(fa_src.Load(int3(x, ys, 0)));
    uint pW = fa_pack(fa_src.Load(int3(xw, y, 0)));
    uint pE = fa_pack(fa_src.Load(int3(xe, y, 0)));
    precise float lM = luma(pM), lN = luma(pN), lS = luma(pS);
    precise float lW = luma(pW), lE = luma(pE);
    float lo = min(min(min(min(lM, lN), lS), lW), lE);
    float hi = max(max(max(max(lM, lN), lS), lW), lE);
    precise float range = hi - lo;
    precise float hr = hi * 0.125f;
    if (range < 8.0f || range < hr) return pM;
    precise float d2x = abs((lW + lE) - 2.0f * lM);
    precise float d2y = abs((lN + lS) - 2.0f * lM);
    uint n1 = (d2x >= d2y) ? pW : pN;
    uint n2 = (d2x >= d2y) ? pE : pS;
    precise float avg = 0.25f * (((lN + lS) + lW) + lE);
    precise float t = div_rn(abs(avg - lM), range);
    t = t * t;
    if (t > 0.5f) t = 0.5f;
    if (t <= 0.002f) return pM;
    uint nb = mix2(n1, n2, 0.5f);
    uint o = mix2(pM, nb, t);
    if (o == pM) return pM;
    return (o & 0x00FFFFFFu) | 0xFE000000u;
}

// ==== THE COMPOSITE (run cardfull, lane CARD2): THE PICTURE PRESENTED FROM THE
// CARD ======================================================================
//
// With the picture-on-the-card path on, the 3D never comes back to the
// processor. The processor's 2D compositor writes only the pixels it owns into
// the framebuffer, and the alpha byte of each of them is a verdict (ntr/gx.h,
// GX_OV_*): 0x00 nothing, 0xFF a 2D pixel in front of the 3D layer, 0xFE a 2D
// pixel BEHIND it. This one pass puts the two together, the way the DS's layer
// compositor and engine A's master brightness do, into a plain colour target
// that the present quad then scales onto the window:
//
//   3D colour   the smoothed picture where the card smoothed it (t1, R32_UINT,
//               alpha 0 = not covered), else the opaque/translucent picture
//               (t0, alpha 0 = not covered); a pixel outside the box the card
//               drew in, or not covered, is the frame's clear colour inside the
//               present rectangle and the pillarbox constant outside it
//   overlay     t2, the framebuffer as the processor left it (R32_UINT,
//               0xAARRGGBB, one texel per picture pixel)
//   fade        engine A's brightness fade, the same integer arithmetic
//               port_engine_a_fade runs on the processor, over the pixels the
//               brightness mask says the DS blends (all of them when the frame
//               has no mask)
//
// EVERYTHING IS INTEGER so the result is exactly what the processor's own copy
// of this rule (hal/gpu_raster.cpp, cp_combine / cp_fade, which is what a
// settle and the capture use) makes of the same inputs: the A/B between the
// two is a check of equality, not of closeness.
cbuffer CompParams : register(b0)
{
    uint4 box;      // x0, y0, x1, y1 of the pixels the card drew in
    uint4 rect;     // the present rectangle: x0, y0, w, h
    uint4 clr;      // clear colour (0xAARRGGBB), pillarbox constant, flags, evy
                    // flags: bit 0 the frame was smoothed, bit 1 a fade is on,
                    // bit 2 the fade goes to white, bit 3 a brightness mask is
                    // bound (t3), bit 4 the 3D layer is shown, bit 5 its clear
                    // plane is opaque
};

Texture2D<float4> cp_col : register(t0);
Texture2D<uint>   cp_sm  : register(t1);
Texture2D<uint>   cp_ov  : register(t2);
// the brightness mask, one byte per picture pixel (hal/message_compositor.cpp):
//   bit 0  would the fade take this pixel if the 3D layer showed here
//   bit 1  ... if the backdrop showed here
//   bit 2  does it take the 2D pixel written here (overlay mode FF or FE)
Texture2D<uint>   cp_bm  : register(t3);

uint cp_pack(float4 c)
{
    uint r = (uint)(c.r * 255.0 + 0.5);
    uint g = (uint)(c.g * 255.0 + 0.5);
    uint b = (uint)(c.b * 255.0 + 0.5);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

float4 comp_ps(float4 pos : SV_Position) : SV_Target0
{
    int x = (int)pos.x, y = (int)pos.y;
    uint ov = cp_ov.Load(int3(x, y, 0));
    uint mode = ov >> 24;
    uint base = ((uint)x >= rect.x && (uint)x < rect.x + rect.z &&
                 (uint)y >= rect.y && (uint)y < rect.y + rect.w) ? clr.x : clr.y;
    bool cov = false;
    if ((uint)x >= box.x && (uint)x < box.z && (uint)y >= box.y && (uint)y < box.w) {
        if (clr.z & 1u) {
            uint s = cp_sm.Load(int3(x, y, 0));
            if ((s >> 24) != 0u) { cov = true; base = 0xFF000000u | (s & 0x00FFFFFFu); }
        } else {
            float4 c = cp_col.Load(int3(x, y, 0));
            if (c.a > 0.0) { cov = true; base = cp_pack(c); }
        }
    }
    uint p = base;
    if (mode == 0xFFu) p = 0xFF000000u | (ov & 0x00FFFFFFu);
    else if (mode == 0xFEu) p = cov ? base : (0xFF000000u | (ov & 0x00FFFFFFu));
    // does the fade take this pixel? Every pixel, unless a mask says otherwise.
    bool take = true;
    if (clr.z & 8u) {
        uint code = cp_bm.Load(int3(x, y, 0));
        bool is3d = ((clr.z & 32u) != 0u) || (((clr.z & 16u) != 0u) && cov);
        bool def = (code & (is3d ? 1u : 2u)) != 0u;
        if (mode == 0xFFu) take = ((code >> 2) & 1u) != 0u;
        else if (mode == 0xFEu) take = cov ? def : (((code >> 2) & 1u) != 0u);
        else take = def;
    }
    int r = (int)((p >> 16) & 0xFFu), g = (int)((p >> 8) & 0xFFu), b = (int)(p & 0xFFu);
    if ((clr.z & 2u) && take) {
        int evy = (int)clr.w;
        if (clr.z & 4u) {
            r += ((255 - r) * evy) >> 4;
            g += ((255 - g) * evy) >> 4;
            b += ((255 - b) * evy) >> 4;
        } else {
            r -= (r * evy) >> 4;
            g -= (g * evy) >> 4;
            b -= (b * evy) >> 4;
        }
    }
    return float4((float)r / 255.0, (float)g / 255.0, (float)b / 255.0, 1.0);
}

// ============================================================================
// ---- TWODGPU1 (run hunt8, lane TWODGPU1): ENGINE A'S 2D, COMPOSED ON THE CARD
// ============================================================================
// Everything below belongs to hal/gpu_raster.cpp's twod_* section. The
// processor still rasterises the 256x192 DS layers into its cell plane
// (hal/message_compositor.cpp); what used to be a host-resolution loop there is
// done here instead, with the same integer rules, into the overlay target
// (R32_UINT, 0xAARRGGBB, alpha = ntr/gx.h's GX_OV_* verdict) and the brightness
// mask target (R8_UINT, the encoding in message_compositor.cpp) that comp_ps
// reads. Keep this section self-contained so a fold can merge and regenerate.
//
// tw0: active_w, active_h, present_h, flags (bit 0 widescreen, bit 2 the window
//      plane is uniform: every cell answers tw1.w)
// tw1: sx (the 4:3 integer scale), half margin, backdrop colour, outside mask
// tw2: target w, target h, BLDCNT first-target bits, 0
// tw3: box x0, y0, x1, y1 (the box passes)
cbuffer TwParams : register(b0)
{
    uint4 tw0;
    uint4 tw1;
    uint4 tw2;
    uint4 tw3;
};

Buffer<uint4>     tw_inst : register(t4);   // one rect per 2D cell (see tw_rect_vs)
Texture2D<uint>   tw_wm   : register(t5);   // the window mask of every DS cell, 256x192
Texture2D<uint>   tw_up   : register(t6);   // pixels the processor wrote itself

// The window mask message_compositor.cpp's bm_fill_default / hide_3d_outside_windows
// give a host pixel: the host row's DS row, the host column's DS column the way
// the message layer is placed, and WINOUT (or all layers) outside the DS panel.
uint tw_wmask(uint hx, uint hy)
{
    uint y = (hy * 192u) / tw0.z;
    uint x;
    bool xin = true;
    if (tw0.w & 1u) {
        int d = (int)hx - (int)tw1.y;
        xin = d >= 0;
        x = xin ? (((uint)d * 192u) / tw0.z) : 0u;
    } else {
        x = hx / tw1.x;
    }
    if (!(xin && x < 256u && y < 192u)) return tw1.w;
    if (tw0.w & 4u) return tw1.w;
    return tw_wm.Load(int3((int)x, (int)y, 0));
}

uint tw_bmv(uint layer, uint wm)
{
    return ((wm & 0x20u) != 0u && (tw2.z & (1u << layer)) != 0u) ? 1u : 0u;
}

uint tw_hide_ps(float4 pos : SV_Position) : SV_Target0
{
    uint wm = tw_wmask((uint)pos.x, (uint)pos.y);
    if (wm & 1u) discard;
    return tw1.z;
}

uint tw_bmdef_ps(float4 pos : SV_Position) : SV_Target0
{
    uint wm = tw_wmask((uint)pos.x, (uint)pos.y);
    return tw_bmv(0u, wm) | (tw_bmv(5u, wm) << 1);
}

struct TwV {
    float4 pos : SV_Position;
    nointerpolation uint iid : TEXCOORD0;
};

float4 tw_ndc(float x, float y)
{
    return float4(x * 2.0 / (float)tw2.x - 1.0, 1.0 - y * 2.0 / (float)tw2.y, 0.0, 1.0);
}

// instance: x = x0 | y0 << 16, y = w | h << 16, z = colour | verdict << 24, w = bit 0 bmv
TwV tw_rect_vs(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    uint4 a = tw_inst[iid];
    float cx = (float)(vid & 1u), cy = (float)(vid >> 1);
    float x0 = (float)(a.x & 0xFFFFu), y0 = (float)(a.x >> 16);
    float w = (float)(a.y & 0xFFFFu), h = (float)(a.y >> 16);
    TwV o;
    o.pos = tw_ndc(x0 + cx * w, y0 + cy * h);
    o.iid = iid;
    return o;
}

struct TwOut {
    uint c0 : SV_Target0;
    uint c1 : SV_Target1;
};

TwOut tw_rect_ps(TwV i)
{
    uint4 a = tw_inst[i.iid];
    uint wm = tw_wmask((uint)i.pos.x, (uint)i.pos.y);
    TwOut o;
    o.c0 = a.z;
    o.c1 = (tw_bmv(0u, wm) | (tw_bmv(5u, wm) << 1)) | ((a.w & 1u) != 0u ? 4u : 0u) |
           ((a.z >> 24) == 0xFFu ? 8u : 16u);
    return o;
}

TwV tw_box_vs(uint vid : SV_VertexID)
{
    float cx = (float)(vid & 1u), cy = (float)(vid >> 1);
    TwV o;
    o.pos = tw_ndc((float)tw3.x + cx * (float)(tw3.z - tw3.x),
                   (float)tw3.y + cy * (float)(tw3.w - tw3.y));
    o.iid = 0u;
    return o;
}

// the inset panel's one verdict: a 2D pixel in front, taken by the fade
uint tw_inset_ps(TwV i) : SV_Target0
{
    uint wm = tw_wmask((uint)i.pos.x, (uint)i.pos.y);
    return (tw_bmv(0u, wm) | (tw_bmv(5u, wm) << 1)) | 4u | 8u;
}

// the pixels the processor wrote itself over the target (zero alpha = untouched)
uint tw_boxup_ps(TwV i) : SV_Target0
{
    uint v = tw_up.Load(int3((int)i.pos.x, (int)i.pos.y, 0));
    if ((v >> 24) == 0u) discard;
    return v;
}
