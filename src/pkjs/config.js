module.exports = [
  { "type": "heading", "defaultValue": "Waves" },
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
      { "label": "Honolulu, HI", "value": 4 },
      { "label": "Rio de Janeiro", "value": 5 },
      { "label": "London, UK", "value": 6 },
      { "label": "Singapore", "value": 7 },
      { "label": "Jeju, South Korea", "value": 8 },
      { "label": "Sydney, AU", "value": 9 }
    ]
  },
  { "type": "submit", "defaultValue": "Save" }
];
