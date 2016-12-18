// Import the Clay package
var Clay = require('pebble-clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// add location javascript function
//var customClay = require('./location');
//var clay = new Clay(clayConfig, customClay);

var clay = new Clay(clayConfig);
