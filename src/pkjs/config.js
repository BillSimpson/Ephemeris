module.exports = [
  {
    "type": "heading",
    "defaultValue": "Ephemeris Configuration"
  },
  {
    "type": "text",
    "defaultValue": "Enter location and options.  SAVE when done."
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Select Location"
      },
      {
        "type": "text",
        "defaultValue": "Loading location from phone",
      },
      {
        "type": "slider",
        "messageKey": "Latitude",
        "defaultValue": "65",
        "label": "Latitude, + for North",
        "min": -90,
        "max": 90, 
        "step": 1
      },
      {
        "type": "slider",
        "messageKey": "Longitude",
        "defaultValue": "-147",
        "label": "Longitude, + for East",
        "min": -180,
        "max": 180,
        "step": 1
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "toggle",
        "messageKey": "ShowInfo",
        "label": "Show extra information",
        "defaultValue": true
      },
      {
        "type": "text",
        "defaultValue": "Tap on screen to advance information.  Cycles through Sun, Moon, MoonAge, and Location information"
      },
//      {   // this slider is to tweak the time. Normally not used
//        "type": "slider",
//        "messageKey": "Dayshift",
//        "defaultValue": "0",
//        "label": "Shift time by hours",
//        "min": -300,
//        "max": 300,
//        "step": 3
//      },
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];