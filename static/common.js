function popup(url, name = "", redirect = "") 
{
	w = window.open(url, name, "menubar=no,status=no,scrollbars=no,menubar=no,width=310,height=500");
	if(window.focus) w.focus();
	if(redirect != "") document.location.href = redirect;
	return false;
}
