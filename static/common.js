/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

if(!String.escape) {
	String.prototype.escape = function() {
		return this.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
	}
}

String.prototype.smileys = function() {
	var smileys = {
		':)' : 'smile.png\" height=15 width=24',
		':-)' : 'smile.png\" height=15 width=24',
		':-('  : 'sad.png\" height=15 width=24',
		':('  : 'sad.png\" height=15 width=24',
		':\'-('  : 'sad.png\" height=15 width=24',
		':S'  : 'embarrassed.png\" height=17 width=26',
		':@'  : 'angry.png\" height=24 width=36'
	}, url = "\"/", patterns = [], metachars = /[[\]{}()*+?.\\|^$\-,&#\s]/g;

	for (var i in smileys) 
	{
		if (smileys.hasOwnProperty(i))
		{
			patterns.push('('+i.replace(metachars, "\\$&")+')');
		}
	}

  	return this.replace(new RegExp(patterns.join('|'),'g'), function (match) 
		{
	   		 return typeof smileys[match] != 'undefined' ?
		   	'<img src='+url+smileys[match]+'/>' :
		   	match;
 		 });

}

if(!String.linkify) {
	String.prototype.linkify = function() {

		// http://, https://, ftp://
		var urlPattern = /\b(?:https?|ftp):\/\/[a-z0-9\-+&@#\/%?=~_|!:,.;]*[a-z0-9\-+&@#\/%=~_|]/gim;

		// www. without http:// or https://
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

if(!String.contains) {
	String.prototype.contains = function(it) { 
		return this.indexOf(it) != -1;
	};
}

function formatTime(timestamp) {
	var date = new Date(timestamp*1000);
	return date.toLocaleDateString() + " " + date.toLocaleTimeString();
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

function displayLoading()
{
	if($(document.documentElement).hasClass('loading'))
		$(document.documentElement).addClass('animloading');
}

if (document.documentElement) {
	$(document.documentElement).addClass('loading');
	setTimeout(displayLoading, 300);
}

$(document).ready( function() {
	if($(document.documentElement).hasClass('loading')) {
		$(document.documentElement).removeClass('loading');
		if($(document.documentElement).hasClass('animloading')) {
			  $(document.documentElement).removeClass('animloading');
			  $(document.body).hide();
			  $(document.body).fadeIn(300);
		}
	}
	resizeContent();
	
	$('table.files tr').css('cursor', 'pointer').click(function() {
		window.location.href = $(this).find('a').attr('href');
	});
	
	$('table.contacts tr').css('cursor', 'pointer').click(function() {
		window.location.href = $(this).find('a').attr('href');
	});
	
	$('table.menu tr').css('cursor', 'pointer').click(function() {
		window.location.href = $(this).find('a').attr('href');
	});
});

$(window).resize( function() {
	resizeContent();
});

var MessageSound;
if((new Audio()).canPlayType('audio/ogg; codecs="vorbis"') != "") MessageSound = new Audio('/message.ogg');
else MessageSound = new Audio('/message.m4a');
if(MessageSound != null) MessageSound.load();

function playMessageSound() {
	if(MessageSound != null) MessageSound.play();
}



function setCallback(url, period, callback) {
	
	$.ajax({
		url: url,
		dataType: 'json',
		timeout: period,
		success: function(data) {
			callback(data);
		}
	})
	.done(function(data) {
		setTimeout(function() {
			setCallback(url, period, callback);
		}, period);
	})
	.fail(function(jqXHR, textStatus) {
		setTimeout(function() {
			setCallback(url, period, callback);
		}, period);
	});
}

function setMessagesReceiver(url, object) {

	$(window).blur(function() {
		$(object).find('.message').addClass('oldmessage');
	});
	
	setMessagesReceiverRec(url, object, '');
}

var BaseDocumentTitle = document.title;
var NbNewMessages = 0;

$(window).focus(function() {
	document.title = BaseDocumentTitle;
	NbNewMessages = 0;
});

$(window).blur(function() {
	document.title = BaseDocumentTitle;
	NbNewMessages = 0;
});

var stopBool = false;

function updateMessagesReceiver(url, object) {
	stopBool = true;
	$(object).html("");
	setTimeout(function() {
		setMessagesReceiver(url, object);
		stopBool = false;
	}, 100);
}

function clickedReply(id) 
{
	$('#'+id.id).css('display','block');
	$('textarea[name=\"'+id.id+'\"]').focus();
}

function submitReply(object, idParent) 
{
	post(object, idParent); 
	return false;
}


function setMessagesReceiverRec(url, object, last) {

	var timeout;

	if(stopBool)
	{
		//alert('Function stopped !');
		clearTimeout(timeout);
	}

	var baseUrl = url;
	
	if(last != '')
	{
		if(url.contains('?')) url+= '&last=' + last;
		else url+= '?last=' + last;
	}
	
	$.ajax({
		url: url,
		dataType: 'json',
		timeout: 60000
	})
	.done(function(data) {
		if(data != null) {
			for(var i=0; i<data.length; i++) {
				var message = data[i];
				var id = "message_" + message.stamp;
				last = message.stamp;

				if(!message.parent)
				{
					if(message.public)
					{
						var idReply = "replyTo" + id;

						$(object).append('<div id=\"'+id+'\" class=\"message\"> <span class=\"author\">'+message.headers.from.escape()+'</span> <span class=\"content\">'+message.content.escape().smileys().linkify()+'</span> <br/> <span class=\"date\">'+formatTime(message.time).escape()+'</span> <img src="/reply.png" alt="Reply" class=\"replybutton\" onclick="clickedReply('+idReply+');" /> </div>');
						$('#'+id).addClass('statusdisplay');
						if(!message.incoming) $('#'+id).addClass('me');

						// Search for children and display in case found
						for(var j=0; j<data.length; j++) 
						{
							var messageChild = data[j];
							var idChild = "message_" + messageChild.stamp;
							var idReplyChild = "replyTo" + idChild;
							if(messageChild.parent == id)
							{
								$(object).append('<div id=\"'+idChild+'\" class=\"message\"> <span class=\"author\">'+messageChild.headers.from.escape()+'</span> <span class=\"content\">'+messageChild.content.escape().smileys().linkify()+'</span> <br/> <span class=\"date\">'+formatTime(messageChild.time).escape()+'</span> <img src="/reply.png" alt="Reply" class=\"replybutton\" onclick="clickedReply('+idReply+');" /> </div>');
								$('#'+idChild).addClass('statusdisplay');
								$('#'+idChild).addClass('childmessage');
								if(!message.incoming) $('#'+idChild).addClass('me');
							}
						}

						// Reply form
						$(object).append('<div id='+idReply+' class=\"reply\"><form name=\"formReplyTo'+id+'\" action="#" method="post" enctype="application/x-www-form-urlencoded"><textarea class="replyinput" name=\"replyTo'+id+'\"></textarea></form> </div>');
					
					}
					else
					{
						$(object).append('<div id=\"'+id+'\" class=\"message\"><span class=\"date\">'+formatTime(message.time).escape()+'</span> <span class=\"author\">'+message.headers.from.escape()+'</span> <span class=\"content\">'+message.content.escape().smileys().linkify()+'</span></div>');
						if(message.incoming && !message.isread) NbNewMessages++;
						if(message.isread) $('#'+id).addClass('oldmessage');
						if(message.incoming) $('#'+id).addClass('in');
						else $('#'+id).addClass('out');
					}
				}
			}
			$(object).scrollTop($(object)[0].scrollHeight);
			
			if(NbNewMessages) {
				document.title = '(' + NbNewMessages + ') ' + BaseDocumentTitle;
				playMessageSound();
			}
		}

		timeout = setTimeout(function() {
			setMessagesReceiverRec(baseUrl, object, last);
		}, 1000);

	})
	.fail(function(jqXHR, textStatus) {
		timeout = setTimeout(function() {
			setMessagesReceiverRec(baseUrl, object, last);
		}, 1000);

	});

	if(stopBool)
	{
		//alert('Function stopped !');
		timeout = setTimeout(function() {clearTimeout(timeout);}, 1000); // ugly but tricks slowness of javascript
	}
}
