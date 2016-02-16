/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

function setMailReceiverRec(url, object, period, next) {

	if(typeof this.messagesTimeout != 'undefined')
		clearTimeout(this.messagesTimeout);
	
	$.ajax({
		url: url + (url.contains('?') ? '&' : '?') + 'next=' + next + '&timeout=0',
		dataType: 'json',
		timeout: 300000
	})
	.done(function(array) {
		var posturl = url;
		var count = 0;
		$.each(array, function(i, mail) {
			next++;
			
			$(object).find('p').remove();
			
			var id = "message_" + mail.digest;
			var idReply = "reply_" + mail.digest;
			var idParent = (mail.parent ? "message_" + mail.parent : "");
			var idParentReply = (mail.parent ? "reply_" + mail.parent : "");
			var author = (mail.author ? mail.author : "Anonymous");
			
			if(idParent && $('#'+idParent).length == 0) return;	// Parent not found
			$('#'+id).remove();
			
			var div = '<div id="'+id+'" class="message"><span class="header"><span class="author">'+author+'</span><span class="date">'+formatTime(mail.time).escape()+'</span><span class="time" style="display:none">'+mail.time+'</span></span><span class="content"></span></div>';
			
			if(!idParent) {
				$(object).prepend('<div class="conversation">'+div+'</div>');
				$('#'+id).append('<div class="buttonsbar"></div>');
				
				/*
				if(!mail.passed)
				{
					$('#'+id+' .buttonsbar').append('<a href="#" class="button passlink"><img alt="Pass" src="/arrow_pass.png"></a>');
					(function(id, stamp) {
						$('#'+id+' .passlink').click(function() {
							if(confirm('Do you want to pass this message to your contacts ?')) {
								$.post(posturl, { stamp: stamp, action: "pass", token: TokenMail })
								.done(function(data) {
									$('#'+id+' .passlink').replaceWith('<span class="button"><img alt="Passed" src="/arrow_passed.png"></span>');
								});
							}
							return false;
						});
					})(id, mail.stamp);
				}
				else {
					$('#'+id+' .buttonsbar').append('<span class="button"><img alt="Passed" src="/arrow_passed.png"></span>');
				}
				*/
				
				$('#'+id+' .buttonsbar').append('<a href="#" class="button replylink"><img alt="Reply" src="/arrow_reply.png"></a>');
				(function(idReply) {
					$('#'+id+' .replylink').click(function() {
						$('#'+idReply).toggle();
						$('#'+idReply+' textarea').focus();
						return false;
					});
				})(idReply);
			}
			else {
				$('#'+idParentReply).before(div);	// insert before parent reply
				$('#'+id).addClass('childmessage');
			}
			
			user = getAuthenticatedUser();
			if(user && 'identifier' in mail) {
				(function(id, identifier) {
					var url = '/user/'+user+'/contacts/';
					$.ajax({
						url: url+'?id='+identifier+'&json',
						dataType: 'json',
						timeout: 10000,
						error: function (xhr) {
							if(xhr.status == 404 && typeof TokenContact != 'undefined') {
								$('#'+id+' .author').append('&nbsp;<form name="addform'+id+'" action="'+url+'" method="post" enctype="application/x-www-form-urlencoded"><input type="hidden" name="token" value="'+TokenContact+'"><input type="hidden" name="action" value="add"><input type="hidden" name="id" value="'+identifier+'"><input type="hidden" name="name" value="'+$('#'+id+' .author').text()+'"></form><a href="#" class="addlink"><img src="/add.png" alt="+"></a>');
								$('#'+id+' .author .addlink').click(function() {
									$('#'+id+' .author form').submit();
									return false;
								});
							}
						}
					})
					.done(function(contact) {
						$('#'+id+' .author').html('<a href="'+contact.prefix+'/">'+contact.name+'</a>');
					});
				})(id, mail.identifier);
			}
			
			// Reply form
			$('#'+id).parent().append('<div id='+idReply+' class="reply"><div class="replypanel"><form name="replyform'+id+'" action="'+posturl+'" method="post" enctype="application/x-www-form-urlencoded"><textarea class="replyinput" name="message"></textarea><input type="hidden" name="attachment"><input type="hidden" name="attachmentname"><input type="hidden" name="parent" value="'+mail.digest+'"><input type="hidden" name="public" value="1"></form></div><div class="attachedfile"></div><div class="fileselector"></div></div>');
			
			$('#'+idReply).hide();
			$('#'+idReply+' .attachedfile').hide();
			$('#'+idReply+' form').ajaxForm(function() {
				$('#'+idReply+' form').resetForm();
				$('#'+idReply+' .attachedfile').html('');
				$('#'+idReply+' .fileselector').html('');
				$('#'+idReply).hide();
			});
			
			if(typeof UrlSelector != 'undefined' && typeof TokenDirectory != 'undefined') {
				$('#'+idReply+' .replypanel').prepend('<a class="button" href="#"><img alt="File" src="/paperclip.png"></a>');
				
				(function(idReply) {
					$('#'+idReply+' .button').click(function() {
						createFileSelector(UrlSelector, '#'+idReply+' .fileselector', '#'+idReply+' input[name="attachment"]', '#'+idReply+' input[name="attachmentname"]', TokenDirectory); 
						return false;
					});
					$('#'+idReply+' input[name="attachment"]').change(function() {
						$('#'+idReply+' .attachedfile').html('').hide();
						var filename = $('#'+idReply+' input[name="attachmentname"]').val();
						if(filename != '') {
							$('#'+idReply+' .attachedfile')
								.append('<img class=\"icon\" src=\"/file.png\">')
								.append('<span class=\"filename\">'+filename+'</span>')
								.show();
						}
						var input = $('#'+idReply+' .replyinput');
						input.focus();
						if(input.val() == '') {
							input.val(filename).select();
						}
					});
				})(idReply);
			}
			
			$('#'+id+' .content').html(mail.content.escape().smileys().linkify().split("\n").join("<br>"));
			
			if(mail.attachments && mail.attachments[0]) {
				
				$('#'+id+' .header').after('<span class="attachment"></span>');
				$('#'+id+' .attachment').html('<img class="icon" src="/smallpaperclip.png">Loading attachment...');
				
				var url = '/file/'+mail.attachments[0];	// TODO
				
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
					
						var content = '';
						if(media == 'image') {
							content = '<a href="'+url+'" target="_blank"><img class="preview" src="'+url+'" alt="'+name.escape()+'"></a><img class="clip" src="/clip.png">';
						}
						else if(media == 'audio' || media == 'video') {
							var usePlaylist = (deviceAgent.indexOf('android') < 0);
							content = '<span class="filename"><a href="'+url+'?download=1"><img class="icon" src="/down.png"></a><a href="'+url+(usePlaylist ? '?play=1' : '')+'"><img class="icon" src="/file.png">'+name.escape()+'</a></span><img class="clip" src="/clip.png">';
						}
						else {
							content = '<span class="filename"><a href="'+url+'" target="_blank"><img class="icon" src="/file.png">'+name.escape()+'</a></span><img class="clip" src="/clip.png">';
						}
						
						transition('#'+id+' .attachment', '<span class="attached">'+content+'</a>');
						
						setTimeout(function() { 
							$(object).scrollTop($(object)[0].scrollHeight);
						}, 10);
					})
					.fail(function(jqXHR, textStatus) {
						$('#'+id+' .attachment').html('<img class="icon" src="/paperclip.png">Attachment not available');
					});
				
				})(id, url);
			}
			
			$('#'+id).append('<span class="footer"></span>');
			
			count++;
		});
		
		// Order messages
		$(object).html($(object).children().detach().sort(function(a,b){
			return $(a).find(".time").text() < $(b).find(".time").text();
		}));
		
		if(count > 0 && isPageHidden()) 
		{
			NbNewMessages+= count;
			top.document.title = '(' + NbNewMessages + ') ' + BaseTitle;
			playMailSound();
		}

		// Hide replies if they're too many
		/*
		$('.conversation').each(function() {
			if($(this).find('.morereplies').length) return;
			
			var nReplies = $(this).find('.childmessage').length;
			var nHiddenReplies = nReplies-5;
			var object = this;
			var showMoreReplies = false;
			
			if(nReplies >= 7) // We hide at least 2 replies
			{
				$(object).find('.childmessage').eq(0).before('<div class="morereplies">Show first '+nHiddenReplies+' replies</div>');

				for(i = 0; i < nReplies-5; i++){
					$(object).find('.childmessage').eq(i).toggle();
				}

				function clickMoreReplies(){

					for(i = 0; i < nReplies-5; i++){
						$(object).find('.childmessage').eq(i).toggle();
					}

					if(!showMoreReplies)
					{
						$(object).find('.morereplies').replaceWith('<div class="morereplies">Hide first '+nHiddenReplies+' replies</div>');
						showMoreReplies = true;
					}
					else
					{
						$(object).find('.morereplies').replaceWith('<div class="morereplies">Show first '+nHiddenReplies+' replies</div>');
						showMoreReplies = false;
					}
					$(object).find('.morereplies').click(clickMoreReplies);
				}

				$(object).find('.morereplies').click(clickMoreReplies);
			}
		});
		*/

		this.messagesTimeout = setTimeout(function() {
			setMailReceiverRec(url, object, period, next);
		}, period);

	})
	.fail(function(jqXHR, textStatus) {
		this.messagesTimeout = setTimeout(function() {
			setMailReceiverRec(url, object, period, next);
		}, period);
	});
}

function setMailReceiver(url, object, period, next) {
	
	setMailReceiverRec(url, object, period, 0);
}
