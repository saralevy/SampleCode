<div id="output">
	<h2 align="center">Upcoming Classes</h2>

	<?php
	include ("config.php");
	$con = mysql_connect($server, $user, $password) or die('Sorry, could not connect to database server');
	mysql_select_db($database, $con) or die('Sorry, could not connect to database');

	$query = "SELECT * FROM hsevents WHERE CURDATE()<=enddate AND verified='Y' ORDER BY location";

	$result = mysql_query($query) or die('Sorry, could not access the database at this time ');
	$SPACES = "</br><b>Description:</b>&nbsp;";

	if (mysql_num_rows($result) == 0)
	{
		echo "<h3>Sorry, there are no classes or events that meet your criteria.</h3>";
	} else
	{
		while($row=mysql_fetch_array($result, MYSQL_ASSOC))
		{
			$tempname=htmlentities($row[name]);
			$tempcontact=htmlentities($row[contact]);
			
			// The following 3 lines get around the problem of <br>, line break, being displayed.
			// First, replace <br> with \n
			// Run htmlentities to clean up input, then replace \n back to the original <br>
			$tempdescription1=preg_replace('#<br\s*/?>#i', "\n", $row[description]);
			$tempdescription2=htmlentities($tempdescription1);
			$tempdescription=nl2br($tempdescription2);
		
			$tempbegdate = date("m-d-Y", strtotime($row[startdate]));
			$tempenddate = date("m-d-Y", strtotime($row[enddate]));
							
			ob_start();
			echo '<table>
			<tr class="trclass">
				<th class="namecolumn">Class Name</th>
				<th class="datecolumn">Date</th>
				<th class="locationcolumn">Location</th>
				<th class="agecolumn">Age</th>
				<th class="contactcolumn">Contact</th>
			</tr>';
			
			echo "<tr>
				<td>$tempname</td>
				<td>$tempbegdate to $tempenddate</td>
				<td>$row[location]</td>
				<td>$row[lowerage]-$row[upperage]</td>
				<td>$tempcontact</br></td>
				</tr>";
				
			echo "<tr><td colspan=5>$SPACES $tempdescription</td></tr>";
			
			echo '</table>';
			$classdata = ob_get_contents();
			ob_end_clean();
			//Jtable.org and tablesorter.com
			?>
			
			<div class="more-less"> 
				<div class="more-block">
					<?php echo $classdata ?>
				</div>
			</div>
			<hr>
			<?php
		}
	}
	?>
</div>