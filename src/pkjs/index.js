// Import the Clay package
var Clay = require('pebble-clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// Initialize Clay
var clay = new Clay(clayConfig);

var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

function printreply(url) {
  // Send request to OpenStreetMap
  alert(url);
  xhrRequest(url, 'GET', 
    function(responseText) {
      // responseText contains a JSON object with weather info
      alert(responseText);
      var json = JSON.parse(responseText);
      var gen = json.generator;
      alert(gen);
      var copyr = json.osm3s.copyright;
      alert(copyr);
      var lat = json.elements[0].lat
      var lon = json.elements[0].lon
      alert('city lat,lon is: '+lat+', '+lon);
    }      
  );
}

//var cname = 'Fairbanks';
//var url = 'http://overpass-api.de/api/interpreter?data=%5Bout%3Ajson%5D%5Btimeout%3A25%5D%3Bnode%5B%22name%22~%22'+cname+'%22%5D%5B%22place%22%3D%22city%22%5D%3Bout%20body%3B%0A';
//printreply(url);