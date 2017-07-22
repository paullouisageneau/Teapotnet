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

 function displayContacts(url, period, object) {
 	setCallback(url, period, function(data) {
 		$(object).find('p').remove();
 		if(data != null) {
 			$.each(data.contacts, function(uname, contact) {
 				var isSelf = (contact.prefix.substr(contact.prefix.length-6) == 'myself');

 				if ($('#contact_'+uname).length == 0) { // if div does not exist
 					var div = '<div class=\"contactstr\"><div id=\"contact_'+uname+'\"><a href=\"'+contact.prefix+'\">'+(isSelf ? "Myself" : uname)+'</a><span class=\"messagescount\"></span><span class=\"status\"></span></div><div id=\"contactinfo_'+uname+'\" class=\"contactinfo\"></div></div>';
 					if(isSelf) $(object).prepend(div);
 					else $(object).append(div);
 				}

 				$('#contact_'+uname).attr('class', contact.status);
 				if(isSelf && contact.status == "disconnected") transition($('#contact_'+uname+' .status'), "");
 				else transition($('#contact_'+uname+' .status'), contact.status.capitalize());

 				if($('#contactinfo_'+uname).html() == '') {
 					$('#contact_'+uname).click(function(event) {
 						if($(window).width() < 1024) $('#contactinfo_'+uname).toggle();
 						else $('#contactinfo_'+uname).slideToggle('fast');
 					});
 					$('#contact_'+uname+' a').click(function(event) {
 						event.stopPropagation(); // So the div contactinfo is not displayed when clicked on contact link
 					});
 					$('#contact_'+uname).hover(function () {
 						$(this).css('cursor','pointer');
 					});
 				}

 				$('#contactinfo_'+uname).html('<span class=\"linkfiles\"><a href=\"'+contact.prefix+'/files/\"><img src="/static/icon_files.png" alt="Files"/></a></span>');
 				if(!isSelf) {
 					$('#contactinfo_'+uname).append('<span class=\"linkboard\"><a href=\"'+contact.prefix+'/board/\"><img src="/static/icon_board.png" alt="Board"/></a></span>');
 					$('#contactinfo_'+uname).append('<span class=\"linkchat\"><a href=\"'+contact.prefix+'/chat/\"><img src="/static/icon_chat.png" alt="Messages"/></a></span>');

 					var count = parseInt(contact.messages);
 					var str = '';
 					if(count != 0) str = ' <a href=\"'+contact.prefix+'/chat/\">('+count+')</a>';
 					transition($('#contact_'+uname+' .messagescount'), str);

 					if(count > 0 && contact.newmessages)
 					{
 						play = true;

 						var notif = notify("New private message from " + uname, "(" + count + " unread messages)", "newmessage_"+uname);
 						notif.onclick = function() {
 							window.location.href = contact.prefix+'/chat/';
 						};
 					}
 				}
 			});
 		}
 	});
 }
