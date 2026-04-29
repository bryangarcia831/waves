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
  { "type": "submit", "defaultValue": "Save" }
];
