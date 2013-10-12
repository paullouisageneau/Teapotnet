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

	switch($mode)
	{
		case MODE_FIRST_REQUEST:
			$subject = "TeapotNet friend request";
			break;

		case MODE_REQUEST_ACCEPTED:
			$subject = "TeapotNet friend request accepted";
			break;

		default:
			$subject = "TeapotNet friend request";
			break;
	}

	// Text and HTML formats
	switch($mode)
	{
		case MODE_FIRST_REQUEST:
			$message_txt = "Someone sent you a TeapotNet friend request. To accept his invitation, just paste the following code in the 'Accept Request' section of your TeapotNet : ".FIRST_REQUEST_PREFIX.$id_request.". If you want more information on TeapotNet, you can visit http://teapotnet.org. Sincerely, The TeapotNet Team";
			$message_html = "<html><head></head><body>Hi !<br>Someone sent you a TeapotNet friend request. To accept his invitation, just paste the following code in the 'Accept Request' section of your TeapotNet :<br>".FIRST_REQUEST_PREFIX.$id_request."<br>If you have installed the TeapotNet plugin for your browser, you can just click on this link :<br>BOZ<br>If you want more information on TeapotNet, you can visit <a href='http://teapotnet.org'>TeapotNet</a><br>Sincerely,<br>The TeapotNet Team</body></html>";
			break;

		case MODE_REQUEST_ACCEPTED:
			$message_txt = "Hi ! Your friend request has been accepted. To finalize connection with your friend, just paste the following code in the 'Accept Request' section of your TeapotNet : ".REQUEST_ACCEPTED_PREFIX.$id_request.". Sincerely,<br>The TeapotNet Team";
			$message_html = "<html><head></head><body>Hi !<br>Your friend request has been accepted. To finalize connection with your friend, just paste the following code in the 'Accept Request' section of your TeapotNet :<br>".REQUEST_ACCEPTED_PREFIX.$id_request."<br>If you have installed the TeapotNet plugin for your browser, you can just click on this link :<br>BOZ<br>Sincerely,<br>The TeapotNet Team</body></html>";
			break;

		default:
			$message_txt = "Someone sent you a TeapotNet friend request. To accept his invitation, just paste the following code in the 'Accept Request' section of your TeapotNet : ".FIRST_REQUEST_PREFIX.$id_request."If you want more information on TeapotNet, you can visit http://teapotnet.org. Sincerely, The TeapotNet Team";
			$message_html = "<html><head></head><body>Hi !<br>Someone sent you a TeapotNet friend request. To accept his invitation, just paste the following code in the 'Accept Request' section of your TeapotNet :<br>".FIRST_REQUEST_PREFIX.$id_request."<br>If you have installed the TeapotNet plugin for your browser, you can just click on this link :<br>BOZ<br>If you want more information on TeapotNet, you can visit <a href='http://teapotnet.org'>TeapotNet</a><br>Sincerely,<br>The TeapotNet Team</body></html>";
			break;

	}

	sendMail($subject, $name_sender, $mail_sender, $mail_destination, $message_txt, $message_html);
}



function sendMail($subject, $name_sender, $mail_sender, $mail_destination, $message_txt, $message_html)
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
	$header = "From: \"".$name_sender."\"<".$mail_sender.">".$change_line;
	$header.= "Reply-to: \"".$name_sender."\"<".$mail_sender.">".$change_line;
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



	  
	//===== Sends e-mail
	/*
	echo $mail;
	echo '<br>';
	echo $header;
	echo $message;
	echo '<br>';
	*/
	$result = mail($mail,$subject,$message,$header);
	if($result)
	{
		//echo 'Mail was successfully sent to : '.$mail_destination;
	}
	else
	{
		echo FAILURE;
	}
	//==========
}


?>
