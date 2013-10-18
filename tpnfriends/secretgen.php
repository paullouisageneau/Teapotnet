<?php
	/*********************************************/
	/*** Second step : receiver of invitation  ***/
	/*** connects to this page to get sender's ***/
	/*** info. Mail is sent back to sender     ***/
	/*********************************************/

include_once("echoes.php");
include_once("dbfunctions.php");
include_once("mailer.php");

// Allow cross-origin dialog
header("Access-Control-Allow-Origin: *");

	// tpn_id of receiver is sent by POST on an url with id_request as GET parameter
	// (not designed for security, but more for avoiding basic users mistakes)

	//$tpn_id_receiver = "groboloss@teapotnet.org"; // Just for tests

	if(isset($_GET['id_request']) && isset($_POST['tpn_id']))
	{
		$id_request = $_GET['id_request'];
		$tpn_id_receiver = $_POST['tpn_id'];

		try {
			$bdd = dbConnect();
		}
		catch (Exception $e)
		{
			die('Erreur : ' . $e->getMessage());
			return;
		}

		$req = $bdd->prepare("SELECT tpn_id_sender, date_proposed, name_receiver, mail_receiver, mail_sender, date_accepted_1, secret FROM teapot.tpn_requests WHERE (id_request = ?);");
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
			$date_accepted_1 = $data['date_accepted_1'];
			$secret = $data['secret'];
			//echo $tpn_id_sender;
		}

		// Case empty
		if($tpn_id_sender == '')
		{
			echo EMPTY_REQUEST;
		}
		else
		{

			$already_accepted = false;
			
			if($date_accepted_1 > 0)
			{
				//echo REQUEST_ALREADY_ACCEPTED;
				//return;

				// We just dont regenerate id_request_2 and send mail if it has already been accepted
				$already_accepted = true;
			}

			// Check if first friend request is not too old
			$date_accepted_1 = date('Y-m-d H:i:s');
			$d1 = new DateTime($date_proposed);
			$d2 = new DateTime($date_accepted_1);
			$time_between = $d2->diff($d1);

			if($time_between->d < 1) // Less than 24 hours
			{

				if(!$already_accepted)
				{
					// Generate secret :
					$secret = openssl_random_pseudo_bytes(64,true); // Uses strong option
					// Generate another id_request
					$id_request_2 = openssl_random_pseudo_bytes(32);
			
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

				// Echoes secret and tpn_id of sender to receiver
				$output = array("secret" => $secret, "tpn_id" => $tpn_id_sender);
				echo json_encode($output, JSON_PRETTY_PRINT);
				//echo SUCCESS;
				
			}
			else
			{
				echo REQUEST_TOO_OLD;
			}
		}

	}
	else
	{
		echo SPECIFY_MAIL;
	}

?>
