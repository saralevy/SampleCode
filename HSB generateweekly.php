<?php

function generateweeklyreport()
{
	// create email distribution list on the fly for weekly email
	include ("config.php");
	$con = mysql_connect($server, $user, $password) or die('Sorry, could not connect to database server');
	mysql_select_db($database, $con) or die('Sorry, could not connect to database');

	//set subject
	$subject = 'Homeschoolboston Upcoming Classes and Events';

	//create body of email for weekly email listing
	$query = "SELECT * FROM hsevents WHERE(CURDATE() >= startdate OR startdate <= DATE_ADD( CURDATE() , INTERVAL 1 WEEK )) 
	AND CURDATE() < enddate AND verified='Y' AND FYIonly='N' ORDER BY startdate";
	
	$result = mysql_query($query);
	$SPACES = "</br><b>Description:</b>&nbsp;";
	$YLINE = "<hr>";
		
	if (mysql_num_rows($result) == 0)
	{
		$message = "There are no homeschoolboston.com classes or events listed for this coming week.";
	} else
	{	
		$today = date("F j, Y");
		$message .= "<html><table><tr><b><h3>Upcoming Classes and Events for the week of $today:</h3></b></tr></table></html>";
		
		while($row=mysql_fetch_array($result, MYSQL_ASSOC))
		{			
			$tempbegdate = date("m-d-Y", strtotime($row[startdate]));
			$tempenddate = date("m-d-Y", strtotime($row[enddate]));

			// The following 3 lines get around the problem of <br>, line break, being displayed.
			// First, replace <br> with \n
			// Run htmlentities to clean up input
			// Then replace \n back to the original <br>
			$tempdescription1=preg_replace('#<br\s*/?>#i', "\n", $row[description]);
			$tempdescription2=htmlentities($tempdescription1);
			$tempdescription=nl2br($tempdescription2);

			$message .= "<html>
				<tr>
					<td>Name: $row[name]</br></td>
					<td>Dates: $tempbegdate to $tempenddate</br></td>
					<td>Location:$row[location]</br></td>
					<td>Age Range: $row[lowerage]-$row[upperage]</br></td>
					<td>Contact: $row[contact]</br></td>
				</tr>
				<tr>
					<td colspan=5>$SPACES $tempdescription $YLINE</td>
				</tr>
			</html>";
		}
		$message .= "<html></table></html>";				
	}
	
	$headers  = 'MIME-Version: 1.0' . "\r\n";
	$headers .= 'Content-type: text/html; charset=iso-8859-1' . "\r\n";
	$headers .= 'From: homeschoolboston.com <info@homeschoolboston.com>' . "\r\n";
	
	//create distribution list for weekly email listing
	$query = "SELECT * FROM hsmaillist WHERE weeklyemail='Y'";
	$result = mysql_query($query);
	$num_rows = mysql_num_rows($result);
 
	$i=1;
	$max_email_addresses=25;
	while($row=mysql_fetch_array($result, MYSQL_ASSOC))
	{
		if (($i <= 24) and ($i < $num_rows)) {
			$to .= $row[address] . ", ";
			$i++;
		} else {
			// if the limit of 25 email addresses has been reached or we have reached
			// the last row of data, end the $to field by concating last email address,
			// send mail and reset $num_rows, $i and $to 
			$to .= $row[address];
			$headers .= 'bcc: ' . $to . "\r\n";
			try {
				mail("info@homeschoolboston.com", $subject, wordwrap($message, 70), $headers);
			} catch (Exception $e) {
				echo ("ERROR: Email NOT sent, Exception: " . $e->getMessage());
			}
			echo "<br />Weekly email sent to: " . $to . "<br />";
			$num_rows = $num_rows - $i;
			$i=1;
			$to = "";
		} 
	}	
}
?>
