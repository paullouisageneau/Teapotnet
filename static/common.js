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

if(!String.appendParam) {
	String.prototype.appendParam = function(key, value) {
	    var sep = (this.indexOf('?') > -1) ? '&' : '?';
	    return this + sep + encodeURIComponent(key) + '=' + encodeURIComponent(value);
	}
}

if(!String.linkify) {
	String.prototype.linkify = function() {

		// http://, https://, ftp://
		var urlPattern = /(^|\s)((?:https?|ftp):\/\/[a-z0-9\-+&@#\/%?=~_|!:,.;]*[a-z0-9\-+&@#\/%=~_|])([!?:,.;]*(?:$|\s))/gim;

		// www. without http:// or https://
		var pseudoUrlPattern = /(^|\s)(www\.[\S]+)([!?:,.;]*(?:$|\s))/gim;

		// Email addresses
		var emailAddressPattern = /(^|\s)([a-zA-Z0-9_\-]+@[a-zA-Z0-9_\-]+?(?:\.[a-zA-Z]{2,6})+)([!?:,.;]*(?:$|\s))/gim;

		// Youtube video pattern
		var youtubePattern = /(?:^|\s)(?:https?:\/\/)?(?:www\.)?(?:youtube\.com|youtu\.be)\/(?:watch\?v=)?([^&\s]+)(?:&[^&\s]+)*([!?:,.;]*(?:$|\s))/gim;
		var youtubeFrame = '<iframe width="427" height="240" src="http://www.youtube.com/embed/$1" frameborder="0" allowfullscreen></iframe>';
	
		return this
		    .replace(youtubePattern, youtubeFrame)
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

if(!String.smileys) {
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

	var boardpanel = $('#board .panel');
	var boardinput = $('#board .input');
	if(boardpanel.length && boardinput.length) {
		boardinput.height(boardpanel.height()-10);
        }
        
        var boardmessages = $('#mail');
	if(boardmessages.length) {
		boardmessages.scrollTop(boardmessages[0].scrollHeight);
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
	
	$('.filestr').css('cursor', 'pointer').click(function() {
		window.location.href = $(this).find('a').attr('href');
	});
	
	$('body').on('keypress','textarea', function (e) {
		if (e.keyCode == 13 && !e.shiftKey) {
			$(this).closest('form').submit();
			return false;
		}
	});
});

$(window).resize( function() {
	resizeContent();
});

function isPageHidden() {
	//return document.hidden || document.msHidden || document.webkitHidden;
	return !document.hasFocus();
}

var MailSound = null;

if(window.Audio)
{
	if((new Audio()).canPlayType('audio/ogg; codecs="vorbis"') != "") MailSound = new Audio('/message.ogg');
	else MailSound = new Audio('/message.m4a');
	if(MailSound != null) MailSound.load();
}

function playMailSound() {
	if(MailSound != null) MailSound.play();
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
		},
		statusCode: {
			401: function() {
				window.location.href = "/";
			}
		}
	})
	.done(function() {
		setTimeout(function() {
			setCallback(url, period, callback);
		}, period);
	})
	.fail(function() {
		setTimeout(function() {
			setCallback(url, period, callback);
		}, period);
	});
}

function setMailsReceiver(url, object) {

	$(window).blur(function() {
		$(object).find('.message').addClass('oldmessage');
	});
	setMailsReceiverRec(url, object, 0);
}

function updateMailsReceiver(url, object) {
	$(object).html("");
	setMailsReceiver(url, object);
}

var BaseDocumentTitle = document.title;
var NbNewMails = 0;

function clearNewMails() {
	if(NbNewMails) {
		document.title = BaseDocumentTitle;
		NbNewMails = 0;
	}
}

$(window).focus(clearNewMails);
$(window).blur(clearNewMails);
$(window).keydown(clearNewMails);
$(window).mousedown(clearNewMails);

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
			$.each(data.contacts, function(uname, contact) {
				
				var isSelf = (contact.prefix.substr(contact.prefix.length-6) == 'myself');
				
				if ($('#contact_'+uname).length == 0) { // if div does not exist
					$(object).append('<div class=\"contactstr\"><div id=\"contact_'+uname+'\"><a href=\"'+contact.prefix+'\">'+uname+'</a><span class=\"messagescount\"></span><span class=\"status\"></span></div><div id=\"contactinfo_'+uname+'\" class=\"contactinfo\"></div></div>');
				}

				$('#contact_'+uname).attr('class', contact.status);
				transition($('#contact_'+uname+' .status'), contact.status.capitalize());
				
				if($('#contactinfo_'+uname).html() == '') {
					$('#contact_'+uname).click(function(event) {
						if($(window).width() < 1024) $('#contactinfo_'+uname).toggle();
						else $('#contactinfo_'+uname).slideToggle('fast');
					});
					$('#contact_'+uname+' a').click(function(event)
					{
						event.stopPropagation(); // So the div contactinfo is not displayed when clicked on contact link
					});
					$('#contact_'+uname).hover(function () {
						$(this).css('cursor','pointer');
					});
				}
				
				$('#contactinfo_'+uname).html('<span class=\"name\">'+contact.name+'@'+contact.tracker+'</span><br><span class=\"linkfiles\"><a href=\"'+contact.prefix+'/files/\"><img src="/icon_files.png" alt="Files"/></a></span><span class=\"linkprofile\"><a href=\"'+contact.prefix+'/profile/\"><img src="/icon_profile.png" alt="Files"/></a></span>');
				if(!isSelf) {
					$('#contactinfo_'+uname).append('<span class=\"linkboard\"><a href=\"'+contact.prefix+'/board/\"><img src="/icon_board.png" alt="Board"/></a></span>');
					$('#contactinfo_'+uname).append('<span class=\"linkchat\"><a href=\"'+contact.prefix+'/chat/\"><img src="/icon_chat.png" alt="Messages"/></a></span>');
				
					var count = parseInt(contact.messages);
					var str = '';
					if(count != 0) str = ' <a href=\"'+contact.prefix+'/chat/\">('+count+')</a>';
					transition($('#contact_'+uname+' .messagescount'), str);
					
					/*if(count > 0 && contact.newmessages)
					{
						play = true;
						
						var notif = notify("New private message from " + uname, "(" + count + " unread messages)", "newmessage_"+uname);
						notif.onclick = function() {
							window.location.href = contact.prefix+'/chat/';
						};
					}*/
				}
			});
		}
	});

}

function setCookie(name,value,exdays)
{
	if (exdays) {
		var exdate = new Date();
		exdate.setDate(exdate.getDate() + exdays);
		var expires = "; expires="+exdate.toUTCString();
	}
	else var expires = "";
	document.cookie = name + "=" + value + expires;
}

function getCookie(name)
{
	var value = document.cookie;
	var start = value.indexOf(' ' + name + '=');
	if (start == -1)
	{
		start = value.indexOf(name + '=');
	}
	if (start == -1)
	{
		value = null;
	}
	else
	{
		start = value.indexOf('=', start) + 1;
		var end = value.indexOf(';', start);
		if (end == -1)
		{
			end = value.length;
		}
		value = unescape(value.substring(start, end));
	}
	return value;
}

function unsetCookie(name)
{
	setCookie(name, "", -1);
}

function checkCookie(name)
{
	var value = getCookie(name);
	if (value!=null && value!='')
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
