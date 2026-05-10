module.exports = [
  { "type": "heading", "defaultValue": "Casio Waves" },
  {
    "type": "toggle",
    "messageKey": "SHOW_SECONDS",
    "label": "Show Seconds",
    "description": "Ticks every second when on.",
    "defaultValue": false
  },
  {
    "type": "toggle",
    "messageKey": "USE_24H",
    "label": "24-Hour Clock",
    "description": "Show time in 24-hour format (13:00 instead of 1:00 PM).",
    "defaultValue": false
  },
  {
    "type": "toggle",
    "messageKey": "DARK_MODE",
    "label": "Dark Mode",
    "description": "Invert LCD colors — black background, white digits.",
    "defaultValue": false
  },
  {
    "type": "select",
    "messageKey": "DATE_FMT",
    "label": "Date Format",
    "description": "Choose the date display format.",
    "defaultValue": 0,
    "options": [
      { "label": "Month-Day (MM-DD)", "value": 0 },
      { "label": "Day-Month (DD-MM)", "value": 1 }
    ]
  },
  {
    "type": "select",
    "messageKey": "TIDE_STATION",
    "label": "Tide Location",
    "description": "Select the tide station for harmonic prediction.",
    "defaultValue": 0,
    "options": [
      { "label": "Monterey, CA", "value": 0 },
      { "label": "San Diego, CA", "value": 1 },
      { "label": "Miami, FL", "value": 2 },
      { "label": "New York, NY", "value": 3 },
      { "label": "Rio de Janeiro", "value": 4 },
      { "label": "London, UK", "value": 5 },
      { "label": "Singapore", "value": 6 },
      { "label": "Jeju, Korea", "value": 7 },
      { "label": "Sydney, AU", "value": 8 },
      { "label": "Honolulu, HI", "value": 9 }
    ]
  },
  { "type": "submit", "defaultValue": "Save" }
];
