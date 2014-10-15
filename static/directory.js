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

// TODO: remove, deprecated
function getResourceLink(resource, privateMode) {
	
	if(resource.digest) 
		return '/' + resource.digest.escape();
	
	var url = window.location.href;
	var basePath = getBasePath(1);
	var fromSelf = (resource.contact && basePath == '/'+resource.contact+'/');
	
	var subPath = 'browse';
	if(privateMode) {
		if(resource.hops == 0 || fromSelf) subPath = 'myself/files';
		else if(resource.hops == 1 && resource.contact) subPath = 'contacts/' + resource.contact.escape() + '/files';
	}
	
	return basePath + subPath
		+ (resource.url[0] != '/' ? '/' : '') + resource.url.escape()
		+ (resource.type == "directory" ? '/' : '');
}

function listDirectory(url, object, showButtons, privateMode) {

	$(object).html('<span class="gifloading"><img src="/loading.gif" alt="Loading..."></span>');
	
	// Loop ajax call until 404
	
	$.ajax({
		url: url,
		dataType: 'json',
		timeout: 300000
	})
	.done(function(data) {
		$(object).html('');
		
		if(showButtons) {
			var location = url.split('?')[0];
			var parentLink = (location[location.length-1] == '/' ? '..' : '.');
			$(object)
				.append('<span class="button"> '+data.length+' files</span>')
				.append('<a href="'+parentLink+'" class="button parentlink"><img src="/arrow_up.png" alt="Parent"></a>')
				.append('<a href="#" class="button refreshlink"><img src="/arrow_refresh.png" alt="Refresh"></a>');
				
			$(object).find('a.refreshlink').click(function() {
				listDirectory(url, object, showButtons, privateMode);
				return false;
			});
		}
		
		if(data && data.length > 0) {

			$(object).append('<table class="files"></table>');
                	var table = $(object).find('table');

			for(var i=0; i<data.length; i++) {
				var resource = data[i];
				
				var link = '/file/' + resource.digest;
				var line = '<tr>';
				if(resource.type == "directory") {
					line+= '<td class="icon"><img src="/dir.png" alt="(directory)"></td>';
					line+= '<td class="filename"><a href="'+link.escape()+'">'+resource.name.escape()+'</a></td>';
					line+= '<td class="actions"></td>';
				}
				else {
					var pos = resource.name.escape().lastIndexOf('.');
					var extension = (pos > 0 ? resource.name.escape().substring(pos+1,resource.name.escape().length) : '');
					var isPlayable = (resource.type != "directory") && isPlayableResource(resource.name);
					
					line+= '<td class="icon"><img src="/file.png" alt="(file)"></td>';
					line+= '<td class="filename"><span class="type">'+extension.toUpperCase()+' </span><a href="'+link.escape()+(isPlayable && deviceAgent.indexOf('android') < 0 ? '?play=1' : '')+'">'+resource.name.escape()+'</a></td>'; 
					line+= '<td class="actions"><a class="downloadlink" href="'+link.escape()+'?download=1"><img src="/down.png" alt="(download)"></a>';
					if(isPlayable) line+= '<a class="playlink" href="'+link.escape()+'?play=1"><img src="/play.png" alt="(play)"></a>';
					line+= '</td>';
				}
				line+= '</tr>';
				table.append(line);
			}
	
			table.find('tr').css('cursor', 'pointer').click(function() {
				window.location.href = $(this).find('a').attr('href');
			});
			
		}
		else {
			$(object).append('<div class="files">No files</div>');
		}
	})
	.fail(function(jqXHR, textStatus) {
		
		$(object).html('');
		
		if(showButtons) {
			var location = url.split('?')[0];
			var parentLink = (location[location.length-1] == '/' ? '..' : '.');
			$(object)
				.append('<span class="button">0 files</span>')
				.append('<a href="'+parentLink+'" class="button parentlink"><img src="/arrow_up.png" alt="Parent"></a>')
				.append('<a href="#" class="button refreshlink">Retry</a>');
		}
		
		$(object).append('<div class="files">Unable to access files</div>');
		
		if(!showButtons) {
			$(object).append('<a href="#" class="button refreshlink">Retry</a>');
		}
		
		$(object).find('a.refreshlink').click(function() {
			listDirectory(url, object, showButtons, privateMode);
			return false;
		});
	});
}

function listFileSelector(url, object, input, inputName, directoryToken, parents) {

	$(object).html('<span class="gifloading"><img src="/loading.gif" alt="Loading..."></span>');
	
	$.ajax({
		url: url,
		dataType: 'json',
		timeout: 30000
	})
	.done(function(data) {
		if(!data) data = [];
	      
		$(object).html('<h2>Select a file</h2>')
		
		if(parents.length > 0) {
			$(object)
				.append('<span class="button"> '+data.length+' files</span>')
				.append('<a href="#" class="button parentlink"><img src="/arrow_up.png" alt="Parent"></a>')
				.find('a.parentlink').click(function() {
					var parentUrl = parents.pop();
					listFileSelector(parentUrl, object, input, inputName, directoryToken, parents);
					return false;
				});
		}
		
		$(object)
			.append('<a href="#" class="button refreshlink"><img src="/arrow_refresh.png" alt="Refresh"></a>')
			.find('a.refreshlink').click(function() {
				listFileSelector(url, object, input, inputName, directoryToken, parents);
				return false;
			});
			
		//$(object).append('<a class="button" href="'+uploadUrl+'">Choose another file</a>');
		
		if(directoryToken)
		{
			var uploadUrl = getBasePath(1) + 'files/_upload/?json';
			
			$(object).append('<form id="uploadform" action="'+uploadUrl+'" method="post" enctype="mutipart/form-data"><input type="hidden" name="token" value="'+directoryToken+'"><input type="file" id="selector_file" name="selector_file" size="30"></form>');
			$('#selector_file')
				.css('visibility', 'hidden').css('display', 'inline').css('width', '0px').css('margin', '0px').css('padding', '0px')
				.after('<a class="button" href="#" onclick="$(\'#selector_file\').click(); return false;">New file</a>')
				.change(function() {
					$(object).children().hide();
					$(object).append('<span>Please wait...</span>');
					
					$('#uploadform').ajaxSubmit({
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
		
		$(object).append('<br><div class="fileselectorwindow"><table class="files"></table></div>');
		var table = $(object).find('table');
		
		if(parents.length == 0) {
			data.unshift({
				url: "/_upload",
				name: "Recently sent files",
				type: "directory"
			});
		}
		
		for(var i=0; i<data.length; i++) {
			var resource = data[i];

			var line = '<tr>';
			var func;
			(function(resource) { // copy resource (only the reference is passed to callbacks)
				if(resource.type == "directory") {
					line+= '<td class="icon"><img src="/dir.png" alt="(directory)"></td>';
					line+= '<td class="filename"><a href="#">'+resource.name.escape()+'</a></td>';
	
					func = function() {
						var link = getResourceLink(resource) + "?json";
						parents.push(url);
						listFileSelector(link, object, input, inputName, directoryToken, parents);
						return false;
					};
				}
				else {
					line+= '<td class="icon"><img src="/file.png" alt="(file)"></td>';
					line+= '<td class="filename"><a href="#">'+resource.name.escape()+'</a></td>';
					
					func = function() {
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
	})
	.fail(function(jqXHR, textStatus) {
		$(object)
			.html('')
			
		if(parents.length > 0) {
			$(object)
				.append('<span class="button">0 files</span>')
				.append('<a href="#" class="button parentlink"><img src="/arrow_up.png" alt="Parent"></a>')
				.find('a.parentlink').click(function() {
					var parentUrl = parents.pop();
					listFileSelector(parentUrl, object, input, inputName, directoryToken, parents);
					return false;
				});
		}
			
		$(object)
			.append('<a href="#" class="button refreshlink">Retry</a>')
			.append('<a href="#" class="button quitlink">Cancel</a>')
			.append('<div class="files">Unable to access files</div>')
			.find('a.refreshlink').click(function() {
				listFileSelector(url, object, input, inputName, directoryToken, parents);
				return false;
			})
			.find('a.quitlink').click(function() {
				$(inputName).val("").change();
				$(input).val("").change();
				$(object).remove();
				return false;
			});
	});
}

function createFileSelector(url, object, input, inputName, directoryToken) 
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
	listFileSelector(url, div, input, inputName, directoryToken, []);	
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

