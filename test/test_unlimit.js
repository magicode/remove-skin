

var fs = require('fs');

var removeskin = require( __dirname + '/../').create();
//console.log(removeskin.getList());

var bufimg = fs.readFileSync(__dirname + '/image1.jpg');

var count = 0;
var d = Date.now();
function test(){
	function next(){
	
		removeskin.removeSkin(bufimg,function(err,buffer){
			count++;
			if(Date.now() > d + 10000){
				d = Date.now();
				console.log("count: "+ count);
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

