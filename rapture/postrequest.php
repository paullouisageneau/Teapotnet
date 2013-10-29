<?php

	/***********************************************/
	/***   First step : sender tpn's instance    ***/
	/***     posts sender's info to this page    ***/
	/*** 	Mail is sent to receiver.            ***/
	/***********************************************/

include_once("echoes.php");
include_once("dbfunctions.php");
include_once("mailer.php");

// Allow cross-origin dialog
header("Access-Control-Allow-Origin: *");

	if(isset($_POST['name_sender']) && isset($_POST['tpn_id_sender']) && isset($_POST['mail_sender']) && isset($_POST['mail_receiver']))
	{

		$name_sender = $_POST['name_sender'];
		$tpn_id_sender = $_POST['tpn_id_sender'];
		$mail_sender = $_POST['mail_sender'];
		$mail_receiver = $_POST['mail_receiver'];

		if(isset($_POST['name_receiver'])) 
			$name_receiver = $_POST['name_receiver'];
		else
			$name_receiver = '';

		try {
			$bdd = dbConnect();
		}
		catch (Exception $e)
		{
			die('Erreur : ' . $e->getMessage());
			return;
		}

		// before processing, we check that there is not an awaiting request from same sender to same receiver
		$req = $bdd->prepare("SELECT id FROM tpn_requests WHERE (tpn_id_sender = ? AND mail_receiver = ? AND mail_sender = ?);");
		$req->execute(array($tpn_id_sender, $mail_receiver, $mail_sender)) 
				or die(print_r($req->errorInfo())
				);

		$return_check_query = '';

		while ($data = $req->fetch())
		{
			$return_check_query = $data['id'];
		}


		if($return_check_query == '')
		{
		

			// Generate id_request :
			$id_request = hash('md5',openssl_random_pseudo_bytes(32));

			// Current time:
			$date_proposed = date('Y-m-d H:i:s');

			$req = $bdd->prepare("INSERT INTO `teapot`.`tpn_requests` (`tpn_id_sender`,`mail_sender` ,`mail_receiver` ,`id_request`, `date_proposed`, `name_receiver`, `name_sender`)
		VALUES (?,?,?,?,?,?,?);");
			$req->execute(array($tpn_id_sender,$mail_sender ,$mail_receiver ,$id_request, $date_proposed, $name_receiver, $name_sender)) 
					or die(print_r($req->errorInfo())
					);


			// Send e-mail to other guy
			sendFriendRequestMail($name_sender, $mail_sender, $mail_receiver, $id_request, MODE_FIRST_REQUEST);

			//include("infopost.php");
		}
		else
		{
			echo REQUEST_ALREADY_EXISTS;
		}

	}
	else
	{
		echo INVALID_ADDRESS;
	}
		
?>
