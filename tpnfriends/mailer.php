<?php

error_reporting(E_ALL);
ini_set('SMTP', 'localhost');
ini_set('smtp_port', 25);

define("MODE_FIRST_REQUEST", 1, true);
define("MODE_REQUEST_ACCEPTED", 2, true);

define("FIRST_REQUEST_PREFIX",1005,true);
define("REQUEST_ACCEPTED_PREFIX",1921,true);

include_once("echoes.php");

function sendFriendRequestMail($name_sender, $mail_sender, $mail_destination, $id_request, $mode)
{

	$prefixFriendSystem = "http://rebecq.fr/tpnfriends/"; // TODO : change

	switch($mode)
	{
		case MODE_REQUEST_ACCEPTED:
			$subject = "TeapotNet friend request accepted";
			break;

		default: // Equivalent to MODE_FIRST_REQUEST
			$subject = "TeapotNet friend request";
			break;
	}

	switch($mode)
	{
		case MODE_REQUEST_ACCEPTED:
			$linkRequest = $prefixFriendSystem."laststep.php?id_request=".$id_request;
			break;

		default: // Equivalent to MODE_FIRST_REQUEST
			$linkRequest = $prefixFriendSystem."secretgen.php?id_request=".$id_request;
			break;
	}

	// Text and HTML formats
	switch($mode)
	{
		case MODE_REQUEST_ACCEPTED:
			$message_txt = "Hi ! Your friend request has been accepted. To finalize connection with your friend, just paste the following code in the 'Accept Request' section of your TeapotNet : ".REQUEST_ACCEPTED_PREFIX.$id_request.". Sincerely,<br>The TeapotNet Team";
			$message_html = "<html><head> <style> .code { text-align: center; font-weight: bold; color: #1560BD; }</style></head><body>Hi !<p>".$mail_sender." has accepted your friend request. To finalize connection with your friend, just paste the following code in the 'Accept Request' section of your TeapotNet :<br><span class='code'>".REQUEST_ACCEPTED_PREFIX.$id_request."</span></p><p>If you have installed the TeapotNet plugin for your browser, you can just click this link :<br><a href=\"".$linkRequest."\">Direct connection via browser plugin</a></p><p>Sincerely,<br>The TeapotNet Team</p></body></html>";
			break;

		default: // Equivalent to MODE_FIRST_REQUEST
		$name_friend = $mail_sender;
		if($name_sender != '')
			$name_friend = $name_sender;
			$message_txt = $name_friend." sent you a TeapotNet friend request. To accept his invitation, just paste the following code in the 'Accept Request' section of your TeapotNet : ".FIRST_REQUEST_PREFIX.$id_request."If you want more information on TeapotNet, you can visit http://teapotnet.org. Sincerely, The TeapotNet Team";
			$message_html = "<html><head> <style> .code { text-align: center; font-weight: bold; color: #1560BD; }</style></head><body>Hi !<p>".$name_friend." sent you a TeapotNet friend request. To accept his invitation, just paste the following code in the 'Accept Request' section of your TeapotNet :<br><span class='code'>".FIRST_REQUEST_PREFIX.$id_request."</span></p><p>If you have installed the TeapotNet plugin for your browser, you can just click this link :<br><a href=\"".$linkRequest."\">Direct connection via browser plugin</a></p><p>If you want more information on TeapotNet, you can visit <a href='http://teapotnet.org'>TeapotNet</a></p><p>Sincerely,<br>The TeapotNet Team</p></body></html>";
			break;

	}

	sendMail($subject, $name_sender, $mail_sender, $mail_destination, $message_txt, $message_html, $mode);
}



function sendMail($subject, $name_sender, $mail_sender, $mail_destination, $message_txt, $message_html, $mode)
{

	$mail = $mail_destination;
	if (!preg_match("#^[a-z0-9._-]+@(hotmail|live|msn).[a-z]{2,4}$#", $mail)) // Different conventions among mail providers
	{
	    $change_line = "\r\n";
	}
	else
	{
	    $change_line = "\n";
	}

	//==========
	  
	$boundary = "-----=".md5(rand());
	  
	//===== Header

	$name_sender_displayed = $mail_sender;
	if($name_sender != '')
		$name_sender_displayed = $name_sender;

	$header = "From: \"".$name_sender_displayed."\"<".$mail_sender.">".$change_line;
	$header.= "Reply-to: \"".$name_sender_displayed."\"<".$mail_sender.">".$change_line;
	$header.= "MIME-Version: 1.0".$change_line;
	$header.= "Content-Type: multipart/alternative;".$change_line." boundary=\"$boundary\"".$change_line;
	//===== 
	  
	//===== Mail
	$message = $change_line."--".$boundary.$change_line;
	//===== Adds text message
	$message.= "Content-Type: text/plain; charset=\"UTF-8\"".$change_line;
	$message.= "Content-Transfer-Encoding: 8bit".$change_line;
	$message.= $change_line.$message_txt.$change_line;
	//==========
	$message.= $change_line."--".$boundary.$change_line;
	//===== Adds HTML message
	$message.= "Content-Type: text/html; charset=\"UTF-8\"".$change_line;
	$message.= "Content-Transfer-Encoding: 8bit".$change_line;
	$message.= $change_line.$message_html.$change_line;
	//==========
	$message.= $change_line."--".$boundary."--".$change_line;
	$message.= $change_line."--".$boundary."--".$change_line;
	//==========

	$result = mail($mail,$subject,$message,$header);
	if($result)
	{
		//echo 'Mail was successfully sent to : '.$mail_destination;
		if($mode == MODE_FIRST_REQUEST) // else silent echo
			echo SUCCESS;
	}
	else
	{
		echo FAILURE;
	}
	//==========
}


?>
