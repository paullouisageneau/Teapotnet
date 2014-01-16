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

var currentObject;
$('#howtotext').hide();

// Load question mark images 
$('.invitationimg').append('<img src="/questionmark.png" alt="?" class="questionmark">'); 
$('.questionmark').click(function() { 
	// Click on same question mark will toggle, else will not change visibility of howtotext 
	if($(this).is(currentObject) || !$('#howtotext').is(':visible')) 
		$('#howtotext').slideToggle('fast'); 
	currentObject = $(this); 
	var text = howtoText($(this).prev()); 
	$('#howtotext').html(text); 
});

function howtoText(object) { 
	if(object.is($('#spyimg'))) 
		return "Classic and most discreet way. Exchange a plain text secret with your friend by email, phone, IRL... and input it here with his TeapotNet id."; 
	if(object.is($('#mailimg'))) 
		return "Input your and your friend's email address. An invitation containing a code will be sent to your friend, and then back to you."; 
	if(object.is($('.gmailimg'))) 
		return "Select from your GMail contacts the people you want to send an invitation containg a code to reach you to."; 
	if(object.is($('#fbimg'))) 
		return "Coming soon."; 
	return "default"; 
}

var minLengthSecret = 10;

// Forms are hidden at page load 
$('#sendrequest').hide(); 
$('#newcontactdiv').hide();
var showOneForm = false;

$('#spyimg').click(function() { 
	if(showOneForm) $('#sendrequest').hide(); 
	$('#newcontactdiv').toggle(); 
	showOneForm = true; 
}); 
$('#mailimg').click(function() { 
	if(showOneForm) $('#newcontactdiv').hide(); 
	$('#sendrequest').toggle(); 
	showOneForm = true; 
}); 
$('#gmailimg').click(function() { 
	$('form[name=gmailform]').submit(); 
}); 

$('#fbimg').click(function() { 
	alert('Coming soon !');
}); 

function isValidMail(object) {
	// TODO : is not correct, rejects mails with a '+' in it
	var str = getInputText(object); 
	var mailRegex = new RegExp('^[a-z0-9]+([_|\\.|-]{1}[a-z0-9]+)*@[a-z0-9]+([_|\\.|-]{1}[a-z0-9]+)*[\\.]{1}[a-z]{2,6}$', 'i'); 
	return mailRegex.test(str); 
}

function isValidSecret(object) { 
	var str = getInputText(object); 
	if(str.length >= minLengthSecret) 
		return true; 
	return false; 
}

function isValidUrl(object) {
	var returnBool = false; 
	var str = getInputText(object); 
	var strlen = str.length;
	returnBool |= ( (str.substring(0,lengthUrl) == centralizedFriendSystemUrl && (str.substring(strlen-16-idLength,strlen-idLength) == '.php?id_request=')) ? true : false); 
	returnBool |= ( ((str.substring(0,prefixLength) == FIRST_REQUEST_PREFIX || str.substring(0,prefixLength) == REQUEST_ACCEPTED_PREFIX) && (strlen == idLength+prefixLength)) ? true : false ); 
	//$('#acceptrequest h2').html((str.substring(strlen-idLength-16,strlen-idLength))); // Just for debug 
	return returnBool; 
}

function getInputText(object) { 
	return object.val(); 
}

function checkInput(object, isValid) {
	object.each(function() {
		if(!$(this).val()) {
			$(this).removeClass('isvalid'); 
			$(this).removeClass('isinvalid'); 
		} 
		else if(isValid($(this))) {
			$(this).removeClass('isinvalid');
			$(this).addClass('isvalid');
		}
		else {
			$(this).removeClass('isvalid');
			$(this).addClass('isinvalid');
		}
	});
}

function setInputChecks(object, isValid) {
	object.each(function()   { checkInput($(this), isValid); });
	object.change(function() { checkInput($(this), isValid); });
	object.keyup(function()  { checkInput($(this), isValid); });
	object.on('paste', function () {
		var element = this;
		setTimeout(function () {
			checkInput($(element), isValid);
		}, 100);
	});
}

$(document).ready(function() {
	setInputChecks($('input.name'), isValidMail);
	setInputChecks($('input.secret'), isValidSecret);
});

// Form checking
$('input.add').click(function(e) {
	var nameInput = $(this).parents('form').find('input.name');
	var secretInput = $(this).parents('form').find('input.secret');
	var validName = isValidMail(nameInput);	// We check validity of tpn name with isValidName because structure of regex is exactly the same
	var validSecret = isValidSecret(secretInput);
	
	if(!validName || !validSecret)
	{
		e.preventDefault();
		if(!validName)	alert('The TeapotNet ID should look like this : name@tracker.tld'); 
		else alert('The secret should be at least '+minLengthSecret+' characters long.'); 
	} 
});

/**** CONSTANTS FOR PHP ECHOES ****/
FIRST_REQUEST_PREFIX = '1005';
REQUEST_ACCEPTED_PREFIX = '1921'; 
REQUEST_ALREADY_EXISTS = 10; 
EMPTY_REQUEST = 11; 
REQUEST_ALREADY_ACCEPTED = 12; 
SPECIFY_MAIL = 13; 
REQUEST_TOO_OLD = 14; 
INVALID_ADDRESS = 15; 
SUCCESS = 16; 
FAILURE = 17;
/*********************************/

// Globals for mail form 
var mailCookie = 'mailCookie';
var mailSenderObject; 
var mailReceiverObject;

$(document).ready(function() {
	mailSenderObject = $('#sendrequest .mail_sender'); 
	mailReceiverObject = $('#sendrequest .mail_receiver');
	
	// Pre-fill mail by cookie and use profile-defined mail if not set by cookie : 
	if(checkCookie(mailCookie)) mailSenderObject.val(getCookie(mailCookie));
	else mailSenderObject.val(mailProfile);
	
	setInputChecks(mailSenderObject, isValidMail);
	setInputChecks(mailReceiverObject, isValidMail);
});

$('#sendrequest .sendinvitation').click(function(e) { 
	e.preventDefault();
	if(isValidMail(mailSenderObject) && isValidMail(mailReceiverObject)) {
		var postUrl = postUrl1;
		var mailReceiver = getInputText(mailReceiverObject);
		var mailSender = getInputText(mailSenderObject); 
		var nameSender = nameProfile; 
		setCookie(mailCookie, mailSender, 365);
		$.post(postUrl, { mail_receiver : mailReceiver, mail_sender : mailSender, name_sender : nameSender, tpn_id_sender: tpn_id, rapture_method: '1' }) 
		.done(function(data) {
			$(mailReceiverObject).val('');
			$(mailReceiverObject).change();
			
			// expects echo from php
			if(data == SUCCESS)
				alert('An invitation was successfully sent to '+mailReceiver); 
			else if(data==FAILURE) 
				alert('Failure when sending email'); 
			else if(data==REQUEST_ALREADY_EXISTS) 
				alert('A request from '+mailSender+' to '+mailReceiver+' already exists.'); 
			else if(data==INVALID_ADDRESS) 
				alert('Invalid address');
			else if(data != '') // Silent case (mode REQUEST_ACCEPTED) 
				alert('An invitation was successfully sent to '+mailReceiver); 
			else 
				alert('Unknown error'); 
		})
		.fail(function() { alert('Error in send function'); });
	} 
	else { 
		alert('Incorrect email address'); 
	} 
});

var lengthUrl = centralizedFriendSystemUrl.length; 
var prefixLength = FIRST_REQUEST_PREFIX.length; 
var idLength = 32; 
var currentIdRequest = ''; 

function getIdRequest(str) { 
	var idrequest = ''; 
	var strlen = str.length; 
	// Assumes str is valid text input 
	idrequest = str.substring(strlen-idLength,strlen); 
	return idrequest; 
}

function getMethod(str) { 
	var method = '0'; 
	var strlen = str.length; 
	if((str.substring(0,prefixLength) == FIRST_REQUEST_PREFIX || str.substring(0,prefixLength) == REQUEST_ACCEPTED_PREFIX) && (strlen == idLength+prefixLength)) 
		return str.substring(0,prefixLength); 
	if(str.substring(0,lengthUrl) == centralizedFriendSystemUrl && (str.substring(strlen-16-idLength,strlen-idLength) == '.php?id_request=')) { 
		if(str.substring(lengthUrl, strlen-16-idLength)=='secretgen') 
			method = FIRST_REQUEST_PREFIX; 
		else if(str.substring(lengthUrl, strlen-16-idLength)=='laststep') 
			method = REQUEST_ACCEPTED_PREFIX; 
		else 
			method = '-1'; 
	} 
	return method; 
}

$(document).ready(function() {
	setInputChecks($('#acceptrequest .posturl'), isValidUrl);
	$('#acceptrequest .postfriendrequest').click(postAcceptRequest);
	$('#acceptrequest .posturl').keypress(function(e) {
		if (e.keyCode == 13 && !e.shiftKey) {
			e.preventDefault();
			postAcceptRequest();
			return;
		}
	});
});

function postAcceptRequest() { 
	var postUrl = centralizedFriendSystemUrl; 
	var urlObject = $('#acceptrequest .posturl'); 
	var nStep = 0; 
	if(isValidUrl(urlObject))
	{ 
		var str = getInputText(urlObject);
		if(getMethod(str) == FIRST_REQUEST_PREFIX) { 
			nStep = 2; 
			postUrl += 'secretgen.php?id_request='; 
		} 
		else if (getMethod(str) == REQUEST_ACCEPTED_PREFIX) { 
			nStep = 3; 
			postUrl += 'laststep.php?id_request='; 
		} 
		else { 
			alert('Impossible to proceed'); 
			return; 
		} 
		var idrequest = getIdRequest(str); 
		postUrl += idrequest; 
		currentIdRequest = idrequest; 
		$.post(postUrl, {tpn_id: tpn_id}) 
		.done(function(data) {
			if(data == EMPTY_REQUEST) 
				alert('Error: request not found'); 
			else if(data == REQUEST_ALREADY_ACCEPTED) 
				alert('This request has already been accepted.'); 
			else if(isJsonString(data)) { 
				var arr = $.parseJSON(data); 
				var secret = arr['secret'];
				var tpnid = arr['tpn_id']; 
				addContact(tpnid, secret, nStep); 
			} 
			else	alert('Error: request not found'); 
		}) 
		.fail(function() {
			alert('Error, please check your internet connectivity and try again.');
		});
	} 
	else { 
		alert('Invalid request'); 
	}
}

function addContact(tpnid, secret, nStep) { 
	$.post(prefix+"/", {name: tpnid, secret: secret, token: token})
	.done(function(data) {
		alert('Contact added to list: ' + tpnid); 
		// Refresh contacts list
		location.reload(); 
	}) 
	.fail(function() { 
		alert('Error: the contact could not be added');
	});
}
