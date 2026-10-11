/* pause_page.inc -- the new pause menu's drawing (run menu1, lane PAUSE3). Included at the end of hal/settings_page.cpp so it can use the page's own
   drawing helpers (the parchment sheet, wooden pieces, the DS lettering renderer) and nothing else; there is no ROM here.

   pause_page_draw is the draw callback of the pause recipe's one custom piece (MenuPiece.custom, hal/menu_compose.cpp). It reads the layout's
   facts blob (PauseFacts, hal/menu_pause_geo.h) and draws, in this order: the star tracker (wide: always; 4:3: while it is slid in), the dialog sheet
   of the save pages, the 4:3 edge tabs and the pointer beside the lit plank. The planks themselves are hostimg pieces the compose draws after this
   (they carry the hit boxes and the lit / pressed look). The settings panel is the page's own draw (settings_page_draw, the overlay).

   NOTE for the identifiers below: S and L are macros in this file (the page state and layout): never name a variable S or L here. */

namespace {

/* a premultiplied host picture scaled to dw x dh blitted at (dx, dy); alpha 0..255 scales it, light adds white under its own alpha, shade darkens */
void blit_hi(const Surf &sf, int id, int dx, int dy, int dw, int dh, int alpha = 255, int light = 0, int shade = 0)
{
    if (dw < 1 || dh < 1) return;
    const uint32_t *src = menu_hostimg_scaled(id, dw, dh);
    if (!src) return;
    for (int y = 0; y < dh; ++y) {
        const int yy = dy + y;
        if (yy < 0 || yy >= sf.h) continue;
        uint32_t *row = sf.px + (size_t)yy * sf.stride;
        const uint32_t *sp = src + (size_t)y * dw;
        int x0 = dx < 0 ? -dx : 0, x1 = dx + dw > sf.w ? sf.w - dx : dw;
        for (int x = x0; x < x1; ++x) {
            const uint32_t c = sp[x];
            int a = c >> 24;
            if (!a) continue;
            int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
            if (shade) { r = r * (255 - shade) / 255; g = g * (255 - shade) / 255; b = b * (255 - shade) / 255; }
            if (light) { r += a * light / 255; g += a * light / 255; b += a * light / 255; if (r > a) r = a; if (g > a) g = a; if (b > a) b = a; }
            if (alpha < 255) { r = r * alpha / 255; g = g * alpha / 255; b = b * alpha / 255; a = a * alpha / 255; }
            uint32_t &o = row[dx + x];
            o = over(o, r, g, b, a);
        }
    }
}

const Piece *arrow_left_piece(int w, int h)
{
    Piece *p = find_piece(7, w, h, 0);
    if (p) return p;
    const Piece *r = arrow_right_piece(w, h);
    p = new_piece(7, w, h, 0, w, h);
    if (!r) return p;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) p->px[(size_t)y * w + x] = r->px[(size_t)y * w + (w - 1 - x)];
    return p;
}

const uint32_t PP_BROWN = 0x7A3A0C, PP_FADED = 0xA67C4A, PP_DKBROWN = 0x5C2C0A;

/* the course's title, shrunk to fit a plank */
void pp_title_text(const Surf &sf, int cx, int cy, int maxw, const char *t, double em)
{
    float e = (float)em;
    const float need = ui_text_width(t, e);
    if (need > maxw) e *= maxw / need;
    const float w = ui_text_width(t, e);
    ui_text_draw(&sf, cx - w * 0.5f, cy + ui_text_cap_height(e) * 0.5f, t, e, PP_BROWN, 255, 0);
}

void pp_star_row(const Surf &sf, int x, int y, int rowh, int icon, const char *name, int got, double sc, int maxw)
{
    blit_hi(sf, got ? MENU_IMG_P_STAR_GOT : MENU_IMG_P_STAR_OFF, x, y + (rowh - icon) / 2, icon, icon);
    float em = (float)(31 * sc);
    const float room = (float)(maxw - icon - 18 * sc);       /* a long name shrinks to fit the sheet */
    const float need = ui_text_width(name, em);
    if (need > room && room > 0) em *= room / need;
    ui_text_draw(&sf, (float)(x + icon + 18 * sc), y + rowh * 0.5f + ui_text_cap_height(em) * 0.5f, name, em, got ? PP_BROWN : PP_FADED, 255, 0);
}

void pp_stat_row(const Surf &sf, int x, int y, int w, int rowh, int icon_id, const char *label, const char *note, const char *val, double sc)
{
    const int ic = (int)(40 * sc + 0.5);
    blit_hi(sf, icon_id, x + (int)(70 * sc), y + (rowh - ic) / 2, ic, ic);
    const float em = (float)(33 * sc);
    ui_text_draw(&sf, (float)(x + 70 * sc + ic + 18 * sc), y + rowh * 0.5f + ui_text_cap_height(em) * 0.5f, label, em, PP_BROWN, 255, 0);
    if (note && note[0]) {
        const float ne = (float)(26 * sc);
        const float nw = ui_text_width(note, ne);
        ui_text_draw(&sf, x + w - 230 * sc - nw, y + rowh * 0.5f + ui_text_cap_height(ne) * 0.5f, note, ne, PP_FADED, 255, 0);
    }
    const float ve = (float)(36 * sc);
    const float vw = ui_text_width(val, ve);
    ui_text_draw(&sf, x + w - 70 * sc - vw, y + rowh * 0.5f + ui_text_cap_height(ve) * 0.5f, val, ve, PP_BROWN, 255, 0);
}

void pp_tracker(const Surf &sf, const PauseGeo &g, const PauseFacts *f)
{
    const double sc = g.tsc;
    const int dx = pause_tracker_dx(g, f);
    const int x = g.tx + dx, y = g.ty, w = g.tw, h = pause_tracker_h(g, f);
    if (x >= sf.w || x + w <= 0) return;
    const Piece *sheet = sheet_piece(w, h, (int)(sc * 100 + 0.5));
    if (sheet) blit_piece(sf, sheet, x, y);
    /* the title plank across the top edge */
    const int tw = (int)(640 * sc + 0.5), th = tw * 256 / 960;
    const int tcx = x + w / 2, tcy = y + (int)(8 * sc);
    blit_hi(sf, MENU_IMG_P_TITLE_BLANK, tcx - tw / 2, tcy - th / 2, tw, th);
    pp_title_text(sf, tcx, tcy, (int)(tw * 0.72), f->title, th * 0.40);
    /* the page row: the course arrows and the page name */
    const int row_cy = y + (int)((110 + 20) * sc + 0.5);
    {
        const float em = (float)(34 * sc);
        const float tw2 = ui_text_width(f->sub, em);
        ui_text_draw(&sf, x + w * 0.5f - tw2 * 0.5f, row_cy + ui_text_cap_height(em) * 0.5f, f->sub, em, PP_BROWN, 255, 0);
    }
    if (f->paging) {
        const int aw = (int)(28 * sc + 0.5), ah = (int)(40 * sc + 0.5);
        const Piece *al = arrow_left_piece(aw, ah), *ar = arrow_right_piece(aw, ah);
        if (al) blit(sf, al->px, al->w, al->h, x + (int)(104 * sc), row_cy - ah / 2, 255, 0, 0, 0);
        if (ar) blit(sf, ar->px, ar->w, ar->h, x + w - (int)(104 * sc) - aw, row_cy - ah / 2, 255, 0, 0, 0);
        const float lb = (float)(26 * sc);
        ui_text_draw(&sf, (float)(x + 36 * sc), row_cy + ui_text_cap_height(lb) * 0.5f, "LB", lb, PP_FADED, 255, 0);
        const float rw = ui_text_width("RB", lb);
        ui_text_draw(&sf, x + w - 36 * sc - rw, row_cy + ui_text_cap_height(lb) * 0.5f, "RB", lb, PP_FADED, 255, 0);
    }
    int yy = y + (int)((110 + 20 + 48) * sc + 0.5);
    const int rowh = (int)(54 * sc + 0.5), ic = (int)(40 * sc + 0.5);
    const int lx = x + (int)(70 * sc);
    char buf[40];
    if (f->course >= 15) {
        /* the castle's secret stars: a grid of thirty stars, the first stars_got lit */
        const int cell = (int)(70 * sc + 0.5), gic = (int)(44 * sc + 0.5);
        const int gx = x + (w - 10 * cell) / 2;
        for (int i = 0; i < 30; ++i)
            blit_hi(sf, i < f->stars_got ? MENU_IMG_P_STAR_GOT : MENU_IMG_P_STAR_OFF, gx + (i % 10) * cell + (cell - gic) / 2, yy + (i / 10) * rowh + (rowh - gic) / 2, gic, gic);
        yy += 4 * rowh;
    } else {
        for (int i = 0; i < 7; ++i) {
            if (!f->names[i][0]) { yy += rowh; continue; }
            pp_star_row(sf, lx, yy, rowh, ic, f->names[i], (f->got_mask >> (i + 1)) & 1, sc, (int)(w - 70 * sc - 50 * sc));
            yy += rowh;
        }
        pp_star_row(sf, lx, yy, rowh, ic, "100 Coins", f->got_mask & 1, sc, (int)(w - 70 * sc - 50 * sc));
        yy += rowh;
    }
    yy += (int)(6 * sc);
    fill_rrect(sf, (float)(x + 60 * sc), (float)yy, (float)(w - 120 * sc), (float)(4 * sc > 3 ? 4 * sc : 3), 2.f, PP_DKBROWN, 255, 0);
    yy += (int)(18 * sc);
    const int sh = (int)(58 * sc + 0.5);
    if (f->course >= 15) {
        snprintf(buf, sizeof buf, "%d / 30", f->stars_got);
        pp_stat_row(sf, x, yy, w, sh, MENU_IMG_P_STAR_GOT, "Secret stars", 0, buf, sc);
        yy += sh;
        snprintf(buf, sizeof buf, "%d / 150", f->total_n);
        pp_stat_row(sf, x, yy, w, sh, MENU_IMG_P_STAR_GOT, "Total stars", 0, buf, sc);
    } else {
        snprintf(buf, sizeof buf, "%d / 8", f->stars_got);
        pp_stat_row(sf, x, yy, w, sh, MENU_IMG_P_STAR_GOT, "Stars", 0, buf, sc);
        yy += sh;
        if (f->in_course) {
            char note[24];
            snprintf(note, sizeof note, "record %d", f->coin_rec);
            snprintf(buf, sizeof buf, "%d", f->coins);
            pp_stat_row(sf, x, yy, w, sh, MENU_IMG_P_COIN_GOLD, "Coins", note, buf, sc);
            yy += sh;
            if (f->show_red) {
                snprintf(buf, sizeof buf, "%d / 8", f->red);
                pp_stat_row(sf, x, yy, w, sh, MENU_IMG_P_COIN_RED, "Red coins", 0, buf, sc);
            }
        } else {
            snprintf(buf, sizeof buf, "%d", f->coin_rec);
            pp_stat_row(sf, x, yy, w, sh, MENU_IMG_P_COIN_GOLD, "Coin record", 0, buf, sc);
        }
    }
}

/* the save pages' sheet: title plank, the words, (the planks are the recipe's pieces on top) */
void pp_dialog(const Surf &sf, const PauseGeo &g, const PauseFacts *f)
{
    const PauseRect d = pause_dialog_rect(g, f, f->variant);
    if (d.x >= sf.w || d.x + d.w <= 0) return;
    const double u = g.u;
    const Piece *sheet = sheet_piece(d.w, d.h, (int)(u * 100 + 0.5));
    if (sheet) blit_piece(sf, sheet, d.x, d.y);
    const char *title = "SAVE";
    const char *l1 = "Save your progress", *l2 = "and quit to the title?";
    if (f->variant == PAUSE_V_STATUS) {
        title = f->status ? "SAVED" : "SAVING";
        l1 = f->status ? "File saved." : "Saving file...";
        l2 = f->status ? "Returning to the title..." : "Do not turn off the power.";
    } else if (f->variant == PAUSE_V_NOSAVE) {
        title = "QUIT";
        l1 = "Really quit without saving?";
        l2 = "Progress since your last save is lost.";
    }
    const int tw = (int)(380 * u + 0.5), th = tw * 256 / 960;
    const int tcx = d.x + d.w / 2, tcy = d.y + (int)(4 * u);
    blit_hi(sf, MENU_IMG_P_TITLE_BLANK, tcx - tw / 2, tcy - th / 2, tw, th);
    pp_title_text(sf, tcx, tcy, (int)(tw * 0.72), title, th * 0.40);
    const float em = (float)(30 * u);
    const int ty0 = d.y + (int)(118 * u);
    const float w1 = ui_text_width(l1, em);
    float e1 = em, e2 = (float)(27 * u);
    float n1 = ui_text_width(l1, e1);
    if (n1 > d.w - 60 * u) e1 *= (float)((d.w - 60 * u) / n1);
    (void)w1;
    ui_text_draw(&sf, tcx - ui_text_width(l1, e1) * 0.5f, ty0 + ui_text_cap_height(e1) * 0.5f, l1, e1, PP_BROWN, 255, 0);
    if (l2[0]) {
        const float n2 = ui_text_width(l2, e2);
        if (n2 > d.w - 60 * u) e2 *= (float)((d.w - 60 * u) / n2);
        ui_text_draw(&sf, tcx - ui_text_width(l2, e2) * 0.5f, ty0 + 44 * u + ui_text_cap_height(e2) * 0.5f, l2, e2, PP_FADED, 255, 0);
    }
    if (f->variant == PAUSE_V_STATUS && !f->status) {
        /* three dots chasing each other while the file is written */
        const int ph = (f->frame / 10) % 3;
        for (int i = 0; i < 3; ++i)
            fill_circle(sf, tcx + (i - 1) * 34 * (float)u, d.y + d.h - 60 * (float)u, (float)(7 * u), i == ph ? PP_BROWN : PP_FADED, 255, 0);
    }
}

/* the 4:3 edge tabs and the pointer beside the lit plank */
void pp_tabs(const Surf &sf, const PauseGeo &g, const PauseFacts *f)
{
    for (int side = 0; side < 2; ++side) {
        const PauseRect r = pause_tab_rect(g, side);
        const int hov = f->hover_tab == side, prs = f->press_tab == side;
        blit_hi(sf, side ? MENU_IMG_P_TAB_R : MENU_IMG_P_TAB_L, r.x + (prs ? (side ? -(int)(2 * g.u) : (int)(2 * g.u)) : 0), r.y, r.w, r.h, 255, hov ? 30 : 0, prs ? 25 : 0);
    }
}

void pp_pointer(const Surf &sf, const PauseGeo &g, const PauseFacts *f)
{
    if (f->sel_slot < 0 || f->sel_slot >= f->nslots || f->settings_open) return;
    PauseRect r;
    if (f->variant == PAUSE_V_CASTLE || f->variant == PAUSE_V_COURSE) r = pause_plank_rect(g, f, f->nslots, f->sel_slot);
    else r = pause_dialog_plank(g, f, f->variant, f->nslots, f->sel_slot);
    const int ah = (int)(r.h * 0.5 + 0.5), aw = (int)(ah * 24.0 / 34.0 + 0.5);
    const Piece *ap = arrow_right_piece(aw, ah);
    if (!ap) return;
    static const int bob[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
    const int ax = r.x - aw - (int)(22 * g.u) + (int)(bob[(f->frame >> 1) & 7] * g.u * 0.5);
    blit(sf, ap->px, ap->w, ap->h, ax, r.y + r.h / 2 - ah / 2, 255, 0, 0, 0);
}

}  // namespace

extern "C" void pause_page_draw(const MenuLayout *lay, int piece, uint32_t *dst, int w, int h, int stride)
{
    (void)piece;
    const PauseFacts *f = (const PauseFacts *)lay->facts.blob;
    if (f->magic != PAUSE_FACTS_MAGIC || w < 64 || h < 64) return;
    const Surf sf = { dst, stride, w, h };
    const PauseGeo g = pause_geo(w, h, f, f->nslots);
    const int dialog = f->variant == PAUSE_V_SAVE || f->variant == PAUSE_V_STATUS || f->variant == PAUSE_V_NOSAVE;
    const int main_page = f->variant == PAUSE_V_CASTLE || f->variant == PAUSE_V_COURSE;
    if (g.wide || (main_page && f->slide_t1000 > 0)) pp_tracker(sf, g, f);
    if (dialog) pp_dialog(sf, g, f);
    if (!g.wide && main_page && !f->settings_open) pp_tabs(sf, g, f);
    if (!(!g.wide && f->slide_t1000 > 500)) pp_pointer(sf, g, f);
}

/* the pictures the pause draws itself, for the warm-up to decode and scale ahead of the first open: idx 0.. -> (id, dw, dh), 1 while idx is valid */
extern "C" int pause_page_art(int w, int h, int idx, int *id, int *dw, int *dh)
{
    if (w < 64 || h < 64) return 0;
    PauseFacts f;
    memset(&f, 0, sizeof f);
    f.variant = PAUSE_V_CASTLE;
    const PauseGeo g = pause_geo(w, h, &f, 3);
    const double sc = g.tsc;
    const int ic = (int)(40 * sc + 0.5), gic = (int)(44 * sc + 0.5), tw = (int)(640 * sc + 0.5), dtw = (int)(380 * g.u + 0.5);
    struct It { int id, w, h; } it[] = {
        { MENU_IMG_P_TITLE_BLANK, tw, tw * 256 / 960 }, { MENU_IMG_P_STAR_GOT, ic, ic }, { MENU_IMG_P_STAR_OFF, ic, ic },
        { MENU_IMG_P_COIN_GOLD, ic, ic }, { MENU_IMG_P_COIN_RED, ic, ic }, { MENU_IMG_P_STAR_GOT, gic, gic }, { MENU_IMG_P_STAR_OFF, gic, gic },
        { MENU_IMG_P_TITLE_BLANK, dtw, dtw * 256 / 960 }, { MENU_IMG_P_TAB_L, g.tabw, g.tabh }, { MENU_IMG_P_TAB_R, g.tabw, g.tabh },
    };
    if (idx < 0 || idx >= (int)(sizeof it / sizeof it[0])) return 0;
    *id = it[idx].id; *dw = it[idx].w; *dh = it[idx].h;
    return 1;
}
