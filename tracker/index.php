<?php

	$host = 'localhost';
	$dbname = 'tpntracker';
	$user = 'tpntracker';
	$password = 'bigbrother'; /**** Change database password here *****/
	$root = 'root';
	$root_password = '';
	$enableLogs = false; /**** Set this to true if you want logs (make sure script has write permission on folder) ****/
	$sendStats = true; /**** Set this to false if you don't want to send stats to teapotnet.org. Stats are only used to help develop Teapotnet (see paragraph "Stats" below) ****/


	// Config page
	// TODO : CSS ?
	if(isset($_GET['config']))
	{

		?>
			<div>This is Teapotnet's tracker config page.</div>

			<div>
			<?php
			$isConfigOK = true;

			if (!defined('PDO::ATTR_DRIVER_NAME')) 
			{
				?>PDO does not seem to be installed.<br>You can check out <a href="http://www.php.net/manual/en/pdo.installation.php">the PHP documentation</a> to learn how to install PDO.

				<?php
				$isConfigOK = false;
			}
			else
			{
				?>Check if PDO is installed : OK<br> <?php
			}

			// Check if database is correctly install, if not propose to start setup over.
			try
			{
				$db = new PDO('mysql:host='.$host.';dbname='.$dbname, $user, $password);
				?>Connection to database : OK<br><?php
			}
			catch (Exception $e)
			{
				$isConfigOK = false;
				// TODO : on form address, case when http://servername doesn't work
				?>
				Can't connect to database. It might not have been created<br>
				<form name="password" action="<?php echo 'http://'.$_SERVER['SERVER_NAME'].'/tracker/' ?>" method="post" enctype="application/x-www-form-urlencoded">
					<h2>Enter root password</h2>
					<p>Please enter your MySQL root password to automatically create database and user for TeapotNet tracker. Password will not be stored.</p>
					<label for="root_password">root_password</label>
					<input type="password" name="root_password" value="">
					<br>
					<input type="submit" name="submit_pass" value="Create database">
				</form>

			</div>
				<?php
				return;
			}

		if($isConfigOK)
			print 'Your tracker configuration seems perfectly functional.<br>';
			print 'tracker : '.$_SERVER['SERVER_NAME'].'<br>';

		return;
	}

	if(isset($_POST['root_password']))
	{
		$root_password = $_POST['root_password'];
		
		// Try to create db
		if(initDatabase($user, $password, $dbname, $host, $root, $root_password))
		{
			print 'Database '.$dbname.' and user '.$user.' were created.';
			logWrite('Database '.$dbname.' and user '.$user.' were created.', $enableLogs);
		}

		exit;
	}

	try
	{
		$db = new PDO('mysql:host='.$host.';dbname='.$dbname, $user, $password);
	}
	catch (Exception $e)
	{
		// Database creations if not exist
		if(initDatabase($user, $password, $dbname, $host, $root, $root_password))
		{
			print 'Database '.$dbname.' and user '.$user.' were created.<br>';
			logWrite('Database '.$dbname.' and user '.$user.' were created.', $enableLogs);
		}

		// Retry and die if not successful
		try
		{
			$db = new PDO('mysql:host='.$host.';dbname='.$dbname, $user, $password);
		}
		catch (Exception $e)
		{
			print "Could not connect to database ".$dbname."<br>";
			print "Please click on : <a href='http://".$_SERVER["SERVER_NAME"]."/tracker/?config'>".$_SERVER["SERVER_NAME"]."/tracker/?config</a> to enter setup.";
			exit;
		}

		print "Connection to database ".$dbname." successful.<br>";
	}

	// Table creations if not exist
	createTables($db);


	/***************************************** STATS ****************************************/
	/* The stats that are computed are : number of distinct IP addresses that posted	*/
	/* an identifier to the tracker ; number of distinct identifiers posted to tracker.	*/
	/* It is impossible to find a user or its activity on Teapotnet with these stats.	*/
	/* By default, stats are sent once a day to teapotnet.org. It helps us improve		*/
	/* Teapotnet, but you can disable sending by setting parameter $sendStats		*/
	/* to false (at the beginning of this script).						*/
	/****************************************************************************************/

	if($sendStats)
		sendStats($db);

	if(isset($_GET['stats']))
	{
		echo "<h1>Stats for tracker : ".$_SERVER["SERVER_NAME"]."</h1>";

		displayStats($db);

		exit;
	}

	/************************************** END OF STATS ***********************************/

	$identifier = '';
	if(isset($_GET['id']))
	{
		$identifier = $_GET['id'];

		// Log
		logWrite('Correct GET request with id : '.$identifier, $enableLogs);

	}
	else
	{
		// Just for tests :
		if(isset($_GET['testid']))
			print retrieve($db, false, $_GET['testid'], $enableLogs);

		// Log
		logWrite('GET request without id', $enableLogs);

		return;
	}

	if(strlen($identifier) != 128)
	{
		logWrite('Incorrect identifier size : '.strlen($identifier).' ('.$identifier.')', $enableLogs);
		return;
	}

	$alternate = false;

	if(isset($_GET['alternate']))
		$alternate = true;


	if($_SERVER['REQUEST_METHOD'] === 'POST')
	{
		logWrite('POST request received', $enableLogs);

		if(isset($_POST['instance']))
		{
			$instance = $_POST['instance'];
		}

		// If instance is empty, set name to "default"
		if($instance == "")
			$instance = "default";

		$count = 0; // TODO : probably useless variable now

		$port;
		if(isset($_POST['port']))
		{
			$port = $_POST['port'];

			$host;
			if(isset($_POST['host']))
			{
				$host = $_POST['host'];
			}
			else
			{
				if(array_key_exists('HTTP_X_FORWARDED_FOR', $_SERVER))
				{
					$host = $_SERVER['HTTP_X_FORWARDED_FOR'];
					logWrite('Request received through proxy for : '.$_SERVER['HTTP_X_FORWARDED_FOR'], $enableLogs);
				}
				else
				{
					$host = $_SERVER['REMOTE_ADDR'];
					logWrite('Request comes from : '.$_SERVER['REMOTE_ADDR'], $enableLogs);
				}
			}

			$addr = $host.':'.$port;

			// No insertion of already contained peering<->addresses
			if(retrieveCheck($db, false, $identifier, $addr) == '')
			{
				insert($db, false, $identifier, $instance, $addr);
				logWrite('Insert into storage : '.$identifier.' ; '.$addr, $enableLogs);
			}
			else
			{
				updateDate($db, false, $identifier, $addr);
				logWrite('Already found, not inserting : '.$identifier.' ; '.$addr, $enableLogs);
			}
			// Alternate addresses should not be copied in table stats
			if(!$alternate)
				insertToday($db, $identifier, $instance, $addr, $enableLogs);

			$count = $count+1;
		}


		$addresses;
		if(isset($_POST['addresses']))
		{
			$addresses = $_POST['addresses'];

			// Explode addresses and store in array
			$array_addresses = explode(",", $addresses);
			
			// For all addresses in $array_adresses, insert in db "alternate" or "storage"
			if(count($array_addresses) > 0)
			{
				for($i=0; $i<count($array_addresses); $i++)
				{
					// Avoid too much cleaning by checking if address and peering already exist
					if(retrieveCheck($db, $alternate, $identifier, $array_addresses[$i]) == '')
					{
						insert($db, $alternate, $identifier, $instance, $array_addresses[$i]);
						logWrite('insert in '.$identifier.' ; '.$array_addresses[$i].' (alternate='.$alternate.')', $enableLogs);
					}
					else
					{
						updateDate($db, $alternate, $identifier, $array_addresses[$i]);
						logWrite('Already found, not inserting : '.$identifier.' ; '.$addr, $enableLogs);
					}

					// Alternate addresses should not be copied in table stats
					if(!$alternate)
						insertToday($db, $identifier, $instance, $array_addresses[$i], $enableLogs);
				}
			}

			$count++;
		}

		if($alternate == false && isset($_POST['alternate']))
		{
			$addresses = $_POST['alternate'];
			$array_addresses = explode(",", $addresses);
			
			// For all addresses in $array_adresses, insert in db "alternate" or "storage"
			if(count($array_addresses) > 0)
			{
				for($i=0; $i<count($array_addresses); $i++)
				{
					// Avoid too much cleaning by checking if address and peering already exist
					if(retrieveCheck($db, true, $identifier, $array_addresses[$i]) == '')
					{
						insert($db, true, $identifier, $instance, $array_addresses[$i]);
						logWrite('insert in '.$identifier.' ; '.$array_addresses[$i].' (alternate='.$alternate.')', $enableLogs);
					}
					else
					{
						updateDate($db, true, $identifier, $array_addresses[$i]);
						logWrite('Already found, not inserting : '.$identifier.' ; '.$addr, $enableLogs);
					}

					// No insertToday because alternate addresses should not be copied in table stats
				}
			}
			
		}

		clean($db, $enableLogs);
		exit;
	}
	else
	{
		$serialized_results = retrieve($db, $alternate, $identifier);
		logWrite('Result of retrieve : '.$serialized_results.', looking for :'.$identifier, $enableLogs);

		header('Vary: Accept-Encoding');
		header('Content-Type: text/plain');
		echo $serialized_results;
		logWrite('Echo done... exiting.', $enableLogs);

		exit;
	}

/***************************************************/
/***************** Begin functions *****************/
/***************************************************/

function logWrite($string, $enableLogs)
{
	// TODO : case if tracker does not have 'w' rights on folder
	if($enableLogs)
	{
		$logFile = 'log.txt';
		$ipaddr = $_SERVER['REMOTE_ADDR'];
		$now = date('Y-m-d H:i:s');

		file_put_contents($logFile, $ipaddr.' ; '.$now.' ; '.$string.PHP_EOL ,FILE_APPEND);
	}
}

function clean($db, $enableLogs)
{
	// Clean on both tables : delay is 15 minutes (could be 10 minutes in fact because posts to tracker are made each 5 minutes)
	for($j = 1; $j <= 2; $j++)
	{
		switch($j)
		{
			case 1:
				$table="alternate";
				break;

			case 2:
				$table="storage";
				break;

			default:
				$table="storage";
				break;
		}

		try {
			$results1 = $db->prepare("DELETE FROM ".$table." WHERE time < DATE_SUB(NOW(), INTERVAL 15 MINUTE);");
			$results1->execute(array());
		}
		catch(PDOException $ex) 
		{
			print $ex->getMessage();
			logWrite($ex->getMessage(), $enableLogs);
		}
	}
	
	logWrite("Clean was made.", $enableLogs);
}

function insert($db, $alternate, $identifier, $instance, $address)
{
	if($alternate)
		$table="alternate";
	else
		$table="storage";

	try {
		$results = $db->prepare("INSERT INTO ".$table." VALUES(null, ?, ?, ?, NOW());");
		$results->execute(array($identifier, $instance, $address));
	}
	catch(PDOException $ex) 
	{
		print $ex->getMessage();
		logWrite($ex->getMessage(), $enableLogs);
	}
}

function retrieve($db, $alternate, $identifier)
{
	if($alternate)
		$results = $db->prepare("SELECT DISTINCT address, instance FROM alternate WHERE(identifier=? AND address!='');");
	else
		$results = $db->prepare("SELECT DISTINCT address, instance FROM storage WHERE(identifier=? AND address!='');");

	$results->execute(array($identifier));

	$string_result = "\r\n";
	$instance = '';
	$instance_backup=''; // Useful for multiple instances
	while ($row = $results->fetch()) {
		$instance = $row['instance'];

		if($instance !== $instance_backup)
		{
			$string_result.=$instance.":"."\r\n";
		}

		$instance_backup = $instance;
		$string_result.="\t- ".$row['address']."\r\n";
	}

	if($instance === '')
		return '';

	// TODO : cleaner yaml emit function
	//return "\r\n".$instance.":"."\r\n".$string_result."\r\n";
	return $string_result."\r\n";
}

function retrieveCheck($db, $alternate, $identifier, $address)
{
	if($alternate)
		$results = $db->prepare("SELECT DISTINCT address, instance FROM alternate WHERE(identifier=? AND address=?);");
	else
		$results = $db->prepare("SELECT DISTINCT address, instance FROM storage WHERE(identifier=? AND address=?);");

	$results->execute(array($identifier, $address));

	$string_result = '';
	$instance = '';
	while ($row = $results->fetch()) {
		$instance = $row['instance'];
		$string_result.="\t- ".$row['address']."\r\n";
	}

	if($instance === '')
		return '';

	// TODO : cleaner yaml emit function
	return "\r\n".$instance.":"."\r\n".$string_result."\r\n";
}

function updateDate($db, $alternate, $identifier, $address)
{
	if($alternate)
		$table="alternate";
	else
		$table="storage";

	try {
		$results = $db->prepare("UPDATE ".$table." SET time=NOW() WHERE(identifier=? AND address=?);");
	}		
	catch(PDOException $ex) 
	{
		print $ex->getMessage();
		logWrite($ex->getMessage(), $enableLogs);
	}

	$results->execute(array($identifier, $address));
}


function initDatabase($user, $password, $dbname, $host, $root, $root_password)
{
	// User and database creation
	try {
		$dbh = new PDO("mysql:host=$host", $root, $root_password);

		$dbh->exec("CREATE DATABASE `$dbname`;
			CREATE USER '$user'@'localhost' IDENTIFIED BY '$password';
			GRANT ALL ON `$dbname`.* TO '$user'@'localhost';
			FLUSH PRIVILEGES;") 
		or die(print_r($dbh->errorInfo(), true)); // TODO : more user-friendly error display

		return true;

	} catch (PDOException $e) {
		//die("Error in user and database creation : ". $e->getMessage());
		//print "Error in user and database creation : ". $e->getMessage();
		return false;
	}

	return false;
}

function selectToday($db, $identifier, $address)
{
	$results = $db->prepare("SELECT DISTINCT identifier, address, time FROM stats WHERE(identifier=? AND address=? AND time >= CURDATE());");

	$results->execute(array($identifier, $address));

	// TODO : if empty
	while ($row = $results->fetch()) {
		$time = $row['time'];
	}

	return $time;
}

function insertToday($db, $identifier, $instance, $address, $enableLogs)
{
	if(selectToday($db, $identifier, $address) == '')
	{
		try {
			$results = $db->prepare("INSERT INTO stats VALUES(null, ?, ?, ?, NOW());");
			$results->execute(array($identifier, $instance, $address));
		}
		catch(PDOException $ex) 
		{
			print $ex->getMessage();
			logWrite($ex->getMessage(), $enableLogs);
		}
		logWrite('Insertion in table stats made : '.$identifier.' ; '.$address , $enableLogs);
	}
}

function createTables($db)
{
	try {
		$results1 = $db->query('CREATE TABLE IF NOT EXISTS storage(reqid INT PRIMARY KEY NOT NULL AUTO_INCREMENT, identifier VARCHAR(128), instance VARCHAR(50), address VARCHAR(45), time DATETIME);');
	}
	catch(PDOException $ex) 
	{
		print $ex->getMessage();
		logWrite($ex->getMessage(), $enableLogs);
	}	

	try {
		$results2 = $db->query('CREATE TABLE IF NOT EXISTS alternate(reqid INT PRIMARY KEY NOT NULL AUTO_INCREMENT, identifier VARCHAR(128), instance VARCHAR(50), address VARCHAR(45), time DATETIME);');
	}
	catch(PDOException $ex) 
	{
		print $ex->getMessage();
		logWrite($ex->getMessage(), $enableLogs);
	}

	try {
		$results3 = $db->query('CREATE TABLE IF NOT EXISTS stats(reqid INT PRIMARY KEY NOT NULL AUTO_INCREMENT, identifier VARCHAR(128), instance VARCHAR(50), address VARCHAR(45), time DATETIME);');
	}
	catch(PDOException $ex) 
	{
		print $ex->getMessage();
		logWrite($ex->getMessage(), $enableLogs);
	}
}

function checkStatsSent($db, $date)
{
	$statsSent = false;
	$today = date('Y-m-d 00:00:00');

	// Check if marker is present in db == if stats have been sent to teapotnet.org
	$results = $db->prepare("SELECT DISTINCT instance FROM stats WHERE(instance='STATS_SENT' AND time = CURDATE()-".daysBetween($date, $today).");");

	$results->execute(array());

	$marker = '';
	while ($row = $results->fetch()) {
		$marker = $row['instance'];
	}

	if($marker == 'STATS_SENT')
		$statsSent = true;

	return $statsSent;
}

function insertStatsSentMarker($db, $date)
{
	$today = date('Y-m-d 00:00:00');
	$marker = "STATS_SENT";

	// Marker is inserted in instance field, so that there are no interferences with stats made on nidentifiers or naddresses
	if(daysBetween($date, $today) <= 10000) // Just to be sure nothing stupid is going on
	{
		try {
			$results = $db->prepare("INSERT INTO stats VALUES(null, null, ?, null, CURDATE()-".daysBetween($date, $today).");");
			$results->execute(array($marker));
		}
		catch(PDOException $ex) 
		{
			print $ex->getMessage();
			logWrite($ex->getMessage(), $enableLogs);
		}
	}
	
}

// TODO : potentially (?) poor performances for big trackers having been active for a long time, as marker is searched for every day
function sendStats($db)
{
	$oldest = getOldestDate($db);
	$today = date('Y-m-d 00:00:00');

	$daysBetween = daysBetween($oldest, $today);
	
	// Do not send stats for today (today is not finished)
	for($j = $daysBetween; $j >= 1; $j--)
	{
		$d = dateBefore($j);
		if(!checkStatsSent($db, $d))
		{
			$naddresses = nAddresses($db, $d);
			$nidentifiers = nIdentifiers($db, $d);
			// Send
			$trackername = $_SERVER['SERVER_NAME'];

			$data = array(
				'date'      => $d,
				'tracker'    => $trackername,
				'naddresses'       => $naddresses,
				'nidentifiers' => $nidentifiers,
			);

			$json = json_encode($data);

			// TODO : change address is file is moved
			$url = 'https://teapotnet.org/rapture/statsrecv.php';

			// must use key 'http' even if request is sent the request via HTTPS
			$options = array(
			    'http' => array(
				'header'  => "Content-type: application/x-www-form-urlencoded\r\n",
				'method'  => 'POST',
				'content' => http_build_query($data),
			    ),
			);
			$context  = stream_context_create($options);
			$result = file_get_contents($url, false, $context);

			// If stats correctly received, mark that stats have been sent for date $d
			if($result == "200")
				insertStatsSentMarker($db, $d);
		}
	}
}

function displayStats($db)
{
	$oldest = getOldestDate($db);
	$today = date('Y-m-d 00:00:00');

	$daysBetween = daysBetween($oldest, $today);
	
	echo "<table border>";
	echo "<tr><td>Date</td>";
	echo "<td>#Addresses</td>";
	echo "<td>#Identifiers</td></tr>";

	for($j = $daysBetween; $j >= 1; $j--)
	{
		$d = dateBefore($j);
		$naddresses = nAddresses($db, $d);
		$nidentifiers = nIdentifiers($db, $d);
		echo "<tr><td>".$d."</td>";
		echo "<td>".$naddresses."</td>";
		echo "<td>".$nidentifiers."</td></tr>";
	}
	echo "</table>";
}

function getOldestDate($db)
{
	$oldest = date('Y-m-d 00:00:00'); // Today by default

	$results = $db->query("SELECT time FROM stats WHERE(time > 0) ORDER BY time ASC LIMIT 1;");

	while ($row = $results->fetch())
	{
		$oldest = $row['time'];
	}

	return $oldest;
}

function daysBetween($day1, $day2)
{
	$d1 = strtotime($day1);
	$d2 = strtotime($day2);

	return ceil(abs($d2 - $d1) / 86400);
}

function dateBefore($daysBeforeToday) // In format Y-m-d 00:00:00
{
	$today = date('Y-m-d 00:00:00');
	$dt = strtotime($today);

	return date('Y-m-d H:i:s', $dt-$daysBeforeToday*86400);
}

function nAddresses($db, $date)
{
	return nAddressesBetween($db, $date, $date);
}

function nAddressesBetween($db, $d1, $d2)
{
	$count = -1;

	$today = date('Y-m-d 00:00:00');
	$dt = strtotime($today);

	$daysBetween1 = daysBetween($today, $d1);
	$daysBetween2 = daysBetween($today, $d2)-1;

	$stat_results = $db->query("SELECT COUNT(*) as count FROM (SELECT DISTINCT address FROM stats WHERE(time >= CURDATE()-".$daysBetween1." AND time < CURDATE()-".$daysBetween2." AND instance != 'STATS_SENT')) as sub;");

	while ($row = $stat_results->fetch()) 
	{
		$count = $row['count']; 
	}

	return $count;

}

function nIdentifiers($db, $date)
{
	return nIdentifiersBetween($db, $date, $date);
}

function nIdentifiersBetween($db, $d1, $d2)
{
	$count = -1;

	$today = date('Y-m-d 00:00:00');
	$dt = strtotime($today);

	$daysBetween1 = daysBetween($today, $d1);
	$daysBetween2 = daysBetween($today, $d2)-1;

	$stat_results = $db->query("SELECT COUNT(*) as count FROM (SELECT DISTINCT identifier FROM stats WHERE(time >= CURDATE()-".$daysBetween1." AND time < CURDATE()-".$daysBetween2." AND instance != 'STATS_SENT')) as sub;");

	while ($row = $stat_results->fetch()) 
	{
		$count = $row['count'];
	}

	return $count;
}

?>
