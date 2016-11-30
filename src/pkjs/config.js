module.exports = [
  {
    "type": "heading",
    "defaultValue": "Ephemeris Configuration"
  },
  {
    "type": "text",
    "defaultValue": "Enter location and other selections.  Click SAVE when done."
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Location"
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
        "type": "heading",
        "defaultValue": "More Settings"
      },
      {
        "type": "toggle",
        "messageKey": "ShowInfo",
        "label": "Show extra information",
        "defaultValue": true
      },
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];