

var fs = require('fs');

var removeskin = require( __dirname + '/../').create(null,0x00ff0000);
//console.log(removeskin.getList());

var bufimg = fs.readFileSync(__dirname + '/image1.jpg');



removeskin.removeSkin(bufimg,function(err,buffer){
	console.log(err);
	console.log(buffer);
	
	fs.writeFileSync(__dirname + '/imaget1.png',buffer);
});


setTimeout(function(){},1000);