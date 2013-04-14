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

var deviceAgent = navigator.userAgent.toLowerCase();
if(deviceAgent.indexOf("android") > -1) $('#csslink').attr('href', '/android.css');

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

var queryInfoUrl = '';
var queryInfoTimeout = 5000;
var queryInfoCallback;

function queryInfo(url, timeout, callback) {
	$.ajax({
		url: url,
		dataType: 'json',
		timeout: timeout,
		success: function(data) {
			callback(data);
		}
	});
}

function setInfoCallback(url, period, callback) {
	
	queryInfoUrl = url;
	queryInfoTimeout = period;
	queryInfoCallback = callback;
	setInterval( function() {
		queryInfo(queryInfoUrl, queryInfoTimeout, queryInfoCallback);
	}, period);
	$(document).ready( function() {
		queryInfo(queryInfoUrl, queryInfoTimeout, queryInfoCallback);
	});
}
