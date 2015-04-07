var WebSocketServer = require('websocket').server;
var http = require('http');

var request = require('request');
var url = require('url');
var express = require('express');
var bodyParser = require('body-parser');
var multer = require('multer');
var app=express();
var fs = require('fs');

var spawn = require('child_process').spawn;

var userSave= {};
var connectionId=0;
var connections=[];
var onlineUsers = {};
var usePort = 9935;

var debug = true;

var server = http.createServer(app);

app.set('jsonp callback name');
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: false }));
app.use(multer({ dest: './',
			rename: function (fieldname, filename) {
				//return filename+Date.now();
				return filename;
			 },
			onFileUploadStart: function (file) {
					console.log(file.originalname + ' is starting ...')
			},
			onFileUploadComplete: function (file, req, res) {
				console.log(file.fieldname + ' uploaded to  ' + file.path)
				req.done=true;
			},
			onFileSizeLimit: function (file) {
				console.log('[onFileSizeLimit] Failed: ', file.originalname)
				fs.unlink('./' + file.path) // delete the partially written file
				return false;
			},
			limits:{
					//fileSize: 191010
					//fileSize: 10485760
					fileSize: 3000000
			}
}));
app.use(express.static(__dirname + '/public'));
/*app.get('/',function(req,res){
				var options = {
						root: __dirname ,
						dotfiles: 'deny',
						headers: {
								'x-timestamp': Date.now(),
								'x-sent': true
						}
				};

		      res.sendFile("index.html",options);
});*/
app.post('/odb/:db/put',function(req,res){
		var wrong = function(errMessage){
				var resStr = errMessage || '';
				res.write(resStr);
				res.end();
		}
		console.log("in odb/put");
		var str2='';
		url_parts = url.parse(req.url, true);
		var query = url_parts.query;
		var para = [];
		if(req.files && req.files.obj && req.files.obj.truncated){
				res.write("over file size limit!");
				res.end();
		}
		if(req.files && req.files.obj ){
				str2 = JSON.stringify(url_parts) ;
				if(req.body.action === 'put'){
						para = ["-p" , req.params.db , "-F" , req.files.obj.path ];

						var odb = spawn('./odb' , para);
						var str = '';
						var erstr='';
						odb.stdout.pipe(process.stdout);
						odb.stderr.pipe(process.stdout);
						odb.stdout.on('data', function (data) {
										str += data;
										});

						odb.stderr.on('data', function (data) {
										erstr += data;
										});
						odb.on('exit', function (code) {
										res.write(str + '\nurl parameter :'  + str2 + "err:[" + erstr + "]");
										res.end("File uploaded."+req.files.obj.path);
										console.log('['+req.files.obj.path+']');
										fs.unlink('./' + req.files.obj.path) // delete the partially written file
										});
						return;
				}
				else{
						res.end("File upload FAIL.");
						return;
				}
		}
		res.end("File upload FAIL.");
});
app.post('/odb/:db/list',function(req,res){
		var wrong = function(errMessage){
				var resStr = errMessage || '';
				res.write(resStr);
				res.end();
		}
		var para = [];
		var resSet = false;
		if( req.params && req.params.db  ){
			para = ["-p",req.params.db,"--list"];
			var odb = spawn('./odb' , para);
			var str = '';
			var erstr='';
			odb.stdout.on('data', function (data) {
							if(!resSet){
									res.set({'Content-disposition':'attachment'});
									resSet = true;
							}
							res.write(data);
							str += data;
							});

			odb.stderr.on('data', function (data) {
							res.write(data);
							});
			odb.on('close', function (code) {
							console.log(str.length);
							res.end();

							});
			return;
		}
		res.end("File list FAIL.");
});
app.get('/odb/:db/get/:filename',function(req,res){
		var wrong = function(errMessage){
				var resStr = errMessage || '';
				res.write(resStr);
				res.end();
		}
		var str2='';
		url_parts = url.parse(req.url, true);
		var query = url_parts.query;
		var para = [];
		var resSet = false;
		//var_print(req);
/*		if( req.body && (req.body.action === 'md5') ){
			if(req.body.value){
				para = ["-p",req.params.db,"-M",req.body.value];
				console.log(JSON.stringify(para));
			}else{
				wrong("error argument<br\\>\r\n");
				return;
			}
			var odb = spawn('./odb' , para);
			var str = '';
			var erstr='';
			//res.set({'Content-Type: ':'application/octet-stream'});
			odb.stdout.on('data', function (data) {
							if(!resSet){
									res.set({'Content-disposition':'attachment'});
									resSet = true;
							}
							res.write(data);
							str += data;
							});

			odb.stderr.on('data', function (data) {
							res.write(data);
							});
			odb.on('close', function (code) {
					//console.log('md5get: '+req.body.md5);
					console.log(str.length);
							//res.write();
							res.end();

							});
			return;
		}else */
		if( req.params  ){
			if(req.params.filename){
				para = ["-p",req.params.db,"-G",req.params.filename];
				console.log(JSON.stringify(para));
			}else{
				wrong("error argument<br\\>\r\n");
				return;
			}
			var odb = spawn('./odb' , para);
			var str = '';
			var erstr='';
			//res.set({'Content-Type: ':'application/octet-stream'});
			odb.stdout.on('data', function (data) {
							if(!resSet){
									res.set({'Content-disposition':'attachment'});
									resSet = true;
							}
							res.write(data);
							str += data;
							});

			odb.stderr.on('data', function (data) {
					//TODO set header 404 or other
							res.write(data);
							});
			odb.on('close', function (code) {
					console.log(str.length);
							//res.write();
							res.end();

							});
			return;
		}


				//res.end("File upload FAIL.");
});

app.get('/odb/:db/delete/:name',function(req,res){
		//TODO delete
		console.log(req.params.name);
		res.write(JSON.stringify(req.params));
		res.end();
		return;
		var wrong = function(errMessage){
				var resStr = errMessage || '';
				res.write(resStr);
				res.end();
		}
		var str2='';
		url_parts = url.parse(req.url, true);
		var query = url_parts.query;
		var para = [];
		var resSet = false;
		//var_print(req);
		if( req.body && (req.body.action === 'md5') ){
			if(req.body.value){
				para = ["-p",req.params.db,"-M",req.body.value];
				console.log(JSON.stringify(para));
			}else{
				wrong("error argument<br\\>\r\n");
				return;
			}
			var odb = spawn('./odb' , para);
			var str = '';
			var erstr='';
			//res.set({'Content-Type: ':'application/octet-stream'});
			odb.stdout.on('data', function (data) {
							if(!resSet){
									res.set({'Content-disposition':'attachment'});
									resSet = true;
							}
							res.write(data);
							str += data;
							});

			odb.stderr.on('data', function (data) {
							res.write(data);
							});
			odb.on('close', function (code) {
					//console.log('md5get: '+req.body.md5);
					console.log(str.length);
							//res.write();
							res.end();

							});
			return;
		}else if( req.body && (req.body.action === 'filename') ){
			if(req.body.value){
				para = ["-p",req.params.db,"-G","uploads/"+req.body.value];
				console.log(JSON.stringify(para));
			}else{
				wrong("error argument<br\\>\r\n");
				return;
			}
			var odb = spawn('./odb' , para);
			var str = '';
			var erstr='';
			//res.set({'Content-Type: ':'application/octet-stream'});
			odb.stdout.on('data', function (data) {
							if(!resSet){
									res.set({'Content-disposition':'attachment'});
									resSet = true;
							}
							res.write(data);
							str += data;
							});

			odb.stderr.on('data', function (data) {
							res.write(data);
							});
			odb.on('close', function (code) {
					//console.log('md5get: '+req.body.md5);
					console.log(str.length);
							//res.write();
							res.end();

							});
			return;
		}


				//res.end("File upload FAIL.");
});
app.post('/odb/*',function(request, response){
		//testing//
		console.log("testing area");
				var body = '';

				request.on('data', function (data) {
						body += data;

						if (body.length > 1e6){//prevent too big
								request.connection.destroy();
						}
						});
				request.on('end', function () {
						var str2='';
						var para = [];
						url_parts = url.parse(request.url, true);
						var query = url_parts.query;
						str2 = JSON.stringify(url_parts) ;
						var wrong = function(errMessage){
								var resStr = errMessage || '';
								response.write(resStr);
								response.end();
						}
						/*for(var key in query){
						para.push(key);
						}*/
						if(!query.db){
								wrong("no db path");
								return;
						}

						if(query.action === 'get'){
								if(query.md5){
										para = ["-p",query.db,"-M",query.md5];
								}else{
										wrong("error argument<br\\>\r\n");
										return;

								}
						}else if(true){
						}
						var cat = spawn('cat' , ['Makefile']);
						var odb = spawn('./a.out' , para);


						cat.stdout.on('data' , function (data){
								odb.stdin.write(data);
								odb.stdin.write(body);
								odb.stdin.end();  
						});
						odb.stdout.pipe(process.stdout);
						str = '';
						erstr=''
						odb.stdout.on('data', function (data) {
								str += data;
						});

						odb.stderr.on('data', function (data) {
								erstr += data;
						});

						odb.on('close', function (code) {
								  response.write(str + '\nurl parameter :'  + str2 );
								  response.end();
						});
				});
});
app.get('/users/getAll/',function(request, response){
				var body = '';
				request.on('data', function (data) {
						body += data;

						if (body.length > 1e6){//prevent too big
								request.connection.destroy();
						}
						});
				request.on('end', function () {
						var noticeInfo;
						/*TODO check send from our service*/
						response.writeHead(200);
						var tmpJson = {};
						tmpJson.users = [];
						var str = "";
						for(var i in onlineUsers){
								if(onlineUsers[i]){
										tmpJson.users.push(onlineUsers[ i ].uid);
								}
						}
						//onlineUsers[noticeInfo.uid] = ;//jsonData;
						if(debug)console.log((new Date()) + 'request for get all users id' + request.url);
						response.write( JSON.stringify( tmpJson ) );
						response.end();
				});
});
server.listen(usePort, function() {
		    console.log((new Date()) + 'run on port:' + usePort);
});


function var_print(o){
		var cache = [];
		console.log(JSON.stringify(o, function(key, value) {
						if (typeof value === 'object' && value !== null) {
						if (cache.indexOf(value) !== -1) {
						// Circular reference found, discard key
						return;
						}
						// Store value in our collection
						cache.push(value);
						}
						return value;
						}) );
		cache = null;
};
