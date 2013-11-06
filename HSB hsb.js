/*$(function(){
	// Sets the .more-block div to the specified height and hides any content that overflows
	$(".more-less .more-block").css('height', 60).css('overflow', 'hidden');

	// The section added to the bottom of the "more-less" div,  [&hellip;]
	$(".more-less").append('<p class="continued"></p><a href="#" class="adjust"></a>');

	$("a.adjust").text("+  More");

	$(".adjust").toggle(function() {
			$(this).parents("div:first").find(".more-block").css('height', 'auto').css('overflow', 'visible');
			$(this).parents("div:first").find("p.continued").css('display', 'none');
			$(this).text("- Less");
		}, function() {
			$(this).parents("div:first").find(".more-block").css('height', 60).css('overflow', 'hidden');
			$(this).parents("div:first").find("p.continued").css('display', 'block');
			$(this).text("+  More");
	});
});
*/
$(function(){

	$("#fromDate").datepicker({
	defaultDate: "+1w",
	changeMonth: true,
	numberOfMonths: 1,
		onClose: function( selectedDate ) {
		$( "#toDate" ).datepicker( "option", "minDate", selectedDate );
		}
	});
	
	$("#toDate").datepicker({
	defaultDate: "+1w",
	changeMonth: true,
	numberOfMonths: 1,
		onClose: function( selectedDate ) {
		$( "#fromDate" ).datepicker( "option", "maxDate", selectedDate );
		}
	});
});