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
        "defaultValue": "Location"
      },
      {
        "type": "toggle",
        "messageKey": "UsePhoneLocation",
        "label": "Use phone's location",
        "defaultValue": false
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
 //     {
 //       "type": "heading",
 //       "defaultValue": "More Settings"
 //     },
      {
        "type": "toggle",
        "messageKey": "ShowInfo",
        "label": "Show extra information",
        "defaultValue": true
      },
//      {   // this slider is to tweak the time. Normally not used
//        "type": "slider",
//        "messageKey": "Dayshift",
//        "defaultValue": "0",
//        "label": "Shift time by hours",
//        "min": -24,
//        "max": 24,
//        "step": 1
//      },
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];