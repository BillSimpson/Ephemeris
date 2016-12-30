#include <pebble.h>
//
// Watchface "ephemeris"
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
static int16_t solar_elev_x100[25];
static int16_t lunar_elev_x100[25];
static int lunar_offset_hour;
static float lunar_fine_shift;
static int lunar_day;
static int lunar_side = 0;
static int info_offset = 0;
static uint16_t lunar_image_id = 0xffff;
static uint16_t solar_image_id = 0xffff;
static time_t last_update_unixtime;
static float last_update_latitude;
static float last_update_longitude;
static int last_lunar_side = 0;

// for debouncing
static bool debounce;
static AppTimer *debounce_timer;

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

#define NO_AZI 0
#define CALC_AZI 1

#define NUM_INFO_ITEMS 4

// Define our settings struct
typedef struct ClaySettings {
  float Latitude;
  float Longitude;
  bool ShowInfo;
  time_t dayshift_secs;
  int info_display;
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
  if (input>0)
    return (int)(input+0.5);
  else
    return (int)(input-0.5);  
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

void sunPosition(time_t unixdate, int calc_azi, float *azi, float *alt) {
// calculates sun position for a given date and latitude/longitude

  float lw  = DEG2RAD * -1*settings.Longitude;
  float phi = DEG2RAD * settings.Latitude;
  float d = toDays(unixdate);

  float dec, ra;
  sunCoords(d, &dec, &ra);
  float H  = siderealTime(d, lw) - ra;

  *alt = altitude(H, phi, dec) * RAD2DEG;
  if (calc_azi == CALC_AZI)
    *azi = fmod_pebble(((azimuth(H, phi, dec) + PI) * RAD2DEG ),360);
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

void moonPosition(time_t unixdate, int calc_azi, float *azi, float *alt) {
  float lw  = DEG2RAD * -1*settings.Longitude;
  float phi = DEG2RAD * settings.Latitude;
  float d = toDays(unixdate);

  float ra, dec;
  moonCoords(d, &ra, &dec);
  float H = siderealTime(d, lw) - ra;
  float h = altitude(H, phi, dec);
// formula 14.1 of "Astronomical Algorithms" 2nd edition by Jean Meeus (Willmann-Bell, Richmond) 1998.

  *alt = h * RAD2DEG;
  if (calc_azi == CALC_AZI)
    *azi = fmod_pebble(((azimuth(H, phi, dec) + PI) * RAD2DEG ),360);
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
  float elev, azi;
  bool recalculate = false;
  
  // get today's date in local time  
  time_t unixtime = time(NULL);
  unixtime += settings.dayshift_secs;
  struct tm *curr_time = localtime(&unixtime);

  // recalculate if old sky paths are more than an hour old or 0.5 degrees shifted location
  recalculate = ((unixtime - last_update_unixtime) > 3600);
  recalculate = recalculate || (fabs_pebble((settings.Latitude - last_update_latitude))>0.5 );
  recalculate = recalculate || (fabs_pebble((settings.Longitude - last_update_longitude))>0.5 );  
  
  // check lunar side 
  // +1 means lunar display hour > solar display hour; moon ahead of sun
  // -1 means lunar display hour < solar display hour; moon behind (earlier than) sun
  
  float solar_display_hour = (float)curr_time->tm_hour + (float)curr_time->tm_min/60;
  float lunar_display_hour = fmod_pebble(solar_display_hour -
                             24*moonPhase(unixtime)*(SECS_IN_DAY)/((float)MOONPERIOD_SEC),24);
  if (lunar_display_hour > solar_display_hour)
    lunar_side = -1;
  else  
    lunar_side = +1;
  
  // recalculate if lunar side switched
  recalculate = recalculate || (lunar_side != last_lunar_side);
  
  if (!recalculate) {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"avoided recalculation");    
    return;
  }

  // store this as last calculation
  last_update_unixtime = unixtime;
  last_update_latitude = settings.Latitude;
  last_update_longitude = settings.Longitude;  
  last_lunar_side = lunar_side; 
  
  // calculate lunar shift
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Solar %d, Lunar %d hours",(int)solar_display_hour, (int)lunar_display_hour);
  lunar_fine_shift = -24*moonPhase(unixtime)*(SECS_IN_DAY)/((float)MOONPERIOD_SEC);
  if (lunar_fine_shift < -12)
      lunar_fine_shift += 24;  // condition to be in range -12 to +12 hours
  float lunar_day_shift = 0;
  if ((lunar_fine_shift < 0) && (lunar_side == -1)) {// sun wrapped, go back a day for moon
    lunar_day_shift = -1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Sun wrapped, going back a day for moon");
  }  
  if ((lunar_fine_shift > 0) && (lunar_side == +1)) { // sun wrapped, go forward a day for moon
    lunar_day_shift = +1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Moon wrapped, going forward a day for moon");
  } 
  lunar_offset_hour = round_to_int(lunar_fine_shift);
  lunar_fine_shift = lunar_fine_shift - (float)lunar_offset_hour;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Lunar offset hour %d",lunar_offset_hour);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "  Lunar fine shift x100 = %d",(int)(lunar_fine_shift*100));
  
  // set hour, minute, and second to 0, so that we'll calculate hourly starting at midnight
  curr_time->tm_min = 0;
  curr_time->tm_sec = 0;
  curr_time->tm_hour = 0;
  unixtime = mktime(curr_time);

  // cycle through 25 hours for solar and lunar parameters
  i = 0;
//  APP_LOG(APP_LOG_LEVEL_DEBUG,"lat,lon [%d:%d]", (int)settings.Latitude, (int)settings.Longitude);
  do {
    // Solar calculation
    sunPosition(unixtime, NO_AZI, &azi, &elev);
    solar_elev_x100[i] = (int16_t)(100*elev);
//    APP_LOG(APP_LOG_LEVEL_DEBUG, "hour %d Solar: Elev %d", i, (int)elev);
    // Lunar calculation
    moonPosition(unixtime+(time_t)(-3600*lunar_offset_hour + 86400*lunar_day_shift), NO_AZI, NULL, &elev);
    lunar_elev_x100[i] = (int16_t)(100*elev);
//    APP_LOG(APP_LOG_LEVEL_DEBUG, "  Hr %d  Lunar: Elev %d", i, (int)elev);

    unixtime += 3600;     // advance to the next hour and next point
    i++;
  }
  while (i<25);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Re-calculated sky paths");
}

static void load_moon_image() {
  // Get a tm structure
  time_t unixtime = time(NULL);
  unixtime += settings.dayshift_secs;
  uint16_t curr_image_id;

  // get lunar day and load new image
  lunar_day = (int)moonPhase(unixtime);  // truncate (do not round) the result

  if (lunar_day < 3)
    curr_image_id = RESOURCE_ID_IMAGE_MOON1;
  if ((lunar_day < 6) && (lunar_day >= 3)) 
    curr_image_id = RESOURCE_ID_IMAGE_MOON2;
  if ((lunar_day < 10) && (lunar_day >= 6))  // first quarter moon -- 4 days
    curr_image_id = RESOURCE_ID_IMAGE_MOON3;
  if ((lunar_day < 13) && (lunar_day >= 10)) 
    curr_image_id = RESOURCE_ID_IMAGE_MOON4;
  if ((lunar_day < 16) && (lunar_day >= 13)) 
    curr_image_id = RESOURCE_ID_IMAGE_MOON5;
  if ((lunar_day < 19) && (lunar_day >= 16)) 
    curr_image_id = RESOURCE_ID_IMAGE_MOON6;
  if ((lunar_day < 23) && (lunar_day >= 19)) // third quarter moon -- 4 days
    curr_image_id = RESOURCE_ID_IMAGE_MOON7;
  if ((lunar_day < 26) && (lunar_day >= 23)) 
    curr_image_id = RESOURCE_ID_IMAGE_MOON8;
  if (lunar_day >= 26) 
    curr_image_id = RESOURCE_ID_IMAGE_MOON9;
  
  if (curr_image_id != lunar_image_id) {
    if (s_bitmap_moon != NULL) {
      // destroy old moon image  
      gbitmap_destroy(s_bitmap_moon);
//      APP_LOG(APP_LOG_LEVEL_DEBUG, "Deleted old lunar image");
    }
    lunar_image_id = curr_image_id;
    s_bitmap_moon = gbitmap_create_with_resource(lunar_image_id);
//    APP_LOG(APP_LOG_LEVEL_DEBUG, "Selected moon image for lunar day (moon phase 0-29) %d", lunar_day);
  }  
}

static void load_sun_image() {
  uint16_t curr_image_id;

  // select and solar image
  if (settings.curr_solar_elev_int <= 0)
    curr_image_id = RESOURCE_ID_IMAGE_SUN_RIM;
  else 
    curr_image_id = RESOURCE_ID_IMAGE_SUN_RISEN;
  
  if (curr_image_id != solar_image_id) {
    if (s_bitmap_sun != NULL) {
      // destroy old moon image  
      gbitmap_destroy(s_bitmap_sun);
//      APP_LOG(APP_LOG_LEVEL_DEBUG, "Deleted old solar image");
    }
    solar_image_id = curr_image_id;
    s_bitmap_sun = gbitmap_create_with_resource(solar_image_id);
//    APP_LOG(APP_LOG_LEVEL_DEBUG, "Selected new solar image");
  }  
}

static void update_time() {
  // Get a tm structure
  time_t unixtime = time(NULL);
  unixtime += settings.dayshift_secs;
  struct tm *tick_time = localtime(&unixtime);

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
    switch ((settings.info_display + info_offset) % NUM_INFO_ITEMS) {
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
                   (int)moonPhase(unixtime));
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
  return (fmod_pebble(((float)hour + frac_hour + offset),24));
}

int angle_to_ypixel (float angle) {
  // set y scale based upon latitude
  int range = (90 - fabs_pebble(settings.Latitude) + 23.5) * 1.35;  // full graph 135% of the potential range at that lat
  if (range>110) range = 110;
  int top = (90 - fabs_pebble(settings.Latitude) + 23.5) * 1.05;  // this gives a 30% buffer below the horizon.
  if (top>90) top = 90;
  return (int)(((top-angle)*graph_height)/range);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Custom drawing happens here!
  int i;
  GPoint point1, point2;
  float curr_elev, curr_azi, next_elev, curr_azi_hour, next_azi_hour;
  
  // Get the time and a tm structure
  time_t curr_unixtime = time(NULL);
  curr_unixtime += settings.dayshift_secs;
  struct tm *curr_time = localtime(&curr_unixtime);

  // Set the line color
  graphics_context_set_stroke_color(ctx, GColorWhite);
  // Set the stroke width (must be an odd integer value)
  graphics_context_set_stroke_width(ctx, 1);
  // Set the fill color
  graphics_context_set_fill_color(ctx, GColorWhite);
  // Set the compositing mode (GCompOpSet is required for transparency)
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  
  // Draw solar path
  for (i=0;i<24;i++) {
    point1 = GPoint(hour_to_xpixel(i),angle_to_ypixel((float)(solar_elev_x100[i]/100)));
    point2 = GPoint(hour_to_xpixel(i+1),angle_to_ypixel((float)(solar_elev_x100[i+1]/100)));
    if ((solar_elev_x100[i]>0)||(solar_elev_x100[i+1]>0) ) graphics_draw_line(ctx, point1, point2);
  }
  // Draw lunar path (dashed line)
  for (i=0;i<24;i++) {
    curr_azi_hour = interp_hour(i,0,lunar_fine_shift);
    next_azi_hour = interp_hour(i,0.5,lunar_fine_shift);
    if (next_azi_hour > curr_azi_hour) {            // check to prevent "wrap around"
      next_elev = interp_elev((float)(lunar_elev_x100[i]/100),(float)(lunar_elev_x100[i+1]/100),0.5);
      point1 = GPoint(hour_to_xpixel(curr_azi_hour),angle_to_ypixel((float)(lunar_elev_x100[i]/100)));
      point2 = GPoint(hour_to_xpixel(next_azi_hour),angle_to_ypixel(next_elev));
      if ((lunar_elev_x100[i]>0)||(lunar_elev_x100[i+1]>0)) graphics_draw_line(ctx, point1, point2);      
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
  sunPosition(curr_unixtime, CALC_AZI, &curr_azi, &curr_elev);
  settings.curr_solar_elev_int = round_to_int(curr_elev);
  settings.curr_solar_azi_int = round_to_int(curr_azi);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sun [%d:%d]", settings.curr_solar_elev_int, settings.curr_solar_azi_int);
  // having calculated the solar elevation, load a sun image (maybe a new one)
  load_sun_image();  
  // calculate the display azimuth hour
  curr_azi_hour = interp_hour(hour,frac_hour,0);
  // If sun is too low, stop lowering its position
  if (curr_elev < -7) curr_elev = -7;
  // Get the location to place the sun
  GRect bitmap_placed = GRect(hour_to_xpixel(curr_azi_hour)-7,angle_to_ypixel(curr_elev)-6,15,13);
  // Draw the image
  graphics_draw_bitmap_in_rect(ctx, s_bitmap_sun, bitmap_placed);

  // Now calculate moon position
  int lunar_hour = (hour + lunar_offset_hour) % 24;
  if (lunar_hour<0) lunar_hour = lunar_hour + 24;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Moon Hour %d", lunar_hour);
  // calculate and store lunar position
  moonPosition(curr_unixtime, CALC_AZI, &curr_azi, &curr_elev);
  settings.curr_lunar_elev_int = round_to_int(curr_elev);
  settings.curr_lunar_azi_int = round_to_int(curr_azi);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Moon [%d:%d]", settings.curr_lunar_elev_int, settings.curr_lunar_azi_int);
  // load a moon image (maybe a new one)
  load_moon_image();
  // calculate the lunar azimith hour for display
  curr_azi_hour = interp_hour(lunar_hour,frac_hour,lunar_fine_shift);
  // If moon is too low, stop lowering its position
  if (curr_elev < -7) curr_elev = -7;
  // Get the location to place the moon
  GRect bitmap_moon_placed = GRect(hour_to_xpixel(curr_azi_hour)-6,angle_to_ypixel(curr_elev)-6,13,13);
  // Draw the image
  graphics_draw_bitmap_in_rect(ctx, s_bitmap_moon, bitmap_moon_placed);
}

// code to get settings from phone via pebble-clay

// Initialize the default settings
static void prv_default_settings() {
  settings.Latitude = 64.8;
  settings.Longitude = -147;
  settings.ShowInfo = true;
  settings.info_display = 0;
}

// Save the settings to persistent storage
static void prv_save_settings() {
  settings.info_display = (settings.info_display + info_offset) % NUM_INFO_ITEMS;
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

  // The "Dayshift" variable is normally not used, but can be used to test 
  // the behvaior at other times.  In normal operations, the multiplier of the
  // slider variable is set to 0 and then the slider is hidden.
  Tuple *dayshift_t = dict_find(iter, MESSAGE_KEY_Dayshift);
  if(dayshift_t) {
    settings.dayshift_secs = (time_t)(0000*(dayshift_t->value->int32)); // multiplier here in seconds
    redo_sky_paths();
  }
  // redraw watchface
  update_time();
  // save settings
  prv_save_settings();
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

void timer_callback(){
  debounce = false;
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // A tap event (shake) occurred
  if (!debounce) {
    info_offset++;
    update_time();
    debounce = true;
    debounce_timer = app_timer_register(500, timer_callback, NULL);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Advance Information due to Accel Tap Event");
  }
  else
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Accel Tap Event during debounce timeout");
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
  window_stack_push(s_main_window, false);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Set the window background to the image background
  window_set_background_color(s_main_window, GColorBlack);
  
  // load horizon bitmap
  s_bitmap_horizon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HORIZON);
  
  // calculate sun paths
  redo_sky_paths();

  // load proper moon phase image
  load_moon_image();

  // get proper sun image
  load_sun_image();

  // Display time
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
