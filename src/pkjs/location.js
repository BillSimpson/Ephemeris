// from Katherine Berry's Caltrain App
// https://github.com/Katharine/pebble-caltrain/blob/master/src/location.js


module.exports = function(minified) {
  var clayConfig = this;

  // Converts the lon/lat coordinates into integers for consumption by the app
  function cleanCoordinate(coord) {
    return (coord * 100)|0;
  }
  
  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    
    console.log("Ready.");
    navigator.geolocation.getCurrentPosition(
      .then(function success(pos) {
        var lng = clayConfig.getItemByMessageKey('PhoneLngx100')
        lng = cleanCoordinate(pos.coords.longitude)
        var lat = clayConfig.getItemByMessageKey('PhoneLatx100')
        lat = cleanCoordinate(pos.coords.latitude)
        console.log("Got coordinates (" + pos.coords.longitude + ", " + pos.coords.latitude + ")");
      });
      .error(function error(err) {
        console.log('ERROR(' + err.code + '): ' + err.message);
      });,
      timeout: 10000,
      maximumAge: 600000
    );
});    

};