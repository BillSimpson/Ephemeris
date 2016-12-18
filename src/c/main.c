#include <pebble.h>
//
// First attempt at the skypath (sun and moon) watchface "ephemeris"
//
static Window *s_main_window;
// set up text layers for time and date and info information
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_info_layer;

// set up canvas layer for drawing
static Layer *s_canvas_layer;
// set up sun bitmaps
static GBitmap *s_bitmap_sun;
static GBitmap *s_bitmap_horizon;
static GBitmap *s_bitmap_moon;

// Global variables
static float graph_width, graph_height;
static float solar_elev[25];
static float solar_azi[25];
static float lunar_elev[25];
static float lunar_azi[25];
static int lunar_offset_hour;
static float lunar_fine_shift;
static int lunar_day;
static int info_offset = 0;

// Persistent storage key
#define SETTINGS_KEY 1

// date/time constants and conversions
#define PI 3.14159268

#define DEG2RAD 0.017453293
#define RAD2DEG 57.29577903
// degrees to radians = pi / 180 and rad to deg = 180 / pi

#define J2000 946684800
// year 2000 in unix time

#define MOONPERIOD_SEC 2551443

#define SECS_IN_DAY 86400
// day in seconds = 24*60*60

// Define our settings struct
typedef struct ClaySettings {
  float Latitude;
  float Longitude;
  bool ShowInfo;
  bool UsePhoneLocation;
  time_t dayshift_secs;
  int curr_solar_elev_int;
  int curr_solar_azi_int;
  int curr_lunar_elev_int;
  int curr_lunar_azi_int;
} ClaySettings;

// An instance of the struct
static ClaySettings settings;

//
// Adapted from the javascript code below to C
//
// https://github.com/mourner/suncalc/blob/master/suncalc.js
//
//
// (c) 2011-2015, Vladimir Agafonkin
// SunCalc is a JavaScript library for calculating sun/moon position and light phases.
// https://github.com/mourner/suncalc
//
// sun calculations are based on http://aa.quae.nl/en/reken/zonpositie.html formulas


float sin_pebble(float angle_radians) {
  int32_t angle_pebble = angle_radians * TRIG_MAX_ANGLE / (2*PI);
  return ((float)(sin_lookup(angle_pebble)) / (float)TRIG_MAX_RATIO);
}

float asin_pebble(float angle_radians) {
  return (angle_radians);  // use small angle formula.
}

float cos_pebble(float angle_radians) {
  int32_t angle_pebble = angle_radians * TRIG_MAX_ANGLE / (2*PI);
  return ((float)cos_lookup(angle_pebble) / (float)TRIG_MAX_RATIO);
}

float atan2_pebble(float y, float x) {
  if (x>2) APP_LOG(APP_LOG_LEVEL_DEBUG, "atan2: X too large, 100 x value = %d", (int)x);
  if (y>2) APP_LOG(APP_LOG_LEVEL_DEBUG, "atan2: Y too large, 100 x value = %d", (int)y);
  int16_t y_pebble = (int16_t) (8192 * y ); 
  int16_t x_pebble = (int16_t) (8192 * x ); 
  return (2*PI * (float)atan2_lookup(y_pebble, x_pebble) / (float)TRIG_MAX_ANGLE);
}

float fmod_pebble(float product, float divisor) {
  int32_t factor;
  float remainder;
  
  factor = (int32_t)(product/divisor);
  if (product<0) factor--;    // casting to int truncates so check if negative and decrement
  remainder = product - ((float)factor)*divisor;
  return remainder;
}

float fabs_pebble(float input) {
  float sign;
  
  if (input<0) sign = -1;
  else sign = 1;
    
  return (sign*input);
}

int round_to_int(float input) {
  return (int)(input+0.5);
}

float toDays(time_t unixdate) {
  return ((float)(unixdate-J2000) / SECS_IN_DAY -0.5);
}

// general calculations for position

float e = DEG2RAD * 23.4397; // obliquity of the Earth

float rightAscension(float l, float b) {
  return atan2_pebble(sin_pebble(l) * cos_pebble(e) - sin_pebble(b)/cos_pebble(b) * sin_pebble(e), cos_pebble(l));
}

float declination(float l, float b) { 
  return (asin_pebble(sin_pebble(b) * cos_pebble(e) + cos_pebble(b) * sin_pebble(e) * sin_pebble(l)));
}

float azimuth(float H, float phi, float dec) {
  return (atan2_pebble(sin_pebble(H), cos_pebble(H) * sin_pebble(phi) - sin_pebble(dec)/cos_pebble(dec) * cos_pebble(phi)));
}

float altitude(float H, float phi, float dec) { 
  return (asin_pebble(sin_pebble(phi) * sin_pebble(dec) + cos_pebble(phi) * cos_pebble(dec) * cos_pebble(H))); 
}

float siderealTime(float d, float lw) { 
  return (DEG2RAD * (280.16 + 360.9856235 * d) - lw);
}

// general sun calculations

float solarMeanAnomaly(float d) { 
  return (DEG2RAD * (357.5291 + 0.98560028 * d)); 
}

float eclipticLongitude(float M) {
  float C = DEG2RAD * (1.9148 * sin_pebble(M) + 0.02 * sin_pebble(2 * M) + 0.0003 * sin_pebble(3 * M)); // equation of center
  float P = DEG2RAD * 102.9372; // perihelion of the Earth
  return (M + C + P + PI);
}

void sunCoords(float d, float *dec, float *ra) {

  float M = solarMeanAnomaly(d);
  float L = eclipticLongitude(M);

  *dec = declination(L, 0);
  *ra = rightAscension(L, 0);
}

void sunPosition(time_t unixdate, float lat, float lng, float *azi, float *alt) {
// calculates sun position for a given date and latitude/longitude

  float lw  = DEG2RAD * -lng;
  float phi = DEG2RAD * lat;
  float d = toDays(unixdate);

  float dec, ra;
  sunCoords(d, &dec, &ra);
  float H  = siderealTime(d, lw) - ra;

  *azi = fmod_pebble(((azimuth(H, phi, dec) + PI) * RAD2DEG ),360);
  *alt = altitude(H, phi, dec) * RAD2DEG;
};

// moon calculations, based on http://aa.quae.nl/en/reken/hemelpositie.html formulas

void moonCoords(float d, float *ra, float *dec) { 
// geocentric ecliptic coordinates of the moon

  float L = DEG2RAD * (218.316 + 13.176396 * d); // ecliptic longitude
  float M = DEG2RAD * (134.963 + 13.064993 * d); // mean anomaly
  float F = DEG2RAD * (93.272 + 13.229350 * d);  // mean distance

  float l  = L + DEG2RAD * 6.289 * sin_pebble(M); // longitude
  float b  = DEG2RAD * 5.128 * sin_pebble(F);     // latitude

  *ra = rightAscension(l, b);
  *dec = declination(l, b);
}

void moonPosition(time_t unixdate, float lat, float lng, float *azi, float *alt) {
  float lw  = DEG2RAD * -lng;
  float phi = DEG2RAD * lat;
  float d = toDays(unixdate);

  float ra, dec;
  moonCoords(d, &ra, &dec);
  float H = siderealTime(d, lw) - ra;
  float h = altitude(H, phi, dec);
// formula 14.1 of "Astronomical Algorithms" 2nd edition by Jean Meeus (Willmann-Bell, Richmond) 1998.

  *azi = fmod_pebble(((azimuth(H, phi, dec) + PI) * RAD2DEG ),360);
  *alt = h * RAD2DEG;
  
};

// Greatly simplified moon phase algorithm from
// http://jivebay.com/calculating-the-moon-phase/
// This simply takes a new moon and uses the moon cycle from there.
// The function returns an integer between 0 and 29 that is the days
// into the lunar cycle.  Thus, 0 is new moon, 15 is full moon, and 29 is new
// note that in seconds, the lunar period, which is 29.5305882 earth days is
// lunar period = 2551443 seconds

float moonPhase(time_t unixdate) {
  // 2016-Nov-29 12:19:25 UTC was a new moon
  // 2016-Nov-29 12:19:25 UTC 1480421975 seconds (unix time)
  // so mod the current unix timestamp minus this moon with the lunar period
  // then convert to days by dividing by seconds in a day.
  return ((float)((unixdate-1480421975)%MOONPERIOD_SEC)/(SECS_IN_DAY));

}

void redo_sky_paths() {
  int i;
  
  // get today's date in local time  
  time_t temp = time(NULL);
  temp += settings.dayshift_secs;
  struct tm *curr_time = localtime(&temp);
  // set hour, minute, and second to 0, so that we'll calculate hourly
  curr_time->tm_min = 0;
  curr_time->tm_sec = 0;
  curr_time->tm_hour = 0;
  temp = mktime(curr_time);

  // cycle through 25 hours for solar and lunar parameters
  i = 0;
  APP_LOG(APP_LOG_LEVEL_DEBUG,"lat,lon [%d:%d]", (int)settings.Latitude, (int)settings.Longitude);
  do {
    // Solar calculation
    sunPosition(temp, settings.Latitude, settings.Longitude, &solar_azi[i], &solar_elev[i]);
//    APP_LOG(APP_LOG_LEVEL_DEBUG, "hour %d Solar: Elev %d  Azi %d", i, (int)solar_elev[i], (int)solar_azi[i]);
    // Lunar calculation
    
    if (i==0) {
      lunar_fine_shift = 24*moonPhase(temp)*(SECS_IN_DAY)/((float)MOONPERIOD_SEC);
      lunar_offset_hour = round_to_int(lunar_fine_shift);
      lunar_fine_shift = lunar_fine_shift - (float)lunar_offset_hour;
      // determine the growth (waxing vs waning moon)
      if (lunar_offset_hour > 12) {
        lunar_offset_hour = lunar_offset_hour - 24;  // if waning, offset by -24 hours
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Moon is waning, lunar offset hour %d",lunar_offset_hour);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  Lunar fine shift x100 = %d",(int)(lunar_fine_shift*100));
      }
      else {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Moon is waxing, lunar offset hour %d",lunar_offset_hour);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "  Lunar fine shift x100 = %d",(int)(lunar_fine_shift*100));
      }
    }
    moonPosition(temp+(time_t)(3600*lunar_offset_hour), settings.Latitude, settings.Longitude, &lunar_azi[i], &lunar_elev[i]);
//    APP_LOG(APP_LOG_LEVEL_DEBUG, "  Hr %d  Lunar: Elev %d  Azi %d", i, (int)lunar_elev[i], (int)lunar_azi[i]);

    // advance to the next hour
    temp = temp + 3600;
    i++;
  }
  while (i<25);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Re-calculated sky paths");
}

// code to get settings from phone via pebble-clay

// Initialize the default settings
static void prv_default_settings() {
  settings.Latitude = 64.8;
  settings.Longitude = -147;
  settings.ShowInfo = true;
  settings.UsePhoneLocation = false;
}

// Save the settings to persistent storage
static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void prv_load_settings() {
  // Load the default settings
  prv_default_settings();
  // Read settings from persistent storage, if they exist
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  // Read lat / lon and other
  Tuple *latitude_t = dict_find(iter, MESSAGE_KEY_Latitude);
  if(latitude_t) {
    settings.Latitude = (float)(latitude_t->value->int32);
    redo_sky_paths();
  }

  Tuple *longitude_t = dict_find(iter, MESSAGE_KEY_Longitude);
  if(longitude_t) {
    settings.Longitude = (float)(longitude_t->value->int32);
    redo_sky_paths();
  }

  // Read boolean preferences
  Tuple *show_info_t = dict_find(iter, MESSAGE_KEY_ShowInfo);
  if(show_info_t) {
    settings.ShowInfo = show_info_t->value->int32 == 1;
  }

  // Read boolean preferences
  Tuple *use_phone_location_t = dict_find(iter, MESSAGE_KEY_UsePhoneLocation);
  if(use_phone_location_t) {
    settings.UsePhoneLocation = show_info_t->value->int32 == 1;
  }
  
  // The "Dayshift" variable is normally not used, but can be used to test 
  // the behvaior at other times.  In normal operations, the multiplier of the
  // slider variable is set to 0 and then the slider is hidden.
  Tuple *dayshift_t = dict_find(iter, MESSAGE_KEY_Dayshift);
  if(dayshift_t) {
    settings.dayshift_secs = (time_t)(0000*(dayshift_t->value->int32)); // multiplier here in seconds
    redo_sky_paths();
  }
  
  prv_save_settings();
}

static void load_moon_image() {
  // Get a tm structure
  time_t temp = time(NULL);
  temp += settings.dayshift_secs;

  // destroy old moon image
  gbitmap_destroy(s_bitmap_moon);
  // get lunar day and load new image
  lunar_day = (int)moonPhase(temp);
  if (lunar_day < 3)
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON1);
  if ((lunar_day < 6) && (lunar_day >= 3)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON2);
  if ((lunar_day < 10) && (lunar_day >= 6))  // first quarter moon -- 4 days
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON3);
  if ((lunar_day < 13) && (lunar_day >= 10)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON4);
  if ((lunar_day < 16) && (lunar_day >= 13)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON5);
  if ((lunar_day < 19) && (lunar_day >= 16)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON6);
  if ((lunar_day < 23) && (lunar_day >= 19)) // third quarter moon -- 4 days
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON7);
  if ((lunar_day < 26) && (lunar_day >= 23)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON8);
  if (lunar_day >= 26) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON9);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Selected moon image for lunar day (moon phase 0-29) %d", lunar_day);
}

static void load_sun_image() {
  
  // destroy old sun image      
    gbitmap_destroy(s_bitmap_sun);
  // select and load new image
  if (settings.curr_solar_elev_int <= 0) {
    s_bitmap_sun = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUN_RIM);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Selected set sun, elev = %d", settings.curr_solar_elev_int);
  }
  else {
    s_bitmap_sun = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUN_RISEN);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Selected SUN risen, elev = %d", settings.curr_solar_elev_int);
  }
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  temp += settings.dayshift_secs;
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  static char s_date_buffer[12];
  static char s_info_buffer[15];
  
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%k:%M" : "%l:%M", tick_time);
//  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
//                                          "%H:%M" : "%I:%M", tick_time);
  if (s_buffer[0] == ' ') {
    // cut the leading space
    text_layer_set_text(s_time_layer, &(s_buffer[1]));
  }
  else {
    text_layer_set_text(s_time_layer, s_buffer);
  }
  // Display this time on the TextLayer
  
  // Update the date text  
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a, %b %e", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);
  
  // Update the info text -- if we want to show information
  if (settings.ShowInfo) {
    switch ((tick_time->tm_min + info_offset) % 4) {
      case 0:
        snprintf(s_info_buffer, sizeof(s_info_buffer), PBL_IF_ROUND_ELSE("S [%d:%d]","Sun [%d:%d]"), 
                 settings.curr_solar_elev_int, settings.curr_solar_azi_int);
        text_layer_set_text(s_info_layer, s_info_buffer);
        break;
      case 1:
        snprintf(s_info_buffer, sizeof(s_info_buffer), PBL_IF_ROUND_ELSE("M [%d:%d]","Moon [%d:%d]"),
                 settings.curr_lunar_elev_int, settings.curr_lunar_azi_int);
        text_layer_set_text(s_info_layer, s_info_buffer);
        break;
      case 2:
        snprintf(s_info_buffer, sizeof(s_info_buffer), PBL_IF_ROUND_ELSE("Moon %dd","Moon %dd old"),
                 (int)moonPhase(temp));
        text_layer_set_text(s_info_layer, s_info_buffer);
        break;
      case 3:
        snprintf(s_info_buffer, sizeof(s_info_buffer), PBL_IF_ROUND_ELSE("L:%+d,%+d","Loc:%+d,%+d"),
                 round_to_int(settings.Latitude), round_to_int(settings.Longitude));
        text_layer_set_text(s_info_layer, s_info_buffer);
        break;

    }
  }
  else {
    snprintf(s_info_buffer, sizeof(s_info_buffer), " ");
    text_layer_set_text(s_info_layer, s_info_buffer);
  }

  // if it is an 00:01, re-calculate the sun and moon ephemeris
  if ((tick_time->tm_hour == 0)&&(tick_time->tm_min == 1)) {
    // re-calculate skypaths
    redo_sky_paths();
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Midnight (00:01) ephemeris");
  }
  // load a moon image (maybe a new one)
  load_moon_image();
  // load a sun image (maybe a new one)
  load_sun_image();  
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Updated screen");
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

int hour_to_xpixel (float hour) {
  // width = 0 to 24 hours
  return (int)(hour/24 * graph_width);
}

float interp_elev(float curr_elev, float next_elev, float frac_hour) {
  return(curr_elev + (next_elev-curr_elev)*frac_hour);
}

float interp_azi(float curr_azi, float next_azi, float frac_hour) {
  float slope;
  slope = next_azi - curr_azi;
  if (slope<0) slope +=360;  // handle "wrap around situation"
  return(fmod_pebble(curr_azi + slope*frac_hour,360));  
}

float interp_hour (int hour, float frac_hour, float offset) {
  return (fmod_pebble(((float)hour + frac_hour - offset),24));
}

int angle_to_ypixel (float angle) {
  // set y scale based upon latitude
  int range = (90 - fabs_pebble(settings.Latitude) + 23.5) * 1.35;  // full graph 135% of the potential range at that lat
  if (range>110) range = 110;
  int top = (90 - fabs_pebble(settings.Latitude) + 23.5) * 1.05;  // this gives a 30% buffer below the horizon.
  if (top>90) top = 90;
  return (int)((top-angle)/range * graph_height);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Custom drawing happens here!
  int i;
  GPoint point1, point2;
  float curr_elev, curr_azi, next_elev, curr_azi_hour, next_azi_hour, lunar_hour_shift;
  
  // Set the line color
  graphics_context_set_stroke_color(ctx, GColorWhite);
  // Set the stroke width (must be an odd integer value)
  graphics_context_set_stroke_width(ctx, 1);
  // Set the fill color
  graphics_context_set_fill_color(ctx, GColorWhite);
  // Set the compositing mode (GCompOpSet is required for transparency)
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  
  // Get the time and a tm structure
  time_t temp = time(NULL);
  temp += settings.dayshift_secs;
  struct tm *curr_time = localtime(&temp);

  // Draw solar path
  for (i=0;i<24;i++) {
    point1 = GPoint(hour_to_xpixel(i),angle_to_ypixel(solar_elev[i]));
    point2 = GPoint(hour_to_xpixel(i+1),angle_to_ypixel(solar_elev[i+1]));
    if ((solar_elev[i]>0)||(solar_elev[i+1]>0) ) graphics_draw_line(ctx, point1, point2);
  }
  // Draw lunar path
  for (i=0;i<24;i++) {
    lunar_hour_shift = lunar_fine_shift;   // + ((float)i) / 29.5;
    curr_azi_hour = interp_hour(i,0,lunar_hour_shift);
    next_azi_hour = interp_hour(i,0.5,lunar_hour_shift);
    if (next_azi_hour > curr_azi_hour) {            // check to prevent "wrap around"
      next_elev = interp_elev(lunar_elev[i],lunar_elev[i+1],0.5);
      point1 = GPoint(hour_to_xpixel(curr_azi_hour),angle_to_ypixel(lunar_elev[i]));
      point2 = GPoint(hour_to_xpixel(next_azi_hour),angle_to_ypixel(next_elev));
      if ((lunar_elev[i]>0)||(lunar_elev[i+1]>0)) graphics_draw_line(ctx, point1, point2);      
    }
  }
  
  // Generate the horizon 
  GRect horizon_box = GRect(hour_to_xpixel(0),angle_to_ypixel(0),hour_to_xpixel(24),28);
  // Draw the horizon box
  graphics_draw_bitmap_in_rect(ctx, s_bitmap_horizon, horizon_box);
  
  // Calculate sun position
  int hour = curr_time->tm_hour;  
  float frac_hour = ((float)curr_time->tm_min)/60;
  // calculate and store solar position
  sunPosition(temp, settings.Latitude, settings.Longitude, &curr_azi, &curr_elev);
  settings.curr_solar_elev_int = round_to_int(curr_elev);
  settings.curr_solar_azi_int = round_to_int(curr_azi);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sun [%d:%d]", settings.curr_solar_elev_int, settings.curr_solar_azi_int);
  curr_azi_hour = interp_hour(hour,frac_hour,0);
 
  // If sun is too low, stop lowering its position
  if (curr_elev < -7) curr_elev = -7;
  // Get the location to place the sun
  GRect bitmap_placed = GRect(hour_to_xpixel(curr_azi_hour)-7,angle_to_ypixel(curr_elev)-6,15,13);
  // Draw the image
  graphics_draw_bitmap_in_rect(ctx, s_bitmap_sun, bitmap_placed);

  // Now calculate moon position
  int lunar_hour = (hour - lunar_offset_hour) % 24;
  if (lunar_hour<0) lunar_hour = lunar_hour + 24;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Moon Hour %d", lunar_hour);
  // calculate and store lunar position
  moonPosition(temp, settings.Latitude, settings.Longitude, &curr_azi, &curr_elev);
  settings.curr_lunar_elev_int = round_to_int(curr_elev);
  settings.curr_lunar_azi_int = round_to_int(curr_azi);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Moon [%d:%d]", settings.curr_lunar_elev_int, settings.curr_lunar_azi_int);
  lunar_hour_shift = lunar_fine_shift + ((float)lunar_hour) / 29.5;
  curr_azi_hour = interp_hour(lunar_hour,frac_hour,lunar_hour_shift);
  
  // If moon is too low, stop lowering its position
  if (curr_elev < -7) curr_elev = -7;
  // Get the location to place the moon
  GRect bitmap_moon_placed = GRect(hour_to_xpixel(curr_azi_hour)-6,angle_to_ypixel(curr_elev)-6,13,13);
  // Draw the image
  graphics_draw_bitmap_in_rect(ctx, s_bitmap_moon, bitmap_moon_placed);
  
  // Redraw the layer
  layer_mark_dirty(layer);
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // create drawing canvas for data visualization -- top 40% of display
  s_canvas_layer = layer_create(
      GRect(0, 0, bounds.size.w, bounds.size.h*0.4));
  graph_width = bounds.size.w;
  graph_height = bounds.size.h*0.4;
  
  // Assign the custom drawing procedure
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);

  // Add to Window
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
  
  // Create the TextLayer with specific bounds on bottom 60% of display
  s_time_layer = text_layer_create(
      GRect(0, bounds.size.h*0.4, bounds.size.w, 43));
  
  // Set the color scheme
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  // Create date information layer
  s_date_layer = text_layer_create(
    GRect(0, (bounds.size.h*0.4+43), bounds.size.w, 26));

  // Style the text
  text_layer_set_background_color(s_date_layer, GColorBlack);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_date_layer, "Date");

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // Create info layer
  s_info_layer = text_layer_create(
    GRect(0, (bounds.size.h*0.4+70), bounds.size.w, 26));

  // Style the text
  text_layer_set_background_color(s_info_layer, GColorBlack);
  text_layer_set_text_color(s_info_layer, GColorWhite);
  text_layer_set_text_alignment(s_info_layer, GTextAlignmentCenter);
  text_layer_set_font(s_info_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_info_layer, " ");
                                     
  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_info_layer)); 
}

static void main_window_unload(Window *window) {
  // Destroy TextLayers
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_info_layer);

  // Destroy canvas
  layer_destroy(s_canvas_layer);
  
  // Destroy the image data
  gbitmap_destroy(s_bitmap_sun);
  gbitmap_destroy(s_bitmap_horizon);
  gbitmap_destroy(s_bitmap_moon);
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // A tap event (shake) occurred
  info_offset++;
  update_time();
}

static void init() {
  prv_load_settings();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Loaded settings on start");
  
  // Open AppMessage connection
  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(128, 128);
  
  // Subscribe to tap events -- for shake detection
  accel_tap_service_subscribe(accel_tap_handler);

  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Set the window background to the image background
  window_set_background_color(s_main_window, GColorBlack);
  
  // load horizon bitmap
  s_bitmap_horizon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HORIZON);
  
  // calculate sun paths
  redo_sky_paths();

  // load a generic then get proper moon phase image
  s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON5);
  load_moon_image();

  // get basic then proper sun (set/risen) image
  s_bitmap_sun = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUN_RIM);
  load_sun_image();

  // Make sure the time is displayed from the start
  update_time();
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
  prv_save_settings(); // write data if changed
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Stored setting on exit");
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
