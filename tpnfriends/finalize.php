
<?php

	/***********************************************/
	/***   Last step : drop line used in db ********/
	/***********************************************/

include_once("echoes.php");
include_once("dbfunctions.php");

	// Tpn instance of sender posts id_request_2 and his tpn address

	if(isset($_POST['tpn_id']) && isset($_POST['id_request']))
	{

		$tpn_id_sender = $_POST['tpn_id'];
		$id_request_2 = $_POST['id_request'];

		try {
			$bdd = dbConnect();
		}
		catch (Exception $e)
		{
			die('Erreur : ' . $e->getMessage());
			return;
		}

	// Line is deleted

		$req = $bdd->prepare("DELETE FROM teapot.tpn_requests WHERE (id_request_2 = ? AND tpn_id_sender = ?);");
		$req->execute(array($id_request_2, $tpn_id_sender)) 
			or die(print_r($req->errorInfo())
			);

	}
	else
	{
		echo FAILURE;
	}

?>
