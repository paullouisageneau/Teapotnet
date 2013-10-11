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
		// Do not actually modify the string content here, it will break digest handling
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
		':S'  : 'embarrassed.png\" height=15 width=24',
		':@'  : 'angry.png\" height=20 width=30'
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

function getBasePath(nbFolders) {

switch (arguments.length) {  // Is a fix for fact that overloading is not possible in javascript and some browsers don't support = in prototype

	case 0:
		nbFolders = 1;
	break;

	default:

	break;
}

		var pathArray = window.location.pathname.split('/');
		if(nbFolders > pathArray.length) nbFolders = pathArray.length;
		var basePath = '';
		for(var i=0; i<=nbFolders; i++)
			basePath+= pathArray[i] + '/';
		return basePath;

}

function popup(url, redirect) {
  
	w = window.open(url, "_blank", "status=0,toolbar=0,scrollbars=0,menubar=0,directories=0,resizeable=1,width=480,height=500");
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
	var chatinput = $('#chatpanel textarea.chatinput');
	if(chatpanel.length && chatinput.length) {
		chatinput.height(Math.max(chatpanel.height()-10, 20));
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


var title = document.title;

var saveData;
var toShow = true;

function displayContacts(url, period, object) {

	setCallback(url, period, function(data) {
		if(data!=null) {
			saveData = data;
			var totalmessages = 0;
			var play = false;
			var visible = false;
			$.each(data, function(uname, info) {
				
				if ($('#contact_'+uname).length == 0) { // if div does not exist
					$(object).append('<div class=\"contactstr\"><div id=\"contact_'+uname+'\"><a href=\"'+info.prefix+'\">'+uname+'</a><span class=\"messagescount\"></span><span class=\"status\"></span></div><div id=\"contactinfo_'+uname+'\" class=\"contactinfo\"></div></div>');
				}

				var isSelf = (info.prefix.substr(info.prefix.length-6) == 'myself');
				
				$('#contactinfo_'+uname).html('<span class=\"tracker\">'+info.name+'@'+info.tracker+'</span><span class=\"linkfiles\"><a href=\"'+info.prefix+'/files/\"><img src="/icon_files.png" alt="Files"/></a></span><span class=\"linkprofile\"><a href=\"'+info.prefix+'/profile/\"><img src="/icon_profile.png" alt="Files"/></a></span>');
				if(!isSelf) $('#contactinfo_'+uname).append('<span class=\"linkchat\"><a href=\"'+info.prefix+'/chat/\"><img src="/icon_chat.png" alt="Chat"/></a></span>');
				$('#contactinfo_'+uname).hide();
				
				function displayContactInfo(uname) {
					
					if(visible) {
						if($(window).width() < 1024) $('#contactinfo_'+uname).hide();
						else $('#contactinfo_'+uname).slideUp();
						timeout = setTimeout(function() { visible = false; }, 200);
					}
					else
					{
						if(toShow) {
							if ($(window).width() < 1024) $('#contactinfo_'+uname).show();
							else $('#contactinfo_'+uname).slideDown();
						}
						timeout = setTimeout(function() { visible = true; }, 200);
					}
				}
				/*document.getElementById('contact_'+uname).onmouseover = function()
				{
					displayContactInfo(uname);
				}*/
				document.getElementById('contact_'+uname).onclick = function() {
					displayContactInfo(uname);
				}
				$('#contact_'+uname+' a').click(function()
				{
					toShow = false; // So the div infosContact is not displayed when clicked on contact link
				});
				$('#contact_'+uname).hover(function () {
					$(this).css('cursor','pointer');
				});
				$('#contact_'+uname).attr('class', info.status);
				//document.getElementById('contact_'+uname).classList.add(info.status);
				transition($('#contact_'+uname+' .status'), info.status.capitalize());
				var count = parseInt(info.messages);
				var tmp = '';
				if(count != 0) tmp = ' ('+count+')';
				transition($('#contact_'+uname+' .messagescount'), tmp);
				totalmessages+= count;
				if(info.newmessages == 'true') play = true;
			});
			if(totalmessages != 0) document.title = title+' ('+totalmessages+')';
			else document.title = title;
			if(play) playMessageSound();
		}
		else {
			// TODO
		}
	});

}


function setMessagesReceiverRec(url, object, last) {

	var timeout;

	if(stopBool) {
		//alert('Function stopped !');
		clearTimeout(timeout);
	}

	var baseUrl = url;
	
	if(last != '') {
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

				var div = '<div class="messagewrapper"><div id="'+id+'" class="message"><span class="header"><span class="date">'+formatTime(message.time).escape()+'</span><span class="author">'+message.headers.from.escape()+'</span></span><span class="content">'+message.content.escape().smileys().linkify()+'</span></div></div>';
				
				if(message.public) {
					var idReply = "reply_" + id;
					
					if(!message.parent) {
						$(object).prepend(div);
						$('#'+id).append('<a href="#" class="button" onclick="clickedReply('+idReply+');">Reply</a>');
					}
					else {			
						$('#'+message.parent).parent().append(div);
						$('#'+id).addClass('childmessage');
					}

					if(!message.incoming) $('#'+id).addClass('me');

					// Reply form
					$('#'+id).parent().append('<div id='+idReply+' class="reply"><form name="replyform'+id+'" action="#" method="post" enctype="application/x-www-form-urlencoded"><textarea class="replyinput" name="reply_'+id+'"></textarea></form></div>');
				
				}
				else {
					$(object).append(div);
					if(message.incoming && !message.isread) NbNewMessages++;
					if(message.isread) $('#'+id).addClass('oldmessage');
					if(message.incoming) $('#'+id).addClass('in');
					else $('#'+id).addClass('out');
					$(object).scrollTop($(object)[0].scrollHeight);
				}
				
				if('attachment' in message.headers) {
					
					$('#'+id).append('<span class="attachment">Loading attachment...</span>');
					
					var url = '/'+message.headers.attachment;
					var request = $.ajax({
						type: "HEAD",
						url: url,
						timeout: 60000
					})
					.done(function(data) {
						var name = request.getResponseHeader('Content-Name');
						$('#'+id+' .attachment').html('<img class="icon" src="/file.png"><span class="filename"><a href="'+url+'">'+name+'</a></span>');
						$('#'+id+' .attachment').css('cursor', 'pointer').click(function() {
							window.location.href = $(this).find('a').attr('href');
						});
					})
					.fail(function(jqXHR, textStatus) {
						$('#'+id+' .attachment').html('Attachment not available');
					});
				}
			}
			
			
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
