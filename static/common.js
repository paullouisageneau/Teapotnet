if(!String.linkify) {
	String.prototype.linkify = function() {

		// http://, https://, ftp://
		var urlPattern = /\b(?:https?|ftp):\/\/[a-z0-9\-+&@#\/%?=~_|!:,.;]*[a-z0-9\-+&@#\/%=~_|]/gim;

		// www. sans http:// or https://
		var pseudoUrlPattern = /(^|[^\/])(www\.[\S]+(\b|$))/gim;

		// Email addresses
		var emailAddressPattern = /[a-zA-Z0-9_\-]+@[a-zA-Z0-9_\-]+?(?:\.[a-zA-Z]{2,6})+/gim;

		return this
		    .replace(urlPattern, '<a href="$&">$&</a>')
		    .replace(pseudoUrlPattern, '$1<a href="http://$2">$2</a>')
		    .replace(emailAddressPattern, '<a href="mailto:$&">$&</a>');
      };
}

if(!String.capitalize) {
	String.prototype.capitalize = function () {
		return this.replace(/\w\S*/g, function(txt){return txt.charAt(0).toUpperCase() + txt.substr(1).toLowerCase();});
	};
}

function popup(url, redirect) {
  
	w = window.open(url, "_blank", "status=0,toolbar=0,scrollbars=0,menubar=0,directories=0,resizeable=1,width=310,height=500");
	if(window.focus) w.focus();
	if(typeof redirect !== 'undefined') document.location.href = redirect;
	return false;
}

function transition(selector, html) {
	
	var elem = $(selector);
	if(elem.html() == html) return elem;
	return elem.fadeOut('slow', function() {
	  	$(this).html(html);
		$(this).fadeIn('slow');
	});
}

function resizeContent() {
  
	var content = $('#content');
	if(content.length)
	{
		var wh = $(window).height();
		var ct = content.offset().top;
		var h = Math.max(wh - ct - 40, 240);
		content.height(h);
	}

	var chatpanel = $('#chatpanel');
	var chatpanelmessage = $('#chatpanel .message');
	if(chatpanel.length && chatpanelmessage.length)
        {
		chatpanelmessage.height(Math.max(chatpanel.height()-10, 20));
        }
}

$(document).ready( function() {
	resizeContent();
});

$(window).resize( function() {
	resizeContent();
});

MessageSound = new Audio('/message.ogg');
MessageSound.load();

function playMessageSound() {
	MessageSound.play();
}

var queryContactsUserName = '';
var queryContactsTimeout = 5000;
var queryContactsCallback;

function queryContactsInfo(userName, timeout, callback) {
	$.ajax({
		url: '/'+userName+'/contacts/?json',
		dataType: 'json',
		timeout: timeout,
		success: function(data) {
			play = false;
			$.each(data, function(uname, info) {
				if(info['newmessages'] == 'true') {
					play = true;
				}
			});
			if(play) playMessageSound();
			callback(data);
		}
	});
}

function setContactsInfoCallback(userName, period, callback) {
	
	queryContactsUserName = userName;
	queryContactsTimeout = period;
	queryContactsCallback = callback;
	setInterval( function() {
		queryContactsInfo(queryContactsUserName, queryContactsTimeout, queryContactsCallback);
	}, period);
	$(document).ready( function() {
		queryContactsInfo(queryContactsUserName, queryContactsTimeout, queryContactsCallback);
	});
}
