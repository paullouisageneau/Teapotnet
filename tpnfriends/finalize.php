
<?php

	/***********************************************/
	/***   Last step : drop line used in db ********/
	/***********************************************/

include_once("echoes.php");
include_once("dbfunctions.php");

	// Tpn instance of sender posts id_request_2, his tpn address and his friend's address


		try {
			$bdd = dbConnect();
		}
		catch (Exception $e)
		{
			die('Erreur : ' . $e->getMessage());
			return;
		}

	// Line is deleted

		$req = $bdd->prepare("DELETE FROM teapot.tpn_requests WHERE (id_request_2 = ? AND tpn_id_sender = ? AND tpn_id_receiver = ?);");
		$req->execute(array($id_request_2, $tpn_id_sender, $tpn_id_receiver)) 
			or die(print_r($req->errorInfo())
			);


?>
