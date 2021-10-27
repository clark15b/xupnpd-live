-- Copyright (C) 2018 Anton Burdinuk
-- clark15b@gmail.com
-- http://xupnpd.org

-- Kineskop.tv
function kineskop(url)
    local s=string.match(http_get(url),"loadStream%(decodeURIComponent%(getURLParam%('src','(.-)'%)")

    if not s or s=='' then
        trace('unknown Kineskop.tv channel')
    end

    return s
end

-- JurnalTV MD
function jurnaltv(url)
    return string.match(http_get(url),'"(http://live%.cdn%.jurnaltv%.md/.-)"')
end

function wetter(url)
    return string.match(http_get(url),'data%-video%-url%-hls="(.-)"')
end

function skylinewebcams(url)
    return string.match(http_get(url),'[\'"](https://[%w%./]+%.m3u8%?[=%w]+)[\'"]')
end

function youtube(url)
    return string.gsub(string.match(http_get(url),'"hlsvp":"(.-%.m3u8)"'),'\\/','/')
end

function angelcam(url)
     return string.match(http_get(url), "source: '(.-)',")
end
