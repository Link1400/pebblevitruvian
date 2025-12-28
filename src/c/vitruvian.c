#include <pebble.h>

static Window *s_main_window;
static Layer *s_canvas_layer;

static TextLayer *s_hour_layer;
static TextLayer *s_min_layer;
static TextLayer *s_colon_layer;
static TextLayer *s_date_layer;
static GBitmap *s_background_bitmap;
static BitmapLayer *s_background_layer;

static int curve_offset = 0;        
static int curve_progress = 0;      
static bool is_animating = false;   

// Color definitions
#define ORO_BG     GColorBlack
#define ORO_WHITE  GColorWhite
#define ORO_GOLD   PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite)
#define ORO_GRAY   PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite)
#define VOID_BLUE  PBL_IF_COLOR_ELSE(GColorElectricBlue, GColorWhite)
#define CURVE_POINTS 40 

// Forward declaration
static void draw_energy_curves(GContext *ctx, GPoint center, int height, int width) {
    graphics_context_set_stroke_color(ctx, ORO_GOLD);
    graphics_context_set_stroke_width(ctx, 3);

    for (int i = 0; i < CURVE_POINTS - 1; i++) {
        // Shared math for Y and Progress
        int y0 = (i * height) / (CURVE_POINTS - 1);
        int y1 = ((i + 1) * height) / (CURVE_POINTS - 1);
        float p0 = ((float)i / (CURVE_POINTS - 1)) * 2.0f - 1.0f;
        float p1 = ((float)(i + 1) / (CURVE_POINTS - 1)) * 2.0f - 1.0f;

        // Curve offsets
        int x_off0 = (int)(30.0f * (p0 * p0 * p0));
        int x_off1 = (int)(30.0f * (p1 * p1 * p1));

        // SET 1: Downward curves (Top to Bottom)
        graphics_draw_line(ctx, GPoint(center.x - 55 - x_off0, y0), GPoint(center.x - 55 - x_off1, y1));
        graphics_draw_line(ctx, GPoint(center.x + 55 + x_off0, y0), GPoint(center.x + 55 + x_off1, y1));

        // SET 2: Upward curves (Bottom to Top)
        // We flip the x_off to create a criss-cross "DNA" or "X" effect
        graphics_draw_line(ctx, GPoint(center.x - 55 + x_off0, y0), GPoint(center.x - 55 + x_off1, y1));
        graphics_draw_line(ctx, GPoint(center.x + 55 - x_off0, y0), GPoint(center.x + 55 - x_off1, y1));
    }
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  // 1. Background
  graphics_context_set_fill_color(ctx, ORO_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // 2. Draw Vertical Curves
  draw_energy_curves(ctx, center, bounds.size.h, bounds.size.w);

  // 3. Masking Circle (The "Eraser")
  graphics_context_set_fill_color(ctx, ORO_BG);
  graphics_fill_circle(ctx, center, 68);

  // 4. Dial Math
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int hour = t->tm_hour % 12;
  int minutes = t->tm_min;
  // ONLY ONE DEFINITION OF SWEEP:
  int sweep = minutes*6;

  // 5. Outer Vitruvian Ring
  graphics_context_set_stroke_color(ctx, ORO_WHITE);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_arc(ctx, GRect(center.x - 66, center.y - 66, 132, 132), GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(30), DEG_TO_TRIGANGLE(150));
  graphics_draw_arc(ctx, GRect(center.x - 66, center.y - 66, 132, 132), GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(210), DEG_TO_TRIGANGLE(330));

  // Gold endcaps
  graphics_context_set_stroke_color(ctx, ORO_GOLD);
  graphics_draw_arc(ctx, GRect(center.x - 66, center.y - 66, 132, 132), GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(28), DEG_TO_TRIGANGLE(32));
  graphics_draw_arc(ctx, GRect(center.x - 66, center.y - 66, 132, 132), GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(148), DEG_TO_TRIGANGLE(152));

  // 6. Cardinal Gold Anchors
  graphics_context_set_stroke_width(ctx, 2);
  for(int i = 0; i < 4; i++) {
    int32_t angle = i * TRIG_MAX_ANGLE / 4;
    GPoint a = {
      .x = center.x + (sin_lookup(angle) * 62 / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * 62 / TRIG_MAX_RATIO)
    };
    GPoint b = {
      .x = center.x + (sin_lookup(angle) * 72 / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * 72 / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, a, b);
  }

  // 7. Hour Gold Arc (Smooth movement)
  graphics_context_set_stroke_color(ctx, VOID_BLUE);
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_arc(ctx, 
                  GRect(center.x - 56, center.y - 56, 112, 112), 
                  GOvalScaleModeFitCircle, 
                  0, 
                  DEG_TO_TRIGANGLE(sweep));
  // 8. Inner Glyph Ring
  graphics_context_set_stroke_color(ctx, ORO_GRAY);
  for (int i = 0; i < 60; i++) {
    int32_t angle = i * TRIG_MAX_ANGLE / 60;
    int r_out = 66;
    int r_in = (i % 5 == 0) ? 58 : 62;
    graphics_context_set_stroke_width(ctx, (i % 5 == 0) ? 2 : 1);

    GPoint p1 = { .x = center.x + (sin_lookup(angle) * r_in / TRIG_MAX_RATIO), .y = center.y - (cos_lookup(angle) * r_in / TRIG_MAX_RATIO) };
    GPoint p2 = { .x = center.x + (sin_lookup(angle) * r_out / TRIG_MAX_RATIO), .y = center.y - (cos_lookup(angle) * r_out / TRIG_MAX_RATIO) };
    graphics_draw_line(ctx, p1, p2);
  }
}
static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char hour_buf[3];
  static char min_buf[3];
  static char date_buf[12];

  if (clock_is_24h_style()) {
  strftime(hour_buf, sizeof(hour_buf), "%H", tick_time);
} else {
  strftime(hour_buf, sizeof(hour_buf), "%I", tick_time);
}

  strftime(min_buf, sizeof(min_buf), "%M", tick_time);
  strftime(date_buf, sizeof(date_buf), "%a %d", tick_time);

  text_layer_set_text(s_hour_layer, hour_buf);
  text_layer_set_text(s_min_layer, min_buf);
  text_layer_set_text(s_date_layer, date_buf);

  layer_mark_dirty(s_canvas_layer);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  is_animating = true;
  curve_progress = 0;
  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  curve_offset = 0; 
  update_time();
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update);
  layer_add_child(window_layer, s_canvas_layer);

  s_hour_layer = text_layer_create(GRect(28, 62, 40, 40));
  s_min_layer  = text_layer_create(GRect(76, 62, 40, 40));
  s_colon_layer = text_layer_create(GRect(68, 62, 10, 40));
  s_date_layer = text_layer_create(GRect(0, 95, 144, 30));

  text_layer_set_background_color(s_hour_layer, GColorClear);
  text_layer_set_background_color(s_min_layer, GColorClear);
  text_layer_set_background_color(s_colon_layer, GColorClear);
  text_layer_set_background_color(s_date_layer, GColorClear);

  text_layer_set_text_color(s_hour_layer, ORO_WHITE);
  text_layer_set_text_color(s_min_layer, ORO_WHITE);
  text_layer_set_text_color(s_colon_layer, ORO_GOLD);
  text_layer_set_text_color(s_date_layer, ORO_GRAY);

  text_layer_set_font(s_hour_layer, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
  text_layer_set_font(s_min_layer, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
  text_layer_set_font(s_colon_layer, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));

  text_layer_set_text_alignment(s_colon_layer, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_text(s_colon_layer, ":");

  layer_add_child(window_layer, text_layer_get_layer(s_hour_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_colon_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_min_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  is_animating = true;      // Enable the logic in draw_energy_curves
  curve_progress = 0;       // Start from the beginning
  layer_mark_dirty(s_canvas_layer); // Trigger the first draw call
  update_time();

  
}

static void main_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  text_layer_destroy(s_hour_layer);
  text_layer_destroy(s_min_layer);
  text_layer_destroy(s_colon_layer);
  text_layer_destroy(s_date_layer);
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, ORO_BG);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  accel_tap_service_subscribe(tap_handler);
}

static void deinit() {
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}