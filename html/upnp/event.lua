-- Copyright (C) 2018 Anton Burdinuk
-- clark15b@gmail.com
-- http://xupnpd.org

if method=='SUBSCRIBE' then
    local sid=headers.SID or 'uuid:'..uuidgen()

    local timeout=headers.TIMEOUT or 'Second-300'

    status(200,"OK", { 'Content-Length: 0', 'SID: '..sid,'TIMEOUT: '..timeout } )
else
    status(200,"OK", { 'Content-Length: 0' })
end
