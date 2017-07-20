/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
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

// ==================== New prototypes ====================

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

		// Youtube video
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

var Notification = window.Notification || window.mozNotification || window.webkitNotification;
if(Notification) {
	Notification.requestPermission(function (permission) {
		// console.log(permission);
	});
}

// ==================== Helper functions ====================

function formatTime(timestamp) {
	var date = new Date(timestamp*1000);
	//return date.toLocaleDateString() + " " + date.toLocaleTimeString();
	return date.getFullYear() + '-' + ("0" + (date.getMonth() + 1)).slice(-2) + '-' + ("0" + date.getDate()).slice(-2) + ' ' + ("0" + date.getHours()).slice(-2) + ':' + ("0" + date.getMinutes()).slice(-2) + ':' + ("0" + date.getSeconds()).slice(-2);
}

function formatBytes(bytes, decimals) {
	if(bytes == 0) return '0 B';
	var k = 1000;
	var dm = decimals + 1 || 3;
	var sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
	var i = Math.min(Math.floor(Math.log(bytes) / Math.log(k)), 4);
	return (bytes / Math.pow(k, i)).toPrecision(dm) + ' ' + sizes[i];
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

function isPageHidden() {
	//return document.hidden || document.msHidden || document.webkitHidden;
	return !top.document.hasFocus();
}

function setCookie(name,value,exdays) {
	if (exdays) {
		var exdate = new Date();
		exdate.setDate(exdate.getDate() + exdays);
		var expires = "; expires="+exdate.toUTCString();
	}
	else var expires = "";
	document.cookie = name + "=" + value + expires;
}

function getCookie(name) {
	var value = document.cookie;
	var start = value.indexOf(' ' + name + '=');
	if (start < 0) {
		start = value.indexOf(name + '=');
	}
	if (start < 0) {
		value = null;
	}
	else {
		start = value.indexOf('=', start) + 1;
		var end = value.indexOf(';', start);
		if (end < 0) {
			end = value.length;
		}
		value = unescape(value.substring(start, end));
	}
	return value;
}

function unsetCookie(name) {
	setCookie(name, "", -1);
}

function checkCookie(name) {
	var value = getCookie(name);
	if (value!=null && value!='')
		return true;
	return false;
}

var OriginalTitle = top.document.title;

function resetTitle() {
	top.document.title = OriginalTitle;
}

var MailSound = null;

function playMailSound() {
	if(window.Audio) {
		if(MailSound == null) {
			if((new Audio()).canPlayType('audio/ogg; codecs="vorbis"') != "") MailSound = new Audio('/static/message.ogg');
			else MailSound = new Audio('/static/message.m4a');
			MailSound.load();
		}
		MailSound.play();
	}
}

function notify(title, message, tag) {
	if(Notification) {
		return new Notification(
			title, {
				body: message,
				icon: "/static/icon.png",
				tag: tag
			}
		);
	}
	return null;
}

// ==================== Teapotnet functions ====================

function getAuthenticatedUser() {
	var name = getCookie('user_name');
	if(name && checkCookie('auth_'+name))
		return name;

	var start = document.cookie.indexOf('auth_');
	if(start < 0) return null;
	start+=5;
	var end = document.cookie.indexOf('=', start);
	if(end < 0) return null;
	return unescape(document.cookie.substring(start, end));
}

function getAuthenticatedUserIdentifier() {
	var name = getCookie('user_name');
	if(name && checkCookie('auth_'+name))
		return getCookie('user_identifier');
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

// ==================== Document callbacks ====================

$(document).ready( function() {
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

	var user = getAuthenticatedUser();
	if(user) {
		$('#logo a').attr('href', '/user/'+user);
		$('#backlink').attr('href', '/user/'+user);
	}
});

//$(window).resize(resizeContent());
$(window).focus(resetTitle);
$(window).blur(resetTitle);
$(window).keydown(resetTitle);
$(window).mousedown(resetTitle);
