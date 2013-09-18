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

	$(object).html('Loading...');
	
	$.ajax({
		url: url,
		dataType: 'json',
		timeout: 30000
	})
	.done(function(data) {
		if(data && data.length > 0) {
			$(object).html('<table class="files"></table>');
                	var table = $(object).find('table');

			for(var i=0; i<data.length; i++) {
				var resource = data[i];
				if(!resource.url) continue;
				var link = getResourceLink(resource);

				var line = '<tr>';
				line+= '<td><a href="'+link.escape()+'">'+resource.name.escape()+'</a></td>';
				line+= '</tr>';
				table.append(line);
			}
			
			table.find('tr').css('cursor', 'pointer').click(function() {
				window.location.href = $(this).find('a').attr('href');
			});
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

	$(object).html('Loading...');
	
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

