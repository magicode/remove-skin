

var addon = require('./build/Release/removeskin');


//module.exports.RemoveSkin = addon.RemoveSkin;



var fs = require('fs');


module.exports.create = function(bufimg){
	if(!bufimg)
		bufimg = fs.readFileSync(__dirname + '/skins.png');
	return new addon.RemoveSkin(bufimg);
};


