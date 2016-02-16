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

function listDirectory(url, object, showButtons) {

	$(object).empty();
	
	if(showButtons) {
		//var location = url.split('?')[0];
		//var parentLink = (location[location.length-1] == '/' ? '..' : '.');
		//$(object)
		//	.append('<span class="button"> '+data.length+' files</span>')
		//	.append('<a href="'+parentLink+'" class="button parentlink"><img src="/arrow_up.png" alt="Parent"></a>')
		//	.append('<a href="#" class="button refreshlink"><img src="/arrow_refresh.png" alt="Refresh"></a>');
		//	.find('a.refreshlink').click(function() {
		//		listDirectory(url, object, showButtons);
		//		return false;
		//	});
	}
	
	$(object).append('<div class="gifloading"><img src="/loading.gif" alt="Loading..."></div>');
	
	listDirectoryRec(url, object, 0);
}

function listDirectoryRec(url, object, next) {
	$.ajax({
		url: url.appendParam("next", next),
		dataType: 'json',
		timeout: 300000
	})
	.done(function(data) {
		$(object).find('.gifloading').remove();
		$(object).find('div.files').remove();
		
		if(data && data.length > 0) {
			var table = $(object).find('table.files');
			if(table.length == 0) {
				$(object).append('<table class="files"></table>');
				table = $(object).find('table.files');
			}
			
			var referenceUrl = '';
			if(window.location.href.indexOf("/files") >= 0) {
				referenceUrl = window.location.href.split(/[?#]/)[0];
				if(referenceUrl[referenceUrl.length-1] != '/') referenceUrl+= '/';
			}
			
			for(var i=0; i<data.length; i++) {
				++next;
				var resource = data[i];
				
				var existing = table.find("td.filename:contains('"+resource.name.escape()+"')").parent();
				if(existing.length > 0) {
					var time = parseInt(existing.find('td.time').text());
					if(time > 0 && resource.time > 0) {
						if(time > resource.time) continue;
						existing.hide();
					}
				}
				
				var link = "/file/" + resource.digest;
				var line;
				if(resource.type == "directory") {
					if(referenceUrl) link = referenceUrl + resource.name + "/?digest=" + resource.digest;	// use url as link if possible
					line = '<tr class="directory">';
					line+= '<td class="icon"><img src="/dir.png" alt="(directory)"></td>';
					line+= '<td class="filename"><a href="'+link.escape()+'">'+resource.name.escape()+'</a></td>';
					line+= '<td class="size"></td>';
					line+= '<td class="date">'+('time' in resource ? formatTime(resource.time).escape() : '')+'</td>';
					line+= '<td class="time" style="display:none">'+('time' in resource ? resource.time : 0)+'</td>';
					line+= '<td class="actions"></td>';
					line+= '</tr>';
				}
				else {
					var pos = resource.name.escape().lastIndexOf('.');
					var extension = (pos > 0 ? resource.name.escape().substring(pos+1,resource.name.escape().length) : '');
					var isPlayable = (resource.type != "directory") && isPlayableResource(resource.name);
					
					line = '<tr class="file">';
					line+= '<td class="icon"><img src="/file.png" alt="(file)"></td>';
					line+= '<td class="filename"><span class="type">'+extension.toUpperCase()+' </span><a href="'+link.escape()+(isPlayable && deviceAgent.indexOf('android') < 0 ? '?play=1' : '')+'">'+resource.name.escape()+'</a></td>'; 
					line+= '<td class="size">'+formatBytes(resource.size, 2)+'</td>';
					line+= '<td class="date">'+('time' in resource ? formatTime(resource.time).escape() : '')+'</td>';
					line+= '<td class="time" style="display:none">'+('time' in resource ? resource.time : 0)+'</td>';
					line+= '<td class="actions"><a class="downloadlink" href="'+link.escape()+'?download=1"><img src="/down.png" alt="(download)"></a>';
					if(isPlayable) line+= '<a class="playlink" href="'+link.escape()+'?play=1"><img src="/play.png" alt="(play)"></a>';
					line+= '</td>';
					line+= '</tr>';
				}
				table.append(line);
				
				if(resource.name.length > 0 && (resource.name[0] == '_' || resource.name[0] == '.'))
					table.find('tr:last').hide();
			}
	
			table.find('tr').css('cursor', 'pointer').click(function() {
				window.location.href = $(this).find('a').attr('href');
			});
			
			// Order files
			table.html(table.find('tr').detach().sort(function(a,b){
				if($(a).hasClass("directory") && !$(b).hasClass("directory")) return false;
				if($(b).hasClass("directory") && !$(a).hasClass("directory")) return true;  
				return $(a).find(".filename a").text() > $(b).find(".filename a").text()
					|| ($(a).find(".filename a").text() == $(b).find(".filename a").text() && parseInt($(a).find(".time").text()) < parseInt($(b).find(".time").text()));
			}));
		
			listDirectoryRec(url, object, next);
		}
		else {
			if($(object).find('table.files tr:visible').length == 0) {
				$(object).append('<div class="files">No files</div>');
			}
		}
	})
	.fail(function(jqXHR, textStatus) {
		$(object).find('.gifloading').remove();
		
		if($(object).find('table.files tr:visible').length == 0) {
			$(object).append('<div class="files">Unable to access files</div>');
		}
	});
}

function listFileSelector(url, object, input, inputName, parents) {
	$(object).html('<h2>Select a file</h2>');
		
	if(parents.length > 0) {
		$(object)
			.append('<a href="#" class="button parentlink"><img src="/arrow_up.png" alt="Parent"></a>')
			.find('a.parentlink').click(function() {
				var parentUrl = parents.pop();
				listFileSelector(parentUrl, object, input, inputName, parents);
				return false;
			});
	}
		
	$(object)
		.append('<a href="#" class="button refreshlink"><img src="/arrow_refresh.png" alt="Refresh"></a>')
		.find('a.refreshlink').click(function() {
			listFileSelector(url, object, input, inputName, parents);
			return false;
		});
		
	if(UrlUpload)
	{
		$(object).append('<form class="uploadform" action="'+UrlUpload+'" method="post" enctype="mutipart/form-data"><input type="hidden" name="token" value="'+TokenDirectory+'"><input type="file" name="selector_file" size="30"></form>');
		$(object).find('form.uploadform input[name="selector_file"]')
			.css('visibility', 'hidden').css('display', 'inline').css('width', '0px').css('margin', '0px').css('padding', '0px')
			.after('<a class="button" href="#" onclick="$(\'form.uploadform input[name="selector_file"]\').click(); return false;">Other file</a>')
			.change(function() {
				$(object).children().hide();
				$(object).append('<span>Please wait...</span>');
				
				$(object).find('form.uploadform').ajaxSubmit({
					timeout: 600000,
					dataType: 'json',
					error: function() { 
						alert('Unable to send the file.'); 
						$(inputName).val("").change();
						$(input).val("").change();
						$(object).remove();
					},
					success: function(resources) {
						if(resources != null && resources.length > 0)
						{
							var resource = resources[0];
							$(inputName).val(resource.name).change();
							$(input).val(resource.digest).change();
							$(object).remove();
						}
					}
				});
			});
	}

	$(object)
		.append('<a href="#" class="button quitlink">Cancel</a>')
		.find('a.quitlink').click(function() {
			$(inputName).val("").change();
			$(input).val("").change();
			$(object).remove();
			return false;
		});
	
	$(object).append('<br>');
	$(object).append('<span class="gifloading"><img src="/loading.gif" alt="Loading..."></span>');
	
	listFileSelectorRec(url, object, input, inputName, parents, 0);
}

function listFileSelectorRec(url, object, input, inputName, parents, next) {
	
	var lock_url = $(object).find('.lock_url');
	if(lock_url.length == 0) {
		$(object).append('<span class="lock_url" style="display:none"></span>');
		lock_url = $(object).find('.lock_url');
	}
	lock_url.text(url);
	
	var xhr = $.ajax({
		url: url.appendParam("next", next),
		dataType: 'json',
		timeout: 300000
	})
	.done(function(data) {
		var lock_url = $(object).find('.lock_url');
		if(lock_url.text() != url)
			return;	// request is not valid anymore
		
		$(object).find('.gifloading').remove();
		$(object).find('div.files').remove();
		
		if(data && data.length > 0) {
			var table = $(object).find('table');
			if(table.length == 0) {
				$(object).append('<div class="fileselectorwindow"><table class="files"></table></div>');
				table = $(object).find('table.files');
			}
		
			var referenceUrl = '';
			if(url.indexOf("/files") >= 0) {
				referenceUrl = url.split('?')[0];
				if(referenceUrl[referenceUrl.length-1] != '/') referenceUrl+= '/';
			}
		
			for(var i=0; i<data.length; i++) {
				++next;
				var resource = data[i];
			
				if(resource.name.length > 0 && (resource.name[0] == '_' || resource.name[0] == '.'))
					if(resource.name == "_upload") resource.name = "Sent files";
					else return;

				var existing = table.find("td.filename:contains('"+resource.name.escape()+"')").parent();
				if(existing.length > 0) {
					var time = parseInt(existing.find('td.time').text());
					if(time > 0 && resource.time > 0) {
						if(time > resource.time) continue;
						existing.hide();
					}
				}

				var line = '<tr>';
				var func;
				(function(resource) { // copy resource (only the reference is passed to callbacks)
					if(resource.type == "directory") {
						line+= '<td class="icon"><img src="/dir.png" alt="(directory)"></td>';
						line+= '<td class="filename"><a href="#">'+resource.name.escape()+'</a></td>';
						line+= '<td class="time" style="display:none">'+('time' in resource ? resource.time : 0)+'</td>';	

						func = function() {
							xhr.abort();
							var link;
							if(referenceUrl) link = referenceUrl + resource.name + "/?digest=" + resource.digest + "&json";
							else link = "/file/" + resource.digest + "?json";
							parents.push(url);
							listFileSelector(link, object, input, inputName, parents);
							return false;
						};
					}
					else {
						line+= '<td class="icon"><img src="/file.png" alt="(file)"></td>';
						line+= '<td class="filename"><a href="#">'+resource.name.escape()+'</a></td>';
						line+= '<td class="time" style="display:none">'+('time' in resource ? resource.time : 0)+'</td>';
		
						func = function() {
							xhr.abort();
							$(inputName).val(resource.name).change();
							$(input).val(resource.digest).change();
							$(object).remove();
							return false;
						};
					}
				})(resource);
				line+= '</tr>';
				table.append(line);

				table.find('tr:last').click(func).css('cursor', 'pointer');
				table.find('tr:last a').click(func);
			}
		
			// Order files
			table.html(table.find('tr').detach().sort(function(a,b){
				if($(a).hasClass("directory") && !$(b).hasClass("directory")) return false;
				if($(b).hasClass("directory") && !$(a).hasClass("directory")) return true;
				return $(a).find(".filename a").text() > $(b).find(".filename a").text()
					|| ($(a).find(".filename a").text() == $(b).find(".filename a").text() && parseInt($(a).find(".time").text()) < parseInt($(b).find(".time").text()));
				}));
			
			listFileSelectorRec(url, object, input, inputName, parents, next);
		}
		else {
			if($(object).find('table.files tr:visible').length == 0) {
				$(object).append('<div class="files">No files</div>');
			}
		}
	})
	.fail(function(jqXHR, textStatus) {
		var lock_url = $(object).find('.lock_url');
		if(lock_url.text() != url)
			return;	// request is not valid anymore
		
		$(object).find('.gifloading').remove();
		
		if($(object).find('table.files tr:visible').length == 0) {
			$(object).append('<div class="files">Unable to access files</div>');
		}
	});
}

function createFileSelector(url, object, input, inputName) 
{
	if($(object).html()) {
		$(object).html("");
		$(inputName).val("").change();
		$(input).val("").change();
		return;
	}
	
	$(object).show();
	$(object).html('<div class="box"></div>');
	var div = $(object).find('div');
	listFileSelector(url, div, input, inputName, []);	
}

function isPlayableResource(fileName)
{
	return (isAudio(fileName) || isVideo(fileName));
}

function isAudio(fileName)
{
	var regexp = new RegExp("(mp3|ogg|ogga|flac|wav|ape|aac|m4a|mp4a|wma)$","i");
	return regexp.test(fileName);
}

function isVideo(fileName)
{
	var regexp = new RegExp("(avi|mkv|ogv|ogx|ogm|wmv|asf|flv|mpg|mpeg|mp4|m4v|mov|3gp|3g2|divx|xvid|rm|rv)$","i");
	return regexp.test(fileName);
}

