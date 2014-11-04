

var fs = require('fs');

var removeskin = require( __dirname + '/../').create();
//console.log(removeskin.getList());

var bufimg = fs.readFileSync(__dirname + '/image1.jpg');


function test(){
	
	var d = Date.now();
	var count = 0;
	function next(){
	
		removeskin.removeSkin(bufimg,function(err,buffer){
			count++;
			if(Date.now() > d + 1000000){
				console.log("count: "+ count);
				return;
			}
			//setImmediate(next);
			next();
		});
	}
	next();
	
}

test();
test();
test();
test();

