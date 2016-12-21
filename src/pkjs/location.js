module.exports = function(minified) {
  var clayConfig = this;

function locationSuccess(pos) {
  var pos_data = 'lat = ' + pos.coords.latitude + ' lon = ' + pos.coords.longitude;
  console.log(pos_data);
}

function locationError(err) {
  console.log('Error requesting location!');
}

// Converts the lon/lat coordinates into integers for consumption by the app
function cleanCoordinate(coord) {
  return (coord * 100)|0;
}   
  
function getLocation() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 15000, maximumAge: 60000}
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
  
clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
  clayConfig.getItemByMessageKey('LocationText').set('gotit');
  };
             )
};