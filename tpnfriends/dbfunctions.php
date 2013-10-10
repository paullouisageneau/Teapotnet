<?php


function dbConnect()
{
	return new PDO('mysql:host=localhost;dbname=teapot', 'teapot', 'planques_toujours_planques', array(PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8"));
}

?>
