function popup(url, redirect = "") 
{
	w = window.open(url, "_blank", "status=0,toolbar=0,scrollbars=0,menubar=0,directories=0,resizeable=1,width=310,height=500");
	if(window.focus) w.focus();
	if(redirect != "") document.location.href = redirect;
	return false;
}
