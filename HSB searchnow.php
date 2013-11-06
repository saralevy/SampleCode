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
		include("nav.php");
		include("config.php");
	?>
	
	<div id="output">
	<h2 align="center">Upcoming Classes</h2>

	<?php
	$con = mysql_connect($server, $user, $password) or die('Sorry, could not connect to database server');
	mysql_select_db($database, $con) or die('Sorry, could not connect to database');
		
 	$newstartdate = date("Ymd", strtotime($_REQUEST["startDate"]));
 	$newenddate = date("Ymd", strtotime($_REQUEST["endDate"]));
	$templocation = $_REQUEST["FMlocation"];
	$templowerage = $_REQUEST["FMlowerage"]; 
	$tempupperage = $_REQUEST["FMupperage"]; 

	//tempclassname is the only field that needs to be validated, since it is the only field that can be entered by user.
	$regex = "'[^A-Za-z0-9*&#,/:()_,-]'"; 
	if (preg_match($regex, $_REQUEST["FMclassname"])) 
		$tempclassname = $_REQUEST["FMclassname"]; 
	else
		$tempclassname = 'ALL';
		
	// Date Checks: 
	//	requested start date is between startdate and enddate OR
	//  requested end date is between startdate and enddate
	$query = "SELECT * FROM hsevents WHERE 
		(((cast('$newstartdate' as DATE) >= startdate) AND (cast('$newstartdate' as DATE) <= enddate)) OR
		((cast('$newenddate' as DATE) >= startdate) AND (cast('$newenddate' as DATE) <= enddate))) AND
 		(name LIKE '%$tempclassname%' OR 'ALL'='$tempclassname') AND 	
 		(lowerage BETWEEN $templowerage AND $tempupperage OR
		upperage BETWEEN $templowerage AND $tempupperage) AND
		(location='$templocation' OR 'ALL'='$templocation') AND 
 		verified='Y' 
		ORDER BY startdate";
		
	$result = mysql_query($query) or die('Sorry, could not access the database at this time ');
	$SPACES = "</br><b>Description:</b>&nbsp;";
	
	if (mysql_num_rows($result) == 0)
	{
		echo "<h3>Sorry, there are no classes or events that meet your criteria.</h3>";
	} else
	{		
		ob_start();
		echo '<table id="myTable" class="tablesorter">';
		echo '<thead>'; 
		echo '<tr class="trclass">
			<th>Class Name</th>
			<th>Start</th>
			<th>End</th>
			<th>Location</th>
			<th>Lower Age</th>
			<th>Upper Age</th>
			<th>Contact</th>
			<th>Description</th>
		</tr>';
		echo '</thead>';
		echo '<tbody>';

		while($row=mysql_fetch_array($result, MYSQL_ASSOC))
		{
			$tempname=htmlentities($row[name]);
			$tempcontact=htmlentities($row[contact]);
			
			// The following 4 lines get around the problem of <br>, line break, being displayed.
			// First, replace <br> with \n
			// Run htmlentities to clean up input
			// Then replace \n back to the original <br>
			$tempdescription1=preg_replace('#<br\s*/?>#i', "\n", $row[description]);
			$tempdescription2=htmlentities($tempdescription1);
			$tempdescription=nl2br($tempdescription2);
			
			$tempbegdate = date("m-d-Y", strtotime($row[startdate]));
			$tempenddate = date("m-d-Y", strtotime($row[enddate]));

			echo "<tr>
				<td>$tempname</td>
				<td>$tempbegdate</td>
				<td>$tempenddate</td>
				<td>$row[location]</td>
				<td>$row[lowerage]</td>
				<td>$row[upperage]</td>	
				<td><a href=$tempcontact>$tempcontact</a></td>
				<td>$tempdescription</td>
				</tr>";
		}
			
		echo '</tbody>';			
		echo '</table>';			
		$classdata = ob_get_contents();
		ob_end_clean();
		?>
		
		<div class="more-less"> 
			<div class="more-block">
				<?php echo $classdata ?>
			</div>
		</div>
		<?php
	}
	?>
	</div>

<?php
	include("footer.php"); 
?>
	
</body>
</html>