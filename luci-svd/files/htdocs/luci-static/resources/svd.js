function av_update(id,num)
{
  var s=""
  var novalues=true;
  for (var i=1; i<=num; i++) {
    var curid=id+i;
    f=document.getElementById(curid);
    if (i>1) s=s+" ";
    if (f.value=="") {
      s=s+"0";
    } else {
      s=s+f.value;
      novalues=false;
    }
  }
  if (novalues) s="";
  document.getElementById(id).value=s;
  return true; 
}

function DbCopyList(flist,tlist)
{
  var s="";
  for(i=0; i<flist.options.length; i++) {
    if (i>0) s=s+" ";
    s=s+flist.options[i].value;
  }
  tlist.value=s;
}

function DbMove(fboxname,tboxname,selectedname)
{
  fbox=document.getElementById(fboxname);
  tbox=document.getElementById(tboxname);
  sbox=document.getElementById(selectedname);
  for(var i=0; i<fbox.options.length; i++) {
    if (fbox.options[i].selected)
    {
      var no = new Option();
      no.value = fbox.options[i].value;
      no.text = fbox.options[i].text;
      tbox.options.add(no);
    }
  }
  for(var i=fbox.options.length-1; i>=0; i--) {
    if (fbox.options[i].selected)
      fbox.options.remove(i);
  }
  if (fboxname.indexOf(".selected")>=0)
  {
    DbCopyList(fbox,sbox);
  } else
  {
     DbCopyList(tbox,sbox);
  }
}


function DbMoveUpDown(direction,selname,destname)
{
  sel = document.getElementById(selname);
  sbox = document.getElementById(destname);
  var i = sel.selectedIndex;
  var newi = i+direction;
  if (i>=0 && i<=(sel.options.length-1) && newi>=0 && newi <= (sel.options.length - 1))
  {
    var no = new Option();
    no.value = sel.options[newi].value;
    no.text = sel.options[newi].text;
    sel.options[newi].value = sel.options[i].value;
    sel.options[newi].text = sel.options[i].text;
    sel.options[i].value = no.value;
    sel.options[i].text = no.text;
    sel.selectedIndex = newi;
    DbCopyList(sel,sbox);
  }  	
}

function fa_update(id,num,enabled,disabled)
{
  var s=""
  for (var i=1; i<=num; i++) {
    var curid=id+i;
    f=document.getElementById(curid);
    if (i>1) s=s+" ";
    if (f.checked) {
      s=s+enabled;
    } else {
      s=s+disabled;
    }
  }
  document.getElementById(id).value=s;
  return true; 
}
