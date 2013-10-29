<?php


function dbConnect()
{
	return new PDO('mysql:host=localhost;dbname=teapot', 'teapot', 'hollande_has_a_big_gne_gne', array(PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8"));
}

?>
