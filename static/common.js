/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

	for (var i in smileys) {
		if (smileys.hasOwnProperty(i)) {
			patterns.push('('+i.replace(metachars, "\\$&")+')');
		}
	}

  	return this.replace(new RegExp(patterns.join('|'),'g'), function (match) {
		return (typeof smileys[match] != 'undefined' ?
			'<img src='+url+smileys[match]+'/>' :
			match);
 	});

}

if(!String.linkify) {
	String.prototype.linkify = function() {

		// http://, https://, ftp://
		var urlPattern = /(^|\s)((?:https?|ftp):\/\/[a-z0-9\-+&@#\/%?=~_|!:,.;]*[a-z0-9\-+&@#\/%=~_|])($|\s)/gim;

		// www. without http:// or https://
		var pseudoUrlPattern = /(^|\s)(www\.[\S]+)($|\s)/gim;

		// Email addresses
		var emailAddressPattern = /(^|\s)([a-zA-Z0-9_\-]+@[a-zA-Z0-9_\-]+?(?:\.[a-zA-Z]{2,6})+)($|\s)/gim;

		return this
		    .replace(urlPattern, '$1<a target="_blank" href="$2">$2</a>$3')
		    .replace(pseudoUrlPattern, '$1<a target="_blank" href="http://$2">$2</a>$3')
		    .replace(emailAddressPattern, '$1<a href="mailto:$2">$2</a>$3')
		    
		    // Support for bold and italic
		    .replace(/(^|\s)\*([^\/\*_<>]*)\*($|\s)/g,'$1<b>$2</b>$3')
		    .replace(/(^|\s)_([^\/\*_<>]*)_($|\s)/g,'$1<i>$2</i>$3');
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
	//return date.toLocaleDateString() + " " + date.toLocaleTimeString();
	return date.getFullYear() + '-' + ("0" + (date.getMonth() + 1)).slice(-2) + '-' + ("0" + date.getDate()).slice(-2) + ' ' + ("0" + date.getHours()).slice(-2) + ':' + ("0" + date.getMinutes()).slice(-2) + ':' + ("0" + date.getSeconds()).slice(-2);
}

function getBasePath(nbFolders) {

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
	setTimeout(displayLoading, 250);
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

function isPageHidden() {
	return document.hidden || document.msHidden || document.webkitHidden;
}

var MessageSound = null;

if(window.Audio)
{
	if((new Audio()).canPlayType('audio/ogg; codecs="vorbis"') != "") MessageSound = new Audio('/message.ogg');
	else MessageSound = new Audio('/message.m4a');
	if(MessageSound != null) MessageSound.load();
}

function playMessageSound() {
	if(MessageSound != null) MessageSound.play();
}

var Notification = window.Notification || window.mozNotification || window.webkitNotification;
if(Notification)
{
	Notification.requestPermission(function (permission) {
		// console.log(permission);
	});
}

function notify(title, message, tag) {
	if(Notification) {
		return new Notification(
			title, {
				body: message,
				icon: "/icon.png",
				tag: tag
			}
		);
	}
	
	return null;
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
	
	setMessagesReceiverRec(url, object, 0);
}

var BaseDocumentTitle = document.title;
var NbNewMessages = 0;

function clearNewMessages() {
	document.title = BaseDocumentTitle;
	NbNewMessages = 0;
}

$(window).focus(clearNewMessages);
$(window).blur(clearNewMessages);
$(window).keydown(clearNewMessages);
$(window).mousedown(clearNewMessages);

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

function displayContacts(url, period, object) {

	setCallback(url, period, function(data) {
		$(object).find('p').remove();
		if(data != null) {
			var totalmessages = 0;
			var play = false;
			$.each(data, function(uname, info) {
				
				var isSelf = (info.prefix.substr(info.prefix.length-6) == 'myself');
				
				if ($('#contact_'+uname).length == 0) { // if div does not exist
					$(object).append('<div class=\"contactstr\"><div id=\"contact_'+uname+'\"><a href=\"'+info.prefix+'\">'+uname+'</a><span class=\"messagescount\"></span><span class=\"status\"></span></div><div id=\"contactinfo_'+uname+'\" class=\"contactinfo\"></div></div>');
				}

				$('#contact_'+uname).attr('class', info.status);
				transition($('#contact_'+uname+' .status'), info.status.capitalize());
				
				if($('#contactinfo_'+uname).html() == '') {
					$('#contact_'+uname).click(function(event) {
						if($(window).width() < 1024) $('#contactinfo_'+uname).toggle();
						else $('#contactinfo_'+uname).slideToggle('fast');
					});
					$('#contact_'+uname+' a').click(function(event)
					{
						event.stopPropagation(); // So the div infosContact is not displayed when clicked on contact link
					});
					$('#contact_'+uname).hover(function () {
						$(this).css('cursor','pointer');
					});
				}
				
				$('#contactinfo_'+uname).html('<span class=\"name\">'+info.name+'@'+info.tracker+'</span><br><span class=\"linkfiles\"><a href=\"'+info.prefix+'/files/\"><img src="/icon_files.png" alt="Files"/></a></span><span class=\"linkprofile\"><a href=\"'+info.prefix+'/profile/\"><img src="/icon_profile.png" alt="Files"/></a></span>');
				if(!isSelf) {
					$('#contactinfo_'+uname).append('<span class=\"linkchat\"><a href=\"'+info.prefix+'/chat/\"><img src="/icon_chat.png" alt="Chat"/></a></span>');
				
					var count = parseInt(info.messages);
					var str = '';
					if(count != 0) str = ' ('+count+')';
					transition($('#contact_'+uname+' .messagescount'), str);
					totalmessages+= count;
					if(count > 0 && info.newmessages)
					{
						play = true;
						
						var notif = notify("New private message from " + uname, "(" + count + " unread messages)", "newmessage_"+uname);
						notif.onclick = function() {
							window.location.href = info.prefix+'/chat/';
						};
					}
				}
			});
			
			if(totalmessages != 0) {
				document.title = title+' ('+totalmessages+')';
				if(play) playMessageSound();
			}
			else document.title = title;
		}
	});

}

function setMessagesReceiverRec(url, object, next) {

	var timeout;

	if(stopBool) {
		//alert('Function stopped !');
		clearTimeout(timeout);
	}

	var baseUrl = url;
	
	if(next > 0) {
		url+= (url.contains('?') ? '&' : '?') + 'next=' + next;
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
				if(message.number >= next) next = message.number + 1;
				if(message.parent && $('#message_'+message.parent).length == 0) continue;
				
				var id = "message_" + message.stamp;
				$('#'+id).remove();
				
				var isLocalRead = (!message.incoming || message.isread);
				var isRemoteRead = (message.incoming || message.isread);
				
				var author;	// equals uname when non-relayed
				var authorHtml;
				if(!message.incoming) {
					var link = getBasePath(1) + 'myself/';
					author = message.author
					authorHtml = '<a href="'+link+'"><img class="avatar" src="'+link+'avatar">'+message.author.escape()+'</a>';
				}
				else if(message.contact) {
					var link = getBasePath(1) + 'contacts/' + message.contact.escape() + '/';
					if(message.relayed) {
						author = message.author;
						authorHtml = '<img class="avatar" src="/default_avatar.png">'+message.author.escape()+' (via&nbsp;<a href="'+link+'">'+message.contact.escape()+'</a>)';
					}
					else {
						author = message.contact;
						authorHtml = '<a href="'+link+'"><img class="avatar" src="'+link+'avatar">'+message.contact.escape()+'</a>';
					}
				}
				else {
					author = message.author;
					authorHtml = message.author.escape();
				}
	      
				var div = '<div id="'+id+'" class="message"><span class="header"><span class="author">'+authorHtml+'</span><span class="date">'+formatTime(message.time).escape()+'</span></span><span class="content">'+message.content.escape().smileys().linkify().split("\n").join("<br>");+'</span></div>';
				
				if(message.public) {
					var idReply = "reply_" + id;
					
					if(!message.parent) {
						$(object).prepend('<div class="conversation">'+div+'</div>');
						$('#'+id).append('<a href="#" class="button" onclick="clickedReply('+idReply+'); return false;">Reply</a>');
					}
					else {
						$('#message_'+message.parent).parent().append(div);
						$('#'+id).addClass('childmessage');
					}
					
					// Reply form
					$('#'+id).parent().append('<div id='+idReply+' class="reply"><form name="replyform'+id+'" action="#" method="post" enctype="application/x-www-form-urlencoded"><textarea class="replyinput" name="reply_'+id+'"></textarea></form></div>');
				
				}
				else {
					$(object).append(div);
					if(!isLocalRead) 
					{
						NbNewMessages++;
						if(isPageHidden()) notify("New message from " + author, message.content, "message_"+author);
					}
					
					setTimeout(function() { 
						$(object).scrollTop($(object)[0].scrollHeight);
					}, 10);
				}
				
				if(!message.incoming) $('#'+id).addClass('me');
				if(isLocalRead) $('#'+id).addClass('oldmessage');
	      
				if('attachment' in message.headers) {
					
					$('#'+id+' .header').after('<span class="attachment"></span>');
					$('#'+id+' .attachment').html('<img class="icon" src="/file.png">Loading attachment...');
					
					var url = '/'+message.headers.attachment;
					
					(function(id, url) {
						var request = $.ajax({
							type: "HEAD",
							url: url,
							timeout: 60000
						})
						.done(function(data) {
							var name = request.getResponseHeader('Content-Name');
							var type = request.getResponseHeader('Content-Type');
							var media = type.substr(0, type.indexOf('/'));
							
							if(media == 'image') {
								$('#'+id+' .attachment').html('<a href="'+url+'" target="_blank" class="preview"><img class="preview" src="'+url+'" alt="'+name.escape()+'"></a>');
							}
							else if(media == 'audio' || media == 'video') {
								$('#'+id+' .attachment').html('<a href="'+url+(deviceAgent.indexOf('android') < 0 ? '?play=1' : '')+'"><img class="icon" src="/file.png"><span class="filename">'+name.escape()+'</span></a>');
							}
							else {
								$('#'+id+' .attachment').html('<a href="'+url+'" target="_blank"><img class="icon" src="/file.png"><span class="filename">'+name.escape()+'</span></a>');
							}
							
							setTimeout(function() { 
								$(object).scrollTop($(object)[0].scrollHeight);
							}, 10);
						})
						.fail(function(jqXHR, textStatus) {
							$('#'+id+' .attachment').html('<img class="icon" src="/file.png">Attachment not available');
						});
					
					})(id, url);
				}
			}
			
			
			if(NbNewMessages) {
				document.title = '(' + NbNewMessages + ') ' + BaseDocumentTitle;
				if(!document.hasFocus()) playMessageSound();
			}
		}

		timeout = setTimeout(function() {
			setMessagesReceiverRec(baseUrl, object, next);
		}, 1000);

	})
	.fail(function(jqXHR, textStatus) {
		timeout = setTimeout(function() {
			setMessagesReceiverRec(baseUrl, object, next);
		}, 1000);

	});

	if(stopBool)
	{
		//alert('Function stopped !');
		timeout = setTimeout(function() {clearTimeout(timeout);}, 1000); // ugly but tricks slowness of javascript
	}
}

function setCookie(c_name,value,exdays)
{
	var exdate=new Date();
	exdate.setDate(exdate.getDate() + exdays);
	var c_value=escape(value) + ((exdays==null) ? '' : '; expires='+exdate.toUTCString());
	document.cookie=c_name + '=' + c_value;
}

function getCookie(c_name)
{
	var c_value = document.cookie;
	var c_start = c_value.indexOf(' ' + c_name + '=');
	if (c_start == -1)
	{
		c_start = c_value.indexOf(c_name + '=');
	}
	if (c_start == -1)
	{
		c_value = null;
	}
	else
	{
		c_start = c_value.indexOf('=', c_start) + 1;
		var c_end = c_value.indexOf(';', c_start);
		if (c_end == -1)
		{
			c_end = c_value.length;
		}
		c_value = unescape(c_value.substring(c_start,c_end));
	}
	return c_value;
}

function checkCookie(name)
{
	var c_value = getCookie(name);
	if (c_value!=null && c_value!='')
		return true;
	return false;
}

function isJsonString(str) {
    try {
	JSON.parse(str);
    } catch (e) {
	return false;
    }
    return true;
}
