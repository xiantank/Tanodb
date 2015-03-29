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
app.use(multer({ dest: './uploads/',
			rename: function (fieldname, filename) {
				return filename+Date.now();
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
					fileSize: 191010
					//fileSize:191050 
			}
}));
app.get('/',function(req,res){
		      res.sendfile("index.html");
});

app.post('/api/photo',function(req,res){
		if(req.files && !req.files.obj.truncated){
				console.log(req.files);
				console.log(req.body);
				res.end("File uploaded."+req.files.obj.path);
		}
		else{
				res.end("File upload FAIL.");
		}
});
app.post('/odb/*',function(request, response){
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
						console.log(JSON.stringify(url_parts) );
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
						var proc = spawn('./a.out' , para);

						proc.stdout.pipe(process.stdout);
						proc.stdin.write(body);
						proc.stdin.end();  
						str = '';
						erstr=''
						proc.stdout.on('data', function (data) {
								str += data;
						});

						proc.stderr.on('data', function (data) {
								erstr += data;
						});

						proc.on('close', function (code) {
								  console.log('child process exited with code ' + code);
								  console.log(str);

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


