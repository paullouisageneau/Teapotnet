
<?php

	/***********************************************/
	/***   Third step : sender of invitation     ***/
	/*** connects to this page to get receiver's ***/
	/*** info. Connection is complete.           ***/
	/***********************************************/

include_once("echoes.php");
include_once("dbfunctions.php");

// Allow cross-origin dialog
header("Access-Control-Allow-Origin: *");

	// id_request_2 is sent by get (id_request), and tpn_send id as post just to be sure

	//$tpn_id_sender = "jotun@teapotnet.org"; // Just for tests

	if(isset($_GET['id_request']) && isset($_POST['tpn_id']))
	{
		$id_request_2 = $_GET['id_request'];
		$tpn_id_sender = $_POST['tpn_id'];
	

		// Get dates
		$date_last_step = date('Y-m-d H:i:s');
		$d2 = new DateTime($date_last_step);


		try {
			$bdd = dbConnect();
		}
		catch (Exception $e)
		{
			die('Erreur : ' . $e->getMessage());
			return;
		}

		$req = $bdd->prepare("SELECT date_accepted_1 FROM teapot.tpn_requests WHERE (id_request_2 = ? AND tpn_id_sender = ?);");
		$req->execute(array($id_request_2, $tpn_id_sender)) 
			or die(print_r($req->errorInfo())
			);

		$date_accepted_1 = '';

		while ($data = $req->fetch())
		{
			$date_accepted_1 = $data['date_accepted_1'];
		}
		$d1 = new DateTime($date_accepted_1);
		$time_between = $d2->diff($d1);

		// Get secret and tpn_id_receiver if request has not been accepted too long ago
		if($time_between->d < 1) // 24 hours
		{

			$req = $bdd->prepare("SELECT secret, tpn_id_receiver FROM teapot.tpn_requests WHERE (id_request_2 = ? AND tpn_id_sender = ?);");
			$req->execute(array($id_request_2, $tpn_id_sender)) 
				or die(print_r($req->errorInfo())
				);

			$secret = '';
			$tpn_id_receiver = '';

			while ($data = $req->fetch())
			{
				$secret = $data['secret'];
				$tpn_id_receiver = $data['tpn_id_receiver'];
			}

			// TODO : Case return of sql query on secret and tpn_id_receiver is empty

			

			// Output in json :
			$output = array("secret" => $secret, "tpn_id" => $tpn_id_receiver);
			echo json_encode($output, JSON_PRETTY_PRINT);
			//echo SUCCESS;
		}
		else
		{
			echo REQUEST_TOO_OLD;
		}
		
	}
	else
	{
		echo INVALID_ADDRESS;
	}


?>
