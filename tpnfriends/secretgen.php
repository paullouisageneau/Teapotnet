<?php
	/*********************************************/
	/*** Second step : receiver of invitation  ***/
	/*** connects to this page to get sender's ***/
	/*** info. Mail is sent back to sender     ***/
	/*********************************************/

include_once("dbfunctions.php");
include_once("mailer.php");

	// TODO : tpn_id of receiver is sent by POST on an url with id_request as GET parameter
	// (not designed for security, but more for avoiding basic users mistakes)

	// TODO : also check if request hasn't been accepted yet

	$tpn_id_receiver = "groboloss@teapotnet.org"; // Just for tests

	if(isset($_GET['id_request'])) // and $_POST tpn_id of receiver
	{
		$id_request = $_GET['id_request'];

		try {
			$bdd = dbConnect();
		}
		catch (Exception $e)
		{
			die('Erreur : ' . $e->getMessage());
			return;
		}

		$req = $bdd->prepare("SELECT tpn_id_sender, date_proposed, name_receiver, mail_receiver, mail_sender  FROM teapot.tpn_requests WHERE (id_request = ?);");
		$req->execute(array($id_request)) 
		or die(print_r($req->errorInfo())
		);

		$tpn_id_sender = '';

		while ($data = $req->fetch())
		{
			$tpn_id_sender = $data['tpn_id_sender'];
			$date_proposed = $data['date_proposed'];
			$name_receiver = $data['name_receiver'];
			$mail_receiver = $data['mail_receiver'];
			$mail_sender = $data['mail_sender'];
			echo $tpn_id_sender;
		}

		// Case empty
		if($tpn_id_sender == '')
		{
			echo 'Empty request. Try again !';
		}
		else
		{

			// Check if first friend request is not too old or TODO : hasn't already been accepted
			$date_accepted_1 = date('Y-m-d H:i:s');
			$d1 = new DateTime($date_proposed);
			$d2 = new DateTime($date_accepted_1);
			$time_between = $d2->diff($d1);

			if($time_between->d < 1) // Less than 24 hours
			{
				// Generate secret :
				$secret = md5(rand());
				// Generate another id_request (necessity is questionable)

				$id_request_2 = md5(rand());
			

				// Record tpn_id of receiver in db and send mail to sender (with id_request)
				$req = $bdd->prepare("UPDATE `teapot`.`tpn_requests` SET secret = ?, date_accepted_1 = ?, id_request_2 = ?, tpn_id_receiver = ? WHERE (id_request = ?);");
				$req->execute(array($secret, $date_accepted_1, $id_request_2, $tpn_id_receiver, $id_request)) 
							or die(print_r($req->errorInfo())
						);

				// Send mail back to sender for him to get infos from receiver's tpn
				if($name_receiver == '')
					$name_receiver = 'TeapotNet';

				sendFriendRequestMail($name_receiver, $mail_receiver, $mail_sender, $id_request_2, MODE_REQUEST_ACCEPTED);
				
			}
			else
			{
				echo 'Request is too old !';
			}
		}

	}
	else
	{
		echo 'Please specify a mail address for sender and a request id';
	}

?>
