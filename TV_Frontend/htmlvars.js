function kodeki(){
	var odpowiedz = "";
	odpowiedz += '<ul class="demo-list-item mdl-list">';
	var kodeki = [
	{ nazwa: "PASS - bez transkodowania", wartosc: "pass" },
        { nazwa: "H265 HD", 				wartosc: "view-h265" },
        { nazwa: "H265 SD - Niska przepustowosc", 	wartosc: "view-h265-sd-lb" },
        { nazwa: "H264 HD", 				wartosc: "view-h264" },
        { nazwa: "H264 HD - Niska przepustowosc", 	wartosc: "view-h264-lb" },
        { nazwa: "H264 SD", 				wartosc: "view-h264-sd" },
        { nazwa: "H264 SD - niska przepustowosc", 	wartosc: "view-h264-sd-lb" },
        { nazwa: "VP9 HD", 				wartosc: "view-vp9" },
        { nazwa: "VP9 SD", 				wartosc: "view-vp9-sd" },
        { nazwa: "VP9 SD - Niska przepustowosc", 	wartosc: "view-vp9-sd-lb" },
        { nazwa: "VP8", 				wartosc: "view-vp8" },
        { nazwa: "MPEG2", 				wartosc: "view-mpeg2" }
	];
	var ilosckodekow = kodeki.length;
	for (var i = 1; i <= ilosckodekow; i++){
		odpowiedz += (`
		   <li class="mdl-list__item">
                   <span class="mdl-list__item-primary-content">
                   <label class="mdl-radio mdl-js-radio mdl-js-ripple-effect" for="option-codec-` + i + `">
                   <input type="radio" id="option-codec-` + i + `" class="mdl-radio__button" name="codec" value="` + kodeki[i-1].wartosc + `"`);
		if ( i == 1){
		  odpowiedz += `checked`;
		}
		 odpowiedz += (`>
                   <span class="mdl-radio__label">` + kodeki[i-1].nazwa + `</span>
                   </label>
                   </span>
                   </li>
		`);
	}

	odpowiedz += '</ul>';
return odpowiedz;
}

module.exports.kartasub = function (wejscie){
	var odpowiedz = "";
	odpowiedz +=(`
<div class="mdl-grid">
<div class="mdl-cell mdl-cell--2-offset-desktop mdl-cell--1-offset-tablet mdl-cell--8-col-desktop mdl-cell--6-col-tablet mdl-cell--4-col-phone">
<div style="width: 100%" class="mdl-card mdl-shadow--4dp">
<div class="mdl-card__title mdl-card--border">
<h2  class="mdl-card__title-text">SUBSKRYPCJE: `+wejscie.totalCount+`</h2>
</div>
<div class="mdl-card__supporting-text">`);
for(sub of wejscie.entries){
        odpowiedz += ("<p><b>"+sub.title+"</b>");
        if(sub.channel != undefined){
        odpowiedz += ("<small><i> "+sub.channel+"</i></small>");
        }
        if(sub.username != undefined){
        odpowiedz += ("<small style='font-size: x-small;'><i> "+sub.username+"</i></small>");
        }
        if(sub.hostname != undefined){
        odpowiedz += ("<small style='font-size: x-small;'><i> "+sub.hostname+"</i></small>");
        }
        if(sub.profile != undefined){
        odpowiedz += ("<small style='font-size: xx-small;'><i> "+sub.profile+"</i></small>");
        }
        if(sub.state != undefined){
        odpowiedz += ("<small style='font-size: xx-small;'><i> "+sub.state+"</i></small>");
        }
    }
    odpowiedz += (`</div> </div> </div> </div>`);

return odpowiedz;
}

module.exports.NaglowekGlowny = `
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>tvhGUI</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/icon?family=Material+Icons">
<link rel="stylesheet" href="https://code.getmdl.io/1.3.0/material.indigo-pink.min.css">
<script defer src="https://code.getmdl.io/1.3.0/material.min.js"></script>
</head>
<body>
<!--
<script>
setInterval(function() {
console.log(document.getElementById("glokno").scrollTop);
sessionStorage.scrollTop = document.getElementById("glokno").scrollTop;
document.location.reload();
}, 300 * 1000);

window.addEventListener("load", function(){
if (sessionStorage.scrollTop != "undefined") {
    document.getElementById("glokno").scrollTop = sessionStorage.scrollTop;
}
});
</script>
-->
<div class="mdl-layout mdl-js-layout mdl-layout--fixed-header mdl-layout--fixed-tabs">
<header class="mdl-layout__header">
<div style="padding: 0 16px 0 24px;" class="mdl-layout__header-row">
<span class="mdl-layout-title">MSXtv</span>
</div>
<div class="mdl-layout__tab-bar mdl-js-ripple-effect">
<a href="#fixed-tab-1" class="mdl-layout__tab is-active">Kanały</a>
<a href="#fixed-tab-2" class="mdl-layout__tab">Ustawienia</a>
</div>
</header>
<main id="glokno" class="mdl-layout__content">
`;

module.exports.NaglowekTB1 = `
<section class="mdl-layout__tab-panel is-active" id="fixed-tab-1">
<div class="page-content">

`;

module.exports.NaglowekListyKan = `
<div class="mdl-grid">
<div class="mdl-cell mdl-cell--2-offset-desktop mdl-cell--8-col-desktop mdl-cell--8-col-tablet mdl-cell--4-col-phone">
<ul class="mdl-list">
`;

module.exports.FooterListyKan = `
</ul>
</div>
</div>
`;

module.exports.FooterTB1 = `
</div>
</section>
`;

module.exports.TB2 = `
<section class="mdl-layout__tab-panel" id="fixed-tab-2">
        <div class="page-content">
          <form id="ogld" action="/play" method="get">
           <div class="mdl-grid">
            <div class="mdl-cell mdl-cell--2-offset-desktop mdl-cell--1-offset-tablet mdl-cell--8-col-desktop mdl-cell--6-col-tablet mdl-cell--4-col-phone">
             <div style="width: 100%" class="mdl-card mdl-shadow--4dp">
              <div class="mdl-card__title mdl-card--border">
               <h2  class="mdl-card__title-text">STRUMIEŃ</h2>
              </div>
              <div style="width: initial;" class="mdl-card__supporting-text mdl-card--border">
               <label class="mdl-switch mdl-js-switch mdl-js-ripple-effect" for="switch-htmlvid">
                <input name="htmlvidtag" type="checkbox" id="switch-htmlvid" class="mdl-switch__input">
                <span class="mdl-switch__label">Używaj tagu HTML5 &ltvideo&gt</span>
               </label>
              </div>
              <div class="mdl-card__supporting-text">
` + kodeki() + `
              </div>
             </div>
            </div>
           </div>
          </form>
         </div>
</section>
`;

module.exports.FooterGlowny = `
</main>
</div>
</body>
`;

module.exports.VidView1 = `
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>tvhGUI</title>
</head>
<body>
<video width='400' height='400' controls>
`;

module.exports.VidView2 = `
</video>
</body>
</html>
`;


module.exports.PlakietkaKanalu = function(uuid, numer, nazwa, start1, stop1, ep1, start2, ep2){
var procent = 0;
var odpowiedz = "";
if (uuid == undefined){return odpowiedz;}
if (numer == undefined){return odpowiedz;}
if (nazwa == undefined){return odpowiedz;}
if (start1 != undefined && stop1 == undefined){return odpowiedz;}
odpowiedz += `
<li class="mdl-list__item mdl-list__item--three-line">
 <span class="mdl-list__item-primary-content" style="width: calc(100% - 68px);">
   <div style="display: flex; align-items: center; justify-content: center; font-size: 120%;" class="mdl-list__item-avatar" >` + numer.toString() + `</div>
     <span>`+ nazwa +`</span>
     <span class="mdl-list__item-text-body" style="margin-left: 56px; width: calc(100% - 56px);">
       <div style="text-overflow: ellipsis;height: 40%; overflow: hidden; white-space: nowrap; width: 100%;">
`;

if (start1 != undefined){
odpowiedz += "<span style='font-size: x-small;'>";
odpowiedz += ( "00" + (new Date(start1 * 1000)).getHours().toString()).slice(-2);
odpowiedz += ":";
odpowiedz += ( "00" + (new Date(start1 * 1000)).getMinutes().toString()).slice(-2);
odpowiedz += "</span> ";
odpowiedz += ep1;

var czas = Math.floor( new Date() / 1000);
procent = Math.round(((czas - start1)/(stop1 - start1))*100);
}

odpowiedz += `
       </div>
       <div style="height: 10%; width: 100%;">
        <div id="p`+ kanal.number +`" class="mdl-progress mdl-js-progress" style="width: 100%;"></div>
        <script>
         document.querySelector('#p`+ kanal.number +`').addEventListener('mdl-componentupgraded', function() {
          this.MaterialProgress.setProgress(` + procent + `);
         });
        </script>
       </div>
       <div style="opacity: 0.4; text-overflow: ellipsis;height: 40%; overflow: hidden; white-space: nowrap; width: 100%;">
`;

if (start2 != undefined){
odpowiedz += "<span style='font-size: x-small;'>";
odpowiedz += ( "00" + (new Date(start2 * 1000)).getHours().toString()).slice(-2);
odpowiedz += ":";
odpowiedz += ( "00" + (new Date(start2 * 1000)).getMinutes().toString()).slice(-2);
odpowiedz += "</span> ";
odpowiedz += ep2;
}

odpowiedz += `
       </div>
      </span>
     </span>
     <span class="mdl-list__item-secondary-content">
      <button name="cid" value="`;
odpowiedz += uuid;

odpowiedz += `" type="submit" form="ogld" class="mdl-button mdl-button--accent mdl-js-button mdl-button--icon mdl-button--colored" style="height: 52px; width: 52px; top: -9px;">
       <i class="material-icons" style="font-size: 170%; align-items: center; justify-content: center; height: 100%; width: 100%; display: flex; position: initial; transform: initial;">play_circle_outline</i>
      </button>
     </span>
    </li>
`;
return odpowiedz;
};
