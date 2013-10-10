<?php

	/***********************************************/
	/***   First step : sender tpn's instance    ***/
	/***     posts sender's info to this page    ***/
	/*** 	Mail is sent to receiver.            ***/
	/***********************************************/

include_once("dbfunctions.php");
include_once("mailer.php");

	// TODO : TeapotNet sends this info via POST
	/*
	$name_sender = 'Jotun';
	$tpn_id_sender = 'jotun@teapotnet.org';
	$mail_sender = 'antoine.rebecq@polytechnique.org';
	$mail_receiver = 'antoine@rebecq.fr';
	$name_receiver = 'Antoine Rebecq';
	*/

	if(isset($_POST['name_sender']) && isset($_POST['tpn_id_sender']) && isset($_POST['mail_sender']) && isset($_POST['mail_receiver']))
	{

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

		// TODO : before processing, we check that there is not an awaiting request from same sender to same receiver
		$req = $bdd->prepare("SELECT name_sender FROM tpn_requests WHERE (tpn_id_sender = ? AND mail_receiver = ?);");
		$req->execute(array($tpn_id_sender, $mail_receiver)) 
				or die(print_r($req->errorInfo())
				);

		$return_check_query = '';

		while ($data = $req->fetch())
		{
			$return_check_query = $data['name_sender'];
		}


		if($return_check_query == '')
		{
		

			// Generate id_request :
			$id_request = md5(rand());

			// Current time:
			$date_proposed = date('Y-m-d H:i:s');

			$req = $bdd->prepare("INSERT INTO `teapot`.`tpn_requests` (`tpn_id_sender`,`mail_sender` ,`mail_receiver` ,`id_request`, `date_proposed`, `name_receiver`)
		VALUES (?,?,?,?,?,?);");
			$req->execute(array($tpn_id_sender,$mail_sender ,$mail_receiver ,$id_request, $date_proposed, $name_receiver)) 
					or die(print_r($req->errorInfo())
					);


			// Send e-mail to other guy
			sendFriendRequestMail($name_sender, $mail_sender, $mail_receiver, $id_request, MODE_FIRST_REQUEST);
		}
		else
		{
			echo 'There already is a request from you to same friend';
		}

	}
	else
	{
		echo 'Incorrect Form';
	}
		
?>
