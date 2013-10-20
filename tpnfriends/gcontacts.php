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
		print '<div class="gmailcontact">';
		print $contact['title'];
		print '<br>';
		print $contact['gdemail']['@attributes']['address'];
		print '<br>';

		// Check if image exists (f**k this API which gives images URLs when they don't exist)
		// TODO : is incorrect for now
		// TODO : write function
		$img_exists = false;
		$urlPrefix = $contact['id'];
		$photoUrlPrefix = $contact['link'][1]['@attributes']['href'];

		print $photoUrlPrefix;
		print '<br>';
print $urlPrefix;
		print '<br>';
		print substr($photoUrlPrefix,strlen($urlPrefix),1);

		if(substr($photoUrlPrefix,strlen($urlPrefix)+1,1) == '/')
		{
			$img_exists = true;
		}
		
		if($img_exists)
			print 'TRUE';

		// Retreive and display photo passing access_token in GET TODO : in javascript so that if it is a bit long, it doesn't lag
/*
		$accessToken = substr($_SESSION['token'],17,51);
		$photoUrl = $contact['link'][1]['@attributes']['href'].'?access_token='.$accessToken;

		print '<img src="'.$photoUrl.'" />';
*/
		print '</div>';
	
	}

	// The access token may have been updated lazily (was in google API example --> useful or not ?) 
	$_SESSION['token'] = $client->getAccessToken();

}
else 
{
	$auth = $client->createAuthUrl();
}


// Authentication
if (isset($auth)) 
{
    print "<a class=login href='$auth'>Connect Me!</a>";
} 
else 
{
    print "<a class=logout href='?logout'>Logout</a>";
}
