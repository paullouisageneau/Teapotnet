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
	if(object.is($('form[name=gmailform]'))) 
		return "Select from your GMail contacts the people you want to send an invitation containg a code to reach you to."; 
	if(object.is($('#fbimg'))) 
		return "Coming soon."; 
	return "default"; 
}

// Forms are hidden at page load 
var tpnIdCookie = 'tpnIdCookie'; 
setCookie(tpnIdCookie, tpn_id, 365); // Keep tpn_id in a cookie 
$('#sendrequest').hide(); 
$('#newcontactdiv').hide(); 
var showOneForm = false;

$('#spyimg').click(function() { 
	if(showOneForm) 
	$('#sendrequest').hide(); 
	$('#newcontactdiv').toggle(); 
	showOneForm = true; 
}); 
$('#mailimg').click(function() { 
	if(showOneForm) 
	$('#newcontactdiv').hide(); 
	$('#sendrequest').toggle(); 
	showOneForm = true; 
}); 
$('#gmailimg').click(function() { 
	$('form[name=gmailform]').submit(); 
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

var mailCookie = 'mailCookie'; 
// Globals for classic form 
var minLengthSecret = 10; 
var checkTpnID = false; 
var checkSecret = false; 
var tpnIDInput = $('input[name="name"]');
var secretInput = $('input[name="secret"]');
// Globals for mail form 
var checkMailSender = false; 
var checkMailReceiver = false; 
var mailSenderObject = $('#sendrequest .mail_sender'); 
var mailReceiverObject = $('#sendrequest .mail_receiver'); 
// Pre-fill mail by cookie and use profile-defined mail if not set by cookie : 
if(checkCookie(mailCookie)) 
	mailSenderObject.val(getCookie(mailCookie));
else 
	mailSenderObject.val(mailProfile);
if(checkMailSender)  
	checkForm(mailSenderObject, isValidMail);  
if(checkMailReceiver)  
	checkForm(mailReceiverObject, isValidMail); 

// TODO : could be less ugly with dedicated class (see further)
mailSenderObject.focus(function() { checkMailSender = true; checkForm(mailSenderObject, isValidMail); }); 
mailSenderObject.blur(function() { checkMailSender = false; }); 
mailReceiverObject.focus(function() { checkMailReceiver = true; checkForm(mailReceiverObject, isValidMail); }); 
mailReceiverObject.blur(function() { checkMailReceiver = false; }); 
tpnIDInput.focus(function() { checkTpnID = true; checkForm(tpnIDInput, isValidMail); }); 
tpnIDInput.blur(function() { checkTpnID = false; }); 
secretInput.focus(function() { checkSecret = true; checkForm(secretInput, isValidSecret); }); 
secretInput.blur(function() { checkSecret = false; });

function isValidMail(object) 
{ 
	var str = getInputText(object); 
	var mailRegex = new RegExp('^[a-z0-9]+([_|\\.|-]{1}[a-z0-9]+)*@[a-z0-9]+([_|\\.|-]{1}[a-z0-9]+)*[\\.]{1}[a-z]{2,6}$', 'i'); 
	return mailRegex.test(str); 
}

function isValidSecret(object) 
{ 
	var str = getInputText(object); 
	if(str.length >= minLengthSecret) 
		return true; 
	return false; 
}

function getInputText(object) 
{ 
	return object.val(); 
} 

// TODO : would be better off with Object 'checkForm' and check on global variable check belonging to this Object 
function contCheck(object) 
{ 
	if(object.is($('#acceptrequest .posturl'))) 
		return checkUrl; 
	if(object.is($('#sendrequest .mail_sender'))) 
		return checkMailSender; 
	if(object.is($('#sendrequest .mail_receiver'))) 
		return checkMailReceiver; 
	if(object.is($('input[name="name"]'))) 
		return checkTpnID; 
	if(object.is($('input[name="secret"]'))) 
		return checkSecret; 
	return false; 
}

function checkForm(object, isValid) 
{ 
	if(isValid(object)) 
	{ 
		object.removeClass('isinvalid'); 
		object.addClass('isvalid'); 
	} 
	else 
	{ 
		object.removeClass('isvalid'); 
		object.addClass('isinvalid'); 
	} 
	setTimeout(function() { 
		if(contCheck(object)) { 
			checkForm(object, isValid); }
		else {
			//object.val(''); 
			if(object.val()=='') 
			{ 
				object.removeClass('isvalid'); 
				object.removeClass('isinvalid'); 
			} 
			return; }
	}, 200); 
}

$('#sendrequest .sendinvitation').click(function() { 
	if( isValidMail(mailSenderObject) && isValidMail(mailReceiverObject)) 
	{
		var postUrl = postUrl1;
		var mailReceiver = getInputText(mailReceiverObject);
		var mailSender = getInputText(mailSenderObject); 
		var nameSender = nameProfile; 
		// Add mail to cookie if not already set or updated 
		if(!checkCookie(mailCookie) || getCookie(mailCookie) != mailSender) 
			setCookie(mailCookie,mailSender,365); 
		$.post( postUrl, { mail_receiver : mailReceiver, mail_sender : mailSender, name_sender : nameSender, tpn_id_sender: tpn_id }) 
		.done(function(data) {
			// expects echo from php 
			if(data == SUCCESS) 
				alert('Invitation was successfully sent to : '+mailReceiver); 
			else if(data==FAILURE) 
				alert('Failure in sending mail'); 
			else if(data==REQUEST_ALREADY_EXISTS) 
				alert('A request from '+mailSender+' to : '+mailReceiver+' already exists.'); 
			else if(data==INVALID_ADDRESS) 
				alert('Invalid address'); 
			else if(data != '') // Silent case (mode REQUEST_ACCEPTED) 
				alert('Invitation was successfully sent to : '+mailReceiver); 
			else 
				alert('Unknown error'); 
		}) 
		.fail(function() { alert('error in send function'); });
	} 
	else 
	{ 
		alert('Wrong mail addresses'); 
	} 
} 
);

// AJAX for send contacts form
$('input[name="add"]').click(function() { 
	if( isValidMail(tpnIDInput) &&  isValidSecret(secretInput)) // We check validity of tpn name with isValidName because structure of regex is exactly the same 
	{ 
		// Submit form 
		$('form[name="newcontact"]').submit(); 
	} 
	else 
	{ 
		if(!isValidMail(tpnIDInput)) 
		{ 
			if(!isValidSecret(secretInput)) 
			{ 
				alert('TeapotNet address should look like this : myname@tracker.tld, and secret should be at least '+minLengthSecret+' characters long.'); 
				return; 
			} 
			else 
				alert('TeapotNet address should look like this : myname@tracker.tld.'); 
		} 
		if(!isValidSecret(secretInput)) 
			alert('Secret should be at least '+minLengthSecret+' characters long.'); 
	} 
} 
);

var lengthUrl = centralizedFriendSystemUrl.length; 
var prefixLength = FIRST_REQUEST_PREFIX.length; 
var idLength = 32; 
var checkUrl = false; 
var currentIdRequest = ''; 

if(checkUrl)  
	checkForm($('#acceptrequest .posturl'), isValidUrl); 

function isValidUrl(object)
{
	var returnBool = false; 
	var str = getInputText(object); 
	var strlen = str.length;
	returnBool |= ( (str.substring(0,lengthUrl) == centralizedFriendSystemUrl && (str.substring(strlen-16-idLength,strlen-idLength) == '.php?id_request=')) ? true : false); 
	returnBool |= ( ((str.substring(0,prefixLength) == FIRST_REQUEST_PREFIX || str.substring(0,prefixLength) == REQUEST_ACCEPTED_PREFIX) && (strlen == idLength+prefixLength)) ? true : false ); 
	//$('#acceptrequest h2').html((str.substring(strlen-idLength-16,strlen-idLength))); // Just for debug 
	return returnBool; 
}

function getIdRequest(str) 
{ 
	var idrequest = ''; 
	var strlen = str.length; 
	// Assumes str is valid text input 
	idrequest = str.substring(strlen-idLength,strlen); 
	return idrequest; 
}

function getMethod(str) 
{ 
	var method = '0'; 
	var strlen = str.length; 
	if((str.substring(0,prefixLength) == FIRST_REQUEST_PREFIX || str.substring(0,prefixLength) == REQUEST_ACCEPTED_PREFIX) && (strlen == idLength+prefixLength)) 
		return str.substring(0,prefixLength); 
	if(str.substring(0,lengthUrl) == centralizedFriendSystemUrl && (str.substring(strlen-16-idLength,strlen-idLength) == '.php?id_request=')) 
	{ 
		if(str.substring(lengthUrl, strlen-16-idLength)=='secretgen') 
			method = FIRST_REQUEST_PREFIX; 
		else if(str.substring(lengthUrl, strlen-16-idLength)=='laststep') 
			method = REQUEST_ACCEPTED_PREFIX; 
		else 
			method = '-1'; 
	} 
	return method; 
}

$('#acceptrequest .posturl').focus(function() { checkUrl = true; checkForm($('#acceptrequest .posturl'), isValidUrl); }); 
$('#acceptrequest .posturl').blur(function() { setTimeout(function() {}, 1000); checkUrl = false; }); 
$('#acceptrequest .posturl').keypress(function(e) {
	if (e.keyCode == 13 && !e.shiftKey) {
		e.preventDefault();
		postAcceptRequest();
	}
});
$('#acceptrequest .postfriendrequest').click(postAcceptRequest);

function postAcceptRequest() { 
	var postUrl = centralizedFriendSystemUrl; 
	var urlObject = $('#acceptrequest .posturl'); 
	var nStep = 0; 
	if(isValidUrl(urlObject))
	{ 
		var str = getInputText(urlObject);
		if(getMethod(str) == FIRST_REQUEST_PREFIX) 
		{ 
			nStep = 2; 
			postUrl += 'secretgen.php?id_request='; 
		} 
		else if (getMethod(str) == REQUEST_ACCEPTED_PREFIX) 
		{ 
			nStep = 3; 
			postUrl += 'laststep.php?id_request='; 
		} 
		else 
		{ 
			alert('Impossible to proceed'); 
			return; 
		} 
		var idrequest = getIdRequest(str); 
		postUrl += idrequest; 
		currentIdRequest = idrequest; 
		$.post( postUrl, { tpn_id: tpn_id}) 
		.done(function(data) {
			if(data == EMPTY_REQUEST) 
				alert('error, request not found'); 
			else if(data == REQUEST_ALREADY_ACCEPTED) 
				alert('This request has already been accepted.'); 
			else if(isJsonString(data))
			{ 
				var arr = $.parseJSON(data); 
				var secret = arr['secret'];
				var tpnid = arr['tpn_id']; 
				addContact(tpnid, secret, nStep); 
			} 
			else 
				alert('error, request not found'); 
		}) 
		.fail(function() { alert('error, entered code does not seem to be valid'); });
	} 
	else 
	{ 
		alert('Invalid address'); 
	}
}

function addContact(tpnid, secret, nStep) { 
	$.post(prefix+"/", { name: tpnid, secret: secret, token: token}) 
	.done(function(data) {
		// Drop the line in db 
		if(nStep == 3) 
			$.post(centralizedFriendSystemUrl+"finalize.php", { tpn_id: tpn_id, id_request: currentIdRequest}); 
		alert('Contact added to list'); 
		// Refresh because contacts list is not in javascript 
		location.reload(); 
	}) 
	.fail(function() { alert('error, contact could not be added'); });
}
