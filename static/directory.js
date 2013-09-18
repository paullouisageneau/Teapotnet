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

function getResourceLink(resource) {
	
	var basePath = getBasePath();
	return (resource.hash ? '/' + resource.hash.escape() : 
		basePath + (resource.contact && basePath != '/'+resource.contact+'/' ? 'contacts/' + resource.contact.escape() : 'myself') 
		+ '/files' + (resource.url[0] != '/' ? '/' : '') + resource.url.escape()
		+ (resource.type == "directory" ? '/' : ''));
}

function listDirectory(url, object) {

	$(object).html('<span class="gifloading"><img src="/loading.gif" alt="Loading..."></span>');
	
	$.ajax({
		url: url,
		dataType: 'json',
		timeout: 30000
	})
	.done(function(data) {
		if(data && data.length > 0) {

			urlFile = url.substring(0,url.length-6);
			var parentButton, parentButtonLink, isPlayableDirectory;
			(url.charAt(url.length-6) == '/') ? parentButtonLink = '..' : parentButtonLink = '.'; // url.length-6 because url finishes by "?json"

			parentButton = '<span class="button"> '+data.length+' files</span> <a href="'+parentButtonLink+'" class="button"> <img src="/arrow_up.png" alt="Parent"> </a>';

			$(object).html(parentButton);
			$(object).append('<table class="files"></table>');
                	var table = $(object).find('table');

			for(var i=0; i<data.length; i++) {
				var resource = data[i];
				if(!resource.url) continue;
				var link = getResourceLink(resource);

				var line = '<tr>';

				if(resource.type == "file") 
				{
					var isPlayable;
					var extension = resource.name.escape().substring(resource.name.escape().lastIndexOf('.')+1,resource.name.escape().length);
					isPlayable = isPlayableResource(resource.name.escape());
					isPlayableDirectory |= isPlayable;

					line+= '<td class="icon"> <img src="/file.png" alt="(file)"> </td>';
					//line+= ''
					isPlayable ? line+= '<td class="filename"> <span class="type">'+extension+'  </span><a href="'+link.escape()+'?play=1">'+resource.name.escape()+'</a></td>' : line+= '<td class="filename"><span class="type">'+extension+'  </span><a href="'+link.escape()+'">'+resource.name.escape()+'</a></td>' ;
					line+= '<td class="actions"> <a href="'+link.escape()+"?download=1"+'"> <img src="/down.png" alt="(download)"> </a> </td>';
					if(isPlayable)
					{
						line+= '<td class="actions"> <a href="'+link.escape()+"?play=1"+'"> <img src="/play.png" alt="(play)"> </a> </td>';
					}
				}
				else
				{
					line += '<tr class="dir">';
					line+= '<td class="icon"> <img src="/dir.png" alt="(directory)"> </td>';
					line+= '<td><a href="'+link.escape()+'">'+resource.name.escape()+'</a></td>';
				}
				line+= '</tr>';
				table.append(line);

			}
	
			table.find('tr').css('cursor', 'pointer').click(function() {
				window.location.href = $(this).find('a').attr('href');
			});

			//if(isPlayableDirectory) $(object).prepend('<a href="'+urlFile+'?playlist=1" class="button"> Play this directory </a>'); // TODO
		}
		else {
			$(object).html('No files');
		}
	})
	.fail(function(jqXHR, textStatus) {
		$(object).html('Failed');
	});
}

function listFileSelector(url, object, input, inputName, parents = []) {

	$(object).html('<span class="gifloading"><img src="/loading.gif" alt="Loading..."></span>');
	
	$.ajax({
		url: url,
		dataType: 'json',
		timeout: 30000
	})
	.done(function(data) {
		$(object).html('<table class="files"></table>');
		var table = $(object).find('table');
		
		if(parents.length > 0) {
			var line = '<tr>';
			line+= '<td>[<a href="#">parent</a>]</td>';
			line+= '</tr>';
			table.append(line);
		
			var func = function() {
				var parentUrl = parents.pop();
				listFileSelector(parentUrl, object, input, inputName, parents);
			}

			table.find('tr:last').click(func).css('cursor', 'pointer');
			table.find('tr:last a').click(func);
		}
		
		if(data) {
			for(var i=0; i<data.length; i++) {
				var resource = data[i];
				if(!resource.url) continue;

				var line = '<tr>';
				line+= '<td><a href="#">'+resource.name.escape()+'</a></td>';
				line+= '</tr>';
				table.append(line);
				
				var func;
				
				(function(resource) { // copy resource (only the reference is passed to callbacks)
					if(resource.type == "directory") {
						func = function() {
							var link = getResourceLink(resource) + "?json";
							parents.push(url);
							listFileSelector(link, object, input, inputName, parents);
						};
					}
					else {
						func = function() {
							$(inputName).val(resource.name).change();
							$(input).val(resource.hash).change();
							$(object).remove();
						};
					}
				})(resource);
				
				table.find('tr:last').click(func).css('cursor', 'pointer');
				table.find('tr:last a').click(func);
			}
		}
		else {
			$(object).append('No files');
		}
	})
	.fail(function(jqXHR, textStatus) {
		$(object).html('Failed');
	});
}

function createFileSelector(url, object, input, inputName) 
{
	if($(object).html()) {
		$(object).html("");
		return;
	}

	$(object).show();
	$(object).html('<div class="box"></div>');
	var div = $(object).find('div');
	listFileSelector(url, div, input, inputName);	
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

