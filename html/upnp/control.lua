-- Copyright (C) 2018 Anton Burdinuk
-- clark15b@gmail.com
-- http://xupnpd.org

services=
{
    cds = { schema='urn:schemas-upnp-org:service:ContentDirectory:1'            },
    cms = { schema='urn:schemas-upnp-org:service:ConnectionManager:1'           },
    msr = { schema='urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1'    }
}

prefix='<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/" xmlns:dlna="urn:schemas-dlna-org:metadata-1-0">'

postfix='</DIDL-Lite>'

alt_logo=false

function gettablesize(t)
--    return table.getn(t)
    return #t
end

function services.cds.GetSystemUpdateID()
    return {{'Id',update_id}}
end

function services.cds.GetSortCapabilities()
    return {{'SortCaps','dc:title'}}
end

function services.cds.GetSearchCapabilities()
    return {{'SearchCaps','upnp:class'}}
end

function services.cds.X_GetFeatureList()
    return {{'FeatureList',''}}
end

function services.cds.X_SetBookmark(s)
    return {}
end

function iter(t,from,total)
    local i = from

    local n = gettablesize(t)

    if total>0 then n=from+total end

    return function ()
        i = i + 1

        if i<=n then return t[i],i end
    end
end

function serialize(id,parent,o)
    if o.channels then
        return string.format('<container id="%s" childCount="%s" parentID="%s" restricted="true"><dc:title>%s</dc:title><upnp:class>object.container</upnp:class></container>',
            id,gettablesize(o.channels),parent,soap_escape_xml(o.name))
    else
        -- DLNA.ORG_PN=MPEG_TS_HD_NA;DLNA.ORG_OP=11;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01700000000000000000000000000000

        local logo=''

        if o['tvg-logo'] then
            if alt_logo then
                logo=string.format('<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN">%s</res>',o['tvg-logo'])
            else
                logo=string.format('<upnp:albumArtURI dlna:profileID="JPEG_TN">%s</upnp:albumArtURI>',o['tvg-logo'])
            end
        end

        return string.format('<item id="%s" parentID="%s" restricted="true"><dc:title>%s</dc:title><upnp:class>object.item.videoItem</upnp:class>%s<res size="-1" protocolInfo="http-get:*:video/mpeg:*">%s</res></item>',
            id,parent,soap_escape_xml(o.name),logo,o.url)
    end
end

function playlist2()
    local t={}

    local pls=playlist()

    for i,j in ipairs(pls) do
        for ii,jj in ipairs(j.channels) do
            if jj['group-title'] then
                for ss in string.gmatch(jj['group-title'],'([^;]+)') do
                    if not t[ss] then t[ss]={} end

                    table.insert(t[ss],jj)
                end
            end
        end
    end

    for i,j in pairs(t) do
        table.insert(pls,{ name=i, channels=j})
    end

    return pls
end

-- Закомментировать для поддержки group-title налету
playlist2=playlist

function services.cds.Browse(s)
    local id=(s.ObjectID or s.ContainerID).value

    local p=nil

    local parent=nil

    local found=0

    local total=0

    if s.BrowseFlag.value=='BrowseMetadata' then
        if id=='0' then
            p={ name='root', channels=playlist2() }

            parent=-1
        else
            local fid,oid=string.match(id,'(%d+)/(%d+)')

            if not fid then
                p=playlist2()[tonumber(id)]

                parent=0
            else
                p=playlist2()[tonumber(fid)].channels[tonumber(oid)]

                parent=fid
            end
        end
    else
        if id=='0' then
            p=playlist2()

            parent=0
        else
            if not string.find(id,'/') then
                p=playlist2()[tonumber(id)].channels

                parent=id
            end
        end
    end

    local tt={}

    table.insert(tt,prefix);

    if p.name then
        found=1 total=1
        table.insert(tt,serialize(id,parent,p))
    else
        total=gettablesize(p)


        for oo,ii in iter(p,tonumber(s.StartingIndex.value),tonumber(s.RequestedCount.value)) do
            found=found+1

            if parent==0 then
                table.insert(tt,serialize(ii,parent,oo))
            else
                table.insert(tt,serialize(parent..'/'..ii,parent,oo))
            end
        end

    end

    table.insert(tt,postfix);

    local ss=table.concat(tt)

    return {{'Result',soap_escape_xml(ss)}, {'NumberReturned',found}, {'TotalMatches',total}, {'UpdateID',update_id}}
end

function services.cds.Search(s)
    local from=tonumber(s.StartingIndex.value)
    local to=from+tonumber(s.RequestedCount.value)

    if to==from then to=from+1000000 end

    local found=0

    local total=0

    local tt={}

    table.insert(tt,prefix);

    for i,j in ipairs(playlist()) do
        for ii,jj in ipairs(j.channels) do
            total=total+1

            if total>from and total<=to then
                table.insert(tt,serialize(i..'/'..ii,i,jj))

                found=found+1
            end
        end
    end

    table.insert(tt,postfix);

    local ss=table.concat(tt)

    return {{'Result',soap_escape_xml(ss)}, {'NumberReturned',found}, {'TotalMatches',total}, {'UpdateID',update_id}}
end

function services.cms.GetCurrentConnectionInfo(s)
    return {{'ConnectionID',0}, {'RcsID',-1}, {'AVTransportID',-1}, {'ProtocolInfo',''},
        {'PeerConnectionManager',''}, {'PeerConnectionID',-1}, {'Direction','Output'}, {'Status','OK'}}
end

function services.cms.GetProtocolInfo()
    return {{'Sink',''}, {'Source','http-get:*:video/mpeg:*,http-get:*:video/mp2t:*'}}
end

function services.cms.GetCurrentConnectionIDs()
    return {{'ConnectionIDs',''}}
end

function services.msr.IsAuthorized(s)
    return {{'Result',1}}
end

function services.msr.RegisterDevice(s)
end

function services.msr.IsValidated(args)
    return {{'Result',1}}
end

function doit()
    local action=string.match(headers.SOAPACTION,'#([%w_]+)"$')

    local resp=services[args.s][action](soap_decode(content).Envelope.Body[action])

    local t={}

    table.insert(t,'<?xml version="1.0" encoding="utf-8"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body>')

    table.insert(t,'<u:'.. action ..'Response xmlns:u="'.. services[args.s].schema ..'">')

    if resp then
        for i,j in ipairs(resp) do
            table.insert(t,'<' .. j[1] .. '>');
            table.insert(t,j[2])
            table.insert(t,'</' .. j[1] .. '>');
        end
    end

    table.insert(t,'</u:' .. action .. 'Response>')

    table.insert(t,'</s:Body></s:Envelope>')

    return table.concat(t)
end

rc,resp=pcall(doit)

if not rc then
    trace(resp..'\n')

    resp='<?xml version="1.0" encoding="utf-8"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body><s:Fault><faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail><u:UPnPError xmlns:u="urn:schemas-upnp-org:control-1-0"><u:errorCode>501</u:errorCode><u:errorDescription>' .. soap_escape_xml(resp) .. '</u:errorDescription></u:UPnPError></detail></s:Fault></s:Body></s:Envelope>'
end

status(200,'OK', { 'Content-Length: ' .. string.len(resp), 'Content-Type: text/xml; charset="UTF-8"' } )

io.stdout:write(resp)

-- debug
if debug==1 then
    local sss=string.format('=== SOAP ===\n%s: %s [%s]\n\n%s\n\n%s\n============\n',
        env.peer_addr,headers.SOAPACTION or '',headers.USER_AGENT or '',
        string.sub(content,1,1024),
        string.sub(resp,1,1024))

    io.stderr:write(sss)
end

trace(string.format('%s: %s [%s]',env.peer_addr,headers.SOAPACTION or '',headers.USER_AGENT or ''))
