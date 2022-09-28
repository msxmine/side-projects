const http = require('http');
const https = require('https');
const urlparse = require('url');
const htmlvar = require('./htmlvars');
const request = require('request');

function headget(url, callback){
	request({	"url": 'http://10.0.8.100:9981' + url,
			"auth": {
				"user": "deadbeef",
				"pass": "deadbeef",
				"sendImmediately": false
				}
		},function(RequestError,RequestResponse,RequestBody){
			callback(RequestBody);
		});
}

var resul1,resul2,resul3,resul4;

function refreshtv(){
	var url1 = '/api/channel/grid?limit=500';
	var url2 = '/api/epg/events/grid?mode=now&limit=500';
	var url3 = '/api/epg/events/grid?limit=400';
	headget(url1, function (dane){
	try {
	JSON.parse(dane);
	resul1 = dane;
	} catch (e){
	//console.log(e);
	}
	});
	headget(url2, function (dane){
	try {
	JSON.parse(dane);
        resul2 = dane;
        } catch (e){
	//console.log(e);
        }
	});
	headget(url3, function (dane){
	try {
	JSON.parse(dane);
        resul3 = dane;
        } catch (e){
	//console.log(e);
        }
	});
}
refreshtv();
var inter1 = setInterval(refreshtv,300000);

function refreshsubs(){
	var url4 = '/api/status/subscriptions';
	headget(url4, function (dane){
	try {
	JSON.parse(dane);
	resul4 = dane
	} catch (e){
	//console.log(e);
	}
	});
}

refreshsubs();
var inter2 = setInterval(refreshsubs,30000);


function normalnastrona(res){
	res.writeHead(200, {'Content-Type': 'text/html'});
	res.write(htmlvar.NaglowekGlowny);
	res.write(htmlvar.NaglowekTB1);
	res.write(htmlvar.kartasub(JSON.parse(resul4)));
        var data = resul1;
        var epg = resul2;
        var epgfull = resul3;

	res.write(htmlvar.NaglowekListyKan);
	obiekt = JSON.parse(data);
	obiekt.entries.sort(function(a,b){return a.number-b.number});
	obiektepg = JSON.parse(epg);
	obiektepg.entries.sort(function(a,b){return a.channelNumber - b.channelNumber});
	obiektepgfull = JSON.parse(epgfull);

	tabl = [];
	tabfl = [];

	for (wpis of obiektepg.entries){
        	tabl[wpis.channelNumber] = wpis;
	}
	for (wpis of obiektepgfull.entries){
	       	tabfl[wpis.eventId] = wpis;
	}

	for (kanal of obiekt.entries){
	var star1, stop1, epis1, star2, epis2;
	if (tabl[kanal.number] != undefined){
        	star1 = tabl[kanal.number].start;
        	stop1 = tabl[kanal.number].stop;
	        epis1 = tabl[kanal.number].title;
       		if(tabl[kanal.number].nextEventId != undefined){
             	  var nextev = tabl[kanal.number].nextEventId;
             	   if (tabfl[nextev] != undefined){
                        star2 = tabfl[nextev].start;
                        epis2 = tabfl[nextev].title;
                }
        }
}

res.write(htmlvar.PlakietkaKanalu(kanal.uuid, kanal.number, kanal.name, star1, stop1, epis1, star2, epis2));
}

res.write(htmlvar.FooterListyKan);
res.write(htmlvar.FooterTB1);
res.write(htmlvar.TB2);
res.write(htmlvar.FooterGlowny);
res.end();
}


function Proc(req, res){

if(urlparse.parse(req.url).pathname == "/play" || urlparse.parse(req.url).pathname == "/stream"){
	var chanid = urlparse.parse(req.url, true).query.cid;
	var codec = urlparse.parse(req.url, true).query.codec;
	var vidtag = urlparse.parse(req.url, true).query.htmlvidtag;

	if(chanid.length == 32){
		request({"url": 'http://10.0.8.100:9981/play/ticket/stream/channel/' + chanid,
			"auth": {
				"user": "deadbeef",
				"pass": "deadbeef",
				"sendImmediately": false
			},
			"headers": {
				"User-Agent": "Firefox/52.0"
			}
                },function(RequestError,RequestRespons,data2){

			if (codec != undefined){
				profile = "&profile="+codec;
			}
			else{
				profile = "";
			}
			profile = profile.trim();
			httindx = data2.indexOf("http");

			if (urlparse.parse(req.url).pathname == "/stream"){
				
			}

			if (vidtag != undefined){
				res.writeHead(200, {'Content-Type': 'text/html'});
				res.write(htmlvar.VidView1);
				res.write(`<source src="` + data2.substring(httindx, httindx+118) + profile + `">`);
				res.write(htmlvar.VidView2);
				res.end();
			}
			else{

			/*if (req.headers['user-agent'].indexOf("droid") != -1){
			res.writeHead(303, {'Location': data2.substring(httindx, httindx+121) + profile });
			res.end();
			}
			else{*/
			res.writeHead(200, {'Content-Disposition': 'attachment; filename="'+chanid+'.m3u"', 'Content-Type': 'audio/x-mpegurl'});
			res.write("#EXTM3U\n");
			res.write("#EXTINF:-1,TVHeadend Stream\n");
			res.write(data2.substring(httindx, httindx+118) + profile);
			res.write("\n");
			res.end();

			//}
			}
                //});
                //}).on("error", (err) => {
                //console.log("Error: " + err.message);
                });
	}
}
else{
//res.writeHead(200, {'Content-Type': 'text/html'});
//res.write(htmlvar.NaglowekGlowny);
//res.write(htmlvar.NaglowekTB1);
//fetch(res);
normalnastrona(res);
}
}


http.createServer(Proc).listen(12321);
