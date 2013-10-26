<!DOCTYPE html>
<head>
<meta charset="utf-8">
<meta name="author" content="Antoine Roussel">
<link rel="stylesheet" href="style.css">
<link rel="shortcut icon" type="image/x-icon" href="favicon.ico">
<script type="text/javascript" src="jquery.min.js"></script>
</head>

<body>


<?php

require_once("googleAPI/src/Google_Client.php");

session_start();

$client = new Google_Client();
$client->setApplicationName("TeapotNet Gmail friend requests");
$client->setScopes("http://www.google.com/m8/feeds/");

// Set API TeapotNet information
$client->setClientId('1004486490546.apps.googleusercontent.com');
$client->setClientSecret('qrzDwPvgb72D914ijnGAILtw');
$client->setRedirectUri('http://rebecq.fr/tpnfriends/gcontacts.php');
$client->setDeveloperKey('AIzaSyArpqJdKKQ4PF9bj2FrImOTn_wplvRXz2w');

if (isset($_GET['code'])) 
{
	$client->authenticate();
	$_SESSION['token'] = $client->getAccessToken();
	$redirect = 'http://' . $_SERVER['HTTP_HOST'] . $_SERVER['PHP_SELF'];
	header('Location: ' . filter_var($redirect, FILTER_SANITIZE_URL));
}

if (isset($_SESSION['token']))
{
	$client->setAccessToken($_SESSION['token']);
}

if (isset($_REQUEST['logout'])) 
{
	unset($_SESSION['token']);
	$client->revokeToken();
	// Remove cookie
	setcookie('tpnIdCookie', '', time()-3600);
}


if (isset($_POST['tpn_id']))
{
	$tpn_id = $_POST['tpn_id'];
	// Keep it in a cookie because page will reload if not logged in
	setCookie('tpnIdCookie', $tpn_id, time()+3600);
}
else if(isset($_COOKIE['tpnIdCookie']))
{
	$tpn_id = $_COOKIE['tpnIdCookie'];
}
else
{
	print 'You have been logged out, please try to reconnect from your teapotnet instance';
	return;
}



if ($client->getAccessToken())
{

	$req = new Google_HttpRequest("https://www.google.com/m8/feeds/contacts/default/full?max-results=9999&sortorder=descending");
	$val = $client->getIo()->authenticatedRequest($req);

	$google_contacts = $val->getResponseBody();

	// Small hack because ':' are not understood in XML objects
	$search=array('<gd:','</gd:');
	$replace=array('<gd','</gd');
	$response_replaced = str_replace($search, $replace,$google_contacts);

	// The contacts api only returns XML responses.
	$response = json_encode(simplexml_load_string($response_replaced));
	$array = json_decode($response, true);

	$arrayContacts = $array['entry'];
	uasort($arrayContacts, 'compareContacts');

	// For tests
	//print "<pre>" . print_r($array, true) . "</pre>";

	// The access token may have been updated lazily (was in google API example --> useful or not ?) 
	$_SESSION['token'] = $client->getAccessToken();

}
else 
{
	$auth = $client->createAuthUrl();
}
?>

<div class="gtopbanner">

<?php
// Authentication
if (isset($auth))
{
	print "</div> <div class='connectme'> <a class=login href='$auth'>Connect to Gmail</a> </div>";
	print '<script type="text/javascript"> $(".gtopbanner").hide(); </script>';
	return;
} 
else 
{
	// Display personal information so that user knows he is correctly logged in.
	$me = $array['author']['name'];
	$myEmail = $array['author']['email'];
	$title = $array['title'];
	?>
	<div class='logoutbox'>
		<div class = 'myinfo'>
			<!-- <h2> <?php print $me ?> </h2> -->
			<span class="myemail"> <?php print $myEmail ?> </span>
		</div>
		<div class='logout'>
			<a class=logout href='?logout'>Logout</a>
		</div>
	</div>
	<?php
}

?>

<h1> <?php print $title ?> </h1>

<input id="selectallemails" class="button" type="button" value="Select all">
<input id="sendinvitations" class="button" type="button" value="Send invitations">

</div>

<div id="content">

<div id="searchbargmailid">
	<input type="text" name="searchgmailcontact" class="searchbargmail">
</div>

<div id="listcontacts" class="box">

<?php
	$toLoadPhotos = array();
?>
	<script type="text/javascript">
		var toLoadPhotos = [];
		var index = 0;
		var separator = '10051921';
		
	</script>
<?php
	foreach($arrayContacts as $contact)
	{

		if(array_key_exists('gdemail',$contact))
		{
			$arrayEmails = $contact['gdemail'];

			if(sizeOf($arrayEmails) == 1)
				$emailContact = $contact['gdemail']['@attributes']['address'];
			else
			{
				// TODO : more than the first one
				$emailContact = $contact['gdemail'][0]['@attributes']['address'];
			}
		}
		else
		{
			$emailContact = '';
		}

		$nameContact = $contact['title'];

		if(is_array($nameContact))
			$nameContact = $emailContact; // TODO : what if no name and no contact ?

		$urlPrefix = $contact['link'][1]['@attributes']['href'];
		$photoUrlPrefix = $contact['link'][0]['@attributes']['href'];

		$nameIdContact = trimName($nameContact);

		?>

		<script type="text/javascript">
		$('#listcontacts').append('<div id="contact_<?php print $nameIdContact ?>" class="gmailcontact"><div class="infogmailcontact"><h2> <?php print escape($nameContact) ?> </h2><span class="email"> <?php print $emailContact ?> </span></div><div class="contactphoto">');
		</script>

		<?php

		// Check if image exists (f**k this API which gives images URLs when they don't exist)
		if(isGenuinePhoto($urlPrefix, $photoUrlPrefix))
		{
			// Session token is a string
			$params = explode('"',$_SESSION['token']);
			$accessToken = $params[3];
			$token_type = $params[7];

			$photoUrl = $photoUrlPrefix.'?auth=true&access_token='.$accessToken.'&token_type='.$token_type;

			// TODO : careful, could create double ids with contacts having the same name 
			$divObject = '#contact_'.$nameIdContact;

			// Add to array of to to be loaded photos
			$toLoadPhotos["'".$divObject."'"] = "'".$photoUrlPrefix."'";
			print '<script type="text/javascript"> toLoadPhotos[index] = "'.$divObject.'"+separator+"'.$photoUrl.'"; index++; </script>';

		}
		?>
			</div>
		</div>

		<?php
	}
?>

<script type="text/javascript">

	var loadPhotosInterval = 100;
	var countIndex = -1;
	var currentSearchContact = '';

	loadPhotos();
	displayMatchingContacts();

	function loadPhotos()
	{
		countIndex++;
		//console.log(countIndex);
		if(countIndex >= index)
			return;
		var str = toLoadPhotos[countIndex];
		var splitted = str.split(separator);
		var divObject = splitted[0];
		var photoUrl = splitted[1];
		$(divObject).append('<img src=\"'+photoUrl+'\">');
		setTimeout(function() { 
				loadPhotos();
		},loadPhotosInterval); // should technically be a setInterval ?
	}


	var emailsToSend = [];
	var allSelected = false;

	function emptySelected()
	{
			$('#listcontacts').find('.gmailcontact').each(function() {
				emailsToSend = [];
				$(this).removeClass('selectedcontact');
			});
	}

	function contactClick() {

		var emailContact = $('.email',this).html();

		if(emailContact.length == 2) // Why ? I don't know, but it should work
		{
			alert('This contact has no email');
		}
		else
		{
			if($.inArray(emailContact, emailsToSend) == -1) // Equals false condition. TODO : nicer function doing this check
			{
				$(this).addClass('selectedcontact');
				emailsToSend.push(emailContact);
			}
			else
			{
				$(this).removeClass('selectedcontact');
				// Get email of contact out of emailsToSend
				emailsToSend.splice(emailsToSend.indexOf(emailContact),1);
			}
		}

	}
	$('.gmailcontact').click(contactClick);

	$('#selectallemails').click(function() {
		if(!allSelected)
		{
			$('#listcontacts').find('.gmailcontact:visible').each(function() {
				var emailContact = $('.email',this).html();
				if(emailContact.length > 2 && $.inArray(emailContact, emailsToSend) == -1)
				{
					emailsToSend.push(emailContact);
					$(this).addClass('selectedcontact');
				}
			});
			$('#selectallemails').val('Remove all');
			allSelected = true;
		}
		else
		{
			emptySelected();
			$('#selectallemails').val('Select all');
			allSelected = false;
		}

	});

	$('#sendinvitations').click(function() {

		if(emailsToSend.length > 0)
		{
			if (confirm('Send invitations to '+emailsToSend.length+' '+( (emailsToSend.length == 1) ? 'friend' : 'friends')+' ?'))
			{
				var tpn_id = '<?php print $tpn_id ?>';
				var nameSender = '<?php print $me ?>';
				var mailSender = '<?php print $myEmail ?>';

				var postUrl = "http://rebecq.fr/tpnfriends/postrequest.php";

				var success = 0;
				var failedEmails = [];
				var doneRequests = 0;
				var nEmailsToSend = emailsToSend.length;

				emailsToSend.forEach(function(mailReceiver)
				{
					$.post( postUrl, { mail_receiver : mailReceiver, mail_sender : mailSender, name_sender : nameSender, tpn_id_sender: tpn_id })
					.done(function(data) {
						if(data == 16) // Equals to global "SUCCESS"
						{
							success++;
						}
						else
						{
							failedEmails.push(mailReceiver);
							//console.log(mailReceiver);
							//console.log(failedEmails);
						}
						doneRequests++;
						if(doneRequests == nEmailsToSend)
							statsRequests(success, failedEmails);
					})
					.fail(function() {
							failedEmails.push(mailReceiver);
							doneRequests++;
						if(doneRequests == nEmailsToSend)
							statsRequests(success, failedEmails);
					});

				});

			}
		}
		else
		{
			alert('No contact selected');
		}

	});

	// Done by external function because we have to wait for posts responses
	function statsRequests(success, failedEmails)
	{
		//console.log(failedEmails.length);
		//console.log(failedEmails);

		if(failedEmails.length>0)
			alert('Invitations could not be sent to : '+failedEmails);
		else
			alert('Invitation successfully sent to '+success+' friends.');

		emptySelected();
	}

	function displayMatchingContacts()
	{
		var searchContact = $('input[name="searchgmailcontact"]').val();
		//console.log(searchContact);
		//console.log(currentSearchContact);

		if(searchContact != currentSearchContact) // Don't bother to search contacts if search text hasn't changed
		{
			currentSearchContact = searchContact;

			// Hide not matching contacts
			$('#listcontacts').find('.gmailcontact').each(function() {		
				var nameContact = $(this).find('.infogmailcontact h2').html();
				//console.log(nameContact);

				if(!matchSearch(nameContact, searchContact))
				{
					$(this).hide();
				}
				else
				{
					$(this).show();
				}
			});
		}

		setTimeout(function() {
			displayMatchingContacts();
		}, 1000);

	}

	function matchSearch(nameContact, searchContact)
	{
		if(searchContact == '')
			return true;

		var regExp = new RegExp(searchContact, 'i');
		if(regExp.test(nameContact))
			return true;

		return false;
	}


</script>

</div>

</div>
</body>


<?php

function isGenuinePhoto($urlPrefix, $photoUrlPrefix)
{
// TODO : 	not functional despite following Google API Contacts doc -->
//		always return true and probably counts more requests than it should
/*
	$isGenuinePhoto = false;

		print $photoUrlPrefix;
		print '<br>';
		print $urlPrefix;
		print '<br>';

		$exploded = explode("/", $photoUrlPrefix); 
		// Should be used to fetch a "gdetag" beacon, but is nowhere to be found ???

		if(substr($photoUrlPrefix,strlen($urlPrefix),1) == '/')
		{
			$isGenuinePhoto = true;
		}

	return $isGenuinePhoto;

*/
	return true;
}

function trimName($name)
{
	$oldChars = array(" ", "-", ".", "@", "(", ")", "'", "_",",",";","é","è","à","ù","ç","î","ë","ô");
	$newChars = array("", "", "", "", "", "", "", "", "", "","e","e","a","u","c","i","e","o");
	$returnstr = str_replace($oldChars, $newChars, $name);

	return strtolower($returnstr);
}

function escape($str)
{
	$oldChars = array("'");
	$newChars = array("\'");
	$returnstr = str_replace($oldChars, $newChars, $str);

	return $returnstr;
}

function compareContacts($a, $b)
{
	// We compare using $contact['title'] although some contacts have an array (and not a string) as title
	// (those are contacts with no names registered). They will be displayed at the end
	if($a['title'] > $b['title'])
	{
		return 1;
	}
	else if($a['title'] == $b['title'])
	{
		return 0;
	}
	else
		return -1;

	return -1;
}

?>

