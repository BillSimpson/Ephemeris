// Import the Clay package
var Clay = require('pebble-clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// add location javascript function
//var customClay = require('./location');
//var clay = new Clay(clayConfig, customClay);


// Converts the lon/lat coordinates into integers for consumption by the app
function cleanCoordinate(coord) {
  return (coord * 100)|0;
}

function locationSuccess(pos) {
  var pos_data = 'Phone location: Latitude = ' + Number(pos.coords.latitude).toFixed(0) + ', Longitude = ' + Number(pos.coords.longitude).toFixed(0)
  console.log(pos_data);
  clayConfig[2].items[1].defaultValue = pos_data; // set up with the required value.
  var clay = new Clay(clayConfig);
}

function locationError(err) {
  console.log('Error requesting location!');
  clayConfig[2].items[1].defaultValue = 'Failed to get location from phone'; // set up with the required value.
  var clay = new Clay(clayConfig);
}

function getLocation() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 5000, maximumAge: 60000}
  );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log('PebbleKit JS ready!');

    // Get the initial weather
    getLocation();
  }
);
