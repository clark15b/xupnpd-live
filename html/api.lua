-- Copyright (C) 2018 Anton Burdinuk
-- clark15b@gmail.com
-- http://xupnpd.org

function tr(o,t)
    for i,j in ipairs(o) do
        local name=string.match(j.name,':(.+)$') or j.name

        local ntab={}

        if not t[name] then
            t[name]=ntab table.insert(ntab,ntab)
        else
            table.insert(t[name],ntab)
        end

        ntab.attr=j.attr ntab.value=j.value

        tr(j,ntab)
    end
end

function xml_transform(root) tbl={} tr(root,tbl) return tbl end

function concat_url(base,url)
    if string.find(url,'^http://') then
        return url
    elseif string.find(url,'^/') then
        return string.match(base,'^(%w+://[^/]+)') .. url
    else
        return (string.match(base,'^(%w+://.+)/.*') or base) .. '/' .. url
    end
end

function SetAVTransportURI(service,url,metadata)
    http_get(service,
        '<?xml version="1.0" encoding="utf-8"?><s:Envelope s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"><s:Body><u:SetAVTransportURI xmlns:u="urn:schemas-upnp-org:service:AVTransport:1"><InstanceID>0</InstanceID><CurrentURI>' .. url ..'</CurrentURI><CurrentURIMetaData>' .. metadata .. '</CurrentURIMetaData></u:SetAVTransportURI></s:Body></s:Envelope>',
        { 'Content-Type: text/xml; charset=utf-8', 'SOAPAction: "urn:schemas-upnp-org:service:AVTransport:1#SetAVTransportURI"' }
    )
end

function Play(service)
    http_get(service,
        '<?xml version="1.0" encoding="utf-8"?><s:Envelope s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"><s:Body><u:Play xmlns:u="urn:schemas-upnp-org:service:AVTransport:1"><InstanceID>0</InstanceID><Speed>1</Speed></u:Play></s:Body></s:Envelope>',
        { 'Content-Type: text/xml; charset=utf-8', 'SOAPAction: "urn:schemas-upnp-org:service:AVTransport:1#Play"' }
    )
end

function Stop(service)
    http_get(service,
        '<?xml version="1.0" encoding="utf-8"?><s:Envelope s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"><s:Body><u:Stop xmlns:u="urn:schemas-upnp-org:service:AVTransport:1"><InstanceID>0</InstanceID></u:Stop></s:Body></s:Envelope>',
        { 'Content-Type: text/xml; charset=utf-8', 'SOAPAction: "urn:schemas-upnp-org:service:AVTransport:1#Stop"' }
    )
end

actions=
{
    ['ssdp']=function()
        local t={}
            for i,j in ipairs(ssdp_search('ssdp:all',3)) do
                table.insert(t,{ location=j, desc=http_get(j) })
            end
        return t
    end,

    ['search']=function()
        local t={}

        for i,j in ipairs(ssdp_search('urn:schemas-upnp-org:service:AVTransport:1',3)) do
--        for i,j in ipairs(ssdp_search('ssdp:all',3)) do
            local root=http_get(j)

            if root and root~='' then
                root=xml_transform(xml_decode(root)).root

                local device=root.device

                for ii,jj in ipairs(device.serviceList.service) do
                    if jj.serviceType.value=='urn:schemas-upnp-org:service:AVTransport:1' then
                        local base=jj.URLBase or device.URLBase or root.URLBase

                        if base then base=base.value else base=j end

                        local u=concat_url(base,jj.controlURL.value)

                        if not string.find(u,'^http://') then u='http://' .. u end

                        table.insert(t, { name=device.friendlyName.value, url=u })
                    end
                end
            end
        end

        return t
    end,

    ['play']=function()
        local s=json_decode(content)

        local metadata=soap_escape_xml(
            '<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/" xmlns:dlna="urn:schemas-dlna-org:metadata-1-0">' ..
            '<item id="100" parentID="0" restricted="true"><dc:title>' .. (s.name or 'Live stream') .. '</dc:title><upnp:class>object.item.videoItem</upnp:class>' ..
            '<res protocolInfo="http-get:*:video/mpeg:DLNA.ORG_OP=00;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01700000000000000000000000000000">' .. s.url ..'</res></item></DIDL-Lite>')

        Stop(s.dst)

        SetAVTransportURI(s.dst,s.url,metadata)

        Play(s.dst)
    end,

    ['stop']=function()
        Stop(json_decode(content).dst)
    end,

    ['tags']=function()
        local t={}

        for i,j in pairs(playlist()) do
            table.insert(t, { id=i, name=j.name } )
        end

        return t
    end,

    ['channels']=function(s)
        local t={}

        for i,j in pairs(playlist()[tonumber(s.id)].channels) do
            table.insert(t, { name=j.name, url=j.url, logo=j['tvg-logo'] } )
        end

        return t
    end
}

function doit()
    json_no_unicode_escape(1)

    return json_encode(actions[args.action](args))
end

rc,resp=pcall(doit)

if not rc then
    trace(resp..'\n')

    status(500,'Internal Server Error')
else
    status(200,'OK', { 'Content-Length: ' .. string.len(resp), 'Content-Type: application/json' } )

    io.stdout:write(resp)
end
