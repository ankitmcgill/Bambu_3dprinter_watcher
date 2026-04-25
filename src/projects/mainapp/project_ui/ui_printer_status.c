// UI_PRINTER_STATUS

#include "ui_printer_status.h"
#include <stdio.h>

// ── Colors ────────────────────────────────────────────────────────────────────
#define C_BG     lv_color_hex(0x0D0D0D)
#define C_SEP    lv_color_hex(0x262626)
#define C_WHITE  lv_color_hex(0xEEEEEE)
#define C_GRAY   lv_color_hex(0x5C5C5C)
#define C_ORANGE lv_color_hex(0xFF7830)
#define C_CYAN   lv_color_hex(0x00C8E0)
#define C_GREEN  lv_color_hex(0x38DE78)
#define C_PURPLE lv_color_hex(0xA040F0)
#define C_YELLOW lv_color_hex(0xF5B420)
#define C_RED    lv_color_hex(0xFF3A3A)

// ── Fonts ─────────────────────────────────────────────────────────────────────
#define F_S  (&lv_font_montserrat_8)    // sub-labels, units
#define F_M  (&lv_font_montserrat_10)   // section labels, footer
#define F_L  (&lv_font_montserrat_18)   // large values

// ── Section heights — must sum to display VRES (320) ─────────────────────────
#define H_HEADER    40
#define H_TEMP      48
#define H_PROGRESS  64
#define H_LAYER     48
#define H_TIME      50
#define H_FOOTER    22

// ── Widget handles for update ─────────────────────────────────────────────────
static struct {
    lv_obj_t *status_dot;
    lv_obj_t *status_text;
    lv_obj_t *bed_actual;
    lv_obj_t *bed_target;
    lv_obj_t *nozzle_actual;
    lv_obj_t *nozzle_target;
    lv_obj_t *arc;
    lv_obj_t *arc_lbl;
    lv_obj_t *progress_bar;
    lv_obj_t *progress_lbl;
    lv_obj_t *layer_cur;
    lv_obj_t *layer_tot;
    lv_obj_t *time_val;
    lv_obj_t *footer_lbl;
} s_w;

// ── Internal helpers ──────────────────────────────────────────────────────────

static void s_clear_chrome(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

// Horizontal flex row, children vertically centered
static lv_obj_t *s_hflex(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    s_clear_chrome(obj);
    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return obj;
}

// Vertical flex column, children left-aligned
static lv_obj_t *s_vflex(lv_obj_t *parent, lv_coord_t w, lv_coord_t row_gap)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, LV_SIZE_CONTENT);
    s_clear_chrome(obj);
    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(obj, row_gap, 0);
    return obj;
}

// Styled label
static lv_obj_t *s_label(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

// Circular icon with a tinted background
static lv_obj_t *s_icon(lv_obj_t *parent, lv_color_t color, const char *sym)
{
    lv_obj_t *circ = lv_obj_create(parent);
    lv_obj_set_size(circ, 28, 28);
    lv_obj_set_style_bg_color(circ, color, 0);
    lv_obj_set_style_bg_opa(circ, 28, 0);
    lv_obj_set_style_border_color(circ, color, 0);
    lv_obj_set_style_border_width(circ, 1, 0);
    lv_obj_set_style_border_opa(circ, LV_OPA_40, 0);
    lv_obj_set_style_radius(circ, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(circ, 0, 0);
    lv_obj_set_scrollbar_mode(circ, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *lbl = lv_label_create(circ);
    lv_label_set_text(lbl, sym);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, F_S, 0);
    lv_obj_center(lbl);
    return circ;
}

// Full-width section row with optional bottom separator
static lv_obj_t *s_section(lv_obj_t *parent, lv_coord_t h, bool sep)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), h);
    lv_obj_set_style_bg_color(row, C_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_hor(row, 8, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    if(sep) {
        lv_obj_set_style_border_color(row, C_SEP, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
    } else {
        lv_obj_set_style_border_width(row, 0, 0);
    }
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    return row;
}

// ── Section builders ──────────────────────────────────────────────────────────

static void s_build_header(lv_obj_t *parent)
{
    lv_obj_t *sec = lv_obj_create(parent);
    lv_obj_set_size(sec, LV_PCT(100), H_HEADER);
    lv_obj_set_style_bg_color(sec, C_BG, 0);
    lv_obj_set_style_bg_opa(sec, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(sec, C_SEP, 0);
    lv_obj_set_style_border_side(sec, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(sec, 1, 0);
    lv_obj_set_style_radius(sec, 0, 0);
    lv_obj_set_style_pad_all(sec, 4, 0);
    lv_obj_set_scrollbar_mode(sec, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_layout(sec, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sec, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sec, 4, 0);

    lv_obj_t *title = s_label(sec, "PRINTER STATUS", F_M, C_WHITE);
    lv_obj_set_style_text_letter_space(title, 1, 0);

    lv_obj_t *sr = s_hflex(sec, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(sr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(sr, 5, 0);

    s_w.status_dot = lv_obj_create(sr);
    lv_obj_set_size(s_w.status_dot, 8, 8);
    lv_obj_set_style_bg_color(s_w.status_dot, C_GREEN, 0);
    lv_obj_set_style_bg_opa(s_w.status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_w.status_dot, 0, 0);
    lv_obj_set_style_radius(s_w.status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(s_w.status_dot, 0, 0);
    lv_obj_set_scrollbar_mode(s_w.status_dot, LV_SCROLLBAR_MODE_OFF);

    s_w.status_text = s_label(sr, "ONLINE", F_M, C_GREEN);
}

static void s_build_temp(lv_obj_t *parent, bool is_nozzle)
{
    lv_color_t  color = is_nozzle ? C_CYAN   : C_ORANGE;
    const char *sym   = is_nozzle ? LV_SYMBOL_SETTINGS : LV_SYMBOL_CHARGE;
    const char *title = is_nozzle ? "NOZZLE TEMPERATURE" : "BED TEMPERATURE";
    const char *init_actual = is_nozzle ? "200\xc2\xb0""C" : "60\xc2\xb0""C";
    const char *init_target = is_nozzle ? "| 200\xc2\xb0""C" : "| 60\xc2\xb0""C";

    lv_obj_t *sec = s_section(parent, H_TEMP, true);
    s_icon(sec, color, sym);

    lv_obj_t *col = s_vflex(sec, LV_SIZE_CONTENT, 1);
    lv_obj_set_flex_grow(col, 1);

    s_label(col, title, F_S, C_GRAY);

    // Values: large actual + small target aligned at baseline
    lv_obj_t *vrow = s_hflex(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(vrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(vrow, 4, 0);

    lv_obj_t *actual = s_label(vrow, init_actual, F_L, color);
    lv_obj_t *target = s_label(vrow, init_target, F_M, C_GRAY);

    if(is_nozzle) { s_w.nozzle_actual = actual; s_w.nozzle_target = target; }
    else          { s_w.bed_actual    = actual; s_w.bed_target    = target; }

    // Sub-labels row
    lv_obj_t *slrow = s_hflex(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(slrow, 20, 0);
    s_label(slrow, "ACTUAL", F_S, color);
    s_label(slrow, "TARGET", F_S, C_GRAY);
}

static void s_build_progress(lv_obj_t *parent)
{
    lv_obj_t *sec = s_section(parent, H_PROGRESS, true);

    // Circular arc
    s_w.arc = lv_arc_create(sec);
    lv_obj_set_size(s_w.arc, 52, 52);
    lv_arc_set_rotation(s_w.arc, 270);
    lv_arc_set_bg_angles(s_w.arc, 0, 360);
    lv_arc_set_range(s_w.arc, 0, 100);
    lv_arc_set_value(s_w.arc, 72);
    lv_obj_remove_flag(s_w.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(s_w.arc, C_SEP,   LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_w.arc, 5,        LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_w.arc, C_GREEN,  LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_w.arc, 5,        LV_PART_INDICATOR);
    lv_obj_set_style_opa(s_w.arc, LV_OPA_TRANSP,  LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_w.arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_w.arc, 0, 0);
    lv_obj_set_style_pad_all(s_w.arc, 4, 0);

    s_w.arc_lbl = lv_label_create(s_w.arc);
    lv_label_set_text(s_w.arc_lbl, "72%");
    lv_obj_set_style_text_font(s_w.arc_lbl, F_M, 0);
    lv_obj_set_style_text_color(s_w.arc_lbl, C_GREEN, 0);
    lv_obj_center(s_w.arc_lbl);

    // Right content column
    lv_obj_t *col = s_vflex(sec, LV_SIZE_CONTENT, 4);
    lv_obj_set_flex_grow(col, 1);

    s_label(col, "PRINT PROGRESS", F_S, C_GRAY);

    s_w.progress_bar = lv_bar_create(col);
    lv_obj_set_size(s_w.progress_bar, LV_PCT(100), 7);
    lv_bar_set_range(s_w.progress_bar, 0, 100);
    lv_bar_set_value(s_w.progress_bar, 72, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_w.progress_bar, C_SEP,   LV_PART_MAIN);
    lv_obj_set_style_bg_opa (s_w.progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_w.progress_bar, C_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa (s_w.progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius  (s_w.progress_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius  (s_w.progress_bar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(s_w.progress_bar, 0, LV_PART_MAIN);

    s_w.progress_lbl = s_label(col, "72% COMPLETE", F_S, C_GREEN);
}

static void s_build_layer(lv_obj_t *parent)
{
    lv_obj_t *sec = s_section(parent, H_LAYER, true);
    s_icon(sec, C_PURPLE, LV_SYMBOL_LIST);

    lv_obj_t *col = s_vflex(sec, LV_SIZE_CONTENT, 1);
    lv_obj_set_flex_grow(col, 1);

    s_label(col, "LAYER", F_S, C_GRAY);

    lv_obj_t *vrow = s_hflex(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(vrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(vrow, 6, 0);

    s_w.layer_cur = s_label(vrow, "145", F_L, C_WHITE);
    s_w.layer_tot = s_label(vrow, "250", F_M, C_GRAY);

    lv_obj_t *slrow = s_hflex(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(slrow, 14, 0);
    s_label(slrow, "CURRENT", F_S, C_PURPLE);
    s_label(slrow, "TOTAL",   F_S, C_GRAY);
}

static void s_build_time(lv_obj_t *parent)
{
    lv_obj_t *sec = s_section(parent, H_TIME, true);
    s_icon(sec, C_YELLOW, LV_SYMBOL_REFRESH);

    lv_obj_t *col = s_vflex(sec, LV_SIZE_CONTENT, 1);
    lv_obj_set_flex_grow(col, 1);

    s_label(col, "TIME REMAINING", F_S, C_GRAY);
    s_w.time_val = s_label(col, "02:35:47", F_L, C_WHITE);
    s_label(col, "hh : mm : ss", F_S, C_GRAY);
}

static void s_build_footer(lv_obj_t *parent)
{
    lv_obj_t *sec = lv_obj_create(parent);
    lv_obj_set_size(sec, LV_PCT(100), H_FOOTER);
    lv_obj_set_style_bg_color(sec, C_BG, 0);
    lv_obj_set_style_bg_opa(sec, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sec, 0, 0);
    lv_obj_set_style_radius(sec, 0, 0);
    lv_obj_set_style_pad_all(sec, 2, 0);
    lv_obj_set_scrollbar_mode(sec, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_layout(sec, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sec, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(sec, 4, 0);

    s_label(sec, LV_SYMBOL_DRIVE, F_M, C_CYAN);
    s_w.footer_lbl = s_label(sec, "PRINTING...", F_M, C_CYAN);
}

// ── Public API ────────────────────────────────────────────────────────────────

void ui_printer_status_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, C_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_pad_row(parent, 0, 0);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_build_header(parent);
    s_build_temp(parent, false);    // bed
    s_build_temp(parent, true);     // nozzle
    s_build_progress(parent);
    s_build_layer(parent);
    s_build_time(parent);
    s_build_footer(parent);
}

void ui_printer_status_update(const ui_printer_data_t *data)
{
    char buf[24];

    // Online status
    lv_color_t sc = data->online ? C_GREEN : C_RED;
    lv_obj_set_style_bg_color(s_w.status_dot,  sc, 0);
    lv_obj_set_style_text_color(s_w.status_text, sc, 0);
    lv_label_set_text(s_w.status_text, data->online ? "ONLINE" : "OFFLINE");

    // Bed temperature
    snprintf(buf, sizeof(buf), "%d\xc2\xb0""C", data->bed_actual_c);
    lv_label_set_text(s_w.bed_actual, buf);
    snprintf(buf, sizeof(buf), "| %d\xc2\xb0""C", data->bed_target_c);
    lv_label_set_text(s_w.bed_target, buf);

    // Nozzle temperature
    snprintf(buf, sizeof(buf), "%d\xc2\xb0""C", data->nozzle_actual_c);
    lv_label_set_text(s_w.nozzle_actual, buf);
    snprintf(buf, sizeof(buf), "| %d\xc2\xb0""C", data->nozzle_target_c);
    lv_label_set_text(s_w.nozzle_target, buf);

    // Progress
    uint8_t pct = data->progress_pct > 100 ? 100 : data->progress_pct;
    lv_arc_set_value(s_w.arc, pct);
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(s_w.arc_lbl, buf);
    lv_bar_set_value(s_w.progress_bar, pct, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d%% COMPLETE", pct);
    lv_label_set_text(s_w.progress_lbl, buf);

    // Layer
    snprintf(buf, sizeof(buf), "%u", data->layer_current);
    lv_label_set_text(s_w.layer_cur, buf);
    snprintf(buf, sizeof(buf), "%u", data->layer_total);
    lv_label_set_text(s_w.layer_tot, buf);

    // Time
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", data->time_h, data->time_m, data->time_s);
    lv_label_set_text(s_w.time_val, buf);
}
