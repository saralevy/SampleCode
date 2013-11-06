<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">

<head>
	<link rel="stylesheet" type="text/css" href="homeschool.css" />
	<link rel="stylesheet" type="text/css" href="sorterstyle.css" />
	<title>The Boston Area Homeschool Classes and Events Database</title>
	<meta charset="utf-8" />
	<link rel="stylesheet" href="http://code.jquery.com/ui/1.10.2/themes/smoothness/jquery-ui.css" />
	<script src="http://code.jquery.com/jquery-1.9.1.js"></script>
	<script src="http://code.jquery.com/ui/1.10.2/jquery-ui.js"></script>	
	<script type="text/javascript" src="hsb.js"></script>
	<script type="text/javascript" src="tablesorter.js"></script>
	<script type="text/javascript" src="jquery.tablesorter.js"></script> 
</head>

<body>
	<?php 
		include("getip.php");
		$incomingip = getip();
		$IPallowed = CheckIPaddress($incomingip);
		
		if ($IPallowed == 'Y')
		{
			include("nav.php");
			include("main.php");
			include("footer.php"); 
		}
		else
			include("banned.php");		
	?>
</body>
</html>
