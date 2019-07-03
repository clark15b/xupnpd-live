function getXmlHttp()
{
    var xmlhttp;

    try
    {
        xmlhttp=new ActiveXObject("Msxml2.XMLHTTP");
    }catch (e)
    {
        try
        {
            xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
        }
        catch (E)
        {
            xmlhttp=false;
        }
    }

    if(!xmlhttp && typeof XMLHttpRequest!='undefined')
    {
        xmlhttp=new XMLHttpRequest();
    }

    return xmlhttp;
}

function removeOptions(selectbox)
{
    var i;

    for(i=selectbox.options.length-1;i>=0;i--)
    {
        selectbox.remove(i);
    }
}

function scan()
{
    var req=getXmlHttp();

    var players=document.getElementById('players');

    var refresh=document.getElementById('refresh');

    req.onreadystatechange=function()
    {
        if(req.readyState==4 && req.status==200)
        {
            var s=JSON.parse(req.responseText)

            if(s)
            {
                s.forEach(
                    function(item,i,s)
                    {
                        var option=document.createElement("option");
                        option.text=item.name;
                        option.value=item.url;
                        players.add(option);
                    }
                );
            }

            refresh.disabled=false;
        }
    }

    removeOptions(players);
    refresh.disabled=true;

    req.open('GET','/api.lua?action=search',true);

    req.send(null);
}

function getTags()
{
    var req=getXmlHttp();

    var tags=document.getElementById('tags');

    req.onreadystatechange=function()
    {
        if(req.readyState==4 && req.status==200)
        {
            var s=JSON.parse(req.responseText)

            var ss=""

            s.forEach(
                function(item,i,s)
                {
                    ss=ss.concat("<a href='#' onclick='return getChannels(",item.id,")'>#",item.name,"</a> ");
                }
            );

            tags.innerHTML=ss;
        }
    }

    tags.innerHTML="";

    req.open('GET','/api.lua?action=tags',true);

    req.send(null);
}

function getChannels(id)
{
    var req=getXmlHttp();

    var channels=document.getElementById('channels');

    req.onreadystatechange=function()
    {
        if(req.readyState==4 && req.status==200)
        {
            var s=JSON.parse(req.responseText)

            var ss=""

            s.forEach(
                function(item,i,s)
                {
                    ss=ss.concat("<a href='",item.url,"' onclick='return play(this)'>",item.name,"</a><br>");
                }
            );

            channels.innerHTML=ss;
        }
    }

    channels.innerHTML="none";

    req.open('GET','/api.lua?action=channels&id='+id,true);

    req.send(null);
}

function play(obj)
{
    var players=document.getElementById('players');

    var dst=players.options[players.selectedIndex];

    if(!dst)
        { alert("Плеер не выбран"); return false; }

    var req=getXmlHttp();

    req.onreadystatechange=function()
    {
        if(req.readyState==4 && req.status==200)
        {
            alert("Воспроизводится:\n'"+obj.text+"' на '"+dst.text+"'");
        }
    }

    req.open('POST','/api.lua?action=play',true);

    req.send(JSON.stringify({dst:dst.value, url:obj.href, name:obj.text}));

    return false;
}

function stop(obj)
{
    var players=document.getElementById('players');

    var dst=players.options[players.selectedIndex];

    if(!dst)
        { alert("Плеер не выбран"); return false; }

    var req=getXmlHttp();

    req.onreadystatechange=function()
    {
        if(req.readyState==4 && req.status==200)
        {
        }
    }

    req.open('POST','/api.lua?action=stop',true);

    req.send(JSON.stringify({dst:dst.value}));

    return false;
}
