<?php
	function getIP()
	{
		if (isset($_SERVER['HTTP_X_FORWARDED_FOR']))
		{
			// Use the forwarded IP address, typically set when the client is using a proxy server.
			// Format: "X-Forwarded-For: client1, proxy1, proxy2"
			$client_ips = explode(',', $_SERVER['HTTP_X_FORWARDED_FOR']);
			$ip = array_shift($client_ips);
			unset($client_ips);
		}
		elseif (isset($_SERVER['HTTP_CLIENT_IP']))
		{
			// Use the forwarded IP address, typically set when the client is using a proxy server.
			$client_ips = explode(',', $_SERVER['HTTP_CLIENT_IP']);
			$ip = array_shift($client_ips);
			unset($client_ips);
		}
		elseif (isset($_SERVER['REMOTE_ADDR']))
		{
			// The remote IP address
			$ip = $_SERVER['REMOTE_ADDR'];
		}
		else
			$ip = "UNKNOWN";
			
		return $ip;
	}
		
	function CheckIPaddress($ip)
	{
		include ("config.php");
		
		$con = mysql_connect($server, $user, $password) or die('Sorry, could not connect to database server');
		mysql_select_db($database, $con) or die('Sorry, could not connect to database');					
		$query = "SELECT allowed FROM hsipaddresses WHERE ipaddress='$ip'";
		$result = mysql_query($query, $con) or die('Sorry, could not access the database at this time ');
		
		//no match so OK to proceed
		if (mysql_num_rows($result) == 0)
			return 'Y';  
		else
		{
			$row=mysql_fetch_array($result, MYSQL_ASSOC);
			return $row[allowed];
		}
	} 
?>
