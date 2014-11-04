

var fs = require('fs');

var removeskin = require( __dirname + '/../').create();
//console.log(removeskin.getList());

var bufimg = fs.readFileSync(__dirname + '/image1.jpg');



removeskin.removeSkin(bufimg,function(err,buffer){
	console.log(err);
	console.log(buffer);
	
	fs.writeFileSync(__dirname + '/imaget1.jpg',buffer);
});


setTimeout(function(){},1000);