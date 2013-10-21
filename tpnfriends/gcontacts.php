<!DOCTYPE html>
<head>

</head>

<body>

<content>

<div class="gtopbanner">

<input class="sendfriendemails" type="submit">

<input class="selectallemails" type="submit">

</div>

<div class="listcontacts">

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
}

// Authentication
if (isset($auth)) 
{
	print "<div class='connectme'> <a class=login href='$auth'>Connect Me!</a> </div>"; // TODO : nice layout
	return;
} 
else 
{
	// TODO : display personal information so that user knows he is correctly logged in.
	print "<a class=logout href='?logout'>Logout</a>"; // Append to 
}

if ($client->getAccessToken()) 
{

	$req = new Google_HttpRequest("https://www.google.com/m8/feeds/contacts/default/full");
	$val = $client->getIo()->authenticatedRequest($req);

	$google_contacts = $val->getResponseBody();

	// Small hack because ':' are not understood in XML objects
	$search=array('<gd:','</gd:');
	$replace=array('<gd','</gd');
	$response_replaced = str_replace($search, $replace,$google_contacts);

	// The contacts api only returns XML responses.
	$response = json_encode(simplexml_load_string($response_replaced));
	$array = json_decode($response, true);

	print "<pre>" . print_r($array, true) . "</pre>";

	$arrayContacts = $array['entry'];

	foreach($arrayContacts as $contact)
	{
		$nameContact = $contact['title'];
		$emailContact = $contact['gdemail']['@attributes']['address'];
		$photoUrlPrefix = $contact['link'][1]['@attributes']['href'];

		?>

		<div id="contact_<?php $nameContact ?>" class="gmailcontact">

			<h2> <?php $nameContact ?> </h2>
			<span class="email"> <?php $emailContact ?> </span>

		</div>


		<?php

		// Check if image exists (f**k this API which gives images URLs when they don't exist)
		if(isGenuinePhoto($photoUrlPrefix))
		{
			$accessToken = substr($_SESSION['token'],17,51);
			$photoUrl = $photoUrlPrefix.'?access_token='.$accessToken;
			$divObject = '#contact_'+$nameContact;
			print '<script> $("#'.$divObject.'").append("<img src=\"'.$photoUrl.'\" />") </script>';
		}

		print '</div>';
	
	}

	// The access token may have been updated lazily (was in google API example --> useful or not ?) 
	$_SESSION['token'] = $client->getAccessToken();

}
else 
{
	$auth = $client->createAuthUrl();
}

?>

<script>
	var emailsToSend = [];
	$('.gmailcontact').click(function() {
		if() // TODO : email of contact is in emailsToSend
		{
			$(this).addClass('selectedcontact');
			var emailContact = $(this .email).val();
			emailsToSend.push(emailContact);
		}
		else
		{
			// Remove from class selectedcontact
			// Get email of contact out of emailsToSend
		}
	});

</script>

</div>

</content>
</body>


<?php

function isGenuinePhoto($photoUrlPrefix)
{
	$isGenuinePhoto = false;

		$urlPrefix = $contact['id']; // TODO : is wrong

		print $photoUrlPrefix;
		print '<br>';
		print $urlPrefix;
		print '<br>';
		print substr($photoUrlPrefix,strlen($urlPrefix),1);

		if(substr($photoUrlPrefix,strlen($urlPrefix)+1,1) == '/')
		{
			$img_exists = true;
		}

	return isGenuinePhoto;
}

// TODO : CSS classes gmailcontact, selectedcontact, gtopbanner, sendfriendemails, selectallemails, listcontacts, email, connectme

?>

